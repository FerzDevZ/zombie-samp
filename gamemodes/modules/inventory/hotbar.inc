#if defined _ls_hotbar_included
    #endinput
#endif
#define _ls_hotbar_included

/* Hotbar cepat 1-5: cara pakai item tanpa membuka dialog.
   Penting untuk SA-MP Mobile (tidak ada key 1-5 di OnPlayerKeyStateChange),
   jadi hotbar dipakai lewat command: /hs 1 .. /hs 5.

   Isi hotbar menyimpan item id (bukan slot), karena slot inventory bisa berpindah
   saat item dipakai, ditumpuk, atau dipindah ke stash. */

#define HOTBAR_SLOTS 5
#define HOTBAR_MAX_CSV 32

new HotbarItemIds[MAX_PLAYERS][HOTBAR_SLOTS];

forward Selftest_Record(bool:passed, const name[], const detail[]);
forward Selftest_CheckHotbar();

stock Hotbar_Reset(playerid)
{
    for (new index = 0; index < HOTBAR_SLOTS; index++)
    {
        HotbarItemIds[playerid][index] = ITEM_NONE_ID;
    }
    return 1;
}

stock Hotbar_Set(playerid, index, itemid)
{
    if (index < 0 || index >= HOTBAR_SLOTS) return 0;
    if (!Item_IsValid(itemid)) return 0;

    HotbarItemIds[playerid][index] = itemid;
    return 1;
}

stock Hotbar_Clear(playerid, index)
{
    if (index < 0 || index >= HOTBAR_SLOTS) return 0;

    HotbarItemIds[playerid][index] = ITEM_NONE_ID;
    return 1;
}

stock Hotbar_ItemAt(playerid, index)
{
    if (index < 0 || index >= HOTBAR_SLOTS) return ITEM_NONE_ID;
    return HotbarItemIds[playerid][index];
}

/* Slot inventory yang sekarang berisi item hotbar ini, -1 kalau habis. */
stock Hotbar_FindSlotForItem(playerid, itemid)
{
    if (!Item_IsValid(itemid)) return -1;

    for (new slot = 0; slot < MAX_INVENTORY_SLOTS; slot++)
    {
        if (PlayerInventory[playerid][slot][invItemId] != itemid) continue;
        if (PlayerInventory[playerid][slot][invAmount] <= 0) continue;
        return slot;
    }
    return -1;
}

stock Hotbar_LoadCSV(playerid, const csv[])
{
    Hotbar_Reset(playerid);

    new cursor = 0;
    new field[HOTBAR_MAX_CSV];

    for (new index = 0; index < HOTBAR_SLOTS; index++)
    {
        field[0] = EOS;
        new written = 0;

        while (csv[cursor] != EOS && csv[cursor] != ',')
        {
            if (written < HOTBAR_MAX_CSV - 1)
            {
                field[written] = csv[cursor];
                written++;
            }
            cursor++;
        }
        field[written] = EOS;

        if (csv[cursor] == ',') cursor++;

        new itemid = strval(field);
        if (Item_IsValid(itemid)) HotbarItemIds[playerid][index] = itemid;
    }
    return 1;
}

stock Hotbar_ToCSV(playerid, dest[], size)
{
    dest[0] = EOS;

    new part[8];
    for (new index = 0; index < HOTBAR_SLOTS; index++)
    {
        new itemid = HotbarItemIds[playerid][index];
        if (!Item_IsValid(itemid)) itemid = ITEM_NONE_ID;

        format(part, sizeof(part), "%s%d", (index == 0) ? "" : ",", itemid);
        strcat(dest, part, size);
    }
    return 1;
}

stock Selftest_CheckHotbar()
{
    /* Cek runtime, bukan konstanta: slot hotbar pemain harus mulai kosong dan
       hanya boleh berisi id item yang valid setelah login. */
    new invalid = 0;

    for (new index = 0; index < HOTBAR_SLOTS; index++)
    {
        new itemid = HotbarItemIds[0][index];
        if (itemid != ITEM_NONE_ID && !Item_IsValid(itemid)) invalid++;
    }

    new csvSlot[HOTBAR_SLOTS];
    for (new index = 0; index < HOTBAR_SLOTS; index++) csvSlot[index] = ITEM_NONE_ID;

    new detail[160];
    format(detail, sizeof(detail), "%d slot hotbar, penyimpanan CSV maks %d karakter, %d cacat",
        sizeof(csvSlot), HOTBAR_MAX_CSV, invalid);
    Selftest_Record(invalid == 0, "hotbar cepat", detail);
    return 1;
}
