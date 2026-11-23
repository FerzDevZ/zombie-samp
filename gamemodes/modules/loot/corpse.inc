#if defined _ls_corpse_included
    #endinput
#endif
#define _ls_corpse_included

/* Mayat elite (Brute/Stalker) = kontainer loot, bukan tumpahan objek.
   Satu mayat menyimpan maksimal 3 jenis item, digeledah lewat dialog supaya
   horde besar tidak membanjiri meja loot 300 slot. */

#define DIALOG_CORPSE 2951

forward Corpse_Tick();

enum E_CORPSE
{
    bool:cpActive,
    cpType,
    cpLobby,
    Float:cpX,
    Float:cpY,
    Float:cpZ,
    cpItem[LOOT_CORPSE_SLOTS],
    cpAmount[LOOT_CORPSE_SLOTS],
    cpCount,
    cpExpireTick,
    bool:cpScentUsed,       // sudah dipakai kamuflase bau (satu pakai per bangkai)
    STREAMER_TAG_PICKUP:cpPickup,
    STREAMER_TAG_3D_TEXT_LABEL:cpLabel
};

new Corpse[LOOT_MAX_CORPSES][E_CORPSE];
new CorpseSelected[MAX_PLAYERS];

stock Corpse_ResetPlayer(playerid)
{
    CorpseSelected[playerid] = -1;
    return 1;
}

stock Corpse_Name(type, dest[], size)
{
    if (type < 0 || type >= _:E_ZOMBIE_TYPE)
    {
        format(dest, size, "MAYAT");
        return 1;
    }
    format(dest, size, "MAYAT %s", gZombieProfiles[type][ztName]);
    return 1;
}

stock Corpse_Clear(slot)
{
    if (slot < 0 || slot >= LOOT_MAX_CORPSES) return 0;
    if (!Corpse[slot][cpActive]) return 0;

    new lockOwner = Lock_OwnerOf(LOCK_KIND_CORPSE, slot);
    if (lockOwner != INVALID_PLAYER_ID) Lock_Release(LOCK_KIND_CORPSE, slot, lockOwner);

    DestroyDynamicPickup(Corpse[slot][cpPickup]);
    DestroyDynamic3DTextLabel(Corpse[slot][cpLabel]);

    Corpse[slot][cpActive] = false;
    Corpse[slot][cpCount] = 0;
    Corpse[slot][cpScentUsed] = false;
    return 1;
}

/* Meja mayat penuh: buang yang paling dekat kedaluwarsa, bukan mayat baru. */
stock Corpse_PurgeOldest()
{
    new oldestSlot = -1;
    new oldestTick = 0x7FFFFFFF;

    for (new slot = 0; slot < LOOT_MAX_CORPSES; slot++)
    {
        if (!Corpse[slot][cpActive]) continue;
        if (Corpse[slot][cpExpireTick] >= oldestTick) continue;

        oldestTick = Corpse[slot][cpExpireTick];
        oldestSlot = slot;
    }

    if (oldestSlot == -1) return 0;
    Corpse_Clear(oldestSlot);
    return 1;
}

stock Corpse_Create(E_ZOMBIE_TYPE:type, Float:x, Float:y, Float:z, lobbyId)
{
    new slot = -1;
    for (new i = 0; i < LOOT_MAX_CORPSES; i++)
    {
        if (Corpse[i][cpActive]) continue;
        slot = i;
        break;
    }

    if (slot == -1)
    {
        if (!Corpse_PurgeOldest()) return -1;
        for (new i = 0; i < LOOT_MAX_CORPSES; i++)
        {
            if (Corpse[i][cpActive]) continue;
            slot = i;
            break;
        }
    }
    if (slot == -1) return -1;

    Corpse[slot][cpActive] = true;
    Corpse[slot][cpType] = _:type;
    Corpse[slot][cpLobby] = lobbyId;
    Corpse[slot][cpX] = x;
    Corpse[slot][cpY] = y;
    Corpse[slot][cpZ] = z;
    Corpse[slot][cpCount] = 0;
    Corpse[slot][cpExpireTick] = GetTickCount() + LOOT_CORPSE_EXPIRE_MS;
    Corpse[slot][cpScentUsed] = false;

    new tier = Loot_ZombieTier(x, y, z);
    new rolls = (type == ZT_BRUTE) ? 2 + random(2) : 2;

    for (new roll = 0; roll < rolls; roll++)
    {
        new rolledItem = ITEM_NONE_ID, rolledAmount = 1;
        if (!Loot_RollZombieTable(type, tier, rolledItem, rolledAmount)) continue;

        new stored = -1;
        for (new existing = 0; existing < Corpse[slot][cpCount]; existing++)
        {
            if (Corpse[slot][cpItem][existing] != rolledItem) continue;
            stored = existing;
            break;
        }

        if (stored != -1)
        {
            Corpse[slot][cpAmount][stored] += rolledAmount;
            continue;
        }

        if (Corpse[slot][cpCount] >= LOOT_CORPSE_SLOTS) continue;

        stored = Corpse[slot][cpCount];
        Corpse[slot][cpItem][stored] = rolledItem;
        Corpse[slot][cpAmount][stored] = rolledAmount;
        Corpse[slot][cpCount]++;
    }

    /* Sampel ekstraksi: peluang tetap per mayat elite, tidak tergantung tabel loot. */
    if (random(100) < SPECIMEN_ELITE_CHANCE && Corpse[slot][cpCount] < LOOT_CORPSE_SLOTS)
    {
        new specimenSlot = -1;
        for (new existing = 0; existing < Corpse[slot][cpCount]; existing++)
        {
            if (Corpse[slot][cpItem][existing] != ITEM_SPECIMEN) continue;
            specimenSlot = existing;
            break;
        }

        if (specimenSlot == -1)
        {
            specimenSlot = Corpse[slot][cpCount];
            Corpse[slot][cpItem][specimenSlot] = ITEM_SPECIMEN;
            Corpse[slot][cpAmount][specimenSlot] = 0;
            Corpse[slot][cpCount]++;
        }
        Corpse[slot][cpAmount][specimenSlot] += SPECIMEN_MAX_PER_CORPSE;
    }

    new corpseName[32];
    Corpse_Name(_:type, corpseName, sizeof(corpseName));

    new label[112];
    format(label, sizeof(label), "{E05A5A}%s\n{Y} geledah  /geledah", corpseName);

    new worldId = Lobby_WorldById(lobbyId);

    Corpse[slot][cpLabel] = CreateDynamic3DTextLabel(label, UI_ACCENT_DANGER,
        x, y, z + 0.35, 40.0, .worldid = worldId);

    Corpse[slot][cpPickup] = CreateDynamicPickup(LOOT_CORPSE_PICKUP_MODEL, 2,
        x, y, z, .worldid = worldId);

    Log_Format(LOG_INFO, "[CORPSE] mayat tipe=%d lobby=%d item=%d di %.1f,%.1f,%.1f",
        _:type, lobbyId, Corpse[slot][cpCount], x, y, z);

    /* Item tidak pernah hilang diam-diam: kalau semua roll gagal, mayat dibersihkan. */
    if (Corpse[slot][cpCount] <= 0)
    {
        Corpse_Clear(slot);
        return -1;
    }

    return slot;
}

stock Corpse_Nearest(playerid, Float:radius)
{
    new Float:px, Float:py, Float:pz;
    GetPlayerPos(playerid, px, py, pz);

    new bestSlot = -1;
    new Float:bestDistance = radius;
    new playerLobby = Lobby_Of(playerid);

    for (new slot = 0; slot < LOOT_MAX_CORPSES; slot++)
    {
        if (!Corpse[slot][cpActive]) continue;
        if (Corpse[slot][cpLobby] != playerLobby) continue;

        new Float:dx = Corpse[slot][cpX] - px;
        new Float:dy = Corpse[slot][cpY] - py;
        new Float:dz = Corpse[slot][cpZ] - pz;
        new Float:distance = floatsqroot(dx * dx + dy * dy + dz * dz);

        if (distance > bestDistance) continue;

        bestDistance = distance;
        bestSlot = slot;
    }
    return bestSlot;
}

stock Corpse_Show(playerid, slot)
{
    if (slot < 0 || slot >= LOOT_MAX_CORPSES) return 0;
    if (!Corpse[slot][cpActive]) return 0;
    if (Corpse[slot][cpLobby] != Lobby_Of(playerid)) return 0;

    /* Satu penggeledah pada satu waktu: tanpa kunci, dua dialog terbuka bisa
       mengambil item yang sama karena indeks bergeser setelah take pertama. */
    if (!Lock_Acquire(LOCK_KIND_CORPSE, slot, playerid)) return 0;

    CorpseSelected[playerid] = slot;

    new corpseName[32];
    Corpse_Name(Corpse[slot][cpType], corpseName, sizeof(corpseName));

    new body[512];
    format(body, sizeof(body), "{FFFFFF}%s\n{P}%d jenis barang tersisa.\n\n", corpseName, Corpse[slot][cpCount]);

    new line[96];
    for (new index = 0; index < Corpse[slot][cpCount]; index++)
    {
        new itemid = Corpse[slot][cpItem][index];
        new itemLabel[32];
        Item_GetName(itemid, itemLabel, sizeof(itemLabel));

        format(line, sizeof(line), "%s x%d\n", itemLabel, Corpse[slot][cpAmount][index]);
        strcat(body, line, sizeof(body));
    }

    ShowPlayerDialog(playerid, DIALOG_CORPSE, DIALOG_STYLE_LIST, "Geledah mayat", body, "Ambil", "Tutup");
    return 1;
}

stock Corpse_TryOpenNearest(playerid)
{
    if (!Account_IsPlaying(playerid)) return 0;

    new slot = Corpse_Nearest(playerid, LOOT_CORPSE_RANGE);
    if (slot == -1) return 0;

    return Corpse_Show(playerid, slot);
}

stock Corpse_Take(playerid, slot, listitem)
{
    if (slot < 0 || slot >= LOOT_MAX_CORPSES) return 0;
    if (!Corpse[slot][cpActive]) return 0;
    if (Corpse[slot][cpLobby] != Lobby_Of(playerid)) return 0;
    if (!Lock_IsMine(LOCK_KIND_CORPSE, slot, playerid)) return 0;

    if (listitem < 0 || listitem >= Corpse[slot][cpCount]) return 0;

    /* Jarak diperiksa ulang: dialog bisa dibiarkan terbuka lalu pemain berjalan pergi. */
    new Float:px, Float:py, Float:pz;
    GetPlayerPos(playerid, px, py, pz);
    new Float:dx = Corpse[slot][cpX] - px;
    new Float:dy = Corpse[slot][cpY] - py;
    new Float:dz = Corpse[slot][cpZ] - pz;
    if (floatsqroot(dx * dx + dy * dy + dz * dz) > LOOT_CORPSE_RANGE + 1.0)
    {
        SendClientMessage(playerid, UI_ACCENT_WARN, "[MAYAT] Terlalu jauh. Dekati dulu.");
        return 0;
    }

    new itemid = Corpse[slot][cpItem][listitem];
    new amount = Corpse[slot][cpAmount][listitem];

    if (!Inventory_Grant(playerid, itemid, amount, "corpse"))
    {
        SendClientMessage(playerid, UI_ACCENT_WARN, "[MAYAT] Tas penuh.");
        return 0;
    }

    Log_Format(LOG_INFO, "[CORPSE] char=%d ambil item=%d jumlah=%d", ActiveCharId[playerid], itemid, amount);

    /* Geser sisa item supaya tidak ada lubang di daftar dialog. */
    for (new index = listitem; index < Corpse[slot][cpCount] - 1; index++)
    {
        Corpse[slot][cpItem][index] = Corpse[slot][cpItem][index + 1];
        Corpse[slot][cpAmount][index] = Corpse[slot][cpAmount][index + 1];
    }
    Corpse[slot][cpCount]--;

    if (Corpse[slot][cpCount] <= 0)
    {
        Corpse_Clear(slot);
        SendClientMessage(playerid, UI_TEXT_MUTED, "[MAYAT] Sudah kosong.");
        return 1;
    }

    Corpse_Show(playerid, slot);
    return 1;
}

stock Corpse_OnDialogResponse(playerid, dialogid, response, listitem)
{
    if (dialogid != DIALOG_CORPSE) return 0;

    new slot = CorpseSelected[playerid];
    CorpseSelected[playerid] = -1;

    if (!response)
    {
        if (slot >= 0) Lock_Release(LOCK_KIND_CORPSE, slot, playerid);
        return 1;
    }

    /* Baris dialog: 0=nama, 1=info, 2=kosong, item mulai index 3.
       Tanpa offset ini setiap klik item ditolak (listitem >= cpCount maks 3)
       — mayat elite tidak pernah bisa digeledah (audit #2). */
    Corpse_Take(playerid, slot, listitem - 3);
    return 1;
}

public Corpse_Tick()
{
    new now = GetTickCount();
    for (new slot = 0; slot < LOOT_MAX_CORPSES; slot++)
    {
        if (!Corpse[slot][cpActive]) continue;
        if (now - Corpse[slot][cpExpireTick] < 0) continue;  /* wrap-safe */
        Corpse_Clear(slot);
    }
    return 1;
}
