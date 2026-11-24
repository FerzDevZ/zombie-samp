/* Perkakas superadmin: spawn zombie, item, kebal, set stat, teleport, waktu.

   Semua aksi hanya untuk level >= ADMIN_LEVEL_SUPER kecuali yang ditandai admin.
   Aturan yang dipegang:
   - item TIDAK pernah ditulis langsung ke array inventory: selalu `Inventory_Grant`
     (weight + stack + ledger), dengan alasan `admin_grant` supaya bisa diaudit.
   - setiap teleport memanggil `AC_ClearSample` (anti teleport-hack).
   - kebal hanya memblokir `Damage_Apply` (DAMAGE_KIND_PLAYER), jadi racun/kelaparan
     juga tidak melukai, tapi zombie tetap bisa dihancurkan oleh pemain lain.
   - setiap aksi dicatat `Log_Format` dengan account id pelaku. */

#if defined _ls_superadmin_included
    #endinput
#endif
#define _ls_superadmin_included

#define ADMIN_CASH_MAX          10000000
#define ADMIN_ITEM_MAX          500
#define ADMIN_SPAWN_MAX         30
#define ADMIN_ITEMLIST_PAGE     8

/* AdminFrozen dideklarasikan di admin_state.inc (include lebih awal,
   dipakai achecks.inc). */

stock Admin_ItemIdFromArg(const arg[])
{
    if (arg[0] == EOS) return -1;

    /* Angka langsung = item id; selain itu cari nama item (tanpa peduli besar-kecil). */
    if (arg[0] >= '0' && arg[0] <= '9')
    {
        new itemid = strval(arg);
        return Item_IsValid(itemid) ? itemid : -1;
    }

    new itemLabel[24];
    for (new itemid = 1; itemid < ITEM_COUNT; itemid++)
    {
        Item_GetName(itemid, itemLabel, sizeof(itemLabel));
        if (strcmp(itemLabel, arg, true) == 0) return itemid;
    }
    return -1;
}

stock Admin_TargetFromArg(adminId, const arg[])
{
    if (arg[0] == EOS || !strcmp(arg, "me", true) || !strcmp(arg, "saya", true)) return adminId;

    new target = strval(arg);
    if (!IsPlayerConnected(target)) return INVALID_PLAYER_ID;
    return target;
}

stock bool:Admin_ZombieTypeFromArg(const arg[], &E_ZOMBIE_TYPE:type)
{
    if (!strcmp(arg, "walker", true) || !strcmp(arg, "jalan", true)) { type = ZT_WALKER; return true; }
    if (!strcmp(arg, "runner", true) || !strcmp(arg, "lari", true)) { type = ZT_RUNNER; return true; }
    if (!strcmp(arg, "crawler", true) || !strcmp(arg, "crawl", true) || !strcmp(arg, "merangkak", true)) { type = ZT_CRAWLER; return true; }
    if (!strcmp(arg, "brute", true) || !strcmp(arg, "besar", true)) { type = ZT_BRUTE; return true; }
    if (!strcmp(arg, "bloater", true) || !strcmp(arg, "bau", true)) { type = ZT_BLOATER; return true; }
    if (!strcmp(arg, "screamer", true) || !strcmp(arg, "teriak", true)) { type = ZT_SCREAMER; return true; }
    if (!strcmp(arg, "stalker", true) || !strcmp(arg, "pemburu", true)) { type = ZT_STALKER; return true; }
    if (!strcmp(arg, "boss", true) || !strcmp(arg, "abomination", true)) { type = ZT_BOSS; return true; }
    return false;
}

stock Admin_SpawnZombies(playerid, E_ZOMBIE_TYPE:type, count)
{
    if (!IsServerSuperAdmin(playerid)) return 0;
    if (count < 1) count = 1;
    if (count > ADMIN_SPAWN_MAX) count = ADMIN_SPAWN_MAX;

    new Float:x, Float:y, Float:z;
    GetPlayerPos(playerid, x, y, z);
    new lobby = Lobby_Of(playerid);
    new spawned = 0;

    for (new index = 0; index < count; index++)
    {
        if (Zombie_GetActiveCount() >= MAX_ZOMBIES - 1) break;
        if (Director_SpawnNearPoint(type, x, y, z, 10.0, lobby, 3.0) == INVALID_ZOMBIE) break;
        spawned++;
    }

    new typeLabel[16], line[128];
    format(typeLabel, sizeof(typeLabel), "%s", gZombieProfiles[_:type][ztName]);
    format(line, sizeof(line), "[SA] Spawn %d x %s di lobi %d.", spawned, typeLabel, lobby);
    SendClientMessage(playerid, UI_ACCENT_PRIMARY, line);
    Log_Format(LOG_INFO, "[SA] spawn %d zombie tipe=%d oleh account=%d", spawned, _:type, AccountId[playerid]);
    return spawned;
}

stock Admin_ClearZombies(playerid)
{
    if (!IsServerSuperAdmin(playerid)) return 0;

    new lobby = Lobby_Of(playerid);
    new cleared = 0;

    foreach (new zombieid : ZombiePool)
    {
        if (!Zombie_IsActive(zombieid)) continue;
        if (Zombie_GetLobby(zombieid) != lobby) continue;

        Zombie_QueueDestroy(zombieid);
        cleared++;
    }

    new line[96];
    format(line, sizeof(line), "[SA] %d zombie di lobi %d dihapus.", cleared, lobby);
    SendClientMessage(playerid, UI_ACCENT_PRIMARY, line);
    Log_Format(LOG_INFO, "[SA] clear %d zombie lobi=%d oleh account=%d", cleared, lobby, AccountId[playerid]);
    return cleared;
}

stock Admin_GiveItem(adminId, targetId, itemid, amount)
{
    if (!IsServerSuperAdmin(adminId)) return 0;
    if (!IsPlayerConnected(targetId) || !Account_IsPlaying(targetId)) return 0;
    if (!Item_IsValid(itemid)) return 0;
    if (amount < 1) amount = 1;
    if (amount > ADMIN_ITEM_MAX) amount = ADMIN_ITEM_MAX;

    new itemLabel[24], targetName[MAX_PLAYER_NAME + 1], line[160];
    Item_GetName(itemid, itemLabel, sizeof(itemLabel));
    GetPlayerName(targetId, targetName, sizeof(targetName));

    if (!Inventory_Grant(targetId, itemid, amount, "admin_grant"))
    {
        format(line, sizeof(line), "[SA] Tas %s tidak cukup untuk %s x%d.", targetName, itemLabel, amount);
        SendClientMessage(adminId, UI_ACCENT_WARN, line);
        return 0;
    }

    format(line, sizeof(line), "[SA] %s x%d -> %s.", itemLabel, amount, targetName);
    SendClientMessage(adminId, UI_ACCENT_PRIMARY, line);
    format(line, sizeof(line), "[SA] Kamu menerima %s x%d dari admin.", itemLabel, amount);
    SendClientMessage(targetId, UI_ACCENT_PRIMARY, line);

    Log_Format(LOG_INFO, "[SA] grant item=%d x%d account=%d oleh account=%d",
        itemid, amount, ActiveCharId[targetId], AccountId[adminId]);
    return 1;
}

stock Admin_ListItems(playerid, page)
{
    if (!IsServerSuperAdmin(playerid)) return 0;
    if (page < 0) page = 0;

    new start = page * ADMIN_ITEMLIST_PAGE;
    if (start >= ITEM_COUNT)
    {
        SendClientMessage(playerid, UI_TEXT_MUTED, "[SA] Halaman di luar daftar item.");
        return 0;
    }

    new line[96], itemLabel[24];
    format(line, sizeof(line), "[SA] Item %d-%d dari %d:", start, start + ADMIN_ITEMLIST_PAGE - 1, ITEM_COUNT - 1);
    SendClientMessage(playerid, UI_ACCENT_PRIMARY, line);

    for (new itemid = start; itemid < ITEM_COUNT && itemid < start + ADMIN_ITEMLIST_PAGE; itemid++)
    {
        Item_GetName(itemid, itemLabel, sizeof(itemLabel));
        format(line, sizeof(line), "  %d = %s (%.1f kg)", itemid, itemLabel, Item_GetWeight(itemid));
        SendClientMessage(playerid, UI_TEXT_SECONDARY, line);
    }
    return 1;
}

stock Admin_GodToggle(adminId, targetId)
{
    if (!IsServerSuperAdmin(adminId)) return 0;
    if (!IsPlayerConnected(targetId)) return 0;

    AdminGod[targetId] = !AdminGod[targetId];

    new targetName[MAX_PLAYER_NAME + 1], line[128];
    GetPlayerName(targetId, targetName, sizeof(targetName));
    format(line, sizeof(line), "[SA] Mode kebal %s: %s", targetName, AdminGod[targetId] ? "AKTIF" : "MATI");
    SendClientMessage(adminId, UI_ACCENT_PRIMARY, line);
    SendClientMessage(targetId, UI_ACCENT_PRIMARY, line);

    if (!AdminGod[targetId])
    {
        /* Saat kebal dimatikan, sisa damage yang tertunda tidak diakumulasi. */
        AC_ClearSample(targetId);
    }
    Log_Format(LOG_INFO, "[SA] god account=%d -> %d oleh account=%d", ActiveCharId[targetId], AdminGod[targetId], AccountId[adminId]);
    return 1;
}

stock Admin_SetCash(adminId, targetId, amount)
{
    if (!IsServerSuperAdmin(adminId)) return 0;
    if (!IsPlayerConnected(targetId)) return 0;

    if (amount < 0) amount = 0;
    if (amount > ADMIN_CASH_MAX) amount = ADMIN_CASH_MAX;
    PlayerSurvival[targetId][psCash] = amount;

    new targetName[MAX_PLAYER_NAME + 1], line[128];
    GetPlayerName(targetId, targetName, sizeof(targetName));
    format(line, sizeof(line), "[SA] Uang %s = $%d.", targetName, amount);
    SendClientMessage(adminId, UI_ACCENT_PRIMARY, line);
    Log_Format(LOG_INFO, "[SA] cash account=%d = %d oleh account=%d", ActiveCharId[targetId], amount, AccountId[adminId]);
    return 1;
}

stock Admin_AddCash(adminId, targetId, amount)
{
    if (!IsServerSuperAdmin(adminId)) return 0;
    if (!IsPlayerConnected(targetId)) return 0;
    if (amount < 1) amount = 1;

    new current = PlayerSurvival[targetId][psCash] + amount;
    if (current > ADMIN_CASH_MAX) current = ADMIN_CASH_MAX;
    PlayerSurvival[targetId][psCash] = current;

    new targetName[MAX_PLAYER_NAME + 1], line[128];
    GetPlayerName(targetId, targetName, sizeof(targetName));
    format(line, sizeof(line), "[SA] Uang %s +$%d (total $%d).", targetName, amount, current);
    SendClientMessage(adminId, UI_ACCENT_PRIMARY, line);
    SendClientMessage(targetId, UI_TEXT_MUTED, line);
    Log_Format(LOG_INFO, "[SA] cash +%d account=%d total=%d oleh account=%d",
        amount, ActiveCharId[targetId], current, AccountId[adminId]);
    return 1;
}

stock Admin_AddXp(adminId, targetId, amount)
{
    if (!IsServerSuperAdmin(adminId)) return 0;
    if (!IsPlayerConnected(targetId)) return 0;
    if (amount < 1) amount = 1;
    if (amount > 100000) amount = 100000;

    Skills_AddXp(targetId, amount);

    new targetName[MAX_PLAYER_NAME + 1], line[128];
    GetPlayerName(targetId, targetName, sizeof(targetName));
    format(line, sizeof(line), "[SA] +%d XP untuk %s.", amount, targetName);
    SendClientMessage(adminId, UI_ACCENT_PRIMARY, line);
    Log_Format(LOG_INFO, "[SA] xp +%d account=%d oleh account=%d", amount, ActiveCharId[targetId], AccountId[adminId]);
    return 1;
}

stock Admin_SetSkillPoints(adminId, targetId, points)
{
    if (!IsServerSuperAdmin(adminId)) return 0;
    if (!IsPlayerConnected(targetId)) return 0;
    if (points < 0) points = 0;
    if (points > 99) points = 99;

    SkillPoints[targetId] = points;

    new targetName[MAX_PLAYER_NAME + 1], line[128];
    GetPlayerName(targetId, targetName, sizeof(targetName));
    format(line, sizeof(line), "[SA] Poin skill %s = %d.", targetName, points);
    SendClientMessage(adminId, UI_ACCENT_PRIMARY, line);
    Log_Format(LOG_INFO, "[SA] skillpoints account=%d = %d oleh account=%d", ActiveCharId[targetId], points, AccountId[adminId]);
    return 1;
}

stock Admin_FullRestore(adminId, targetId)
{
    if (!IsServerAdmin(adminId)) return 0;
    if (!IsPlayerConnected(targetId)) return 0;

    PlayerSurvival[targetId][psHealth] = 100.0;
    PlayerSurvival[targetId][psArmour] = 100.0;
    PlayerSurvival[targetId][psHunger] = 100.0;
    PlayerSurvival[targetId][psThirst] = 100.0;
    PlayerSurvival[targetId][psBladder] = 0.0;
    PlayerSurvival[targetId][psInfection] = 0.0;
    /* Stage & downed ikut direset: tanpa ini pemain turned (stage 3) tetap
       turned walau HP 100 — Needs_Tick men-downed-kan lagi (audit #10). */
    PlayerSurvival[targetId][psInfectionStage] = 0;
    PlayerSurvival[targetId][psDowned] = 0;
    PlayerSurvival[targetId][psWounds] = 0;

    SetPlayerHealth(targetId, 100.0);
    SetPlayerArmour(targetId, 100.0);
    SetPlayerDrunkLevel(targetId, 0);

    new targetName[MAX_PLAYER_NAME + 1], line[128];
    GetPlayerName(targetId, targetName, sizeof(targetName));
    format(line, sizeof(line), "[SA] %s dipulihkan penuh (HP, armour, kebutuhan, infeksi, luka).", targetName);
    SendClientMessage(adminId, UI_ACCENT_PRIMARY, line);
    SendClientMessage(targetId, UI_ACCENT_PRIMARY, "[SA] Kamu dipulihkan penuh oleh admin.");
    return 1;
}

stock Admin_Teleport(adminId, targetId, Float:x, Float:y, Float:z)
{
    if (!IsServerSuperAdmin(adminId)) return 0;
    if (!IsPlayerConnected(targetId)) return 0;

    AC_ClearSample(targetId);
    SetPlayerPos(targetId, x, y, z);
    SetPlayerVelocity(targetId, 0.0, 0.0, 0.0);

    new line[128];
    format(line, sizeof(line), "[SA] Teleport ke %.1f, %.1f, %.1f.", x, y, z);
    SendClientMessage(adminId, UI_ACCENT_PRIMARY, line);
    Log_Format(LOG_INFO, "[SA] tp account=%d ke %.1f %.1f %.1f oleh account=%d",
        ActiveCharId[targetId], x, y, z, AccountId[adminId]);
    return 1;
}

stock Admin_TeleportToPlayer(adminId, targetId)
{
    if (!IsServerSuperAdmin(adminId)) return 0;
    if (!IsPlayerConnected(targetId)) return 0;

    new Float:x, Float:y, Float:z;
    GetPlayerPos(targetId, x, y, z);
    Admin_Teleport(adminId, adminId, x + 1.0, y + 1.0, z);
    return 1;
}

stock Admin_BringPlayer(adminId, targetId)
{
    if (!IsServerSuperAdmin(adminId)) return 0;
    if (!IsPlayerConnected(targetId)) return 0;

    new Float:x, Float:y, Float:z;
    GetPlayerPos(adminId, x, y, z);
    Admin_Teleport(adminId, targetId, x + 1.0, y + 1.0, z);

    new targetName[MAX_PLAYER_NAME + 1], line[128];
    GetPlayerName(targetId, targetName, sizeof(targetName));
    format(line, sizeof(line), "[SA] %s ditarik ke posisimu.", targetName);
    SendClientMessage(adminId, UI_ACCENT_PRIMARY, line);
    return 1;
}

stock Admin_FreezeToggle(adminId, targetId)
{
    if (!IsServerAdmin(adminId)) return 0;
    if (!IsPlayerConnected(targetId)) return 0;

    AdminFrozen[targetId] = !AdminFrozen[targetId];
    TogglePlayerControllable(targetId, AdminFrozen[targetId] ? 0 : 1);

    new targetName[MAX_PLAYER_NAME + 1], line[128];
    GetPlayerName(targetId, targetName, sizeof(targetName));
    format(line, sizeof(line), "[SA] %s %s.", targetName, AdminFrozen[targetId] ? "dibekukan" : "dilepas");
    SendClientMessage(adminId, UI_ACCENT_PRIMARY, line);
    return 1;
}

stock Admin_SetHour(adminId, hour)
{
    if (!IsServerSuperAdmin(adminId)) return 0;

    World_SetHour(hour);

    new line[96];
    format(line, sizeof(line), "[SA] Waktu dunia di-set ke jam %02d:00.", World_GetHour());
    SendClientMessage(adminId, UI_ACCENT_PRIMARY, line);
    foreach (new playerid : PlayerPool) SendClientMessage(playerid, UI_TEXT_MUTED, line);
    Log_Format(LOG_INFO, "[SA] hour=%d oleh account=%d", World_GetHour(), AccountId[adminId]);
    return 1;
}

stock Admin_ResetSession(playerid)
{
    AdminFrozen[playerid] = 0;
    Admin_ResetGod(playerid);
    return 1;
}
