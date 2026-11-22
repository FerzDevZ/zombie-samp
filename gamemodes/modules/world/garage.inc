#if defined _ls_garage_included
    #endinput
#endif
#define _ls_garage_included

/* Garasi pemain + pom bensin.

   Kendaraan milik pemain hanya ada di dunia saat dikeluarkan dari garasi.
   Bahan bakar dan kondisi disimpan ke `owned_vehicles` saat disimpan, hancur,
   atau pemain keluar dari server. Ini menjaga jumlah kendaraan di dunia tetap
   kecil dan membuat bahan bakar punya konsekuensi nyata. */

#define MAX_OWNED_VEHICLES 4
#define GARAGE_SLOT_COUNT 4
#define GARAGE_SHOP_COUNT 5

#define DIALOG_GARAGE     2971
#define DIALOG_GARAGE_BUY 2972
#define DIALOG_PUMP_MENU  2973   // menu pom bensin (mobile-safe: satu list, tanpa input teks)

#define GARAGE_X 1674.0
#define GARAGE_Y -2322.0
#define GARAGE_Z 13.5
#define GARAGE_RANGE 12.0

#define FUEL_PUMP_X 1681.0
#define FUEL_PUMP_Y -2322.0
#define FUEL_PUMP_Z 13.5
#define FUEL_PUMP_RANGE 7.0

enum E_OWNED_VEHICLE
{
    ovDbId,
    ovModel,
    Float:ovFuel,
    Float:ovHealth,
    ovLocked,
    ovPlate[16]
};

enum E_GARAGE_SHOP
{
    gsModel,
    gsPrice,
    gsName[24]
};

new GarageVehicles[MAX_PLAYERS][MAX_OWNED_VEHICLES][E_OWNED_VEHICLE];
new GarageCount[MAX_PLAYERS];
new bool:GarageLoaded[MAX_PLAYERS];
new GaragePendingModel[MAX_PLAYERS];
new GaragePendingPrice[MAX_PLAYERS];

new const gGarageShop[GARAGE_SHOP_COUNT][E_GARAGE_SHOP] = {
    {507, 420, "Esperanto bekas"},
    {560, 4500, "Sultan"},
    {422, 3200, "Bobcat"},
    {468, 1800, "Sanchez"},
    {482, 3800, "Burrito"}
};

new const Float:gGarageParkX[GARAGE_SLOT_COUNT] = {1668.0, 1672.0, 1676.0, 1680.0};
new const Float:gGarageParkY[GARAGE_SLOT_COUNT] = {-2316.0, -2316.0, -2316.0, -2316.0};

forward Garage_OnLoaded(playerid);
forward Garage_OnBought(playerid, charId);

stock Garage_CreateWorldObjects()
{
    for (new lobbyId = 0; lobbyId < LobbyCount; lobbyId++)
    {
        new worldId = Lobby_WorldById(lobbyId);
        CreateDynamicObject(1676, FUEL_PUMP_X - 1.4, FUEL_PUMP_Y, GARAGE_Z, 0.0, 0.0, 90.0, .worldid = worldId);
        CreateDynamicObject(1676, FUEL_PUMP_X + 1.4, FUEL_PUMP_Y, GARAGE_Z, 0.0, 0.0, 90.0, .worldid = worldId);
        CreateDynamicObject(970, GARAGE_X - 4.0, GARAGE_Y + 2.0, GARAGE_Z, 0.0, 0.0, 0.0, .worldid = worldId);
    }
    return 1;
}

stock Garage_Reset(playerid)
{
    GarageLoaded[playerid] = false;
    GarageCount[playerid] = 0;
    GaragePendingModel[playerid] = 0;
    GaragePendingPrice[playerid] = 0;

    for (new index = 0; index < MAX_OWNED_VEHICLES; index++)
    {
        GarageVehicles[playerid][index][ovDbId] = 0;
        GarageVehicles[playerid][index][ovModel] = 0;
        GarageVehicles[playerid][index][ovFuel] = 0.0;
        GarageVehicles[playerid][index][ovHealth] = 0.0;
        GarageVehicles[playerid][index][ovLocked] = 0;
        GarageVehicles[playerid][index][ovPlate][0] = EOS;
    }
    return 1;
}

stock Garage_VehicleName(modelid, dest[], size)
{
    for (new index = 0; index < GARAGE_SHOP_COUNT; index++)
    {
        if (gGarageShop[index][gsModel] != modelid) continue;
        format(dest, size, "%s", gGarageShop[index][gsName]);
        return 1;
    }
    format(dest, size, "Model %d", modelid);
    return 1;
}

stock bool:Garage_IsAtGarage(playerid)
{
    new Float:x, Float:y, Float:z;
    GetPlayerPos(playerid, x, y, z);
    return VectorSize(x - GARAGE_X, y - GARAGE_Y, z - GARAGE_Z) <= GARAGE_RANGE;
}

stock bool:Garage_IsAtPump(playerid)
{
    new Float:x, Float:y, Float:z;
    GetPlayerPos(playerid, x, y, z);
    return VectorSize(x - FUEL_PUMP_X, y - FUEL_PUMP_Y, z - FUEL_PUMP_Z) <= FUEL_PUMP_RANGE;
}

stock Garage_Load(playerid)
{
    Garage_Reset(playerid);
    if (!SQL_IsReady() || ActiveCharId[playerid] == 0) return 0;

    new query[192];
    mysql_format(gSQL, query, sizeof(query),
        "SELECT id, model, fuel, health, locked, plate FROM owned_vehicles \
         WHERE char_id = %d ORDER BY id ASC LIMIT %d",
        ActiveCharId[playerid], MAX_OWNED_VEHICLES);
    mysql_tquery(gSQL, query, "Garage_OnLoaded", "i", playerid);
    return 1;
}

public Garage_OnLoaded(playerid)
{
    if (!IsPlayerConnected(playerid)) return 0;

    new rows = cache_num_rows();
    GarageCount[playerid] = 0;

    for (new row = 0; row < rows; row++)
    {
        if (GarageCount[playerid] >= MAX_OWNED_VEHICLES) break;

        new index = GarageCount[playerid];
        cache_get_value_name_int(row, "id", GarageVehicles[playerid][index][ovDbId]);
        cache_get_value_name_int(row, "model", GarageVehicles[playerid][index][ovModel]);
        cache_get_value_name_float(row, "fuel", GarageVehicles[playerid][index][ovFuel]);
        cache_get_value_name_float(row, "health", GarageVehicles[playerid][index][ovHealth]);
        cache_get_value_name_int(row, "locked", GarageVehicles[playerid][index][ovLocked]);
        cache_get_value_name(row, "plate", GarageVehicles[playerid][index][ovPlate], 16);

        GarageCount[playerid]++;
    }

    GarageLoaded[playerid] = true;
    return 1;
}

stock Garage_Show(playerid)
{
    if (!Account_IsPlaying(playerid)) return 0;

    if (!Garage_IsAtGarage(playerid))
    {
        SendClientMessage(playerid, UI_TEXT_MUTED, "[GARASI] Datang ke garasi di kamp untuk membuka.");
        return 1;
    }

    if (!GarageLoaded[playerid])
    {
        SendClientMessage(playerid, UI_TEXT_MUTED, "[GARASI] Sedang memuat, coba lagi sebentar.");
        Garage_Load(playerid);
        return 1;
    }

    new body[768], line[128], vehicleName[24];
    body[0] = EOS;

    if (GarageCount[playerid] == 0)
    {
        strcat(body, "Belum punya kendaraan.\n\n", sizeof(body));
    }

    for (new index = 0; index < GarageCount[playerid]; index++)
    {
        Garage_VehicleName(GarageVehicles[playerid][index][ovModel], vehicleName, sizeof(vehicleName));

        if (Vehicle_TrackedSlotByDbId(GarageVehicles[playerid][index][ovDbId]) >= 0)
        {
            format(line, sizeof(line), "%s — TERKELUAR (pakai /simpan)\n", vehicleName);
        }
        else
        {
            format(line, sizeof(line), "Keluarkan %s — %.1f L\n", vehicleName,
                GarageVehicles[playerid][index][ovFuel]);
        }
        strcat(body, line, sizeof(body));
    }

    strcat(body, "Beli kendaraan\n", sizeof(body));
    ShowPlayerDialog(playerid, DIALOG_GARAGE, DIALOG_STYLE_LIST, "Garasi", body, "Pilih", "Tutup");
    return 1;
}

stock Garage_FindMemoryIndex(playerid, dbId)
{
    for (new index = 0; index < GarageCount[playerid]; index++)
    {
        if (GarageVehicles[playerid][index][ovDbId] == dbId) return index;
    }
    return -1;
}

stock Garage_Deploy(playerid, index)
{
    if (!GarageLoaded[playerid]) return 0;
    if (index < 0 || index >= GarageCount[playerid]) return 0;
    if (!Garage_IsAtGarage(playerid)) return 0;

    new dbId = GarageVehicles[playerid][index][ovDbId];
    if (dbId <= 0) return 0;

    if (Vehicle_TrackedSlotByDbId(dbId) >= 0)
    {
        SendClientMessage(playerid, UI_TEXT_MUTED, "[GARASI] Kendaraan itu sudah di luar. Gunakan /simpan.");
        return 0;
    }

    for (new attempt = 0; attempt < GARAGE_SLOT_COUNT; attempt++)
    {
        new spot = (index + attempt) % GARAGE_SLOT_COUNT;
        new Float:x = gGarageParkX[spot];
        new Float:y = gGarageParkY[spot];
        new Float:z = GARAGE_Z;

        new Float:hitX, Float:hitY, Float:hitZ;
        if (CA_RayCastLine(x, y, z + 40.0, x, y, z - 40.0, hitX, hitY, hitZ) != 0)
        {
            x = hitX;
            y = hitY;
            z = hitZ + 0.3;
        }

        new occupied = 0;
        for (new slot = 0; slot < MAX_TRACKED_VEHICLES; slot++)
        {
            if (!TrackedVehicle[slot][tvActive]) continue;
            new Float:vx, Float:vy, Float:vz;
            GetVehiclePos(TrackedVehicle[slot][tvVehicle], vx, vy, vz);
            if (VectorSize(vx - x, vy - y, vz - z) < 4.0)
            {
                occupied = 1;
                break;
            }
        }
        if (occupied) continue;

        new modelid = GarageVehicles[playerid][index][ovModel];
        new vehicleid = CreateVehicle(modelid, x, y, z, 90.0, -1, -1, -1);
        if (vehicleid == INVALID_VEHICLE_ID) continue;

        SetVehicleVirtualWorld(vehicleid, Lobby_WorldById(Lobby_Of(playerid)));
        LinkVehicleToInterior(vehicleid, 0);
        SetVehicleHealth(vehicleid, GarageVehicles[playerid][index][ovHealth]);
        SetVehicleNumberPlate(vehicleid, GarageVehicles[playerid][index][ovPlate]);

        new tracked = Vehicle_Register(vehicleid, modelid, Lobby_Of(playerid),
            GarageVehicles[playerid][index][ovFuel], true, true,
            ActiveCharId[playerid], dbId);
        if (tracked < 0)
        {
            DestroyVehicle(vehicleid);
            SendClientMessage(playerid, UI_ACCENT_DANGER, "[GARASI] Slot kendaraan server penuh.");
            return 0;
        }

        new line[128], vehicleName[24];
        Garage_VehicleName(modelid, vehicleName, sizeof(vehicleName));
        format(line, sizeof(line), "[GARASI] %s keluar. Bensin %.1f/%.0f L. Kunci terbuka untuk Anda.",
            vehicleName, Vehicle_GetFuel(vehicleid), Vehicle_GetTank(vehicleid));
        SendClientMessage(playerid, UI_ACCENT_PRIMARY, line);
        Log_Format(LOG_INFO, "[GARAGE] char=%d db=%d model=%d keluar", ActiveCharId[playerid], dbId, modelid);
        return 1;
    }

    SendClientMessage(playerid, UI_ACCENT_WARN, "[GARASI] Semua slot parkir terisi.");
    return 0;
}

stock Garage_SaveToDb(vehicleid)
{
    new dbId = Vehicle_GetDbId(vehicleid);
    if (dbId <= 0) return 0;

    new Float:health;
    GetVehicleHealth(vehicleid, health);
    new Float:fuel = Vehicle_GetFuel(vehicleid);
    if (fuel < 0.0) fuel = 0.0;

    new query[192];
    mysql_format(gSQL, query, sizeof(query),
        "UPDATE owned_vehicles SET fuel = %f, health = %f, locked = 1 WHERE id = %d",
        fuel, health, dbId);
    mysql_tquery(gSQL, query, "", "");
    return 1;
}

stock Garage_UpdateMemory(playerid, dbId, Float:fuel, Float:health)
{
    if (!GarageLoaded[playerid]) return 0;

    new index = Garage_FindMemoryIndex(playerid, dbId);
    if (index < 0) return 0;

    GarageVehicles[playerid][index][ovFuel] = fuel;
    GarageVehicles[playerid][index][ovHealth] = health;
    return 1;
}

stock Garage_StoreNearest(playerid)
{
    if (!Garage_IsAtGarage(playerid))
    {
        SendClientMessage(playerid, UI_TEXT_MUTED, "[GARASI] Simpan kendaraan hanya bisa di garasi kamp.");
        return 0;
    }

    new charId = ActiveCharId[playerid];
    new vehicleid = INVALID_VEHICLE_ID;

    if (GetPlayerState(playerid) == PLAYER_STATE_DRIVER)
    {
        new candidate = GetPlayerVehicleID(playerid);
        if (Vehicle_GetOwnerChar(candidate) == charId) vehicleid = candidate;
    }

    if (vehicleid == INVALID_VEHICLE_ID)
    {
        new Float:px, Float:py, Float:pz;
        GetPlayerPos(playerid, px, py, pz);
        new Float:bestDistance = 12.0;

        for (new slot = 0; slot < MAX_TRACKED_VEHICLES; slot++)
        {
            if (!TrackedVehicle[slot][tvActive]) continue;
            if (TrackedVehicle[slot][tvOwnerChar] != charId) continue;
            if (TrackedVehicle[slot][tvLobby] != Lobby_Of(playerid)) continue;

            new Float:vx, Float:vy, Float:vz;
            GetVehiclePos(TrackedVehicle[slot][tvVehicle], vx, vy, vz);
            new Float:distance = VectorSize(px - vx, py - vy, pz - vz);
            if (distance >= bestDistance) continue;

            bestDistance = distance;
            vehicleid = TrackedVehicle[slot][tvVehicle];
        }
    }

    if (vehicleid == INVALID_VEHICLE_ID)
    {
        SendClientMessage(playerid, UI_TEXT_MUTED, "[GARASI] Tidak ada kendaraan milik Anda di dekat sini.");
        return 0;
    }

    new dbId = Vehicle_GetDbId(vehicleid);
    if (dbId <= 0) return 0;

    new Float:fuel = Vehicle_GetFuel(vehicleid);
    new Float:health;
    GetVehicleHealth(vehicleid, health);
    if (fuel < 0.0) fuel = 0.0;

    new query[192];
    mysql_format(gSQL, query, sizeof(query),
        "UPDATE owned_vehicles SET fuel = %f, health = %f, locked = 1 WHERE id = %d",
        fuel, health, dbId);
    mysql_tquery(gSQL, query, "", "");

    Garage_UpdateMemory(playerid, dbId, fuel, health);
    Vehicle_NotifyDestroyed(vehicleid);
    DestroyVehicle(vehicleid);

    new line[128];
    format(line, sizeof(line), "[GARASI] Kendaraan disimpan. Bensin %.1f L, kondisi %.0f.",
        fuel, health);
    SendClientMessage(playerid, UI_ACCENT_PRIMARY, line);
    Log_Format(LOG_INFO, "[GARAGE] char=%d db=%d disimpan fuel=%.1f health=%.0f",
        charId, dbId, fuel, health);
    return 1;
}

stock Garage_OnVehicleDestroyed(vehicleid)
{
    new dbId = Vehicle_GetDbId(vehicleid);
    if (dbId <= 0) return 0;

    new ownerChar = Vehicle_GetOwnerChar(vehicleid);
    new Float:fuel = Vehicle_GetFuel(vehicleid);
    if (fuel < 0.0) fuel = 0.0;

    /* Asuransi kamp: mobil hancur tidak hilang, tetapi kembali dengan kondisi rendah. */
    new query[192];
    mysql_format(gSQL, query, sizeof(query),
        "UPDATE owned_vehicles SET fuel = %f, health = 400.0, locked = 1 WHERE id = %d",
        fuel, dbId);
    mysql_tquery(gSQL, query, "", "");

    foreach (new playerid : PlayerPool)
    {
        if (!IsPlayerConnected(playerid)) continue;
        if (ActiveCharId[playerid] != ownerChar) continue;

        Garage_UpdateMemory(playerid, dbId, fuel, 400.0);
        SendClientMessage(playerid, UI_ACCENT_WARN,
            "[GARASI] Kendaraan Anda hancur. Diasuransikan ke garasi dengan kondisi 400.");
        break;
    }
    return 1;
}

stock Garage_OnPlayerDisconnect(playerid)
{
    if (GetPlayerState(playerid) != PLAYER_STATE_DRIVER) return 0;

    new vehicleid = GetPlayerVehicleID(playerid);
    if (Vehicle_GetOwnerChar(vehicleid) != ActiveCharId[playerid]) return 0;

    Garage_SaveToDb(vehicleid);
    Vehicle_NotifyDestroyed(vehicleid);
    DestroyVehicle(vehicleid);
    return 1;
}

stock Garage_Buy(playerid, index)
{
    if (index < 0 || index >= GARAGE_SHOP_COUNT) return 0;
    if (GaragePendingModel[playerid] != 0)
    {
        SendClientMessage(playerid, UI_ACCENT_WARN, "[GARASI] Pembelian sebelumnya masih diproses. Tunggu sebentar.");
        return 0;
    }
    if (GarageCount[playerid] >= MAX_OWNED_VEHICLES)
    {
        SendClientMessage(playerid, UI_ACCENT_WARN, "[GARASI] Garasi penuh (maks 4 kendaraan).");
        return 0;
    }

    new price = gGarageShop[index][gsPrice];
    if (PlayerSurvival[playerid][psCash] < price)
    {
        new line[128];
        format(line, sizeof(line), "[GARASI] Uang tidak cukup. Harga $%d.", price);
        SendClientMessage(playerid, UI_ACCENT_DANGER, line);
        return 0;
    }

    new modelid = gGarageShop[index][gsModel];
    new Float:fuel = float(Vehicle_TankCapacity(modelid)) * 0.5;
    new plate[16];
    format(plate, sizeof(plate), "OWN%05d", ActiveCharId[playerid]);

    GaragePendingModel[playerid] = modelid;
    GaragePendingPrice[playerid] = price;

    new query[256];
    mysql_format(gSQL, query, sizeof(query),
        "INSERT INTO owned_vehicles (char_id, model, plate, fuel, health, locked, created_at) \
         VALUES (%d, %d, '%e', %f, 1000.0, 1, %d)",
        ActiveCharId[playerid], modelid, plate, fuel, gettime());
    mysql_tquery(gSQL, query, "Garage_OnBought", "ii", playerid, ActiveCharId[playerid]);
    return 1;
}

public Garage_OnBought(playerid, charId)
{
    if (!IsPlayerConnected(playerid)) return 0;
    /* Slot bisa dipakai ulang karakter lain saat query berjalan: jangan
       potong cash penghuni baru untuk kendaraan karakter lama (audit #17). */
    if (ActiveCharId[playerid] != charId)
    {
        GaragePendingModel[playerid] = 0;
        GaragePendingPrice[playerid] = 0;
        return 0;
    }

    new createdId = cache_insert_id();
    if (createdId == 0 || mysql_errno(gSQL) != 0)
    {
        SendClientMessage(playerid, UI_ACCENT_DANGER, "[GARASI] Gagal membeli kendaraan.");
        GaragePendingModel[playerid] = 0;
        GaragePendingPrice[playerid] = 0;
        return 0;
    }

    /* Uang bisa terpakai paralel (trade/vendor) selama query berjalan:
       cek ulang sebelum memotong, dan hapus baris yang sudah terlanjur INSERT. */
    if (PlayerSurvival[playerid][psCash] < GaragePendingPrice[playerid])
    {
        new rollbackQuery[96];
        format(rollbackQuery, sizeof(rollbackQuery), "DELETE FROM owned_vehicles WHERE id = %d", createdId);
        mysql_tquery(gSQL, rollbackQuery, "", "");
        SendClientMessage(playerid, UI_ACCENT_DANGER, "[GARASI] Uang berubah saat pembelian. Transaksi dibatalkan.");
        GaragePendingModel[playerid] = 0;
        GaragePendingPrice[playerid] = 0;
        return 0;
    }

    PlayerSurvival[playerid][psCash] -= GaragePendingPrice[playerid];

    new line[128], vehicleName[24];
    Garage_VehicleName(GaragePendingModel[playerid], vehicleName, sizeof(vehicleName));
    format(line, sizeof(line), "[GARASI] %s dibeli seharga $%d. Gunakan /garasi untuk mengeluarkan.",
        vehicleName, GaragePendingPrice[playerid]);
    SendClientMessage(playerid, UI_ACCENT_PRIMARY, line);

    Log_Format(LOG_INFO, "[GARAGE] char=%d beli model=%d harga=%d db=%d",
        ActiveCharId[playerid], GaragePendingModel[playerid], GaragePendingPrice[playerid], createdId);

    GaragePendingModel[playerid] = 0;
    GaragePendingPrice[playerid] = 0;
    Garage_Load(playerid);
    return 1;
}

stock Garage_ShowBuy(playerid)
{
    new body[512], line[96];
    body[0] = EOS;

    for (new index = 0; index < GARAGE_SHOP_COUNT; index++)
    {
        format(line, sizeof(line), "%s — $%d\n", gGarageShop[index][gsName], gGarageShop[index][gsPrice]);
        strcat(body, line, sizeof(body));
    }
    strcat(body, "\nKendaraan disimpan di garasi, bukan langsung muncul.", sizeof(body));
    ShowPlayerDialog(playerid, DIALOG_GARAGE_BUY, DIALOG_STYLE_LIST, "Beli kendaraan", body, "Beli", "Kembali");
    return 1;
}

stock Garage_RefuelVehicle(playerid, vehicleid, liters)
{
    if (vehicleid == INVALID_VEHICLE_ID) return 0;

    new Float:fuel = Vehicle_GetFuel(vehicleid);
    if (fuel < 0.0)
    {
        SendClientMessage(playerid, UI_TEXT_MUTED, "[BENSIN] Kendaraan ini tidak memakai bahan bakar.");
        return 0;
    }

    new Float:tank = Vehicle_GetTank(vehicleid);
    if (fuel >= tank - 0.5)
    {
        SendClientMessage(playerid, UI_TEXT_MUTED, "[BENSIN] Tangki sudah penuh.");
        return 0;
    }

    new requested = liters;
    if (requested < 1) requested = floatround(tank - fuel);

    new affordable = PlayerSurvival[playerid][psCash] / VEHICLE_FUEL_PRICE;
    if (affordable < 1)
    {
        SendClientMessage(playerid, UI_ACCENT_DANGER, "[BENSIN] Uang tidak cukup. Harga $5 per liter.");
        return 0;
    }
    if (requested > affordable) requested = affordable;

    new Float:room = tank - fuel;
    if (float(requested) > room) requested = floatround(room);

    Vehicle_AddFuel(vehicleid, float(requested));
    PlayerSurvival[playerid][psCash] -= requested * VEHICLE_FUEL_PRICE;

    new line[128];
    format(line, sizeof(line), "[BENSIN] +%d L ($%d). Tangki %.1f/%.0f L.",
        requested, requested * VEHICLE_FUEL_PRICE, Vehicle_GetFuel(vehicleid), tank);
    SendClientMessage(playerid, UI_ACCENT_PRIMARY, line);
    return 1;
}

stock Garage_PumpInteract(playerid)
{
    new vehicleid = Vehicle_NearestToPlayer(playerid, VEHICLE_REFUEL_RANGE);
    if (vehicleid == INVALID_VEHICLE_ID)
    {
        SendClientMessage(playerid, UI_TEXT_MUTED, "[POM] Tidak ada kendaraan dalam 4 meter.");
        return 0;
    }
    return Garage_RefuelVehicle(playerid, vehicleid, -1);
}

stock Garage_FindJerrycanSlot(playerid, bool:needSpace)
{
    for (new slot = 0; slot < MAX_INVENTORY_SLOTS; slot++)
    {
        if (PlayerInventory[playerid][slot][invItemId] != ITEM_JERRYCAN) continue;

        new durability = PlayerInventory[playerid][slot][invDurability];
        if (needSpace && durability >= 100) continue;
        if (!needSpace && durability <= 0) continue;
        return slot;
    }
    return -1;
}

stock bool:Garage_PourJerrycan(playerid, slot)
{
    if (slot < 0 || slot >= MAX_INVENTORY_SLOTS) return false;
    if (PlayerInventory[playerid][slot][invItemId] != ITEM_JERRYCAN) return false;

    new durability = PlayerInventory[playerid][slot][invDurability];
    if (durability <= 0)
    {
        SendClientMessage(playerid, UI_TEXT_MUTED, "[JERRYCAN] Kosong. Isi di pom dengan /isi kan.");
        return false;
    }

    new vehicleid = Vehicle_NearestToPlayer(playerid, VEHICLE_REFUEL_RANGE);
    if (vehicleid == INVALID_VEHICLE_ID)
    {
        SendClientMessage(playerid, UI_TEXT_MUTED, "[JERRYCAN] Tidak ada kendaraan dalam 4 meter.");
        return false;
    }

    new Float:fuel = Vehicle_GetFuel(vehicleid);
    if (fuel < 0.0)
    {
        SendClientMessage(playerid, UI_TEXT_MUTED, "[JERRYCAN] Kendaraan ini tidak memakai bahan bakar.");
        return false;
    }

    new Float:tank = Vehicle_GetTank(vehicleid);
    new Float:room = tank - fuel;
    if (room < 0.5)
    {
        SendClientMessage(playerid, UI_TEXT_MUTED, "[JERRYCAN] Tangki sudah penuh.");
        return false;
    }

    new Float:available = VEHICLE_JERRYCAN_LITERS * (float(durability) / 100.0);
    new Float:transfer = (available < room) ? available : room;

    Vehicle_AddFuel(vehicleid, transfer);

    new spentPercent = floatround(transfer / VEHICLE_JERRYCAN_LITERS * 100.0);
    new remaining = durability - spentPercent;
    if (remaining < 0) remaining = 0;
    PlayerInventory[playerid][slot][invDurability] = remaining;
    Inventory_Save(playerid);

    new line[128];
    format(line, sizeof(line), "[JERRYCAN] +%.1f L. Tangki %.1f/%.0f L. Sisa kaleng %d%%.",
        transfer, Vehicle_GetFuel(vehicleid), tank, remaining);
    SendClientMessage(playerid, UI_ACCENT_PRIMARY, line);
    return true;
}

stock Garage_RefillJerrycan(playerid)
{
    if (!Garage_IsAtPump(playerid))
    {
        SendClientMessage(playerid, UI_TEXT_MUTED, "[JERRYCAN] Isi kaleng hanya di pom bensin kamp.");
        return 0;
    }

    new slot = Garage_FindJerrycanSlot(playerid, true);
    if (slot < 0)
    {
        SendClientMessage(playerid, UI_ACCENT_WARN, "[JERRYCAN] Tidak ada jerrycan kosong di tas.");
        return 0;
    }

    new durability = PlayerInventory[playerid][slot][invDurability];
    new Float:room = VEHICLE_JERRYCAN_LITERS * (float(100 - durability) / 100.0);
    new requested = floatround(room);
    if (requested < 1) requested = 1;

    new affordable = PlayerSurvival[playerid][psCash] / VEHICLE_FUEL_PRICE;
    if (affordable < 1)
    {
        SendClientMessage(playerid, UI_ACCENT_DANGER, "[JERRYCAN] Uang tidak cukup. Harga $5/liter.");
        return 0;
    }
    if (requested > affordable) requested = affordable;
    if (float(requested) > room) requested = floatround(room);

    PlayerSurvival[playerid][psCash] -= requested * VEHICLE_FUEL_PRICE;
    new addedPercent = floatround(float(requested) / VEHICLE_JERRYCAN_LITERS * 100.0);
    new filled = durability + addedPercent;
    if (filled > 100) filled = 100;
    PlayerInventory[playerid][slot][invDurability] = filled;
    Inventory_Save(playerid);

    new line[128];
    format(line, sizeof(line), "[JERRYCAN] +%d L ($%d). Kaleng terisi %d%%.",
        requested, requested * VEHICLE_FUEL_PRICE, filled);
    SendClientMessage(playerid, UI_ACCENT_PRIMARY, line);
    return 1;
}

stock Garage_SiphonNearest(playerid)
{
    new vehicleid = Vehicle_NearestToPlayer(playerid, VEHICLE_REFUEL_RANGE);
    if (vehicleid == INVALID_VEHICLE_ID)
    {
        SendClientMessage(playerid, UI_TEXT_MUTED, "[SEDOT] Tidak ada kendaraan dalam 4 meter.");
        return 0;
    }

    foreach (new other : PlayerPool)
    {
        if (GetPlayerState(other) != PLAYER_STATE_DRIVER) continue;
        if (GetPlayerVehicleID(other) != vehicleid) continue;

        SendClientMessage(playerid, UI_ACCENT_WARN, "[SEDOT] Kendaraan sedang dipakai orang.");
        return 0;
    }

    new Float:fuel = Vehicle_GetFuel(vehicleid);
    if (fuel < 1.0)
    {
        SendClientMessage(playerid, UI_TEXT_MUTED, "[SEDOT] Tangki kendaraan ini hampir kosong.");
        return 0;
    }

    new slot = Garage_FindJerrycanSlot(playerid, true);
    if (slot < 0)
    {
        SendClientMessage(playerid, UI_ACCENT_WARN, "[SEDOT] Butuh jerrycan yang belum penuh.");
        return 0;
    }

    new durability = PlayerInventory[playerid][slot][invDurability];
    new Float:space = VEHICLE_JERRYCAN_LITERS * (float(100 - durability) / 100.0);
    new Float:transfer = (space < fuel) ? space : fuel;
    transfer -= 0.5;
    if (transfer < 0.5)
    {
        SendClientMessage(playerid, UI_TEXT_MUTED, "[SEDOT] Kaleng sudah penuh.");
        return 0;
    }

    Vehicle_AddFuel(vehicleid, -transfer);

    new gainedPercent = floatround(transfer / VEHICLE_JERRYCAN_LITERS * 100.0);
    new filled = durability + gainedPercent;
    if (filled > 100) filled = 100;
    PlayerInventory[playerid][slot][invDurability] = filled;
    Inventory_Save(playerid);

    new Float:px, Float:py, Float:pz;
    GetPlayerPos(playerid, px, py, pz);
    if (random(100) < 25) Horde_AddHeat(px, py, pz, 8.0);

    new line[128];
    format(line, sizeof(line), "[SEDOT] +%.1f L ke jerrycan (%d%%). Sisa tangki %.1f L.",
        transfer, filled, Vehicle_GetFuel(vehicleid));
    SendClientMessage(playerid, UI_ACCENT_PRIMARY, line);
    Log_Format(LOG_INFO, "[SIPHON] char=%d kendaraan=%d liter=%.1f",
        ActiveCharId[playerid], vehicleid, transfer);
    return 1;
}

stock Garage_RepairNearest(playerid)
{
    return Garage_RepairNearestEx(playerid, false);
}

/* fullToolbox: Toolbox memperbaiki penuh tanpa scrap dan menyalakan mesin —
   versi mahal dari Repair Kit untuk mobil rusak berat. */
stock Garage_RepairNearestEx(playerid, bool:fullToolbox)
{
    new vehicleid = Vehicle_NearestToPlayer(playerid, 5.0);
    if (vehicleid == INVALID_VEHICLE_ID)
    {
        SendClientMessage(playerid, UI_TEXT_MUTED, "[REPAIR] Tidak ada kendaraan dalam 5 meter.");
        return 0;
    }

    new Float:health;
    GetVehicleHealth(vehicleid, health);
    if (health >= 950.0 && !fullToolbox)
    {
        SendClientMessage(playerid, UI_TEXT_MUTED, "[REPAIR] Kondisi kendaraan masih baik.");
        return 0;
    }

    if (fullToolbox)
    {
        if (Inventory_CountItem(playerid, ITEM_TOOLBOX) < 1)
        {
            SendClientMessage(playerid, UI_ACCENT_WARN, "[REPAIR] Butuh Toolbox di tas.");
            return 0;
        }
        if (!Inventory_Consume(playerid, ITEM_TOOLBOX, 1, "toolbox_repair")) return 0;

        SetVehicleHealth(vehicleid, 1000.0);
        Vehicle_SetEngine(vehicleid, true);

        SendClientMessage(playerid, UI_ACCENT_PRIMARY, "[REPAIR] Toolbox terpakai. Kendaraan menyala kembali, kondisi penuh.");
        Log_Format(LOG_INFO, "[REPAIR] char=%d kendaraan=%d toolbox health=%.0f",
            ActiveCharId[playerid], vehicleid, health);
        return 1;
    }

    if (Inventory_CountItem(playerid, ITEM_REPAIR_KIT) < 1
        || Inventory_CountItem(playerid, ITEM_SCRAP) < 3)
    {
        SendClientMessage(playerid, UI_ACCENT_WARN, "[REPAIR] Butuh Repair Kit + Scrap 3.");
        return 0;
    }

    if (!Inventory_Consume(playerid, ITEM_REPAIR_KIT, 1, "repair_kit")) return 0;
    if (!Inventory_Consume(playerid, ITEM_SCRAP, 3, "repair_vehicle")) return 0;

    SetVehicleHealth(vehicleid, 1000.0);

    new line[128];
    format(line, sizeof(line), "[REPAIR] Kendaraan diperbaiki ke 1000. Kit dan 3 scrap terpakai.");
    SendClientMessage(playerid, UI_ACCENT_PRIMARY, line);
    Log_Format(LOG_INFO, "[REPAIR] char=%d kendaraan=%d health=%.0f",
        ActiveCharId[playerid], vehicleid, health);
    return 1;
}

/* ---------- Pom bensin: menu & laporan ---------- */

stock Garage_PumpMenu(playerid)
{
    if (!Garage_IsAtPump(playerid))
    {
        Garage_FuelReport(playerid);
        return 0;
    }

    new vehicleid = Vehicle_NearestToPlayer(playerid, VEHICLE_REFUEL_RANGE);
    if (vehicleid == INVALID_VEHICLE_ID)
    {
        SendClientMessage(playerid, UI_TEXT_MUTED, "[POM] Tidak ada kendaraan dalam 4 meter. Dekatkan kendaraan ke pom.");
        return 0;
    }

    new Float:fuel = Vehicle_GetFuel(vehicleid);
    if (fuel < 0.0)
    {
        SendClientMessage(playerid, UI_TEXT_MUTED, "[POM] Kendaraan ini tidak memakai bahan bakar.");
        return 0;
    }

    new Float:tank = Vehicle_GetTank(vehicleid);
    new Float:room = tank - fuel;

    new cash = PlayerSurvival[playerid][psCash];
    new affordable = cash / VEHICLE_FUEL_PRICE;
    new maxLiters = floatround(room);
    if (maxLiters > affordable) maxLiters = affordable;

    new canSlot = Garage_FindJerrycanSlot(playerid, true);

    /* Tangki penuh bukan jalan buntu: pemain tetap bisa mengisi jerrycan di
       pom. Menu hanya menyembunyikan baris isi tangki yang tidak mungkin. */
    if (maxLiters < 1 && canSlot < 0)
    {
        if (room < 1.0) SendClientMessage(playerid, UI_TEXT_MUTED, "[POM] Tangki penuh dan tidak ada jerrycan kosong di tas.");
        else SendClientMessage(playerid, UI_ACCENT_DANGER, "[POM] Uang tidak cukup. Harga $5 per liter.");
        return 0;
    }

    new body[512];
    new line[128];

    format(body, sizeof(body), "Aksi\tHarga\tKeterangan\n");

    if (maxLiters >= 1)
    {
        format(line, sizeof(line), "Isi penuh (%d L)\t$%d\tTangki jadi %.0f L\n",
            maxLiters, maxLiters * VEHICLE_FUEL_PRICE, fuel + float(maxLiters));
        strcat(body, line, sizeof(body));

        new preset[3] = {10, 25, 50};
        for (new i = 0; i < 3; i++)
        {
            if (preset[i] > maxLiters) continue;
            format(line, sizeof(line), "Isi %d L\t$%d\tSisa uang $%d\n",
                preset[i], preset[i] * VEHICLE_FUEL_PRICE, cash - preset[i] * VEHICLE_FUEL_PRICE);
            strcat(body, line, sizeof(body));
        }
    }
    else
    {
        format(line, sizeof(line), "Tangki penuh\t-\t%.0f/%.0f L\n", fuel, tank);
        strcat(body, line, sizeof(body));
    }

    if (canSlot >= 0)
    {
        format(line, sizeof(line), "Isi jerrycan\t$%d\tKaleng 20 L di tas\n", floatround(VEHICLE_JERRYCAN_LITERS) * VEHICLE_FUEL_PRICE);
        strcat(body, line, sizeof(body));
    }

    format(line, sizeof(line), "Tutup\t-\tBatal\n");
    strcat(body, line, sizeof(body));

    format(line, sizeof(line), "Pom Kamp  |  BBM %.0f/%.0f L  (%.0f%%)  |  Uang $%d",
        fuel, tank, Vehicle_FuelPercent(vehicleid) * 100.0, cash);
    ShowPlayerDialog(playerid, DIALOG_PUMP_MENU, DIALOG_STYLE_TABLIST_HEADERS,
        line, body, "Pilih", "Tutup");
    return 1;
}

stock Garage_PumpMenuSelect(playerid, listitem)
{
    new vehicleid = Vehicle_NearestToPlayer(playerid, VEHICLE_REFUEL_RANGE);
    if (vehicleid == INVALID_VEHICLE_ID)
    {
        SendClientMessage(playerid, UI_TEXT_MUTED, "[POM] Kendaraan sudah tidak di dekat pom.");
        return 0;
    }

    new Float:tank = Vehicle_GetTank(vehicleid);
    new Float:room = tank - Vehicle_GetFuel(vehicleid);
    new maxLiters = floatround(room);
    new affordable = PlayerSurvival[playerid][psCash] / VEHICLE_FUEL_PRICE;
    if (maxLiters > affordable) maxLiters = affordable;

    new canSlot = Garage_FindJerrycanSlot(playerid, true);

    /* Urutan baris HARUS sama dengan Garage_PumpMenu: isi penuh, preset yang
       muat, jerrycan, tutup. Baris info "tangki penuh" tidak melakukan apa pun. */
    new row = 0;

    if (maxLiters >= 1)
    {
        if (listitem == row)
        {
            Garage_RefuelVehicle(playerid, vehicleid, maxLiters);
            return 1;
        }
        row++;

        new preset[3] = {10, 25, 50};
        for (new i = 0; i < 3; i++)
        {
            if (preset[i] > maxLiters) continue;
            if (listitem == row)
            {
                Garage_RefuelVehicle(playerid, vehicleid, preset[i]);
                return 1;
            }
            row++;
        }
    }
    else
    {
        row++;
    }

    if (canSlot >= 0)
    {
        if (listitem == row)
        {
            Garage_RefillJerrycan(playerid);
            return 1;
        }
        row++;
    }

    /* Tutup atau indeks di luar daftar: keluarkan tanpa pesan. */
    return 1;
}

/* Status BBM yang bisa dipanggil kapan saja: di kendaraan, di dekat kendaraan,
   atau jalan kaki (menampilkan lokasi pom terdekat). */
stock Garage_FuelReport(playerid)
{
    if (!Account_IsPlaying(playerid)) return 0;

    new vehicleid = GetPlayerState(playerid) == PLAYER_STATE_DRIVER
        ? GetPlayerVehicleID(playerid)
        : Vehicle_NearestToPlayer(playerid, 12.0);

    if (vehicleid != INVALID_VEHICLE_ID && Vehicle_IsTracked(vehicleid))
    {
        new Float:fuel = Vehicle_GetFuel(vehicleid);
        new Float:tank = Vehicle_GetTank(vehicleid);
        new line[144];

        if (fuel < 0.0)
        {
            SendClientMessage(playerid, UI_TEXT_MUTED, "[BBM] Kendaraan ini tidak memakai bahan bakar.");
        }
        else
        {
            format(line, sizeof(line), "[BBM] %.1f/%.0f L (%.0f%%)  ~%.0f km  %s",
                fuel, tank, Vehicle_FuelPercent(vehicleid) * 100.0, Vehicle_RangeKm(vehicleid),
                Vehicle_IsEngineOn(vehicleid) ? "mesin hidup" : "mesin mati");
            SendClientMessage(playerid, UI_ACCENT_INFO, line);

            if (Vehicle_FuelPercent(vehicleid) <= 0.25)
            {
                SendClientMessage(playerid, UI_ACCENT_WARN, "[BBM] Rendah. /isi di pom kamp atau tuang jerrycan: /isi jerrycan.");
            }
        }
    }
    else
    {
        SendClientMessage(playerid, UI_TEXT_MUTED, "[BBM] Tidak ada kendaraan dalam 12 meter.");
    }

    new Float:px, Float:py, Float:pz;
    GetPlayerPos(playerid, px, py, pz);
    new Float:pumpDistance = VectorSize(px - FUEL_PUMP_X, py - FUEL_PUMP_Y, pz - FUEL_PUMP_Z);

    new line2[144];
    format(line2, sizeof(line2), "[BBM] Pom kamp %.0f m dari posisimu. Harga $%d/liter. Uang $%d.",
        pumpDistance, VEHICLE_FUEL_PRICE, PlayerSurvival[playerid][psCash]);
    SendClientMessage(playerid, UI_TEXT_SECONDARY, line2);

    if (pumpDistance <= FUEL_PUMP_RANGE)
    {
        SendClientMessage(playerid, UI_ACCENT_PRIMARY, "[BBM] Kamu di pom. Ketik /isi untuk membuka menu pengisian.");
    }
    return 1;
}

stock Garage_OnDialogResponse(playerid, dialogid, response, listitem)
{
    if (dialogid == DIALOG_GARAGE)
    {
        if (!response) return 1;

        if (listitem == GarageCount[playerid])
        {
            Garage_ShowBuy(playerid);
            return 1;
        }
        Garage_Deploy(playerid, listitem);
        return 1;
    }

    if (dialogid == DIALOG_GARAGE_BUY)
    {
        if (!response) return 1;
        Garage_Buy(playerid, listitem);
        return 1;
    }

    if (dialogid == DIALOG_PUMP_MENU)
    {
        if (!response) return 1;
        Garage_PumpMenuSelect(playerid, listitem);
        return 1;
    }
    return 0;
}
