#if defined _ls_inventory_persist_included
    #endinput
#endif
#define _ls_inventory_persist_included

/* Persistensi inventory & senjata.
   Inventory disimpan sebagai satu REPLACE dengan SEMUA slot (slot kosong dikirim
   item_id 0): satu query = satu transaksi, tidak ada baris hantu kalau slot dikosongkan. */

new WeaponCondition[MAX_PLAYERS][47];

forward Inventory_OnLoaded(playerid);
forward Weapons_OnLoaded(playerid);

stock Inventory_InitCondition(playerid)
{
    for (new weaponId = 0; weaponId < 47; weaponId++) WeaponCondition[playerid][weaponId] = WEAPON_CONDITION_MAX;
    return 1;
}

/* Senjata aus: satu tembakan = satu poin kondisi. Di 0 senjata rusak dan hilang. */
stock Inventory_DamageWeapon(playerid, weaponid)
{
    if (weaponid <= 0 || weaponid >= 47) return 0;
    // Senjata yang belum punya baris DB (mis. pistol spawn) mulai dari kondisi penuh.
    if (WeaponCondition[playerid][weaponid] <= 0) WeaponCondition[playerid][weaponid] = WEAPON_CONDITION_MAX;

    WeaponCondition[playerid][weaponid] -= WEAPON_CONDITION_PER_SHOT;
    if (WeaponCondition[playerid][weaponid] > 0) return 1;

    SetPlayerAmmo(playerid, weaponid, 0);
    Inventory_Ledger(playerid, 0, 0, "weapon_broken", weaponid);
    Weapons_Save(playerid);

    new line[96];
    format(line, sizeof(line), "[SENJATA] Senjata %d rusak total dan dibuang.", weaponid);
    SendClientMessage(playerid, UI_ACCENT_DANGER, line);
    return 1;
}

/* Durabilitas item (alat/blueprint): turun tiap dipakai, hilang saat habis. */
stock Inventory_DamageItem(playerid, slot, amount)
{
    if (slot < 0 || slot >= MAX_INVENTORY_SLOTS) return 0;

    new itemid = PlayerInventory[playerid][slot][invItemId];
    if (!Item_IsValid(itemid)) return 0;

    new durability = PlayerInventory[playerid][slot][invDurability] - amount;
    if (durability > 0)
    {
        PlayerInventory[playerid][slot][invDurability] = durability;
        Inventory_Save(playerid);
        return 1;
    }

    new amountHeld = PlayerInventory[playerid][slot][invAmount];
    PlayerInventory[playerid][slot][invItemId] = ITEM_NONE_ID;
    PlayerInventory[playerid][slot][invAmount] = 0;
    PlayerInventory[playerid][slot][invDurability] = WEAPON_CONDITION_MAX;
    PlayerInventoryDirty[playerid] = true;

    Inventory_Ledger(playerid, itemid, -amountHeld, "broken");
    Inventory_Save(playerid);

    new itemLabel[24], line[112];
    Item_GetName(itemid, itemLabel, sizeof(itemLabel));
    format(line, sizeof(line), "[TAS] %s rusak dan dibuang.", itemLabel);
    SendClientMessage(playerid, UI_ACCENT_WARN, line);
    return 1;
}

forward CharCreate_ApplyPending(playerid);

stock Inventory_Load(playerid)
{
    if (!SQL_IsReady() || ActiveCharId[playerid] == 0) return 0;

    Inventory_Clear(playerid);
    LoadCharGuard[playerid] = ActiveCharId[playerid];

    new query[160];
    mysql_format(gSQL, query, sizeof(query),
        "SELECT slot, item_id, amount, durability FROM inventory WHERE char_id = %d",
        ActiveCharId[playerid]);
    mysql_tquery(gSQL, query, "Inventory_OnLoaded", "i", playerid);
    return 1;
}

public Inventory_OnLoaded(playerid)
{
    if (!IsPlayerConnected(playerid)) return 0;
    /* Slot bisa sudah dihuni karakter lain saat callback tiba (F-09). */
    if (ActiveCharId[playerid] != LoadCharGuard[playerid]) return 0;

    new rows = cache_num_rows();
    for (new row = 0; row < rows; row++)
    {
        new slot, itemid, amount, durability;
        cache_get_value_name_int(row, "slot", slot);
        cache_get_value_name_int(row, "item_id", itemid);
        cache_get_value_name_int(row, "amount", amount);
        cache_get_value_name_int(row, "durability", durability);

        if (slot < 0 || slot >= MAX_INVENTORY_SLOTS) continue;
        if (!Item_IsValid(itemid) || amount <= 0) continue;

        PlayerInventory[playerid][slot][invItemId] = itemid;
        PlayerInventory[playerid][slot][invAmount] = amount;
        PlayerInventory[playerid][slot][invDurability] = durability;
    }
    PlayerInventoryDirty[playerid] = false;
    CharCreate_ApplyPending(playerid);

    /* Bukti angka untuk laporan "HUD scrap 0 padahal ada": count tas dicatat
       di log setiap kali inventory dimuat, jadi klaim bisa diverifikasi. */
    new invLine[96];
    format(invLine, sizeof(invLine), "[INV] char %d dimuat: scrap=%d sampel=%d",
        ActiveCharId[playerid],
        Inventory_CountItem(playerid, ITEM_SCRAP),
        Inventory_CountItem(playerid, ITEM_SPECIMEN));
    Log_Info(invLine);

    /* Konversi scrap legacy (kolom characters.scrap, mata uang mati) menjadi
       ITEM_SCRAP sungguhan — sekali per karakter, lalu kolom dinolkan (E1). */
    if (PendingLegacyScrap[playerid] > 0)
    {
        Inventory_Grant(playerid, ITEM_SCRAP, PendingLegacyScrap[playerid], "legacy_scrap");
        PendingLegacyScrap[playerid] = 0;
    }
    return 1;
}

stock Inventory_Save(playerid)
{
    if (!SQL_IsReady() || ActiveCharId[playerid] == 0) return 0;

    new query[2048], values[96];
    query[0] = EOS;
    strcat(query, "REPLACE INTO inventory (char_id, slot, item_id, amount, durability) VALUES ", sizeof(query));

    for (new slot = 0; slot < MAX_INVENTORY_SLOTS; slot++)
    {
        new itemid = PlayerInventory[playerid][slot][invItemId];
        new amount = PlayerInventory[playerid][slot][invAmount];

        if (!Item_IsValid(itemid)) itemid = ITEM_NONE_ID;
        if (itemid == ITEM_NONE_ID) amount = 0;

        format(values, sizeof(values), "%s(%d, %d, %d, %d, %d)",
            (slot == 0) ? "" : ", ",
            ActiveCharId[playerid], slot, itemid, amount,
            PlayerInventory[playerid][slot][invDurability]);
        strcat(query, values, sizeof(query));
    }

    Tx_Query(query);
    PlayerInventoryDirty[playerid] = false;
    return 1;
}

stock Inventory_Ledger(playerid, itemid, delta, const reason[], refId = 0)
{
    if (!SQL_IsReady() || ActiveCharId[playerid] == 0) return 0;

    new query[256], msBuf[16];
    EpochMs(msBuf, sizeof(msBuf));
    mysql_format(gSQL, query, sizeof(query),
        "INSERT INTO item_ledger (char_id, item_id, delta, reason, ref_id, created_at) VALUES (%d, %d, %d, '%e', %d, %s)",
        ActiveCharId[playerid], itemid, delta, reason, refId, msBuf);
    Tx_Query(query);
    return 1;
}

/* Satu jalur untuk setiap item yang masuk: memori + ledger + simpan.
   Ledger + save dibungkus transaksi: crash di antara keduanya tidak boleh
   meninggalkan mutasi item tanpa jejak audit (atau sebaliknya). */
stock bool:Inventory_Grant(playerid, itemid, amount, const reason[], durability = WEAPON_CONDITION_MAX)
{
    if (!Inventory_AddItem(playerid, itemid, amount, durability)) return false;

    Tx_Begin();
    Inventory_Ledger(playerid, itemid, amount, reason);
    Inventory_Save(playerid);
    Tx_Commit();
    return true;
}

stock bool:Inventory_Consume(playerid, itemid, amount, const reason[])
{
    if (amount <= 0) return false;
    if (!Inventory_RemoveItem(playerid, itemid, amount)) return false;

    Tx_Begin();
    Inventory_Ledger(playerid, itemid, -amount, reason);
    Inventory_Save(playerid);
    Tx_Commit();
    return true;
}

stock Weapons_Save(playerid)
{
    if (!SQL_IsReady() || ActiveCharId[playerid] == 0) return 0;

    new charId = ActiveCharId[playerid];

    /* Bangun VALUES dulu, baru transaksi: DELETE tanpa INSERT pengganti
       (rowCount 0) tetap sah, tapi tidak boleh meninggalkan transaksi terbuka. */
    new values[1024], row[64];
    values[0] = EOS;
    new rowCount = 0;

    for (new slotIndex = 0; slotIndex < 13; slotIndex++)
    {
        new weaponId, ammo;
        GetPlayerWeaponData(playerid, slotIndex, weaponId, ammo);
        if (weaponId == 0 || ammo <= 0) continue;

        format(row, sizeof(row), "%s(%d, %d, %d, %d)",
            (rowCount == 0) ? "" : ", ", charId, weaponId, ammo,
            WeaponCondition[playerid][weaponId]);
        strcat(values, row, sizeof(values));
        rowCount++;
    }

    new query[1200];
    Tx_Begin();
    mysql_format(gSQL, query, sizeof(query), "DELETE FROM weapons WHERE char_id = %d", charId);
    Tx_Query(query);

    if (rowCount > 0)
    {
        format(query, sizeof(query),
            "INSERT INTO weapons (char_id, weapon_id, ammo, weapon_condition) VALUES %s", values);
        Tx_Query(query);
    }
    Tx_Commit();
    return 1;
}

stock Weapons_Load(playerid)
{
    if (!SQL_IsReady() || ActiveCharId[playerid] == 0) return 0;

    new query[160];
    mysql_format(gSQL, query, sizeof(query),
        "SELECT weapon_id, ammo, weapon_condition FROM weapons WHERE char_id = %d", ActiveCharId[playerid]);
    mysql_tquery(gSQL, query, "Weapons_OnLoaded", "i", playerid);
    return 1;
}

public Weapons_OnLoaded(playerid)
{
    if (!IsPlayerConnected(playerid)) return 0;
    if (ActiveCharId[playerid] != LoadCharGuard[playerid]) return 0;

    new rows = cache_num_rows();
    for (new row = 0; row < rows; row++)
    {
        new weaponId, ammo, condition;
        cache_get_value_name_int(row, "weapon_id", weaponId);
        cache_get_value_name_int(row, "ammo", ammo);
        cache_get_value_name_int(row, "weapon_condition", condition);
        if (weaponId <= 0 || ammo <= 0 || weaponId >= 47) continue;

        WeaponCondition[playerid][weaponId] = (condition > 0) ? condition : WEAPON_CONDITION_MAX;
        GivePlayerWeapon(playerid, weaponId, ammo);
    }
    return 1;
}
