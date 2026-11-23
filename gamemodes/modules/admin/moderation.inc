/* Moderasi: ban/unban memakai tabel `bans` (sudah ada di migrasi 001, sebelumnya
   tidak dipakai sama sekali) + flag `accounts.is_banned` yang sudah diperiksa login.

   Alur:
   - `/ban <id> <menit|0> [alasan]` → INSERT bans + is_banned=1 + kick.
   - `0` menit = permanen (`expires_at = 0`).
   - Saat login dengan is_banned=1, `Moderation_ResolveBan` memeriksa ban terakhir:
     kalau sudah kedaluwarsa, flag dibersihkan otomatis (tidak perlu unban manual).
   - `/unban <username>` khusus superadmin dan mencatat admin pelakunya. */

forward Moderation_KickTarget(playerid, accountDbId);
forward Moderation_OnBanSaved(actorId, targetId, accountDbId);
forward Moderation_OnUnbanLoaded(adminId, const username[]);
forward Moderation_OnBanResolved(playerid, accountDbId);
forward Moderation_OnBansLoaded(adminId);

/* Mute/warn (ROADMAP 401). Disimpan di memori: mute hilang saat reconnect,
   sama seperti /freeze — cukup untuk moderasi sesi, dan tidak menambah tabel
   yang harus dimigrasi. Setiap aksi tetap masuk `admin_actions`. */
#define MUTE_MAX_MINUTES   10080      /* 7 hari */
#define WARN_BEFORE_MUTE   3
#define WARN_MUTE_MINUTES  15

forward AdminAudit_Log(adminId, const action[], targetAccount, const detail[]);

new PlayerMutedUntil[MAX_PLAYERS];
new PlayerWarnCount[MAX_PLAYERS];

#define BAN_MAX_REASON     100
#define BAN_MAX_MINUTES    525600   /* 1 tahun, cukup untuk "permanen praktis" */

stock Moderation_Ban(actorId, targetId, minutes, const reason[])
{
    if (!IsServerAdmin(actorId)) return 0;
    if (!IsPlayerConnected(targetId)) return 0;
    if (!Account_IsPlaying(targetId)) return 0;

    if (targetId == actorId)
    {
        SendClientMessage(actorId, UI_ACCENT_DANGER, "[BAN] Tidak bisa mem-ban diri sendiri.");
        return 0;
    }

    /* Admin tidak boleh mem-ban admin lain; hanya superadmin yang boleh. */
    if (AdminLevel[targetId] >= ADMIN_LEVEL_ADMIN && !IsServerSuperAdmin(actorId))
    {
        SendClientMessage(actorId, UI_ACCENT_DANGER, "[BAN] Hanya superadmin yang bisa mem-ban admin.");
        return 0;
    }

    if (AccountId[targetId] <= 0)
    {
        SendClientMessage(actorId, UI_ACCENT_DANGER, "[BAN] Target belum login ke akun.");
        return 0;
    }

    if (minutes < 0) minutes = 0;
    if (minutes > BAN_MAX_MINUTES) minutes = BAN_MAX_MINUTES;

    new reasonText[BAN_MAX_REASON + 1];
    format(reasonText, sizeof(reasonText), "%s", (reason[0] == EOS) ? "tanpa alasan" : reason);
    if (strlen(reasonText) > BAN_MAX_REASON) reasonText[BAN_MAX_REASON] = EOS;

    new adminName[MAX_PLAYER_NAME + 1];
    GetPlayerName(actorId, adminName, sizeof(adminName));

    new expiresAt = (minutes == 0) ? 0 : (gettime() + minutes * 60);
    new targetAccount = AccountId[targetId];

    new query[320];
    mysql_format(gSQL, query, sizeof(query),
        "INSERT INTO bans (account_id, reason, admin_name, expires_at, created_at) \
         VALUES (%d, '%e', '%e', %d, %d)",
        targetAccount, reasonText, adminName, expiresAt, gettime());
    mysql_tquery(gSQL, query, "Moderation_OnBanSaved", "ddd", actorId, targetId, targetAccount);

    new auditDetail[ADMIN_AUDIT_DETAIL_MAX + 1];
    format(auditDetail, sizeof(auditDetail), "alasan=%s menit=%d", reasonText, minutes);
    AdminAudit_Log(actorId, "ban", targetAccount, auditDetail);

    new targetName[MAX_PLAYER_NAME + 1];
    GetPlayerName(targetId, targetName, sizeof(targetName));

    new line[192];
    if (expiresAt == 0)
    {
        format(line, sizeof(line), "[BAN] %s diblokir permanen: %s", targetName, reasonText);
    }
    else
    {
        format(line, sizeof(line), "[BAN] %s diblokir %d menit: %s", targetName, minutes, reasonText);
    }
    SendClientMessage(actorId, UI_ACCENT_PRIMARY, line);
    Log_Format(LOG_INFO, "[BAN] admin=%d account=%d menit=%d alasan=%s", actorId, targetAccount, minutes, reasonText);

    new kickText[160];
    format(kickText, sizeof(kickText), "Diblokir oleh %s. %s", adminName, reasonText);
    Toast_Show(targetId, "DIBLOKIR", reasonText, UI_ACCENT_DANGER);
    SendClientMessage(targetId, UI_ACCENT_DANGER, kickText);
    SetTimerEx("Moderation_KickTarget", 1200, false, "ii", targetId, targetAccount);
    return 1;
}

public Moderation_KickTarget(playerid, accountDbId)
{
    /* Delay kecil supaya pesan alasan sampai ke client sebelum koneksi diputus.
       Akun wajib cocok: slot player bisa dipakai ulang orang lain selama delay. */
    if (IsPlayerConnected(playerid) && AccountId[playerid] == accountDbId) Kick(playerid);
    return 1;
}

public Moderation_OnBanSaved(actorId, targetId, accountDbId)
{
    #pragma unused actorId, targetId

    if (mysql_errno(gSQL) != 0)
    {
        Log_Format(LOG_ERROR, "[BAN] insert gagal errno=%d", mysql_errno(gSQL));
        return 0;
    }

    /* Flag di akun adalah jalur cepat pemeriksaan login. Pakai account id yang
       ditangkap saat ban dibuat, BUKAN AccountId[targetId] saat callback:
       slot bisa sudah dihuni pemain lain (race slot-reuse). */
    new query[128];
    format(query, sizeof(query), "UPDATE accounts SET is_banned = 1 WHERE id = %d", accountDbId);
    mysql_tquery(gSQL, query, "", "");
    return 1;
}

stock Moderation_Unban(actorId, const username[])
{
    if (!IsServerSuperAdmin(actorId)) return 0;

    if (username[0] == EOS)
    {
        SendClientMessage(actorId, UI_ACCENT_WARN, "Pemakaian: /unban [nama akun]");
        return 0;
    }

    new query[192];
    mysql_format(gSQL, query, sizeof(query),
        "SELECT id FROM accounts WHERE username = '%e' LIMIT 1", username);
    mysql_tquery(gSQL, query, "Moderation_OnUnbanLoaded", "is", actorId, username);
    return 1;
}

public Moderation_OnUnbanLoaded(adminId, const username[])
{
    if (!IsPlayerConnected(adminId)) return 0;

    if (cache_num_rows() == 0)
    {
        SendClientMessage(adminId, UI_ACCENT_WARN, "[UNBAN] Akun tidak ditemukan.");
        return 0;
    }

    new accountDbId;
    cache_get_value_name_int(0, "id", accountDbId);

    new query[192];
    format(query, sizeof(query), "UPDATE accounts SET is_banned = 0 WHERE id = %d", accountDbId);
    mysql_tquery(gSQL, query, "", "");

    /* Ban aktif ditutup dengan menandai kedaluwarsa sekarang, riwayatnya tetap ada. */
    format(query, sizeof(query),
        "UPDATE bans SET expires_at = %d WHERE account_id = %d AND (expires_at = 0 OR expires_at > %d)",
        gettime(), accountDbId, gettime());
    mysql_tquery(gSQL, query, "", "");

    new line[160];
    format(line, sizeof(line), "[UNBAN] Akun %s dibuka. Riwayat ban tetap tersimpan.", username);
    SendClientMessage(adminId, UI_ACCENT_PRIMARY, line);
    Log_Format(LOG_INFO, "[UNBAN] admin=%d account=%d", adminId, accountDbId);

    new auditDetail[96];
    format(auditDetail, sizeof(auditDetail), "akun=%s", username);
    AdminAudit_Log(adminId, "unban", accountDbId, auditDetail);
    return 1;
}

/* Dipanggil dari login saat is_banned = 1: permanen, masih aktif, atau kedaluwarsa. */
stock Moderation_CheckBan(playerid, accountDbId)
{
    new query[192];
    format(query, sizeof(query),
        "SELECT reason, expires_at FROM bans WHERE account_id = %d ORDER BY (expires_at = 0) DESC, expires_at DESC LIMIT 1",
        accountDbId);
    mysql_tquery(gSQL, query, "Moderation_OnBanResolved", "ii", playerid, accountDbId);
    return 1;
}

public Moderation_OnBanResolved(playerid, accountDbId)
{
    if (!IsPlayerConnected(playerid)) return 0;

    if (cache_num_rows() == 0)
    {
        /* Flag tanpa baris ban (mis. di-set manual): tetap blokir, tanpa detail. */
        SendClientMessage(playerid, UI_ACCENT_DANGER, "[AKUN] Akun ini diblokir.");
        SetTimerEx("Moderation_KickTarget", 1200, false, "ii", playerid, accountDbId);
        return 1;
    }

    new expiresAt, reason[BAN_MAX_REASON + 1];
    cache_get_value_name_int(0, "expires_at", expiresAt);
    cache_get_value_name(0, "reason", reason, sizeof(reason));

    if (expiresAt != 0 && gettime() >= expiresAt)
    {
        new query[128];
        format(query, sizeof(query), "UPDATE accounts SET is_banned = 0 WHERE id = %d", accountDbId);
        mysql_tquery(gSQL, query, "", "");

        SendClientMessage(playerid, UI_ACCENT_PRIMARY, "[AKUN] Masa blokir selesai. Silakan masuk.");
        /* Lookup ulang penuh: cabang is_banned di OnLookup return SEBELUM
           AccountId/hash/stage di-set, jadi ShowLogin saja = jalan buntu
           (/masuk ditolak, hash kosong). BeginAuth mengulang lookup dari awal
           dengan flag ban yang sudah 0 (audit #5). */
        Account_BeginAuth(playerid);
        Log_Format(LOG_INFO, "[BAN] ban kedaluwarsa dibuka otomatis account=%d", accountDbId);
        return 1;
    }

    new line[192];
    if (expiresAt == 0)
    {
        format(line, sizeof(line), "[AKUN] Diblokir permanen: %s", reason);
    }
    else
    {
        new remaining = (expiresAt - gettime()) / 60;
        format(line, sizeof(line), "[AKUN] Diblokir %d menit lagi: %s", remaining, reason);
    }
    SendClientMessage(playerid, UI_ACCENT_DANGER, line);
    SetTimerEx("Moderation_KickTarget", 1500, false, "ii", playerid, accountDbId);
    return 1;
}

stock Moderation_ShowBans(actorId)
{
    if (!IsServerAdmin(actorId)) return 0;

    new query[224];
    format(query, sizeof(query),
        "SELECT a.username, b.reason, b.expires_at, b.admin_name, b.created_at \
         FROM bans b JOIN accounts a ON a.id = b.account_id \
         ORDER BY b.created_at DESC LIMIT 8");
    mysql_tquery(gSQL, query, "Moderation_OnBansLoaded", "i", actorId);
    return 1;
}

forward Moderation_OnBansLoaded(adminId);
public Moderation_OnBansLoaded(adminId)
{
    if (!IsPlayerConnected(adminId)) return 0;

    if (cache_num_rows() == 0)
    {
        SendClientMessage(adminId, UI_TEXT_MUTED, "[BAN] Belum ada riwayat ban.");
        return 0;
    }

    SendClientMessage(adminId, UI_ACCENT_PRIMARY, "[BAN] Riwayat terakhir:");
    for (new row = 0; row < cache_num_rows(); row++)
    {
        new username[32], reason[BAN_MAX_REASON + 1], adminName[32], expiresAt, createdAt;
        cache_get_value_name(row, "username", username, sizeof(username));
        cache_get_value_name(row, "reason", reason, sizeof(reason));
        cache_get_value_name(row, "admin_name", adminName, sizeof(adminName));
        cache_get_value_name_int(row, "expires_at", expiresAt);
        cache_get_value_name_int(row, "created_at", createdAt);

        new line[224], status[48];
        if (expiresAt == 0) format(status, sizeof(status), "permanen");
        else if (gettime() >= expiresAt) format(status, sizeof(status), "kedaluwarsa");
        else format(status, sizeof(status), "%d menit lagi", (expiresAt - gettime()) / 60);

        format(line, sizeof(line), "%s | %s | oleh %s | %s", username, reason, adminName, status);
        SendClientMessage(adminId, UI_TEXT_SECONDARY, line);
        #pragma unused createdAt
    }
    return 1;
}
/* Mute sementara (ROADMAP 401). 0 = tidak dibisukan. */
stock Moderation_Mute(actorId, targetId, minutes, const reason[])
{
    if (!IsServerAdmin(actorId)) return 0;
    if (!IsPlayerConnected(targetId) || IsPlayerNPC(targetId)) return 0;
    if (minutes <= 0) minutes = 10;
    if (minutes > MUTE_MAX_MINUTES) minutes = MUTE_MAX_MINUTES;

    new until = gettime() + minutes * 60;
    if (until > PlayerMutedUntil[targetId]) PlayerMutedUntil[targetId] = until;

    new reasonText[80];
    format(reasonText, sizeof(reasonText), "%s", (reason[0] == EOS) ? "tanpa alasan" : reason);

    new targetName[MAX_PLAYER_NAME + 1];
    GetPlayerName(targetId, targetName, sizeof(targetName));

    new line[176];
    format(line, sizeof(line), "[MUTE] %s dibisukan %d menit: %s", targetName, minutes, reasonText);
    SendClientMessage(actorId, UI_ACCENT_PRIMARY, line);

    format(line, sizeof(line), "[MUTE] Kamu dibisukan %d menit oleh staff: %s", minutes, reasonText);
    SendClientMessage(targetId, UI_ACCENT_WARN, line);

    new auditDetail[ADMIN_AUDIT_DETAIL_MAX + 1];
    format(auditDetail, sizeof(auditDetail), "menit=%d alasan=%s", minutes, reasonText);
    AdminAudit_Log(actorId, "mute", AccountId[targetId], auditDetail);
    return 1;
}

stock Moderation_Unmute(actorId, targetId)
{
    if (!IsServerAdmin(actorId)) return 0;
    if (!IsPlayerConnected(targetId) || IsPlayerNPC(targetId)) return 0;
    if (PlayerMutedUntil[targetId] == 0) return 0;

    PlayerMutedUntil[targetId] = 0;

    new targetName[MAX_PLAYER_NAME + 1];
    GetPlayerName(targetId, targetName, sizeof(targetName));

    new line[128];
    format(line, sizeof(line), "[MUTE] %s bisa bicara lagi.", targetName);
    SendClientMessage(actorId, UI_ACCENT_PRIMARY, line);
    SendClientMessage(targetId, UI_ACCENT_PRIMARY, "[MUTE] Mute kamu dicabut staff.");

    AdminAudit_Log(actorId, "unmute", AccountId[targetId], "dicabut manual");
    return 1;
}

/* Peringatan bertingkat: 3 peringatan dalam satu sesi berakhir mute 15 menit.
   Ambang disimpan sebagai konstanta supaya bisa diuji tanpa chat. */
stock Moderation_Warn(actorId, targetId, const reason[])
{
    if (!IsServerAdmin(actorId)) return 0;
    if (!IsPlayerConnected(targetId) || IsPlayerNPC(targetId)) return 0;

    new reasonText[96];
    format(reasonText, sizeof(reasonText), "%s", (reason[0] == EOS) ? "tanpa alasan" : reason);

    new targetName[MAX_PLAYER_NAME + 1];
    GetPlayerName(targetId, targetName, sizeof(targetName));

    PlayerWarnCount[targetId]++;
    new count = PlayerWarnCount[targetId];

    new line[176];
    format(line, sizeof(line), "[WARN] %s diperingatkan (%d/%d): %s",
        targetName, count, WARN_BEFORE_MUTE, reasonText);
    SendClientMessage(actorId, UI_ACCENT_PRIMARY, line);

    format(line, sizeof(line), "[WARN] Peringatan %d/%d dari staff: %s", count, WARN_BEFORE_MUTE, reasonText);
    SendClientMessage(targetId, UI_ACCENT_WARN, line);

    new auditDetail[ADMIN_AUDIT_DETAIL_MAX + 1];
    format(auditDetail, sizeof(auditDetail), "peringatan=%d alasan=%s", count, reasonText);
    AdminAudit_Log(actorId, "warn", AccountId[targetId], auditDetail);

    if (count >= WARN_BEFORE_MUTE)
    {
        PlayerWarnCount[targetId] = 0;
        Moderation_Mute(actorId, targetId, WARN_MUTE_MINUTES, "akumulasi peringatan");
    }
    return 1;
}

stock bool:Moderation_IsMuted(playerid)
{
    if (playerid < 0 || playerid >= MAX_PLAYERS) return false;
    if (PlayerMutedUntil[playerid] == 0) return false;
    if (gettime() >= PlayerMutedUntil[playerid])
    {
        PlayerMutedUntil[playerid] = 0;
        return false;
    }
    return true;
}

/* Sisa mute untuk pesan ke pemain. */
stock Moderation_MuteSecondsLeft(playerid)
{
    if (!Moderation_IsMuted(playerid)) return 0;
    return PlayerMutedUntil[playerid] - gettime();
}

stock Moderation_ResetPlayerState(playerid)
{
    if (playerid < 0 || playerid >= MAX_PLAYERS) return 0;
    PlayerMutedUntil[playerid] = 0;
    PlayerWarnCount[playerid] = 0;
    return 1;
}
