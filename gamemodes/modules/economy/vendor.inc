#if defined _ls_vendor_included
    #endinput
#endif
#define _ls_vendor_included

/* Pedagang kamp: satu-satunya sink uang tunai di awal game.
   Beli memakai cash, jual memakai material. Semua perpindahan item lewat
   Inventory_Grant/Consume supaya ledger tetap utuh. */

#define VENDOR_MAX_BUY  14
#define VENDOR_MAX_SELL 9

#define DIALOG_VENDOR_MAIN 2401
#define DIALOG_VENDOR_BUY  2402
#define DIALOG_VENDOR_SELL 2403

enum E_VENDOR_ENTRY
{
    veItemId,
    veAmount,
    vePrice,
    veLabel[28]
};

new const gVendorBuy[VENDOR_MAX_BUY][E_VENDOR_ENTRY] = {
    {ITEM_BANDAGE,         1,  25, "Bandage"},
    {ITEM_MEDKIT,          1,  90, "Medkit"},
    {ITEM_ANTIBIOTICS,     1,  70, "Antibiotics"},
    {ITEM_FOOD_CAN,        1,  25, "Canned Food"},
    {ITEM_WATER_BOTTLE,    1,  20, "Water Bottle"},
    {ITEM_PISTOL_AMMO,    12,  60, "Peluru Pistol x12"},
    {ITEM_RIFLE_AMMO,     10,  90, "Peluru Rifle x10"},
    {ITEM_BARRICADE_KIT,   1, 220, "Barricade Kit"},
    {ITEM_LOCKPICK,        1, 150, "Lockpick"},
    {ITEM_JERRYCAN,        1,  90, "Jerrycan"},
    {ITEM_REPAIR_KIT,      1, 350, "Repair Kit"},
    {ITEM_BAG_DAY,         1, 420, "Tas Harian (+20 kg)"},
    {ITEM_BAG_HIKING,      1, 950, "Tas Gunung (+32 kg)"},
    {ITEM_BAG_MILITARY,    1, 2100, "Tas Militer (+45 kg)"}
};

new const gVendorSell[VENDOR_MAX_SELL][E_VENDOR_ENTRY] = {
    {ITEM_SCRAP,       1,  8, "Scrap"},
    {ITEM_CLOTH,       1,  5, "Cloth"},
    {ITEM_WOOD,        1,  6, "Wood"},
    {ITEM_CHEMICAL,    1, 12, "Chemical"},
    {ITEM_HERB,        1,  6, "Herb"},
    {ITEM_ELECTRONICS, 1, 15, "Electronics"},
    {ITEM_POWDER,      1, 10, "Powder"},
    {ITEM_DOCUMENT,    1, 180, "Dokumen Rahasia"},
    {ITEM_NOTEBOOK,    1,  90, "Buku Catatan"}
};

stock Vendor_Cash(playerid)
{
    return PlayerSurvival[playerid][psCash];
}

stock Vendor_SpendCash(playerid, amount)
{
    if (amount <= 0) return 0;
    if (PlayerSurvival[playerid][psCash] < amount) return 0;

    PlayerSurvival[playerid][psCash] -= amount;
    return 1;
}

stock Vendor_EarnCash(playerid, amount)
{
    if (amount <= 0) return 0;

    PlayerSurvival[playerid][psCash] += amount;
    return 1;
}

stock Vendor_ShowMenu(playerid)
{
    if (!Account_IsPlaying(playerid)) return 0;

    if (!Player_IsSafe(playerid))
    {
        SendClientMessage(playerid, UI_ACCENT_WARN, "[VENDOR] Pedagang hanya ada di kamp karantina.");
        return 0;
    }

    new body[256], line[64];
    format(body, sizeof(body), "Beli barang\nJual material\n");
    format(line, sizeof(line), "Uang: $%d", Vendor_Cash(playerid));

    ShowPlayerDialog(playerid, DIALOG_VENDOR_MAIN, DIALOG_STYLE_LIST, line, body, "Pilih", "Tutup");
    return 1;
}

stock Vendor_ShowBuy(playerid)
{
    new body[1024], line[96];

    for (new index = 0; index < VENDOR_MAX_BUY; index++)
    {
        format(line, sizeof(line), "%s  $%d\n", gVendorBuy[index][veLabel], gVendorBuy[index][vePrice]);
        if (index == 0) body[0] = EOS;
        strcat(body, line, sizeof(body));
    }

    new caption[64];
    format(caption, sizeof(caption), "Beli - uang $%d", Vendor_Cash(playerid));
    ShowPlayerDialog(playerid, DIALOG_VENDOR_BUY, DIALOG_STYLE_LIST, caption, body, "Beli", "Kembali");
    return 1;
}

stock Vendor_ShowSell(playerid)
{
    new body[1024], line[112], itemLabel[24];

    body[0] = EOS;

    for (new index = 0; index < VENDOR_MAX_SELL; index++)
    {
        new held = Inventory_CountItem(playerid, gVendorSell[index][veItemId]);
        Item_GetName(gVendorSell[index][veItemId], itemLabel, sizeof(itemLabel));
        format(line, sizeof(line), "%s x%d  $%d/unit\n", itemLabel, held, gVendorSell[index][vePrice]);
        strcat(body, line, sizeof(body));
    }

    /* Baris jual-semua (audit U2): jual 20 scrap satu-satu = 20 klik dialog,
       menyiksa di mobile. Snapshot jumlah saat dialog dibuka. */
    new sellAllCount = 0;
    for (new index = 0; index < VENDOR_MAX_SELL; index++)
        sellAllCount += Inventory_CountItem(playerid, gVendorSell[index][veItemId]);
    format(line, sizeof(line), ">> Jual SEMUA material (%d unit)\n", sellAllCount);
    strcat(body, line, sizeof(body));

    ShowPlayerDialog(playerid, DIALOG_VENDOR_SELL, DIALOG_STYLE_LIST, "Jual material", body, "Jual", "Kembali");
    return 1;
}

stock Vendor_Buy(playerid, index)
{
    if (index < 0 || index >= VENDOR_MAX_BUY) return 0;

    new price = gVendorBuy[index][vePrice];
    /* Uang + item + save karakter SATU transaksi: crash sebelum save tidak
       boleh mengembalikan cash sementara item tetap terpersist (T3). */
    Tx_Begin();
    if (!Vendor_SpendCash(playerid, price))
    {
        Tx_Rollback();
        new message[112];
        format(message, sizeof(message), "[VENDOR] Uang kurang. Butuh $%d.", price);
        SendClientMessage(playerid, UI_ACCENT_WARN, message);
        return 0;
    }

    if (!Inventory_Grant(playerid, gVendorBuy[index][veItemId], gVendorBuy[index][veAmount], "vendor_buy"))
    {
        Tx_Rollback();
        Vendor_EarnCash(playerid, price);
        SendClientMessage(playerid, UI_ACCENT_WARN, "[VENDOR] Tas penuh, pembelian dibatalkan.");
        return 0;
    }
    Account_SaveCharacter(playerid);
    Tx_Commit();

    new itemLabel[24], message[112];
    Item_GetName(gVendorBuy[index][veItemId], itemLabel, sizeof(itemLabel));
    format(message, sizeof(message), "[VENDOR] Beli %s x%d ($%d).", itemLabel, gVendorBuy[index][veAmount], price);
    SendClientMessage(playerid, UI_ACCENT_PRIMARY, message);
    return 1;
}

stock Vendor_Sell(playerid, index)
{
    if (index < 0 || index >= VENDOR_MAX_SELL) return 0;

    new itemid = gVendorSell[index][veItemId];
    if (Inventory_CountItem(playerid, itemid) <= 0)
    {
        SendClientMessage(playerid, UI_TEXT_MUTED, "[VENDOR] Tidak ada item itu di tas.");
        return 0;
    }

    Tx_Begin();
    if (!Inventory_Consume(playerid, itemid, 1, "vendor_sell"))
    {
        Tx_Rollback();
        return 0;
    }

    new price = gVendorSell[index][vePrice];
    price += (price * Skills_SellBonus(playerid)) / 100;
    Vendor_EarnCash(playerid, price);
    Account_SaveCharacter(playerid);
    Tx_Commit();

    new itemLabel[24], message[112];
    Item_GetName(itemid, itemLabel, sizeof(itemLabel));
    format(message, sizeof(message), "[VENDOR] Jual %s (+$%d).", itemLabel, price);
    SendClientMessage(playerid, UI_ACCENT_PRIMARY, message);
    return 1;
}

/* Jual semua material yang laku sekaligus (audit U2): satu transaksi,
   satu klik — mobile tidak perlu 20 klik dialog. */
stock Vendor_SellAll(playerid)
{
    new totalUnits = 0, totalCash = 0;

    Tx_Begin();
    for (new index = 0; index < VENDOR_MAX_SELL; index++)
    {
        new itemid = gVendorSell[index][veItemId];
        new held = Inventory_CountItem(playerid, itemid);
        if (held <= 0) continue;

        if (!Inventory_Consume(playerid, itemid, held, "vendor_sell_all"))
        {
            Tx_Rollback();
            SendClientMessage(playerid, UI_ACCENT_DANGER, "[VENDOR] Gagal menjual semua, dibatalkan.");
            return 0;
        }
        new price = gVendorSell[index][vePrice];
        price += (price * Skills_SellBonus(playerid)) / 100;
        totalUnits += held;
        totalCash += price * held;
    }

    if (totalUnits == 0)
    {
        Tx_Rollback();
        SendClientMessage(playerid, UI_TEXT_MUTED, "[VENDOR] Tidak ada material yang bisa dijual.");
        return 0;
    }

    Vendor_EarnCash(playerid, totalCash);
    Account_SaveCharacter(playerid);
    Tx_Commit();

    new message[112];
    format(message, sizeof(message), "[VENDOR] Jual semua: %d unit (+$%d).", totalUnits, totalCash);
    SendClientMessage(playerid, UI_ACCENT_PRIMARY, message);
    return 1;
}

stock Vendor_OnDialogResponse(playerid, dialogid, response, listitem, const inputtext[])
{
    #pragma unused inputtext

    /* Toko hanya di kamp: validasi ulang saat respons (audit F-13). */
    if (response && (!Account_IsPlaying(playerid) || !Zone_IsHub(Player_GetZone(playerid)))) return 0;

    switch (dialogid)
    {
        case DIALOG_VENDOR_MAIN:
        {
            if (!response) return 1;
            if (listitem == 0) Vendor_ShowBuy(playerid);
            else Vendor_ShowSell(playerid);
            return 1;
        }
        case DIALOG_VENDOR_BUY:
        {
            if (!response) return Vendor_ShowMenu(playerid);
            Vendor_Buy(playerid, listitem);
            Vendor_ShowBuy(playerid);
            return 1;
        }
        case DIALOG_VENDOR_SELL:
        {
            if (!response) return Vendor_ShowMenu(playerid);
            if (listitem == VENDOR_MAX_SELL) Vendor_SellAll(playerid);
            else Vendor_Sell(playerid, listitem);
            Vendor_ShowSell(playerid);
            return 1;
        }
    }
    return 0;
}
