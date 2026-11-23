#if defined _ls_vehicle_mods_included
    #endinput
#endif
#define _ls_vehicle_mods_included

/* Tiga modifikasi kendaraan dalam satu modul (semuanya menyentuh state
   kendaraan yang sama, dipisah per fitur di bawah):

   1. ZIRAH — pelat besi / bumper berduri untuk model off-road & truck.
      Efek: damage tabrakan ke zombie dikali bonus, kerusakan bodi diserap.
   2. BAGASI — 8 slot penyimpanan di bagasi. Ikut pindah tangan saat mobil
      dicuri, musnah saat mobil meledak, ikut tersimpan di garasi.
   3. HOTWIRE — minigame sambung kabel untuk mobil tanpa kunci. Kabel salah
      = alarm bunyi + noise besar yang memanggil zombie.

   State kendaraan memakai array paralel per slot TrackedVehicle (bukan
   memperluas enum di vehicles.inc) supaya modul ini lepas-pasang. */

#define VEHICLE_TRUNK_SLOTS         8
#define VEHICLE_PLATE_SCRAP         6
#define VEHICLE_SPIKE_SCRAP         10
#define VEHICLE_SPIKE_CHEMICAL      2
#define VEHICLE_RAM_BONUS_PLATE     2.5
#define VEHICLE_RAM_BONUS_SPIKE     3.5
#define VEHICLE_ARMOR_ABSORB        0.5
#define VEHICLE_HOTWIRE_COOLDOWN_MS 10000
#define VEHICLE_ALARM_RADIUS        80.0
#define VEHICLE_ALARM_S             8
#define VEHICLE_MOD_RANGE           4.5
#define VEHICLE_HOTWIRE_HORDE       6

/* Bobcat 422, Patriot 470, Rancher 489 — bodi berat yang masuk akal dilapis. */
new const gPlatableModels[] = { 422, 470, 489 };

#define DIALOG_VTRUNK_MAIN   2955
#define DIALOG_VTRUNK_ITEM   2956
#define DIALOG_VTRUNK_DEPOSIT 2957
#define DIALOG_VHOTWIRE      2958

enum E_VTRUNK_SLOT
{
    vtItemId,
    vtAmount,
    vtDurability
}

new VehicleTrunk[MAX_TRACKED_VEHICLES][VEHICLE_TRUNK_SLOTS][E_VTRUNK_SLOT];
new VehiclePlateLevel[MAX_TRACKED_VEHICLES];   // 0 tanpa, 1 pelat, 2 bumper duri
new VehicleHotwireAnswer[MAX_PLAYERS];
new VehicleHotwireVehicle[MAX_PLAYERS];
new VehicleHotwireCooldown[MAX_PLAYERS];
new VehicleAlarmUntil[MAX_TRACKED_VEHICLES];

new Float:VehicleHealthMemo[MAX_TRACKED_VEHICLES];

new VehicleTrunkSel[MAX_PLAYERS];

forward VehicleMods_OnDialogResponse(playerid, dialogid, response, listitem, inputtext[]);

/* ------------------------------------------------------------ state dasar */

stock VehicleMods_ResetSlot(slot)
{
    if (slot < 0 || slot >= MAX_TRACKED_VEHICLES) return 0;
    for (new i = 0; i < VEHICLE_TRUNK_SLOTS; i++)
    {
        VehicleTrunk[slot][i][vtItemId] = 0;
        VehicleTrunk[slot][i][vtAmount] = 0;
        VehicleTrunk[slot][i][vtDurability] = WEAPON_CONDITION_MAX;
    }
    VehiclePlateLevel[slot] = 0;
    VehicleHealthMemo[slot] = 1000.0;
    VehicleAlarmUntil[slot] = 0;
    return 1;
}

stock bool:VehicleMods_PlatableModel(modelid)
{
    for (new i = 0; i < sizeof(gPlatableModels); i++)
    {
        if (gPlatableModels[i] == modelid) return true;
    }
    return false;
}

stock VehicleMods_PlateLevel(vehicleid)
{
    new slot = Vehicle_TrackedSlot(vehicleid);
    if (slot < 0) return 0;
    return VehiclePlateLevel[slot];
}

/* Bonus damage tabrakan kendaraan ke zombie (dipakai damage_pipeline). */
stock Float:VehicleMods_RamMultiplier(playerid)
{
    if (!IsPlayerInAnyVehicle(playerid)) return 1.0;
    new level = VehicleMods_PlateLevel(GetPlayerVehicleID(playerid));
    if (level == 2) return VEHICLE_RAM_BONUS_SPIKE;
    if (level == 1) return VEHICLE_RAM_BONUS_PLATE;
    return 1.0;
}

/* ------------------------------------------------------------------ zirah */

stock VehicleMods_InstallPlate(playerid)
{
    new vehicleid = Vehicle_NearestToPlayer(playerid, VEHICLE_MOD_RANGE);
    if (vehicleid == INVALID_VEHICLE_ID)
    {
        SendClientMessage(playerid, UI_TEXT_MUTED, "[ZIRAH] Tidak ada kendaraan dalam 4 meter.");
        return 0;
    }

    new slot = Vehicle_TrackedSlot(vehicleid);
    if (slot < 0)
    {
        SendClientMessage(playerid, UI_TEXT_MUTED, "[ZIRAH] Kendaraan ini tidak terlacak server.");
        return 0;
    }
    new modelid = TrackedVehicle[slot][tvModel];

    if (!VehicleMods_PlatableModel(modelid))
    {
        SendClientMessage(playerid, UI_ACCENT_WARN, "[ZIRAH] Model ini tidak bisa dilapisi. Coba Bobcat/Patriot/Rancher.");
        return 0;
    }

    new level = VehiclePlateLevel[slot];
    if (level >= 2)
    {
        SendClientMessage(playerid, UI_ACCENT_WARN, "[ZIRAH] Kendaraan ini sudah berbumper duri (maksimal).");
        return 0;
    }

    if (level == 0)
    {
        if (Inventory_CountItem(playerid, ITEM_SCRAP) < VEHICLE_PLATE_SCRAP)
        {
            SendClientMessage(playerid, UI_ACCENT_WARN, "[ZIRAH] Butuh 6 scrap untuk pelat besi.");
            return 0;
        }
        Inventory_Consume(playerid, ITEM_SCRAP, VEHICLE_PLATE_SCRAP, "zirah pelat");
        VehiclePlateLevel[slot] = 1;
        SendClientMessage(playerid, UI_ACCENT_PRIMARY, "[ZIRAH] Pelat besi terpasang. Tabrakan lebih mematikan bagi zombie, bodi lebih tahan.");
    }
    else
    {
        if (Inventory_CountItem(playerid, ITEM_SCRAP) < VEHICLE_SPIKE_SCRAP
            || Inventory_CountItem(playerid, ITEM_CHEMICAL) < VEHICLE_SPIKE_CHEMICAL)
        {
            SendClientMessage(playerid, UI_ACCENT_WARN, "[ZIRAH] Butuh 10 scrap + 2 chemical untuk bumper berduri.");
            return 0;
        }
        Inventory_Consume(playerid, ITEM_SCRAP, VEHICLE_SPIKE_SCRAP, "zirah duri");
        Inventory_Consume(playerid, ITEM_CHEMICAL, VEHICLE_SPIKE_CHEMICAL, "zirah duri");
        VehiclePlateLevel[slot] = 2;
        SendClientMessage(playerid, UI_ACCENT_PRIMARY, "[ZIRAH] Bumper berduri terpasang. Menerobos kerumunan jauh lebih mematikan.");
    }

    Log_Format(LOG_INFO, "[ZIRAH] char=%d kendaraan=%d level=%d", ActiveCharId[playerid], vehicleid, VehiclePlateLevel[slot]);
    return 1;
}

/* Zirah menyerap separuh kerusakan bodi: dipanggil dari tick 2 detik. */
stock VehicleMods_Tick()
{
    for (new slot = 0; slot < MAX_TRACKED_VEHICLES; slot++)
    {
        if (!TrackedVehicle[slot][tvActive]) continue;
        if (VehiclePlateLevel[slot] == 0) continue;

        new vehicleid = TrackedVehicle[slot][tvVehicle];
        new Float:health;
        GetVehicleHealth(vehicleid, health);

        if (health < VehicleHealthMemo[slot])
        {
            new Float:absorbed = (VehicleHealthMemo[slot] - health) * VEHICLE_ARMOR_ABSORB;
            new Float:restored = health + absorbed;
            if (restored > 1000.0) restored = 1000.0;
            SetVehicleHealth(vehicleid, restored);
            VehicleHealthMemo[slot] = restored;
        }
        else
        {
            VehicleHealthMemo[slot] = health;
        }

        /* Alarm hotwire mati sendiri setelah beberapa detik. */
        if (VehicleAlarmUntil[slot] != 0 && gettime() >= VehicleAlarmUntil[slot])
        {
            new engine, lights, alarm, doors, bonnet, boot, objective;
            GetVehicleParamsEx(vehicleid, engine, lights, alarm, doors, bonnet, boot, objective);
            SetVehicleParamsEx(vehicleid, engine, lights, 0, doors, bonnet, boot, objective);
            VehicleAlarmUntil[slot] = 0;
        }
    }
    return 1;
}

/* ----------------------------------------------------------------- bagasi */

stock VehicleMods_TrunkUsed(slot)
{
    new used = 0;
    for (new i = 0; i < VEHICLE_TRUNK_SLOTS; i++)
    {
        if (Item_IsValid(VehicleTrunk[slot][i][vtItemId])) used++;
    }
    return used;
}

stock VehicleMods_TrunkAddItem(slot, itemid, amount, durability)
{
    for (new i = 0; i < VEHICLE_TRUNK_SLOTS; i++)
    {
        if (VehicleTrunk[slot][i][vtItemId] != itemid) continue;
        VehicleTrunk[slot][i][vtAmount] += amount;
        return i;
    }
    for (new i = 0; i < VEHICLE_TRUNK_SLOTS; i++)
    {
        if (Item_IsValid(VehicleTrunk[slot][i][vtItemId])) continue;
        VehicleTrunk[slot][i][vtItemId] = itemid;
        VehicleTrunk[slot][i][vtAmount] = amount;
        VehicleTrunk[slot][i][vtDurability] = durability;
        return i;
    }
    return -1;
}

stock VehicleMods_ShowTrunk(playerid)
{
    new vehicleid = Vehicle_NearestToPlayer(playerid, VEHICLE_MOD_RANGE);
    if (vehicleid == INVALID_VEHICLE_ID)
    {
        SendClientMessage(playerid, UI_TEXT_MUTED, "[BAGASI] Tidak ada kendaraan dalam 4 meter.");
        return 0;
    }

    new slot = Vehicle_TrackedSlot(vehicleid);
    if (slot < 0)
    {
        SendClientMessage(playerid, UI_TEXT_MUTED, "[BAGASI] Kendaraan ini tidak terlacak server.");
        return 0;
    }
    if (!Lock_Acquire(LOCK_KIND_VEHICLE_TRUNK, slot, playerid)) return 0;

    new body[512], line[96];
    body[0] = EOS;
    strcat(body, "Slot\tBarang\tJumlah\n", sizeof(body));

    new rows = 0;
    for (new i = 0; i < VEHICLE_TRUNK_SLOTS; i++)
    {
        new itemid = VehicleTrunk[slot][i][vtItemId];
        if (!Item_IsValid(itemid)) continue;

        new itemLabel[24];
        Item_GetName(itemid, itemLabel, sizeof(itemLabel));
        format(line, sizeof(line), "%d\t%s\tx%d\n", i + 1, itemLabel, VehicleTrunk[slot][i][vtAmount]);
        strcat(body, line, sizeof(body));
        rows++;
    }
    strcat(body, ">> Simpan item dari tas\n", sizeof(body));

    new caption[64];
    format(caption, sizeof(caption), "Bagasi kendaraan %d/%d slot", rows, VEHICLE_TRUNK_SLOTS);
    ShowPlayerDialog(playerid, DIALOG_VTRUNK_MAIN, DIALOG_STYLE_TABLIST_HEADERS, caption, body, "Pilih", "Tutup");
    return 1;
}

stock VehicleMods_TrunkSlotFromRow(slot, row)
{
    new seen = 0;
    for (new i = 0; i < VEHICLE_TRUNK_SLOTS; i++)
    {
        if (!Item_IsValid(VehicleTrunk[slot][i][vtItemId])) continue;
        if (seen == row) return i;
        seen++;
    }
    return -1;
}

stock VehicleMods_TrunkTake(playerid, slot, trunkIndex, amount)
{
    if (!Lock_IsMine(LOCK_KIND_VEHICLE_TRUNK, slot, playerid)) return 0;
    if (trunkIndex < 0 || trunkIndex >= VEHICLE_TRUNK_SLOTS) return 0;

    new itemid = VehicleTrunk[slot][trunkIndex][vtItemId];
    if (!Item_IsValid(itemid)) return 0;

    new take = amount;
    if (take <= 0 || take > VehicleTrunk[slot][trunkIndex][vtAmount]) take = VehicleTrunk[slot][trunkIndex][vtAmount];

    if (!Inventory_Grant(playerid, itemid, take, "bagasi", VehicleTrunk[slot][trunkIndex][vtDurability]))
    {
        Toast_Show(playerid, "Tas penuh", "Tidak bisa mengambil dari bagasi.", UI_ACCENT_WARN);
        return 0;
    }

    VehicleTrunk[slot][trunkIndex][vtAmount] -= take;
    if (VehicleTrunk[slot][trunkIndex][vtAmount] <= 0)
    {
        VehicleTrunk[slot][trunkIndex][vtItemId] = 0;
        VehicleTrunk[slot][trunkIndex][vtAmount] = 0;
    }

    Tx_Begin();
    Inventory_Ledger(playerid, itemid, take, "trunk_out");
    Inventory_Save(playerid);
    Tx_Commit();
    return 1;
}

stock VehicleMods_TrunkPut(playerid, slot, invSlot, amount)
{
    if (!Lock_IsMine(LOCK_KIND_VEHICLE_TRUNK, slot, playerid)) return 0;
    if (invSlot < 0 || invSlot >= MAX_INVENTORY_SLOTS) return 0;

    new itemid = PlayerInventory[playerid][invSlot][invItemId];
    if (!Item_IsValid(itemid)) return 0;
    if (itemid == ITEM_SPECIMEN)
    {
        Toast_Show(playerid, "Bagasi", "Sampel ekstraksi tidak bisa ditimbun.", UI_ACCENT_WARN);
        return 0;
    }

    new put = amount;
    if (put <= 0 || put > PlayerInventory[playerid][invSlot][invAmount]) put = PlayerInventory[playerid][invSlot][invAmount];

    new held = PlayerInventory[playerid][invSlot][invAmount];
    new durability = PlayerInventory[playerid][invSlot][invDurability];

    new trunkIndex = VehicleMods_TrunkAddItem(slot, itemid, put, durability);
    if (trunkIndex < 0)
    {
        Toast_Show(playerid, "Bagasi penuh", "Tidak ada slot bagasi tersisa.", UI_ACCENT_WARN);
        return 0;
    }

    new remaining = held - put;
    PlayerInventory[playerid][invSlot][invAmount] = remaining;
    if (remaining <= 0)
    {
        PlayerInventory[playerid][invSlot][invItemId] = ITEM_NONE_ID;
        PlayerInventory[playerid][invSlot][invDurability] = WEAPON_CONDITION_MAX;
    }
    PlayerInventoryDirty[playerid] = true;

    Tx_Begin();
    Inventory_Ledger(playerid, itemid, -put, "trunk_in");
    Inventory_Save(playerid);
    Tx_Commit();
    return 1;
}

stock VehicleMods_ShowDeposit(playerid, slot)
{
    new body[2048], line[96], rows = 0;
    body[0] = EOS;
    strcat(body, "Barang\tJumlah di tas\n", sizeof(body));

    for (new i = 0; i < MAX_INVENTORY_SLOTS; i++)
    {
        new itemid = PlayerInventory[playerid][i][invItemId];
        if (!Item_IsValid(itemid)) continue;
        if (itemid == ITEM_SPECIMEN) continue;

        new itemLabel[24];
        Item_GetName(itemid, itemLabel, sizeof(itemLabel));
        format(line, sizeof(line), "%s\tx%d\n", itemLabel, PlayerInventory[playerid][i][invAmount]);
        strcat(body, line, sizeof(body));
        rows++;
    }

    if (rows == 0)
    {
        Toast_Show(playerid, "Tas kosong", "Tidak ada item untuk disimpan.", UI_TEXT_MUTED);
        return 0;
    }

    new caption[48];
    format(caption, sizeof(caption), "Simpan ke Bagasi (kendaraan slot %d)", slot);
    ShowPlayerDialog(playerid, DIALOG_VTRUNK_DEPOSIT, DIALOG_STYLE_TABLIST_HEADERS,
        caption, body, "Pilih", "Kembali");
    return 1;
}

stock VehicleMods_DepositSlotFromRow(playerid, row)
{
    new seen = 0;
    for (new i = 0; i < MAX_INVENTORY_SLOTS; i++)
    {
        new itemid = PlayerInventory[playerid][i][invItemId];
        if (!Item_IsValid(itemid) || itemid == ITEM_SPECIMEN) continue;
        if (seen == row) return i;
        seen++;
    }
    return -1;
}

/* Meledak = isi bagasi musnah (spesifikasi fitur #29). */
stock VehicleMods_OnVehicleDeath(vehicleid)
{
    new slot = Vehicle_TrackedSlot(vehicleid);
    if (slot < 0) return 0;

    new lost = 0;
    for (new i = 0; i < VEHICLE_TRUNK_SLOTS; i++)
    {
        if (!Item_IsValid(VehicleTrunk[slot][i][vtItemId])) continue;
        lost += VehicleTrunk[slot][i][vtAmount];
        VehicleTrunk[slot][i][vtItemId] = 0;
        VehicleTrunk[slot][i][vtAmount] = 0;
    }
    if (lost > 0)
    {
        Log_Format(LOG_INFO, "[BAGASI] kendaraan=%d hancur, %d item musnah", vehicleid, lost);
    }
    return 1;
}

/* ---------------------------------------------------------------- hotwire */

stock VehicleMods_StartHotwire(playerid)
{
    new vehicleid = Vehicle_NearestToPlayer(playerid, VEHICLE_LOCKPICK_RANGE);
    if (vehicleid == INVALID_VEHICLE_ID)
    {
        SendClientMessage(playerid, UI_TEXT_MUTED, "[HOTWIRE] Tidak ada kendaraan dalam 4 meter.");
        return 0;
    }
    if (!Vehicle_IsLocked(vehicleid))
    {
        SendClientMessage(playerid, UI_TEXT_MUTED, "[HOTWIRE] Kendaraan ini tidak terkunci.");
        return 0;
    }
    if (GetTickCount() - VehicleHotwireCooldown[playerid] < VEHICLE_HOTWIRE_COOLDOWN_MS)
    {
        SendClientMessage(playerid, UI_ACCENT_WARN, "[HOTWIRE] Tanganmu masih gemetar. Tunggu sebentar.");
        return 0;
    }

    VehicleHotwireVehicle[playerid] = vehicleid;
    VehicleHotwireAnswer[playerid] = random(4);
    VehicleHotwireCooldown[playerid] = GetTickCount();

    ShowPlayerDialog(playerid, DIALOG_VHOTWIRE, DIALOG_STYLE_LIST, "Sambung kabel — pilih pasangan yang benar",
        "Kabel merah - kabel hitam\nKabel biru - kabel hitam\nKabel kuning - kabel hitam\nKabel hijau - kabel hitam",
        "Sambung", "Batal");
    return 1;
}

stock VehicleMods_HotwireResult(playerid, listitem)
{
    new vehicleid = VehicleHotwireVehicle[playerid];
    VehicleHotwireVehicle[playerid] = INVALID_VEHICLE_ID;
    if (vehicleid == INVALID_VEHICLE_ID) return 0;

    if (listitem == VehicleHotwireAnswer[playerid])
    {
        Vehicle_SetLocked(vehicleid, false);
        SendClientMessage(playerid, UI_ACCENT_PRIMARY, "[HOTWIRE] Mesin menyala. Mobil sekarang milikmu — sampai direbut orang lain.");
        Log_Format(LOG_INFO, "[HOTWIRE] char=%d kendaraan=%d berhasil", ActiveCharId[playerid], vehicleid);
        return 1;
    }

    /* Kabel salah: alarm bunyi + noise besar + horde kecil menuju mobil. */
    new engine, lights, alarm, doors, bonnet, boot, objective;
    GetVehicleParamsEx(vehicleid, engine, lights, alarm, doors, bonnet, boot, objective);
    SetVehicleParamsEx(vehicleid, engine, lights, 1, doors, bonnet, boot, objective);

    new slot = Vehicle_TrackedSlot(vehicleid);
    if (slot >= 0) VehicleAlarmUntil[slot] = gettime() + VEHICLE_ALARM_S;

    new Float:x, Float:y, Float:z;
    GetVehiclePos(vehicleid, x, y, z);
    Noise_EmitAtPosition(x, y, z, VEHICLE_ALARM_RADIUS, Lobby_Of(playerid));

    new zone = Zone_GetIdAt(x, y, z);
    if (zone >= 0 && !Zone_IsHub(zone))
    {
        Director_TriggerHorde(zone, VEHICLE_HOTWIRE_HORDE, Lobby_Of(playerid));
    }

    SendClientMessage(playerid, UI_ACCENT_DANGER, "[HOTWIRE] KABEL SALAH! Alarm bunyi — zombie mendengarnya.");
    Log_Format(LOG_INFO, "[HOTWIRE] char=%d kendaraan=%d alarm", ActiveCharId[playerid], vehicleid);
    return 0;
}

/* -------------------------------------------------------- persistensi DB */

stock VehicleMods_EncodeTrunk(slot, dest[], size)
{
    dest[0] = EOS;
    for (new i = 0; i < VEHICLE_TRUNK_SLOTS; i++)
    {
        if (!Item_IsValid(VehicleTrunk[slot][i][vtItemId])) continue;
        format(dest, size, "%s%d:%d:%d;", dest,
            VehicleTrunk[slot][i][vtItemId], VehicleTrunk[slot][i][vtAmount],
            VehicleTrunk[slot][i][vtDurability]);
    }
    return 1;
}

stock VehicleMods_ApplyPersisted(slot, plating, const trunkData[])
{
    if (slot < 0 || slot >= MAX_TRACKED_VEHICLES) return 0;
    VehiclePlateLevel[slot] = (plating > 0 && plating <= 2) ? plating : 0;

    for (new i = 0; i < VEHICLE_TRUNK_SLOTS; i++)
    {
        VehicleTrunk[slot][i][vtItemId] = 0;
        VehicleTrunk[slot][i][vtAmount] = 0;
    }

    /* Format: "itemid:amount:dur;..." — parse manual, tanpa sscanf agar
       tidak menambah dependensi di jalur panas. */
    new length = strlen(trunkData);
    new cursor = 0, trunkIndex = 0;
    while (cursor < length && trunkIndex < VEHICLE_TRUNK_SLOTS)
    {
        new itemid = 0, amount = 0, durability = WEAPON_CONDITION_MAX;
        new fields = 0, value = 0;

        while (cursor <= length)
        {
            new ch = (cursor < length) ? trunkData[cursor] : ';';
            if (ch >= '0' && ch <= '9')
            {
                value = value * 10 + (ch - '0');
            }
            else if (ch == ':' || ch == ';')
            {
                if (fields == 0) itemid = value;
                else if (fields == 1) amount = value;
                else if (fields == 2) durability = value;
                fields++;
                value = 0;
                if (ch == ';') break;
            }
            cursor++;
        }
        cursor++;

        if (fields < 2 || !Item_IsValid(itemid) || amount <= 0) continue;

        VehicleTrunk[slot][trunkIndex][vtItemId] = itemid;
        VehicleTrunk[slot][trunkIndex][vtAmount] = amount;
        VehicleTrunk[slot][trunkIndex][vtDurability] = durability;
        trunkIndex++;
    }
    return 1;
}

/* --------------------------------------------------- router dialog bagasi */

public VehicleMods_OnDialogResponse(playerid, dialogid, response, listitem, inputtext[])
{
    #pragma unused inputtext

    switch (dialogid)
    {
        case DIALOG_VTRUNK_MAIN:
        {
            new slot = Vehicle_TrackedSlotOfLock(playerid);
            if (!response)
            {
                if (slot >= 0) Lock_Release(LOCK_KIND_VEHICLE_TRUNK, slot, playerid);
                return 1;
            }
            if (slot < 0) return 1;

            new used = VehicleMods_TrunkUsed(slot);
            if (listitem >= used)
            {
                VehicleMods_ShowDeposit(playerid, slot);
                return 1;
            }
            new trunkIndex = VehicleMods_TrunkSlotFromRow(slot, listitem);
            if (trunkIndex < 0) return 1;

            VehicleTrunkSel[playerid] = trunkIndex;
            new itemLabel[24], caption[64];
            Item_GetName(VehicleTrunk[slot][trunkIndex][vtItemId], itemLabel, sizeof(itemLabel));
            format(caption, sizeof(caption), "%s x%d", itemLabel, VehicleTrunk[slot][trunkIndex][vtAmount]);
            ShowPlayerDialog(playerid, DIALOG_VTRUNK_ITEM, DIALOG_STYLE_TABLIST_HEADERS, caption,
                "Aksi\tEfek\nAmbil semua\tSeluruh tumpukan ke tas\nKembali\tTutup aksi", "Pilih", "Kembali");
            return 1;
        }
        case DIALOG_VTRUNK_ITEM:
        {
            new slot = Vehicle_TrackedSlotOfLock(playerid);
            if (!response)
            {
                if (slot >= 0) VehicleMods_ShowTrunkRefresh(playerid);
                return 1;
            }
            if (slot < 0) return 1;
            VehicleMods_TrunkTake(playerid, slot, VehicleTrunkSel[playerid], 0);
            VehicleMods_ShowTrunkRefresh(playerid);
            return 1;
        }
        case DIALOG_VTRUNK_DEPOSIT:
        {
            new slot = Vehicle_TrackedSlotOfLock(playerid);
            if (!response || slot < 0)
            {
                if (slot >= 0) VehicleMods_ShowTrunkRefresh(playerid);
                return 1;
            }
            new invSlot = VehicleMods_DepositSlotFromRow(playerid, listitem);
            if (invSlot < 0) return 1;
            VehicleMods_TrunkPut(playerid, slot, invSlot, 0);
            VehicleMods_ShowDeposit(playerid, slot);
            return 1;
        }
        case DIALOG_VHOTWIRE:
        {
            if (response) VehicleMods_HotwireResult(playerid, listitem);
            return 1;
        }
    }
    return 0;
}

stock Vehicle_TrackedSlotOfLock(playerid)
{
    /* Slot kendaraan yang sedang dikunci pemain ini di dialog bagasi.
       ContainerLock tidak punya pencarian per-pemain, jadi iterasi di sini. */
    for (new i = 0; i < LOCK_MAX; i++)
    {
        if (!ContainerLock[i][clActive]) continue;
        if (ContainerLock[i][clKind] != LOCK_KIND_VEHICLE_TRUNK) continue;
        if (ContainerLock[i][clPlayer] != playerid) continue;
        if (ContainerLock[i][clExpireTick] <= GetTickCount()) continue;
        return ContainerLock[i][clEntity];
    }
    return -1;
}

stock VehicleMods_ShowTrunkRefresh(playerid)
{
    /* Buka ulang dialog bagasi setelah ambil/simpan, tanpa rebutan lock. */
    new slot = Vehicle_TrackedSlotOfLock(playerid);
    if (slot < 0) return 0;

    new body[512], line[96];
    body[0] = EOS;
    strcat(body, "Slot\tBarang\tJumlah\n", sizeof(body));

    new rows = 0;
    for (new i = 0; i < VEHICLE_TRUNK_SLOTS; i++)
    {
        new itemid = VehicleTrunk[slot][i][vtItemId];
        if (!Item_IsValid(itemid)) continue;
        new itemLabel[24];
        Item_GetName(itemid, itemLabel, sizeof(itemLabel));
        format(line, sizeof(line), "%d\t%s\tx%d\n", i + 1, itemLabel, VehicleTrunk[slot][i][vtAmount]);
        strcat(body, line, sizeof(body));
        rows++;
    }
    strcat(body, ">> Simpan item dari tas\n", sizeof(body));

    new caption[64];
    format(caption, sizeof(caption), "Bagasi kendaraan %d/%d slot", rows, VEHICLE_TRUNK_SLOTS);
    ShowPlayerDialog(playerid, DIALOG_VTRUNK_MAIN, DIALOG_STYLE_TABLIST_HEADERS, caption, body, "Pilih", "Tutup");
    return 1;
}

stock VehicleMods_Selftest()
{
    /* Roundtrip encode/decode bagasi di slot uji terisolasi. */
    new bool:ok = true;
    new testSlot = MAX_TRACKED_VEHICLES - 1;
    VehicleMods_ResetSlot(testSlot);
    VehicleTrunk[testSlot][0][vtItemId] = ITEM_SCRAP;
    VehicleTrunk[testSlot][0][vtAmount] = 7;
    VehicleTrunk[testSlot][0][vtDurability] = 880;

    new encoded[192];
    VehicleMods_EncodeTrunk(testSlot, encoded, sizeof(encoded));

    VehicleMods_ResetSlot(testSlot);
    VehicleMods_ApplyPersisted(testSlot, 2, encoded);

    ok = ok && VehiclePlateLevel[testSlot] == 2;
    ok = ok && VehicleTrunk[testSlot][0][vtItemId] == ITEM_SCRAP;
    ok = ok && VehicleTrunk[testSlot][0][vtAmount] == 7;
    ok = ok && VehicleTrunk[testSlot][0][vtDurability] == 880;

    /* Model zirah tidak boleh kosong dan semua harus kendaraan darat. */
    ok = ok && sizeof(gPlatableModels) >= 3;

    VehicleMods_ResetSlot(testSlot);
    Selftest_Record(ok, "modifikasi kendaraan",
        ok ? "roundtrip bagasi + zirah konsisten" : "roundtrip bagasi gagal");
    return 1;
}
