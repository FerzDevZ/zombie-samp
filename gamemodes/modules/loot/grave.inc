#if defined _ls_grave_included
    #endinput
#endif
#define _ls_grave_included

/* Mayat pemain. Mati = tas jatuh di tempat, bukan hilang.
   Pemilik punya 2 menit eksklusif sebelum orang lain boleh menggeledah.
   Semua baris disimpan ke MySQL supaya restart server tidak menghapus tas pemain. */

#define DIALOG_GRAVE       2952
#define GRAVE_SLOTS        20
#define MAX_GRAVES         16
#define GRAVE_EXPIRE_MS    600000
#define GRAVE_OWNER_MS     120000
#define GRAVE_RANGE        3.0

forward Grave_BootLoad();
forward Grave_OnLoaded();
forward Grave_Tick();

enum E_GRAVE
{
    bool:gvActive,
    gvId,
    gvCharId,
    gvLobby,
    gvWorld,
    gvOwnerName[24],
    Float:gvX,
    Float:gvY,
    Float:gvZ,
    gvItem[GRAVE_SLOTS],
    gvAmount[GRAVE_SLOTS],
    gvDurability[GRAVE_SLOTS],
    gvCount,
    gvExpireTick,
    gvOwnerFreeTick,
    STREAMER_TAG_PICKUP:gvPickup,
    STREAMER_TAG_3D_TEXT_LABEL:gvLabel
};

new Grave[MAX_GRAVES][E_GRAVE];
new GraveSelected[MAX_PLAYERS];
/* id makam yang dipilih (bukan cuma slot): slot bisa dipakai ulang makam
   lain saat dialog terbuka — respons lama tidak boleh mengambil makam baru
   (audit F-20). */
new GraveSelectedId[MAX_PLAYERS];
new GraveNextId = 1;
new bool:GraveBooted = false;
/* Id makam harus mulai di atas MAX(id) DB (termasuk baris kedaluwarsa yang
   tidak dimuat boot): counter dari 1 bisa tabrakan dan INSERT gagal senyap. */
new bool:GraveIdReady = false;
forward Grave_OnMaxIdLoaded();

stock Grave_ResetPlayer(playerid)
{
    GraveSelected[playerid] = -1;
    GraveSelectedId[playerid] = 0;
    return 1;
}

stock Grave_SpawnVisuals(slot)
{
    new worldId = Grave[slot][gvWorld];

    new label[96];
    format(label, sizeof(label), "{E05A5A}MAKAM %s\n{Y} geledah  /geledah", Grave[slot][gvOwnerName]);

    Grave[slot][gvLabel] = CreateDynamic3DTextLabel(label, UI_ACCENT_DANGER,
        Grave[slot][gvX], Grave[slot][gvY], Grave[slot][gvZ] + 0.35, 40.0, .worldid = worldId);

    Grave[slot][gvPickup] = CreateDynamicPickup(LOOT_CORPSE_PICKUP_MODEL, 2,
        Grave[slot][gvX], Grave[slot][gvY], Grave[slot][gvZ], .worldid = worldId);
    return 1;
}

stock Grave_Clear(slot)
{
    if (slot < 0 || slot >= MAX_GRAVES) return 0;
    if (!Grave[slot][gvActive]) return 0;

    /* Lock keyed id makam dilepas paksa: entitasnya hilang, lock tidak boleh
       menggantung atau mengunci makam berikutnya di slot ini (audit F-20). */
    Lock_Invalidate(LOCK_KIND_GRAVE, Grave[slot][gvId]);

    DestroyDynamicPickup(Grave[slot][gvPickup]);
    DestroyDynamic3DTextLabel(Grave[slot][gvLabel]);

    if (SQL_IsReady() && Grave[slot][gvId] > 0)
    {
        new query[96];
        mysql_format(gSQL, query, sizeof(query), "DELETE FROM graves WHERE id = %d", Grave[slot][gvId]);
        mysql_tquery(gSQL, query, "", "");
    }

    Grave[slot][gvActive] = false;
    Grave[slot][gvCount] = 0;
    return 1;
}

/* Tulis ulang seluruh isi makam: slot bergeser setiap kali item diambil,
   jadi UPDATE per baris akan meninggalkan slot yatim. */
stock Grave_PersistItems(slot)
{
    if (!SQL_IsReady() || Grave[slot][gvId] <= 0) return 0;

    /* DELETE + seluruh INSERT satu transaksi: crash di tengah tidak boleh
       meninggalkan makam dengan isi separuh. Kalau pemanggil sudah membuka
       transaksi (mis. CreateFromPlayer), ikut transaksi itu, jangan nesting. */
    new bool:ownTransaction = !Tx_InTransaction();
    if (ownTransaction) Tx_Begin();

    new query[192];
    mysql_format(gSQL, query, sizeof(query), "DELETE FROM grave_items WHERE grave_id = %d", Grave[slot][gvId]);
    Tx_Query(query);

    /* SATU multi-row INSERT, bukan 20 query per ambil (audit performa F12:
       amplifikasi 21 q/ambil memenuhi FIFO transaksi saat mass death). */
    if (Grave[slot][gvCount] > 0)
    {
        new insertQuery[1024];
        new pos = format(insertQuery, sizeof(insertQuery),
            "INSERT INTO grave_items (grave_id, slot, item_id, amount, durability) VALUES ");
        for (new stored = 0; stored < Grave[slot][gvCount]; stored++)
        {
            pos += format(insertQuery[pos], sizeof(insertQuery) - pos, "%s(%d, %d, %d, %d, %d)",
                (stored > 0) ? ", " : "",
                Grave[slot][gvId], stored, Grave[slot][gvItem][stored],
                Grave[slot][gvAmount][stored], Grave[slot][gvDurability][stored]);
        }
        Tx_Query(insertQuery);
    }

    if (ownTransaction) Tx_Commit();
    return 1;
}

stock Grave_PurgeOldest()
{
    new oldestSlot = -1;
    new oldestTick = 0x7FFFFFFF;

    for (new slot = 0; slot < MAX_GRAVES; slot++)
    {
        if (!Grave[slot][gvActive]) continue;
        if (Grave[slot][gvExpireTick] >= oldestTick) continue;

        oldestTick = Grave[slot][gvExpireTick];
        oldestSlot = slot;
    }

    if (oldestSlot == -1) return 0;
    Grave_Clear(oldestSlot);
    return 1;
}

stock Grave_AllocSlot()
{
    for (new slot = 0; slot < MAX_GRAVES; slot++)
    {
        if (!Grave[slot][gvActive]) return slot;
    }

    if (!Grave_PurgeOldest()) return -1;

    for (new slot = 0; slot < MAX_GRAVES; slot++)
    {
        if (!Grave[slot][gvActive]) return slot;
    }
    return -1;
}

/* Dipanggil SEBELUM Inventory_Clear supaya tidak ada item yang menguap. */
stock Grave_CreateFromPlayer(playerid)
{
    if (!Account_IsPlaying(playerid)) return -1;
    if (ActiveCharId[playerid] == 0) return -1;

    new itemCount = 0;
    for (new slot = 0; slot < MAX_INVENTORY_SLOTS; slot++)
    {
        if (!Item_IsValid(PlayerInventory[playerid][slot][invItemId])) continue;
        if (PlayerInventory[playerid][slot][invAmount] <= 0) continue;
        itemCount++;
    }
    if (itemCount <= 0) return -1;

    new graveSlot = Grave_AllocSlot();
    if (graveSlot == -1) return -1;

    new Float:x, Float:y, Float:z;
    GetPlayerPos(playerid, x, y, z);

    Grave[graveSlot][gvActive] = true;
    Grave[graveSlot][gvCharId] = ActiveCharId[playerid];
    Grave[graveSlot][gvLobby] = Lobby_Of(playerid);
    Grave[graveSlot][gvWorld] = GetPlayerVirtualWorld(playerid);
    Grave[graveSlot][gvX] = x;
    Grave[graveSlot][gvY] = y;
    Grave[graveSlot][gvZ] = z;
    Grave[graveSlot][gvCount] = 0;
    Grave[graveSlot][gvExpireTick] = GetTickCount() + GRAVE_EXPIRE_MS;
    /* Epoch detik (bukan tick): bertahan restart via kolom owner_free_at. */
    Grave[graveSlot][gvOwnerFreeTick] = gettime() + (GRAVE_OWNER_MS / 1000);

    GetPlayerName(playerid, Grave[graveSlot][gvOwnerName], 24);

    for (new slot = 0; slot < MAX_INVENTORY_SLOTS; slot++)
    {
        new itemid = PlayerInventory[playerid][slot][invItemId];
        if (!Item_IsValid(itemid)) continue;

        new amount = PlayerInventory[playerid][slot][invAmount];
        if (amount <= 0) continue;

        if (Grave[graveSlot][gvCount] >= GRAVE_SLOTS) break;

        new stored = Grave[graveSlot][gvCount];
        Grave[graveSlot][gvItem][stored] = itemid;
        Grave[graveSlot][gvAmount][stored] = amount;
        Grave[graveSlot][gvDurability][stored] = PlayerInventory[playerid][slot][invDurability];
        Grave[graveSlot][gvCount]++;
    }

    Grave_SpawnVisuals(graveSlot);

    Grave[graveSlot][gvId] = GraveNextId++;

    /* Sebelum MAX(id) DB diketahui, jangan INSERT: id bisa tabrakan dengan
       baris kedaluwarsa. Makam tetap hidup di memori sampai expire. */
    if (SQL_IsReady() && GraveIdReady)
    {
        new minutes = GRAVE_EXPIRE_MS / 1000;
        new query[320];
        /* Baris makam + isinya satu transaksi (PersistItems ikut TxOpen). */
        Tx_Begin();
        mysql_format(gSQL, query, sizeof(query),
            "INSERT INTO graves (id, char_id, owner_name, lobby_id, world_id, pos_x, pos_y, pos_z, expires_at, created_at, owner_free_at) \
             VALUES (%d, %d, '%e', %d, %d, %f, %f, %f, %d, %d, %d)",
            Grave[graveSlot][gvId], Grave[graveSlot][gvCharId], Grave[graveSlot][gvOwnerName],
            Grave[graveSlot][gvLobby], Grave[graveSlot][gvWorld], x, y, z, gettime() + minutes, gettime(),
            Grave[graveSlot][gvOwnerFreeTick]);
        Tx_Query(query);

        Grave_PersistItems(graveSlot);
        Tx_Commit();
    }

    Log_Format(LOG_INFO, "[GRAVE] char=%d lobby=%d item=%d di %.1f,%.1f,%.1f",
        Grave[graveSlot][gvCharId], Grave[graveSlot][gvLobby], Grave[graveSlot][gvCount], x, y, z);
    return graveSlot;
}

public Grave_BootLoad()
{
    if (GraveBooted) return 0;
    if (!SQL_IsReady())
    {
        SetTimer("Grave_BootLoad", 2000, false);
        return 0;
    }
    GraveBooted = true;

    /* Purge baris kedaluwarsa (audit T11): boot load hanya memfilter
       expires_at > now tanpa pernah menghapus — tabel tumbuh tanpa batas.
       Items dulu (FK), baru graves. */
    mysql_tquery(gSQL, "DELETE gi FROM grave_items gi JOIN graves g ON gi.grave_id = g.id WHERE g.expires_at <= UNIX_TIMESTAMP()", "", "");
    mysql_tquery(gSQL, "DELETE FROM graves WHERE expires_at <= UNIX_TIMESTAMP()", "", "");

    mysql_tquery(gSQL, "SELECT COALESCE(MAX(id), 0) AS maxId FROM graves", "Grave_OnMaxIdLoaded", "");
    mysql_tquery(gSQL,
        "SELECT g.id, g.char_id, g.owner_name, g.lobby_id, g.world_id, g.pos_x, g.pos_y, g.pos_z, g.expires_at, g.owner_free_at, \
                i.item_id, i.amount, i.durability \
         FROM graves g LEFT JOIN grave_items i ON i.grave_id = g.id \
         WHERE g.expires_at > UNIX_TIMESTAMP() ORDER BY g.id, i.slot",
        "Grave_OnLoaded", "");
    return 1;
}

public Grave_OnMaxIdLoaded()
{
    new maxId = 0;
    if (cache_num_rows() > 0) cache_get_value_name_int(0, "maxId", maxId);
    if (maxId >= GraveNextId) GraveNextId = maxId + 1;
    GraveIdReady = true;
    return 1;
}

public Grave_OnLoaded()
{
    new rows = cache_num_rows();
    new now = gettime();

    for (new row = 0; row < rows; row++)
    {
        new graveId, charId, lobbyId, worldId, expiresAt, ownerFreeAt;
        new ownerName[24];
        new Float:x, Float:y, Float:z;

        cache_get_value_name_int(row, "id", graveId);
        cache_get_value_name_int(row, "char_id", charId);
        cache_get_value_name_int(row, "lobby_id", lobbyId);
        cache_get_value_name_int(row, "world_id", worldId);
        cache_get_value_name_int(row, "expires_at", expiresAt);
        cache_get_value_name_int(row, "owner_free_at", ownerFreeAt);
        cache_get_value_name(row, "owner_name", ownerName, sizeof(ownerName));
        cache_get_value_name_float(row, "pos_x", x);
        cache_get_value_name_float(row, "pos_y", y);
        cache_get_value_name_float(row, "pos_z", z);

        new graveSlot = -1;
        for (new slot = 0; slot < MAX_GRAVES; slot++)
        {
            if (!Grave[slot][gvActive]) continue;
            if (Grave[slot][gvId] != graveId) continue;
            graveSlot = slot;
            break;
        }

        if (graveSlot == -1)
        {
            graveSlot = Grave_AllocSlot();
            if (graveSlot == -1) continue;

            Grave[graveSlot][gvActive] = true;
            Grave[graveSlot][gvId] = graveId;
            Grave[graveSlot][gvCharId] = charId;
            Grave[graveSlot][gvLobby] = lobbyId;
            Grave[graveSlot][gvWorld] = worldId;
            Grave[graveSlot][gvX] = x;
            Grave[graveSlot][gvY] = y;
            Grave[graveSlot][gvZ] = z;
            Grave[graveSlot][gvCount] = 0;
            Grave[graveSlot][gvExpireTick] = GetTickCount() + (expiresAt - now) * 1000;
            /* Eksklusivitas pemilik bertahan restart (audit F-20). */
            Grave[graveSlot][gvOwnerFreeTick] = ownerFreeAt;

            format(Grave[graveSlot][gvOwnerName], 24, "%s", ownerName);
            Grave_SpawnVisuals(graveSlot);

            if (graveId >= GraveNextId) GraveNextId = graveId + 1;
        }

        new itemid, amount, durability;
        cache_get_value_name_int(row, "item_id", itemid);
        cache_get_value_name_int(row, "amount", amount);
        cache_get_value_name_int(row, "durability", durability);

        if (!Item_IsValid(itemid) || amount <= 0) continue;
        if (Grave[graveSlot][gvCount] >= GRAVE_SLOTS) continue;

        new stored = Grave[graveSlot][gvCount];
        Grave[graveSlot][gvItem][stored] = itemid;
        Grave[graveSlot][gvAmount][stored] = amount;
        Grave[graveSlot][gvDurability][stored] = durability;
        Grave[graveSlot][gvCount]++;
    }

    new loaded = 0;
    for (new slot = 0; slot < MAX_GRAVES; slot++)
    {
        if (Grave[slot][gvActive]) loaded++;
    }

    Log_Format(LOG_INFO, "[GRAVE] %d makam dimuat dari database.", loaded);
    return 1;
}

stock Grave_Nearest(playerid, Float:radius)
{
    new Float:px, Float:py, Float:pz;
    GetPlayerPos(playerid, px, py, pz);

    new bestSlot = -1;
    new Float:bestDistance = radius;
    new playerLobby = Lobby_Of(playerid);
    new playerWorld = GetPlayerVirtualWorld(playerid);
    new charId = ActiveCharId[playerid];

    for (new slot = 0; slot < MAX_GRAVES; slot++)
    {
        if (!Grave[slot][gvActive]) continue;
        if (Grave[slot][gvLobby] != playerLobby) continue;
        if (Grave[slot][gvWorld] != playerWorld) continue;

        /* Eksklusif untuk pemilik selama 2 menit, setelah itu bebas diambil. */
        if (gettime() < Grave[slot][gvOwnerFreeTick] && Grave[slot][gvCharId] != charId) continue;

        new Float:dx = Grave[slot][gvX] - px;
        new Float:dy = Grave[slot][gvY] - py;
        new Float:dz = Grave[slot][gvZ] - pz;
        new Float:distance = floatsqroot(dx * dx + dy * dy + dz * dz);

        if (distance > bestDistance) continue;

        bestDistance = distance;
        bestSlot = slot;
    }
    return bestSlot;
}

stock Grave_Show(playerid, slot)
{
    if (slot < 0 || slot >= MAX_GRAVES) return 0;
    if (!Grave[slot][gvActive]) return 0;
    if (Grave[slot][gvLobby] != Lobby_Of(playerid)) return 0;
    if (Grave[slot][gvWorld] != GetPlayerVirtualWorld(playerid)) return 0;

    /* Satu penggedah pada satu waktu: dua pemain yang melihat daftar bersamaan
       akan kehilangan indeks baris ketika isi makam bergeser. Lock keyed id
       makam (bukan slot) supaya slot reuse tidak mengunci makam yang salah. */
    if (!Lock_Acquire(LOCK_KIND_GRAVE, Grave[slot][gvId], playerid)) return 0;

    /* Eksklusivitas pemilik dicek ulang di sini (bukan hanya di pencarian
       terdekat): spoof dialog tidak boleh melewati jendela 2 menit (F-20). */
    if (Grave[slot][gvCharId] != ActiveCharId[playerid] && gettime() < Grave[slot][gvOwnerFreeTick])
    {
        Lock_Release(LOCK_KIND_GRAVE, Grave[slot][gvId], playerid);
        new waitLine[96];
        format(waitLine, sizeof(waitLine), "[MAKAM] Masih eksklusif untuk pemiliknya. Tunggu %d detik.",
            Grave[slot][gvOwnerFreeTick] - gettime());
        SendClientMessage(playerid, UI_ACCENT_WARN, waitLine);
        return 0;
    }

    GraveSelected[playerid] = slot;
    GraveSelectedId[playerid] = Grave[slot][gvId];

    new body[1024], line[96], itemLabel[32];
    body[0] = EOS;
    strcat(body, "Barang\tJumlah\n", sizeof(body));

    for (new index = 0; index < Grave[slot][gvCount]; index++)
    {
        Item_GetName(Grave[slot][gvItem][index], itemLabel, sizeof(itemLabel));
        format(line, sizeof(line), "%s\tx%d\n", itemLabel, Grave[slot][gvAmount][index]);
        strcat(body, line, sizeof(body));
    }

    new title[64];
    format(title, sizeof(title), "Makam %s | %d jenis", Grave[slot][gvOwnerName], Grave[slot][gvCount]);
    ShowPlayerDialog(playerid, DIALOG_GRAVE, DIALOG_STYLE_TABLIST_HEADERS, title, body, "Ambil", "Tutup");
    return 1;
}

stock Grave_TryOpenNearest(playerid)
{
    if (!Account_IsPlaying(playerid)) return 0;

    new slot = Grave_Nearest(playerid, GRAVE_RANGE);
    if (slot == -1) return 0;

    return Grave_Show(playerid, slot);
}

stock Grave_Take(playerid, slot, listitem)
{
    if (slot < 0 || slot >= MAX_GRAVES) return 0;
    if (!Grave[slot][gvActive]) return 0;
    if (Grave[slot][gvLobby] != Lobby_Of(playerid)) return 0;
    if (Grave[slot][gvWorld] != GetPlayerVirtualWorld(playerid)) return 0;
    if (listitem < 0 || listitem >= Grave[slot][gvCount]) return 0;

    if (!Lock_IsMine(LOCK_KIND_GRAVE, Grave[slot][gvId], playerid))
    {
        SendClientMessage(playerid, UI_ACCENT_WARN, "[MAKAM] Makam ini sedang digeledah pemain lain.");
        return 0;
    }

    /* Jendela eksklusif pemilik dicek ulang saat ambil (audit F-20). */
    if (Grave[slot][gvCharId] != ActiveCharId[playerid] && gettime() < Grave[slot][gvOwnerFreeTick])
    {
        SendClientMessage(playerid, UI_ACCENT_WARN, "[MAKAM] Masih eksklusif untuk pemiliknya. Tunggu sebentar.");
        return 0;
    }

    new Float:px, Float:py, Float:pz;
    GetPlayerPos(playerid, px, py, pz);
    new Float:dx = Grave[slot][gvX] - px;
    new Float:dy = Grave[slot][gvY] - py;
    new Float:dz = Grave[slot][gvZ] - pz;
    if (floatsqroot(dx * dx + dy * dy + dz * dz) > GRAVE_RANGE + 1.0)
    {
        SendClientMessage(playerid, UI_ACCENT_WARN, "[MAKAM] Terlalu jauh. Dekati dulu.");
        return 0;
    }

    new itemid = Grave[slot][gvItem][listitem];
    new amount = Grave[slot][gvAmount][listitem];

    /* Satu transaksi untuk grant + persist makam: crash di antara keduanya
       tidak boleh mendupe item (tas + makam). Grant memakai durability asli
       (bukan reset 100 — audit F-07). */
    Tx_Begin();
    if (!Inventory_Grant(playerid, itemid, amount, "grave", Grave[slot][gvDurability][listitem]))
    {
        Tx_Rollback();
        SendClientMessage(playerid, UI_ACCENT_WARN, "[MAKAM] Tas penuh.");
        return 0;
    }

    Log_Format(LOG_INFO, "[GRAVE] char=%d ambil item=%d jumlah=%d", ActiveCharId[playerid], itemid, amount);

    for (new index = listitem; index < Grave[slot][gvCount] - 1; index++)
    {
        Grave[slot][gvItem][index] = Grave[slot][gvItem][index + 1];
        Grave[slot][gvAmount][index] = Grave[slot][gvAmount][index + 1];
        Grave[slot][gvDurability][index] = Grave[slot][gvDurability][index + 1];
    }
    Grave[slot][gvCount]--;

    Grave_PersistItems(slot);
    Tx_Commit();

    if (Grave[slot][gvCount] <= 0)
    {
        Grave_Clear(slot);
        SendClientMessage(playerid, UI_TEXT_MUTED, "[MAKAM] Sudah kosong.");
        return 1;
    }

    Grave_Show(playerid, slot);
    return 1;
}

stock Grave_OnDialogResponse(playerid, dialogid, response, listitem)
{
    if (dialogid != DIALOG_GRAVE) return 0;

    new slot = GraveSelected[playerid];
    new selectedId = GraveSelectedId[playerid];
    GraveSelected[playerid] = -1;
    GraveSelectedId[playerid] = 0;

    /* Slot bisa sudah dipakai ulang makam lain sejak dialog dibuka: respons
       hanya sah bila id makam masih sama (audit F-20). */
    if (slot < 0 || !Grave[slot][gvActive] || Grave[slot][gvId] != selectedId)
    {
        if (slot >= 0 && Grave[slot][gvActive]) Lock_Release(LOCK_KIND_GRAVE, Grave[slot][gvId], playerid);
        SendClientMessage(playerid, UI_TEXT_MUTED, "[MAKAM] Makam itu sudah tidak ada.");
        return 1;
    }

    if (!response)
    {
        Lock_Release(LOCK_KIND_GRAVE, selectedId, playerid);
        return 1;
    }

    /* Kunci sengaja TIDAK dilepas saat mengambil: dialog langsung dibuka ulang
       untuk isi yang tersisa dan kunci harus tetap di tangan penggedah yang sama. */
    Grave_Take(playerid, slot, listitem);
    return 1;
}

public Grave_Tick()
{
    new now = GetTickCount();
    for (new slot = 0; slot < MAX_GRAVES; slot++)
    {
        if (!Grave[slot][gvActive]) continue;
        if (now - Grave[slot][gvExpireTick] < 0) continue;  /* wrap-safe */
        Grave_Clear(slot);
    }
    return 1;
}
