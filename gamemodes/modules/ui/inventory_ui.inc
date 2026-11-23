#if defined _ls_inventory_ui_included
    #endinput
#endif
#define _ls_inventory_ui_included

/* UI tas berbasis dialog: satu-satunya jalur yang bisa dipakai di PC dan SA-MP Mobile.
   Hanya satu dialog aktif per pemain; setiap aksi divalidasi ulang ke slot yang dipilih. */

#define DIALOG_INV_MAIN   2101
#define DIALOG_INV_ACTION 2102
#define DIALOG_HOTBAR     2103
#define DIALOG_INV_HOTBAR 2104

new InventorySelectedSlot[MAX_PLAYERS];

forward bool:InventoryUI_Use(playerid, slot);

stock InventoryUI_Show(playerid)
{
    new body[1024], line[128], rows = 0;
    body[0] = EOS;

    /* TABLIST_HEADERS: baris pertama WAJIB header. Baris header tidak dihitung
       sebagai listitem, jadi klik tetap sejajar dengan data. */
    strcat(body, "Barang\tJumlah / kondisi\n", sizeof(body));

    for (new slot = 0; slot < MAX_INVENTORY_SLOTS; slot++)
    {
        new itemid = PlayerInventory[playerid][slot][invItemId];
        if (!Item_IsValid(itemid)) continue;

        new itemLabel[24];
        Item_GetName(itemid, itemLabel, sizeof(itemLabel));

        new mark[16], hotIndex = Hotbar_FindSlotForItem(playerid, itemid);
        if (hotIndex >= 0) format(mark, sizeof(mark), "[H%d] ", hotIndex + 1);
        else mark[0] = EOS;

        if (PlayerInventory[playerid][slot][invDurability] < WEAPON_CONDITION_MAX)
        {
            format(line, sizeof(line), "%s%s x%d (%d%%)\n", mark, itemLabel,
                PlayerInventory[playerid][slot][invAmount],
                PlayerInventory[playerid][slot][invDurability]);
        }
        else
        {
            format(line, sizeof(line), "%s%s x%d\n", mark, itemLabel, PlayerInventory[playerid][slot][invAmount]);
        }
        strcat(body, line, sizeof(body));
        rows++;
    }

    if (rows == 0)
    {
        SendClientMessage(playerid, UI_TEXT_MUTED, "[TAS] Kosong. Cari loot di zona terbuka.");
        SendClientMessage(playerid, UI_TEXT_MUTED, "[TAS] Tas menentukan kapasitas: /tas untuk lihat kg dan tingkat tas.");
        return 0;
    }

    new tierLabel[20], title[64];
    Bag_LevelName(Bag_BestTier(playerid), tierLabel, sizeof(tierLabel));
    format(title, sizeof(title), "%s - %.1f/%.0f kg", tierLabel,
        Inventory_GetWeight(playerid), Bag_Capacity(playerid));

    Bag_PlayRummage(playerid);
    ShowPlayerDialog(playerid, DIALOG_INV_MAIN, DIALOG_STYLE_TABLIST_HEADERS, title, body, "Pilih", "Tutup");
    return 1;
}

/* Nomor urut 1..N sesuai urutan yang tampil di /inv. Dipakai /pakai 2 dan /buang 3 5. */
stock InventoryUI_SlotByNumber(playerid, ordinal)
{
    if (ordinal <= 0) return -1;

    new seen = 1;
    for (new slot = 0; slot < MAX_INVENTORY_SLOTS; slot++)
    {
        if (!Item_IsValid(PlayerInventory[playerid][slot][invItemId])) continue;
        if (seen == ordinal) return slot;
        seen++;
    }
    return -1;
}

/* Dipakai command /pakai dan /buang: cari slot lewat nama item, bukan nomor baris,
   supaya tetap gampang di HP tanpa dialog. */
stock InventoryUI_FindSlotByName(playerid, const needle[])
{
    if (needle[0] == EOS) return -1;

    new itemLabel[24];
    for (new slot = 0; slot < MAX_INVENTORY_SLOTS; slot++)
    {
        new itemid = PlayerInventory[playerid][slot][invItemId];
        if (!Item_IsValid(itemid)) continue;

        Item_GetName(itemid, itemLabel, sizeof(itemLabel));
        if (strfind(itemLabel, needle, true) != -1) return slot;
    }
    return -1;
}

/* Dialog hanya mengirim indeks baris, bukan slot asli: petakan ulang tiap kali dibuka. */
stock InventoryUI_SlotFromRow(playerid, row)
{
    new seen = 0;
    for (new slot = 0; slot < MAX_INVENTORY_SLOTS; slot++)
    {
        if (!Item_IsValid(PlayerInventory[playerid][slot][invItemId])) continue;
        if (seen == row) return slot;
        seen++;
    }
    return -1;
}

stock InventoryUI_ShowAction(playerid, slot)
{
    if (slot < 0 || slot >= MAX_INVENTORY_SLOTS) return 0;

    new itemid = PlayerInventory[playerid][slot][invItemId];
    if (!Item_IsValid(itemid)) return 0;

    new itemLabel[24], caption[48];
    Item_GetName(itemid, itemLabel, sizeof(itemLabel));
    format(caption, sizeof(caption), "%s x%d", itemLabel, PlayerInventory[playerid][slot][invAmount]);

    InventorySelectedSlot[playerid] = slot;

    new body[192];
    format(body, sizeof(body), "Aksi\tEfek\nPakai\tKonsumsi atau aktifkan item ini\nBuang 1\tJatuhkan 1 buah ke tanah\nBuang semua\tJatuhkan seluruh tumpukan\nSlot hotbar\tTandai item ini di /hs 1-5");
    ShowPlayerDialog(playerid, DIALOG_INV_ACTION, DIALOG_STYLE_TABLIST_HEADERS, caption, body, "Pilih", "Kembali");
    return 1;
}

/* Hotbar 1-5: panel pengaturan + pemakaian cepat tanpa dialog tas. */
stock Hotbar_Show(playerid)
{
    new body[512], line[96], itemLabel[24], count, caption[64];
    body[0] = EOS;

    strcat(body, "Slot\tIsi\n", sizeof(body));

    for (new index = 0; index < HOTBAR_SLOTS; index++)
    {
        new itemid = Hotbar_ItemAt(playerid, index);
        if (!Item_IsValid(itemid))
        {
            format(line, sizeof(line), "%d\t(kosong)\n", index + 1);
            strcat(body, line, sizeof(body));
            continue;
        }

        Item_GetName(itemid, itemLabel, sizeof(itemLabel));
        count = Inventory_CountItem(playerid, itemid);
        format(line, sizeof(line), "%d\t%s x%d\n", index + 1, itemLabel, count);
        strcat(body, line, sizeof(body));
    }

    strcat(body, ">> Pakai: /hs 1\n", sizeof(body));
    strcat(body, ">> Isi slot: /hotbar set 1 bandage\n", sizeof(body));

    format(caption, sizeof(caption), "Hotbar Cepat - beban %.1f/%.0f kg",
        Inventory_GetWeight(playerid), Bag_Capacity(playerid));
    ShowPlayerDialog(playerid, DIALOG_HOTBAR, DIALOG_STYLE_TABLIST_HEADERS, caption, body, "Pakai", "Tutup");
    return 1;
}

/* Pilih slot hotbar dari dialog, tanpa mengetik nama item (ramah mobile). */
stock Hotbar_AssignPanel(playerid, invSlot)
{
    if (invSlot < 0 || invSlot >= MAX_INVENTORY_SLOTS) return 0;

    new itemid = PlayerInventory[playerid][invSlot][invItemId];
    if (!Item_IsValid(itemid)) return 0;

    new itemLabel[24], body[384], line[96], caption[64];
    Item_GetName(itemid, itemLabel, sizeof(itemLabel));

    body[0] = EOS;
    strcat(body, "Slot\tIsi sekarang\n", sizeof(body));

    for (new index = 0; index < HOTBAR_SLOTS; index++)
    {
        new current = Hotbar_ItemAt(playerid, index);
        if (!Item_IsValid(current))
        {
            format(line, sizeof(line), "%d\t(kosong)\n", index + 1);
        }
        else
        {
            new currentLabel[24];
            Item_GetName(current, currentLabel, sizeof(currentLabel));
            format(line, sizeof(line), "%d\t%s\n", index + 1, currentLabel);
        }
        strcat(body, line, sizeof(body));
    }

    InventorySelectedSlot[playerid] = invSlot;
    format(caption, sizeof(caption), "Tandai %s ke hotbar", itemLabel);
    ShowPlayerDialog(playerid, DIALOG_INV_HOTBAR, DIALOG_STYLE_TABLIST_HEADERS, caption, body, "Simpan", "Kembali");
    return 1;
}

stock bool:Hotbar_Use(playerid, index)
{
    if (index < 0 || index >= HOTBAR_SLOTS)
    {
        SendClientMessage(playerid, UI_TEXT_MUTED, "[HOTBAR] Nomor slot 1-5. Contoh: /hs 1");
        return false;
    }

    new itemid = Hotbar_ItemAt(playerid, index);
    if (!Item_IsValid(itemid))
    {
        new line[96];
        format(line, sizeof(line), "[HOTBAR] Slot %d kosong. Isi dengan /hotbar set %d <nama item>.", index + 1, index + 1);
        SendClientMessage(playerid, UI_TEXT_MUTED, line);
        return false;
    }

    new slot = Hotbar_FindSlotForItem(playerid, itemid);
    if (slot == -1)
    {
        new itemLabel[24], line[112];
        Item_GetName(itemid, itemLabel, sizeof(itemLabel));
        format(line, sizeof(line), "[HOTBAR] %s tidak ada di tas.", itemLabel);
        SendClientMessage(playerid, UI_ACCENT_WARN, line);
        return false;
    }

    return InventoryUI_Use(playerid, slot);
}

stock bool:InventoryUI_Use(playerid, slot)
{
    if (slot < 0 || slot >= MAX_INVENTORY_SLOTS) return false;

    new itemid = PlayerInventory[playerid][slot][invItemId];
    if (!Item_IsValid(itemid)) return false;

    /* Adrenaline adalah satu-satunya item yang boleh dipakai saat downed:
       menyelamatkan diri sendiri tanpa menunggu pemain lain. */
    if (itemid == ITEM_ADRENALINE && PlayerSurvival[playerid][psDowned])
    {
        if (!Inventory_Consume(playerid, ITEM_ADRENALINE, 1, "adrenaline")) return false;

        Player_Revive(playerid);
        PlayerSurvival[playerid][psHealth] = 45.0;
        SetPlayerHealth(playerid, 45.0);
        Toast_Show(playerid, "ADRENALINE", "Denyut melaju. Kamu bangkit sendiri.", UI_ACCENT_PRIMARY);
        return true;
    }

    if (PlayerSurvival[playerid][psDowned]) return false;

    new Float:heal = 0.0;
    new Float:cure = 0.0;
    new woundsToRemove = 0;

    switch (itemid)
    {
        case ITEM_BANDAGE:
        {
            heal = ITEM_BANDAGE_HEAL;
            woundsToRemove = 1;
        }
        case ITEM_MEDKIT:
        {
            heal = ITEM_MEDKIT_HEAL;
            woundsToRemove = MAX_WOUND_STACKS;
        }
        case ITEM_BLOOD_PACK: heal = ITEM_BLOODPACK_HEAL;
        case ITEM_ANTIBIOTICS: cure = ITEM_ANTIBIOTICS_CURE;
        case ITEM_FOOD_CAN, ITEM_WATER_BOTTLE: heal = 5.0;
        case ITEM_ENERGY_DRINK: heal = 10.0;
        case ITEM_VACCINE: cure = 200.0;
        case ITEM_BAG_SMALL, ITEM_BAG_DAY, ITEM_BAG_HIKING, ITEM_BAG_MILITARY:
        {
            // Tas pasif: begitu ada di dalam tas, kapasitas dan model punggung langsung aktif.
            Bag_ShowInfo(playerid);
            Toast_Show(playerid, "TAS", "Tas aktif: kapasitas dan model punggung sudah diperbarui.", UI_ACCENT_PRIMARY);
            return true;
        }
        case ITEM_RADIO:
        {
            SendClientMessage(playerid, UI_ACCENT_PRIMARY, "[RADIO] Radio aktif. Pakai /radio <pesan> untuk bicara satu lobby.");
            return true;
        }
        case ITEM_DECOY:
        {
            if (!Inventory_Consume(playerid, ITEM_DECOY, 1, "decoy")) return false;

            new Float:angle = float(random(360));
            new Float:distance = 25.0 + float(random(11));
            new Float:px, Float:py, Float:pz;
            GetPlayerPos(playerid, px, py, pz);
            new Float:tx = px + floatsin(angle, degrees) * distance;
            new Float:ty = py + floatcos(angle, degrees) * distance;

            Horde_AddHeat(tx, ty, pz, 22.0);
            Toast_Show(playerid, "DECOY", "Sirene bunyi jauh dari posisimu. Zombie tertarik ke sana.", UI_ACCENT_PRIMARY);
            return true;
        }
        case ITEM_TOOLBOX:
        {
            return Garage_RepairNearestEx(playerid, true) != 0;
        }
        case ITEM_DOCUMENT:
        {
            return Lore_ShowDocument(playerid) != 0;
        }
        case ITEM_NOTEBOOK:
        {
            if (!Inventory_Consume(playerid, ITEM_NOTEBOOK, 1, "notebook")) return false;

            Contracts_OnDocumentFound(playerid);
            Skills_AddXp(playerid, 5);
            Toast_Show(playerid, "BUKU CATATAN", "Kode gudang terbaca. Progres kontrak dokumen +1.", UI_ACCENT_PRIMARY);
            return true;
        }
        case ITEM_WAREHOUSE_KEY:
        {
            if (Zone_IsHub(Player_GetZone(playerid)))
            {
                SendClientMessage(playerid, UI_ACCENT_WARN, "[KUNCI] Kunci gudang hanya bisa dipakai di luar kamp.");
                return false;
            }
            if (!Loot_HasFreeSlot())
            {
                SendClientMessage(playerid, UI_ACCENT_WARN, "[KUNCI] Dunia penuh loot. Coba beberapa saat lagi.");
                return false;
            }
            if (!Inventory_Consume(playerid, ITEM_WAREHOUSE_KEY, 1, "warehouse_key")) return false;

            new rolledItem = ITEM_NONE_ID, rolledAmount = 1;
            if (!Loot_RollTable(gRareLoot, LOOT_RARE_SLOTS, rolledItem, rolledAmount))
            {
                rolledItem = ITEM_REPAIR_KIT;
                rolledAmount = 1;
            }

            new Float:px, Float:py, Float:pz;
            GetPlayerPos(playerid, px, py, pz);
            new Float:angle = float(random(360));
            new crateSlot = Loot_Spawn(
                px + floatsin(angle, degrees) * 6.0,
                py + floatcos(angle, degrees) * 6.0,
                pz + 0.2, rolledItem, rolledAmount + 1,
                INVALID_PLAYER_ID, Lobby_Of(playerid), LOOT_KIND_CRATE);

            if (crateSlot == -1)
            {
                Inventory_Grant(playerid, ITEM_WAREHOUSE_KEY, 1, "warehouse_key_refund");
                SendClientMessage(playerid, UI_ACCENT_WARN, "[KUNCI] Peti gagal muncul. Kunci dikembalikan.");
                return false;
            }

            Toast_Show(playerid, "GUDANG", "Peti gudang muncul di dekatmu. Ambil cepat.", UI_ACCENT_PRIMARY);
            Log_Format(LOG_INFO, "[KEY] char=%d peti gudang item=%d x%d",
                ActiveCharId[playerid], rolledItem, rolledAmount + 1);
            return true;
        }
        case ITEM_BARRICADE_KIT:
        {
            // Barricade persisten: disimpan ke DB, punya HP, dan bisa diperbaiki dengan /fix.
            if (!Base_PlaceBarricade(playerid)) return false;
            return true;
        }
        case ITEM_JERRYCAN:
        {
            return Garage_PourJerrycan(playerid, slot);
        }
        case ITEM_REPAIR_KIT:
        {
            return Garage_RepairNearest(playerid) != 0;
        }
        default: return false;
    }

    if (heal > 0.0 && PlayerSurvival[playerid][psHealth] >= FULL_HP && cure == 0.0
        && Wounds_Get(playerid) == 0
        && itemid != ITEM_FOOD_CAN && itemid != ITEM_WATER_BOTTLE)
    {
        SendClientMessage(playerid, UI_TEXT_MUTED, "[TAS] HP sudah penuh.");
        return false;
    }

    if (cure > 0.0 && PlayerSurvival[playerid][psInfection] <= 0.0)
    {
        SendClientMessage(playerid, UI_TEXT_MUTED, "[TAS] Tidak ada infeksi untuk diobati.");
        return false;
    }

    if (!Inventory_Consume(playerid, itemid, 1, "consume")) return false;

    if (heal > 0.0)
    {
        PlayerSurvival[playerid][psHealth] += heal;
        if (PlayerSurvival[playerid][psHealth] > FULL_HP) PlayerSurvival[playerid][psHealth] = FULL_HP;
        SetPlayerHealth(playerid, PlayerSurvival[playerid][psHealth]);
    }

    if (itemid == ITEM_FOOD_CAN || itemid == ITEM_WATER_BOTTLE)
    {
        Needs_ConsumeItem(playerid, itemid);
    }

    if (itemid == ITEM_ENERGY_DRINK)
    {
        PlayerSurvival[playerid][psThirst] += 25.0;
        PlayerSurvival[playerid][psBladder] += 5.0;
        Needs_Clamp(PlayerSurvival[playerid][psThirst]);
        Needs_Clamp(PlayerSurvival[playerid][psBladder]);
    }

    if (woundsToRemove > 0)
    {
        Wounds_Remove(playerid, woundsToRemove);
    }

    if (cure > 0.0)
    {
        PlayerSurvival[playerid][psInfection] -= cure;
        if (PlayerSurvival[playerid][psInfection] < 0.0) PlayerSurvival[playerid][psInfection] = 0.0;
    }

    new itemLabel[24], line[96];
    Item_GetName(itemid, itemLabel, sizeof(itemLabel));
    format(line, sizeof(line), "[TAS] Memakai %s.", itemLabel);
    SendClientMessage(playerid, UI_ACCENT_PRIMARY, line);

    // Alat (kategori TOOL) aus tiap dipakai; consumable habis karena memang dikonsumsi.
    if (Item_GetCategory(itemid) == ITEM_TOOL)
    {
        Inventory_DamageItem(playerid, slot, ITEM_DURABILITY_USE_COST);
    }
    return true;
}

stock bool:InventoryUI_Drop(playerid, slot, amountOverride = 0)
{
    if (slot < 0 || slot >= MAX_INVENTORY_SLOTS) return false;

    new itemid = PlayerInventory[playerid][slot][invItemId];
    new amount = PlayerInventory[playerid][slot][invAmount];
    if (!Item_IsValid(itemid) || amount <= 0) return false;

    /* Durability ikut ke drop: tanpanya jerrycan sisa 2L kembali penuh 20L
       saat diambil (refresh bahan bakar gratis — audit F-07). */
    new slotDurability = PlayerInventory[playerid][slot][invDurability];

    if (amountOverride > 0 && amountOverride < amount) amount = amountOverride;
    if (!Inventory_Consume(playerid, itemid, amount, "drop")) return false;

    new Float:x, Float:y, Float:z;
    GetPlayerPos(playerid, x, y, z);
    Loot_Spawn(x, y, z - 0.8, itemid, amount, playerid, -1, LOOT_KIND_ITEM, slotDurability);

    new itemLabel[24], line[112];
    Item_GetName(itemid, itemLabel, sizeof(itemLabel));
    format(line, sizeof(line), "[TAS] %s x%d dibuang (hanya Anda yang bisa mengambil).", itemLabel, amount);
    SendClientMessage(playerid, UI_ACCENT_WARN, line);
    return true;
}

stock InventoryUI_Handle(playerid, dialogid, response, listitem, const inputtext[])
{
    #pragma unused inputtext

    switch (dialogid)
    {
        case DIALOG_INV_MAIN:
        {
            if (!response) return 1;

            new slot = InventoryUI_SlotFromRow(playerid, listitem);
            if (slot == -1) return 1;

            InventoryUI_ShowAction(playerid, slot);
            return 1;
        }
        case DIALOG_INV_HOTBAR:
        {
            if (!response)
            {
                InventoryUI_Show(playerid);
                return 1;
            }

            if (listitem < 0 || listitem >= HOTBAR_SLOTS)
            {
                InventoryUI_Show(playerid);
                return 1;
            }

            new invSlot = InventorySelectedSlot[playerid];
            new itemid = (invSlot >= 0 && invSlot < MAX_INVENTORY_SLOTS) ? PlayerInventory[playerid][invSlot][invItemId] : ITEM_NONE_ID;

            if (!Item_IsValid(itemid))
            {
                InventoryUI_Show(playerid);
                return 1;
            }

            Hotbar_Set(playerid, listitem, itemid);
            Account_SaveCharacter(playerid);

            new itemLabel[24], line[128];
            Item_GetName(itemid, itemLabel, sizeof(itemLabel));
            format(line, sizeof(line), "[HOTBAR] %s masuk slot %d. Pakai dengan /hs %d.", itemLabel, listitem + 1, listitem + 1);
            SendClientMessage(playerid, UI_ACCENT_PRIMARY, line);
            InventoryUI_Show(playerid);
            return 1;
        }
        case DIALOG_HOTBAR:
        {
            if (!response) return 1;

            if (listitem >= HOTBAR_SLOTS)
            {
                SendClientMessage(playerid, UI_TEXT_MUTED, "[HOTBAR] Pakai: /hs 1 .. /hs 5 | Isi: /hotbar set 1 bandage");
                return 1;
            }

            Hotbar_Use(playerid, listitem);
            Hotbar_Show(playerid);
            return 1;
        }
        case DIALOG_INV_ACTION:
        {
            if (!response)
            {
                InventoryUI_Show(playerid);
                return 1;
            }

            new slot = InventorySelectedSlot[playerid];

            if (listitem == 0)
            {
                if (!InventoryUI_Use(playerid, slot))
                {
                    SendClientMessage(playerid, UI_TEXT_MUTED, "[TAS] Item ini tidak bisa dipakai.");
                }
                InventoryUI_Show(playerid);
                return 1;
            }

            if (listitem == 1)
            {
                InventoryUI_Drop(playerid, slot, 1);
                InventoryUI_Show(playerid);
                return 1;
            }

            if (listitem == 2)
            {
                InventoryUI_Drop(playerid, slot);
                InventoryUI_Show(playerid);
                return 1;
            }

            if (listitem == 3)
            {
                Hotbar_AssignPanel(playerid, slot);
                return 1;
            }
            return 1;
        }
    }
    return 0;
}
