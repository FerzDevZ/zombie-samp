/* Peti bersama party ("/peti").

   Kenapa modul sendiri, bukan menumpang Stash: `StashContents` di-key per playerid
   dan per karakter, jadi menukar isi array antar pemain akan merusak penyimpanan
   pribadi. Di sini container punya tabel sendiri (`party_stash`) yang di-key
   `char_id` pemimpin party, sehingga anggota lain membaca sumber yang sama.

   Anti-dupe (penting):
   - Hanya satu pemain boleh mengedit peti pada satu waktu (`Lock_Acquire`).
     Pemain lain boleh membuka tapi dialognya read-only.
   - Setiap ambil/simpan = "act then verify": item diberikan dulu, lalu SQL
     dijalankan dan `affected_rows` diperiksa. Kalau 0 (baris sudah diambil
     orang lain / berubah), pemberian dibatalkan lewat `Inventory_Consume`.
     Urutan ini tidak bisa menggandakan item, karena item hanya sah kalau baris
     DB benar-benar berubah.

   Aturan lain:
   - Hanya bisa dibuka saat berada di party.
   - Pemimpin bisa membuka di mana saja; anggota harus dalam 8 m dari pemimpin.
   - Ambil/simpan bisa sebagian (1 / 5 / semua), dibatasi berat tas dan berat peti.
   - Setiap perpindahan lewat `Inventory_Grant`/`Inventory_Consume` + ledger. */

#if defined _ls_party_stash_included
    #endinput
#endif
#define _ls_party_stash_included

#define PARTY_STASH_SLOTS        40
#define PARTY_STASH_OPEN_RANGE   8.0
#define PARTY_STASH_MAX_KG       240.0

#define DIALOG_PARTY_STASH       3401
#define DIALOG_PARTY_STASH_PUT   3402
#define DIALOG_PARTY_STASH_TAKE_A 3403
#define DIALOG_PARTY_STASH_PUT_A  3404

new PartyStashItems[MAX_PLAYERS][PARTY_STASH_SLOTS];
new PartyStashAmounts[MAX_PLAYERS][PARTY_STASH_SLOTS];
new PartyStashDurability[MAX_PLAYERS][PARTY_STASH_SLOTS];
/* Guard anti double-take: slot dengan verifikasi DB pending tidak boleh
   diambil lagi (snapshot memori yang sama pernah menghasilkan dupe — F-08). */
new bool:PartyStashTakePending[MAX_PLAYERS][PARTY_STASH_SLOTS];
new PartyStashChar[MAX_PLAYERS];
new PartyStashSelSlot[MAX_PLAYERS];
new PartyStashSelInv[MAX_PLAYERS];

forward PartyStash_OnLoaded(playerid);
forward PartyStash_OnTakeVerified(playerid, itemid, amount, slot);
forward PartyStash_OnPutVerified(playerid, itemid, amount, stashSlot);

stock PartyStash_Reset(playerid)
{
    PartyStashChar[playerid] = 0;
    PartyStashSelSlot[playerid] = -1;
    PartyStashSelInv[playerid] = -1;

    for (new slot = 0; slot < PARTY_STASH_SLOTS; slot++)
    {
        PartyStashItems[playerid][slot] = ITEM_NONE_ID;
        PartyStashAmounts[playerid][slot] = 0;
        PartyStashDurability[playerid][slot] = WEAPON_CONDITION_MAX;
        PartyStashTakePending[playerid][slot] = false;
    }
    return 1;
}

/* Pemimpin party yang petinya boleh dibuka, atau INVALID kalau tidak berhak. */
stock PartyStash_Host(playerid)
{
    new leader = Party_LeaderOf(playerid);
    if (leader == INVALID_PLAYER_ID) return INVALID_PLAYER_ID;
    if (!IsPlayerConnected(leader) || !Player_IsAlive(leader)) return INVALID_PLAYER_ID;
    if (ActiveCharId[leader] <= 0) return INVALID_PLAYER_ID;

    if (leader != playerid)
    {
        new Float:leaderX, Float:leaderY, Float:leaderZ;
        new Float:playerX, Float:playerY, Float:playerZ;
        GetPlayerPos(leader, leaderX, leaderY, leaderZ);
        GetPlayerPos(playerid, playerX, playerY, playerZ);

        if (VectorSize(leaderX - playerX, leaderY - playerY, leaderZ - playerZ) > PARTY_STASH_OPEN_RANGE)
        {
            return INVALID_PLAYER_ID;
        }
    }
    return leader;
}

stock Float:PartyStash_GetWeight(playerid)
{
    new Float:total = 0.0;
    for (new slot = 0; slot < PARTY_STASH_SLOTS; slot++)
    {
        new itemid = PartyStashItems[playerid][slot];
        if (!Item_IsValid(itemid)) continue;
        total += Item_GetWeight(itemid) * float(PartyStashAmounts[playerid][slot]);
    }
    return total;
}

stock PartyStash_CountUsed(playerid)
{
    new used = 0;
    for (new slot = 0; slot < PARTY_STASH_SLOTS; slot++)
    {
        if (Item_IsValid(PartyStashItems[playerid][slot])) used++;
    }
    return used;
}

stock PartyStash_Open(playerid)
{
    new host = PartyStash_Host(playerid);
    if (host == INVALID_PLAYER_ID)
    {
        if (!Party_InParty(playerid))
        {
            SendClientMessage(playerid, UI_ACCENT_WARN, "[PETI] Peti bersama hanya untuk party. Ketik /party.");
        }
        else
        {
            SendClientMessage(playerid, UI_ACCENT_WARN, "[PETI] Dekati pemimpin party (maks 8 m) untuk membuka peti.");
        }
        return 0;
    }

    new charId = ActiveCharId[host];

    /* Kunci dipegang pembuka; anggota lain tetap boleh membuka (read-only) supaya
       bisa melihat isi, tapi tidak bisa mengubah apa pun. */
    Lock_Acquire(LOCK_KIND_PARTY_STASH, charId, playerid);

    PartyStash_Reset(playerid);
    PartyStashChar[playerid] = charId;

    new query[192];
    format(query, sizeof(query),
        "SELECT slot, item_id, amount, durability FROM party_stash WHERE char_id = %d ORDER BY slot",
        charId);
    mysql_tquery(gSQL, query, "PartyStash_OnLoaded", "i", playerid);
    return 1;
}

public PartyStash_OnLoaded(playerid)
{
    if (!IsPlayerConnected(playerid)) return 0;

    if (mysql_errno(gSQL) != 0)
    {
        SendClientMessage(playerid, UI_ACCENT_DANGER, "[PETI] Gagal memuat peti.");
        Log_Format(LOG_ERROR, "[PETI] errno=%d", mysql_errno(gSQL));
        return 0;
    }

    for (new row = 0; row < cache_num_rows(); row++)
    {
        new slot, itemid, amount, durability;
        cache_get_value_name_int(row, "slot", slot);
        cache_get_value_name_int(row, "item_id", itemid);
        cache_get_value_name_int(row, "amount", amount);
        cache_get_value_name_int(row, "durability", durability);

        if (slot < 0 || slot >= PARTY_STASH_SLOTS) continue;
        if (!Item_IsValid(itemid)) continue;

        PartyStashItems[playerid][slot] = itemid;
        PartyStashAmounts[playerid][slot] = amount;
        PartyStashDurability[playerid][slot] = durability;
        PartyStashTakePending[playerid][slot] = false;
    }

    PartyStash_ShowContainer(playerid);
    return 1;
}

/* Baris dialog = slot berisi, urut slot; snapshot supaya isi berubah saat dialog
   terbuka tidak menggeser pilihan pemain. */
stock PartyStash_SlotFromRow(playerid, row)
{
    new seen = 0;
    for (new slot = 0; slot < PARTY_STASH_SLOTS; slot++)
    {
        new itemid = PartyStashItems[playerid][slot];
        if (!Item_IsValid(itemid)) continue;
        if (seen == row) return slot;
        seen++;
    }
    return -1;
}

stock PartyStash_InvSlotFromRow(playerid, row)
{
    new seen = 0;
    for (new slot = 0; slot < MAX_INVENTORY_SLOTS; slot++)
    {
        new itemid = PlayerInventory[playerid][slot][invItemId];
        if (!Item_IsValid(itemid)) continue;
        if (itemid == ITEM_SPECIMEN) continue;
        if (seen == row) return slot;
        seen++;
    }
    return -1;
}

stock PartyStash_CanEdit(playerid)
{
    if (PartyStashChar[playerid] <= 0) return 0;
    if (Lock_IsMine(LOCK_KIND_PARTY_STASH, PartyStashChar[playerid], playerid)) return 1;
    return 0;
}

stock PartyStash_ShowContainer(playerid)
{
    if (PartyStashChar[playerid] <= 0)
    {
        SendClientMessage(playerid, UI_TEXT_MUTED, "[PETI] Buka dulu dengan /peti.");
        return 0;
    }

    new body[1536], line[112], rows = 0;
    body[0] = EOS;
    strcat(body, "Barang\tJumlah / kondisi\n", sizeof(body));

    for (new slot = 0; slot < PARTY_STASH_SLOTS; slot++)
    {
        new itemid = PartyStashItems[playerid][slot];
        if (!Item_IsValid(itemid)) continue;

        new itemLabel[24];
        Item_GetName(itemid, itemLabel, sizeof(itemLabel));
        format(line, sizeof(line), "%s\tx%d\n", itemLabel, PartyStashAmounts[playerid][slot]);
        strcat(body, line, sizeof(body));
        rows++;
    }

    if (rows == 0) strcat(body, "(kosong)\t-\n", sizeof(body));
    strcat(body, ">> Simpan item dari tas\t\n", sizeof(body));

    new title[112], holder[24];
    if (PartyStash_CanEdit(playerid))
    {
        format(title, sizeof(title), "Peti Party %d/%d | %.1f/%.0f kg",
            rows, PARTY_STASH_SLOTS, PartyStash_GetWeight(playerid), PARTY_STASH_MAX_KG);
    }
    else
    {
        new ownerId = Lock_OwnerOf(LOCK_KIND_PARTY_STASH, PartyStashChar[playerid]);
        if (ownerId != INVALID_PLAYER_ID) GetPlayerName(ownerId, holder, sizeof(holder));
        else format(holder, sizeof(holder), "anggota lain");

        format(title, sizeof(title), "Peti Party (dibuka %s) - lihat saja", holder);
    }

    ShowPlayerDialog(playerid, DIALOG_PARTY_STASH, DIALOG_STYLE_TABLIST_HEADERS, title, body, "Pilih", "Tutup");
    return 1;
}

stock PartyStash_ShowTakeAmount(playerid, slot)
{
    new itemid = PartyStashItems[playerid][slot];
    new amount = PartyStashAmounts[playerid][slot];
    if (!Item_IsValid(itemid) || amount <= 0) return 0;

    new itemLabel[24], title[64];
    Item_GetName(itemid, itemLabel, sizeof(itemLabel));
    format(title, sizeof(title), "%s x%d", itemLabel, amount);

    PartyStashSelSlot[playerid] = slot;
    ShowPlayerDialog(playerid, DIALOG_PARTY_STASH_TAKE_A, DIALOG_STYLE_TABLIST_HEADERS, title,
        "Aksi\tEfek\nAmbil 1\tSatu buah ke tas\nAmbil 5\tLima buah ke tas\nAmbil semua\tSeluruh isi slot\nKembali\tBatal", "Pilih", "Kembali");
    return 1;
}

stock PartyStash_ShowPutAmount(playerid, invSlot)
{
    new itemid = PlayerInventory[playerid][invSlot][invItemId];
    new amount = PlayerInventory[playerid][invSlot][invAmount];
    if (!Item_IsValid(itemid) || amount <= 0) return 0;

    new itemLabel[24], title[64];
    Item_GetName(itemid, itemLabel, sizeof(itemLabel));
    format(title, sizeof(title), "%s x%d", itemLabel, amount);

    PartyStashSelInv[playerid] = invSlot;
    ShowPlayerDialog(playerid, DIALOG_PARTY_STASH_PUT_A, DIALOG_STYLE_TABLIST_HEADERS, title,
        "Aksi\tEfek\nSimpan 1\tSatu buah ke peti\nSimpan 5\tLima buah ke peti\nSimpan semua\tSeluruh tumpukan\nKembali\tBatal", "Pilih", "Kembali");
    return 1;
}

stock PartyStash_Take(playerid, slot, requestedAmount)
{
    if (slot < 0 || slot >= PARTY_STASH_SLOTS) return 0;

    if (!PartyStash_CanEdit(playerid))
    {
        SendClientMessage(playerid, UI_ACCENT_WARN, "[PETI] Peti sedang dipegang anggota lain. Coba lagi sebentar.");
        return 0;
    }

    /* Slot dengan verifikasi pending tidak boleh diambil lagi: dua Take
       cepat dari snapshot memori yang sama pernah mendupe item (F-08). */
    if (PartyStashTakePending[playerid][slot])
    {
        SendClientMessage(playerid, UI_TEXT_MUTED, "[PETI] Slot ini sedang diproses, tunggu sebentar.");
        return 0;
    }

    new itemid = PartyStashItems[playerid][slot];
    new amount = PartyStashAmounts[playerid][slot];
    if (!Item_IsValid(itemid) || amount <= 0) return 0;

    if (requestedAmount <= 0 || requestedAmount > amount) requestedAmount = amount;

    new charId = PartyStashChar[playerid];
    if (charId <= 0) return 0;

    new slotDurability = PartyStashDurability[playerid][slot];
    new itemLabel[24], line[128];

    /* Satu transaksi: grant (ledger+save tas) dan pengurangan baris peti
       commit bersama — crash di antaranya tidak boleh mendupe (T3). */
    Tx_Begin();
    if (!Inventory_Grant(playerid, itemid, requestedAmount, "party_stash_take", slotDurability))
    {
        Tx_Rollback();
        Item_GetName(itemid, itemLabel, sizeof(itemLabel));
        format(line, sizeof(line), "[PETI] Tas tidak cukup untuk %s x%d.", itemLabel, requestedAmount);
        SendClientMessage(playerid, UI_ACCENT_DANGER, line);
        return 0;
    }

    /* Kurangi cache SEGERA (jangan tunggu reload): snapshot untuk Take
       berikutnya sudah berkurang, menutup jendela double-take. */
    PartyStashAmounts[playerid][slot] -= requestedAmount;
    if (PartyStashAmounts[playerid][slot] <= 0)
    {
        PartyStashItems[playerid][slot] = ITEM_NONE_ID;
        PartyStashAmounts[playerid][slot] = 0;
    }
    PartyStashTakePending[playerid][slot] = true;

    /* Verifikasi atomik: hanya sah kalau baris DB benar-benar berkurang.
       DELETE ambil-semua wajib memakai predikat amount snapshot: kalau stack
       bertambah sejak dialog dibuka, baris tidak cocok dan grant di-rollback. */
    new query[256];
    if (requestedAmount >= amount)
    {
        format(query, sizeof(query),
            "DELETE FROM party_stash WHERE char_id = %d AND slot = %d AND item_id = %d AND amount = %d",
            charId, slot, itemid, amount);
    }
    else
    {
        format(query, sizeof(query),
            "UPDATE party_stash SET amount = amount - %d WHERE char_id = %d AND slot = %d AND item_id = %d AND amount >= %d",
            requestedAmount, charId, slot, itemid, requestedAmount);
    }

    Item_GetName(itemid, itemLabel, sizeof(itemLabel));
    format(line, sizeof(line), "[PETI] %s x%d diambil.", itemLabel, requestedAmount);
    SendClientMessage(playerid, UI_ACCENT_PRIMARY, line);

    /* Query verifikasi harus di handle yang SAMA dengan grant agar satu
       transaksi; callback tetap membaca cache_affected_rows. */
    if (Tx_IsReady())
        mysql_tquery(gTxSQL, query, "PartyStash_OnTakeVerified", "iiii", playerid, itemid, requestedAmount, slot);
    else
        mysql_tquery(gSQL, query, "PartyStash_OnTakeVerified", "iiii", playerid, itemid, requestedAmount, slot);
    Tx_Commit();
    return 1;
}

public PartyStash_OnTakeVerified(playerid, itemid, amount, slot)
{
    if (!IsPlayerConnected(playerid)) return 0;
    if (slot >= 0 && slot < PARTY_STASH_SLOTS) PartyStashTakePending[playerid][slot] = false;

    new affected = cache_affected_rows();
    if (affected <= 0)
    {
        /* Baris sudah tidak ada / berubah: batalkan pemberian. Inilah pengaman
           anti-dupe ketika dua pemain menyentuh slot yang sama. */
        if (!Inventory_Consume(playerid, itemid, amount, "party_stash_rollback"))
        {
            /* Tas berubah di antara callback: rollback gagal = potensi dupe,
               wajib jejak ERROR untuk audit item_ledger manual. */
            Log_Format(LOG_ERROR, "[PETI] ROLLBACK GAGAL char=%d item=%d amount=%d — audit item_ledger!",
                ActiveCharId[playerid], itemid, amount);
        }

        new itemLabel[24], line[160];
        Item_GetName(itemid, itemLabel, sizeof(itemLabel));
        format(line, sizeof(line), "[PETI] %s x%d sudah diambil anggota lain. Aksi dibatalkan.", itemLabel, amount);
        SendClientMessage(playerid, UI_ACCENT_WARN, line);
        Log_Format(LOG_WARN, "[PETI] rollback char=%d item=%d amount=%d", ActiveCharId[playerid], itemid, amount);
    }

    /* Reload dari DB: cache kembali sinkron dengan keadaan sebenarnya. */
    PartyStash_Open(playerid);
    return 1;
}

stock PartyStash_FreeSlot(playerid)
{
    for (new slot = 0; slot < PARTY_STASH_SLOTS; slot++)
    {
        if (!Item_IsValid(PartyStashItems[playerid][slot])) return slot;
    }
    return -1;
}

public PartyStash_OnPutVerified(playerid, itemid, amount, stashSlot)
{
    if (!IsPlayerConnected(playerid)) return 0;

    if (cache_affected_rows() <= 0)
    {
        if (stashSlot >= 0 && stashSlot < PARTY_STASH_SLOTS
            && PartyStashItems[playerid][stashSlot] == itemid)
        {
            PartyStashAmounts[playerid][stashSlot] -= amount;
            if (PartyStashAmounts[playerid][stashSlot] <= 0)
            {
                PartyStashItems[playerid][stashSlot] = ITEM_NONE_ID;
                PartyStashAmounts[playerid][stashSlot] = 0;
            }
        }
        if (!Inventory_Grant(playerid, itemid, amount, "party_stash_put_rollback"))
        {
            Log_Format(LOG_ERROR, "[PETI] PUT ROLLBACK GAGAL char=%d item=%d amount=%d — audit item_ledger!",
                ActiveCharId[playerid], itemid, amount);
        }
        SendClientMessage(playerid, UI_ACCENT_WARN, "[PETI] Gagal menyimpan ke peti (DB). Item dikembalikan ke tas.");
        Log_Format(LOG_WARN, "[PETI] put rollback char=%d item=%d amount=%d", ActiveCharId[playerid], itemid, amount);
    }
    return 1;
}

stock PartyStash_ShowPut(playerid)
{
    if (PartyStashChar[playerid] <= 0)
    {
        SendClientMessage(playerid, UI_TEXT_MUTED, "[PETI] Buka dulu dengan /peti.");
        return 0;
    }

    if (!PartyStash_CanEdit(playerid))
    {
        SendClientMessage(playerid, UI_ACCENT_WARN, "[PETI] Peti sedang dipegang anggota lain.");
        return 0;
    }

    if (PartyStash_FreeSlot(playerid) == -1)
    {
        SendClientMessage(playerid, UI_ACCENT_WARN, "[PETI] Peti penuh.");
        return 0;
    }

    new body[1536], line[112], rows = 0;
    body[0] = EOS;
    strcat(body, "Barang\tJumlah di tas\n", sizeof(body));

    for (new slot = 0; slot < MAX_INVENTORY_SLOTS; slot++)
    {
        new itemid = PlayerInventory[playerid][slot][invItemId];
        if (!Item_IsValid(itemid)) continue;
        if (itemid == ITEM_SPECIMEN) continue;   /* sampel hanya untuk ekstraksi */

        new itemLabel[24];
        Item_GetName(itemid, itemLabel, sizeof(itemLabel));
        format(line, sizeof(line), "%s\tx%d\n", itemLabel, PlayerInventory[playerid][slot][invAmount]);
        strcat(body, line, sizeof(body));
        rows++;
    }

    if (rows == 0)
    {
        SendClientMessage(playerid, UI_TEXT_MUTED, "[PETI] Tas kosong.");
        return 0;
    }

    new title[96];
    format(title, sizeof(title), "Simpan ke Peti Party | %.1f/%.0f kg",
        PartyStash_GetWeight(playerid), PARTY_STASH_MAX_KG);
    ShowPlayerDialog(playerid, DIALOG_PARTY_STASH_PUT, DIALOG_STYLE_TABLIST_HEADERS, title, body, "Pilih", "Tutup");
    return 1;
}

stock PartyStash_Put(playerid, inventorySlot, requestedAmount)
{
    if (inventorySlot < 0 || inventorySlot >= MAX_INVENTORY_SLOTS) return 0;

    if (!PartyStash_CanEdit(playerid))
    {
        SendClientMessage(playerid, UI_ACCENT_WARN, "[PETI] Peti sedang dipegang anggota lain.");
        return 0;
    }

    new itemid = PlayerInventory[playerid][inventorySlot][invItemId];
    new amount = PlayerInventory[playerid][inventorySlot][invAmount];
    if (!Item_IsValid(itemid) || amount <= 0) return 0;

    /* Specimen TIDAK boleh ditimbun di kontainer mana pun (audit F-21):
       filter hanya di tampilan bisa dilewati SelInv basi + spoof. */
    if (itemid == ITEM_SPECIMEN)
    {
        SendClientMessage(playerid, UI_ACCENT_WARN, "[PETI] Sampel zombie tidak bisa disimpan. Serahkan lewat /ekstrak.");
        return 0;
    }

    new slotDurability = PlayerInventory[playerid][inventorySlot][invDurability];

    if (requestedAmount <= 0 || requestedAmount > amount) requestedAmount = amount;

    new charId = PartyStashChar[playerid];
    if (charId <= 0) return 0;

    new Float:newWeight = PartyStash_GetWeight(playerid) + (Item_GetWeight(itemid) * float(requestedAmount));
    if (newWeight > PARTY_STASH_MAX_KG)
    {
        new line[128];
        format(line, sizeof(line), "[PETI] Berat peti akan menjadi %.1f/%.0f kg. Ambil isi dulu.", newWeight, PARTY_STASH_MAX_KG);
        SendClientMessage(playerid, UI_ACCENT_WARN, line);
        return 0;
    }

    /* Item yang ditumpuk ke slot yang sudah berisi id sama tidak butuh slot baru.
       Durability harus cocok: jerrycan 2L tidak boleh menumpuk dengan 20L.
       Hormati maxStack registry (audit #9a): tumpukan penuh → slot lain/free. */
    new maxStack = Item_GetMaxStack(itemid);
    if (maxStack < 1) maxStack = 1;
    new stashSlot = -1;
    for (new slot = 0; slot < PARTY_STASH_SLOTS; slot++)
    {
        if (PartyStashItems[playerid][slot] == itemid
            && PartyStashDurability[playerid][slot] == slotDurability
            && PartyStashAmounts[playerid][slot] + requestedAmount <= maxStack)
        {
            stashSlot = slot;
            break;
        }
    }
    if (stashSlot == -1) stashSlot = PartyStash_FreeSlot(playerid);
    if (stashSlot == -1)
    {
        SendClientMessage(playerid, UI_ACCENT_WARN, "[PETI] Peti penuh.");
        return 0;
    }

    /* Satu transaksi: consume tas + INSERT peti commit bersama (T3). */
    Tx_Begin();
    if (!Inventory_Consume(playerid, itemid, requestedAmount, "party_stash_put"))
    {
        Tx_Rollback();
        SendClientMessage(playerid, UI_ACCENT_DANGER, "[PETI] Gagal mengambil item dari tas.");
        return 0;
    }

    PartyStashItems[playerid][stashSlot] = itemid;
    PartyStashAmounts[playerid][stashSlot] += requestedAmount;
    PartyStashDurability[playerid][stashSlot] = slotDurability;

    /* Act-then-verify seperti Take: kalau INSERT gagal, item sudah terconsume
       dari tas dan harus dikembalikan, bukan lenyap tanpa jejak.
       Durability asli ikut disimpan (bukan reset 100 — audit F-07). */
    new query[256];
    format(query, sizeof(query),
        "INSERT INTO party_stash (char_id, slot, item_id, amount, durability) VALUES (%d, %d, %d, %d, %d) \
         ON DUPLICATE KEY UPDATE amount = amount + VALUES(amount)",
        charId, stashSlot, itemid, requestedAmount, slotDurability);
    if (Tx_IsReady())
        mysql_tquery(gTxSQL, query, "PartyStash_OnPutVerified", "iiii", playerid, itemid, requestedAmount, stashSlot);
    else
        mysql_tquery(gSQL, query, "PartyStash_OnPutVerified", "iiii", playerid, itemid, requestedAmount, stashSlot);
    Tx_Commit();

    new itemLabel[24], line[128];
    Item_GetName(itemid, itemLabel, sizeof(itemLabel));
    format(line, sizeof(line), "[PETI] %s x%d disimpan ke peti party.", itemLabel, requestedAmount);
    SendClientMessage(playerid, UI_ACCENT_PRIMARY, line);
    return 1;
}

stock PartyStash_Close(playerid)
{
    if (PartyStashChar[playerid] > 0)
    {
        Lock_Release(LOCK_KIND_PARTY_STASH, PartyStashChar[playerid], playerid);
    }
    PartyStash_Reset(playerid);
    return 1;
}

stock PartyStash_OnDisconnect(playerid)
{
    PartyStash_Close(playerid);
    return 1;
}

stock PartyStash_OnDialogResponse(playerid, dialogid, response, listitem, const inputtext[])
{
    #pragma unused inputtext

    if (dialogid != DIALOG_PARTY_STASH && dialogid != DIALOG_PARTY_STASH_PUT &&
        dialogid != DIALOG_PARTY_STASH_TAKE_A && dialogid != DIALOG_PARTY_STASH_PUT_A)
    {
        return 0;
    }

    if (!response)
    {
        PartyStash_Close(playerid);
        return 1;
    }

    Lock_Refresh(LOCK_KIND_PARTY_STASH, PartyStashChar[playerid], playerid);

    switch (dialogid)
    {
        case DIALOG_PARTY_STASH:
        {
            if (listitem == PartyStash_CountUsed(playerid))
            {
                PartyStash_ShowPut(playerid);
                return 1;
            }

            new slot = PartyStash_SlotFromRow(playerid, listitem);
            if (slot == -1)
            {
                PartyStash_ShowContainer(playerid);
                return 1;
            }

            if (!PartyStash_CanEdit(playerid))
            {
                SendClientMessage(playerid, UI_ACCENT_WARN, "[PETI] Peti sedang dipegang anggota lain.");
                PartyStash_ShowContainer(playerid);
                return 1;
            }

            PartyStash_ShowTakeAmount(playerid, slot);
            return 1;
        }
        case DIALOG_PARTY_STASH_TAKE_A:
        {
            new slot = PartyStashSelSlot[playerid];
            switch (listitem)
            {
                case 0: PartyStash_Take(playerid, slot, 1);
                case 1: PartyStash_Take(playerid, slot, 5);
                case 2: PartyStash_Take(playerid, slot, 0);
                default:
                {
                    PartyStash_ShowContainer(playerid);
                    return 1;
                }
            }
            if (listitem > 2) PartyStash_ShowContainer(playerid);
            return 1;
        }
        case DIALOG_PARTY_STASH_PUT:
        {
            new invSlot = PartyStash_InvSlotFromRow(playerid, listitem);
            if (invSlot == -1)
            {
                PartyStash_ShowContainer(playerid);
                return 1;
            }
            PartyStash_ShowPutAmount(playerid, invSlot);
            return 1;
        }
        case DIALOG_PARTY_STASH_PUT_A:
        {
            new invSlot = PartyStashSelInv[playerid];
            switch (listitem)
            {
                case 0: PartyStash_Put(playerid, invSlot, 1);
                case 1: PartyStash_Put(playerid, invSlot, 5);
                case 2: PartyStash_Put(playerid, invSlot, 0);
                default:
                {
                    PartyStash_ShowContainer(playerid);
                    return 1;
                }
            }
            if (listitem > 2) PartyStash_ShowContainer(playerid);
            return 1;
        }
    }
    return 1;
}