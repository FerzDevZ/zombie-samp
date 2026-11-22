#if defined _ls_loot_nodes_included
    #endinput
#endif
#define _ls_loot_nodes_included

/* Peti loot yang muncul sendiri di zona berbahaya. Ini yang membuat dunia terasa
   hidup: pemain tidak hanya menunggu zombie mati, tapi juga berburu peti.
   Setiap lobby punya jatah sendiri supaya lobby sepi tidak "mencuri" loot lobby lain. */

#define LOOT_NODE_LIMIT_PER_LOBBY 8
#define LOOT_NODE_MIN_PER_LOBBY 3
#define LOOT_NODE_INTERVAL_MS 20000
#define LOOT_NODE_MIN_PLAYER_DIST 12.0
#define LOOT_NEAR_MIN_DIST 45.0
#define LOOT_NEAR_MAX_DIST 120.0
#define LOOT_NEAR_GUARANTEE 2     // peti yang dijamin muncul dekat pemain
#define LOOT_NODE_SPOT_RADIUS 40

new LootNodeLastTick = 0;

/* Tingkat peti ditentukan saat spawn, bukan saat diambil: label, warna map, dan
   isi harus konsisten sejak peti muncul di dunia. */
stock LootNode_RollTier()
{
    new chance = World_IsNight() ? LOOT_NODE_RARE_CHANCE_NIGHT : LOOT_NODE_RARE_CHANCE;
    return (random(100) < chance) ? LOOT_TIER_RARE : LOOT_TIER_NORMAL;
}

stock bool:LootNode_RollCrateLoot(tier, &itemid, &amount)
{
    if (tier == LOOT_TIER_RARE) return Loot_RollTable(gRareLoot, LOOT_RARE_SLOTS, itemid, amount);
    return Loot_RollTable(gNodeLoot, LOOT_NODE_SLOTS, itemid, amount);
}


forward LootNode_Tick();

stock LootNode_ActiveCount(lobbyId)
{
    new count = 0;
    for (new slot = 0; slot < MAX_LOOT_DROPS; slot++)
    {
        if (!LootDrop[slot][ldActive]) continue;
        if (LootDrop[slot][ldOwnerPlayer] != INVALID_PLAYER_ID) continue;
        // Drop zombie juga ownerless; jangan sampai item di tanah menghabiskan
        // jatah peti dan membuat dunia terlihat tanpa loot.
        if (LootDrop[slot][ldKind] != LOOT_KIND_CRATE) continue;
        if (LootDrop[slot][ldLobby] != lobbyId) continue;
        count++;
    }
    return count;
}

stock bool:LootNode_FindSpot(lobbyId, &Float:outX, &Float:outY, &Float:outZ)
{
    for (new attempt = 0; attempt < 24; attempt++)
    {
        new zone = random(MAX_ZONE_COUNT);
        if (Zone_IsHub(zone)) continue;
        if (Zone_GetAmbientTarget(zone) == 0) continue;

        new Float:spotX = gZones[zone][zoneX] + float(random(LOOT_NODE_SPOT_RADIUS * 2 + 1) - LOOT_NODE_SPOT_RADIUS);
        new Float:spotY = gZones[zone][zoneY] + float(random(LOOT_NODE_SPOT_RADIUS * 2 + 1) - LOOT_NODE_SPOT_RADIUS);

        new Float:hitX, Float:hitY, Float:hitZ;
        new hit = CA_RayCastLine(spotX, spotY, gZones[zone][zoneZ] + 120.0,
            spotX, spotY, gZones[zone][zoneZ] - 80.0, hitX, hitY, hitZ);
        if (hit == 0) continue;

        /* Jangan jatuhkan peti tepat di kaki pemain: loot harus dicari, bukan gratis. */
        new tooClose = 0;
        foreach (new playerid : PlayerPool)
        {
            if (!Player_IsAlive(playerid)) continue;
            if (Lobby_Of(playerid) != lobbyId) continue;

            new Float:px, Float:py, Float:pz;
            GetPlayerPos(playerid, px, py, pz);
            if (VectorSize(px - spotX, py - spotY, pz - hitZ) < LOOT_NODE_MIN_PLAYER_DIST)
            {
                tooClose = 1;
                break;
            }
        }
        if (tooClose) continue;

        outX = spotX;
        outY = spotY;
        outZ = hitZ + 1.0;
        return true;
    }
    return false;
}

stock LootNode_SpawnOne(lobbyId)
{
    if (!Lobby_IsValid(lobbyId)) return -1;
    if (LootNode_ActiveCount(lobbyId) >= LOOT_NODE_LIMIT_PER_LOBBY) return -1;

    new Float:spotX, Float:spotY, Float:spotZ;
    if (!LootNode_FindSpot(lobbyId, spotX, spotY, spotZ)) return -1;

    new tier = LootNode_RollTier();
    new rolledItem = ITEM_NONE_ID, rolledAmount = 1;
    if (!LootNode_RollCrateLoot(tier, rolledItem, rolledAmount)) return -1;

    new slot = Loot_Spawn(spotX, spotY, spotZ, rolledItem, rolledAmount, INVALID_PLAYER_ID, lobbyId,
        LOOT_KIND_CRATE, WEAPON_CONDITION_MAX, tier);
    if (slot == -1) return -1;

    Log_Format(LOG_INFO, "[LOOT] peti lobby=%d zona=%d item=%d jumlah=%d tier=%d",
        lobbyId, Zone_GetIdAt(spotX, spotY, spotZ), rolledItem, rolledAmount, tier);
    return slot;
}

/* Peti pertama harus sudah ada saat pemain masuk, bukan setelah satu menit.
   Tiga peti per lobby membuat dunia langsung terasa bisa dicari. */
stock LootNode_RandomPlayerInLobby(lobbyId)
{
    new candidates[MAX_PLAYERS], found = 0;

    foreach (new playerid : PlayerPool)
    {
        if (!Account_IsPlaying(playerid)) continue;
        if (Lobby_Of(playerid) != lobbyId) continue;
        if (!Player_IsAlive(playerid)) continue;

        candidates[found++] = playerid;
    }

    if (found == 0) return INVALID_PLAYER_ID;
    return candidates[random(found)];
}

/* Peti di dekat pemain: radius 45-120 m, jadi masih harus dicari sebentar
   tetapi kelihatan di layar dan ikonnya muncul di map. */
stock bool:LootNode_FindSpotNear(Float:cx, Float:cy, Float:cz, &Float:outX, &Float:outY, &Float:outZ)
{
    for (new attempt = 0; attempt < 24; attempt++)
    {
        new Float:angle = float(random(360));
        new Float:distance = LOOT_NEAR_MIN_DIST + (LOOT_NEAR_MAX_DIST - LOOT_NEAR_MIN_DIST) * (float(random(100)) / 100.0);
        new Float:spotX = cx + floatsin(-angle, degrees) * distance;
        new Float:spotY = cy + floatcos(-angle, degrees) * distance;

        new Float:hitX, Float:hitY, Float:hitZ;
        if (CA_RayCastLine(spotX, spotY, cz + 130.0, spotX, spotY, cz - 90.0, hitX, hitY, hitZ) == 0) continue;
        if (hitZ < -25.0 || hitZ > 120.0) continue;

        new landedZone = Zone_GetIdAt(hitX, hitY, hitZ);
        if (Zone_IsHub(landedZone)) continue;   // jangan taruh peti di dalam camp

        outX = hitX;
        outY = hitY;
        outZ = hitZ + 1.0;
        return true;
    }
    return false;
}

stock LootNode_SpawnNearPlayer(lobbyId, playerid)
{
    if (!Lobby_IsValid(lobbyId)) return -1;
    if (!Player_IsAlive(playerid)) return -1;
    if (LootNode_ActiveCount(lobbyId) >= LOOT_NODE_LIMIT_PER_LOBBY) return -1;

    new Float:px, Float:py, Float:pz;
    GetPlayerPos(playerid, px, py, pz);

    new Float:spotX, Float:spotY, Float:spotZ;
    if (!LootNode_FindSpotNear(px, py, pz, spotX, spotY, spotZ)) return -1;

    new tier = LootNode_RollTier();
    new rolledItem = ITEM_NONE_ID, rolledAmount = 1;
    if (!LootNode_RollCrateLoot(tier, rolledItem, rolledAmount)) return -1;

    new slot = Loot_Spawn(spotX, spotY, spotZ, rolledItem, rolledAmount, INVALID_PLAYER_ID, lobbyId,
        LOOT_KIND_CRATE, WEAPON_CONDITION_MAX, tier);
    if (slot == -1) return -1;

    Log_Format(LOG_INFO, "[LOOT] peti dekat pemain lobby=%d anchor=%d item=%d jumlah=%d tier=%d",
        lobbyId, playerid, rolledItem, rolledAmount, tier);
    return slot;
}

stock LootNode_EnsureNearPlayer(playerid)
{
    new lobbyId = Lobby_Of(playerid);
    if (!Lobby_IsValid(lobbyId)) return 0;

    new spawned = 0;
    for (new attempt = 0; attempt < LOOT_NEAR_GUARANTEE; attempt++)
    {
        if (LootNode_SpawnNearPlayer(lobbyId, playerid) == -1) break;
        spawned++;
    }
    return spawned;
}

stock LootNode_OnPlayerSpawn(playerid)
{
    if (!Account_IsPlaying(playerid)) return 0;

    new lobbyId = Lobby_Of(playerid);
    if (!Lobby_IsValid(lobbyId)) return 0;

    for (new target = LootNode_ActiveCount(lobbyId); target < LOOT_NODE_MIN_PER_LOBBY; target++)
    {
        if (LootNode_SpawnOne(lobbyId) == -1) break;
    }

    // Dua peti pertama sengaja ditaruh dekat pemain supaya dunia tidak terlihat
    // kosong saat baru masuk.
    LootNode_EnsureNearPlayer(playerid);
    return 1;
}

public LootNode_Tick()
{
    new perfStart = GetTickCount();
    LootNode_TickBody();
    Perf_Update(PERF_LOOT, perfStart);
    return 1;
}

stock LootNode_TickBody()
{
    if (GetTickCount() - LootNodeLastTick < LOOT_NODE_INTERVAL_MS) return 0;

    for (new lobbyId = 0; lobbyId < LobbyCount; lobbyId++)
    {
        if (Lobby_CountPlayers(lobbyId, false) == 0) continue;

        // Isi dulu jatah minimum, lalu tambah satu peti per siklus.
        while (LootNode_ActiveCount(lobbyId) < LOOT_NODE_MIN_PER_LOBBY)
        {
            if (LootNode_SpawnOne(lobbyId) == -1) break;
        }
        LootNode_SpawnOne(lobbyId);

        // Satu peti tambahan di sekitar pemain supaya selalu ada loot yang bisa
        // ditemukan tanpa menyeberang peta.
        new anchor = LootNode_RandomPlayerInLobby(lobbyId);
        if (anchor != INVALID_PLAYER_ID) LootNode_SpawnNearPlayer(lobbyId, anchor);
    }

    LootNodeLastTick = GetTickCount();
    return 1;
}

/* Uji cepat dari dalam game: /lootnode menyebar 3 peti di sekitar pemain. */
stock LootNode_AdminSpawn(playerid)
{
    new lobbyId = Lobby_Of(playerid);
    new spawned = 0;

    for (new attempt = 0; attempt < 3; attempt++)
    {
        if (LootNode_SpawnOne(lobbyId) != -1) spawned++;
    }

    new message[128];
    format(message, sizeof(message), "[LOOT] %d peti baru di lobby %d (maks %d).",
        spawned, lobbyId, LOOT_NODE_LIMIT_PER_LOBBY);
    SendClientMessage(playerid, UI_ACCENT_PRIMARY, message);
    return 1;
}
