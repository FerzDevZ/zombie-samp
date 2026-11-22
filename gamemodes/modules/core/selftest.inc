#if defined _ls_selftest_included
    #endinput
#endif
#define _ls_selftest_included

/* Self-test server-side: membuktikan konfigurasi, registry, dan tabel DB konsisten
   tanpa butuh client. Dijalankan otomatis 8 detik setelah boot dan lewat /selftest. */

#define DIALOG_SELFTEST 2801

forward Selftest_Boot();
forward Selftest_OnTablesLoaded(playerid);

new SelftestChecks = 0;
new SelftestFailures = 0;
new SelftestReport[2048];
new bool:SelftestRunning = false;

stock Selftest_Reset()
{
    SelftestChecks = 0;
    SelftestFailures = 0;
    SelftestReport[0] = EOS;
    return 1;
}

stock Selftest_Record(bool:passed, const name[], const detail[])
{
    SelftestChecks++;

    new line[192];
    format(line, sizeof(line), "%s %s%s%s", passed ? "[SELFTEST] PASS" : "[SELFTEST] FAIL",
        name, (detail[0] == EOS) ? "" : " | ", detail);

    if (passed) Log_Format(LOG_INFO, "%s", line);
    else
    {
        SelftestFailures++;
        Log_Format(LOG_ERROR, "%s", line);
    }
    return 1;
}

stock Selftest_GroundCheck(const name[], Float:x, Float:y, Float:z)
{
    new Float:hitX, Float:hitY, Float:hitZ;
    new model = CA_RayCastLine(x, y, z + 100.0, x, y, z - 100.0, hitX, hitY, hitZ);

    new detail[128];
    if (model == 0) format(detail, sizeof(detail), "%s: tidak ada tanah di %.1f/%.1f", name, x, y);
    else format(detail, sizeof(detail), "%s: model %d z=%.2f", name, model, hitZ);

    Selftest_Record(model != 0, "tanah", detail);
    return 1;
}

stock Selftest_CheckItems()
{
    new invalidCount = 0;
    new detail[160];

    for (new itemid = 1; itemid < ITEM_COUNT; itemid++)
    {
        if (!Item_IsValid(itemid)) invalidCount++;
        else if (Item_GetMaxStack(itemid) <= 0) invalidCount++;
        else if (Item_GetName(itemid, detail, sizeof(detail)) == 0) invalidCount++;
    }

    format(detail, sizeof(detail), "%d item diperiksa, %d cacat", ITEM_COUNT - 1, invalidCount);
    Selftest_Record(invalidCount == 0, "registry item", detail);
    return 1;
}

stock Selftest_CheckRecipes()
{
    new invalidCount = 0;
    new detail[192];

    for (new index = 0; index < MAX_RECIPES; index++)
    {
        if (gRecipes[index][rcResult] > 0 && !Item_IsValid(gRecipes[index][rcResult])) invalidCount++;
        if (!Item_IsValid(gRecipes[index][rcIngredientA])) invalidCount++;
        if (gRecipes[index][rcIngredientAAmount] <= 0) invalidCount++;

        if (gRecipes[index][rcIngredientB] != ITEM_NONE_ID)
        {
            if (!Item_IsValid(gRecipes[index][rcIngredientB])) invalidCount++;
            if (gRecipes[index][rcIngredientBAmount] <= 0) invalidCount++;
        }

        if (gRecipes[index][rcBlueprint] != ITEM_NONE_ID && !Item_IsValid(gRecipes[index][rcBlueprint])) invalidCount++;
    }

    format(detail, sizeof(detail), "%d resep diperiksa, %d cacat", MAX_RECIPES, invalidCount);
    Selftest_Record(invalidCount == 0, "registry resep", detail);
    return 1;
}

stock Selftest_CheckStructures()
{
    new invalidCount = 0;
    new detail[192];

    for (new structure = 0; structure < STRUCTURE_COUNT; structure++)
    {
        if (gStructures[structure][stHealth] <= 0) invalidCount++;
        if (gStructures[structure][stLimit] <= 0) invalidCount++;
        if (gStructures[structure][stTier] < 1 || gStructures[structure][stTier] > BASE_MAX_TIER) invalidCount++;
        if (!Item_IsValid(gStructures[structure][stMatA])) invalidCount++;
        if (!Item_IsValid(gStructures[structure][stMatB])) invalidCount++;
        if (gStructures[structure][stAmtA] <= 0 || gStructures[structure][stAmtB] <= 0) invalidCount++;

        if (gStructures[structure][stSolid] && gStructures[structure][stPickupModel] != 0) invalidCount++;
        if (!gStructures[structure][stSolid] && gStructures[structure][stPickupModel] == 0) invalidCount++;
    }

    format(detail, sizeof(detail), "%d struktur, %d cacat, wall tier2 limit=%d",
        STRUCTURE_COUNT, invalidCount, BaseBuild_Limit(STRUCT_WALL, 2));
    Selftest_Record(invalidCount == 0, "registry struktur", detail);
    return 1;
}

stock Selftest_CheckZones()
{
    new populated = 0, ambientTotal = 0;
    new emptySlot = -1;

    for (new zoneid = 0; zoneid < MAX_ZONE_COUNT; zoneid++)
    {
        if (gZones[zoneid][zoneName][0] != EOS)
        {
            populated++;
            ambientTotal += gZones[zoneid][zoneAmbientTarget];
        }
        else if (emptySlot < 0) emptySlot = zoneid;
    }

    new detail[160];
    format(detail, sizeof(detail), "%d zona berisi, slot kosong=%d, target ambient total=%d",
        populated, emptySlot, ambientTotal);
    Selftest_Record(populated == MAX_ZONE_COUNT - 1, "registry zona", detail);
    return 1;
}

/* Lobby harus punya world unik: kalau dua lobby berbagi world, pemain dari lobby
   berbeda akan saling melihat dan zombie bisa menyeberang. */
stock Selftest_CheckLobbies()
{
    new duplicateWorld = 0;
    new emptyName = 0;

    for (new lobbyId = 0; lobbyId < LobbyCount; lobbyId++)
    {
        new lobbyLabel[32];
        Lobby_GetName(lobbyId, lobbyLabel, sizeof(lobbyLabel));
        if (lobbyLabel[0] == EOS) emptyName++;

        for (new otherId = lobbyId + 1; otherId < LobbyCount; otherId++)
        {
            if (Lobby_GetWorld(lobbyId) == Lobby_GetWorld(otherId)) duplicateWorld++;
        }
    }

    new detail[160];
    format(detail, sizeof(detail), "%d lobby aktif, world bentrok=%d, nama kosong=%d",
        LobbyCount, duplicateWorld, emptyName);
    Selftest_Record(LobbyCount > 0 && LobbyCount <= MAX_LOBBIES && duplicateWorld == 0 && emptyName == 0,
        "registry lobby", detail);
    return 1;
}

/* Tabel loot harus punya bobot dan item valid: tabel kosong berarti zombie mati
   tanpa drop sama sekali, dan item id salah berarti drop tidak bisa diambil. */
stock Selftest_CheckLootTables()
{
    new invalidTables = 0, invalidItems = 0;

    for (new type = 0; type < MAX_ZOMBIE_TYPES; type++)
    {
        new totalWeight = 0;
        for (new slot = 0; slot < LOOT_TABLE_SLOTS; slot++)
        {
            new weight = gZombieLoot[type][slot][leWeight];
            if (weight <= 0) continue;

            totalWeight += weight;
            if (!Item_IsValid(gZombieLoot[type][slot][leItem])) invalidItems++;
        }
        if (totalWeight <= 0) invalidTables++;
    }

    for (new tierRow = 0; tierRow < 2; tierRow++)
    {
        for (new bonusSlot = 0; bonusSlot < 2; bonusSlot++)
        {
            if (gTierBonusLoot[tierRow][bonusSlot][leWeight] <= 0) invalidTables++;
            if (!Item_IsValid(gTierBonusLoot[tierRow][bonusSlot][leItem])) invalidItems++;
        }
    }

    for (new slot = 0; slot < LOOT_NODE_SLOTS; slot++)
    {
        if (gNodeLoot[slot][leWeight] <= 0) continue;
        if (!Item_IsValid(gNodeLoot[slot][leItem])) invalidItems++;
    }

    new detail[160];
    format(detail, sizeof(detail), "%d tabel zombie, item tidak valid=%d", MAX_ZOMBIE_TYPES, invalidItems);
    Selftest_Record(invalidTables == 0 && invalidItems == 0, "tabel loot", detail);
    return 1;
}

stock Selftest_CheckVehicles()
{
    new detail[160];
    format(detail, sizeof(detail), "maks %d kendaraan, %d liar/lobby, garasi %d, sampel min %d",
        MAX_TRACKED_VEHICLES, WILD_MAX_PER_LOBBY, MAX_OWNED_VEHICLES, EXTRACTION_MIN_SAMPLES);

    new bool:valid = false;
    new tank = Vehicle_TankCapacity(560);
    new Float:consumption = Vehicle_ConsumptionPer100Km(560);
    if (MAX_TRACKED_VEHICLES > 0 && WILD_MAX_PER_LOBBY > 0 && VEHICLE_FUEL_GAME_SCALE > 0.0
        && tank >= 30 && tank <= 200 && consumption > 0.0
        && MAX_OWNED_VEHICLES > 0 && GARAGE_SHOP_COUNT > 0
        && Vehicle_ModelAllowed(TEST_VEHICLE_DEFAULT)
        && Item_IsValid(ITEM_LOCKPICK) && Item_IsValid(ITEM_JERRYCAN)
        && Item_IsValid(ITEM_SPECIMEN) && Item_IsValid(ITEM_REPAIR_KIT)
        && EXTRACTION_MIN_SAMPLES > 0 && EXTRACTION_COOLDOWN_MS > 0
        && EXTRACTION_PAY_PER_SAMPLE > 0)
    {
        valid = true;
    }

    Selftest_Record(valid, "kendaraan liar", detail);
    return 1;
}

stock Selftest_CheckTutorial()
{
    new label[64];
    new broken = 0;

    for (new step = TUT_STEP_KILL; step <= TUT_STEP_EXTRACT; step++)
    {
        if (Tutorial_TargetForStep(step) <= 0) broken++;
        Tutorial_Label(step, label, sizeof(label));
        if (label[0] == EOS) broken++;
    }

    Selftest_Record(broken == 0, "tutorial onboarding", "target/label langkah");
    return 1;
}

stock Selftest_CheckMapping()
{
    /* Cek runtime: dekorasi kamp dan actor benar-benar dibuat saat boot. */
    new bool:ok = (MapObjectCount >= 5 && MapActorCount >= 1);

    Selftest_Record(ok, "mapping kamp/shelter", "objek dekorasi dan actor pedagang");
    return 1;
}

stock Selftest_CheckParty()
{
    new leaks = 0;
    for (new playerid = 0; playerid < MAX_PLAYERS; playerid++)
    {
        if (PartyMember[playerid] && PartyLeader[playerid] == INVALID_PLAYER_ID) leaks++;
        if (PartyLeader[playerid] != INVALID_PLAYER_ID && !PartyMember[playerid]) leaks++;
    }

    new detail[96];
    format(detail, sizeof(detail), "%d slot tidak konsisten dari %d", leaks, MAX_PLAYERS);
    Selftest_Record(leaks == 0, "konsistensi party", detail);
    return 1;
}

stock Selftest_CheckCompanion()
{
    /* Cek NYATA terhadap konfigurasi server. Rekrutan bersifat elastis:
       Companion_Tick men-despawn rekrutan saat budget zombie penuh, jadi yang
       wajib muat adalah zombie + 1 trader escort. maxnpc juga tidak boleh
       melebihi maxplayers (FCNPC mencetak "Crash possible" bila lebih). */
    new maxNpc = GetServerVarAsInt("maxnpc");
    new maxPlayers = GetServerVarAsInt("maxplayers");
    new needed = MAX_ZOMBIES + 1;

    new detail[160];
    format(detail, sizeof(detail), "maxnpc=%d butuh>=%d (zombie %d + trader 1; rekrutan elastis), maxplayers=%d",
        maxNpc, needed, MAX_ZOMBIES, maxPlayers);
    Selftest_Record(maxNpc >= needed && maxNpc <= maxPlayers,
        "anggaran NPC rekrutan", detail);

    /* Slot pemain dan NPC satu pool: kalau maxplayers kecil, budget zombie harus
       ikut menyusut (Npc_SpawnRoom) atau FCNPC gagal dan RakNet abort. */
    format(detail, sizeof(detail), "slot terpakai=%d/%d, bebas=%d, jatah spawn=%d, cadangan=%d",
        Npc_OccupiedSlots(), GetMaxPlayers(), Npc_SlotsFree(), Npc_SpawnRoom(), NPC_SLOT_RESERVE);
    Selftest_Record(GetMaxPlayers() > NPC_SLOT_RESERVE
        && Npc_SlotsFree() <= GetMaxPlayers()
        && Npc_SpawnRoom() < Npc_SlotsFree(),
        "anggaran slot NPC", detail);
    return 1;
}

stock Selftest_CheckBoss()
{
    new detail[144];
    format(detail, sizeof(detail), "tipe=%d, HP boss=%.0f > brute=%.0f, minTier=%d",
        MAX_ZOMBIE_TYPES, gZombieProfiles[_:ZT_BOSS][ztHealth], gZombieProfiles[_:ZT_BRUTE][ztHealth],
        gZombieProfiles[_:ZT_BOSS][ztMinTier]);

    new bool:ok = (MAX_ZOMBIE_TYPES == 8)
        && (gZombieProfiles[_:ZT_BOSS][ztHealth] > gZombieProfiles[_:ZT_BRUTE][ztHealth])
        && (gZombieProfiles[_:ZT_BOSS][ztMinTier] >= 90)
        && (gZombieProfiles[_:ZT_BOSS][ztLootChance] == 100);

    Selftest_Record(ok, "boss malam", detail);
    return 1;
}

stock Selftest_RunStatic()
{
    /* Status lapisan transaksi harus terlihat di selftest (audit T1/#13):
       fallback pool diam-diam = atomisitas hilang tanpa jejak. */
    Selftest_Record(Tx_IsReady(), "transaksi item",
        Tx_IsReady() ? "handle khusus aktif" : "NONAKTIF (fallback pool, cek mysql.ini)");
    Selftest_CheckItems();
    Selftest_CheckLootTables();
    Selftest_CheckLobbies();
    Selftest_CheckRecipes();
    Selftest_CheckBags();
    Selftest_CheckHotbar();
    Selftest_CheckLocks();
    Selftest_CheckStructures();
    Selftest_CheckZones();
    Selftest_CheckVehicles();
    Selftest_CheckTutorial();
    Selftest_CheckMapping();
    Selftest_CheckParty();
    Selftest_CheckCompanion();
    Selftest_CheckBoss();

    Selftest_GroundCheck("spawn kamp", SPAWN_X, SPAWN_Y, SPAWN_Z);
    Selftest_GroundCheck("hotspot stash", 1662.0, -2318.0, 13.5);
    Selftest_GroundCheck("hotspot craft", 1666.0, -2320.0, 13.5);
    Selftest_GroundCheck("hotspot shop", 1670.0, -2324.0, 13.5);
    Selftest_GroundCheck("hotspot ekstraksi", 1697.0, -2349.0, 13.5);
    Selftest_GroundCheck("garasi", GARAGE_X, GARAGE_Y, GARAGE_Z);
    Selftest_GroundCheck("parkir garasi", gGarageParkX[0], gGarageParkY[0], GARAGE_Z);
    Selftest_GroundCheck("pom bensin", FUEL_PUMP_X, FUEL_PUMP_Y, FUEL_PUMP_Z);

    for (new zoneid = 0; zoneid < MAX_ZONE_COUNT; zoneid++)
    {
        if (gZones[zoneid][zoneName][0] == EOS) continue;

        Selftest_GroundCheck(gZones[zoneid][zoneName],
            gZones[zoneid][zoneX], gZones[zoneid][zoneY], gZones[zoneid][zoneZ]);
    }
    return 1;
}

stock Selftest_Run(playerid)
{
    if (!SQL_IsReady())
    {
        if (playerid != INVALID_PLAYER_ID) SendClientMessage(playerid, UI_ACCENT_WARN, "[SELFTEST] MySQL belum siap.");
        Log_Format(LOG_WARN, "[SELFTEST] dilewati: MySQL belum siap.");
        return 0;
    }

    Selftest_Reset();
    SelftestRunning = true;
    Selftest_RunStatic();

    new query[3072];
    query[0] = EOS;
    strcat(query, "SELECT \
        (SELECT COUNT(*) FROM accounts) AS accounts, \
        (SELECT COUNT(*) FROM characters) AS characters, \
        (SELECT COUNT(*) FROM bases) AS bases, \
        (SELECT COUNT(*) FROM base_structures) AS base_structures, \
        (SELECT COUNT(*) FROM inventory) AS inventory, \
        (SELECT COUNT(*) FROM stash) AS stash, \
        (SELECT COUNT(*) FROM item_ledger) AS item_ledger, \
        (SELECT COUNT(*) FROM kill_log) AS kill_log, \
        (SELECT COUNT(*) FROM anticheat_flags) AS anticheat_flags, \
        (SELECT COUNT(*) FROM barricades) AS barricades,         (SELECT COUNT(*) FROM graves) AS graves,         (SELECT COUNT(*) FROM contracts) AS contracts,         (SELECT COUNT(*) FROM owned_vehicles) AS owned_vehicles, \
        (SELECT COUNT(*) FROM companions) AS companions, \
        (SELECT COUNT(*) FROM bans) AS bans, \
        (SELECT COUNT(*) FROM party_stash) AS party_stash, \
        (SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'characters' AND COLUMN_NAME = 'wounds') AS wound_column, \
        (SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'base_structures' AND COLUMN_NAME = 'fuel') AS fuel_column, \
        (SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'accounts' AND COLUMN_NAME = 'admin_level') AS admin_column, \
        (SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'companions' AND COLUMN_NAME = 'tier') AS tier_column, \
        (SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'characters' AND COLUMN_NAME = 'occupation') AS profile_column, \
        (SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'characters' AND COLUMN_NAME = 'hotbar') AS hotbar_column", sizeof(query));

    mysql_tquery(gSQL, query, "Selftest_OnTablesLoaded", "i", playerid);
    return 1;
}

public Selftest_OnTablesLoaded(playerid)
{
    if (mysql_errno(gSQL) != 0)
    {
        new detail[128];
        format(detail, sizeof(detail), "errno=%d", mysql_errno(gSQL));
        Selftest_Record(false, "tabel database", detail);
        Selftest_Finish(playerid);
        return 0;
    }

    new const tableNames[][] = {
        "accounts", "characters", "bases", "base_structures", "inventory",
        "stash", "item_ledger", "kill_log", "anticheat_flags", "barricades",
        "graves", "contracts", "owned_vehicles", "companions", "bans", "party_stash"
    };

    new details[384];
    details[0] = EOS;

    for (new index = 0; index < sizeof(tableNames); index++)
    {
        new rows = 0;
        cache_get_value_name_int(0, tableNames[index], rows);

        new part[32];
        format(part, sizeof(part), "%s%s=%d", (index == 0) ? "" : " ", tableNames[index], rows);
        strcat(details, part, sizeof(details));
    }

    Selftest_Record(cache_num_rows() == 1, "tabel database", details);

    new woundColumn = 0, fuelColumn = 0, adminColumn = 0, tierColumn = 0, profileColumn = 0, hotbarColumn = 0;
    cache_get_value_name_int(0, "wound_column", woundColumn);
    cache_get_value_name_int(0, "fuel_column", fuelColumn);
    cache_get_value_name_int(0, "admin_column", adminColumn);
    cache_get_value_name_int(0, "tier_column", tierColumn);
    cache_get_value_name_int(0, "profile_column", profileColumn);
    cache_get_value_name_int(0, "hotbar_column", hotbarColumn);
    format(details, sizeof(details), "wounds=%d fuel=%d admin_level=%d companion_tier=%d profil=%d hotbar=%d",
        woundColumn, fuelColumn, adminColumn, tierColumn, profileColumn, hotbarColumn);
    Selftest_Record(woundColumn == 1 && fuelColumn == 1 && adminColumn == 1 && tierColumn == 1
        && profileColumn == 1 && hotbarColumn == 1, "kolom survival/role", details);
    Selftest_Finish(playerid);
    return 1;
}

stock Selftest_Finish(playerid)
{
    SelftestRunning = false;

    new summary[192];
    format(summary, sizeof(summary), "[SELFTEST] %d/%d PASS, %d FAIL",
        SelftestChecks - SelftestFailures, SelftestChecks, SelftestFailures);
    Log_Format(SelftestFailures == 0 ? LOG_INFO : LOG_ERROR, "%s", summary);

    if (playerid == INVALID_PLAYER_ID || !IsPlayerConnected(playerid)) return 1;

    if (SelftestFailures == 0)
    {
        SendClientMessage(playerid, UI_ACCENT_PRIMARY, summary);
        SendClientMessage(playerid, UI_TEXT_MUTED, "[SELFTEST] Detail lengkap ada di scriptfiles/logs/laststand.log.");
        return 1;
    }

    SendClientMessage(playerid, UI_ACCENT_DANGER, summary);
    ShowPlayerDialog(playerid, DIALOG_SELFTEST, DIALOG_STYLE_MSGBOX, "Self-test gagal",
        "Ada pemeriksaan yang FAIL. Baris detailnya ada di log:\n\
        scriptfiles/logs/laststand.log\n\n\
        Jalankan /selftest lagi setelah memperbaiki konfigurasi.",
        "Tutup", "");
    return 1;
}

/* Probe bcrypt: membuktikan callback plugin benar-benar terpanggil dan
   hash hasil plugin bisa diverifikasi ulang. Tanpa probe ini, kegagalan
   auth hanya terlihat sebagai "password salah" di klien. */
public BcryptProbe_OnHashed(playerid)
{
    #pragma unused playerid

    new probeHash[BCRYPT_HASH_LENGTH];
    bcrypt_get_hash(probeHash);

    if (probeHash[0] == EOS)
    {
        Log_Format(LOG_ERROR, "[PROBE] bcrypt_hash tidak menghasilkan hash.");
        return 0;
    }

    Log_Format(LOG_INFO, "[PROBE] hash bcrypt dibuat (prefix %.4s, panjang %d).", probeHash, strlen(probeHash));
    bcrypt_verify(0, "BcryptProbe_OnVerified", "probe123", probeHash);
    return 1;
}

public BcryptProbe_OnVerified(playerid, bool:success)
{
    #pragma unused playerid

    if (success) Log_Format(LOG_INFO, "[PROBE] bcrypt hash+verify OK.");
    else Log_Format(LOG_ERROR, "[PROBE] bcrypt verify GAGAL untuk hash buatan plugin sendiri.");
    return 1;
}

stock Selftest_BcryptProbe()
{
    bcrypt_hash(0, "BcryptProbe_OnHashed", "probe123", BCRYPT_COST);
    return 1;
}

public Selftest_Boot()
{
    if (SelftestRunning) return 0;
    Selftest_Run(INVALID_PLAYER_ID);
    return 1;
}
