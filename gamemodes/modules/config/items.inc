#if defined _ls_items_config_included
    #endinput
#endif
#define _ls_items_config_included

enum E_ITEM_CATEGORY
{
    ITEM_NONE = 0,
    ITEM_CONSUMABLE,
    ITEM_MATERIAL,
    ITEM_TOOL,
    ITEM_WEAPON,
    ITEM_BLUEPRINT
};

enum E_ITEM_DATA
{
    itemModel,
    itemName[24],
    Float:itemWeight,
    itemStackable,
    itemMaxStack,
    E_ITEM_CATEGORY:itemCategory
};

/* Item ids are permanent: they are persisted in MySQL, never renumber them.
   Plain consts keep arithmetic and array indexing tag-free. */
const ITEM_NONE_ID = 0;
const ITEM_SCRAP = 1;
const ITEM_CLOTH = 2;
const ITEM_WOOD = 3;
const ITEM_CHEMICAL = 4;
const ITEM_HERB = 5;
const ITEM_ELECTRONICS = 6;
const ITEM_POWDER = 7;
const ITEM_FOOD_CAN = 8;
const ITEM_WATER_BOTTLE = 9;
const ITEM_BANDAGE = 10;
const ITEM_MEDKIT = 11;
const ITEM_ANTIBIOTICS = 12;
const ITEM_BLOOD_PACK = 13;
const ITEM_BARRICADE_KIT = 14;
const ITEM_PISTOL_AMMO = 15;
const ITEM_RIFLE_AMMO = 16;
const ITEM_BLUEPRINT_SUPPRESSOR = 17;
const ITEM_BLUEPRINT_DEFIB = 18;
const ITEM_LOCKPICK = 19;
const ITEM_JERRYCAN = 20;
const ITEM_SPECIMEN = 21;
const ITEM_REPAIR_KIT = 22;
const ITEM_ENERGY_DRINK = 23;
const ITEM_RADIO = 24;
const ITEM_DECOY = 25;
const ITEM_ADRENALINE = 26;
const ITEM_VACCINE = 27;
const ITEM_TOOLBOX = 28;
const ITEM_DOCUMENT = 29;
const ITEM_WAREHOUSE_KEY = 30;
const ITEM_NOTEBOOK = 31;
const ITEM_BAG_SMALL = 32;
const ITEM_BAG_DAY = 33;
const ITEM_BAG_HIKING = 34;
const ITEM_BAG_MILITARY = 35;
const ITEM_SUPPRESSOR = 36;
const ITEM_COUNT = 37;

new const gItemRegistry[ITEM_COUNT][E_ITEM_DATA] = {
    {0,      "None",             0.0, 0, 0,   ITEM_NONE},
    {2045,   "Scrap",            0.4, 1, 20,  ITEM_MATERIAL},
    {19993,  "Cloth",            0.1, 1, 20,  ITEM_MATERIAL},
    {2905,   "Wood",             0.8, 1, 15,  ITEM_MATERIAL},
    {2904,   "Chemical",         0.5, 1, 10,  ITEM_MATERIAL},
    {2906,   "Herb",             0.1, 1, 20,  ITEM_MATERIAL},
    {19832,  "Electronics",      0.6, 1, 10,  ITEM_MATERIAL},
    {2037,   "Powder",           0.2, 1, 20,  ITEM_MATERIAL},
    {2670,   "Canned Food",      0.5, 1, 10,  ITEM_CONSUMABLE},
    {19822,  "Water Bottle",     0.5, 1, 10,  ITEM_CONSUMABLE},
    {19992,  "Bandage",          0.2, 1, 10,  ITEM_CONSUMABLE},
    {2702,   "Medkit",           1.5, 0, 1,   ITEM_CONSUMABLE},
    {19994,  "Antibiotics",      0.2, 1, 5,   ITEM_CONSUMABLE},
    {19995,  "Blood Pack",       1.0, 0, 1,   ITEM_CONSUMABLE},
    {19996,  "Barricade Kit",    5.0, 0, 1,   ITEM_TOOL},
    {2038,   "Pistol Ammo",      0.1, 1, 60,  ITEM_WEAPON},
    {2039,   "Rifle Ammo",       0.1, 1, 90,  ITEM_WEAPON},
    {19997,  "Suppressor Plan",  0.1, 0, 1,   ITEM_BLUEPRINT},
    {19998,  "Defib Plan",       0.1, 0, 1,   ITEM_BLUEPRINT},
    {19574,  "Lockpick",         0.1, 0, 1,   ITEM_TOOL},
    {1650,   "Jerrycan",         4.0, 0, 1,   ITEM_TOOL},
    {1240,   "Zombie Specimen",  0.5, 1, 8,   ITEM_MATERIAL},
    {19921,  "Repair Kit",       3.0, 0, 1,   ITEM_TOOL},
    {1512,   "Energy Drink",     0.3, 1, 10,  ITEM_CONSUMABLE},
    {330,    "Radio",            0.8, 0, 1,   ITEM_TOOL},
    {1651,   "Decoy Sirene",     0.6, 1, 5,   ITEM_CONSUMABLE},
    {19994,  "Adrenaline Shot",  0.2, 1, 3,   ITEM_CONSUMABLE},
    {19995,  "Vaksin",           0.3, 1, 2,   ITEM_CONSUMABLE},
    {1210,   "Toolbox",          3.0, 0, 1,   ITEM_TOOL},
    {2894,   "Dokumen Rahasia",  0.1, 1, 10,  ITEM_MATERIAL},
    {1172,   "Kunci Gudang",     0.1, 0, 1,   ITEM_TOOL},
    {2828,   "Buku Catatan",     0.3, 0, 1,   ITEM_TOOL},
    {1550,   "Tas Kecil",        0.6, 0, 1,   ITEM_TOOL},
    {1550,   "Tas Harian",       0.9, 0, 1,   ITEM_TOOL},
    {1550,   "Tas Gunung",       1.2, 0, 1,   ITEM_TOOL},
    {1550,   "Tas Militer",      1.6, 0, 1,   ITEM_TOOL},
    /* Suppressor: durability = sisa tembakan (SUPPRESSOR_SHOTS saat craft).
       Selama dibawa di tas, tembakan pistol (22-24) memakai radius senyap. */
    {19997,  "Suppressor",       0.3, 0, 1,   ITEM_TOOL}
};

stock bool:Item_IsValid(itemid)
{
    return (itemid > ITEM_NONE_ID && itemid < ITEM_COUNT);
}

stock Float:Item_GetWeight(itemid)
{
    if (!Item_IsValid(itemid)) return 0.0;
    return gItemRegistry[itemid][itemWeight];
}

stock Item_GetName(itemid, dest[], size)
{
    if (!Item_IsValid(itemid)) return format(dest, size, "Unknown");
    return format(dest, size, "%s", gItemRegistry[itemid][itemName]);
}

stock Item_GetMaxStack(itemid)
{
    if (!Item_IsValid(itemid)) return 1;
    return gItemRegistry[itemid][itemMaxStack];
}

stock E_ITEM_CATEGORY:Item_GetCategory(itemid)
{
    if (!Item_IsValid(itemid)) return ITEM_NONE;
    return gItemRegistry[itemid][itemCategory];
}

stock bool:Item_IsStackable(itemid)
{
    if (!Item_IsValid(itemid)) return false;
    return gItemRegistry[itemid][itemStackable] != 0;
}

stock Item_GetModel(itemid)
{
    if (!Item_IsValid(itemid)) return 0;
    return gItemRegistry[itemid][itemModel];
}
