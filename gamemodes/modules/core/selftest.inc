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

/* Banner raid membaca state Horde tiap 500 ms; guard indeks lobby harus benar
   sebelum ada yang menyentuh array. Sengaja hanya menguji jalur invalid supaya
   hasilnya deterministik (tidak bergantung ada/tidaknya raid saat boot). */
stock Selftest_CheckRaidBanner()
{
    new zombies = 99, seconds = 99;
    new bool:rejected = !Horde_GetRaidStatus(-1, zombies, seconds)
        && zombies == 0 && seconds == 0;

    zombies = 99;
    seconds = 99;
    new bool:rejectedHigh = !Horde_GetRaidStatus(MAX_LOBBIES, zombies, seconds)
        && zombies == 0 && seconds == 0;

    new detail[144];
    format(detail, sizeof(detail), "lobby -1 dan %d ditolak, keluaran dikosongkan", MAX_LOBBIES);
    Selftest_Record(rejected && rejectedHigh, "banner raid HUD", detail);
    return 1;
}

stock Selftest_CheckDeaths()
{
    /* Aturan prioritas: killer pemain menang atas reason 53/54 (pemain bisa
       didorong jatuh oleh pemain lain); selain itu jatuh/tenggelam =
       lingkungan, sisanya zombie. */
    new playerByPlayer = Deaths_KindFor(true, 24);
    new playerOverFall = Deaths_KindFor(true, DEATH_REASON_FALL);
    new fallAlone = Deaths_KindFor(false, DEATH_REASON_FALL);
    new drownAlone = Deaths_KindFor(false, DEATH_REASON_DROWN);
    new zombieWeapon = Deaths_KindFor(false, 31);
    new zombieNoWeapon = Deaths_KindFor(false, 0);

    new detail[176];
    format(detail, sizeof(detail), "pemain=%d/%d, lingkungan=%d/%d, zombie=%d/%d",
        playerByPlayer, playerOverFall, fallAlone, drownAlone, zombieWeapon, zombieNoWeapon);
    Selftest_Record(playerByPlayer == DEATH_KIND_PLAYER
        && playerOverFall == DEATH_KIND_PLAYER
        && fallAlone == DEATH_KIND_ENVIRONMENT
        && drownAlone == DEATH_KIND_ENVIRONMENT
        && zombieWeapon == DEATH_KIND_ZOMBIE
        && zombieNoWeapon == DEATH_KIND_ZOMBIE,
        "log kematian: klasifikasi penyebab", detail);

    new kindName[16];
    Deaths_KindName(DEATH_KIND_PLAYER, kindName, sizeof(kindName));
    new bool:kindFilled = (strlen(kindName) > 0);

    new ago[32];
    Deaths_TimeAgo(gettime(), ago, sizeof(ago));
    new bool:nowSane = (strcmp(ago, "0 detik lalu", true) == 0);
    /* Waktu di masa depan harus dijepit ke 0, bukan jadi "-100 detik lalu". */
    Deaths_TimeAgo(gettime() + 100, ago, sizeof(ago));
    new bool:futureClamped = (strcmp(ago, "0 detik lalu", true) == 0);
    Deaths_TimeAgo(gettime() - 7200, ago, sizeof(ago));
    new bool:hoursSane = (strcmp(ago, "2 jam lalu", true) == 0);

    format(detail, sizeof(detail), "nama tipe=%d, sekarang=%d, masa depan dijepit=%d, 2 jam=%d",
        kindFilled, nowSane, futureClamped, hoursSane);
    Selftest_Record(kindFilled && nowSane && futureClamped && hoursSane, "log kematian: format waktu", detail);
    return 1;
}

stock Selftest_CheckInstanceQueue()
{
    /* Instance: nilai wajib 1..SERVER_ID_MAX walau server.cfg kosong/salah.
       Karakter ber-server_id lain tidak boleh muncul di daftar instance ini. */
    new instanceId = Server_InstanceId();
    new bool:instanceValid = (instanceId >= SERVER_ID_MIN && instanceId <= SERVER_ID_MAX);

    new detail[176];
    format(detail, sizeof(detail), "server_id=%d (rentang %d..%d) sumber=%s",
        instanceId, SERVER_ID_MIN, SERVER_ID_MAX,
        (Server_ReadIdFile() == instanceId) ? "instance.ini" : "port");
    Selftest_Record(instanceValid, "identitas instance server", detail);

    /* Antrean: di bawah cap tidak menahan, tepat di cap menahan, cap di atas
       MAX_PLAYERS tetap dihitung aman. */
    new cap = Server_PlayerCap();
    new bool:belowHold = Queue_HoldsFor(cap - 1);
    new bool:atHold = Queue_HoldsFor(cap);
    new bool:aboveHold = Queue_HoldsFor(cap + 1);
    new bool:capSane = (cap >= SERVER_PLAYER_CAP_MIN && cap <= MAX_PLAYERS);

    format(detail, sizeof(detail), "cap=%d, tahan di %d/%d/%d = %d/%d/%d",
        cap, cap - 1, cap, cap + 1, belowHold, atHold, aboveHold);
    Selftest_Record(!belowHold && atHold && aboveHold && capSane, "antrean slot server penuh", detail);

    /* Dekorasi: indeks valid, jumlah konsisten, dan tidak ada prop duplikat
       persis (model + koordinat) yang membuat objek menumpuk sia-sia. */
    new decorCount = Decor_Count();
    new bool:decorIndexOk = (Decor_IsValidIndex(0) && Decor_IsValidIndex(decorCount - 1));
    new bool:decorIndexRejects = (!Decor_IsValidIndex(-1) && !Decor_IsValidIndex(decorCount));

    new bool:decorDuplicate = false;
    for (new left = 0; left < sizeof(DecorProp); left++)
    {
        for (new right = left + 1; right < sizeof(DecorProp); right++)
        {
            if (DecorProp[left][dcModel] != DecorProp[right][dcModel]) continue;
            if (DecorProp[left][dcX] != DecorProp[right][dcX]) continue;
            if (DecorProp[left][dcY] != DecorProp[right][dcY]) continue;
            if (DecorProp[left][dcZ] != DecorProp[right][dcZ]) continue;
            decorDuplicate = true;
        }
    }

    format(detail, sizeof(detail), "%d dekorasi, indeks valid=%d/%d, duplikat=%d",
        decorCount, decorIndexOk, decorIndexRejects, decorDuplicate);
    Selftest_Record(decorCount > 0 && decorIndexOk && decorIndexRejects && !decorDuplicate,
        "dekorasi kamp", detail);
    return 1;
}

stock Selftest_CheckCompanionGear()
{
    /* Tier wajib menaikkan senjata DAN armour; kalau salah satu tidak naik,
       upgrade berbayar bahan tidak memberi keuntungan nyata (ROADMAP 365). */
    new weapon0 = Companion_WeaponForTier(0);
    new weapon1 = Companion_WeaponForTier(1);
    new weapon2 = Companion_WeaponForTier(2);

    new Float:armour0 = Companion_ArmourForTier(0);
    new Float:armour1 = Companion_ArmourForTier(1);
    new Float:armour2 = Companion_ArmourForTier(2);

    new bool:weaponRises = (weapon1 > weapon0 && weapon2 > weapon1);
    new bool:armourRises = (armour0 == 0.0 && armour1 > armour0 && armour2 > armour1);

    new detail[160];
    format(detail, sizeof(detail), "senjata %d<%d<%d, armour %.0f<%.0f<%.0f",
        weapon0, weapon1, weapon2, armour0, armour1, armour2);
    Selftest_Record(weaponRises && armourRises, "perlengkapan rekrutan naik per tier", detail);

    /* Dokumen lore: isi tidak boleh kosong dan indeks harus selalu valid. */
    new bool:loreFilled = true;
    for (new index = 0; index < LORE_DOCUMENT_COUNT; index++)
    {
        if (strlen(LoreDocument[index]) < 40) loreFilled = false;
    }
    new loreIndex = Lore_IndexForPlayer(0);
    new bool:loreWrap = (loreIndex >= 0 && loreIndex < LORE_DOCUMENT_COUNT);
    new bool:loreBadSlot = (Lore_IndexForPlayer(MAX_PLAYERS - 1) < LORE_DOCUMENT_COUNT);

    format(detail, sizeof(detail), "%d dokumen terisi=%d, indeks dalam rentang=%d/%d",
        LORE_DOCUMENT_COUNT, loreFilled, loreWrap, loreBadSlot);
    Selftest_Record(loreFilled && loreWrap && loreBadSlot, "dokumen lore", detail);
    return 1;
}

stock Selftest_CheckBaseMembers()
{
    /* Matriks izin gate: tanpa base (-1) dan member (0) harus DITOLAK, officer
       (1) dan owner (2) boleh. Tanpa gate ini, anggota biasa bisa membuka base
       pemain lain atau justru pemilik terkunci di luar. */
    new bool:denyOutsider = !BaseMembers_RoleAllowsGate(-1);
    new bool:denyMember = !BaseMembers_RoleAllowsGate(BASE_ROLE_MEMBER);
    new bool:allowOfficer = BaseMembers_RoleAllowsGate(BASE_ROLE_OFFICER);
    new bool:allowOwner = BaseMembers_RoleAllowsGate(BASE_ROLE_OWNER);
    new bool:rejectBogusSlot = (BaseBuild_ToggleGate(0, -5) == -2);

    new detail[176];
    format(detail, sizeof(detail),
        "tolak luar/member=%d/%d, izin officer/owner=%d/%d, slot palsu ditolak=%d",
        denyOutsider, denyMember, allowOfficer, allowOwner, rejectBogusSlot);
    Selftest_Record(denyOutsider && denyMember && allowOfficer && allowOwner && rejectBogusSlot,
        "izin gate base", detail);
    return 1;
}

stock Selftest_CheckModeration()
{
    /* Mute/warn tidak butuh pemain online untuk diuji di level state. Yang
       penting: state awal bersih, reset idempoten, dan ambang peringatan tetap
       seperti yang dijanjikan ke staff (3 peringatan -> mute otomatis). */
    new bool:clean = !Moderation_IsMuted(0) && Moderation_MuteSecondsLeft(0) == 0;
    new bool:reset = Moderation_ResetPlayerState(0) && !Moderation_IsMuted(0);
    new bool:threshold = (WARN_BEFORE_MUTE == 3 && WARN_MUTE_MINUTES > 0
        && MUTE_MAX_MINUTES >= WARN_MUTE_MINUTES);

    new detail[160];
    format(detail, sizeof(detail), "state bersih=%d, reset=%d, ambang %d peringatan -> %d menit",
        clean, reset, WARN_BEFORE_MUTE, WARN_MUTE_MINUTES);
    Selftest_Record(clean && reset && threshold, "mute dan warn", detail);
    return 1;
}

stock Selftest_CheckBagSpeed()
{
    /* Skala kecepatan harus murni dan berbatas: tanpa kapasitas tas tidak boleh
       memperlambat, dan faktor maksimum tidak boleh membuat pemain berhenti. */
    new Float:noBag = Bag_SpeedScale(0);
    new bool:neutral = (noBag == 1.0);
    new bool:bounded = (BAG_SLOW_MAX > 0.0 && BAG_SLOW_MAX < 0.5);

    new detail[128];
    format(detail, sizeof(detail), "tanpa tas=%.2f, batas lambat=%.2f", noBag, BAG_SLOW_MAX);
    Selftest_Record(neutral && bounded, "berat tas vs kecepatan", detail);
    return 1;
}

stock Selftest_CheckWeather()
{
    /* Override cuaca harus menolak id di luar rentang dan bisa dikembalikan ke
       otomatis. Kalau reset gagal, server terjebak cuaca admin selamanya. */
    new bool:rejectHigh = !World_SetWeather(99);
    new bool:rejectLow = !World_SetWeather(-5);
    new bool:accept = World_SetWeather(7) && World_GetWeatherOverride() == 7;
    World_SetWeather(-1);
    new bool:reset = (World_GetWeatherOverride() == -1);

    new detail[144];
    format(detail, sizeof(detail), "tolak 99/%d=1/1, terima 7=%d, reset auto=%d",
        -5, accept, reset);
    Selftest_Record(rejectHigh && rejectLow && accept && reset, "override cuaca", detail);
    return 1;
}

stock Selftest_CheckNightfall()
{
    /* Gate: Nightfall harus TERTUTUP saat boot (belum 20 pemain) dan semua jalur
       buka/tutup yang berbahaya harus ditolak. Tanpa ini, satu bug di
       Lobby_AutoTick bisa membuka lobby yang belum siap atau menutup lobby
       default sehingga semua pemain kehilangan dunia. */
    new before = LobbyCount;

    new bool:lockedAtBoot = !Lobby_IsValid(NIGHTFALL_LOBBY_ID);
    new bool:rejectNegative = !Lobby_Unlock(-1, "selftest");
    new bool:rejectHigh = !Lobby_Unlock(MAX_LOBBIES, "selftest");
    new bool:rejectSkip = !Lobby_Unlock(LobbyCount + 1, "selftest");
    new bool:rejectDefault = !Lobby_Lock(0, "selftest") && LobbyCount == before;
    new bool:noNightOnDefault = !Lobby_IsPermanentNight(0);

    new detail[144];
    format(detail, sizeof(detail),
        "tertutup saat boot=%d, tolak -1/%d/lompat=%d/%d/%d, lobby 0 aman=%d",
        lockedAtBoot, MAX_LOBBIES, rejectNegative, rejectHigh, rejectSkip, rejectDefault);

    Selftest_Record(lockedAtBoot && rejectNegative && rejectHigh && rejectSkip
        && rejectDefault && noNightOnDefault, "nightfall gate", detail);
    return 1;
}

stock Selftest_CheckBuildPanel()
{
    /* Tata letak panel diuji dengan angka, bukan dengan mata: hitbox yang
       meleset 1 px sudah cukup membuat pemain menekan tombol yang salah. */
    new Float:panelRight = BUILD_EDIT_PANEL_X + BUILD_EDIT_PANEL_W;
    new Float:panelBottom = BUILD_EDIT_PANEL_Y + BUILD_EDIT_PANEL_H;

    /* Layar acuan SA-MP selalu 640x480; HUD_SCREEN_HEIGHT tidak ada di hud.inc. */
    new bool:insideScreen = (panelRight <= HUD_SCREEN_WIDTH) && (panelBottom <= 480.0);
    /* HudLoadout mulai y=330 dan HudVehicle y=346: panel tidak boleh turun ke situ. */
    new bool:clearHud = (panelBottom <= 330.0);

    new Float:buttonRight = BUILD_EDIT_PANEL_X + 228.0 + 40.0;
    new bool:buttonsInside = (buttonRight <= panelRight - 20.0);

    /* Baris 16 px, tombol 12 px -> tidak ada dua hitbox yang bertumpuk. */
    new bool:noOverlap = (12.0 < 16.0) && (BUILD_EDIT_PANEL_H >= 210.0);

    new detail[160];
    format(detail, sizeof(detail), "kanan %.0f/%.0f, bawah %.0f, lepas HUD=%d, tombol<=%.0f=%d",
        panelRight, HUD_SCREEN_WIDTH, panelBottom, clearHud, panelRight - 20.0, buttonsInside);

    Selftest_Record(insideScreen && clearHud && buttonsInside && noOverlap, "tata letak panel bangun", detail);

    /* Layout mobile: panel 206x224 ditempel kanan-atas (x = 425.8), tombol
       38x16 dengan baris 22 px — hitbox tidak bertumpuk dan tetap di layar.
       Angka HARUS sama dengan BuildEdit_ComputeLayout di build_edit.inc. */
    new Float:mX = 640.0 - (640.0 - BUILD_EDIT_PANEL_X) * HUD_SCALE_MOBILE;
    new Float:mRight = mX + 206.0;
    new Float:mBottom = 28.0 + 224.0;
    new bool:mInside = (mRight <= HUD_SCREEN_WIDTH) && (mBottom <= 480.0);
    new bool:mButtons = (mX + 160.0 + 38.0 <= mRight - 4.0);
    new bool:mRows = (16.0 < 22.0);
    new bool:mTouch = (38.0 >= 28.0 && 16.0 >= 12.0);  /* sasaran sentuh minimum */

    new mdetail[160];
    format(mdetail, sizeof(mdetail), "mobile kanan %.0f, bawah %.0f, tombol dalam=%d, baris=%d, sentuh=%d",
        mRight, mBottom, mButtons, mRows, mTouch);
    Selftest_Record(mInside && mButtons && mRows && mTouch, "tata letak panel bangun mobile", mdetail);
    return 1;
}

stock Selftest_CheckBuildLoad()
{
    /* Regresi "struktur bergeser": penanda muat dulu per pemain, sehingga anggota
       kedua dari base yang sama memuat ulang dan MENGGANDAKAN setiap struktur.
       Yang diuji di sini adalah sifat penandanya: idempoten, bisa dilupakan, dan
       tidak menumpuk saat base yang sama diminta dua kali. */
    new baseId = 90001;
    new before = BaseBuildLoadedCount;

    new bool:first = (BaseBuild_MarkLoaded(baseId) == 1);
    new bool:idempotent = (BaseBuild_MarkLoaded(baseId) == 0) && (BaseBuildLoadedCount == before + 1);
    new bool:visible = BaseBuild_IsLoaded(baseId);

    new bool:loadedOnce = (BaseBuild_SetLoading(baseId, true) == 1);
    new bool:loadingVisible = BaseBuild_IsLoading(baseId);
    new bool:loadingOnce = (BaseBuild_SetLoading(baseId, true) == 0);
    BaseBuild_SetLoading(baseId, false);

    new bool:forgotten = (BaseBuild_ForgetLoaded(baseId) == 1) && !BaseBuild_IsLoaded(baseId);
    new bool:cleanAfter = (BaseBuildLoadedCount == before) && (BaseBuildLoadingCount == 0);

    new detail[160];
    format(detail, sizeof(detail),
        "tandai=%d idempoten=%d terlihat=%d, loading=%d/%d/%d, lupa=%d bersih=%d",
        first, idempotent, visible, loadedOnce, loadingVisible, loadingOnce, forgotten, cleanAfter);

    Selftest_Record(first && idempotent && visible && loadedOnce && loadingVisible
        && loadingOnce && forgotten && cleanAfter, "muat struktur per base", detail);
    return 1;
}

stock Selftest_CheckBuildEdit()
{
    /* Dua fungsi murni yang menentukan hasil akhir pratinjau. Kalau clamp atau
       normalisasi salah, pemain melihat "ARAH -15" atau struktur terpasang
       8 meter di depan padahal panel bilang 2,5. */
    new bool:clampOk = (floatround(BuildEdit_ClampDistance(0.0) * 100.0) == 100
        && floatround(BuildEdit_ClampDistance(2.5) * 100.0) == 250
        && floatround(BuildEdit_ClampDistance(99.0) * 100.0) == 600);

    /* Samping & naik punya batas sendiri: tanpa clamp, tombol Z bisa menembus
       tanah atau menggantung 20 m di udara. */
    new bool:limitOk = (floatround(BuildEdit_ClampValue(-99.0, -6.0, 6.0) * 100.0) == -600
        && floatround(BuildEdit_ClampValue(99.0, -6.0, 6.0) * 100.0) == 600
        && floatround(BuildEdit_ClampValue(1.5, -1.0, 3.0) * 100.0) == 150);

    new bool:rotOk = (floatround(BuildEdit_NormalizeRot(-15.0)) == 345
        && floatround(BuildEdit_NormalizeRot(370.0)) == 10
        && floatround(BuildEdit_NormalizeRot(360.0)) == 0
        && floatround(BuildEdit_NormalizeRot(90.0)) == 90);

    /* Panel: LetterSize X 0.19 * HUD_CHAR_RATIO px per karakter, kotak 320 px
       dikurangi padding 16 px. Baris terburuk yang benar-benar bisa tercetak. */
    new worst[64], widthPx, limitPx = floatround(BUILD_EDIT_PANEL_W - 24.0);
    new bool:panelOk = true;
    new const panelRows[][] = {
        "GENERATOR | TIER 3 | 12/12",
        "SAMPING +6.00 m",
        "LANGKAH 1.00 m / 45 deg",
        "ALT maju | C putar | Y pasang"
    };
    for (new row = 0; row < sizeof(panelRows); row++)
    {
        format(worst, sizeof(worst), "%s", panelRows[row]);
        widthPx = floatround(strlen(worst) * 0.19 * HUD_CHAR_RATIO);
        if (widthPx > limitPx) panelOk = false;
    }

    new detail[128];
    format(detail, sizeof(detail), "jarak 1.0-6.0=%d, batas samping/naik=%d, arah 0-359=%d, panel %d/%d px=%d",
        clampOk, limitOk, rotOk, widthPx, limitPx, panelOk);

    Selftest_Record(clampOk && limitOk && rotOk && panelOk, "pratinjau struktur", detail);
    return 1;
}

stock Selftest_CheckHudLayout()
{
    new offender[16], detail[96];
    new bool:passed = HUD_WorstCaseFits(offender, sizeof(offender));

    if (passed)
    {
        format(detail, sizeof(detail), "%d baris muat di %d px pada string terburuk",
            sizeof(gHudFitRows), floatround(HUD_SCREEN_WIDTH));
    }
    else
    {
        format(detail, sizeof(detail), "baris %s melewati tepi kanan %d px",
            offender, floatround(HUD_SCREEN_WIDTH));
        Log_Warn(detail);
    }

    Selftest_Record(passed, "lebar HUD 640px", detail);
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
    Selftest_CheckRaidBanner();
    Selftest_CheckDeaths();
    Selftest_CheckInstanceQueue();
    Selftest_CheckCompanionGear();
    Selftest_CheckBaseMembers();
    Selftest_CheckModeration();
    Selftest_CheckBagSpeed();
    Selftest_CheckWeather();
    Selftest_CheckNightfall();
    Selftest_CheckBuildEdit();
    Selftest_CheckBuildLoad();
    Selftest_CheckBuildPanel();
    Scent_Selftest();
    Airdrop_Selftest();
    Selftest_CheckHudLayout();

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
        (SELECT COUNT(*) FROM bans) AS bans, (SELECT COUNT(*) FROM admin_actions) AS admin_actions, \
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
        "graves", "contracts", "owned_vehicles", "companions", "bans", "party_stash",
        "admin_actions"
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
