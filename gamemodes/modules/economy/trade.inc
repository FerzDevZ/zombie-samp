#if defined _ls_trade_included
    #endinput
#endif
#define _ls_trade_included

/* Barter antar pemain. Dua pihak menandai item + uang, keduanya konfirmasi,
   lalu pertukaran dieksekusi atomik. Setiap perubahan penawaran me-reset
   konfirmasi kedua pihak (anti-swap-scam). Item hanya berpindah lewat
   Inventory_Consume/Inventory_Grant agar item_ledger tetap lengkap. */

#define DIALOG_TRADE       3101
#define DIALOG_TRADE_CASH  3102
#define TRADE_RANGE        5.0
#define TRADE_RANGE_EXEC   6.0
#define TRADE_MAX_CASH     100000

#define TRADE_ROW_UANG    (-1)
#define TRADE_ROW_TOMBOL  (-2)

forward bool:Trade_ValidateSession(playerid);

new TradePartner[MAX_PLAYERS];      // INVALID_PLAYER_ID saat tidak sesi
new TradeOfferMask[MAX_PLAYERS];    // bitmask slot tas yang ditawarkan
new TradeCashOffer[MAX_PLAYERS];
new bool:TradeConfirmed[MAX_PLAYERS];
new TradeRowSlot[MAX_PLAYERS][MAX_INVENTORY_SLOTS + 2];

stock Trade_Reset(playerid)
{
    TradePartner[playerid] = INVALID_PLAYER_ID;
    TradeOfferMask[playerid] = 0;
    TradeCashOffer[playerid] = 0;
    TradeConfirmed[playerid] = false;
    return 1;
}

stock bool:Trade_InSession(playerid)
{
    return TradePartner[playerid] != INVALID_PLAYER_ID;
}

/* Setiap perubahan penawaran membatalkan konfirmasi kedua pihak. */
stock Trade_UnconfirmBoth(playerid)
{
    new partner = TradePartner[playerid];
    if (partner == INVALID_PLAYER_ID) return 0;

    TradeConfirmed[playerid] = false;
    TradeConfirmed[partner] = false;
    return 1;
}

stock Trade_Cancel(playerid, const why[])
{
    new partner = TradePartner[playerid];
    if (partner == INVALID_PLAYER_ID) return 0;

    Trade_Reset(playerid);
    Trade_Reset(partner);

    SendClientMessage(playerid, UI_ACCENT_WARN, why);
    SendClientMessage(partner, UI_ACCENT_WARN, why);
    Log_Format(LOG_INFO, "[TRADE] batal char=%d partner=%d", ActiveCharId[playerid], ActiveCharId[partner]);
    return 1;
}

stock bool:Trade_CanTrade(playerid, target)
{
    if (!Account_IsPlaying(playerid) || !Account_IsPlaying(target)) return false;
    if (playerid == target) return false;
    if (Player_IsDowned(playerid) || Player_IsDowned(target)) return false;
    if (Lobby_Of(playerid) != Lobby_Of(target)) return false;
    return true;
}

stock Trade_Start(playerid, target)
{
    if (!Trade_CanTrade(playerid, target))
    {
        SendClientMessage(playerid, UI_ACCENT_WARN, "[TRADE] Target harus sedang main, tidak downed, dan satu lobby denganmu.");
        return 0;
    }
    if (Trade_InSession(playerid) || Trade_InSession(target))
    {
        SendClientMessage(playerid, UI_ACCENT_WARN, "[TRADE] Salah satu dari kalian sedang dalam sesi trade lain.");
        return 0;
    }

    new Float:px, Float:py, Float:pz, Float:tx, Float:ty, Float:tz;
    GetPlayerPos(playerid, px, py, pz);
    GetPlayerPos(target, tx, ty, tz);
    if (VectorSize(tx - px, ty - py, tz - pz) > TRADE_RANGE)
    {
        SendClientMessage(playerid, UI_ACCENT_WARN, "[TRADE] Terlalu jauh. Dekati target (5 meter).");
        return 0;
    }

    Trade_Reset(playerid);
    Trade_Reset(target);
    TradePartner[playerid] = target;
    TradePartner[target] = playerid;

    SendClientMessage(playerid, UI_ACCENT_PRIMARY, "[TRADE] Sesi dibuka. Ketik /trade lagi untuk menandai barangmu.");
    SendClientMessage(target, UI_ACCENT_PRIMARY, "[TRADE] Sesi dibuka. Ketik /trade lagi untuk menandai barangmu.");
    Log_Format(LOG_INFO, "[TRADE] mulai char=%d partner=%d", ActiveCharId[playerid], ActiveCharId[target]);
    return 1;
}

stock Trade_OpenPanel(playerid)
{
    new partner = TradePartner[playerid];
    if (partner == INVALID_PLAYER_ID) return 0;

    if (!Trade_ValidateSession(playerid))
    {
        Trade_Cancel(playerid, "[TRADE] Dibatalkan: salah satu pihak tidak memenuhi syarat.");
        return 1;
    }

    new body[1536], line[96], rowCount = 0, tradeItemName[32];

    for (new slot = 0; slot < MAX_INVENTORY_SLOTS; slot++)
    {
        new itemid = PlayerInventory[playerid][slot][invItemId];
        if (!Item_IsValid(itemid)) continue;

        TradeRowSlot[playerid][rowCount] = slot;
        Item_GetName(itemid, tradeItemName, sizeof(tradeItemName));
        format(line, sizeof(line), "%s%s x%d\n",
            (TradeOfferMask[playerid] & (1 << slot)) ? "[DITAWARKAN] " : "",
            tradeItemName,
            PlayerInventory[playerid][slot][invAmount]);
        strcat(body, line);
        rowCount++;
    }

    TradeRowSlot[playerid][rowCount] = TRADE_ROW_UANG;
    format(line, sizeof(line), "Uang: $%d (klik untuk ubah)\n", TradeCashOffer[playerid]);
    strcat(body, line);
    rowCount++;

    TradeRowSlot[playerid][rowCount] = TRADE_ROW_TOMBOL;
    format(line, sizeof(line), "%s\n", TradeConfirmed[playerid] ? "BATALKAN KONFIRMASI" : "KONFIRMASI TRADE");
    strcat(body, line);
    rowCount++;

    new partnerName[MAX_PLAYER_NAME + 1];
    GetPlayerName(partner, partnerName, sizeof(partnerName));
    format(line, sizeof(line), "Barter dengan %s", partnerName);

    ShowPlayerDialog(playerid, DIALOG_TRADE, DIALOG_STYLE_LIST, line, body, "Pilih", "Tutup");
    return 1;
}

/* Syarat sesi masih sah: kedua pihak main, tidak downed, satu lobby, dekat. */
stock bool:Trade_ValidateSession(playerid)
{
    new partner = TradePartner[playerid];
    if (partner == INVALID_PLAYER_ID) return false;
    if (!Trade_CanTrade(playerid, partner)) return false;

    new Float:px, Float:py, Float:pz, Float:tx, Float:ty, Float:tz;
    GetPlayerPos(playerid, px, py, pz);
    GetPlayerPos(partner, tx, ty, tz);
    if (VectorSize(tx - px, ty - py, tz - pz) > TRADE_RANGE_EXEC) return false;

    return true;
}

/* Pra-validasi kapasitas penerima terhadap penawaran PEMBERI.
   Kode lama membaca PlayerInventory[penerima] dengan mask pemberi (item salah)
   dan hanya menghitung jenis item baru tanpa stack space (audit #7): precheck
   lolos lalu AddItem menolak → item pemberi hangus. Sekarang: berat total +
   kebutuhan slot nyata (stack space per itemid+durability, non-stackable 1 slot
   per penawaran). */
stock bool:Trade_Fits(receiverId, giverId, offerMask, bool:silent)
{
    new Float:weight = Inventory_GetWeight(receiverId);
    new slotsNeeded = 0;

    new groupItem[MAX_INVENTORY_SLOTS], groupDur[MAX_INVENTORY_SLOTS];
    new groupAmount[MAX_INVENTORY_SLOTS], groupCount = 0;

    for (new slot = 0; slot < MAX_INVENTORY_SLOTS; slot++)
    {
        if (!(offerMask & (1 << slot))) continue;

        new itemid = PlayerInventory[giverId][slot][invItemId];
        if (!Item_IsValid(itemid)) continue;

        new amount = PlayerInventory[giverId][slot][invAmount];
        weight += Item_GetWeight(itemid) * float(amount);

        if (!Item_IsStackable(itemid))
        {
            slotsNeeded++;   // tiap item non-stackable butuh slot sendiri
            continue;
        }

        new dur = PlayerInventory[giverId][slot][invDurability];
        new found = -1;
        for (new group = 0; group < groupCount; group++)
        {
            if (groupItem[group] == itemid && groupDur[group] == dur) { found = group; break; }
        }
        if (found == -1)
        {
            found = groupCount++;
            groupItem[found] = itemid;
            groupDur[found] = dur;
            groupAmount[found] = 0;
        }
        groupAmount[found] += amount;
    }

    for (new group = 0; group < groupCount; group++)
    {
        new maxStack = Item_GetMaxStack(groupItem[group]);
        if (maxStack < 1) maxStack = 1;

        new stackSpace = 0;
        for (new slot = 0; slot < MAX_INVENTORY_SLOTS; slot++)
        {
            if (PlayerInventory[receiverId][slot][invItemId] != groupItem[group]) continue;
            if (PlayerInventory[receiverId][slot][invDurability] != groupDur[group]) continue;
            new space = maxStack - PlayerInventory[receiverId][slot][invAmount];
            if (space > 0) stackSpace += space;
        }

        new extra = groupAmount[group] - stackSpace;
        if (extra > 0) slotsNeeded += (extra + maxStack - 1) / maxStack;
    }

    if (weight > Bag_Capacity(receiverId))
    {
        if (!silent) SendClientMessage(receiverId, UI_ACCENT_WARN, "[TRADE] Gagal: tas penerima akan melebihi kapasitas bebannya.");
        return false;
    }

    new freeSlots = 0;
    for (new slot = 0; slot < MAX_INVENTORY_SLOTS; slot++)
    {
        if (PlayerInventory[receiverId][slot][invItemId] == ITEM_NONE_ID) freeSlots++;
    }
    if (slotsNeeded > freeSlots)
    {
        if (!silent) SendClientMessage(receiverId, UI_ACCENT_WARN, "[TRADE] Gagal: tas penerima tidak punya slot kosong cukup.");
        return false;
    }

    return true;
}

/* Refund semua penawaran yang SUDAH terconsume saat fase mana pun gagal
   (audit F-17): partner bisa memakai itemnya sendiri setelah confirm untuk
   menggagalkan consume-nya — tanpa refund, item pihak lain hangus (grief). */
stock Trade_RefundMask(playerid, partner, const offerItemId[][MAX_INVENTORY_SLOTS],
    const offerAmount[][MAX_INVENTORY_SLOTS], const offerDurability[][MAX_INVENTORY_SLOTS],
    const consumedMask[2])
{
    for (new side = 0; side < 2; side++)
    {
        new receiver = (side == 0) ? playerid : partner;
        for (new slot = 0; slot < MAX_INVENTORY_SLOTS; slot++)
        {
            if (!(consumedMask[side] & (1 << slot))) continue;
            if (!Item_IsValid(offerItemId[side][slot])) continue;
            Inventory_Grant(receiver, offerItemId[side][slot], offerAmount[side][slot],
                "trade_refund", offerDurability[side][slot]);
        }
    }
    return 1;
}

stock Trade_Execute(playerid)
{
    new partner = TradePartner[playerid];
    if (partner == INVALID_PLAYER_ID) return 0;

    if (!TradeConfirmed[playerid])
    {
        SendClientMessage(playerid, UI_TEXT_MUTED, "[TRADE] Kamu belum konfirmasi.");
        return 1;
    }
    if (!TradeConfirmed[partner])
    {
        SendClientMessage(playerid, UI_TEXT_MUTED, "[TRADE] Menunggu partner konfirmasi.");
        Trade_OpenPanel(partner);
        return 1;
    }
    if (!Trade_ValidateSession(playerid))
    {
        Trade_Cancel(playerid, "[TRADE] Dibatalkan: salah satu pihak pindah/downed/terputus.");
        return 1;
    }

    /* Re-check uang pada saat eksekusi (bisa berubah sejak penawaran). */
    if (TradeCashOffer[playerid] > PlayerSurvival[playerid][psCash]) TradeCashOffer[playerid] = PlayerSurvival[playerid][psCash];
    if (TradeCashOffer[partner] > PlayerSurvival[partner][psCash]) TradeCashOffer[partner] = PlayerSurvival[partner][psCash];

    if (!Trade_Fits(partner, playerid, TradeOfferMask[playerid], false)
        || !Trade_Fits(playerid, partner, TradeOfferMask[partner], false))
    {
        Trade_UnconfirmBoth(playerid);
        return 1;
    }

    /* Snapshot penawaran SEBELUM konsumsi: Inventory_Consume mengosongkan slot,
       jadi fase grant tidak boleh membaca ulang PlayerInventory (item pernah
       hangus karena fase 2 membaca slot yang sudah kosong). */
    new offerItemId[2][MAX_INVENTORY_SLOTS];
    new offerAmount[2][MAX_INVENTORY_SLOTS];
    new offerDurability[2][MAX_INVENTORY_SLOTS];
    for (new slot = 0; slot < MAX_INVENTORY_SLOTS; slot++)
    {
        offerItemId[0][slot] = ITEM_NONE_ID;
        offerItemId[1][slot] = ITEM_NONE_ID;
        offerDurability[0][slot] = WEAPON_CONDITION_MAX;
        offerDurability[1][slot] = WEAPON_CONDITION_MAX;
        if (TradeOfferMask[playerid] & (1 << slot))
        {
            offerItemId[0][slot] = PlayerInventory[playerid][slot][invItemId];
            offerAmount[0][slot] = PlayerInventory[playerid][slot][invAmount];
            offerDurability[0][slot] = PlayerInventory[playerid][slot][invDurability];
        }
        if (TradeOfferMask[partner] & (1 << slot))
        {
            offerItemId[1][slot] = PlayerInventory[partner][slot][invItemId];
            offerAmount[1][slot] = PlayerInventory[partner][slot][invAmount];
            offerDurability[1][slot] = PlayerInventory[partner][slot][invDurability];
        }
    }

    /* Fase 1: konsumsi semua penawaran dari kedua pihak (pakai snapshot).
       Seluruh eksekusi (consume + grant + uang) SATU transaksi: crash di
       tengah fase tidak boleh menghilangkan/mendupe item atau uang (T3).
       consumedMask melacak yang sudah terconsume untuk refund bila gagal. */
    new consumedMask[2];
    Tx_Begin();
    for (new slot = 0; slot < MAX_INVENTORY_SLOTS; slot++)
    {
        if (!Item_IsValid(offerItemId[0][slot])) continue;
        if (!Inventory_Consume(playerid, offerItemId[0][slot], offerAmount[0][slot], "trade_out"))
        {
            Log_Format(LOG_ERROR, "[TRADE] gagal consume char=%d item=%d", ActiveCharId[playerid], offerItemId[0][slot]);
            Trade_RefundMask(playerid, partner, offerItemId, offerAmount, offerDurability, consumedMask);
            Tx_Rollback();
            Trade_Cancel(playerid, "[TRADE] Gagal eksekusi, trade dibatalkan.");
            return 1;
        }
        consumedMask[0] |= (1 << slot);
    }
    for (new slot = 0; slot < MAX_INVENTORY_SLOTS; slot++)
    {
        if (!Item_IsValid(offerItemId[1][slot])) continue;
        if (!Inventory_Consume(partner, offerItemId[1][slot], offerAmount[1][slot], "trade_out"))
        {
            Log_Format(LOG_ERROR, "[TRADE] gagal consume char=%d item=%d", ActiveCharId[partner], offerItemId[1][slot]);
            Trade_RefundMask(playerid, partner, offerItemId, offerAmount, offerDurability, consumedMask);
            Tx_Rollback();
            Trade_Cancel(playerid, "[TRADE] Gagal eksekusi, trade dibatalkan.");
            return 1;
        }
        consumedMask[1] |= (1 << slot);
    }

    /* Fase 2: berikan ke penerima dari snapshot (sudah pra-validasi muat). */
    for (new slot = 0; slot < MAX_INVENTORY_SLOTS; slot++)
    {
        if (!Item_IsValid(offerItemId[0][slot])) continue;
        if (!Inventory_Grant(partner, offerItemId[0][slot], offerAmount[0][slot], "trade_in", offerDurability[0][slot]))
        {
            /* Grant gagal = item sudah terconsume dari pemberi: WAJIB refund,
               bukan sekadar log (item hangus — audit #7). */
            Inventory_Grant(playerid, offerItemId[0][slot], offerAmount[0][slot], "trade_refund", offerDurability[0][slot]);
            Log_Format(LOG_ERROR, "[TRADE] gagal grant char=%d item=%d x%d (refund ke pemberi)", ActiveCharId[partner], offerItemId[0][slot], offerAmount[0][slot]);
        }
    }
    for (new slot = 0; slot < MAX_INVENTORY_SLOTS; slot++)
    {
        if (!Item_IsValid(offerItemId[1][slot])) continue;
        if (!Inventory_Grant(playerid, offerItemId[1][slot], offerAmount[1][slot], "trade_in", offerDurability[1][slot]))
        {
            Inventory_Grant(partner, offerItemId[1][slot], offerAmount[1][slot], "trade_refund", offerDurability[1][slot]);
            Log_Format(LOG_ERROR, "[TRADE] gagal grant char=%d item=%d x%d (refund ke pemberi)", ActiveCharId[playerid], offerItemId[1][slot], offerAmount[1][slot]);
        }
    }

    /* Fase 3: uang. */
    PlayerSurvival[playerid][psCash] -= TradeCashOffer[playerid];
    PlayerSurvival[partner][psCash] += TradeCashOffer[playerid];
    PlayerSurvival[partner][psCash] -= TradeCashOffer[partner];
    PlayerSurvival[playerid][psCash] += TradeCashOffer[partner];
    Account_SaveCharacter(playerid);
    Account_SaveCharacter(partner);
    Tx_Commit();

    new line[128];
    format(line, sizeof(line), "[TRADE] Selesai. Uang terkirim: kamu $%d, partner $%d.", TradeCashOffer[partner], TradeCashOffer[playerid]);
    SendClientMessage(playerid, UI_ACCENT_PRIMARY, line);
    format(line, sizeof(line), "[TRADE] Selesai. Uang terkirim: kamu $%d, partner $%d.", TradeCashOffer[playerid], TradeCashOffer[partner]);
    SendClientMessage(partner, UI_ACCENT_PRIMARY, line);

    Log_Format(LOG_INFO, "[TRADE] sukses char=%d (uang keluar $%d) partner=%d (uang keluar $%d)",
        ActiveCharId[playerid], TradeCashOffer[playerid], ActiveCharId[partner], TradeCashOffer[partner]);

    Trade_Reset(playerid);
    Trade_Reset(partner);
    return 1;
}

stock Trade_OnDisconnect(playerid)
{
    if (!Trade_InSession(playerid)) return 0;
    Trade_Cancel(playerid, "[TRADE] Partner terputus, trade dibatalkan.");
    return 1;
}

stock Trade_OnDialogResponse(playerid, dialogid, response, listitem, const inputtext[])
{
    if (dialogid != DIALOG_TRADE && dialogid != DIALOG_TRADE_CASH) return 0;
    if (!Trade_InSession(playerid)) return 1;

    if (dialogid == DIALOG_TRADE_CASH)
    {
        if (!response)
        {
            Trade_OpenPanel(playerid);
            return 1;
        }

        new amount = strval(inputtext);
        if (amount < 0) amount = 0;
        if (amount > TRADE_MAX_CASH) amount = TRADE_MAX_CASH;
        if (amount > PlayerSurvival[playerid][psCash]) amount = PlayerSurvival[playerid][psCash];

        TradeCashOffer[playerid] = amount;
        Trade_UnconfirmBoth(playerid);
        Trade_OpenPanel(playerid);
        return 1;
    }

    /* DIALOG_TRADE */
    if (!response) return 1;   // tutup panel tanpa membatalkan sesi
    if (!Trade_ValidateSession(playerid))
    {
        Trade_Cancel(playerid, "[TRADE] Dibatalkan: salah satu pihak pindah/downed/terputus.");
        return 1;
    }

    /* Pakai snapshot TradeRowSlot ( invariant MEMORY: baris dialog = snapshot
       id). Menghitung ulang dari inventory hidup menggeser baris saat tas
       berubah di tengah dialog → klik bisa menandai slot berbeda (F-22). */
    if (listitem < 0 || listitem >= MAX_INVENTORY_SLOTS + 2) return 1;
    new rowValue = TradeRowSlot[playerid][listitem];

    if (rowValue == TRADE_ROW_UANG)
    {
        Trade_OpenCashInput(playerid);
        return 1;
    }

    if (rowValue == TRADE_ROW_TOMBOL)
    {
        if (TradeConfirmed[playerid])
        {
            TradeConfirmed[playerid] = false;
            SendClientMessage(playerid, UI_TEXT_MUTED, "[TRADE] Konfirmasi ditarik.");
        }
        else
        {
            TradeConfirmed[playerid] = true;
            Trade_Execute(playerid);
            return 1;
        }
        Trade_OpenPanel(playerid);
        return 1;
    }

    if (rowValue >= 0 && rowValue < MAX_INVENTORY_SLOTS)
    {
        TradeOfferMask[playerid] ^= (1 << rowValue);
        Trade_UnconfirmBoth(playerid);
        Trade_OpenPanel(playerid);
    }
    return 1;
}

stock Trade_OpenCashInput(playerid)
{
    new info[96];
    format(info, sizeof(info), "Uang saat ini: $%d\nMaksimal $%d. Masukkan jumlah:", PlayerSurvival[playerid][psCash], TRADE_MAX_CASH);
    ShowPlayerDialog(playerid, DIALOG_TRADE_CASH, DIALOG_STYLE_INPUT, "Barter - Setor Uang", info, "Setor", "Kembali");
    return 1;
}
