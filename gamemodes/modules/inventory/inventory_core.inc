#if defined _ls_inventory_core_included
    #endinput
#endif
#define _ls_inventory_core_included

enum E_INVENTORY_SLOT
{
    invItemId,
    invAmount,
    invDurability
};

new PlayerInventory[MAX_PLAYERS][MAX_INVENTORY_SLOTS][E_INVENTORY_SLOT];
new bool:PlayerInventoryDirty[MAX_PLAYERS];

/* bag.inc mendefinisikan kapasitas & efek tas, tapi dipanggil dari sini.
   Forward wajib ada karena compiler Pawn berjalan satu arah. */
forward Float:Bag_Capacity(playerid);
forward Float:Bag_CapacityForTier(playerid, tier);
forward Bag_TierOfItem(itemid);
forward Bag_OnInventoryChanged(playerid);

/* Grant/Consume didefinisikan di invpers.inc tapi dipakai modul yang di-include
   lebih awal (state.inc revive refund, char_create kit). Forward di state.inc
   (include paling awal dari pemakai) menghilangkan warning 208. */

/* Scrap legacy dari characters.scrap dikonversi jadi ITEM_SCRAP saat load
   inventory (E1); di-set di Account_OnCharacterLoaded. */
new PendingLegacyScrap[MAX_PLAYERS];

/* Guard load async (F-09): callback inventory/weapons/stash hanya boleh
   menulis ke slot bila ActiveCharId masih sama dengan saat request. */
new LoadCharGuard[MAX_PLAYERS];

stock Item_CountSlots(playerid)
{
    new used = 0;
    for (new slot = 0; slot < MAX_INVENTORY_SLOTS; slot++)
    {
        if (Item_IsValid(PlayerInventory[playerid][slot][invItemId])) used++;
    }
    return used;
}

stock Float:Inventory_GetWeight(playerid)
{
    new Float:total = 0.0;
    for (new slot = 0; slot < MAX_INVENTORY_SLOTS; slot++)
    {
        new itemid = PlayerInventory[playerid][slot][invItemId];
        if (!Item_IsValid(itemid)) continue;
        total += Item_GetWeight(itemid) * float(PlayerInventory[playerid][slot][invAmount]);
    }
    return total;
}

stock Inventory_CountItem(playerid, itemid)
{
    new total = 0;
    for (new slot = 0; slot < MAX_INVENTORY_SLOTS; slot++)
    {
        if (PlayerInventory[playerid][slot][invItemId] != itemid) continue;
        total += PlayerInventory[playerid][slot][invAmount];
    }
    return total;
}

stock bool:Inventory_AddItem(playerid, itemid, amount = 1, durability = WEAPON_CONDITION_MAX)
{
    if (!Item_IsValid(itemid) || amount <= 0) return false;
    if (!IsPlayerConnected(playerid)) return false;

    new Float:addedWeight = Item_GetWeight(itemid) * float(amount);

    /* Tas baru boleh masuk walau beban sekarang melebihi kapasitas tas lama:
       tanpa ini, craft Tas Harian bisa menghilangkan Tas Kecil di tengah tukar. */
    new Float:capacity = Bag_Capacity(playerid);
    new bagTier = Bag_TierOfItem(itemid);
    if (bagTier > 0) capacity = Bag_CapacityForTier(playerid, bagTier);

    if (Inventory_GetWeight(playerid) + addedWeight > capacity)
    {
        new reason[64];
        format(reason, sizeof(reason), "Beban melebihi %.0f kg. Pakai tas lebih besar.", capacity);
        Toast_Show(playerid, "Inventory penuh", reason, UI_ACCENT_DANGER);
        return false;
    }

    if (Item_IsStackable(itemid))
    {
        /* Stack menghormati itemMaxStack: cek kapasitas total DULU (all-or-nothing),
           baru mutasi. Tanpa ini tumpukan bisa tumbuh melewati batas registry. */
        new maxStack = Item_GetMaxStack(itemid);
        if (maxStack < 1) maxStack = 1;

        new stackSpace = 0, freeSlots = 0;
        for (new slot = 0; slot < MAX_INVENTORY_SLOTS; slot++)
        {
            if (PlayerInventory[playerid][slot][invItemId] == itemid
                && PlayerInventory[playerid][slot][invDurability] == durability)
            {
                new space = maxStack - PlayerInventory[playerid][slot][invAmount];
                if (space > 0) stackSpace += space;
            }
            else if (PlayerInventory[playerid][slot][invItemId] == ITEM_NONE_ID) freeSlots++;
        }
        if (stackSpace + freeSlots * maxStack < amount)
        {
            Toast_Show(playerid, "Inventory penuh", "Tumpukan dan slot tidak cukup.", UI_ACCENT_DANGER);
            return false;
        }

        new remaining = amount;
        for (new slot = 0; slot < MAX_INVENTORY_SLOTS && remaining > 0; slot++)
        {
            if (PlayerInventory[playerid][slot][invItemId] != itemid) continue;
            if (PlayerInventory[playerid][slot][invDurability] != durability) continue;
            new space = maxStack - PlayerInventory[playerid][slot][invAmount];
            if (space <= 0) continue;
            new moved = (remaining < space) ? remaining : space;
            PlayerInventory[playerid][slot][invAmount] += moved;
            remaining -= moved;
        }
        for (new slot = 0; slot < MAX_INVENTORY_SLOTS && remaining > 0; slot++)
        {
            if (PlayerInventory[playerid][slot][invItemId] != ITEM_NONE_ID) continue;
            new moved = (remaining < maxStack) ? remaining : maxStack;
            PlayerInventory[playerid][slot][invItemId] = itemid;
            PlayerInventory[playerid][slot][invAmount] = moved;
            PlayerInventory[playerid][slot][invDurability] = durability;
            remaining -= moved;
        }

        PlayerInventoryDirty[playerid] = true;
        Bag_OnInventoryChanged(playerid);
        return true;
    }

    for (new slot = 0; slot < MAX_INVENTORY_SLOTS; slot++)
    {
        if (PlayerInventory[playerid][slot][invItemId] != ITEM_NONE_ID) continue;

        PlayerInventory[playerid][slot][invItemId] = itemid;
        PlayerInventory[playerid][slot][invAmount] = amount;
        PlayerInventory[playerid][slot][invDurability] = durability;
        PlayerInventoryDirty[playerid] = true;
        Bag_OnInventoryChanged(playerid);
        return true;
    }

    Toast_Show(playerid, "Slot habis", "Kosongkan slot tas Anda", UI_ACCENT_DANGER);
    return false;
}

stock bool:Inventory_RemoveItem(playerid, itemid, amount = 1)
{
    if (!Item_IsValid(itemid) || amount <= 0) return false;
    if (Inventory_CountItem(playerid, itemid) < amount) return false;

    new remaining = amount;
    for (new slot = 0; slot < MAX_INVENTORY_SLOTS && remaining > 0; slot++)
    {
        if (PlayerInventory[playerid][slot][invItemId] != itemid) continue;

        new stored = PlayerInventory[playerid][slot][invAmount];
        if (stored <= remaining)
        {
            remaining -= stored;
            PlayerInventory[playerid][slot][invItemId] = ITEM_NONE_ID;
            PlayerInventory[playerid][slot][invAmount] = 0;
        }
        else
        {
            PlayerInventory[playerid][slot][invAmount] = stored - remaining;
            remaining = 0;
        }
    }

    PlayerInventoryDirty[playerid] = true;
    Bag_OnInventoryChanged(playerid);
    return (remaining == 0);
}

stock Inventory_Clear(playerid)
{
    for (new slot = 0; slot < MAX_INVENTORY_SLOTS; slot++)
    {
        PlayerInventory[playerid][slot][invItemId] = ITEM_NONE_ID;
        PlayerInventory[playerid][slot][invAmount] = 0;
        PlayerInventory[playerid][slot][invDurability] = 100;
    }
    PlayerInventoryDirty[playerid] = true;
    Bag_OnInventoryChanged(playerid);
    return 1;
}

/* /rapikan: gabungkan tumpukan sejenis (kondisi sama) lalu rapatkan ke slot depan.
   Tanpa ini, /buang sebagian dan /trade bisa menyisakan banyak tumpukan kecil. */
stock Inventory_Compact(playerid)
{
    new mergedCount = 0;

    for (new source = 0; source < MAX_INVENTORY_SLOTS; source++)
    {
        new itemid = PlayerInventory[playerid][source][invItemId];
        if (!Item_IsValid(itemid)) continue;
        if (!Item_IsStackable(itemid)) continue;

        for (new target = source + 1; target < MAX_INVENTORY_SLOTS; target++)
        {
            if (PlayerInventory[playerid][target][invItemId] != itemid) continue;
            if (PlayerInventory[playerid][target][invDurability] != PlayerInventory[playerid][source][invDurability]) continue;

            new maxStack = Item_GetMaxStack(itemid);
            new space = maxStack - PlayerInventory[playerid][source][invAmount];
            if (space <= 0) break;

            new moved = PlayerInventory[playerid][target][invAmount];
            if (moved > space) moved = space;
            if (moved <= 0) continue;

            PlayerInventory[playerid][source][invAmount] += moved;
            PlayerInventory[playerid][target][invAmount] -= moved;
            mergedCount += moved;

            if (PlayerInventory[playerid][target][invAmount] <= 0)
            {
                PlayerInventory[playerid][target][invItemId] = ITEM_NONE_ID;
                PlayerInventory[playerid][target][invAmount] = 0;
                PlayerInventory[playerid][target][invDurability] = WEAPON_CONDITION_MAX;
            }
        }

        if (PlayerInventory[playerid][source][invAmount] <= 0)
        {
            PlayerInventory[playerid][source][invItemId] = ITEM_NONE_ID;
            PlayerInventory[playerid][source][invAmount] = 0;
            PlayerInventory[playerid][source][invDurability] = WEAPON_CONDITION_MAX;
        }
    }

    // Rapatkan: item hidup naik ke slot paling depan, urutan antar item dipertahankan.
    new writeSlot = 0;
    for (new readSlot = 0; readSlot < MAX_INVENTORY_SLOTS; readSlot++)
    {
        if (!Item_IsValid(PlayerInventory[playerid][readSlot][invItemId])) continue;
        if (readSlot == writeSlot) { writeSlot++; continue; }

        PlayerInventory[playerid][writeSlot][invItemId] = PlayerInventory[playerid][readSlot][invItemId];
        PlayerInventory[playerid][writeSlot][invAmount] = PlayerInventory[playerid][readSlot][invAmount];
        PlayerInventory[playerid][writeSlot][invDurability] = PlayerInventory[playerid][readSlot][invDurability];

        PlayerInventory[playerid][readSlot][invItemId] = ITEM_NONE_ID;
        PlayerInventory[playerid][readSlot][invAmount] = 0;
        PlayerInventory[playerid][readSlot][invDurability] = WEAPON_CONDITION_MAX;
        writeSlot++;
    }

    PlayerInventoryDirty[playerid] = true;
    Bag_OnInventoryChanged(playerid);

    new message[128], bar[48];
    Bag_WeightBar(playerid, bar, sizeof(bar));
    format(message, sizeof(message), "[TAS] Dirapikan: %d item digabung. %s | slot %d/%d",
        mergedCount, bar, Item_CountSlots(playerid), MAX_INVENTORY_SLOTS);
    SendClientMessage(playerid, UI_ACCENT_PRIMARY, message);
    return 1;
}
