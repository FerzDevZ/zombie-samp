#if defined _ls_vehicles_included
    #endinput
#endif
#define _ls_vehicles_included

/* Satu pool untuk semua kendaraan yang dilacak server:
   - kendaraan liar (spawn sendiri di zona, bisa terkunci, bahan bakar terbatas)
   - kendaraan uji admin (/veh), bahan bakar penuh, tidak terkunci

   Bahan bakar tidak disimpan ke DB karena kendaraan liar memang ephemeral.
   Kendaraan pemain (kepemilikan permanen) belum ada di scope ini. */

#define MAX_TEST_VEHICLES 10
#define TEST_VEHICLE_DEFAULT 411

#define MAX_TRACKED_VEHICLES 64
#define WILD_MAX_PER_LOBBY 9
#define WILD_SPAWN_CHANCE 70
#define WILD_LOCKED_CHANCE 45
#define WILD_BROKEN_CHANCE 35   // mobil mati total: mesin mati, BBM nyaris habis, bodi rusak
#define WILD_MIN_PLAYER_DIST 22.0
#define WILD_NEAR_TARGET 3          // mobil yang dijamin muncul dekat pemain, bukan di zona jauh
#define WILD_NEAR_MIN_DIST 28.0
#define WILD_NEAR_MAX_DIST 85.0
#define WILD_MIN_VEHICLE_DIST 7.0
#define VEHICLE_REFUEL_RANGE 4.5
#define VEHICLE_LOCKPICK_RANGE 4.5
#define VEHICLE_FUEL_PRICE 3
/* Skala liter terhadap km. Konsumsi tabel 4,5–28 L/100 km adalah angka dunia
   nyata; tanpa skala satu tangki 80 L hanya bertahan ~7 km, jadi pemain
   mengisi BBM tiap 4 menit dan ekonomi hancur. Skala 40 memberi sedan ±15 km
   per tangki (±15 menit mengemudi jarak jauh), sesuai docs/SURVIVAL.md. */
#define VEHICLE_FUEL_GAME_SCALE 40.0
#define VEHICLE_JERRYCAN_LITERS 20.0
#define VEHICLE_TOW_RANGE 6.0

new TestVehicle[MAX_TEST_VEHICLES];
new TestVehicleCursor = 0;

enum E_TRACKED_VEHICLE
{
    tvVehicle,
    tvModel,
    tvLobby,
    Float:tvFuel,
    Float:tvTank,
    tvOwnerChar,
    tvDbId,
    tvLocked,
    tvPersistent,
    tvActive,
    tvLastWarn,
    tvSpawnTick
};

new TrackedVehicle[MAX_TRACKED_VEHICLES][E_TRACKED_VEHICLE];

new const gWildModels[] = {
    400, 401, 404, 410, 411, 412, 413, 418, 419, 422, 436, 439, 442, 458,
    463, 466, 467, 474, 475, 477, 478, 479, 482, 489, 490, 491, 492, 496,
    500, 503, 504, 507, 516, 517, 518, 525, 526, 527, 529, 533, 534, 535,
    536, 540, 542, 543, 544, 545, 546, 547, 548, 549, 550, 551, 552, 554,
    555, 556, 557, 558, 559, 560, 561, 562, 565, 566, 567, 568, 575, 576,
    579, 580, 581, 582, 583, 585, 586, 587, 589, 600, 602, 603, 604, 605, 609
};

forward WildVeh_Tick();
forward VehicleFuel_Tick();

stock bool:Vehicle_ModelAllowed(modelid)
{
    if (modelid < 400 || modelid > 611) return false;

    switch (modelid)
    {
        case 537, 538, 569, 570: return false;   // kereta & tram
    }
    return true;
}

/* Tangki & konsumsi per kategori. Angka mendekati kendaraan nyata (liter dan
   L/100 km); skala permainan diterapkan saat konsumsi supaya satu tangki
   bertahan belasan menit, bukan berjam-jam. */
stock Vehicle_TankCapacity(modelid)
{
    switch (modelid)
    {
        case 448, 461, 462, 463, 468, 471, 481, 509, 510, 521, 522, 523, 581, 586:
            return 14;

        case 403, 406, 413, 414, 422, 423, 424, 438, 455, 456, 498, 499,
             524, 525, 530, 531, 532, 543, 552, 578, 582, 583, 588, 601, 609:
            return 150;

        case 400, 404, 418, 426, 436, 439, 442, 458, 459, 467, 482, 489, 490,
             491, 492, 496, 500, 505, 506, 507, 514, 515, 540, 541, 551, 554,
             555, 560, 561, 562, 565, 566, 567, 575, 576, 579, 580, 585, 587,
             589, 600, 602, 603, 604, 605:
            return 80;

        case 402, 415, 429, 451, 477, 480, 533, 558:
            return 65;

        case 470, 516, 517, 518, 527, 529, 534, 535, 536, 546, 547, 548, 549,
             550, 559, 568:
            return 70;
    }
    return 60;
}

stock Float:Vehicle_ConsumptionPer100Km(modelid)
{
    switch (modelid)
    {
        case 448, 461, 462, 463, 468, 471, 481, 509, 510, 521, 522, 523, 581, 586:
            return 4.5;

        case 403, 406, 413, 414, 422, 423, 424, 438, 455, 456, 498, 499,
             524, 525, 530, 531, 532, 543, 552, 578, 582, 583, 588, 601, 609:
            return 28.0;

        case 400, 404, 418, 426, 436, 439, 442, 458, 459, 467, 482, 489, 490,
             491, 492, 496, 500, 505, 506, 507, 514, 515, 540, 541, 551, 554,
             555, 560, 561, 562, 565, 566, 567, 575, 576, 579, 580, 585, 587,
             589, 600, 602, 603, 604, 605:
            return 13.5;

        case 402, 415, 429, 451, 477, 480, 533, 558:
            return 19.0;

        case 470, 516, 517, 518, 527, 529, 534, 535, 536, 546, 547, 548, 549,
             550, 559, 568:
            return 12.0;
    }
    return 9.0;
}

stock Vehicle_SetEngine(vehicleid, bool:engineOn)
{
    if (vehicleid == INVALID_VEHICLE_ID || vehicleid == 0) return 0;

    new engine, lights, alarm, doors, bonnet, boot, objective;
    GetVehicleParamsEx(vehicleid, engine, lights, alarm, doors, bonnet, boot, objective);
    SetVehicleParamsEx(vehicleid, engineOn ? 1 : 0, lights, alarm, doors, bonnet, boot, objective);
    return 1;
}

stock Vehicle_TrackedSlot(vehicleid)
{
    if (vehicleid == INVALID_VEHICLE_ID || vehicleid == 0) return -1;

    for (new slot = 0; slot < MAX_TRACKED_VEHICLES; slot++)
    {
        if (!TrackedVehicle[slot][tvActive]) continue;
        if (TrackedVehicle[slot][tvVehicle] == vehicleid) return slot;
    }
    return -1;
}

stock Vehicle_Register(vehicleid, modelid, lobbyId, Float:fuel, bool:locked, bool:persistent,
    ownerChar = 0, dbId = 0)
{
    if (!Vehicle_ModelAllowed(modelid)) return -1;

    new tank = Vehicle_TankCapacity(modelid);

    for (new slot = 0; slot < MAX_TRACKED_VEHICLES; slot++)
    {
        if (TrackedVehicle[slot][tvActive]) continue;

        TrackedVehicle[slot][tvVehicle] = vehicleid;
        TrackedVehicle[slot][tvModel] = modelid;
        TrackedVehicle[slot][tvLobby] = lobbyId;
        TrackedVehicle[slot][tvTank] = float(tank);
        TrackedVehicle[slot][tvFuel] = fuel;
        if (TrackedVehicle[slot][tvFuel] > TrackedVehicle[slot][tvTank])
            TrackedVehicle[slot][tvFuel] = TrackedVehicle[slot][tvTank];
        TrackedVehicle[slot][tvOwnerChar] = ownerChar;
        TrackedVehicle[slot][tvDbId] = dbId;
        TrackedVehicle[slot][tvLocked] = locked ? 1 : 0;
        TrackedVehicle[slot][tvPersistent] = persistent ? 1 : 0;
        TrackedVehicle[slot][tvActive] = 1;
        TrackedVehicle[slot][tvLastWarn] = 0;
        TrackedVehicle[slot][tvSpawnTick] = GetTickCount();

        Vehicle_SetEngine(vehicleid, !locked);
        return slot;
    }
    return -1;
}

stock Vehicle_Unregister(vehicleid)
{
    new slot = Vehicle_TrackedSlot(vehicleid);
    if (slot < 0) return 0;

    TrackedVehicle[slot][tvActive] = 0;
    TrackedVehicle[slot][tvVehicle] = 0;
    TrackedVehicle[slot][tvFuel] = 0.0;
    return 1;
}

stock Vehicle_IsTracked(vehicleid)
{
    return Vehicle_TrackedSlot(vehicleid) >= 0;
}

stock Float:Vehicle_GetTank(vehicleid)
{
    new slot = Vehicle_TrackedSlot(vehicleid);
    if (slot < 0) return 0.0;
    return TrackedVehicle[slot][tvTank];
}

stock Vehicle_GetOwnerChar(vehicleid)
{
    new slot = Vehicle_TrackedSlot(vehicleid);
    if (slot < 0) return 0;
    return TrackedVehicle[slot][tvOwnerChar];
}

stock Vehicle_GetDbId(vehicleid)
{
    new slot = Vehicle_TrackedSlot(vehicleid);
    if (slot < 0) return 0;
    return TrackedVehicle[slot][tvDbId];
}

stock Vehicle_TrackedSlotByDbId(dbId)
{
    if (dbId <= 0) return -1;
    for (new slot = 0; slot < MAX_TRACKED_VEHICLES; slot++)
    {
        if (!TrackedVehicle[slot][tvActive]) continue;
        if (TrackedVehicle[slot][tvDbId] == dbId) return slot;
    }
    return -1;
}

stock Float:Vehicle_GetFuel(vehicleid)
{
    new slot = Vehicle_TrackedSlot(vehicleid);
    if (slot < 0) return -1.0;
    return TrackedVehicle[slot][tvFuel];
}

stock Vehicle_AddFuel(vehicleid, Float:amount)
{
    new slot = Vehicle_TrackedSlot(vehicleid);
    if (slot < 0) return 0;

    TrackedVehicle[slot][tvFuel] += amount;
    if (TrackedVehicle[slot][tvFuel] > TrackedVehicle[slot][tvTank])
        TrackedVehicle[slot][tvFuel] = TrackedVehicle[slot][tvTank];

    return 1;
}

/* Setter untuk admin/uji: menaruh tangki pada liter tertentu (dijepit 0..tank). */
stock Vehicle_SetFuel(vehicleid, Float:liters)
{
    new slot = Vehicle_TrackedSlot(vehicleid);
    if (slot < 0) return 0;

    if (liters < 0.0) liters = 0.0;
    if (liters > TrackedVehicle[slot][tvTank]) liters = TrackedVehicle[slot][tvTank];
    TrackedVehicle[slot][tvFuel] = liters;
    return 1;
}

stock Vehicle_IsLocked(vehicleid)
{
    new slot = Vehicle_TrackedSlot(vehicleid);
    if (slot < 0) return 0;
    return TrackedVehicle[slot][tvLocked];
}

stock Vehicle_SetLocked(vehicleid, bool:locked)
{
    new slot = Vehicle_TrackedSlot(vehicleid);
    if (slot < 0) return 0;

    TrackedVehicle[slot][tvLocked] = locked ? 1 : 0;
    if (locked) Vehicle_SetEngine(vehicleid, false);
    return 1;
}

stock Vehicle_NearestToPlayer(playerid, Float:radius)
{
    if (!IsPlayerConnected(playerid)) return INVALID_VEHICLE_ID;

    new Float:px, Float:py, Float:pz;
    GetPlayerPos(playerid, px, py, pz);

    new bestVehicle = INVALID_VEHICLE_ID;
    new Float:bestDistance = radius + 0.001;
    new playerLobby = Lobby_Of(playerid);

    for (new slot = 0; slot < MAX_TRACKED_VEHICLES; slot++)
    {
        if (!TrackedVehicle[slot][tvActive]) continue;
        if (TrackedVehicle[slot][tvLobby] != playerLobby) continue;

        new vehicleid = TrackedVehicle[slot][tvVehicle];
        if (GetVehicleVirtualWorld(vehicleid) != GetPlayerVirtualWorld(playerid)) continue;

        new Float:vx, Float:vy, Float:vz;
        GetVehiclePos(vehicleid, vx, vy, vz);

        new Float:distance = VectorSize(px - vx, py - vy, pz - vz);
        if (distance >= bestDistance) continue;

        bestDistance = distance;
        bestVehicle = vehicleid;
    }
    return bestVehicle;
}

/* Kendaraan uji admin: penuh, tidak terkunci, ikut pool supaya HUD bahan bakar konsisten. */
stock Vehicle_CreateForPlayer(playerid, modelid)
{
    if (!IsPlayerConnected(playerid)) return INVALID_VEHICLE_ID;
    if (!Vehicle_ModelAllowed(modelid)) return INVALID_VEHICLE_ID;

    new slot = TestVehicleCursor % MAX_TEST_VEHICLES;
    TestVehicleCursor++;

    if (TestVehicle[slot] != INVALID_VEHICLE_ID && TestVehicle[slot] != 0)
    {
        Vehicle_NotifyDestroyed(TestVehicle[slot]);
        DestroyVehicle(TestVehicle[slot]);
        TestVehicle[slot] = INVALID_VEHICLE_ID;
    }

    new Float:x, Float:y, Float:z, Float:angle;
    GetPlayerPos(playerid, x, y, z);
    GetPlayerFacingAngle(playerid, angle);

    new Float:spawnX = x + floatsin(-angle, degrees) * 4.5;
    new Float:spawnY = y + floatcos(-angle, degrees) * 4.5;

    new vehicleid = CreateVehicle(modelid, spawnX, spawnY, z + 0.4, angle, -1, -1, -1);
    if (vehicleid == INVALID_VEHICLE_ID) return INVALID_VEHICLE_ID;

    SetVehicleNumberPlate(vehicleid, "LASTSTAND");
    SetVehicleHealth(vehicleid, 1200.0);
    LinkVehicleToInterior(vehicleid, GetPlayerInterior(playerid));
    SetVehicleVirtualWorld(vehicleid, GetPlayerVirtualWorld(playerid));

    if (Vehicle_Register(vehicleid, modelid, Lobby_Of(playerid),
        float(Vehicle_TankCapacity(modelid)), false, true) < 0)
    {
        DestroyVehicle(vehicleid);
        return INVALID_VEHICLE_ID;
    }

    TestVehicle[slot] = vehicleid;
    PutPlayerInVehicle(playerid, vehicleid, 0);
    return vehicleid;
}

stock Vehicle_DestroyAll()
{
    for (new slot = 0; slot < MAX_TRACKED_VEHICLES; slot++)
    {
        if (!TrackedVehicle[slot][tvActive]) continue;

        DestroyVehicle(TrackedVehicle[slot][tvVehicle]);
        TrackedVehicle[slot][tvActive] = 0;
        TrackedVehicle[slot][tvVehicle] = 0;
        TrackedVehicle[slot][tvFuel] = 0.0;
    }

    for (new slot = 0; slot < MAX_TEST_VEHICLES; slot++)
    {
        TestVehicle[slot] = INVALID_VEHICLE_ID;
    }
    return 1;
}

stock Vehicle_NotifyDestroyed(vehicleid)
{
    Vehicle_Unregister(vehicleid);

    for (new slot = 0; slot < MAX_TEST_VEHICLES; slot++)
    {
        if (TestVehicle[slot] != vehicleid) continue;
        TestVehicle[slot] = INVALID_VEHICLE_ID;
        return 1;
    }
    return 0;
}

stock Vehicle_OnDeath(vehicleid)
{
    new tracked = Vehicle_TrackedSlot(vehicleid);
    if (tracked < 0) return 0;

    Garage_OnVehicleDestroyed(vehicleid);
    Vehicle_NotifyDestroyed(vehicleid);
    DestroyVehicle(vehicleid);
    return 1;
}

stock Vehicle_OnPlayerStateChange(playerid, newstate)
{
    if (newstate != PLAYER_STATE_DRIVER) return 0;

    new vehicleid = GetPlayerVehicleID(playerid);
    new slot = Vehicle_TrackedSlot(vehicleid);
    if (slot < 0) return 0;

    if (TrackedVehicle[slot][tvFuel] <= 0.0)
    {
        Vehicle_SetEngine(vehicleid, false);
        SendClientMessage(playerid, UI_ACCENT_WARN, "[VEH] Tangki kosong. Isi dengan /isi atau jerrycan.");
        return 1;
    }

    if (TrackedVehicle[slot][tvLocked])
    {
        if (TrackedVehicle[slot][tvOwnerChar] == ActiveCharId[playerid])
        {
            TrackedVehicle[slot][tvLocked] = 0;   // kunci pintar pemilik
        }
        else
        {
            Vehicle_SetEngine(vehicleid, false);
            SendClientMessage(playerid, UI_ACCENT_WARN, "[VEH] Terkunci. Gunakan /lockpick untuk membuka.");
            return 1;
        }
    }

    Vehicle_SetEngine(vehicleid, true);
    return 1;
}

stock Vehicle_FindWildSpot(lobbyId, &Float:x, &Float:y, &Float:z)
{
    for (new attempt = 0; attempt < 12; attempt++)
    {
        new zoneid = 1 + random(MAX_ZONE_COUNT - 1);
        if (gZones[zoneid][zoneRadius] <= 0.0) continue;
        if (gZones[zoneid][zoneIsHub]) continue;

        new Float:angle = float(random(360));
        new Float:distance = gZones[zoneid][zoneRadius] * (0.25 + float(random(50)) / 100.0);

        x = gZones[zoneid][zoneX] + floatsin(-angle, degrees) * distance;
        y = gZones[zoneid][zoneY] + floatcos(-angle, degrees) * distance;
        z = gZones[zoneid][zoneZ];

        new Float:hitX, Float:hitY, Float:hitZ;
        if (CA_RayCastLine(x, y, z + 150.0, x, y, z - 80.0, hitX, hitY, hitZ) == 0) continue;

        new landedZone = Zone_GetIdAt(hitX, hitY, hitZ);
        if (Zone_IsHub(landedZone)) continue;
        if (hitZ < -20.0 || hitZ > 120.0) continue;

        new blocked = 0;
        foreach (new playerid : PlayerPool)
        {
            if (!IsPlayerConnected(playerid)) continue;
            if (Lobby_Of(playerid) != lobbyId) continue;

            new Float:px, Float:py, Float:pz;
            GetPlayerPos(playerid, px, py, pz);
            if (VectorSize(px - hitX, py - hitY, pz - hitZ) < WILD_MIN_PLAYER_DIST)
            {
                blocked = 1;
                break;
            }
        }
        if (blocked) continue;

        for (new slot = 0; slot < MAX_TRACKED_VEHICLES; slot++)
        {
            if (!TrackedVehicle[slot][tvActive]) continue;

            new Float:vx, Float:vy, Float:vz;
            GetVehiclePos(TrackedVehicle[slot][tvVehicle], vx, vy, vz);
            if (VectorSize(vx - hitX, vy - hitY, vz - hitZ) < WILD_MIN_VEHICLE_DIST)
            {
                blocked = 1;
                break;
            }
        }
        if (blocked) continue;

        x = hitX;
        y = hitY;
        z = hitZ + 0.25;
        return 1;
    }
    return 0;
}

stock WildVeh_CountForLobby(lobbyId)
{
    new count = 0;
    for (new slot = 0; slot < MAX_TRACKED_VEHICLES; slot++)
    {
        if (!TrackedVehicle[slot][tvActive]) continue;
        if (TrackedVehicle[slot][tvPersistent]) continue;
        if (TrackedVehicle[slot][tvLobby] != lobbyId) continue;
        count++;
    }
    return count;
}

/* Cari tanah di sekitar satu titik acuan (mis. posisi pemain). Dipakai supaya
   mobil pertama selalu kelihatan dari tempat pemain berdiri, tidak hanya di
   zona yang jauh dari camp. */
stock Vehicle_FindSpotAround(lobbyId, Float:cx, Float:cy, Float:cz, Float:minDist, Float:maxDist,
    &Float:x, &Float:y, &Float:z)
{
    for (new attempt = 0; attempt < 24; attempt++)
    {
        new Float:angle = float(random(360));
        new Float:distance = minDist + (maxDist - minDist) * (float(random(100)) / 100.0);

        new Float:spotX = cx + floatsin(-angle, degrees) * distance;
        new Float:spotY = cy + floatcos(-angle, degrees) * distance;

        new Float:hitX, Float:hitY, Float:hitZ;
        if (CA_RayCastLine(spotX, spotY, cz + 120.0, spotX, spotY, cz - 90.0, hitX, hitY, hitZ) == 0) continue;
        if (hitZ < -20.0 || hitZ > 120.0) continue;

        new blocked = 0;
        foreach (new playerid : PlayerPool)
        {
            if (!IsPlayerConnected(playerid)) continue;
            if (Lobby_Of(playerid) != lobbyId) continue;

            new Float:px, Float:py, Float:pz;
            GetPlayerPos(playerid, px, py, pz);
            if (VectorSize(px - hitX, py - hitY, pz - hitZ) < 8.0)
            {
                blocked = 1;
                break;
            }
        }
        if (blocked) continue;

        for (new slot = 0; slot < MAX_TRACKED_VEHICLES; slot++)
        {
            if (!TrackedVehicle[slot][tvActive]) continue;

            new Float:vx, Float:vy, Float:vz;
            GetVehiclePos(TrackedVehicle[slot][tvVehicle], vx, vy, vz);
            if (VectorSize(vx - hitX, vy - hitY, vz - hitZ) < WILD_MIN_VEHICLE_DIST)
            {
                blocked = 1;
                break;
            }
        }
        if (blocked) continue;

        x = hitX;
        y = hitY;
        z = hitZ + 0.25;
        return 1;
    }
    return 0;
}

stock WildVeh_SpawnAt(lobbyId, Float:x, Float:y, Float:z, bool:forceUsable)
{
    new modelid = gWildModels[random(sizeof(gWildModels))];
    if (!Vehicle_ModelAllowed(modelid)) return 0;

    new vehicleid = CreateVehicle(modelid, x, y, z, float(random(360)), -1, -1, -1);
    if (vehicleid == INVALID_VEHICLE_ID) return 0;

    new Float:tank = float(Vehicle_TankCapacity(modelid));
    new Float:fuel = tank * (0.10 + float(random(36)) / 100.0);
    new bool:locked = random(100) < WILD_LOCKED_CHANCE;
    new bool:broken = random(100) < WILD_BROKEN_CHANCE;

    if (forceUsable)
    {
        // Mobil "dekat pemain" harus bisa langsung dipakai: kunci lebih jarang,
        // tidak pernah WRECK, dan tangki minimal seperempat supaya bisa jalan.
        locked = random(100) < 20;
        broken = false;
        if (fuel < tank * 0.25) fuel = tank * (0.25 + float(random(40)) / 100.0);
    }

    if (broken)
    {
        // Mobil rusak: bodi hampir habis, BBM nyaris kosong, mesin mati.
        // Pemain butuh Toolbox/Repair Kit + jerrycan atau BBM dari mobil lain.
        SetVehicleHealth(vehicleid, 240.0 + float(random(110)));
        fuel = float(random(30)) / 10.0;
        locked = random(100) < (WILD_LOCKED_CHANCE + 20);
    }

    SetVehicleVirtualWorld(vehicleid, Lobby_WorldById(lobbyId));
    LinkVehicleToInterior(vehicleid, 0);
    if (!broken) SetVehicleHealth(vehicleid, 520.0 + float(random(430)));
    SetVehicleNumberPlate(vehicleid, broken ? "WRECK" : "ABANDON");

    if (Vehicle_Register(vehicleid, modelid, lobbyId, fuel, locked, false) < 0)
    {
        DestroyVehicle(vehicleid);
        return 0;
    }

    if (broken) Vehicle_SetEngine(vehicleid, false);

    Log_Format(LOG_INFO, "[WILDVEH] lobby=%d model=%d fuel=%.1f/%.0f L locked=%d broken=%d",
        lobbyId, modelid, fuel, tank, locked ? 1 : 0, broken ? 1 : 0);
    return 1;
}

stock WildVeh_Spawn(lobbyId)
{
    new Float:x, Float:y, Float:z;
    if (!Vehicle_FindWildSpot(lobbyId, x, y, z)) return 0;
    return WildVeh_SpawnAt(lobbyId, x, y, z, false);
}

stock WildVeh_RandomPlayerInLobby(lobbyId)
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

/* Jaminan visual: minimal WILD_NEAR_TARGET mobil selalu ada dalam radius pandang
   pemain. Tanpa ini spawn acak sering jatuh di zona jauh dan pemain mengira
   server tidak punya kendaraan sama sekali. */
stock WildVeh_EnsureNearLobby(lobbyId)
{
    if (WildVeh_CountForLobby(lobbyId) >= WILD_NEAR_TARGET) return 0;

    new anchor = WildVeh_RandomPlayerInLobby(lobbyId);
    if (anchor == INVALID_PLAYER_ID) return 0;

    new Float:px, Float:py, Float:pz;
    GetPlayerPos(anchor, px, py, pz);

    new Float:x, Float:y, Float:z;
    if (!Vehicle_FindSpotAround(lobbyId, px, py, pz, WILD_NEAR_MIN_DIST, WILD_NEAR_MAX_DIST, x, y, z)) return 0;
    if (!WildVeh_SpawnAt(lobbyId, x, y, z, true)) return 0;

    Log_Format(LOG_INFO, "[WILDVEH] mobil dekat pemain lobby=%d anchor=%d", lobbyId, anchor);
    return 1;
}

/* Definisi ada di garage.inc (pemilik konstanta pom). Deklarasi maju supaya
   petunjuk "SEKITAR" bisa menyebut arah pom tanpa memindahkan konstanta. */
forward Garage_FuelCompass(playerid, dest[], size, bool:short);

/* Kode 2 huruf untuk HUD sempit: 8 arah penuh tidak muat di baris BBM. */
stock World_CompassShort(Float:deltaX, Float:deltaY, dest[], size)
{
    new Float:angle = atan2(deltaY, deltaX);
    if (angle < 0.0) angle += 360.0;

    if (angle < 22.5 || angle >= 337.5) return format(dest, size, "T");
    if (angle < 67.5) return format(dest, size, "TG");
    if (angle < 112.5) return format(dest, size, "S");
    if (angle < 157.5) return format(dest, size, "BD");
    if (angle < 202.5) return format(dest, size, "B");
    if (angle < 247.5) return format(dest, size, "BL");
    if (angle < 292.5) return format(dest, size, "U");
    return format(dest, size, "TL");
}

stock World_CompassName(Float:deltaX, Float:deltaY, dest[], size)
{
    new Float:angle = atan2(deltaY, deltaX);
    if (angle < 0.0) angle += 360.0;

    if (angle < 22.5 || angle >= 337.5) return format(dest, size, "timur");
    if (angle < 67.5) return format(dest, size, "tenggara");
    if (angle < 112.5) return format(dest, size, "selatan");
    if (angle < 157.5) return format(dest, size, "barat daya");
    if (angle < 202.5) return format(dest, size, "barat");
    if (angle < 247.5) return format(dest, size, "barat laut");
    if (angle < 292.5) return format(dest, size, "utara");
    return format(dest, size, "timur laut");
}

/* Petunjuk arah setelah spawn: pemain tidak perlu menebak apakah server punya
   mobil atau peti. Kalau belum ada, server menjamin spawn dulu lalu memberi tahu. */
stock World_NearbyHint(playerid)
{
    if (!IsPlayerConnected(playerid)) return 0;
    if (!Account_IsPlaying(playerid)) return 0;

    new Float:px, Float:py, Float:pz;
    GetPlayerPos(playerid, px, py, pz);

    new lobbyId = Lobby_Of(playerid);

    /* Dua kandidat: peti biasa terdekat dan peti langka terdekat. Peti langka
       diprioritaskan bila jaraknya tidak lebih dari 2x peti biasa — hadiah
       blueprint/vaccine lebih layak dikejar daripada peti scrap di sebelah. */
    new crateSlot = -1;
    new crateTier = LOOT_TIER_NORMAL;
    new Float:crateBest = 9999.0;

    new rareSlot = -1;
    new Float:rareBest = 9999.0;

    for (new slot = 0; slot < MAX_LOOT_DROPS; slot++)
    {
        if (!LootDrop[slot][ldActive]) continue;
        if (LootDrop[slot][ldKind] != LOOT_KIND_CRATE) continue;
        if (LootDrop[slot][ldLobby] != lobbyId) continue;

        new Float:distance = VectorSize(LootDrop[slot][ldX] - px, LootDrop[slot][ldY] - py, LootDrop[slot][ldZ] - pz);

        if (LootDrop[slot][ldTier] == LOOT_TIER_RARE)
        {
            if (distance >= rareBest) continue;
            rareBest = distance;
            rareSlot = slot;
            continue;
        }

        if (distance >= crateBest) continue;
        crateBest = distance;
        crateSlot = slot;
    }

    if (rareSlot >= 0 && (crateSlot < 0 || rareBest <= crateBest * 2.0))
    {
        crateSlot = rareSlot;
        crateBest = rareBest;
        crateTier = LOOT_TIER_RARE;
    }
    else
    {
        crateTier = LOOT_TIER_NORMAL;
    }

    new vehicleid = INVALID_VEHICLE_ID;
    new Float:vehicleBest = 9999.0;

    for (new slot = 0; slot < MAX_TRACKED_VEHICLES; slot++)
    {
        if (!TrackedVehicle[slot][tvActive]) continue;
        if (TrackedVehicle[slot][tvLobby] != lobbyId) continue;

        new Float:vx, Float:vy, Float:vz;
        GetVehiclePos(TrackedVehicle[slot][tvVehicle], vx, vy, vz);

        new Float:distance = VectorSize(vx - px, vy - py, vz - pz);
        if (distance >= vehicleBest) continue;

        vehicleBest = distance;
        vehicleid = TrackedVehicle[slot][tvVehicle];
    }

    new cratePart[64], vehiclePart[64], direction[16];

    if (crateSlot >= 0)
    {
        World_CompassName(LootDrop[crateSlot][ldX] - px, LootDrop[crateSlot][ldY] - py, direction, sizeof(direction));
        format(cratePart, sizeof(cratePart), "%s %.0f m %s",
            (crateTier == LOOT_TIER_RARE) ? "PETI LANGKA" : "Peti darurat", crateBest, direction);
    }
    else format(cratePart, sizeof(cratePart), "Peti darurat: cari di zona berbahaya");

    if (vehicleid != INVALID_VEHICLE_ID)
    {
        new Float:vx, Float:vy, Float:vz;
        GetVehiclePos(vehicleid, vx, vy, vz);
        World_CompassName(vx - px, vy - py, direction, sizeof(direction));
        format(vehiclePart, sizeof(vehiclePart), "kendaraan %.0f m %s", vehicleBest, direction);
    }
    else format(vehiclePart, sizeof(vehiclePart), "kendaraan: belum ada di sekitar");

    new line[160];
    format(line, sizeof(line), "[SEKITAR] %s | %s", cratePart, vehiclePart);
    SendClientMessage(playerid, (crateTier == LOOT_TIER_RARE) ? UI_ACCENT_WARN : UI_TEXT_MUTED, line);

    /* Petunjuk pom: tanpa ini pemain baru tahu arah pom setelah BBM habis di
       tengah zona. Warna kuning saat BBM rendah supaya terbaca. */
    new pumpPart[64], fuelLine[160];
    Garage_FuelCompass(playerid, pumpPart, sizeof(pumpPart), false);
    format(fuelLine, sizeof(fuelLine), "[SEKITAR] BBM: %s  isi $3/L dengan /isi di pom", pumpPart);
    SendClientMessage(playerid, UI_TEXT_SECONDARY, fuelLine);

    SendClientMessage(playerid, UI_TEXT_MUTED, "[SEKITAR] Ambil peti: jalan mendekat lalu /ambil   Masuk mobil: /lockpick kalau terkunci.");
    return 1;
}

public WildVeh_Tick()
{
    new perfStart = GetTickCount();
    WildVeh_TickBody();
    Perf_Update(PERF_VEHICLE, perfStart);
    return 1;
}

/* Mobil liar yang ditinggal jauh > 10 menit didespawn: tanpa ini pool 64
   tersumbat wreck terbengkalai dan garasi gagal deploy mobil baru. */
#define WILD_ABANDON_MS   600000
#define WILD_ABANDON_DIST 150.0

stock WildVeh_SweepAbandoned()
{
    new now = GetTickCount();
    for (new slot = 0; slot < MAX_TRACKED_VEHICLES; slot++)
    {
        if (!TrackedVehicle[slot][tvActive]) continue;
        if (TrackedVehicle[slot][tvPersistent]) continue;
        if (now - TrackedVehicle[slot][tvSpawnTick] < WILD_ABANDON_MS) continue;

        new vehicleid = TrackedVehicle[slot][tvVehicle];
        new Float:vx, Float:vy, Float:vz;
        GetVehiclePos(vehicleid, vx, vy, vz);

        new bool:hasDriver = false;
        new Float:nearest = 9999.0;
        foreach (new playerid : PlayerPool)
        {
            if (!IsPlayerConnected(playerid)) continue;
            /* Filter lobby DULU (audit performa F8): kendaraan hanya bisa
               dimasuki pemain di VW yang sama; skip state/posisi pemain lobby lain. */
            if (Lobby_Of(playerid) != TrackedVehicle[slot][tvLobby]) continue;
            if (GetPlayerState(playerid) == PLAYER_STATE_DRIVER && GetPlayerVehicleID(playerid) == vehicleid)
                hasDriver = true;

            new Float:px, Float:py, Float:pz;
            GetPlayerPos(playerid, px, py, pz);
            new Float:dist = VectorSize(px - vx, py - vy, pz - vz);
            if (dist < nearest) nearest = dist;
        }
        if (hasDriver || nearest < WILD_ABANDON_DIST) continue;

        new abandonedLobby = TrackedVehicle[slot][tvLobby];
        Vehicle_Unregister(vehicleid);
        DestroyVehicle(vehicleid);
        Log_Format(LOG_INFO, "[VEH] mobil liar terbengkalai didespawn (lobby %d)", abandonedLobby);
    }
    return 1;
}

stock WildVeh_TickBody()
{
    WildVeh_SweepAbandoned();

    for (new lobbyId = 0; lobbyId < LobbyCount; lobbyId++)
    {
        if (Lobby_CountPlayers(lobbyId, false) == 0) continue;

        // Prioritas pertama: isi kuota mobil dekat pemain, tanpa undian.
        WildVeh_EnsureNearLobby(lobbyId);

        if (WildVeh_CountForLobby(lobbyId) >= WILD_MAX_PER_LOBBY) continue;
        if (random(100) >= WILD_SPAWN_CHANCE) continue;

        WildVeh_Spawn(lobbyId);
    }
    return 1;
}

/* Bahan bakar dihitung dari kecepatan, bukan dari jarak state: server tidak pernah
   menerima odometer dari client. Kecepatan 0.3 unit/tick ≈ 54 km/jam. */
public VehicleFuel_Tick()
{
    new perfStart = GetTickCount();
    VehicleFuel_TickBody();
    Perf_Update(PERF_VEHICLE, perfStart);
    return 1;
}

stock VehicleFuel_TickBody()
{
    foreach (new playerid : PlayerPool)
    {
        if (!IsPlayerConnected(playerid)) continue;
        if (GetPlayerState(playerid) != PLAYER_STATE_DRIVER) continue;

        new vehicleid = GetPlayerVehicleID(playerid);
        new slot = Vehicle_TrackedSlot(vehicleid);
        if (slot < 0) continue;

        if (TrackedVehicle[slot][tvLocked])
        {
            Vehicle_SetEngine(vehicleid, false);
            if (GetTickCount() - TrackedVehicle[slot][tvLastWarn] > 8000)
            {
                TrackedVehicle[slot][tvLastWarn] = GetTickCount();
                SendClientMessage(playerid, UI_ACCENT_WARN, "[VEH] Mesin terkunci. /lockpick untuk membuka.");
            }
            continue;
        }

        if (TrackedVehicle[slot][tvFuel] <= 0.0)
        {
            Vehicle_SetEngine(vehicleid, false);
            if (GetTickCount() - TrackedVehicle[slot][tvLastWarn] > 8000)
            {
                TrackedVehicle[slot][tvLastWarn] = GetTickCount();
                SendClientMessage(playerid, UI_ACCENT_DANGER, "[VEH] Bahan bakar habis. Cari /isi atau jerrycan.");
            }
            continue;
        }

        new Float:vx, Float:vy, Float:vz;
        GetVehicleVelocity(vehicleid, vx, vy, vz);
        new Float:speed = VectorSize(vx, vy, vz);
        if (speed < 0.01) continue;

        new Float:health;
        GetVehicleHealth(vehicleid, health);
        if (health < 350.0 && random(100) < 6)
        {
            Vehicle_SetEngine(vehicleid, false);
            if (GetTickCount() - TrackedVehicle[slot][tvLastWarn] > 8000)
            {
                TrackedVehicle[slot][tvLastWarn] = GetTickCount();
                SendClientMessage(playerid, UI_ACCENT_DANGER,
                    "[VEH] Mesin mati: kondisi rusak. /perbaiki dengan Repair Kit + 3 Scrap.");
            }
            continue;
        }

        /* GetVehicleVelocity ≈ 0.01 km/h per unit; 2 s tick → km = v * 100 * 2 / 3600 */
        new Float:distanceKm = speed * 0.0556;
        new Float:consumed = distanceKm * Vehicle_ConsumptionPer100Km(TrackedVehicle[slot][tvModel])
            / 100.0 * VEHICLE_FUEL_GAME_SCALE;
        TrackedVehicle[slot][tvFuel] -= consumed;
        if (TrackedVehicle[slot][tvFuel] < 0.0) TrackedVehicle[slot][tvFuel] = 0.0;

        if (TrackedVehicle[slot][tvFuel] <= 0.0)
        {
            Vehicle_SetEngine(vehicleid, false);
            SendClientMessage(playerid, UI_ACCENT_DANGER, "[VEH] Tangki kering. Kendaraan berhenti.");
        }
    }
    return 1;
}

/* Status mesin untuk laporan BBM (HUD/pom). */
stock bool:Vehicle_IsEngineOn(vehicleid)
{
    new engine, lights, alarm, doors, bonnet, boot, objective;
    GetVehicleParamsEx(vehicleid, engine, lights, alarm, doors, bonnet, boot, objective);
    return engine == VEHICLE_PARAMS_ON;
}

/* Persentase tangki (0.0–1.0) untuk gauge HUD & warna peringatan. */
stock Float:Vehicle_FuelPercent(vehicleid)
{
    new slot = Vehicle_TrackedSlot(vehicleid);
    if (slot < 0) return 0.0;
    if (TrackedVehicle[slot][tvTank] <= 0.0) return 0.0;
    return TrackedVehicle[slot][tvFuel] / TrackedVehicle[slot][tvTank];
}

/* Jarak tempuh sisa dalam km. Konsumsi memakai rumus tick yang sama dengan
   VehicleFuel_TickBody, jadi angka di HUD bukan tebakan terpisah. */
stock Float:Vehicle_RangeKm(vehicleid)
{
    new slot = Vehicle_TrackedSlot(vehicleid);
    if (slot < 0) return 0.0;

    new Float:per100 = Vehicle_ConsumptionPer100Km(TrackedVehicle[slot][tvModel]) * VEHICLE_FUEL_GAME_SCALE;
    if (per100 < 0.01) return 0.0;
    return TrackedVehicle[slot][tvFuel] / per100 * 100.0;
}

stock Vehicle_FuelStatus(vehicleid, dest[], size)
{
    /* Satu lookup slot, bukan 3 scan O(64) per pengemudi per tick HUD
       (audit performa F7). */
    new slot = Vehicle_TrackedSlot(vehicleid);
    if (slot < 0)
    {
        format(dest, size, "FUEL --");
        return 0;
    }

    format(dest, size, "FUEL %.1f/%.0f L%s", TrackedVehicle[slot][tvFuel], TrackedVehicle[slot][tvTank],
        TrackedVehicle[slot][tvLocked] ? "  LOCKED" : "");
    return 1;
}
