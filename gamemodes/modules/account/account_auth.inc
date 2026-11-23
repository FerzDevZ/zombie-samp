#if defined _ls_account_auth_included
    #endinput
#endif
#define _ls_account_auth_included

/* Alur: lookup akun -> register/login (bcrypt) -> pilih karakter -> load karakter. */

/* State pending dideklarasikan di atas supaya bisa di-reset di BeginAuth:
   flag yang bocor ke penghuni slot berikutnya pernah jadi risiko takeover. */
new RecoveryPending[MAX_PLAYERS];
new PasswordChangePending[MAX_PLAYERS];
new PasswordChangeNew[MAX_PLAYERS][MAX_PASSWORD_LENGTH + 1];
/* Akun ditangkap saat permintaan dibuat: callback bcrypt tidak boleh memakai
   AccountId[playerid] saat callback (slot bisa sudah dihuni akun lain). */
new RecoveryAccount[MAX_PLAYERS];
new PasswordChangeAccount[MAX_PLAYERS];
/* Level DB ditangkap saat lookup, diterapkan setelah login sukses. */
new AccountDbLevel[MAX_PLAYERS];

/* Rate-limit login per IP (bertahan lintas reconnect) + guard satu verify
   per sesi: tanpa ini bot reconnect = brute force tak terbatas, dan spam
   /masuk mengantrikan bcrypt cost-12 tanpa batas (DoS CPU). */
#define LOGIN_IP_SLOTS 16
new LoginIpAddr[LOGIN_IP_SLOTS][16];
new LoginIpTries[LOGIN_IP_SLOTS];
new LoginIpBlockedUntil[LOGIN_IP_SLOTS];
new LoginIpLastSeen[LOGIN_IP_SLOTS];
new bool:LoginPending[MAX_PLAYERS];
/* Akun yang sedang diverifikasi: callback login tidak boleh menulis ke akun
   penghuni baru bila slot dipakai ulang selama bcrypt_verify (audit #11). */
new LoginAccount[MAX_PLAYERS];
/* Race login: /masuk bisa tiba SEBELUM lookup akun selesai (koneksi lambat,
   mobile). Password di-pending dan di-submit otomatis saat hash tiba —
   tanpa ini pemain sah disuruh ulang terus dan bot load-test gagal login. */
new bool:LoginSubmitPending[MAX_PLAYERS];
new LoginPendingPassword[MAX_PLAYERS][41];

stock Login_IpSlot(playerid)
{
    new ip[16];
    GetPlayerIp(playerid, ip, sizeof(ip));
    new now = GetTickCount();
    new oldest = -1;
    for (new index = 0; index < LOGIN_IP_SLOTS; index++)
    {
        if (LoginIpAddr[index][0] != EOS && !strcmp(LoginIpAddr[index], ip, false))
        {
            LoginIpLastSeen[index] = now;
            return index;
        }
        /* Jangan evict entri yang masih diblokir. */
        if (LoginIpBlockedUntil[index] > now) continue;
        if (oldest == -1 || LoginIpLastSeen[index] < LoginIpLastSeen[oldest]) oldest = index;
    }
    if (oldest == -1) return -1;   /* semua slot diblokir: tolak */
    format(LoginIpAddr[oldest], 16, "%s", ip);
    LoginIpTries[oldest] = 0;
    LoginIpBlockedUntil[oldest] = 0;
    LoginIpLastSeen[oldest] = now;
    return oldest;
}

stock bool:Login_IpBlocked(playerid)
{
    new ipSlot = Login_IpSlot(playerid);
    if (ipSlot == -1) return true;
    return GetTickCount() < LoginIpBlockedUntil[ipSlot];
}

stock Login_IpNoteFailure(playerid)
{
    new ipSlot = Login_IpSlot(playerid);
    if (ipSlot == -1) return 0;
    LoginIpTries[ipSlot]++;
    if (LoginIpTries[ipSlot] >= MAX_LOGIN_ATTEMPTS)
    {
        LoginIpBlockedUntil[ipSlot] = GetTickCount() + 60000;
        LoginIpTries[ipSlot] = 0;
    }
    return 1;
}

stock Account_BeginAuth(playerid)
{
    /* FCNPC memakai slot player; mereka tidak butuh akun, dialog, atau query DB.
       Tanpa guard ini setiap spawn zombie menembak SELECT ke MySQL. */
    if (IsPlayerNPC(playerid)) return 1;

    RecoveryPending[playerid] = 0;
    PasswordChangePending[playerid] = 0;
    PasswordChangeNew[playerid][0] = EOS;
    RecoveryAccount[playerid] = 0;
    PasswordChangeAccount[playerid] = 0;
    AccountDbLevel[playerid] = 0;
    LoginPending[playerid] = false;
    LoginAccount[playerid] = 0;
    LoginSubmitPending[playerid] = false;
    LoginPendingPassword[playerid][0] = EOS;

    if (!SQL_IsReady())
    {
        SendClientMessage(playerid, UI_ACCENT_DANGER, "[AKUN] Database belum siap. Hubungi admin.");
        return 0;
    }

    new playerName[MAX_PLAYER_NAME + 1];
    GetPlayerName(playerid, playerName, sizeof(playerName));

    new authLine[144];
    format(authLine, sizeof(authLine), "[AUTH] cek akun nick '%s'", playerName);
    /* authLine = DATA (nick bisa mengandung '%'); jangan jadikan format string. */
    Log_Format(LOG_INFO, "%s", authLine);

    new query[192];
    mysql_format(gSQL, query, sizeof(query),
        "SELECT id, password_hash, is_banned, admin_level FROM accounts WHERE username = '%e' LIMIT 1",
        playerName);
    mysql_tquery(gSQL, query, "Account_OnLookup", "i", playerid);

    Account_LockPlayer(playerid);
    return 1;
}

public Account_OnLookup(playerid)
{
    if (!IsPlayerConnected(playerid)) return 0;

    if (cache_num_rows() == 0)
    {
        LoginSubmitPending[playerid] = false;
        LoginPendingPassword[playerid][0] = EOS;
        new missingNick[MAX_PLAYER_NAME + 1];
        GetPlayerName(playerid, missingNick, sizeof(missingNick));
        Log_Format(LOG_INFO, "[AUTH] nick '%s' belum ada, arahkan ke /daftar.", missingNick);
        /* Stage wajib di-set di jalur chat (AUTH_INPUT_DIALOGS 0): tanpa ini
           guard /daftar menolak semua pemain baru. */
        AuthStage[playerid] = AUTH_REGISTER;
        SendClientMessage(playerid, UI_ACCENT_PRIMARY, "[AKUN] Nickname ini belum terdaftar. Buat akun dengan cara mengetik di chat:");
        SendClientMessage(playerid, UI_TEXT_SECONDARY, "[AKUN] /daftar passwordmu    contoh: /daftar tahan2026");
        Account_ShowRegister(playerid);
        return 1;
    }

    new isBanned, accountDbId;
    cache_get_value_name_int(0, "is_banned", isBanned);
    cache_get_value_name_int(0, "id", accountDbId);

    if (isBanned == 1)
    {
        LoginSubmitPending[playerid] = false;
        LoginPendingPassword[playerid][0] = EOS;
        /* Detail ban + auto-unban kalau sudah kedaluwarsa ditangani moderation. */
        Moderation_CheckBan(playerid, accountDbId);
        return 1;
    }

    AccountId[playerid] = accountDbId;

    /* Level admin dibaca di sini tapi BARU diterapkan setelah password
       diverifikasi (Account_OnPasswordVerified). Menerapkan hak istimewa
       dari sekadar lookup nick = escalation tanpa password (nick bisa
       dipakai siapa pun saat pemilik offline). */
    new adminLevel;
    cache_get_value_name_int(0, "admin_level", adminLevel);
    AccountDbLevel[playerid] = adminLevel;
    cache_get_value_name(0, "password_hash", StoredPasswordHash[playerid], BCRYPT_HASH_LENGTH);

    AuthStage[playerid] = AUTH_LOGIN;
    SendClientMessage(playerid, UI_ACCENT_PRIMARY, "[AKUN] Nickname ini sudah terdaftar. Masuk dengan:");
    SendClientMessage(playerid, UI_TEXT_SECONDARY, "[AKUN] /masuk passwordmu    contoh: /masuk passwordmu");
    SendClientMessage(playerid, UI_TEXT_MUTED, "[AKUN] Lupa password? /pulihkan passwordbaru <kunci pemilik> (minta ke pemilik server).");
    Account_ShowLogin(playerid);

    /* Password yang tiba sebelum lookup selesai: submit sekarang. */
    if (LoginSubmitPending[playerid])
    {
        LoginSubmitPending[playerid] = false;
        new earlyPassword[41];
        strcpy(earlyPassword, LoginPendingPassword[playerid], 41);
        LoginPendingPassword[playerid][0] = EOS;
        Account_SubmitLogin(playerid, earlyPassword);
    }
    return 1;
}

stock Account_SendRegistrationHelp(playerid)
{
    new nickLine[144], nick[MAX_PLAYER_NAME + 1];
    GetPlayerName(playerid, nick, sizeof(nick));

    format(nickLine, sizeof(nickLine), "[DAFTAR] Nickname kamu sekarang: %s - ini otomatis jadi username akun.", nick);
    SendClientMessage(playerid, UI_ACCENT_PRIMARY, nickLine);
    SendClientMessage(playerid, UI_TEXT_SECONDARY, "[DAFTAR] Contoh nickname benar: Budi_Santoso | Sari_123 | Rizky_Pratama | FerzDevZ");
    SendClientMessage(playerid, UI_TEXT_SECONDARY, "[DAFTAR] Aturan nickname: 3-24 huruf/angka/garis bawah, tanpa spasi, tanpa simbol lain.");
    SendClientMessage(playerid, UI_TEXT_SECONDARY, "[DAFTAR] Contoh password benar: tahan2026 | Kubu_Zombie99 | api!malam7 (6-40 karakter).");
    SendClientMessage(playerid, UI_TEXT_SECONDARY, "[DAFTAR] Dialog tidak muncul di HP? Ketik: /daftar tahan2026 (ganti dengan passwordmu).");
    return 1;
}

stock Account_ShowRegister(playerid)
{
    if (ChatAuthOnly[playerid])
    {
        SendClientMessage(playerid, UI_ACCENT_INFO, "[AKUN] Ketik di chat: /daftar passwordmu   contoh: /daftar tahan2026");
        return 1;
    }
    return Account_ShowRegisterDialog(playerid);
}

stock Account_ShowRegisterDialog(playerid)
{
#if AUTH_INPUT_DIALOGS
    AuthStage[playerid] = AUTH_REGISTER;
    Account_SendRegistrationHelp(playerid);
    new nick[MAX_PLAYER_NAME + 1], body[256];
    GetPlayerName(playerid, nick, sizeof(nick));
    format(body, sizeof(body),
        "Akun baru. Username otomatis: %s\n\n\
         Ketik PASSWORD baru (6-40 karakter), contoh: tahan2026\n\
         Jangan ketik nickname di sini.\n\
         HP tidak bisa mengetik di dialog? Tutup lalu ketik: /daftar passwordmu",
        nick);

    ShowPlayerDialog(playerid, DIALOG_AUTH_REGISTER, DIALOG_STYLE_INPUT,
        "Daftar Akun", body, "Daftar", "Keluar");
#else
    #pragma unused playerid
    AuthStage[playerid] = AUTH_REGISTER;
    SendClientMessage(playerid, UI_ACCENT_INFO, "[AKUN] Daftar lewat chat: /daftar passwordmu   contoh: /daftar tahan2026");
#endif
    return 1;
}

stock Account_ShowLogin(playerid)
{
    if (ChatAuthOnly[playerid])
    {
        SendClientMessage(playerid, UI_ACCENT_INFO, "[AKUN] Ketik di chat: /masuk passwordmu   contoh: /masuk passwordmu");
        return 1;
    }
    return Account_ShowLoginDialog(playerid);
}

stock Account_ShowLoginDialog(playerid)
{
#if AUTH_INPUT_DIALOGS
    AuthStage[playerid] = AUTH_LOGIN;
    new nick[MAX_PLAYER_NAME + 1], body[192];
    GetPlayerName(playerid, nick, sizeof(nick));
    format(body, sizeof(body),
        "Akun: %s\n\n\
         Ketik password akun ini (6-40 karakter).\n\
         HP tidak bisa mengetik di dialog? Tutup lalu ketik: /masuk passwordmu",
        nick);

    ShowPlayerDialog(playerid, DIALOG_AUTH_LOGIN, DIALOG_STYLE_PASSWORD,
        "Masuk", body, "Masuk", "Keluar");
#else
    #pragma unused playerid
    SendClientMessage(playerid, UI_ACCENT_INFO, "[AKUN] Masuk lewat chat: /masuk passwordmu   contoh: /masuk passwordmu");
#endif
    return 1;
}

/* ---------- registrasi ---------- */

/* Cap registrasi per-IP (audit F-18): akun alt tak terbatas = farm kit awal
   + tutorial $300 lalu trade ke akun utama. Maks 3 akun baru per IP / 24 jam.
   Tabel penuh = fail-open (jangan blokir pemain sah; dicatat log). */
#define REG_IP_SLOTS 32
#define REG_IP_MAX 3
#define REG_IP_WINDOW_S 86400
new RegIpAddr[REG_IP_SLOTS][16];
new RegIpCount[REG_IP_SLOTS];
new RegIpWindowStart[REG_IP_SLOTS];

stock bool:Reg_IpAllowed(playerid)
{
    new ip[16];
    GetPlayerIp(playerid, ip, sizeof(ip));
    new now = gettime();
    new freeSlot = -1;

    for (new index = 0; index < REG_IP_SLOTS; index++)
    {
        if (RegIpAddr[index][0] != EOS && !strcmp(RegIpAddr[index], ip, false))
        {
            if (now - RegIpWindowStart[index] >= REG_IP_WINDOW_S)
            {
                RegIpCount[index] = 0;
                RegIpWindowStart[index] = now;
            }
            if (RegIpCount[index] >= REG_IP_MAX) return false;
            RegIpCount[index]++;
            return true;
        }
        if (freeSlot == -1 && (RegIpAddr[index][0] == EOS
            || now - RegIpWindowStart[index] >= REG_IP_WINDOW_S)) freeSlot = index;
    }

    if (freeSlot == -1)
    {
        Log_Format(LOG_WARN, "[AUTH] tabel reg-IP penuh: fail-open untuk %s", ip);
        return true;
    }
    format(RegIpAddr[freeSlot], 16, "%s", ip);
    RegIpCount[freeSlot] = 1;
    RegIpWindowStart[freeSlot] = now;
    return true;
}

stock Account_RegisterPassword(playerid, const password[])
{
    if (!Account_IsValidPassword(password))
    {
        SendClientMessage(playerid, UI_ACCENT_WARN, "[AKUN] Password harus 6-40 karakter. Contoh: tahan2026");
        Log_Format(LOG_WARN, "[AUTH] password registrasi tidak valid (panjang %d).", strlen(password));
        Account_ShowRegister(playerid);
        return 0;
    }

    if (!Reg_IpAllowed(playerid))
    {
        SendClientMessage(playerid, UI_ACCENT_DANGER, "[AKUN] Terlalu banyak akun baru dari IP ini (maks 3 per 24 jam). Coba besok.");
        Log_Format(LOG_WARN, "[AUTH] registrasi ditolak: cap per-IP tercapai.");
        return 0;
    }

    /* Tangkap nick + generasi koneksi SAAT request: callback bcrypt bisa
       tiba setelah slot dihuni pemain lain (pre-hijack — audit F-09). */
    GetPlayerName(playerid, RegNick[playerid], MAX_PLAYER_NAME + 1);
    RegGen[playerid] = ConnectGen[playerid];
    bcrypt_hash(playerid, "Account_OnPasswordHashed", password, BCRYPT_COST);
    return 1;
}

public Account_OnPasswordHashed(playerid)
{
    if (!IsPlayerConnected(playerid)) return 0;
    if (RegGen[playerid] != ConnectGen[playerid]) return 0;

    new hashedPassword[BCRYPT_HASH_LENGTH];
    bcrypt_get_hash(hashedPassword);

    new playerIp[16], query[320], msBuf[16];
    GetPlayerIp(playerid, playerIp, sizeof(playerIp));
    EpochMs(msBuf, sizeof(msBuf));

    mysql_format(gSQL, query, sizeof(query),
        "INSERT INTO accounts (username, password_hash, created_at, last_ip) VALUES ('%e', '%e', %s, '%e')",
        RegNick[playerid], hashedPassword, msBuf, playerIp);
    mysql_tquery(gSQL, query, "Account_OnAccountInserted", "i", playerid);
    return 1;
}

public Account_OnAccountInserted(playerid)
{
    if (!IsPlayerConnected(playerid)) return 0;
    if (RegGen[playerid] != ConnectGen[playerid]) return 0;

    new insertedId = cache_insert_id();
    if (insertedId == 0)
    {
        new failLine[160];
        format(failLine, sizeof(failLine), "[AKUN] Registrasi gagal (kode DB %d). Coba nama lain atau hubungi admin.", mysql_errno(gSQL));
        SendClientMessage(playerid, UI_ACCENT_DANGER, failLine);
        Log_Format(LOG_ERROR, "[AKUN] insert akun gagal untuk player %d (mysql errno %d)", playerid, mysql_errno(gSQL));
        Account_ShowRegister(playerid);
        return 1;
    }

    AccountId[playerid] = insertedId;
    /* Registrasi = terautentikasi (pemain baru saja membuat password sendiri).
       Akun pemilik langsung jadi superadmin pada registrasi pertama. */
    AccountAuthed[playerid] = true;
    Admin_ApplyLoadedLevel(playerid, ADMIN_LEVEL_PLAYER);
    Log_Format(LOG_INFO, "[AKUN] akun baru id %d dibuat.", insertedId);
    SendClientMessage(playerid, UI_ACCENT_PRIMARY, "[AKUN] Akun dibuat. Pilih karakter.");
    Account_LoadCharacters(playerid);
    return 1;
}

/* ---------- pemulihan pemilik server ---------- */

/* Password yang dibuat plugin sendiri (bcrypt_hash), bukan dari luar. Dipakai
   kalau pemilik terkunci: nickname pemilik = identitas, jadi tidak menambah
   permukaan serangan baru dibanding login biasa. */

stock Account_RecoverPassword(playerid, const newPassword[], const key[])
{
    if (IsPlayerNPC(playerid)) return 0;

    /* Tebakan kunci pemulihan dibatasi per IP, sama seperti /admin & /superadmin. */
    if (Admin_KeyBlocked(playerid))
    {
        SendClientMessage(playerid, UI_ACCENT_WARN, "[AKUN] Terlalu banyak percobaan kunci. Coba lagi sebentar.");
        return 0;
    }

    /* Pemulihan WAJIB kunci super untuk SEMUA akun, termasuk pemilik.
       Bypass nickname lama dihapus: nick SA-MP tidak terautentikasi — siapa
       pun bisa connect memakai nick pemilik saat offline dan mengambil alih
       akun superadmin tanpa password (temuan audit F-01). */
    if (!Admin_KeysLoaded())
    {
        SendClientMessage(playerid, UI_ACCENT_DANGER, "[AKUN] Pemulihan tidak tersedia: kunci server tidak dikonfigurasi.");
        return 0;
    }
    new bool:allowed = (key[0] != EOS && !strcmp(key, AdminKeySuper, false));

    if (!allowed)
    {
        if (key[0] != EOS) Admin_KeyNoteFailure(playerid);
        SendClientMessage(playerid, UI_ACCENT_DANGER, "[AKUN] Butuh kunci pemilik: /pulihkan passwordbaru <kunci pemilik>");
        Log_Format(LOG_WARN, "[AKUN] pemulihan ditolak playerid=%d (tanpa kunci).", playerid);
        return 0;
    }

    if (AccountId[playerid] == 0)
    {
        SendClientMessage(playerid, UI_ACCENT_WARN, "[AKUN] Nickname ini belum punya akun. Pakai /daftar passwordmu");
        return 0;
    }

    if (!Account_IsValidPassword(newPassword))
    {
        SendClientMessage(playerid, UI_ACCENT_WARN, "[AKUN] Password baru harus 6-40 karakter.");
        return 0;
    }

    RecoveryPending[playerid] = 1;
    RecoveryAccount[playerid] = AccountId[playerid];
    SendClientMessage(playerid, UI_TEXT_SECONDARY, "[AKUN] Membuat hash password baru...");
    bcrypt_hash(playerid, "Account_OnRecoveryHashed", newPassword, BCRYPT_COST);
    return 1;
}

public Account_OnRecoveryHashed(playerid)
{
    if (!IsPlayerConnected(playerid)) return 0;
    if (!RecoveryPending[playerid]) return 0;

    RecoveryPending[playerid] = 0;

    /* Slot dipakai ulang akun lain selama hashing: batalkan, jangan tulis
       password ke akun penghuni baru. */
    if (RecoveryAccount[playerid] <= 0 || AccountId[playerid] != RecoveryAccount[playerid])
    {
        Log_Format(LOG_WARN, "[AKUN] pemulihan dibatalkan: slot berubah akun (playerid=%d).", playerid);
        return 0;
    }

    new freshHash[BCRYPT_HASH_LENGTH];
    bcrypt_get_hash(freshHash);

    new query[192];
    mysql_format(gSQL, query, sizeof(query),
        "UPDATE accounts SET password_hash = '%e' WHERE id = %d",
        freshHash, RecoveryAccount[playerid]);
    mysql_tquery(gSQL, query, "", "");

    format(StoredPasswordHash[playerid], BCRYPT_HASH_LENGTH, "%s", freshHash);
    Log_Format(LOG_WARN, "[AKUN] pemulihan password pemilik untuk akun %d.", RecoveryAccount[playerid]);
    SendClientMessage(playerid, UI_ACCENT_PRIMARY, "[AKUN] Password dipulihkan. Login pakai: /masuk passwordbaru");
    Account_ShowLogin(playerid);
    return 1;
}

/* ---------- ganti password ---------- */

stock Account_RequestPasswordChange(playerid, const oldPassword[], const newPassword[])
{
    if (!Account_IsPlaying(playerid)) return 0;

    if (!Account_IsValidPassword(newPassword))
    {
        SendClientMessage(playerid, UI_ACCENT_WARN, "[AKUN] Password baru harus 6-40 karakter.");
        return 0;
    }

    if (!strcmp(oldPassword, newPassword, false))
    {
        SendClientMessage(playerid, UI_ACCENT_WARN, "[AKUN] Password baru tidak boleh sama dengan yang lama.");
        return 0;
    }

    if (StoredPasswordHash[playerid][0] == EOS)
    {
        SendClientMessage(playerid, UI_ACCENT_WARN, "[AKUN] Sesi tidak punya hash password. Login ulang dulu.");
        return 0;
    }

    format(PasswordChangeNew[playerid], sizeof(PasswordChangeNew[]), "%s", newPassword);
    PasswordChangePending[playerid] = 1;
    PasswordChangeAccount[playerid] = AccountId[playerid];
    SendClientMessage(playerid, UI_TEXT_SECONDARY, "[AKUN] Memeriksa password lama...");
    bcrypt_verify(playerid, "Account_OnPassChangeVerified", oldPassword, StoredPasswordHash[playerid]);
    return 1;
}

public Account_OnPassChangeVerified(playerid, bool:success)
{
    if (!IsPlayerConnected(playerid)) return 0;
    if (!PasswordChangePending[playerid]) return 0;
    if (PasswordChangeAccount[playerid] <= 0 || AccountId[playerid] != PasswordChangeAccount[playerid])
    {
        PasswordChangePending[playerid] = 0;
        return 0;
    }

    if (!success)
    {
        PasswordChangePending[playerid] = 0;
        SendClientMessage(playerid, UI_ACCENT_DANGER, "[AKUN] Password lama salah. Pakai: /gantipass lamabaru");
        return 1;
    }

    bcrypt_hash(playerid, "Account_OnPasswordChangeHashed", PasswordChangeNew[playerid], BCRYPT_COST);
    return 1;
}

public Account_OnPasswordChangeHashed(playerid)
{
    if (!IsPlayerConnected(playerid)) return 0;
    if (!PasswordChangePending[playerid]) return 0;
    if (PasswordChangeAccount[playerid] <= 0 || AccountId[playerid] != PasswordChangeAccount[playerid])
    {
        PasswordChangePending[playerid] = 0;
        return 0;
    }

    new freshHash[BCRYPT_HASH_LENGTH];
    bcrypt_get_hash(freshHash);

    new query[192];
    mysql_format(gSQL, query, sizeof(query),
        "UPDATE accounts SET password_hash = '%e' WHERE id = %d",
        freshHash, PasswordChangeAccount[playerid]);
    mysql_tquery(gSQL, query, "Account_OnPasswordChanged", "i", playerid);

    format(StoredPasswordHash[playerid], BCRYPT_HASH_LENGTH, "%s", freshHash);
    PasswordChangePending[playerid] = 0;
    PasswordChangeNew[playerid][0] = EOS;
    Log_Format(LOG_INFO, "[AKUN] password akun %d diganti.", PasswordChangeAccount[playerid]);
    SendClientMessage(playerid, UI_ACCENT_PRIMARY, "[AKUN] Password berhasil diganti. Ingat password baru untuk login berikutnya.");
    return 1;
}

public Account_OnPasswordChanged(playerid)
{
    #pragma unused playerid
    return 1;
}

/* ---------- login ---------- */

stock Account_SubmitLogin(playerid, const password[])
{
    if (StoredPasswordHash[playerid][0] == EOS)
    {
        /* Lookup belum selesai: simpan password, submit otomatis di
           Account_OnLookup. Jangan paksa pemain mengetik ulang. */
        LoginSubmitPending[playerid] = true;
        format(LoginPendingPassword[playerid], 41, "%s", password);
        SendClientMessage(playerid, UI_TEXT_MUTED, "[AKUN] Password diterima, menunggu data akun...");
        return 1;
    }

    if (Login_IpBlocked(playerid))
    {
        SendClientMessage(playerid, UI_ACCENT_DANGER, "[AKUN] Terlalu banyak percobaan dari IP ini. Tunggu 60 detik.");
        return 0;
    }
    if (LoginPending[playerid])
    {
        SendClientMessage(playerid, UI_TEXT_MUTED, "[AKUN] Password sedang diperiksa, tunggu sebentar.");
        return 0;
    }
    LoginPending[playerid] = true;
    LoginAccount[playerid] = AccountId[playerid];
    bcrypt_verify(playerid, "Account_OnPasswordVerified", password, StoredPasswordHash[playerid]);
    return 1;
}

public Account_OnPasswordVerified(playerid, bool:success)
{
    if (!IsPlayerConnected(playerid)) return 0;
    LoginPending[playerid] = false;
    /* Slot dipakai ulang selama verify: hasil ini bukan untuk penghuni sekarang. */
    if (AccountId[playerid] != LoginAccount[playerid] || LoginAccount[playerid] == 0) return 0;

    if (!success)
    {
        LoginAttempts[playerid]++;
        Login_IpNoteFailure(playerid);

        if (LoginAttempts[playerid] >= MAX_LOGIN_ATTEMPTS)
        {
            SendClientMessage(playerid, UI_ACCENT_DANGER, "[AKUN] Terlalu banyak percobaan. Koneksi ditutup.");
            Kick(playerid);
            return 1;
        }

        new line[96];
        format(line, sizeof(line), "[AKUN] Password salah (%d/%d).", LoginAttempts[playerid], MAX_LOGIN_ATTEMPTS);
        SendClientMessage(playerid, UI_ACCENT_WARN, line);
        Account_ShowLogin(playerid);
        return 1;
    }

    LoginAttempts[playerid] = 0;

    /* Password benar = terautentikasi. Baru di sini level admin diterapkan
     (termasuk bootstrap pemilik) — tidak pernah dari lookup nick saja. */
    AccountAuthed[playerid] = true;
    Admin_ApplyLoadedLevel(playerid, AccountDbLevel[playerid]);

    new loginIp[16], query[160], loginMs[16];
    GetPlayerIp(playerid, loginIp, sizeof(loginIp));
    EpochMs(loginMs, sizeof(loginMs));
    mysql_format(gSQL, query, sizeof(query),
        "UPDATE accounts SET last_login_at = %s, last_ip = '%e' WHERE id = %d",
        loginMs, loginIp, AccountId[playerid]);
    mysql_tquery(gSQL, query, "", "");

    Account_LoadCharacters(playerid);
    return 1;
}

/* ---------- karakter ---------- */

stock Account_LoadCharacters(playerid)
{
    new query[192];
    mysql_format(gSQL, query, sizeof(query),
        "SELECT id, slot, name, level, cash FROM characters          WHERE account_id = %d AND server_id = %d ORDER BY slot ASC",
        AccountId[playerid], Server_InstanceId());
    mysql_tquery(gSQL, query, "Account_OnCharactersLoaded", "i", playerid);
    return 1;
}

public Account_OnCharactersLoaded(playerid)
{
    if (!IsPlayerConnected(playerid)) return 0;

    new rows = cache_num_rows();
    new body[640], line[128];
    body[0] = EOS;

    CharacterCount[playerid] = 0;

    for (new row = 0; row < rows && row < MAX_CHARACTER_SLOTS; row++)
    {
        new charName[MAX_NAME_LENGTH + 1], level, cash;
        cache_get_value_name(row, "name", charName, sizeof(charName));
        cache_get_value_name_int(row, "level", level);
        cache_get_value_name_int(row, "cash", cash);

        new characterDbId, characterSlot;
        cache_get_value_name_int(row, "id", characterDbId);
        cache_get_value_name_int(row, "slot", characterSlot);

        CharacterDBId[playerid][row] = characterDbId;
        CharacterDBSlot[playerid][row] = characterSlot;

        format(line, sizeof(line), "%s  -  Level %d, $%d\n", charName, level, cash);
        strcat(body, line, sizeof(body));
        CharacterCount[playerid]++;
    }

    if (CharacterCount[playerid] < MAX_CHARACTER_SLOTS)
    {
        strcat(body, "+ Buat karakter baru", sizeof(body));
    }

    if (CharacterCount[playerid] == 0)
    {
        PendingCharacterSlot[playerid] = 0;
        CharCreate_Start(playerid);
        return 1;
    }

    AuthStage[playerid] = AUTH_CHARSELECT;
    ShowPlayerDialog(playerid, DIALOG_AUTH_CHARSEL, DIALOG_STYLE_LIST,
        "Pilih Karakter", body, "Main", "Keluar");
    return 1;
}

stock Account_CreateCharacter(playerid, const charName[])
{
    if (!Account_IsValidCharacterName(charName))
    {
        SendClientMessage(playerid, UI_ACCENT_WARN, "[KARAKTER] Nama tidak valid. Contoh benar: Arif_Pratama (3-20 huruf/angka/garis bawah, tanpa spasi).");
        ShowPlayerDialog(playerid, DIALOG_AUTH_CHARNEW, DIALOG_STYLE_INPUT,
            "Karakter Baru",
            "Masukkan nama karakter (3-20 huruf, angka, garis bawah).",
            "Buat", "Keluar");
        return 0;
    }

    new slot = PendingCharacterSlot[playerid];
    if (slot < 0 || slot >= MAX_CHARACTER_SLOTS) slot = CharacterCount[playerid];

    new query[256];
    mysql_format(gSQL, query, sizeof(query),
        "INSERT INTO characters (account_id, server_id, slot, name) VALUES (%d, %d, %d, '%e')",
        AccountId[playerid], Server_InstanceId(), slot, charName);
    mysql_tquery(gSQL, query, "Account_OnCharacterCreated", "i", playerid);
    return 1;
}

public Account_OnCharacterCreated(playerid)
{
    if (!IsPlayerConnected(playerid)) return 0;

    new createdId = cache_insert_id();
    if (createdId == 0)
    {
        SendClientMessage(playerid, UI_ACCENT_DANGER, "[KARAKTER] Nama sudah dipakai. Pilih nama lain.");
        ShowPlayerDialog(playerid, DIALOG_AUTH_CHARNEW, DIALOG_STYLE_INPUT,
            "Karakter Baru",
            "Masukkan nama karakter (3-20 huruf, angka, garis bawah).",
            "Buat", "Keluar");
        return 1;
    }

    ActiveCharId[playerid] = createdId;
    Log_Format(LOG_INFO, "[KARAKTER] karakter id %d dibuat untuk akun %d.", createdId, AccountId[playerid]);
    Account_LoadCharacterData(playerid);
    return 1;
}

stock Account_SelectCharacter(playerid, index)
{
    if (!Account_IsAuthed(playerid)) return 0;
    if (index < 0 || index >= CharacterCount[playerid]) return 0;

    ActiveCharId[playerid] = CharacterDBId[playerid][index];
    if (ActiveCharId[playerid] == 0) return 0;

    Account_LoadCharacterData(playerid);
    return 1;
}

stock Account_LoadCharacterData(playerid)
{
    if (ActiveCharId[playerid] == 0)
    {
        SendClientMessage(playerid, UI_ACCENT_DANGER, "[KARAKTER] Karakter tidak valid.");
        return 0;
    }

    new query[384];
    mysql_format(gSQL, query, sizeof(query),
        "SELECT name, skin, gender, age, race, origin, occupation, trait, appearance, level, xp, cash, scrap, infection, health, zone_id, lobby_id, pos_x, pos_y, pos_z, pos_a, hunger, thirst, bladder, wounds, tutorial_step, tutorial_count, hotbar, playtime_sec, rank_points \
         FROM characters WHERE id = %d AND server_id = %d LIMIT 1",
        ActiveCharId[playerid], Server_InstanceId());
    mysql_tquery(gSQL, query, "Account_OnCharacterLoaded", "i", playerid);
    return 1;
}

public Account_OnCharacterLoaded(playerid)
{
    if (!IsPlayerConnected(playerid)) return 0;

    if (cache_num_rows() == 0)
    {
        SendClientMessage(playerid, UI_ACCENT_DANGER, "[KARAKTER] Data karakter hilang. Hubungi admin.");
        ActiveCharId[playerid] = 0;
        Account_LoadCharacters(playerid);
        return 1;
    }

    new charName[MAX_NAME_LENGTH + 1], skin, level, xp, cash, scrap, zoneId, lobbyId;
    new Float:infection, Float:health;
    new Float:needHunger, Float:needThirst, Float:needBladder;
    new needWounds;
    new Float:posX;
    new Float:posY;
    new Float:posZ;
    new Float:posA;

    cache_get_value_name(0, "name", charName, sizeof(charName));
    cache_get_value_name_int(0, "skin", skin);
    cache_get_value_name_int(0, "level", level);
    cache_get_value_name_int(0, "xp", xp);
    cache_get_value_name_int(0, "cash", cash);
    cache_get_value_name_int(0, "scrap", scrap);
    cache_get_value_name_int(0, "zone_id", zoneId);
    cache_get_value_name_int(0, "lobby_id", lobbyId);
    if (!Lobby_IsValid(lobbyId)) lobbyId = 0;
    PlayerLobby[playerid] = lobbyId;
    cache_get_value_name_float(0, "infection", infection);
    cache_get_value_name_float(0, "health", health);
    cache_get_value_name_float(0, "pos_x", posX);
    cache_get_value_name_float(0, "pos_y", posY);
    cache_get_value_name_float(0, "pos_z", posZ);
    cache_get_value_name_float(0, "pos_a", posA);
    cache_get_value_name_float(0, "hunger", needHunger);
    cache_get_value_name_float(0, "thirst", needThirst);
    cache_get_value_name_float(0, "bladder", needBladder);
    cache_get_value_name_int(0, "wounds", needWounds);
    if (needWounds < 0) needWounds = 0;
    if (needWounds > MAX_WOUND_STACKS) needWounds = MAX_WOUND_STACKS;
    PlayerSurvival[playerid][psWounds] = needWounds;

    new profileGender, profileAge, profileRace, profileOrigin, profileJob, profileTrait, profileLook;
    cache_get_value_name_int(0, "gender", profileGender);
    cache_get_value_name_int(0, "age", profileAge);
    cache_get_value_name_int(0, "race", profileRace);
    cache_get_value_name_int(0, "origin", profileOrigin);
    cache_get_value_name_int(0, "occupation", profileJob);
    cache_get_value_name_int(0, "trait", profileTrait);
    cache_get_value_name_int(0, "appearance", profileLook);
    CharCreate_LoadProfile(playerid, profileGender, profileAge, profileRace, profileOrigin, profileJob, profileTrait, profileLook);

    new hotbarCsv[HOTBAR_MAX_CSV];
    cache_get_value_name(0, "hotbar", hotbarCsv, sizeof(hotbarCsv));
    Hotbar_LoadCSV(playerid, hotbarCsv);

    new tutorialStep, tutorialCount;
    cache_get_value_name_int(0, "tutorial_step", tutorialStep);
    cache_get_value_name_int(0, "tutorial_count", tutorialCount);
    Tutorial_LoadFromDB(playerid, tutorialStep, tutorialCount);

    // Jangan simpan koordinat interior shelter: saat login berikutnya pemain akan
    // jatuh di world 0 pada titik yang bukan jalanan.
    if (PlayerSurvival[playerid][psInShelter])
    {
        posX = SPAWN_X;
        posY = SPAWN_Y;
        posZ = SPAWN_Z;
        posA = SPAWN_ANGLE;
    }
    else if (posX == 0.0 && posY == 0.0)
    {
        posX = SPAWN_X;
        posY = SPAWN_Y;
        posZ = SPAWN_Z;
        posA = SPAWN_ANGLE;
    }

    PlayerSurvival[playerid][psHealth] = (health > 0.0) ? health : FULL_HP;
    PlayerSurvival[playerid][psCash] = cash;
    /* Scrap legacy dikonversi jadi ITEM_SCRAP saat inventory dimuat (E1);
       kolom DB dinolkan supaya konversi hanya terjadi sekali. */
    PlayerSurvival[playerid][psScrap] = 0;
    PendingLegacyScrap[playerid] = scrap;
    if (scrap > 0)
    {
        new scrapQuery[96];
        mysql_format(gSQL, scrapQuery, sizeof(scrapQuery),
            "UPDATE characters SET scrap = 0 WHERE id = %d", ActiveCharId[playerid]);
        mysql_tquery(gSQL, scrapQuery, "", "");
    }
    cache_get_value_name_int(0, "playtime_sec", PlaytimeSec[playerid]);
    cache_get_value_name_int(0, "rank_points", RankPoints[playerid]);
    PlayerSurvival[playerid][psLevel] = level;
    PlayerSurvival[playerid][psXp] = xp;
    PlayerSurvival[playerid][psInfection] = infection;
    /* Peringatan infeksi kritis saat login (audit U3/N1): pemain harus tahu
       cara keluar (/klinik, antibiotics, vaksin) sebelum turned lagi. */
    if (infection >= 80.0)
    {
        SendClientMessage(playerid, UI_ACCENT_DANGER, "[INFEKSI] KRITIS! Cari antibiotics/vaksin, atau /klinik di kamp untuk cure total.");
        Toast_Show(playerid, "INFEKSI KRITIS", "Cure di /klinik kamp sebelum kamu turned.", UI_ACCENT_DANGER);
    }
    // 0 adalah nilai sah (kelaparan/dehidrasi): jangan diganti default.
    PlayerSurvival[playerid][psHunger] = needHunger;
    PlayerSurvival[playerid][psThirst] = needThirst;
    PlayerSurvival[playerid][psBladder] = needBladder;
    PlayerSurvival[playerid][psZone] = zoneId;
    PlayerSurvival[playerid][psInfectionStage] = 0;

    SetSpawnInfo(playerid, 0, skin, posX, posY, posZ, posA, 0, 0, 0, 0, 0, 0);
    SpawnPlayer(playerid);
    SetPlayerHealth(playerid, PlayerSurvival[playerid][psHealth]);

    SetPlayerName(playerid, charName);

    AuthStage[playerid] = AUTH_PLAYING;
    AccountLastSave[playerid] = GetTickCount();
    Account_UnlockPlayer(playerid);

    new line[128];
    format(line, sizeof(line), "[AKUN] Masuk sebagai %s. Level %d, $%d, scrap %d.", charName, level, cash, scrap);
    SendClientMessage(playerid, UI_ACCENT_PRIMARY, line);
    Toast_Show(playerid, "ZONA KARANTINA", "Selamat datang kembali. Jangan berisik.", UI_ACCENT_PRIMARY);
    Tutorial_ShowIntro(playerid);

    Inventory_Load(playerid);
    Weapons_Load(playerid);
    Skills_Load(playerid);
    Contracts_Load(playerid);
    Companion_Load(playerid);
    Garage_Load(playerid);
    return 1;
}

/* Autosave 2 menit per karakter, disebar maksimal 5 karakter tiap 10 detik.
   Ini mencegah semua query REPLACE menumpuk di satu tick saat server ramai. */
public Account_AutosaveTick()
{
    new perfStart = GetTickCount();
    new saved = 0;
    new now = GetTickCount();

    foreach (new playerid : PlayerPool)
    {
        if (!IsPlayerConnected(playerid)) continue;
        if (!Account_IsPlaying(playerid)) continue;

        if (AccountLastSave[playerid] != 0
            && now - AccountLastSave[playerid] < AUTOSAVE_INTERVAL_MS) continue;

        /* Akumulasi jam main dari delta autosave (T8): kolom playtime_sec
           sebelumnya tak pernah ditulis. */
        if (AccountLastSave[playerid] != 0)
            PlaytimeSec[playerid] += (now - AccountLastSave[playerid]) / 1000;

        Account_SaveCharacter(playerid);
        /* Inventory/Weapons/Skills sudah disimpan di dalam Account_SaveCharacter;
           jangan panggil dua kali (duplikasi REPLACE per tick). */
        Contracts_Save(playerid);
        Companion_Save(playerid);
        AccountLastSave[playerid] = now;
        saved++;

        if (saved >= AUTOSAVE_BATCH_SIZE) break;
    }

    if (saved > 0) Log_Format(LOG_INFO, "[AUTOSAVE] %d karakter disimpan (stagger).", saved);
    Perf_Update(PERF_AUTOSAVE, perfStart);
    return 1;
}

stock Account_SaveCharacter(playerid)
{
    if (ActiveCharId[playerid] == 0) return 0;

    new Float:posX;
    new Float:posY;
    new Float:posZ;
    new Float:posA;
    GetPlayerPos(playerid, posX, posY, posZ);
    GetPlayerFacingAngle(playerid, posA);

    // Jangan simpan koordinat interior shelter: saat login berikutnya pemain akan
    // jatuh di world 0 pada titik yang bukan jalanan.
    if (PlayerSurvival[playerid][psInShelter])
    {
        posX = SPAWN_X;
        posY = SPAWN_Y;
        posZ = SPAWN_Z;
        posA = SPAWN_ANGLE;
    }
    else if (posX == 0.0 && posY == 0.0)
    {
        posX = SPAWN_X;
        posY = SPAWN_Y;
        posZ = SPAWN_Z;
        posA = SPAWN_ANGLE;
    }

    new hotbarCsv[HOTBAR_MAX_CSV];
    Hotbar_ToCSV(playerid, hotbarCsv, sizeof(hotbarCsv));

    new query[704], seenMs[16];
    EpochMs(seenMs, sizeof(seenMs));
    mysql_format(gSQL, query, sizeof(query),
        "UPDATE characters SET pos_x = %f, pos_y = %f, pos_z = %f, pos_a = %f, \
            health = %f, infection = %f, hunger = %f, thirst = %f, bladder = %f, wounds = %d, \
            cash = %d, scrap = %d, level = %d, xp = %d, \
            zone_id = %d, hotbar = '%e', last_seen_at = %s, playtime_sec = %d, rank_points = %d WHERE id = %d",
        posX, posY, posZ, posA,
        PlayerSurvival[playerid][psHealth],
        PlayerSurvival[playerid][psInfection],
        PlayerSurvival[playerid][psHunger],
        PlayerSurvival[playerid][psThirst],
        PlayerSurvival[playerid][psBladder],
        PlayerSurvival[playerid][psWounds],
        PlayerSurvival[playerid][psCash],
        PlayerSurvival[playerid][psScrap],
        PlayerSurvival[playerid][psLevel],
        PlayerSurvival[playerid][psXp],
        (PlayerSurvival[playerid][psZone] < 0) ? 255 : PlayerSurvival[playerid][psZone],
        hotbarCsv,
        seenMs,
        PlaytimeSec[playerid],
        RankPoints[playerid],
        ActiveCharId[playerid]);
    /* Lewat handle transaksi bila ada: save karakter yang dipanggil di dalam
       Tx (trade/vendor) harus commit bersama mutasi item, bukan autocommit
       terpisah di pool utama (audit T3). zona -1 (wasteland) di-clamp ke 255:
       kolom UNSIGNED + strict mode menolak -1 dan menggagalkan SELURUH save. */
    if (Tx_IsReady()) mysql_tquery(gTxSQL, query, "Account_OnSaved", "i", playerid);
    else mysql_tquery(gSQL, query, "Account_OnSaved", "i", playerid);

    Inventory_Save(playerid);
    Weapons_Save(playerid);
    Skills_Save(playerid);
    return 1;
}

public Account_OnSaved(playerid)
{
    #pragma unused playerid
    return 1;
}

stock Account_OnDialogResponse(playerid, dialogid, response, listitem, const inputtext[])
{
    switch (dialogid)
    {
        case DIALOG_CC_NAME .. DIALOG_CC_CONFIRM:
        {
            /* Semua dialog pembuatan karakter ditangani modul char_create. */
            return CharCreate_OnDialogResponse(playerid, dialogid, response, listitem, inputtext) ? 1 : 0;
        }
        case DIALOG_AUTH_REGISTER:
        {
            if (!response)
            {
                Account_MarkChatAuth(playerid);
                SendClientMessage(playerid, UI_TEXT_SECONDARY, "[AKUN] Dialog ditutup. Ketik di chat: /daftar passwordmu   contoh: /daftar tahan2026");
                return 1;
            }

            new password[MAX_PASSWORD_LENGTH + 1];
            if (inputtext[0] == EOS || sscanf(inputtext, "s[41]", password) != 1 || password[0] == EOS)
            {
                Account_MarkChatAuth(playerid);
                SendClientMessage(playerid, UI_ACCENT_WARN, "[AKUN] Dialog mengirim password kosong (klien mobile). Lanjut lewat chat saja.");
                SendClientMessage(playerid, UI_ACCENT_INFO, "[AKUN] Ketik: /daftar passwordmu   contoh: /daftar tahan2026");
                return 1;
            }
            Account_RegisterPassword(playerid, password);
            return 1;
        }
        case DIALOG_AUTH_LOGIN:
        {
            if (!response)
            {
                Account_MarkChatAuth(playerid);
                SendClientMessage(playerid, UI_TEXT_SECONDARY, "[AKUN] Dialog ditutup. Ketik di chat: /masuk passwordmu   contoh: /masuk passwordmu");
                return 1;
            }

            new password[MAX_PASSWORD_LENGTH + 1];
            if (inputtext[0] == EOS || sscanf(inputtext, "s[41]", password) != 1 || password[0] == EOS)
            {
                Account_MarkChatAuth(playerid);
                SendClientMessage(playerid, UI_ACCENT_WARN, "[AKUN] Dialog mengirim password kosong (klien mobile). Lanjut lewat chat saja.");
                SendClientMessage(playerid, UI_ACCENT_INFO, "[AKUN] Ketik: /masuk passwordmu   contoh: /masuk passwordmu");
                return 1;
            }
            Account_SubmitLogin(playerid, password);
            return 1;
        }
        case DIALOG_AUTH_CHARSEL:
        {
            /* Tanpa gate ini, spoof dialog 2003 bisa memilih karakter akun
               orang lain tanpa password (AccountId terisi saat lookup). */
            if (!Account_IsAuthed(playerid) || AuthStage[playerid] != AUTH_CHARSELECT) return 0;
            if (!response) return Kick(playerid);

            if (listitem == CharacterCount[playerid])
            {
                PendingCharacterSlot[playerid] = CharacterCount[playerid];
                CharCreate_Start(playerid);
                return 1;
            }

            Account_SelectCharacter(playerid, listitem);
            return 1;
        }
        case DIALOG_AUTH_CHARNEW:
        {
            if (!Account_IsAuthed(playerid)) return 0;
            if (!response)
            {
                Account_LoadCharacters(playerid);
                return 1;
            }

            new charName[MAX_NAME_LENGTH + 1];
            if (sscanf(inputtext, "s[21]", charName) == 0) Account_CreateCharacter(playerid, charName);
            return 1;
        }
    }
    return 0;
}
