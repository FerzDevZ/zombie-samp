#if defined _ls_craft_ui_included
    #endinput
#endif
#define _ls_craft_ui_included

/* Meja crafting di kamp. Semua bahan diambil lewat Inventory_Consume supaya
   item_ledger tetap jadi satu-satunya sumber kebenaran. */

#define DIALOG_CRAFT_LIST   2301
#define DIALOG_CRAFT_RESULT 2302

new CraftSelectedRecipe[MAX_PLAYERS];

stock CraftUI_RecipeUnlocked(playerid, recipeIndex)
{
    new blueprint = gRecipes[recipeIndex][rcBlueprint];
    if (blueprint == ITEM_NONE_ID) return 1;
    return (Inventory_CountItem(playerid, blueprint) > 0);
}

stock CraftUI_HasIngredients(playerid, recipeIndex)
{
    new itemA = gRecipes[recipeIndex][rcIngredientA];
    new itemB = gRecipes[recipeIndex][rcIngredientB];

    if (itemA != ITEM_NONE_ID && Inventory_CountItem(playerid, itemA) < gRecipes[recipeIndex][rcIngredientAAmount]) return 0;
    if (itemB != ITEM_NONE_ID && Inventory_CountItem(playerid, itemB) < gRecipes[recipeIndex][rcIngredientBAmount]) return 0;
    return 1;
}

stock CraftUI_IngredientLine(recipeIndex, line[], size)
{
    new labelA[24], labelB[24];
    Item_GetName(gRecipes[recipeIndex][rcIngredientA], labelA, sizeof(labelA));
    Item_GetName(gRecipes[recipeIndex][rcIngredientB], labelB, sizeof(labelB));

    if (gRecipes[recipeIndex][rcIngredientB] == ITEM_NONE_ID)
    {
        format(line, size, "%s x%d", labelA, gRecipes[recipeIndex][rcIngredientAAmount]);
    }
    else
    {
        format(line, size, "%s x%d + %s x%d", labelA, gRecipes[recipeIndex][rcIngredientAAmount],
            labelB, gRecipes[recipeIndex][rcIngredientBAmount]);
    }
    return 1;
}

stock CraftUI_Show(playerid)
{
    if (!Account_IsPlaying(playerid)) return 0;

    new body[2048], line[192], ingredientLine[96];
    body[0] = EOS;

    for (new index = 0; index < MAX_RECIPES; index++)
    {
        CraftUI_IngredientLine(index, ingredientLine, sizeof(ingredientLine));

        if (!CraftUI_RecipeUnlocked(playerid, index))
        {
            format(line, sizeof(line), "%s  [butuh blueprint]\n", gRecipes[index][rcName]);
        }
        else if (!CraftUI_HasIngredients(playerid, index))
        {
            format(line, sizeof(line), "%s  (%s) [bahan kurang]\n", gRecipes[index][rcName], ingredientLine);
        }
        else
        {
            format(line, sizeof(line), "%s  (%s)\n", gRecipes[index][rcName], ingredientLine);
        }
        strcat(body, line, sizeof(body));
    }

    ShowPlayerDialog(playerid, DIALOG_CRAFT_LIST, DIALOG_STYLE_LIST, "Meja Crafting", body, "Buat", "Tutup");
    return 1;
}

stock CraftUI_RepairTools(playerid)
{
    new bestSlot = -1;
    new lowest = WEAPON_CONDITION_MAX;

    for (new slot = 0; slot < MAX_INVENTORY_SLOTS; slot++)
    {
        new itemid = PlayerInventory[playerid][slot][invItemId];
        if (!Item_IsValid(itemid)) continue;
        if (Item_GetCategory(itemid) != ITEM_TOOL) continue;

        new durability = PlayerInventory[playerid][slot][invDurability];
        if (durability >= WEAPON_CONDITION_MAX) continue;
        if (durability >= lowest) continue;

        lowest = durability;
        bestSlot = slot;
    }

    if (bestSlot == -1)
    {
        Toast_Show(playerid, "Perbaikan", "Tidak ada alat yang perlu diperbaiki.", UI_TEXT_MUTED);
        return 0;
    }

    if (!Inventory_Consume(playerid, ITEM_SCRAP, 2, "repair") ||
        !Inventory_Consume(playerid, ITEM_WOOD, 1, "repair"))
    {
        Toast_Show(playerid, "Bahan kurang", "Butuh Scrap 2 + Wood 1.", UI_ACCENT_WARN);
        return 0;
    }

    new repaired = PlayerInventory[playerid][bestSlot][invDurability] + 40;
    if (repaired > WEAPON_CONDITION_MAX) repaired = WEAPON_CONDITION_MAX;
    PlayerInventory[playerid][bestSlot][invDurability] = repaired;
    PlayerInventoryDirty[playerid] = true;
    Inventory_Save(playerid);

    new itemLabel[24], message[112];
    Item_GetName(PlayerInventory[playerid][bestSlot][invItemId], itemLabel, sizeof(itemLabel));
    format(message, sizeof(message), "[CRAFT] %s diperbaiki ke %d%%.", itemLabel, repaired);
    SendClientMessage(playerid, UI_ACCENT_PRIMARY, message);
    return 1;
}

stock CraftUI_RepairWeapon(playerid)
{
    new weaponid = GetPlayerWeapon(playerid);
    if (weaponid <= 0 || weaponid >= 47)
    {
        Toast_Show(playerid, "Perbaikan", "Pegang senjata yang mau diperbaiki.", UI_TEXT_MUTED);
        return 0;
    }

    if (WeaponCondition[playerid][weaponid] >= WEAPON_CONDITION_MAX)
    {
        Toast_Show(playerid, "Perbaikan", "Senjata itu masih dalam kondisi penuh.", UI_TEXT_MUTED);
        return 0;
    }

    if (!Inventory_Consume(playerid, ITEM_SCRAP, 3, "repair_weapon") ||
        !Inventory_Consume(playerid, ITEM_ELECTRONICS, 1, "repair_weapon"))
    {
        Toast_Show(playerid, "Bahan kurang", "Butuh Scrap 3 + Electronics 1.", UI_ACCENT_WARN);
        return 0;
    }

    new repaired = WeaponCondition[playerid][weaponid] + 40;
    if (repaired > WEAPON_CONDITION_MAX) repaired = WEAPON_CONDITION_MAX;
    WeaponCondition[playerid][weaponid] = repaired;
    Weapons_Save(playerid);

    new message[112];
    format(message, sizeof(message), "[CRAFT] Senjata %d diperbaiki ke %d%%.", weaponid, repaired);
    SendClientMessage(playerid, UI_ACCENT_PRIMARY, message);
    return 1;
}

stock CraftUI_Craft(playerid, recipeIndex)
{
    if (recipeIndex < 0 || recipeIndex >= MAX_RECIPES) return 0;

    if (!CraftUI_RecipeUnlocked(playerid, recipeIndex))
    {
        Toast_Show(playerid, "Terkunci", "Butuh blueprint untuk resep ini.", UI_ACCENT_WARN);
        return 0;
    }

    if (!CraftUI_HasIngredients(playerid, recipeIndex))
    {
        Toast_Show(playerid, "Bahan kurang", "Kumpulkan bahan yang diminta.", UI_ACCENT_WARN);
        return 0;
    }

    if (gRecipes[recipeIndex][rcResult] == RECIPE_REPAIR_TOOLS) return CraftUI_RepairTools(playerid);
    if (gRecipes[recipeIndex][rcResult] == RECIPE_REPAIR_WEAPON) return CraftUI_RepairWeapon(playerid);

    new itemA = gRecipes[recipeIndex][rcIngredientA];
    new itemB = gRecipes[recipeIndex][rcIngredientB];
    new amountA = gRecipes[recipeIndex][rcIngredientAAmount];
    new amountB = gRecipes[recipeIndex][rcIngredientBAmount];

    // Consume dulu; kalau salah satu gagal, bahan yang sudah diambil dikembalikan.
    if (itemA != ITEM_NONE_ID && !Inventory_Consume(playerid, itemA, amountA, "craft"))
    {
        Toast_Show(playerid, "Gagal", "Bahan pertama tidak bisa diambil.", UI_ACCENT_DANGER);
        return 0;
    }
    if (itemB != ITEM_NONE_ID && !Inventory_Consume(playerid, itemB, amountB, "craft"))
    {
        if (itemA != ITEM_NONE_ID) Inventory_Grant(playerid, itemA, amountA, "craft_refund");
        Toast_Show(playerid, "Gagal", "Bahan kedua tidak bisa diambil.", UI_ACCENT_DANGER);
        return 0;
    }

    new resultItem = gRecipes[recipeIndex][rcResult];
    new resultAmount = gRecipes[recipeIndex][rcResultAmount];

    if (!Inventory_Grant(playerid, resultItem, resultAmount, "craft",
        (resultItem == ITEM_SUPPRESSOR) ? SUPPRESSOR_SHOTS : WEAPON_CONDITION_MAX))
    {
        if (itemA != ITEM_NONE_ID) Inventory_Grant(playerid, itemA, amountA, "craft_refund");
        if (itemB != ITEM_NONE_ID) Inventory_Grant(playerid, itemB, amountB, "craft_refund");
        Toast_Show(playerid, "Tas penuh", "Kosongkan slot sebelum membuat item.", UI_ACCENT_WARN);
        return 0;
    }

    new resultLabel[24], message[128];
    Item_GetName(resultItem, resultLabel, sizeof(resultLabel));
    format(message, sizeof(message), "[CRAFT] %s x%d dibuat.", resultLabel, resultAmount);
    SendClientMessage(playerid, UI_ACCENT_PRIMARY, message);
    Tutorial_OnCraft(playerid);

    CraftSelectedRecipe[playerid] = recipeIndex;
    format(message, sizeof(message), "%s x%d berhasil dibuat.\n\nCraft lagi?", resultLabel, resultAmount);
    ShowPlayerDialog(playerid, DIALOG_CRAFT_RESULT, DIALOG_STYLE_MSGBOX, "Berhasil", message, "Craft lagi", "Selesai");
    return 1;
}

stock CraftUI_OnDialogResponse(playerid, dialogid, response, listitem, const inputtext[])
{
    #pragma unused inputtext

    /* Meja kamp atau Bench base: validasi ulang saat respons (audit F-13). */
    if (response && (!Account_IsPlaying(playerid)
        || (!Player_IsSafe(playerid) && !BaseBuild_HasNearby(playerid, STRUCT_BENCH, 6.0)))) return 0;

    switch (dialogid)
    {
        case DIALOG_CRAFT_LIST:
        {
            if (!response) return 1;
            CraftUI_Craft(playerid, listitem);
            return 1;
        }
        case DIALOG_CRAFT_RESULT:
        {
            if (response)
            {
                CraftUI_Craft(playerid, CraftSelectedRecipe[playerid]);
            }
            else
            {
                CraftUI_Show(playerid);
            }
            return 1;
        }
    }
    return 0;
}
