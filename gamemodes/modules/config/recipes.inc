#if defined _ls_recipes_included
    #endinput
#endif
#define _ls_recipes_included

/* Resep crafting. Hasil dan bahan memakai id permanen dari config/items.inc.
   rcResult -1 berarti resep khusus (perbaikan alat), bukan pembuatan item. */
#define MAX_RECIPES 22
#define RECIPE_REPAIR_TOOLS -1
#define RECIPE_REPAIR_WEAPON -2

enum E_RECIPE
{
    rcResult,
    rcResultAmount,
    rcIngredientA,
    rcIngredientAAmount,
    rcIngredientB,
    rcIngredientBAmount,
    rcBlueprint,
    rcName[40]
};

new const gRecipes[MAX_RECIPES][E_RECIPE] = {
    {ITEM_BANDAGE,       2, ITEM_CLOTH,     2, ITEM_NONE_ID,   0, ITEM_NONE_ID,             "Bandage x2"},
    {ITEM_MEDKIT,        1, ITEM_CLOTH,     3, ITEM_CHEMICAL,  1, ITEM_NONE_ID,             "Medkit x1"},
    {ITEM_ANTIBIOTICS,   1, ITEM_HERB,      2, ITEM_CHEMICAL,  1, ITEM_NONE_ID,             "Antibiotics x1"},
    {ITEM_BLOOD_PACK,    1, ITEM_MEDKIT,    1, ITEM_CLOTH,     2, ITEM_BLUEPRINT_DEFIB,     "Blood Pack x1"},
    {ITEM_BARRICADE_KIT, 1, ITEM_WOOD,      3, ITEM_SCRAP,     2, ITEM_NONE_ID,             "Barricade Kit x1"},
    {ITEM_PISTOL_AMMO,  12, ITEM_POWDER,    2, ITEM_SCRAP,     1, ITEM_NONE_ID,             "Peluru Pistol x12"},
    {ITEM_RIFLE_AMMO,   10, ITEM_POWDER,    3, ITEM_SCRAP,     2, ITEM_NONE_ID,             "Peluru Rifle x10"},
    {RECIPE_REPAIR_TOOLS,  1, ITEM_SCRAP,   2, ITEM_WOOD,      1, ITEM_NONE_ID,             "Perbaiki alat (+40%)"},
    {RECIPE_REPAIR_WEAPON, 1, ITEM_SCRAP,   3, ITEM_ELECTRONICS, 1, ITEM_NONE_ID,          "Perbaiki senjata di tangan (+40%)"},
    {ITEM_LOCKPICK,      1, ITEM_SCRAP,     2, ITEM_ELECTRONICS, 1, ITEM_NONE_ID,             "Lockpick x1"},
    {ITEM_REPAIR_KIT,    1, ITEM_SCRAP,     3, ITEM_ELECTRONICS, 1, ITEM_NONE_ID,             "Repair Kit x1"},
    {ITEM_ENERGY_DRINK,  1, ITEM_CHEMICAL,  1, ITEM_NONE_ID,   0, ITEM_NONE_ID,             "Energy Drink x1"},
    {ITEM_RADIO,         1, ITEM_ELECTRONICS, 2, ITEM_SCRAP,   2, ITEM_NONE_ID,             "Radio x1"},
    {ITEM_DECOY,         2, ITEM_POWDER,    2, ITEM_ELECTRONICS, 1, ITEM_NONE_ID,             "Decoy Sirene x2"},
    {ITEM_ADRENALINE,    1, ITEM_CHEMICAL,  2, ITEM_HERB,      1, ITEM_NONE_ID,             "Adrenaline Shot x1"},
    {ITEM_VACCINE,       1, ITEM_HERB,      3, ITEM_CHEMICAL,  2, ITEM_NONE_ID,             "Vaksin x1"},
    {ITEM_TOOLBOX,       1, ITEM_SCRAP,     4, ITEM_ELECTRONICS, 2, ITEM_NONE_ID,             "Toolbox x1"},
    {ITEM_BAG_SMALL,     1, ITEM_CLOTH,     4, ITEM_SCRAP,      2, ITEM_NONE_ID,             "Tas Kecil x1"},
    {ITEM_BAG_DAY,       1, ITEM_BAG_SMALL, 1, ITEM_CLOTH,      6, ITEM_NONE_ID,             "Tas Harian x1"},
    {ITEM_BAG_HIKING,    1, ITEM_BAG_DAY,   1, ITEM_ELECTRONICS, 3, ITEM_NONE_ID,            "Tas Gunung x1"},
    {ITEM_BAG_MILITARY,  1, ITEM_BAG_HIKING, 1, ITEM_ELECTRONICS, 5, ITEM_NONE_ID,           "Tas Militer x1"},
    {ITEM_SUPPRESSOR,    1, ITEM_SCRAP,      8, ITEM_ELECTRONICS, 2, ITEM_BLUEPRINT_SUPPRESSOR, "Suppressor x1"}
};
