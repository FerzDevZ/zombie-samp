#if defined _ls_director_included
    #endinput
#endif
#define _ls_director_included

#define DIRECTOR_TICK_MS 5000
#define DIRECTOR_NIGHT_MULTIPLIER 1.6
#define DIRECTOR_MIN_SPAWN_DISTANCE 35.0
#define DIRECTOR_MAX_SPAWN_DISTANCE 150.0
#define DIRECTOR_SAMPLE_PRESSURE_CAP 4

new ZoneZombieCount[MAX_LOBBIES][MAX_ZONE_COUNT];
new DirectorLastHordeTick = 0;
new DirectorLastReportTick = 0;

forward Director_Report();
forward Director_TickBody();

stock Zombie_RecountZones()
{
    for (new lobbyId = 0; lobbyId < MAX_LOBBIES; lobbyId++)
    {
        for (new zone = 0; zone < MAX_ZONE_COUNT; zone++)
        {
            ZoneZombieCount[lobbyId][zone] = 0;
        }
    }

    foreach (new zombieid : ZombiePool)
    {
        if (!Zombie_IsActive(zombieid)) continue;

        new Float:x, Float:y, Float:z;
        FCNPC_GetPosition(zombieid, x, y, z);

        new zone = Zone_GetIdAt(x, y, z);
        if (zone < 0 || zone >= MAX_ZONE_COUNT) continue;
        ZoneZombieCount[Zombie_GetLobby(zombieid)][zone]++;
    }
    return 1;
}

/* Pengumuman horde harus sampai ke lobby yang diserang saja. */
stock Director_NotifyLobby(lobbyId, const title[], const message[], color)
{
    foreach (new playerid : PlayerPool)
    {
        if (!IsPlayerConnected(playerid)) continue;
        if (Lobby_Of(playerid) != lobbyId) continue;
        Toast_Show(playerid, title, message, color);
    }
    return 1;
}

/* ColAndreas needs the generated .cadb map data. Without it the raycast misses and we
   fall back to the zone's configured height so the server still runs. */
stock bool:Director_FindGround(Float:x, Float:y, Float:fallbackZ, &Float:outZ)
{
    new Float:hitX, Float:hitY, Float:hitZ;
    new hit = CA_RayCastLine(x, y, fallbackZ + 120.0, x, y, fallbackZ - 80.0, hitX, hitY, hitZ);

    if (hit == 0)
    {
        outZ = fallbackZ + 1.0;
        return true;
    }

    outZ = hitZ + 1.0;
    return true;
}

stock Director_SpawnForZone(zone, E_ZOMBIE_TYPE:type, lobbyId = 0, bool:avoidPlayers = true)
{
    new Float:centerX = gZones[zone][zoneX];
    new Float:centerY = gZones[zone][zoneY];
    new Float:radius = gZones[zone][zoneRadius];

    for (new attempt = 0; attempt < 6; attempt++)
    {
        new Float:angle = float(random(360)) * 0.017453292;
        new Float:distance = radius * (0.35 + float(random(60)) / 100.0);

        new Float:spawnX = centerX + floatsin(angle) * distance;
        new Float:spawnY = centerY + floatcos(angle) * distance;

        if (avoidPlayers)
        {
            new Float:playerDistance;
            if (Hash_NearestPlayer(spawnX, spawnY, centerY, DIRECTOR_MIN_SPAWN_DISTANCE, playerDistance, lobbyId) != INVALID_PLAYER_ID)
            {
                continue;
            }
        }

        new Float:spawnZ;
        if (!Director_FindGround(spawnX, spawnY, gZones[zone][zoneZ], spawnZ)) continue;

        return Zombie_Create(type, spawnX, spawnY, spawnZ, lobbyId);
    }
    return INVALID_ZOMBIE;
}

stock E_ZOMBIE_TYPE:Director_PickType(zone)
{
    new tier = Zone_GetTier(zone);
    new roll = random(100);

    if (tier == 0)
    {
        return (roll < 70) ? ZT_WALKER : ZT_CRAWLER;
    }

    if (tier == 1)
    {
        if (roll < 45) return ZT_WALKER;
        if (roll < 65) return ZT_CRAWLER;
        if (roll < 82) return ZT_RUNNER;
        if (roll < 92) return ZT_BLOATER;
        return ZT_SCREAMER;
    }

    if (roll < 30) return ZT_WALKER;
    if (roll < 50) return ZT_RUNNER;
    if (roll < 62) return ZT_CRAWLER;
    if (roll < 74) return ZT_BRUTE;
    if (roll < 84) return ZT_BLOATER;
    if (roll < 92) return ZT_SCREAMER;

    return World_IsNight() ? ZT_STALKER : ZT_BRUTE;
}

/* Ambient population: keeps pressure per zone without scripting every zombie by hand. */
/* Heartbeat: tanpa ini tidak ada bukti di log bahwa spawn/respawn zombie berjalan
   saat server tidak ada pemain yang mengawasi. */
public Director_Report()
{
    if (GetTickCount() - DirectorLastReportTick < 60000) return 1;
    DirectorLastReportTick = GetTickCount();

    Zombie_RecountZones();

    new zoneLabel[48], line[144], lobbyLabel[32];

    for (new lobbyId = 0; lobbyId < LobbyCount; lobbyId++)
    {
        Lobby_GetName(lobbyId, lobbyLabel, sizeof(lobbyLabel));

        for (new zone = 1; zone < MAX_ZONE_COUNT; zone++)
        {
            if (ZoneZombieCount[lobbyId][zone] == 0) continue;
            Zone_GetName(zone, zoneLabel, sizeof(zoneLabel));
            format(line, sizeof(line), "[DIRECTOR] %s/%s: %d zombie", lobbyLabel, zoneLabel, ZoneZombieCount[lobbyId][zone]);
            Log_Info(line);
        }
    }

    format(line, sizeof(line), "[DIRECTOR] total aktif %d/%d | malam %d", Zombie_GetActiveCount(), MAX_ZOMBIES, World_IsNight());
    Log_Info(line);
    return 1;
}

/* Buang zombie yang paling jauh dari pemain sampai budget aktif terpenuhi.
   Tanpa ini, populasi 190 dari sesi lama tetap hidup dan terus mengirim sync. */
stock Zombie_TrimToBudget(budget)
{
    if (budget < 0) budget = 0;
    new over = Zombie_GetActiveCount() - budget;
    if (over <= 0) return 0;

    /* SATU pass: jarak pemain terdekat per zombie lewat spatial hash (bukan
       scan penuh PlayerPool), lalu pilih terjauh. Kode lama O(Z^2 x P) —
       rescan semua zombie tiap satu pembuangan, jutaan operasi saat horde
       besar menyusut (audit performa F1). */
    new trimIds[MAX_ZOMBIE_SLOTS];
    new Float:trimDist[MAX_ZOMBIE_SLOTS];
    new trimCount = 0;

    foreach (new zombieid : ZombiePool)
    {
        if (!Zombie_IsActive(zombieid)) continue;
        /* Zombie raid ber-anchor tidak boleh dibuang trim: mereka sedang
           menyerang base dan reward raid menghitung keberadaan mereka. */
        if (ZombieAI_HasAnchor(zombieid)) continue;

        new Float:zx, Float:zy, Float:zz;
        FCNPC_GetPosition(zombieid, zx, zy, zz);

        new Float:nearDistance;
        Hash_NearestPlayer(zx, zy, zz, 400.0, nearDistance, Zombie_GetLobby(zombieid));

        trimIds[trimCount] = zombieid;
        trimDist[trimCount] = nearDistance;
        trimCount++;
    }

    new removed = 0;
    while (removed < over && removed < trimCount)
    {
        new farthest = -1;
        for (new index = 0; index < trimCount; index++)
        {
            if (trimIds[index] == INVALID_ZOMBIE) continue;
            if (farthest == -1 || trimDist[index] > trimDist[farthest]) farthest = index;
        }
        if (farthest == -1) break;

        Zombie_QueueDestroy(trimIds[farthest]);
        trimIds[farthest] = INVALID_ZOMBIE;
        removed++;
    }

    if (removed > 0)
    {
        Log_Format(LOG_INFO, "[DIRECTOR] budget %d: %d zombie jauh dibuang", budget, removed);
    }
    return removed;
}

public Director_Tick()
{
    new perfStart = GetTickCount();
    Director_TickBody();
    Perf_Update(PERF_DIRECTOR, perfStart);
    return 1;
}

stock Director_TickBody()
{
    Zombie_RecountZones();

    if (Zombie_GetActiveCount() >= MAX_ZOMBIES) return 1;

    /* Server kosong: simpan zombie sebagai NPC parkir (reuse), jangan biarkan
       populasi menganggur menahan ~0,7 MB per NPC. Cek ini HARUS di depan
       daftar lobby: tanpa pemain, Lobby_ActiveList() kosong dan trim tak pernah
       tercapai (159 zombie menganggur setelah bot terakhir keluar). */
    new livePlayers = 0;
    foreach (new playerid : PlayerPool)
    {
        if (!IsPlayerConnected(playerid)) continue;
        if (IsPlayerNPC(playerid)) continue;
        livePlayers++;
    }
    if (livePlayers == 0)
    {
        if (Zombie_GetActiveCount() > 0)
        {
            new removedIdle = Zombie_TrimToBudget(0);
            Zombie_RecountZones();
            if (removedIdle > 0)
            {
                Log_Format(LOG_INFO, "[DIRECTOR] server kosong: %d zombie diparkir", removedIdle);
            }
        }
        return 1;
    }

    new activeLobbies[MAX_LOBBIES];
    new lobbyActive = Lobby_ActiveList(activeLobbies, MAX_LOBBIES);
    if (lobbyActive == 0) return 1;

    new totalPlayers = 0;
    for (new slot = 0; slot < lobbyActive; slot++)
    {
        totalPlayers += Lobby_CountPlayers(activeLobbies[slot], false);
    }
    if (totalPlayers == 0) return 1;

    /* Budget dinamis: satu pemain tidak perlu 190 NPC. Horde yang sedang aktif
       diberi jendela 3 menit sebelum ambient trim kembali normal. */
    new activeBudget = DIRECTOR_BUDGET_BASE + totalPlayers * DIRECTOR_BUDGET_PER_PLAYER;
    if (activeBudget > DIRECTOR_BUDGET_MAX) activeBudget = DIRECTOR_BUDGET_MAX;
    if (activeBudget > MAX_ZOMBIES) activeBudget = MAX_ZOMBIES;

    /* Kepadatan = biaya: satu NPC disinkronkan ke setiap pemain di sekitarnya,
       jadi zona yang menumpuk dibatasi lebih ketat daripada zona tersebar. */
    new zoneDensity[MAX_ZONE_COUNT];
    new peakDensity = 0;
    foreach (new playerid : PlayerPool)
    {
        if (!IsPlayerConnected(playerid)) continue;
        if (IsPlayerNPC(playerid)) continue;

        new zone = Player_GetZone(playerid);
        if (zone < 0 || zone >= MAX_ZONE_COUNT) continue;

        zoneDensity[zone]++;
        if (zoneDensity[zone] > peakDensity) peakDensity = zoneDensity[zone];
    }
    if (peakDensity >= FCNPC_DENSE_THRESHOLD && activeBudget > ZOMBIE_DENSE_CAP)
    {
        activeBudget = ZOMBIE_DENSE_CAP;
    }

    /* Batas nyata adalah slot server (maxplayers), bukan MAX_ZOMBIES: pemain dan
       NPC berbagi pool slot. Tanpa clamp ini spawn massal gagal dan server abort. */
    new slotBudget = Zombie_GetActiveCount() + Npc_SpawnRoom();
    if (activeBudget > slotBudget) activeBudget = slotBudget;
    /* Trim hanya saat kelebihan nyata (>25%). Trim presisi ke budget membuat
       zombie dibunuh tepat sebelum mencapai pemain: serangan jadi nol. */
    new trimBudget = activeBudget + (activeBudget / 4);
    if (GetTickCount() - DirectorLastHordeTick > 180000 && Zombie_GetActiveCount() > trimBudget)
    {
        Zombie_TrimToBudget(activeBudget);
        Zombie_RecountZones();
    }
    if (Zombie_GetActiveCount() >= activeBudget) return 1;

    /* Pemain yang membawa sampel zombie menarik perhatian: tiap pembawa menambah
       jatah zombie di lobinya, dibatasi supaya lobby kecil tidak dibanjiri. */
    new sampleCarriers[MAX_LOBBIES];
    foreach (new playerid : PlayerPool)
    {
        if (!Account_IsPlaying(playerid)) continue;
        if (Inventory_CountItem(playerid, ITEM_SPECIMEN) <= 0) continue;

        new lobbyId = Lobby_Of(playerid);
        if (lobbyId < 0 || lobbyId >= MAX_LOBBIES) continue;
        if (sampleCarriers[lobbyId] >= DIRECTOR_SAMPLE_PRESSURE_CAP) continue;
        sampleCarriers[lobbyId]++;
    }

    for (new zone = 0; zone < MAX_ZONE_COUNT; zone++)
    {
        if (Zone_IsHub(zone)) continue;

        new baseTarget = Zone_GetAmbientTarget(zone);
        if (baseTarget == 0) continue;

        if (World_IsNight()) baseTarget = floatround(float(baseTarget) * DIRECTOR_NIGHT_MULTIPLIER, floatround_ceil);

        for (new slot = 0; slot < lobbyActive; slot++)
        {
            new lobbyId = activeLobbies[slot];
            new lobbyPlayers = Lobby_CountPlayers(lobbyId, false);
            if (lobbyPlayers == 0) continue;

            /* Jatah per lobby proporsional: total zombie tetap di bawah MAX_ZOMBIES,
               bukan berlipat dua hanya karena ada dua dunia. */
            new target = floatround(float(baseTarget) * float(lobbyPlayers) / float(totalPlayers), floatround_ceil);
            target += sampleCarriers[lobbyId];
            if (target < 1) target = 1;
            if (target > DIRECTOR_ZONE_TARGET_MAX) target = DIRECTOR_ZONE_TARGET_MAX;

            if (ZoneZombieCount[lobbyId][zone] >= target) continue;

            if (random(100) < 55)
            {
                new E_ZOMBIE_TYPE:type = Director_PickType(zone);
                if (Director_SpawnForZone(zone, type, lobbyId) != INVALID_ZOMBIE)
                {
                    ZoneZombieCount[lobbyId][zone]++;
                }
            }
        }
    }
    return 1;
}

/* Horde night: one zone gets a real push, announced to everyone in it. */
stock Director_TriggerHorde(zone, count = 45, lobbyId = -1)
{
    if (Zone_IsHub(zone)) return 0;
    if (Zombie_GetActiveCount() >= MAX_ZOMBIES) return 0;

    new zoneLabel[48];
    Zone_GetName(zone, zoneLabel, sizeof(zoneLabel));

    if (lobbyId >= 0)
    {
        if (!Lobby_IsValid(lobbyId)) return 0;

        new room = MAX_ZOMBIES - Zombie_GetActiveCount();
        if (count > room) count = room;

        new spawned = 0;
        for (new index = 0; index < count; index++)
        {
            if (Director_SpawnForZone(zone, Director_PickType(zone), lobbyId) == INVALID_ZOMBIE) break;
            spawned++;
        }

        if (spawned == 0) return 0;

        Director_NotifyLobby(lobbyId, "HORDE", zoneLabel, UI_ACCENT_DANGER);
        DirectorLastHordeTick = GetTickCount();
        return spawned;
    }

    new activeLobbies[MAX_LOBBIES];
    new lobbyActive = Lobby_ActiveList(activeLobbies, MAX_LOBBIES);
    if (lobbyActive == 0) return 0;

    new perLobby = count / lobbyActive;
    if (perLobby < 1) perLobby = 1;

    new totalSpawned = 0;
    for (new slot = 0; slot < lobbyActive; slot++)
    {
        new room = MAX_ZOMBIES - Zombie_GetActiveCount();
        if (room <= 0) break;

        new quota = (perLobby > room) ? room : perLobby;
        new spawned = 0;
        for (new index = 0; index < quota; index++)
        {
            if (Director_SpawnForZone(zone, Director_PickType(zone), activeLobbies[slot]) == INVALID_ZOMBIE) break;
            spawned++;
        }
        totalSpawned += spawned;
    }

    if (totalSpawned == 0) return 0;

    Toast_NotifyAll("HORDE", zoneLabel, UI_ACCENT_DANGER);
    DirectorLastHordeTick = GetTickCount();
    return totalSpawned;
}

/* Boss malam: satu Abomination per lobi, cooldown 15 menit nyata, hadiah sekali. */
new BossZombie[MAX_LOBBIES] = {INVALID_ZOMBIE, ...};
new BossLastSpawnTick[MAX_LOBBIES];
new bool:BossEnraged[MAX_LOBBIES];

/* Dipakai HUD untuk bar HP boss di lobi pemain. */
stock bool:Director_BossStatus(lobbyId, &Float:health, &Float:maxHealth)
{
    if (lobbyId < 0 || lobbyId >= MAX_LOBBIES) return false;
    if (BossZombie[lobbyId] == INVALID_ZOMBIE) return false;
    if (!Zombie_IsActive(BossZombie[lobbyId])) return false;

    health = Zombie_GetHealth(BossZombie[lobbyId]);
    maxHealth = Zombie_GetMaxHealth(BossZombie[lobbyId]);
    return (health > 0.0);
}

stock Director_OnBossKilled()
{
    for (new lobby = 0; lobby < MAX_LOBBIES; lobby++)
    {
        if (BossZombie[lobby] == INVALID_ZOMBIE) continue;
        if (Zombie_IsActive(BossZombie[lobby])) continue;
        BossZombie[lobby] = INVALID_ZOMBIE;
        BossEnraged[lobby] = false;
    }
    return 1;
}

/* Fase 2: begitu HP boss di bawah 50%, dia memanggil gerombolan sekali saja. */
stock Director_BossPhaseTwo(lobbyId)
{
    if (BossEnraged[lobbyId]) return 0;

    new Float:health, Float:maxHealth;
    if (!Director_BossStatus(lobbyId, health, maxHealth)) return 0;
    if (health > maxHealth * (float(ZOMBIE_BOSS_PHASE2_PERCENT) / 100.0)) return 0;

    BossEnraged[lobbyId] = true;

    new Float:x, Float:y, Float:z;
    if (!FCNPC_GetPosition(BossZombie[lobbyId], x, y, z)) return 0;

    for (new spawnIndex = 0; spawnIndex < ZOMBIE_BOSS_PHASE2_ADDS; spawnIndex++)
    {
        if (Zombie_GetActiveCount() >= MAX_ZOMBIES - 1) break;
        Director_SpawnNearPoint(ZT_RUNNER, x, y, z, 12.0, lobbyId, 4.0);
    }

    Director_NotifyLobby(lobbyId, "ABOMINATION MENGAMUK",
        "Dia memanggil gerombolan. Bertahan atau lari.", UI_ACCENT_DANGER);
    Log_Format(LOG_INFO, "[BOSS] lobi %d fase 2 health=%.0f", lobbyId, health);
    return 1;
}

stock Director_BossTick(lobbyId)
{
    if (!World_IsNight()) return 0;

    if (BossZombie[lobbyId] != INVALID_ZOMBIE)
    {
        if (Zombie_IsActive(BossZombie[lobbyId]))
        {
            Director_BossPhaseTwo(lobbyId);
            return 1;
        }

        BossZombie[lobbyId] = INVALID_ZOMBIE;
        BossEnraged[lobbyId] = false;
    }

    new now = GetTickCount();
    if (BossLastSpawnTick[lobbyId] != 0 && now - BossLastSpawnTick[lobbyId] < BOSS_RESPAWN_MS) return 0;

    /* Boss butuh pemain di luar hub: tidak ada tontonan AFK di safe zone.
       Anchor juga harus layak (audit N3): level >= 5 ATAU party >= 2 ATAU
       zona tier >= 2 — newbie solo berpistol tidak boleh jadi anchor
       Abomination 2400 HP. */
    new anchor = INVALID_PLAYER_ID;
    foreach (new playerid : PlayerPool)
    {
        if (!Player_IsAlive(playerid)) continue;
        if (Lobby_Of(playerid) != lobbyId) continue;
        if (Zone_IsHub(Player_GetZone(playerid))) continue;

        new bool:partyReady = Party_InParty(playerid) && Party_Count(Party_LeaderOf(playerid)) >= 2;
        if (PlayerSurvival[playerid][psLevel] < 5 && !partyReady
            && Zone_GetTier(Player_GetZone(playerid)) < 2) continue;

        anchor = playerid;
        break;
    }

    if (anchor == INVALID_PLAYER_ID) return 0;
    if (Zombie_GetActiveCount() >= MAX_ZOMBIES - 2) return 0;
    if (random(100) >= BOSS_SPAWN_CHANCE) return 0;

    new Float:x, Float:y, Float:z;
    GetPlayerPos(anchor, x, y, z);

    new boss = Director_SpawnNearPoint(ZT_BOSS, x, y, z, BOSS_SPAWN_RADIUS, lobbyId, BOSS_MIN_PLAYER_DIST);
    if (boss == INVALID_ZOMBIE) return 0;

    BossZombie[lobbyId] = boss;
    BossLastSpawnTick[lobbyId] = now;
    BossEnraged[lobbyId] = false;
    Director_NotifyLobby(lobbyId, "ABOMINATION",
        "Sesuatu yang besar bergerak di kegelapan. Jangan hadapi sendiri.", UI_ACCENT_DANGER);
    Log_Format(LOG_INFO, "[BOSS] lobi %d spawn id=%d", lobbyId, boss);
    return 1;
}

/* Automatic night horde: every 15 in-game night minutes the map escalates once. */
public Director_NightWatch()
{
    if (!World_IsNight()) return 0;

    for (new lobbyId = 0; lobbyId < MAX_LOBBIES; lobbyId++) Director_BossTick(lobbyId);
    if (GetTickCount() - DirectorLastHordeTick < 900000) return 0;
    if (Zombie_GetActiveCount() < 60) return 0;

    new bestZone = -1;
    new bestPlayers = 0;

    for (new zone = 0; zone < MAX_ZONE_COUNT; zone++)
    {
        if (Zone_IsHub(zone)) continue;

        new zonePlayers = 0;
        foreach (new playerid : PlayerPool)
        {
            if (!Player_IsAlive(playerid)) continue;
            if (Player_GetZone(playerid) != zone) continue;
            zonePlayers++;
        }

        if (zonePlayers > bestPlayers)
        {
            bestPlayers = zonePlayers;
            bestZone = zone;
        }
    }

    if (bestZone == -1) return 0;
    return Director_TriggerHorde(bestZone, 30);
}

/* Spawn di sekitar titik bebas (base pemain), bukan pusat zona: dipakai raid base.
   Jarak minimum ke pemain tetap dijaga supaya zombie tidak muncul di pangkuan. */
stock Director_SpawnNearPoint(E_ZOMBIE_TYPE:type, Float:centerX, Float:centerY, Float:centerZ,
    Float:radius, lobbyId = 0, Float:minPlayerDistance = DIRECTOR_MIN_SPAWN_DISTANCE)
{
    for (new attempt = 0; attempt < 8; attempt++)
    {
        new Float:angle = float(random(360)) * 0.017453292;
        new Float:distance = radius * (0.8 + float(random(40)) / 100.0);

        new Float:spawnX = centerX + floatsin(angle) * distance;
        new Float:spawnY = centerY + floatcos(angle) * distance;

        new Float:playerDistance;
        if (Hash_NearestPlayer(spawnX, spawnY, centerZ, minPlayerDistance, playerDistance, lobbyId) != INVALID_PLAYER_ID)
        {
            continue;
        }

        new Float:spawnZ;
        if (!Director_FindGround(spawnX, spawnY, centerZ, spawnZ)) continue;

        return Zombie_Create(type, spawnX, spawnY, spawnZ, lobbyId);
    }
    return INVALID_ZOMBIE;
}
