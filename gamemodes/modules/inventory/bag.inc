#if defined _ls_bag_included
    #endinput
#endif
#define _ls_bag_included

/* Sistem tas: kapasitas beban bertingkat + bentuk nyata di punggung.
   Tas adalah item pasif (kategori TOOL): memilikinya langsung menaikkan kapasitas
   dan mengganti model yang menempel di tulang punggung (bone 1).
   Aturan keras: satu tas terbaik yang dihitung, bukan penjumlahan semua tas. */

#define BAG_TIER_NONE     0
#define BAG_TIER_COUNT    4

#define BAG_ATTACH_SLOT   9         // slot attach terakhir, tidak dipakai sistem lain
#define BAG_ATTACH_BONE   1         // bone 1 = spine (punggung)
#define BAG_ATTACH_MODEL  1550      // kantong/ransel GTA SA, aman untuk semua skin
#define BAG_ATTACH_OFFSET_Y -0.24   // Y = arah depan-belakang, negatif = ke belakang
#define BAG_ATTACH_OFFSET_Z -0.10

#define BAG_PICKUP_ANIM_LIB   "COP_AMBIENT"
#define BAG_PICKUP_ANIM_NAME  "Copbrowse_loop"
#define BAG_PICKUP_ANIM_MS    2200

#define BAG_OVERLOAD_RATIO    0.90  // di atas ini beban terasa berat

new const gBagItemIds[BAG_TIER_COUNT] = {
    ITEM_BAG_SMALL, ITEM_BAG_DAY, ITEM_BAG_HIKING, ITEM_BAG_MILITARY
};

/* Bonus kapasitas per tier (kg). Basis tetap MAX_INVENTORY_WEIGHT = 30 kg. */
new const Float:gBagBonus[BAG_TIER_COUNT + 1] = {0.0, 10.0, 20.0, 32.0, 45.0};

new const gBagNames[BAG_TIER_COUNT + 1][20] = {
    "Tanpa tas", "Tas Kecil", "Tas Harian", "Tas Gunung", "Tas Militer"
};

new BagOverloadWarned[MAX_PLAYERS];

/* Modul ini di-include sebelum account_core & selftest, jadi fungsi mereka
   dideklarasikan lebih dulu. */
forward bool:Account_IsPlaying(playerid);
forward Selftest_Record(bool:passed, const name[], const detail[]);
forward Bag_RummageDone(playerid);
forward Selftest_CheckBags();

stock Bag_TierOfItem(itemid)
{
    for (new tier = 0; tier < BAG_TIER_COUNT; tier++)
    {
        if (gBagItemIds[tier] == itemid) return tier + 1;
    }
    return BAG_TIER_NONE;
}

stock Bag_BestTier(playerid)
{
    new best = BAG_TIER_NONE;

    for (new slot = 0; slot < MAX_INVENTORY_SLOTS; slot++)
    {
        new itemid = PlayerInventory[playerid][slot][invItemId];
        if (!Item_IsValid(itemid)) continue;

        new tier = Bag_TierOfItem(itemid);
        if (tier > best) best = tier;
    }
    return best;
}

stock Float:Bag_Capacity(playerid)
{
    return MAX_INVENTORY_WEIGHT + gBagBonus[Bag_BestTier(playerid)];
}

/* Kapasitas setelah tas baru masuk. Dipakai Inventory_AddItem supaya upgrade tas
   (Tas Kecil -> Tas Harian) tidak ditolak beban saat proses tukar barang. */
stock Float:Bag_CapacityForTier(playerid, tier)
{
    new best = Bag_BestTier(playerid);
    if (tier > best) best = tier;
    if (best < 0) best = 0;
    if (best > BAG_TIER_COUNT) best = BAG_TIER_COUNT;

    return MAX_INVENTORY_WEIGHT + gBagBonus[best];
}

stock Bag_LevelName(tier, dest[], size)
{
    if (tier < 0) tier = 0;
    if (tier > BAG_TIER_COUNT) tier = BAG_TIER_COUNT;
    return format(dest, size, "%s", gBagNames[tier]);
}

/* Model menempel di punggung; skala naik mengikuti tier supaya beda tas kelihatan
   dari jarak dekat tanpa perlu model khusus tiap tingkat. */
stock Bag_UpdateVisual(playerid)
{
    if (!IsPlayerConnected(playerid)) return 0;

    RemovePlayerAttachedObject(playerid, BAG_ATTACH_SLOT);

    new tier = Bag_BestTier(playerid);
    if (tier == BAG_TIER_NONE) return 1;

    new Float:scale = 0.85 + (float(tier) * 0.14);
    SetPlayerAttachedObject(playerid, BAG_ATTACH_SLOT, BAG_ATTACH_MODEL, BAG_ATTACH_BONE,
        0.0, BAG_ATTACH_OFFSET_Y, BAG_ATTACH_OFFSET_Z, 0.0, 0.0, 0.0,
        scale, scale, scale);
    return 1;
}

stock Bag_ResetVisual(playerid)
{
    if (!IsPlayerConnected(playerid)) return 0;
    RemovePlayerAttachedObject(playerid, BAG_ATTACH_SLOT);
    return 1;
}

/* Animasi menggeledah tas: dipakai saat membuka /inv supaya aksi terasa nyata,
   lalu animasi dilepas kembali lewat timer supaya pemain tidak terkunci. */
stock Bag_PlayRummage(playerid)
{
    if (!IsPlayerConnected(playerid)) return 0;
    if (IsPlayerInAnyVehicle(playerid)) return 0;
    if (PlayerSurvival[playerid][psDowned]) return 0;

    ApplyAnimation(playerid, BAG_PICKUP_ANIM_LIB, BAG_PICKUP_ANIM_NAME, 4.0, true, false, false, false, 0, 1);
    SetTimerEx("Bag_RummageDone", BAG_PICKUP_ANIM_MS, false, "i", playerid);
    return 1;
}

public Bag_RummageDone(playerid)
{
    if (!IsPlayerConnected(playerid)) return 0;
    if (GetPlayerSpecialAction(playerid) == SPECIAL_ACTION_CARRY) return 0;

    ClearAnimations(playerid);
    return 1;
}

stock bool:Bag_IsOverloaded(playerid)
{
    new Float:capacity = Bag_Capacity(playerid);
    if (capacity <= 0.0) return false;
    return Inventory_GetWeight(playerid) >= capacity * BAG_OVERLOAD_RATIO;
}

stock Bag_WeightBar(playerid, dest[], size)
{
    new Float:weight = Inventory_GetWeight(playerid);
    new Float:capacity = Bag_Capacity(playerid);

    new filled = 0;
    if (capacity > 0.0) filled = floatround((weight / capacity) * 10.0);
    if (filled < 0) filled = 0;
    if (filled > 10) filled = 10;

    new bar[16];
    for (new index = 0; index < 10; index++)
    {
        bar[index] = (index < filled) ? '#' : '-';
    }
    bar[10] = EOS;

    return format(dest, size, "[%s] %.1f/%.1f kg", bar, weight, capacity);
}

/* Ringkasan /tas: kapasitas, isi, sisa, dan cara naik tingkat. */
stock Bag_ShowInfo(playerid)
{
    if (!Account_IsPlaying(playerid))
    {
        SendClientMessage(playerid, UI_ACCENT_WARN, "[TAS] Login dulu untuk melihat tas.");
        return 0;
    }

    new tier = Bag_BestTier(playerid);
    new tierLabel[20], bar[48], line[144];

    Bag_LevelName(tier, tierLabel, sizeof(tierLabel));
    Bag_WeightBar(playerid, bar, sizeof(bar));

    new Float:weight = Inventory_GetWeight(playerid);
    new Float:capacity = Bag_Capacity(playerid);
    new slots = Item_CountSlots(playerid);

    format(line, sizeof(line), "[TAS] %s | %s | slot %d/%d", tierLabel, bar, slots, MAX_INVENTORY_SLOTS);
    SendClientMessage(playerid, UI_ACCENT_PRIMARY, line);

    format(line, sizeof(line), "[TAS] Sisa kapasitas %.1f kg. Bonus tas ini +%.0f kg.",
        capacity - weight, gBagBonus[tier]);
    SendClientMessage(playerid, UI_TEXT_SECONDARY, line);

    if (tier >= BAG_TIER_COUNT)
    {
        SendClientMessage(playerid, UI_TEXT_MUTED, "[TAS] Tas Militer: kapasitas maksimum tercapai. /inv untuk isi tas.");
        return 1;
    }

    new nextTier = tier + 1;
    new nextLabel[20];
    Bag_LevelName(nextTier, nextLabel, sizeof(nextLabel));
    format(line, sizeof(line), "[TAS] Naik ke %s (+%.0f kg): /craft lalu pilih resepnya, atau beli di vendor.",
        nextLabel, gBagBonus[nextTier]);
    SendClientMessage(playerid, UI_TEXT_MUTED, line);
    return 1;
}

/* Dipanggil setiap kali isi tas berubah supaya model di punggung ikut benar. */
stock Bag_OnInventoryChanged(playerid)
{
    Bag_UpdateVisual(playerid);

    new Float:weight = Inventory_GetWeight(playerid);
    new Float:capacity = Bag_Capacity(playerid);
    if (weight > capacity + 0.001)
    {
        new line[128];
        format(line, sizeof(line), "[TAS] Kelebihan beban (%.1f/%.0f kg). Pakai tas lagi atau buang item: /buang <nama>.",
            weight, capacity);
        SendClientMessage(playerid, UI_ACCENT_DANGER, line);
        return 1;
    }

    if (!Bag_IsOverloaded(playerid))
    {
        BagOverloadWarned[playerid] = 0;
        return 1;
    }

    if (BagOverloadWarned[playerid]) return 1;
    BagOverloadWarned[playerid] = 1;

    new bar[48], line[128];
    Bag_WeightBar(playerid, bar, sizeof(bar));
    format(line, sizeof(line), "[TAS] Beban berat %s. Jalan tetap bisa, tapi minum & makan cepat habis.", bar);
    SendClientMessage(playerid, UI_ACCENT_WARN, line);
    return 1;
}

stock Bag_ResetPlayer(playerid)
{
    BagOverloadWarned[playerid] = 0;
    Bag_ResetVisual(playerid);
    return 1;
}

stock Selftest_CheckBags()
{
    new detail[192];
    new invalid = 0;

    for (new tier = 0; tier < BAG_TIER_COUNT; tier++)
    {
        new itemid = gBagItemIds[tier];
        if (!Item_IsValid(itemid)) invalid++;
        else if (Item_GetWeight(itemid) <= 0.0) invalid++;
        else if (Bag_TierOfItem(itemid) != tier + 1) invalid++;
    }

    if (BAG_TIER_COUNT >= 1 && gBagBonus[0] != 0.0) invalid++;

    format(detail, sizeof(detail), "%d tingkat tas, bonus maks +%.0f kg, %d cacat",
        BAG_TIER_COUNT, gBagBonus[BAG_TIER_COUNT], invalid);
    Selftest_Record(invalid == 0, "sistem tas", detail);
    return 1;
}
