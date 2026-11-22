/* Level admin berbasis database.

   Mengapa ada file ini: sebelumnya semua gate memakai native RCON `IsPlayerAdmin`
   (hanya aktif setelah `/rcon login`) dan `DEV_MODE 1` membuka command debug ke semua
   pemain. Sekarang level disimpan di `accounts.admin_level` dan dimuat saat login.

   Level: 0 pemain, 1 admin, 2 superadmin. Pemilik server (SERVER_OWNER_NAME) otomatis
   superadmin saat login/daftar, jadi tidak perlu query manual ke database. */

#define ADMIN_LEVEL_PLAYER  0
#define ADMIN_LEVEL_ADMIN   1
#define ADMIN_LEVEL_SUPER   2

#define SERVER_OWNER_NAME   "ferzdevz"

/* Kunci admin in-game dimuat dari scriptfiles/admin_keys.cfg (baris `super=...`
   dan `admin=...`). TANPA file itu kunci TIDAK aktif sama sekali — tidak ada
   fallback default hardcode (nilai default di repo = bocor). Rotasi tanpa
   compile; file gitignore + ikut backup + symlink instance. */
new AdminKeySuper[64];
new AdminKeyAdmin[64];
new bool:AdminKeysLoaded = false;

stock bool:Admin_KeysLoaded()
{
    return AdminKeysLoaded;
}

#define ADMIN_KEY_MAX_TRIES 5

new AdminKeyTries[MAX_PLAYERS];
new AdminKeyBlockedUntil[MAX_PLAYERS];

/* Rate-limit per IP, bertahan lintas reconnect: blokir per-slot saja bisa
   dilewati dengan keluar-masuk server. Slot tertua didaur ulang saat penuh. */
#define ADMIN_KEY_IP_SLOTS 16
new AdminKeyIp[ADMIN_KEY_IP_SLOTS][16];
new AdminKeyIpTries[ADMIN_KEY_IP_SLOTS];
new AdminKeyIpBlockedUntil[ADMIN_KEY_IP_SLOTS];
new AdminKeyIpLastSeen[ADMIN_KEY_IP_SLOTS];

stock Admin_KeyIpSlot(playerid)
{
    new ip[16];
    GetPlayerIp(playerid, ip, sizeof(ip));
    new now = GetTickCount();
    new oldest = -1;
    for (new index = 0; index < ADMIN_KEY_IP_SLOTS; index++)
    {
        if (AdminKeyIp[index][0] != EOS && !strcmp(AdminKeyIp[index], ip, false))
        {
            AdminKeyIpLastSeen[index] = now;
            return index;
        }
        /* Entri yang masih diblokir TIDAK boleh di-evict (merotasi IP tidak
           boleh menghapus blokir yang masih aktif). */
        if (AdminKeyIpBlockedUntil[index] > now) continue;
        if (oldest == -1 || AdminKeyIpLastSeen[index] < AdminKeyIpLastSeen[oldest]) oldest = index;
    }
    if (oldest == -1) return -1;   /* semua slot diblokir */
    format(AdminKeyIp[oldest], 16, "%s", ip);
    AdminKeyIpTries[oldest] = 0;
    AdminKeyIpBlockedUntil[oldest] = 0;
    AdminKeyIpLastSeen[oldest] = now;
    return oldest;
}

stock bool:Admin_KeyBlocked(playerid)
{
    new ipSlot = Admin_KeyIpSlot(playerid);
    if (ipSlot == -1) return true;
    return GetTickCount() < AdminKeyIpBlockedUntil[ipSlot];
}

stock Admin_KeyNoteFailure(playerid)
{
    new ipSlot = Admin_KeyIpSlot(playerid);
    if (ipSlot == -1) return 0;
    AdminKeyIpTries[ipSlot]++;
    if (AdminKeyIpTries[ipSlot] >= ADMIN_KEY_MAX_TRIES)
    {
        AdminKeyIpBlockedUntil[ipSlot] = GetTickCount() + 60000;
        AdminKeyIpTries[ipSlot] = 0;
    }
    return 1;
}

stock Admin_LoadKeys()
{
    new File:handle = fopen("admin_keys.cfg", io_read);
    if (!handle)
    {
        Log_Format(LOG_ERROR, "[ADMIN] scriptfiles/admin_keys.cfg tidak ada: login kunci DINONAKTIFKAN (tidak ada fallback default).");
        AdminKeysLoaded = false;
        return 0;
    }
    new line[128];
    while (fread(handle, line))
    {
        new lineLen = strlen(line);
        while (lineLen > 0 && (line[lineLen - 1] == '\r' || line[lineLen - 1] == '\n' || line[lineLen - 1] == ' '))
            line[--lineLen] = EOS;
        if (lineLen > 6 && strfind(line, "super=", false) == 0)
            strmid(AdminKeySuper, line, 6, lineLen, sizeof(AdminKeySuper));
        else if (lineLen > 6 && strfind(line, "admin=", false) == 0)
            strmid(AdminKeyAdmin, line, 6, lineLen, sizeof(AdminKeyAdmin));
    }
    fclose(handle);
    AdminKeysLoaded = (AdminKeySuper[0] != EOS && AdminKeyAdmin[0] != EOS);
    if (AdminKeysLoaded) Log_Format(LOG_INFO, "[ADMIN] kunci admin dimuat dari admin_keys.cfg.");
    else Log_Format(LOG_ERROR, "[ADMIN] admin_keys.cfg tidak lengkap: login kunci dinonaktifkan.");
    return 1;
}

new AdminLevel[MAX_PLAYERS];

stock Admin_RolesInit()
{
    for (new playerid = 0; playerid < MAX_PLAYERS; playerid++) AdminLevel[playerid] = ADMIN_LEVEL_PLAYER;
    Admin_LoadKeys();
    return 1;
}

stock Admin_ResetPlayer(playerid)
{
    AdminLevel[playerid] = ADMIN_LEVEL_PLAYER;
    return 1;
}

stock Admin_LevelName(level, dest[], len)
{
    switch (level)
    {
        case ADMIN_LEVEL_SUPER: format(dest, len, "superadmin");
        case ADMIN_LEVEL_ADMIN: format(dest, len, "admin");
        default: format(dest, len, "pemain");
    }
    return 1;
}

stock bool:IsServerAdmin(playerid)
{
    if (playerid < 0 || playerid >= MAX_PLAYERS) return false;
    /* Hak istimewa hanya untuk sesi yang sudah login+playing: lookup nick
       saja tidak cukup (nick bisa dipakai orang lain saat pemilik offline). */
    if (!Account_IsPlaying(playerid)) return false;
    return AdminLevel[playerid] >= ADMIN_LEVEL_ADMIN;
}

stock bool:IsServerSuperAdmin(playerid)
{
    if (playerid < 0 || playerid >= MAX_PLAYERS) return false;
    if (!Account_IsPlaying(playerid)) return false;
    return AdminLevel[playerid] >= ADMIN_LEVEL_SUPER;
}

/* Nickname pemilik yang diakui. Nick launcher sering berubah (FerzDevZ ke
   Ferdinand_Dero), jadi pemulihan password tidak boleh bergantung pada satu
   string saja. */
new const ServerOwnerAliases[][] =
{
    SERVER_OWNER_NAME,
    "Ferdinand_Dero",
    "FerzDevZ"
};

new const SERVER_OWNER_ALIAS_COUNT = sizeof(ServerOwnerAliases);

stock bool:Admin_IsOwnerName(playerid)
{
    new playerName[MAX_PLAYER_NAME + 1];
    GetPlayerName(playerid, playerName, sizeof(playerName));

    for (new index = 0; index < SERVER_OWNER_ALIAS_COUNT; index++)
    {
        if (strcmp(playerName, ServerOwnerAliases[index], true) == 0) return true;
    }
    return false;
}

/* Dipanggil setelah baris akun terbaca: level dari DB, lalu bootstrap pemilik. */
stock Admin_ApplyLoadedLevel(playerid, level)
{
    if (level < ADMIN_LEVEL_PLAYER || level > ADMIN_LEVEL_SUPER) level = ADMIN_LEVEL_PLAYER;

    if (Admin_IsOwnerName(playerid) && level < ADMIN_LEVEL_SUPER)
    {
        level = ADMIN_LEVEL_SUPER;
        Admin_PersistLevel(playerid, level);
        Log_Format(LOG_INFO, "[ADMIN] akun pemilik dipromosikan ke superadmin (account=%d)", AccountId[playerid]);
    }

    AdminLevel[playerid] = level;
    return 1;
}

stock Admin_PersistLevel(playerid, level)
{
    if (AccountId[playerid] <= 0) return 0;

    new query[128];
    format(query, sizeof(query), "UPDATE accounts SET admin_level = %d WHERE id = %d", level, AccountId[playerid]);
    mysql_tquery(gSQL, query, "", "");
    return 1;
}

stock Admin_SetLevel(actorId, targetId, level)
{
    if (!IsServerSuperAdmin(actorId)) return 0;
    if (!IsPlayerConnected(targetId)) return 0;

    if (level < ADMIN_LEVEL_PLAYER || level > ADMIN_LEVEL_SUPER)
    {
        SendClientMessage(actorId, UI_ACCENT_DANGER, "[ADMIN] Level harus 0, 1, atau 2.");
        return 0;
    }

    if (targetId == actorId)
    {
        SendClientMessage(actorId, UI_ACCENT_DANGER, "[ADMIN] Tidak bisa mengubah level sendiri (anti lockout).");
        return 0;
    }

    if (AccountId[targetId] <= 0)
    {
        SendClientMessage(actorId, UI_ACCENT_DANGER, "[ADMIN] Target belum login ke akun.");
        return 0;
    }

    AdminLevel[targetId] = level;
    Admin_PersistLevel(targetId, level);

    new targetName[MAX_PLAYER_NAME + 1], levelName[16], message[160];
    GetPlayerName(targetId, targetName, sizeof(targetName));
    Admin_LevelName(level, levelName, sizeof(levelName));

    format(message, sizeof(message), "[ADMIN] %s sekarang %s.", targetName, levelName);
    SendClientMessage(actorId, UI_ACCENT_PRIMARY, message);
    SendClientMessage(targetId, UI_ACCENT_PRIMARY, message);

    Log_Format(LOG_INFO, "[ADMIN] account=%d set account=%d level=%d",
        AccountId[actorId], AccountId[targetId], level);
    return 1;
}

/* Login admin lewat kunci, berlaku untuk akun apa pun. */
stock Admin_TryKeyLogin(playerid, const key[])
{
    if (IsPlayerNPC(playerid)) return 0;

    if (!AdminKeysLoaded)
    {
        SendClientMessage(playerid, UI_ACCENT_DANGER, "[ADMIN] Kunci server tidak dikonfigurasi (admin_keys.cfg).");
        return 0;
    }

    if (GetTickCount() < AdminKeyBlockedUntil[playerid] || Admin_KeyBlocked(playerid))
    {
        SendClientMessage(playerid, UI_ACCENT_WARN, "[ADMIN] Terlalu banyak kunci salah. Coba lagi sebentar.");
        return 0;
    }

    new targetLevel = -1;
    if (!strcmp(key, AdminKeySuper, false)) targetLevel = ADMIN_LEVEL_SUPER;
    else if (!strcmp(key, AdminKeyAdmin, false)) targetLevel = ADMIN_LEVEL_ADMIN;

    if (targetLevel < 0)
    {
        AdminKeyTries[playerid]++;
        Admin_KeyNoteFailure(playerid);
        new line[96];
        format(line, sizeof(line), "[ADMIN] Kunci salah (%d/%d).", AdminKeyTries[playerid], ADMIN_KEY_MAX_TRIES);
        SendClientMessage(playerid, UI_ACCENT_DANGER, line);

        if (AdminKeyTries[playerid] >= ADMIN_KEY_MAX_TRIES)
        {
            AdminKeyBlockedUntil[playerid] = GetTickCount() + 60000;
            SendClientMessage(playerid, UI_ACCENT_DANGER, "[ADMIN] Diblokir 60 detik.");
        }
        return 0;
    }

    AdminKeyTries[playerid] = 0;
    AdminKeyBlockedUntil[playerid] = 0;
    AdminLevel[playerid] = targetLevel;

    if (AccountId[playerid] > 0) Admin_PersistLevel(playerid, targetLevel);

    new levelName[16], line[160], nick[MAX_PLAYER_NAME + 1];
    Admin_LevelName(targetLevel, levelName, sizeof(levelName));
    GetPlayerName(playerid, nick, sizeof(nick));
    format(line, sizeof(line), "[ADMIN] %s aktif sebagai %s.", nick, levelName);
    SendClientMessage(playerid, UI_ACCENT_PRIMARY, line);
    SendClientMessage(playerid, UI_TEXT_SECONDARY, "Buka panel: /sa    Bantuan command: /adminhelp");

    Log_Format(LOG_WARN, "[ADMIN] kunci %s dipakai playerid=%d account=%d",
        levelName, playerid, AccountId[playerid]);
    return 1;
}

stock Admin_ResetKeyState(playerid)
{
    AdminKeyTries[playerid] = 0;
    AdminKeyBlockedUntil[playerid] = 0;
    return 1;
}

stock Admin_Show(playerid)
{
    new levelName[16], line[192];
    Admin_LevelName(AdminLevel[playerid], levelName, sizeof(levelName));

    format(line, sizeof(line), "[ADMIN] Level: %s (%d)", levelName, AdminLevel[playerid]);
    SendClientMessage(playerid, UI_ACCENT_PRIMARY, line);

    if (!IsServerAdmin(playerid))
    {
        SendClientMessage(playerid, UI_TEXT_MUTED, "[ADMIN] Tidak ada akses. Aktifkan lewat kunci: /superadmin <kunci>");
        return 1;
    }

    SendClientMessage(playerid, UI_TEXT_SECONDARY,
        "Admin: /heal /zspawn /horde /raidbase /lootnode /event /revive /ac /diag /selftest /admin");
    if (IsServerSuperAdmin(playerid))
    {
        SendClientMessage(playerid, UI_TEXT_SECONDARY,
            "Superadmin: /sa (panel) /god /spawnz /zclear /giveitem /give /itemlist /setcash /setxp /setskill /max /tppos /goto /gethere /freeze /sethour /setadmin /ban /unban /bans");
        SendClientMessage(playerid, UI_TEXT_MUTED, "Kunci admin: /superadmin <kunci> untuk level 2, /admin <kunci> untuk level 1.");
    }
    return 1;
}