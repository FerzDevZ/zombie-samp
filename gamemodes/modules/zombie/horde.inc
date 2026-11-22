#if defined _ls_horde_included
    #endinput
#endif
#define _ls_horde_included

/* Horde director 2.0: panas (heat) per zona per lobby dari kebisingan pemain, bukan
   timer buta. Tembakan di zona mengumpulkan heat; heat tinggi memicu push lokal setelah
   cooldown. Heat dipisah per lobby supaya tembakan di Lobby 1 tidak memanggil zombie
   ke Lobby 2. */

new Float:ZoneHeat[MAX_LOBBIES][MAX_ZONE_COUNT];
new HordeLastPushTick[MAX_LOBBIES][MAX_ZONE_COUNT];
new ScreamerLastPulseTick = 0;

/* Heat per base (di-key slot pemilik, lobby terpisah): hanya noise di dalam
   BASE_HEAT_RADIUS petak base yang menaikkan heat. Raid memilih heat tertinggi. */
new Float:BaseHeat[MAX_LOBBIES][MAX_PLAYERS];
new BaseHeatChar[MAX_LOBBIES][MAX_PLAYERS];
new HordeLastBaseRaidTick[MAX_LOBBIES];
new BaseRaidOwner[MAX_LOBBIES];
new BaseRaidOwnerChar[MAX_LOBBIES];
new BaseRaidStartTick[MAX_LOBBIES];
/* Posisi base saat raid dimulai: reward menuntut pemilik hadir di dekat
   base (tanpa ini raid bisa dipicu sendiri lalu dipanen turret dari hub
   sambil AFK — audit F2). */
new Float:BaseRaidX[MAX_LOBBIES];
new Float:BaseRaidY[MAX_LOBBIES];
new Float:BaseRaidZ[MAX_LOBBIES];
new bool:BaseRaidActive[MAX_LOBBIES];

forward Horde_Tick();

/* Base pemain dengan heat tertinggi di satu lobby jadi sasaran raid. */
stock bool:Horde_FindBaseTarget(lobbyId, &Float:targetX, &Float:targetY, &Float:targetZ,
    &targetZone, &ownerPlayer, &ownerCharId)
{
    new Float:bestHeat = BASE_RAID_MIN_HEAT;
    new found = 0;

    foreach (new playerid : PlayerPool)
    {
        if (!IsPlayerConnected(playerid)) continue;
        if (Lobby_Of(playerid) != lobbyId) continue;
        if (Base_Id(playerid) == 0) continue;

        new Float:plotX, Float:plotY, Float:plotZ;
        Base_GetPlot(playerid, plotX, plotY, plotZ);
        if (plotX == 0.0 && plotY == 0.0) continue;

        new zone = Zone_GetIdAt(plotX, plotY, plotZ);
        if (zone < 0 || zone >= MAX_ZONE_COUNT) continue;
        if (Zone_IsHub(zone)) continue;

        if (BaseHeat[lobbyId][playerid] < bestHeat) continue;

        bestHeat = BaseHeat[lobbyId][playerid];
        targetX = plotX;
        targetY = plotY;
        targetZ = plotZ;
        targetZone = zone;
        ownerPlayer = playerid;
        ownerCharId = ActiveCharId[playerid];
        found = 1;
    }
    return found == 1;
}

stock Horde_CountRaidZombies(lobbyId)
{
    new count = 0;
    foreach (new zombieid : ZombiePool)
    {
        if (Zombie_GetLobby(zombieid) != lobbyId) continue;
        if (!ZombieAI_HasAnchor(zombieid)) continue;
        count++;
    }
    return count;
}

stock Horde_RewardRaidSurvived(lobbyId)
{
    new owner = BaseRaidOwner[lobbyId];
    BaseRaidActive[lobbyId] = false;
    BaseRaidOwner[lobbyId] = INVALID_PLAYER_ID;

    if (owner == INVALID_PLAYER_ID || !IsPlayerConnected(owner)) return 0;
    if (Lobby_Of(owner) != lobbyId) return 0;
    /* Slot player bisa dipakai ulang orang lain: reward hanya untuk karakter
       yang sama dengan pemilik raid saat dimulai. */
    if (BaseRaidOwnerChar[lobbyId] <= 0 || ActiveCharId[owner] != BaseRaidOwnerChar[lobbyId]) return 0;

    /* Pemilik harus hadir mempertahankan base (<= 40 m): AFK di hub sementara
       turret membersihkan raid tidak boleh membayar (audit F2). */
    new Float:ownerX, Float:ownerY, Float:ownerZ;
    GetPlayerPos(owner, ownerX, ownerY, ownerZ);
    if (VectorSize(ownerX - BaseRaidX[lobbyId], ownerY - BaseRaidY[lobbyId], ownerZ - BaseRaidZ[lobbyId]) > 40.0)
    {
        SendClientMessage(owner, UI_ACCENT_WARN, "[BASE] Raid surut, tapi kamu tidak di tempat. Reward hangus.");
        BaseRaidOwnerChar[lobbyId] = 0;
        return 0;
    }

    PlayerSurvival[owner][psCash] += BASE_RAID_REWARD_CASH;
    Skills_AddXp(owner, BASE_RAID_REWARD_XP);
    RankPoints[owner] += 15;

    new message[128];
    format(message, sizeof(message),
        "Base bertahan! +$%d dan +%d XP.", BASE_RAID_REWARD_CASH, BASE_RAID_REWARD_XP);
    SendClientMessage(owner, UI_ACCENT_PRIMARY, message);
    Toast_Show(owner, "RAID DIPATAHKAN", "Base-mu masih berdiri.", UI_ACCENT_PRIMARY);

    Log_Format(LOG_INFO, "[HORDE] raid base dipatahkan char=%d lobby=%d",
        BaseRaidOwnerChar[lobbyId], lobbyId);
    BaseRaidOwnerChar[lobbyId] = 0;
    return 1;
}

stock Horde_TickBaseRaid(lobbyId)
{
    if (BaseRaidActive[lobbyId])
    {
        new elapsed = GetTickCount() - BaseRaidStartTick[lobbyId];

        if (elapsed > 12000 && Horde_CountRaidZombies(lobbyId) == 0)
        {
            Horde_RewardRaidSurvived(lobbyId);
            return 1;
        }

        if (elapsed > BASE_RAID_TIMEOUT_MS)
        {
            BaseRaidActive[lobbyId] = false;
            BaseRaidOwner[lobbyId] = INVALID_PLAYER_ID;
            BaseRaidOwnerChar[lobbyId] = 0;
            Director_NotifyLobby(lobbyId, "BASE DISERANG",
                "Gerombolan surut. Periksa dinding dan generator.", UI_ACCENT_WARN);
        }
        return 0;
    }

    if (GetTickCount() - HordeLastBaseRaidTick[lobbyId] < HORDE_BASE_RAID_COOLDOWN_MS) return 0;

    new Float:baseX, Float:baseY, Float:baseZ, baseZone, ownerPlayer, ownerCharId;
    if (!Horde_FindBaseTarget(lobbyId, baseX, baseY, baseZ, baseZone, ownerPlayer, ownerCharId)) return 0;

    new wanted = HORDE_BASE_RAID_MIN + random(HORDE_BASE_RAID_MAX - HORDE_BASE_RAID_MIN + 1);
    new spawned = 0;

    for (new index = 0; index < wanted; index++)
    {
        new zombieid = Director_SpawnNearPoint(Director_PickType(baseZone), baseX, baseY, baseZ,
            HORDE_BASE_RAID_RADIUS, lobbyId);
        if (zombieid == INVALID_ZOMBIE) break;

        ZombieAI_SetAnchor(zombieid, baseX, baseY, baseZ);
        spawned++;
    }

    if (spawned == 0) return 0;

    HordeLastBaseRaidTick[lobbyId] = GetTickCount();
    /* Raid base juga horde: beri jendela 3 menit sebelum ambient trim boleh
       membuang zombie lagi (tanpa ini raid bisa dipangkas director). */
    DirectorLastHordeTick = GetTickCount();
    BaseRaidActive[lobbyId] = true;
    BaseRaidOwner[lobbyId] = ownerPlayer;
    BaseRaidOwnerChar[lobbyId] = ownerCharId;
    BaseRaidStartTick[lobbyId] = GetTickCount();
    BaseRaidX[lobbyId] = baseX;
    BaseRaidY[lobbyId] = baseY;
    BaseRaidZ[lobbyId] = baseZ;
    BaseHeat[lobbyId][ownerPlayer] = 0.0;

    new ownerName[MAX_PLAYER_NAME];
    GetPlayerName(ownerPlayer, ownerName, sizeof(ownerName));

    new message[144];
    format(message, sizeof(message),
        "%d zombie menyerbu base %s. Lindungi dinding!", spawned, ownerName);
    Director_NotifyLobby(lobbyId, "BASE DISERANG", message, UI_ACCENT_DANGER);

    Log_Format(LOG_INFO, "[HORDE] raid base char=%d lobby=%d zona=%d zombie=%d",
        ownerCharId, lobbyId, baseZone, spawned);
    return 1;
}


stock Horde_ResetBaseHeat(playerid)
{
    if (playerid < 0 || playerid >= MAX_PLAYERS) return 0;

    new lobbyId = Lobby_Of(playerid);
    if (lobbyId < 0 || lobbyId >= MAX_LOBBIES) return 0;

    BaseHeat[lobbyId][playerid] = 0.0;
    BaseHeatChar[lobbyId][playerid] = ActiveCharId[playerid];
    return 1;
}

stock Horde_AddHeat(Float:x, Float:y, Float:z, Float:radius)
{
    new zone = Zone_GetIdAt(x, y, z);
    if (zone < 0 || zone >= MAX_ZONE_COUNT) return 0;
    if (Zone_IsHub(zone)) return 0;

    /* Noise milik lobby penembak: pemain dalam radius sumber suara.
       Iterasi sel spatial hash dalam radius (bukan scan 200 pemain penuh
       per tembakan — audit performa F13: 400 tembakan/dtk x 200 = 80 ribu
       native/dtk). Pola sama dengan Hash_NearestPlayer. */
    new touched = 0;
    new baseCell = Hash_PlayerCellIndex(x, y);
    new centerX = baseCell / HASH_PLAYER_CELLS;
    new centerY = baseCell % HASH_PLAYER_CELLS;
    new span = floatround((radius + 2.0) / 200.0, floatround_ceil) + 1;

    for (new cellX = centerX - span; cellX <= centerX + span; cellX++)
    {
        if (cellX < 0 || cellX >= HASH_PLAYER_CELLS) continue;
        for (new cellY = centerY - span; cellY <= centerY + span; cellY++)
        {
            if (cellY < 0 || cellY >= HASH_PLAYER_CELLS) continue;
            new cellIndex = cellX * HASH_PLAYER_CELLS + cellY;

            foreach (new playerid : HashCellPlayers[cellIndex])
            {
                if (!IsPlayerConnected(playerid)) continue;
                if (IsPlayerNPC(playerid)) continue;
                if (!Player_IsAlive(playerid)) continue;

                new Float:px, Float:py, Float:pz;
                GetPlayerPos(playerid, px, py, pz);
                if (VectorSize(px - x, py - y, pz - z) > radius + 2.0) continue;

                new lobbyId = Lobby_Of(playerid);
                ZoneHeat[lobbyId][zone] += radius / 10.0;

                /* Kebisingan di dalam petak base sendiri yang membangunkan raid terarah. */
                if (Base_Id(playerid) != 0)
                {
                    new Float:plotX, Float:plotY, Float:plotZ;
                    Base_GetPlot(playerid, plotX, plotY, plotZ);
                    if (VectorSize(plotX - x, plotY - y, 0.0) <= BASE_HEAT_RADIUS)
                    {
                        BaseHeat[lobbyId][playerid] += radius / BASE_HEAT_GAIN_DIV;
                    }
                }

                touched++;
            }
        }
    }

    if (touched == 0)
    {
        ZoneHeat[0][zone] += radius / 10.0;
    }
    return 1;
}

public Horde_Tick()
{
    new perfStart = GetTickCount();
    foreach (new playerid : PlayerPool)
    {
        new lobbyId = Lobby_Of(playerid);
        if (lobbyId < 0 || lobbyId >= MAX_LOBBIES) continue;

        /* Slot dipakai ulang oleh karakter lain: heat lama tidak boleh ikut. */
        if (BaseHeatChar[lobbyId][playerid] != ActiveCharId[playerid])
        {
            BaseHeatChar[lobbyId][playerid] = ActiveCharId[playerid];
            BaseHeat[lobbyId][playerid] = 0.0;
        }

        BaseHeat[lobbyId][playerid] *= BASE_HEAT_DECAY;
        if (BaseHeat[lobbyId][playerid] < 0.5) BaseHeat[lobbyId][playerid] = 0.0;
    }

    for (new lobbyId = 0; lobbyId < LobbyCount; lobbyId++)
    {
        if (Lobby_CountPlayers(lobbyId, false) == 0) continue;

        for (new zone = 0; zone < MAX_ZONE_COUNT; zone++)
        {
            if (Zone_IsHub(zone)) continue;

            ZoneHeat[lobbyId][zone] *= HORDE_HEAT_DECAY;
            if (ZoneHeat[lobbyId][zone] < 0.5) ZoneHeat[lobbyId][zone] = 0.0;

            if (ZoneHeat[lobbyId][zone] < HORDE_HEAT_TRIGGER) continue;
            if (GetTickCount() - HordeLastPushTick[lobbyId][zone] < HORDE_COOLDOWN_MS) continue;

            new playersInZone = 0;
            foreach (new playerid : PlayerPool)
            {
                if (!Player_IsAlive(playerid)) continue;
                if (Lobby_Of(playerid) != lobbyId) continue;
                if (Player_GetZone(playerid) != zone) continue;
                playersInZone++;
            }

            if (playersInZone == 0)
            {
                ZoneHeat[lobbyId][zone] = 0.0;
                continue;
            }

            new wanted = HORDE_CLUSTER_MIN + random(HORDE_CLUSTER_MAX - HORDE_CLUSTER_MIN + 1);
            new spawned = 0;

            for (new index = 0; index < wanted; index++)
            {
                if (Director_SpawnForZone(zone, Director_PickType(zone), lobbyId) == INVALID_ZOMBIE) break;
                spawned++;
            }

            if (spawned == 0) continue;

            ZoneHeat[lobbyId][zone] = 0.0;
            HordeLastPushTick[lobbyId][zone] = GetTickCount();

            new zoneLabel[48];
            Zone_GetName(zone, zoneLabel, sizeof(zoneLabel));

            new message[128];
            format(message, sizeof(message), "Bising di %s memanggil %d zombie.", zoneLabel, spawned);
            Director_NotifyLobby(lobbyId, "HORDE", message, UI_ACCENT_DANGER);
        }

        Horde_TickBaseRaid(lobbyId);
    }

    Horde_ScreamerPulse();
    Perf_Update(PERF_HORDE, perfStart);
    return 1;
}

/* Uji raid tanpa menunggu heat: spawn di sekitar petak base pemain sendiri, di lobby-nya. */
stock Horde_ForceBaseRaid(playerid)
{
    if (Base_Id(playerid) == 0)
    {
        SendClientMessage(playerid, UI_ACCENT_WARN, "[HORDE] Base belum dimuat. Coba /base dulu.");
        return 0;
    }

    new Float:plotX, Float:plotY, Float:plotZ;
    Base_GetPlot(playerid, plotX, plotY, plotZ);
    if (plotX == 0.0 && plotY == 0.0)
    {
        SendClientMessage(playerid, UI_ACCENT_WARN, "[HORDE] Petak base belum ada. Bangun satu struktur dulu (/build).");
        return 0;
    }

    new zone = Zone_GetIdAt(plotX, plotY, plotZ);
    new lobbyId = Lobby_Of(playerid);
    new spawned = 0;

    for (new index = 0; index < 12; index++)
    {
        new zombieid = Director_SpawnNearPoint(Director_PickType(zone), plotX, plotY, plotZ,
            HORDE_BASE_RAID_RADIUS, lobbyId);
        if (zombieid == INVALID_ZOMBIE) break;

        ZombieAI_SetAnchor(zombieid, plotX, plotY, plotZ);
        spawned++;
    }

    if (spawned > 0)
    {
        BaseRaidActive[lobbyId] = true;
        BaseRaidOwner[lobbyId] = playerid;
        BaseRaidOwnerChar[lobbyId] = ActiveCharId[playerid];
        BaseRaidStartTick[lobbyId] = GetTickCount();
        HordeLastBaseRaidTick[lobbyId] = GetTickCount();
        DirectorLastHordeTick = GetTickCount();
        BaseHeat[lobbyId][playerid] = 0.0;
    }

    new message[128];
    format(message, sizeof(message), "[HORDE] Raid uji: %d zombie menuju base (zona %d).", spawned, zone);
    SendClientMessage(playerid, UI_ACCENT_PRIMARY, message);
    Log_Format(LOG_INFO, "[HORDE] raid paksa char=%d lobby=%d zona=%d zombie=%d",
        ActiveCharId[playerid], lobbyId, zone, spawned);
    return 1;
}

/* Screamer yang hidup menambah tekanan: 3 zombie baru di sekitarnya tiap 20 detik
   selama ada pemain di lobby yang sama dalam jarak SCREAMER_RANGE. */
stock Horde_ScreamerPulse()
{
    if (GetTickCount() - ScreamerLastPulseTick < SCREAMER_PULSE_MS) return 0;

    new bool:foundScreamer = false;
    new Float:screamerX, Float:screamerY, Float:screamerZ;
    new screamerLobby = 0;
    new nearestPlayer = INVALID_PLAYER_ID;

    foreach (new zombieid : ZombiePool)
    {
        if (_:Zombie_GetType(zombieid) != _:ZT_SCREAMER) continue;

        new Float:zx, Float:zy, Float:zz;
        FCNPC_GetPosition(zombieid, zx, zy, zz);
        new zombieLobby = Zombie_GetLobby(zombieid);

        foreach (new playerid : PlayerPool)
        {
            if (!Player_IsAlive(playerid)) continue;
            if (Lobby_Of(playerid) != zombieLobby) continue;

            new Float:px, Float:py, Float:pz;
            GetPlayerPos(playerid, px, py, pz);

            new Float:dx = px - zx;
            new Float:dy = py - zy;
            if (floatsqroot(dx * dx + dy * dy) > SCREAMER_RANGE) continue;

            foundScreamer = true;
            screamerX = zx;
            screamerY = zy;
            screamerZ = zz;
            screamerLobby = zombieLobby;
            nearestPlayer = playerid;
            break;
        }
        if (foundScreamer) break;
    }

    if (!foundScreamer) return 0;

    ScreamerLastPulseTick = GetTickCount();
    Noise_EmitAtPosition(screamerX, screamerY, screamerZ, 60.0, screamerLobby);

    new zone = Zone_GetIdAt(screamerX, screamerY, screamerZ);
    if (zone < 0 || zone >= MAX_ZONE_COUNT) return 0;

    for (new index = 0; index < 3; index++)
    {
        Director_SpawnForZone(zone, Director_PickType(zone), screamerLobby);
    }

    if (nearestPlayer != INVALID_PLAYER_ID)
    {
        SendClientMessage(nearestPlayer, UI_ACCENT_DANGER, "[HORDE] Screamer memanggil bala bantuan.");
    }
    return 1;
}

/* -1 = panas tertinggi dari lobby mana pun (dipakai tampilan zona). */
stock Float:Horde_GetZoneHeat(zone, lobbyId = -1)
{
    if (zone < 0 || zone >= MAX_ZONE_COUNT) return 0.0;
    if (lobbyId >= 0 && lobbyId < MAX_LOBBIES) return ZoneHeat[lobbyId][zone];

    new Float:best = 0.0;
    for (new index = 0; index < MAX_LOBBIES; index++)
    {
        if (ZoneHeat[index][zone] > best) best = ZoneHeat[index][zone];
    }
    return best;
}
