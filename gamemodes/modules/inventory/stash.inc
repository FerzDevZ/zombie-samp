#if defined _ls_stash_included
    #endinput
#endif
#define _ls_stash_included

/* Stash kamp: 60 slot per karakter, persisten di tabel `stash`.
   Sama seperti inventory: satu REPLACE berisi semua slot supaya tidak ada baris hantu. */

enum E_STASH_SLOT
{
    stashItemId,
    stashAmount,
    stashDurability
};

forward BaseBuild_StashCapacity(playerid);

new StashContents[MAX_PLAYERS][MAX_STASH_SLOTS][E_STASH_SLOT];

/* Kapasitas efektif = 60 slot dasar + 20 per crate di base (maks 120). */
stock Stash_Capacity(playerid)
{
    return BaseBuild_StashCapacity(playerid);
}
new bool:StashLoaded[MAX_PLAYERS];
new StashSelectedSlot[MAX_PLAYERS];
new StashSelectedInvSlot[MAX_PLAYERS];

#define DIALOG_STASH_MAIN    2201
#define DIALOG_STASH_ITEM    2202
#define DIALOG_STASH_DEPOSIT 2203
#define DIALOG_STASH_SIP_AMOUNT 2204

forward Stash_OnLoaded(playerid);

stock Stash_Reset(playerid)
{
    for (new slot = 0; slot < MAX_STASH_SLOTS; slot++)
    {
        StashContents[playerid][slot][stashItemId] = ITEM_NONE_ID;
        StashContents[playerid][slot][stashAmount] = 0;
        StashContents[playerid][slot][stashDurability] = WEAPON_CONDITION_MAX;
    }
    StashLoaded[playerid] = false;
    StashSelectedSlot[playerid] = -1;
    return 1;
}

stock Stash_CountUsed(playerid)
{
    new used = 0;
    /* Basis iterasi = kapasitas tampilan: item di slot >= kapasitas (mis.
       setelah crate dibongkar) tidak boleh menggeser indeks navigasi (audit #15). */
    for (new slot = 0; slot < Stash_Capacity(playerid); slot++)
    {
        if (Item_IsValid(StashContents[playerid][slot][stashItemId])) used++;
    }
    return used;
}

stock Stash_Load(playerid)
{
    if (!SQL_IsReady() || ActiveCharId[playerid] == 0) return 0;

    Stash_Reset(playerid);
    LoadCharGuard[playerid] = ActiveCharId[playerid];

    new query[160];
    mysql_format(gSQL, query, sizeof(query),
        "SELECT slot, item_id, amount, durability FROM stash WHERE char_id = %d",
        ActiveCharId[playerid]);
    mysql_tquery(gSQL, query, "Stash_OnLoaded", "i", playerid);
    return 1;
}

public Stash_OnLoaded(playerid)
{
    if (!IsPlayerConnected(playerid)) return 0;
    if (ActiveCharId[playerid] != LoadCharGuard[playerid]) return 0;

    new rows = cache_num_rows();
    for (new row = 0; row < rows; row++)
    {
        new slot, itemid, amount, durability;
        cache_get_value_name_int(row, "slot", slot);
        cache_get_value_name_int(row, "item_id", itemid);
        cache_get_value_name_int(row, "amount", amount);
        cache_get_value_name_int(row, "durability", durability);

        if (slot < 0 || slot >= MAX_STASH_SLOTS) continue;
        if (!Item_IsValid(itemid) || amount <= 0) continue;

        StashContents[playerid][slot][stashItemId] = itemid;
        StashContents[playerid][slot][stashAmount] = amount;
        StashContents[playerid][slot][stashDurability] = durability;
    }

    StashLoaded[playerid] = true;
    Stash_Show(playerid);
    return 1;
}

stock Stash_Save(playerid)
{
    if (!SQL_IsReady() || ActiveCharId[playerid] == 0) return 0;

    new query[4096], values[96];
    query[0] = EOS;
    strcat(query, "REPLACE INTO stash (char_id, slot, item_id, amount, durability) VALUES ", sizeof(query));

    for (new slot = 0; slot < MAX_STASH_SLOTS; slot++)
    {
        new itemid = StashContents[playerid][slot][stashItemId];
        new amount = StashContents[playerid][slot][stashAmount];

        if (!Item_IsValid(itemid)) itemid = ITEM_NONE_ID;
        if (itemid == ITEM_NONE_ID) amount = 0;

        format(values, sizeof(values), "%s(%d, %d, %d, %d, %d)",
            (slot == 0) ? "" : ", ",
            ActiveCharId[playerid], slot, itemid, amount,
            StashContents[playerid][slot][stashDurability]);
        strcat(query, values, sizeof(query));
    }

    Tx_Query(query);
    return 1;
}

stock Stash_Open(playerid)
{
    if (!Account_IsPlaying(playerid)) return 0;

    if (!Player_IsSafe(playerid))
    {
        Toast_Show(playerid, "Stash", "Stash hanya bisa diakses di kamp karantina.", UI_ACCENT_WARN);
        return 0;
    }

    if (!StashLoaded[playerid])
    {
        Toast_Show(playerid, "Stash", "Membuka peti...", UI_TEXT_SECONDARY);
        Stash_Load(playerid);
        return 1;
    }

    Stash_Show(playerid);
    return 1;
}

stock Float:Stash_GetWeight(playerid)
{
    new Float:total = 0.0;
    for (new slot = 0; slot < Stash_Capacity(playerid); slot++)
    {
        new itemid = StashContents[playerid][slot][stashItemId];
        if (!Item_IsValid(itemid)) continue;
        total += Item_GetWeight(itemid) * float(StashContents[playerid][slot][stashAmount]);
    }
    return total;
}

/* Batas berat stash: 8x kapasitas tas supaya pemain tidak bisa menimbun
   ratusan kg material di satu peti tanpa konsekuensi. */
stock Float:Stash_CapacityKg(playerid)
{
    return 240.0 + (float(BaseBuild_CrateCount(playerid)) * 40.0);
}

stock Stash_Compact(playerid)
{
    for (new source = 0; source < Stash_Capacity(playerid); source++)
    {
        new itemid = StashContents[playerid][source][stashItemId];
        if (!Item_IsValid(itemid)) continue;
        if (!Item_IsStackable(itemid)) continue;

        for (new target = source + 1; target < Stash_Capacity(playerid); target++)
        {
            if (StashContents[playerid][target][stashItemId] != itemid) continue;
            if (StashContents[playerid][target][stashDurability] != StashContents[playerid][source][stashDurability]) continue;

            new maxStack = Item_GetMaxStack(itemid);
            new space = maxStack - StashContents[playerid][source][stashAmount];
            if (space <= 0) break;

            new moved = StashContents[playerid][target][stashAmount];
            if (moved > space) moved = space;
            if (moved <= 0) continue;

            StashContents[playerid][source][stashAmount] += moved;
            StashContents[playerid][target][stashAmount] -= moved;

            if (StashContents[playerid][target][stashAmount] <= 0)
            {
                StashContents[playerid][target][stashItemId] = ITEM_NONE_ID;
                StashContents[playerid][target][stashAmount] = 0;
                StashContents[playerid][target][stashDurability] = WEAPON_CONDITION_MAX;
            }
        }
    }

    new writeSlot = 0;
    for (new readSlot = 0; readSlot < Stash_Capacity(playerid); readSlot++)
    {
        if (!Item_IsValid(StashContents[playerid][readSlot][stashItemId])) continue;
        if (readSlot == writeSlot) { writeSlot++; continue; }

        StashContents[playerid][writeSlot][stashItemId] = StashContents[playerid][readSlot][stashItemId];
        StashContents[playerid][writeSlot][stashAmount] = StashContents[playerid][readSlot][stashAmount];
        StashContents[playerid][writeSlot][stashDurability] = StashContents[playerid][readSlot][stashDurability];

        StashContents[playerid][readSlot][stashItemId] = ITEM_NONE_ID;
        StashContents[playerid][readSlot][stashAmount] = 0;
        StashContents[playerid][readSlot][stashDurability] = WEAPON_CONDITION_MAX;
        writeSlot++;
    }
    return 1;
}

stock Stash_Show(playerid)
{
    new body[4096], line[128];
    body[0] = EOS;

    strcat(body, "Barang\tJumlah / kondisi\n", sizeof(body));

    for (new slot = 0; slot < Stash_Capacity(playerid); slot++)
    {
        new itemid = StashContents[playerid][slot][stashItemId];
        if (!Item_IsValid(itemid)) continue;

        new itemLabel[24];
        Item_GetName(itemid, itemLabel, sizeof(itemLabel));

        if (StashContents[playerid][slot][stashDurability] < WEAPON_CONDITION_MAX)
        {
            format(line, sizeof(line), "%s x%d (%d%%)\n", itemLabel,
                StashContents[playerid][slot][stashAmount],
                StashContents[playerid][slot][stashDurability]);
        }
        else
        {
            format(line, sizeof(line), "%s x%d\n", itemLabel,
                StashContents[playerid][slot][stashAmount]);
        }
        strcat(body, line, sizeof(body));
    }

    strcat(body, ">> Simpan item dari tas\n", sizeof(body));
    strcat(body, ">> Rapikan tumpukan\n", sizeof(body));

    new caption[96];
    format(caption, sizeof(caption), "Stash Kamp %d/%d slot | %.1f/%.0f kg",
        Stash_CountUsed(playerid), Stash_Capacity(playerid),
        Stash_GetWeight(playerid), Stash_CapacityKg(playerid));
    ShowPlayerDialog(playerid, DIALOG_STASH_MAIN, DIALOG_STYLE_TABLIST_HEADERS, caption, body, "Pilih", "Tutup");
    return 1;
}

stock Stash_SlotFromRow(playerid, row)
{
    new seen = 0;
    for (new slot = 0; slot < Stash_Capacity(playerid); slot++)
    {
        if (!Item_IsValid(StashContents[playerid][slot][stashItemId])) continue;
        if (seen == row) return slot;
        seen++;
    }
    return -1;
}

stock Stash_ShowItem(playerid, slot)
{
    if (slot < 0 || slot >= Stash_Capacity(playerid)) return 0;

    new itemid = StashContents[playerid][slot][stashItemId];
    if (!Item_IsValid(itemid)) return 0;

    new itemLabel[24], caption[64];
    Item_GetName(itemid, itemLabel, sizeof(itemLabel));
    format(caption, sizeof(caption), "%s x%d", itemLabel, StashContents[playerid][slot][stashAmount]);

    StashSelectedSlot[playerid] = slot;
    ShowPlayerDialog(playerid, DIALOG_STASH_ITEM, DIALOG_STYLE_TABLIST_HEADERS, caption,
        "Aksi\tEfek\nAmbil 1\tSatu buah ke tas\nAmbil 5\tLima buah ke tas\nAmbil semua\tSeluruh tumpukan\nKembali\tTutup aksi", "Pilih", "Kembali");
    return 1;
}

stock Stash_ShowDeposit(playerid)
{
    new body[4096], line[128], rows = 0;
    body[0] = EOS;
    strcat(body, "Barang\tJumlah di tas\n", sizeof(body));

    for (new slot = 0; slot < MAX_INVENTORY_SLOTS; slot++)
    {
        new itemid = PlayerInventory[playerid][slot][invItemId];
        if (!Item_IsValid(itemid)) continue;
        if (itemid == ITEM_SPECIMEN) continue;   // sampel ekstraksi tidak bisa ditimbun

        new itemLabel[24];
        Item_GetName(itemid, itemLabel, sizeof(itemLabel));
        format(line, sizeof(line), "%s x%d\n", itemLabel, PlayerInventory[playerid][slot][invAmount]);
        strcat(body, line, sizeof(body));
        rows++;
    }

    if (rows == 0)
    {
        Toast_Show(playerid, "Tas kosong", "Tidak ada item untuk disimpan.", UI_TEXT_MUTED);
        return 0;
    }

    ShowPlayerDialog(playerid, DIALOG_STASH_DEPOSIT, DIALOG_STYLE_TABLIST_HEADERS,
        "Simpan ke Stash", body, "Pilih", "Kembali");
    return 1;
}

stock Stash_InventorySlotFromRow(playerid, row)
{
    new seen = 0;
    for (new slot = 0; slot < MAX_INVENTORY_SLOTS; slot++)
    {
        new itemid = PlayerInventory[playerid][slot][invItemId];
        if (!Item_IsValid(itemid)) continue;
        if (itemid == ITEM_SPECIMEN) continue;   // baris specimen tidak ditampilkan
        if (seen == row) return slot;
        seen++;
    }
    return -1;
}

stock bool:Stash_PlaceInStash(playerid, itemid, amount, durability)
{
    if (itemid == ITEM_SPECIMEN) return false;   // hanya bisa dibawa sampai ekstraksi

    /* Cek berat DAN kapasitas slot DULU (all-or-nothing): mutasi parsial yang
       gagal di tengah pernah membuat item ada di tas dan stash sekaligus. */
    if (Stash_GetWeight(playerid) + Item_GetWeight(itemid) * float(amount) > Stash_CapacityKg(playerid))
        return false;

    new maxStack = Item_GetMaxStack(itemid);
    if (maxStack < 1) maxStack = 1;

    new stackSpace = 0, freeSlots = 0;
    for (new slot = 0; slot < Stash_Capacity(playerid); slot++)
    {
        if (StashContents[playerid][slot][stashItemId] == itemid)
        {
            new space = maxStack - StashContents[playerid][slot][stashAmount];
            if (space > 0) stackSpace += space;
        }
        else if (!Item_IsValid(StashContents[playerid][slot][stashItemId])) freeSlots++;
    }
    if (stackSpace + freeSlots * maxStack < amount) return false;

    new remaining = amount;
    for (new slot = 0; slot < Stash_Capacity(playerid) && remaining > 0; slot++)
    {
        new stored = StashContents[playerid][slot][stashItemId];
        if (stored != itemid) continue;

        new space = maxStack - StashContents[playerid][slot][stashAmount];
        if (space <= 0) continue;

        new moved = (remaining < space) ? remaining : space;
        StashContents[playerid][slot][stashAmount] += moved;
        remaining -= moved;
    }

    for (new slot = 0; slot < Stash_Capacity(playerid) && remaining > 0; slot++)
    {
        if (Item_IsValid(StashContents[playerid][slot][stashItemId])) continue;

        new moved = (remaining < maxStack) ? remaining : maxStack;
        StashContents[playerid][slot][stashItemId] = itemid;
        StashContents[playerid][slot][stashAmount] = moved;
        StashContents[playerid][slot][stashDurability] = durability;
        remaining -= moved;
    }

    return (remaining == 0);
}

stock bool:Stash_TakeToInventory(playerid, itemid, amount, durability)
{
    /* Berat tas wajib dicek: tanpa ini withdraw bisa membuat overloadgratis. */
    if (Inventory_GetWeight(playerid) + Item_GetWeight(itemid) * float(amount) > Bag_Capacity(playerid))
        return false;

    new maxStack = Item_GetMaxStack(itemid);
    if (maxStack < 1) maxStack = 1;

    new stackSpace = 0, freeSlots = 0;
    for (new slot = 0; slot < MAX_INVENTORY_SLOTS; slot++)
    {
        if (PlayerInventory[playerid][slot][invItemId] == itemid)
        {
            new space = maxStack - PlayerInventory[playerid][slot][invAmount];
            if (space > 0) stackSpace += space;
        }
        else if (PlayerInventory[playerid][slot][invItemId] == ITEM_NONE_ID) freeSlots++;
    }
    if (stackSpace + freeSlots * maxStack < amount) return false;

    new remaining = amount;
    for (new slot = 0; slot < MAX_INVENTORY_SLOTS && remaining > 0; slot++)
    {
        if (PlayerInventory[playerid][slot][invItemId] != itemid) continue;

        new space = maxStack - PlayerInventory[playerid][slot][invAmount];
        if (space <= 0) continue;

        new moved = (remaining < space) ? remaining : space;
        PlayerInventory[playerid][slot][invAmount] += moved;
        remaining -= moved;
    }

    for (new slot = 0; slot < MAX_INVENTORY_SLOTS && remaining > 0; slot++)
    {
        if (Item_IsValid(PlayerInventory[playerid][slot][invItemId])) continue;

        new moved = (remaining < maxStack) ? remaining : maxStack;
        PlayerInventory[playerid][slot][invItemId] = itemid;
        PlayerInventory[playerid][slot][invAmount] = moved;
        PlayerInventory[playerid][slot][invDurability] = durability;
        remaining -= moved;
    }

    PlayerInventoryDirty[playerid] = true;
    Bag_OnInventoryChanged(playerid);
    return (remaining == 0);
}

stock Stash_DepositStack(playerid, invSlot)
{
    if (invSlot < 0 || invSlot >= MAX_INVENTORY_SLOTS) return 0;

    new itemid = PlayerInventory[playerid][invSlot][invItemId];
    new amount = PlayerInventory[playerid][invSlot][invAmount];
    new durability = PlayerInventory[playerid][invSlot][invDurability];
    if (!Item_IsValid(itemid) || amount <= 0) return 0;

    if (!Stash_PlaceInStash(playerid, itemid, amount, durability))
    {
        Toast_Show(playerid, "Stash penuh", "Kosongkan sedikit ruang dulu.", UI_ACCENT_WARN);
        return 0;
    }

    PlayerInventory[playerid][invSlot][invItemId] = ITEM_NONE_ID;
    PlayerInventory[playerid][invSlot][invAmount] = 0;
    PlayerInventory[playerid][invSlot][invDurability] = WEAPON_CONDITION_MAX;
    PlayerInventoryDirty[playerid] = true;

    Tx_Begin();
    Inventory_Ledger(playerid, itemid, -amount, "stash_in");
    Inventory_Save(playerid);
    Stash_Save(playerid);
    Tx_Commit();

    new itemLabel[24], line[112];
    Item_GetName(itemid, itemLabel, sizeof(itemLabel));
    format(line, sizeof(line), "[STASH] %s x%d disimpan.", itemLabel, amount);
    SendClientMessage(playerid, UI_ACCENT_PRIMARY, line);
    return 1;
}

/* Simpan sebagian tumpukan: sisa tetap di tas. Dipakai dialog "Simpan 1/5/semua". */
stock Stash_DepositPartial(playerid, invSlot, amount)
{
    if (invSlot < 0 || invSlot >= MAX_INVENTORY_SLOTS) return 0;

    new itemid = PlayerInventory[playerid][invSlot][invItemId];
    new held = PlayerInventory[playerid][invSlot][invAmount];
    new durability = PlayerInventory[playerid][invSlot][invDurability];
    if (!Item_IsValid(itemid) || held <= 0) return 0;

    if (itemid == ITEM_SPECIMEN)
    {
        SendClientMessage(playerid, UI_ACCENT_WARN, "[STASH] Sampel ekstraksi tidak bisa ditimbun. Bawa ke titik ekstraksi.");
        return 0;
    }

    if (amount <= 0 || amount > held) amount = held;

    new Float:addedWeight = Item_GetWeight(itemid) * float(amount);
    if (Stash_GetWeight(playerid) + addedWeight > Stash_CapacityKg(playerid))
    {
        Toast_Show(playerid, "Stash kepenuhan", "Batas berat stash tercapai. Rapikan atau jual dulu.", UI_ACCENT_WARN);
        return 0;
    }

    if (!Stash_PlaceInStash(playerid, itemid, amount, durability))
    {
        Toast_Show(playerid, "Stash penuh", "Kosongkan sedikit ruang dulu.", UI_ACCENT_WARN);
        return 0;
    }

    new remaining = held - amount;
    PlayerInventory[playerid][invSlot][invAmount] = remaining;
    if (remaining <= 0)
    {
        PlayerInventory[playerid][invSlot][invItemId] = ITEM_NONE_ID;
        PlayerInventory[playerid][invSlot][invDurability] = WEAPON_CONDITION_MAX;
    }
    PlayerInventoryDirty[playerid] = true;

    Tx_Begin();
    Inventory_Ledger(playerid, itemid, -amount, "stash_in");
    Inventory_Save(playerid);
    Stash_Save(playerid);
    Tx_Commit();

    new itemLabel[24], line[112];
    Item_GetName(itemid, itemLabel, sizeof(itemLabel));
    format(line, sizeof(line), "[STASH] %s x%d disimpan.", itemLabel, amount);
    SendClientMessage(playerid, UI_ACCENT_PRIMARY, line);
    return 1;
}

stock Stash_ShowDepositAmount(playerid, invSlot)
{
    if (invSlot < 0 || invSlot >= MAX_INVENTORY_SLOTS) return 0;

    new itemid = PlayerInventory[playerid][invSlot][invItemId];
    new held = PlayerInventory[playerid][invSlot][invAmount];
    if (!Item_IsValid(itemid) || held <= 0) return 0;

    new itemLabel[24], caption[64];
    Item_GetName(itemid, itemLabel, sizeof(itemLabel));
    format(caption, sizeof(caption), "%s x%d", itemLabel, held);

    StashSelectedInvSlot[playerid] = invSlot;
    ShowPlayerDialog(playerid, DIALOG_STASH_SIP_AMOUNT, DIALOG_STYLE_TABLIST_HEADERS, caption,
        "Aksi\tEfek\nSimpan 1\tSatu buah ke stash\nSimpan 5\tLima buah ke stash\nSimpan semua\tSeluruh tumpukan\nKembali\tBatal", "Pilih", "Kembali");
    return 1;
}

stock Stash_Withdraw(playerid, stashSlot, amount)
{
    if (stashSlot < 0 || stashSlot >= MAX_STASH_SLOTS) return 0;

    new itemid = StashContents[playerid][stashSlot][stashItemId];
    new held = StashContents[playerid][stashSlot][stashAmount];
    new durability = StashContents[playerid][stashSlot][stashDurability];
    if (!Item_IsValid(itemid) || held <= 0) return 0;

    if (amount <= 0 || amount > held) amount = held;

    if (!Stash_TakeToInventory(playerid, itemid, amount, durability))
    {
        Toast_Show(playerid, "Tas penuh", "Buang sesuatu sebelum mengambil lagi.", UI_ACCENT_WARN);
        return 0;
    }

    StashContents[playerid][stashSlot][stashAmount] = held - amount;
    if (StashContents[playerid][stashSlot][stashAmount] <= 0)
    {
        StashContents[playerid][stashSlot][stashItemId] = ITEM_NONE_ID;
        StashContents[playerid][stashSlot][stashDurability] = WEAPON_CONDITION_MAX;
    }

    Tx_Begin();
    Inventory_Ledger(playerid, itemid, amount, "stash_out");
    Inventory_Save(playerid);
    Stash_Save(playerid);
    Tx_Commit();

    new itemLabel[24], line[112];
    Item_GetName(itemid, itemLabel, sizeof(itemLabel));
    format(line, sizeof(line), "[STASH] %s x%d diambil.", itemLabel, amount);
    SendClientMessage(playerid, UI_ACCENT_PRIMARY, line);
    return 1;
}

stock Stash_OnDialogResponse(playerid, dialogid, response, listitem, const inputtext[])
{
    #pragma unused inputtext

    /* Validasi ulang lokasi/state saat RESPONS (audit F-13): cek hanya saat
       buka dialog bisa dilewati dengan berjalan ke zona bahaya lalu menjawab
       dialog lama (atau spoof). Tutup dialog (response 0) tetap diproses
       supaya lock dilepas. */
    if (response && (!Account_IsPlaying(playerid) || !Zone_IsHub(Player_GetZone(playerid)))) return 0;

    switch (dialogid)
    {
        case DIALOG_STASH_MAIN:
        {
            if (!response) return 1;

            // Baris navigasi ditaruh di akhir supaya indeks item tidak bergeser.
            new used = Stash_CountUsed(playerid);
            if (listitem == used)
            {
                Stash_ShowDeposit(playerid);
                return 1;
            }
            if (listitem == used + 1)
            {
                Stash_Compact(playerid);
                Stash_Save(playerid);
                SendClientMessage(playerid, UI_ACCENT_PRIMARY, "[STASH] Tumpukan dirapikan.");
                Stash_Show(playerid);
                return 1;
            }

            new slot = Stash_SlotFromRow(playerid, listitem);
            if (slot != -1) Stash_ShowItem(playerid, slot);
            return 1;
        }
        case DIALOG_STASH_ITEM:
        {
            if (!response)
            {
                Stash_Show(playerid);
                return 1;
            }

            new slot = StashSelectedSlot[playerid];

            switch (listitem)
            {
                case 0: Stash_Withdraw(playerid, slot, 1);
                case 1: Stash_Withdraw(playerid, slot, 5);
                case 2: Stash_Withdraw(playerid, slot, 0);
                default:
                {
                    Stash_Show(playerid);
                    return 1;
                }
            }
            Stash_Show(playerid);
            return 1;
        }
        case DIALOG_STASH_DEPOSIT:
        {
            if (!response)
            {
                Stash_Show(playerid);
                return 1;
            }

            new invSlot = Stash_InventorySlotFromRow(playerid, listitem);
            if (invSlot != -1) Stash_ShowDepositAmount(playerid, invSlot);
            else Stash_Show(playerid);
            return 1;
        }
        case DIALOG_STASH_SIP_AMOUNT:
        {
            if (!response)
            {
                Stash_Show(playerid);
                return 1;
            }

            new invSlot = StashSelectedInvSlot[playerid];

            switch (listitem)
            {
                case 0: Stash_DepositPartial(playerid, invSlot, 1);
                case 1: Stash_DepositPartial(playerid, invSlot, 5);
                case 2: Stash_DepositPartial(playerid, invSlot, 0);
                default:
                {
                    Stash_Show(playerid);
                    return 1;
                }
            }
            Stash_Show(playerid);
            return 1;
        }
    }
    return 0;
}
