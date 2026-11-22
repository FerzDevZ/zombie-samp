#if defined _ls_loot_drops_included
    #endinput
#endif
#define _ls_loot_drops_included

#define MAX_LOOT_DROPS 300
#define LOOT_DROP_EXPIRE_MS 180000
/* Drop milik sendiri eksklusif 60 detik (biar tidak "dicuri" saat masih di tempat),
   setelah itu siapa pun boleh ambil supaya item tidak menjadi sampah permanen. */
#define LOOT_OWNER_LOCK_MS  60000

forward Loot_ExpireTick();
forward Corpse_Create(E_ZOMBIE_TYPE:type, Float:x, Float:y, Float:z, lobbyId);

enum E_LOOT_DROP
{
    bool:ldActive,
    ldItemId,
    ldAmount,
    STREAMER_TAG_OBJECT:ldObject,
    STREAMER_TAG_PICKUP:ldPickup,
    STREAMER_TAG_MAP_ICON:ldMapIcon,
    STREAMER_TAG_3D_TEXT_LABEL:ldLabel,
    Float:ldX,
    Float:ldY,
    Float:ldZ,
    ldOwnerPlayer,
    ldOwnerUntilTick,
    ldExpireTick,
    ldLobby,
    ldWorld,
    ldDurability,
    ldKind,
    ldTier          // LOOT_TIER_*: peti biasa vs langka
};

new LootDrop[MAX_LOOT_DROPS][E_LOOT_DROP];

/* Dipakai /uicheck: pemain melaporkan "cuma ada lootingan" dan kita butuh
   angka nyata berapa peti yang benar-benar hidup di sekitar posisi itu. */
stock LootDrop_CountNear(Float:x, Float:y, Float:radius)
{
    new found = 0;
    for (new slot = 0; slot < MAX_LOOT_DROPS; slot++)
    {
        if (!LootDrop[slot][ldActive]) continue;
        if (VectorSize(x - LootDrop[slot][ldX], y - LootDrop[slot][ldY], 0.0) > radius) continue;
        found++;
    }
    return found;
}
new LootDropCount = 0;

/* Kalau meja loot penuh, buang drop paling tua supaya drop baru tidak hilang diam-diam
   (mis. horde besar menumpuk mayat lebih cepat daripada expire 3 menit). */
stock Loot_PurgeOldest()
{
    new oldestSlot = -1;
    new oldestTick = 0x7FFFFFFF;

    for (new slot = 0; slot < MAX_LOOT_DROPS; slot++)
    {
        if (!LootDrop[slot][ldActive]) continue;
        if (LootDrop[slot][ldExpireTick] >= oldestTick) continue;

        oldestTick = LootDrop[slot][ldExpireTick];
        oldestSlot = slot;
    }

    if (oldestSlot == -1) return 0;
    Loot_RemoveDrop(oldestSlot);
    return 1;
}

stock bool:Loot_HasFreeSlot()
{
    for (new slot = 0; slot < MAX_LOOT_DROPS; slot++)
    {
        if (!LootDrop[slot][ldActive]) return true;
    }
    return false;
}

/* Warna & tag kategori: drop di tanah harus bisa dibaca sekilas, tanpa
   membuka inventory. Kategori menentukan warna label 3D dan teks tag. */
stock Loot_VisualColor(itemid)
{
    switch (Item_GetCategory(itemid))
    {
        case ITEM_CONSUMABLE: return 0x7CFC00FF;
        case ITEM_WEAPON:     return UI_ACCENT_DANGER;
        case ITEM_TOOL:       return UI_ACCENT_INFO;
        case ITEM_BLUEPRINT:  return 0xA78BFAFF;
        case ITEM_MATERIAL:   return 0xCBD5E1FF;
    }
    return UI_TEXT_PRIMARY;
}

stock Loot_VisualTag(itemid, dest[], size)
{
    switch (Item_GetCategory(itemid))
    {
        case ITEM_CONSUMABLE: return format(dest, size, "OBAT");
        case ITEM_WEAPON:     return format(dest, size, "AMMO");
        case ITEM_TOOL:       return format(dest, size, "ALAT");
        case ITEM_BLUEPRINT:  return format(dest, size, "PLAN");
        case ITEM_MATERIAL:   return format(dest, size, "BAHAN");
    }
    return format(dest, size, "ITEM");
}

stock Loot_Spawn(Float:x, Float:y, Float:z, itemid, amount, ownerPlayer = INVALID_PLAYER_ID, lobbyId = -1, kind = LOOT_KIND_ITEM, durability = WEAPON_CONDITION_MAX, tier = LOOT_TIER_NORMAL)
{
    if (!Item_IsValid(itemid) || amount <= 0) return -1;

    new slot = -1;
    for (new i = 0; i < MAX_LOOT_DROPS; i++)
    {
        if (LootDrop[i][ldActive]) continue;
        slot = i;
        break;
    }

    if (slot == -1)
    {
        if (!Loot_PurgeOldest()) return -1;
        for (new i = 0; i < MAX_LOOT_DROPS; i++)
        {
            if (LootDrop[i][ldActive]) continue;
            slot = i;
            break;
        }
    }
    if (slot == -1) return -1;

    new itemLabel[32];
    Item_GetName(itemid, itemLabel, sizeof(itemLabel));

    if (lobbyId < 0) lobbyId = (ownerPlayer != INVALID_PLAYER_ID) ? Lobby_Of(ownerPlayer) : 0;
    if (!Lobby_IsValid(lobbyId)) lobbyId = 0;

    LootDrop[slot][ldActive] = true;
    LootDrop[slot][ldItemId] = itemid;
    LootDrop[slot][ldAmount] = amount;
    LootDrop[slot][ldOwnerPlayer] = ownerPlayer;
    LootDrop[slot][ldOwnerUntilTick] = (ownerPlayer == INVALID_PLAYER_ID) ? 0 : (GetTickCount() + LOOT_OWNER_LOCK_MS);
    LootDrop[slot][ldExpireTick] = GetTickCount() + LOOT_DROP_EXPIRE_MS;
    LootDrop[slot][ldX] = x;
    LootDrop[slot][ldY] = y;
    LootDrop[slot][ldZ] = z;
    LootDrop[slot][ldLobby] = lobbyId;
    LootDrop[slot][ldKind] = kind;
    LootDrop[slot][ldDurability] = durability;
    LootDrop[slot][ldTier] = tier;
    LootDrop[slot][ldMapIcon] = INVALID_STREAMER_ID;
    LootDrop[slot][ldObject] = INVALID_STREAMER_ID;

    new worldId = Lobby_WorldById(lobbyId);
    LootDrop[slot][ldWorld] = worldId;
    new label[96];

    if (kind == LOOT_KIND_CRATE)
    {
        /* Peti darurat: koper lapangan + ikon peta. Sebelumnya model 1225 yang
           di GTA SA adalah tong minyak, jadi peti terlihat seperti drum. */
        new Float:crateZ = (tier == LOOT_TIER_RARE) ? (z - 0.35) : (z - 0.55);
        LootDrop[slot][ldObject] = CreateDynamicObject(LOOT_CRATE_OBJECT_MODEL,
            x, y, crateZ, 0.0, 0.0, (tier == LOOT_TIER_RARE) ? 45.0 : 18.0, .worldid = worldId);

        if (tier == LOOT_TIER_RARE)
        {
            /* Emas + tanda LANGKA: satu-satunya peti yang boleh memakai warna emas
               di dunia, jadi dari jauh pun langsung terbaca sebagai hadiah. */
            format(label, sizeof(label), "{FFD24A}[PETI LANGKA] {FFFFFF}%s x%d\n{FFD24A}/ambil  (Y)", itemLabel, amount);
        }
        else
        {
            format(label, sizeof(label), "{38BDF8}[PETI] {FFFFFF}%s x%d\n{94A3B8}/ambil  (Y)", itemLabel, amount);
        }

        new labelColor = (tier == LOOT_TIER_RARE) ? LOOT_CRATE_COLOR_RARE : LOOT_CRATE_COLOR_NORMAL;
        new Float:labelDist = (tier == LOOT_TIER_RARE) ? LOOT_CRATE_LABEL_DIST_RARE : LOOT_CRATE_LABEL_DIST;
        LootDrop[slot][ldLabel] = CreateDynamic3DTextLabel(label, labelColor,
            x, y, z + 0.45, labelDist, .worldid = worldId);

        LootDrop[slot][ldPickup] = CreateDynamicPickup(LOOT_CRATE_PICKUP_MODEL, 2,
            x, y, z, .worldid = worldId, .playerid = ownerPlayer);

        // Ikon peta: biru = peti biasa, emas = peti langka.
        LootDrop[slot][ldMapIcon] = CreateDynamicMapIcon(x, y, z - 0.8,
            (tier == LOOT_TIER_RARE) ? LOOT_CRATE_ICON_RARE : LOOT_CRATE_ICON_NORMAL,
            labelColor, worldId, 0, -1, 300.0, MAPICON_LOCAL);
    }
    else
    {
        /* Drop item tidak memakai objek 3D sendiri: model asli item (tong sampah,
           kontainer gudang, botol) membuat drop terlihat seperti drum/tong.
           Satu pickup koper 1210 + label kategori berwarna jauh lebih terbaca,
           dan memangkas 1 objek dinamis per drop (maks 300). */
        LootDrop[slot][ldObject] = INVALID_STREAMER_ID;

        new tag[8], tagColor[8];
        Loot_VisualTag(itemid, tag, sizeof(tag));
        format(tagColor, sizeof(tagColor), "{%06X}", (Loot_VisualColor(itemid) >> 8) & 0xFFFFFF);

        if (ownerPlayer != INVALID_PLAYER_ID)
        {
            format(label, sizeof(label), "%s[%s] {FFFFFF}%s x%d\n{FFD24A}milikmu 60s{FFFFFF}  /ambil",
                tagColor, tag, itemLabel, amount);
        }
        else
        {
            format(label, sizeof(label), "%s[%s] {FFFFFF}%s x%d\n{94A3B8}/ambil  (Y)", tagColor, tag, itemLabel, amount);
        }

        LootDrop[slot][ldLabel] = CreateDynamic3DTextLabel(label, Loot_VisualColor(itemid),
            x, y, z + 0.35, 10.0, .worldid = worldId);

        // Pickup khusus pemain pemilik: tidak bocor sebagai ESP loot global.
        // Tipe 2 = terambil saat pemain berjalan melewatinya.
        LootDrop[slot][ldPickup] = CreateDynamicPickup(LOOT_ITEM_PICKUP_MODEL, 2,
            x, y, z, .worldid = worldId, .playerid = ownerPlayer);
    }

    LootDropCount++;
    return slot;
}

stock Loot_RemoveDrop(slot)
{
    if (slot < 0 || slot >= MAX_LOOT_DROPS) return 0;
    if (!LootDrop[slot][ldActive]) return 0;

    if (LootDrop[slot][ldObject] != INVALID_STREAMER_ID)
    {
        DestroyDynamicObject(LootDrop[slot][ldObject]);
        LootDrop[slot][ldObject] = INVALID_STREAMER_ID;
    }
    DestroyDynamic3DTextLabel(LootDrop[slot][ldLabel]);
    DestroyDynamicPickup(LootDrop[slot][ldPickup]);
    if (LootDrop[slot][ldMapIcon] != INVALID_STREAMER_ID)
    {
        DestroyDynamicMapIcon(LootDrop[slot][ldMapIcon]);
        LootDrop[slot][ldMapIcon] = INVALID_STREAMER_ID;
    }

    LootDrop[slot][ldActive] = false;
    LootDropCount--;
    if (LootDropCount < 0) LootDropCount = 0;
    return 1;
}

stock Loot_GetDropAtPickup(STREAMER_TAG_PICKUP:pickupid)
{
    for (new slot = 0; slot < MAX_LOOT_DROPS; slot++)
    {
        if (!LootDrop[slot][ldActive]) continue;
        if (LootDrop[slot][ldPickup] == pickupid) return slot;
    }
    return -1;
}

/* Roll satu tabel berbobot. Dipakai loot zombie (config/loot_tables.inc). */
stock bool:Loot_RollTable(const entries[][E_LOOT_ENTRY], slotCount, &itemid, &amount)
{
    new totalWeight = 0;
    for (new slot = 0; slot < slotCount; slot++)
    {
        if (entries[slot][leWeight] > 0) totalWeight += entries[slot][leWeight];
    }
    if (totalWeight <= 0) return false;

    new pick = random(totalWeight);
    new running = 0;

    for (new slot = 0; slot < slotCount; slot++)
    {
        new weight = entries[slot][leWeight];
        if (weight <= 0) continue;

        running += weight;
        if (pick >= running) continue;

        itemid = entries[slot][leItem];
        new minimum = entries[slot][leMin];
        new maximum = entries[slot][leMax];
        if (maximum < minimum) maximum = minimum;
        amount = minimum + random(maximum - minimum + 1);
        return true;
    }
    return false;
}

/* Jumlah roll per tipe. Walker bisa kosong (zombie paling umum tidak selalu membawa
   apa pun), elite memberi 2-3 roll supaya terasa sepadan. */
stock Loot_DropCountFor(E_ZOMBIE_TYPE:type)
{
    switch (type)
    {
        case ZT_WALKER:  return (random(100) < 10) ? 0 : 1 + random(3);
        case ZT_RUNNER:  return 1 + random(2);
        case ZT_CRAWLER: return (random(100) < 10) ? 0 : 1 + random(2);
        case ZT_BRUTE:   return 3 + random(2);
        case ZT_BLOATER: return 1 + random(3);
        case ZT_SCREAMER:return 1 + random(2);
        case ZT_STALKER: return 2 + random(2);
    }
    return 1;
}

stock Loot_ZombieTier(Float:x, Float:y, Float:z)
{
    new zone = Zone_GetIdAt(x, y, z);
    if (zone < 0 || zone >= MAX_ZONE_COUNT) return 1;

    new tier = Zone_GetTier(zone);
    if (tier < 1) return 1;
    if (tier > 3) return 3;
    return tier;
}

/* Roll tabel zombie dengan bonus tier zona. Tabel dasar disalin dulu supaya
   gZombieLoot tetap read-only dan tidak pernah termutasi di runtime. */
stock bool:Loot_RollZombieTable(E_ZOMBIE_TYPE:type, tier, &itemid, &amount)
{
    new entries[LOOT_TABLE_SLOTS][E_LOOT_ENTRY];

    for (new slot = 0; slot < LOOT_TABLE_SLOTS; slot++)
    {
        entries[slot][leItem] = gZombieLoot[_:type][slot][leItem];
        entries[slot][leWeight] = gZombieLoot[_:type][slot][leWeight];
        entries[slot][leMin] = gZombieLoot[_:type][slot][leMin];
        entries[slot][leMax] = gZombieLoot[_:type][slot][leMax];
    }

    if (tier >= 2)
    {
        new bonusRow = tier - 2;
        if (bonusRow > 1) bonusRow = 1;

        for (new bonusSlot = 0; bonusSlot < 2; bonusSlot++)
        {
            new replaceSlot = LOOT_TABLE_SLOTS - 2 + bonusSlot;
            entries[replaceSlot][leItem] = gTierBonusLoot[bonusRow][bonusSlot][leItem];
            entries[replaceSlot][leWeight] = gTierBonusLoot[bonusRow][bonusSlot][leWeight];
            entries[replaceSlot][leMin] = gTierBonusLoot[bonusRow][bonusSlot][leMin];
            entries[replaceSlot][leMax] = gTierBonusLoot[bonusRow][bonusSlot][leMax];
        }
    }

    return Loot_RollTable(entries, LOOT_TABLE_SLOTS, itemid, amount);
}

/* Peluang bonus langka: naik di malam hari dan di zona tier tinggi. */
stock Loot_RareChance(Float:x, Float:y, Float:z)
{
    new chance = 8;

    new zone = Zone_GetIdAt(x, y, z);
    if (zone >= 0 && zone < MAX_ZONE_COUNT) chance += Zone_GetTier(zone);
    if (World_IsNight()) chance += 4;

    if (chance > 25) chance = 25;
    return chance;
}

stock Loot_DropFromZombie(E_ZOMBIE_TYPE:type, Float:x, Float:y, Float:z, lobbyId, killerId = INVALID_PLAYER_ID)
{
    /* Brute dan Stalker tidak menumpahkan item ke tanah: mayatnya digeledah lewat
       dialog, jadi objek dunia tidak meledak saat horde besar mati. */
    if (type == ZT_BRUTE || type == ZT_STALKER)
    {
        return Corpse_Create(type, x, y, z, lobbyId);
    }

    new rolledItem = ITEM_NONE_ID, rolledAmount = 1;
    new drops = Loot_DropCountFor(type);
    new spawned = 0;
    new tier = Loot_ZombieTier(x, y, z);

    for (new index = 0; index < drops; index++)
    {
        if (!Loot_RollZombieTable(type, tier, rolledItem, rolledAmount)) break;

        new Float:jitterX = x + (float(random(140) - 70) / 100.0);
        new Float:jitterY = y + (float(random(140) - 70) / 100.0);
        if (Loot_Spawn(jitterX, jitterY, z, rolledItem, rolledAmount, INVALID_PLAYER_ID, lobbyId) != -1)
        {
            spawned++;
        }
    }

    /* Specimen hanya jatuh bila ada tag pemain dan pembunuh tidak berdiri di
       zona aman: menembak dari dalam hub lalu memungut sampel = uang tanpa
       risiko (audit F1). Kill murni NPC juga tidak menghasilkan sampel.
       Pity timer (U1): dijamin setelah 5 kill tanpa specimen. */
    new bool:specimenAllowed = (killerId != INVALID_PLAYER_ID && IsPlayerConnected(killerId)
        && !Player_IsSafe(killerId));
    if (specimenAllowed)
    {
        new bool:dropSpecimen = (random(100) < SPECIMEN_NORMAL_CHANCE);
        SpecimenPity[killerId]++;
        if (SpecimenPity[killerId] >= 5) dropSpecimen = true;
        if (dropSpecimen)
        {
            if (Loot_Spawn(x, y, z, ITEM_SPECIMEN, 1, INVALID_PLAYER_ID, lobbyId) != -1)
            {
                spawned++;
                SpecimenPity[killerId] = 0;
            }
        }
    }

    if (random(100) < Loot_RareChance(x, y, z))
    {
        if (Loot_RollTable(gRareLoot, LOOT_RARE_SLOTS, rolledItem, rolledAmount))
        {
            new Float:rareX = x + (float(random(120) - 60) / 100.0);
            new Float:rareY = y + (float(random(120) - 60) / 100.0);
            if (Loot_Spawn(rareX, rareY, z, rolledItem, rolledAmount, INVALID_PLAYER_ID, lobbyId) != -1)
            {
                spawned++;
                Log_Format(LOG_INFO, "[LOOT] bonus langka item=%d jumlah=%d zona=%d",
                    rolledItem, rolledAmount, Zone_GetIdAt(x, y, z));
            }
        }
    }

    return spawned;
}

public Loot_ExpireTick()
{
    new perfStart = GetTickCount();
    Loot_ExpireTickBody();
    Perf_Update(PERF_LOOT, perfStart);
    return 1;
}

stock Loot_ExpireTickBody()
{
    new now = GetTickCount();
    for (new slot = 0; slot < MAX_LOOT_DROPS; slot++)
    {
        if (!LootDrop[slot][ldActive]) continue;
        if (now - LootDrop[slot][ldExpireTick] >= 0)  /* wrap-safe */
        {
            Loot_RemoveDrop(slot);
            continue;
        }

        /* Jendela kepemilikan habis → pickup jadi publik. Tanpa ini pickup
           player-specific tetap hanya terlihat pemilik selamanya, dan pemain
           lain cuma bisa mengambil lewat /take tanpa melihat barangnya. */
        if (LootDrop[slot][ldOwnerPlayer] != INVALID_PLAYER_ID
            && now - LootDrop[slot][ldOwnerUntilTick] >= 0)  /* wrap-safe */
        {
            new pickupModel = (LootDrop[slot][ldKind] == LOOT_KIND_CRATE)
                ? LOOT_CRATE_PICKUP_MODEL
                : LOOT_ITEM_PICKUP_MODEL;
            DestroyDynamicPickup(LootDrop[slot][ldPickup]);
            LootDrop[slot][ldPickup] = CreateDynamicPickup(pickupModel, 2,
                LootDrop[slot][ldX], LootDrop[slot][ldY], LootDrop[slot][ldZ],
                .worldid = Lobby_WorldById(LootDrop[slot][ldLobby]));
            LootDrop[slot][ldOwnerPlayer] = INVALID_PLAYER_ID;
        }
    }
    return 1;
}

/* Jalur ambil yang tidak bergantung pada pickup streamer: tombol Y, /take, dan mobile. */
stock Loot_TakeNearest(playerid, Float:radius)
{
    if (!Account_IsPlaying(playerid)) return 0;

    new Float:px, Float:py, Float:pz;
    GetPlayerPos(playerid, px, py, pz);

    new lobbyId = Lobby_Of(playerid);
    new bestSlot = -1;
    new Float:bestDistanceSq = radius * radius;

    for (new slot = 0; slot < MAX_LOOT_DROPS; slot++)
    {
        if (!LootDrop[slot][ldActive]) continue;
        if (LootDrop[slot][ldLobby] != lobbyId) continue;   // loot lobby lain tidak bisa diambil
        /* Cek virtual world: shelter berada di lobby sama dengan VW berbeda;
           tanpa ini /take menembus dinding VW (audit F-24). */
        if (LootDrop[slot][ldWorld] != GetPlayerVirtualWorld(playerid)) continue;
        /* Eksklusif hanya selama jendela kepemilikan; sesudahnya drop menjadi publik. */
        if (LootDrop[slot][ldOwnerPlayer] != INVALID_PLAYER_ID && LootDrop[slot][ldOwnerPlayer] != playerid &&
            GetTickCount() - LootDrop[slot][ldOwnerUntilTick] < 0)  /* wrap-safe */
        {
            continue;
        }

        new Float:dx = LootDrop[slot][ldX] - px;
        new Float:dy = LootDrop[slot][ldY] - py;
        new Float:dz = LootDrop[slot][ldZ] - pz;
        new Float:distanceSq = dx * dx + dy * dy + dz * dz;

        if (distanceSq > bestDistanceSq) continue;
        bestDistanceSq = distanceSq;
        bestSlot = slot;
    }

    if (bestSlot == -1) return 0;

    new itemid = LootDrop[bestSlot][ldItemId];
    new amount = LootDrop[bestSlot][ldAmount];

    if (!Inventory_Grant(playerid, itemid, amount, "loot", LootDrop[bestSlot][ldDurability]))
    {
        SendClientMessage(playerid, UI_ACCENT_WARN, "[LOOT] Tas penuh atau terlalu berat.");
        return 0;
    }

    new itemLabel[32], line[112];
    Item_GetName(itemid, itemLabel, sizeof(itemLabel));
    format(line, sizeof(line), "[LOOT] %s x%d masuk tas.", itemLabel, amount);
    SendClientMessage(playerid, UI_ACCENT_PRIMARY, line);
    Toast_Show(playerid, "Loot diambil", itemLabel, UI_ACCENT_PRIMARY);
    Tutorial_OnLootTaken(playerid, itemid, amount);
    if (itemid == ITEM_DOCUMENT) Contracts_OnDocumentFound(playerid);

    Loot_RemoveDrop(bestSlot);
    return 1;
}

public OnPlayerPickUpDynamicPickup(playerid, pickupid)
{
    new slot = Loot_GetDropAtPickup(pickupid);
    if (slot == -1) return 0;
    if (LootDrop[slot][ldLobby] != Lobby_Of(playerid)) return 0;

    /* Jendela kepemilikan wajib dicek di sini juga, bukan hanya di /take. */
    if (LootDrop[slot][ldOwnerPlayer] != INVALID_PLAYER_ID
        && LootDrop[slot][ldOwnerPlayer] != playerid
        && GetTickCount() - LootDrop[slot][ldOwnerUntilTick] < 0)  /* wrap-safe */ return 0;

    new itemid = LootDrop[slot][ldItemId];
    new amount = LootDrop[slot][ldAmount];

    if (!Inventory_Grant(playerid, itemid, amount, "loot", LootDrop[slot][ldDurability])) return 0;

    if (itemid == ITEM_DOCUMENT) Contracts_OnDocumentFound(playerid);

    new itemLabel[32];
    Item_GetName(itemid, itemLabel, sizeof(itemLabel));
    Toast_Show(playerid, "Loot diambil", itemLabel, UI_ACCENT_PRIMARY);
    Tutorial_OnLootTaken(playerid, itemid, amount);

    Loot_RemoveDrop(slot);
    return 1;
}
