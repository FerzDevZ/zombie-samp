#if defined _ls_loot_tables_included
    #endinput
#endif
#define _ls_loot_tables_included

/* Tabel loot berbobot. Roll dilakukan di server (loot/loottable.inc) sehingga client
   tidak pernah tahu isi tabel; yang dikirim hanya item yang benar-benar jatuh.

   Aturan mengisi baris:
   - leWeight = bobot relatif (bukan persen). Bobot 0 berarti slot kosong.
   - leMin/leMax = jumlah item yang dijatuhkan (inklusif).
   - Urutan item boleh berubah, id item tidak boleh. */

#define LOOT_TABLE_SLOTS 10
#define LOOT_RARE_SLOTS 17
#define LOOT_NODE_SLOTS 19

enum E_LOOT_ENTRY
{
    leItem,
    leWeight,
    leMin,
    leMax
};

/* ZT_WALKER, ZT_RUNNER, ZT_CRAWLER, ZT_BRUTE, ZT_BLOATER, ZT_SCREAMER, ZT_STALKER */
new const gZombieLoot[MAX_ZOMBIE_TYPES][LOOT_TABLE_SLOTS][E_LOOT_ENTRY] =
{
    {   // Walker: material dasar, sesekali amunisi pistol
        {ITEM_SCRAP,        30, 1, 3},
        {ITEM_CLOTH,        20, 1, 2},
        {ITEM_FOOD_CAN,     12, 1, 1},
        {ITEM_WATER_BOTTLE, 12, 1, 1},
        {ITEM_POWDER,        8, 1, 2},
        {ITEM_BANDAGE,       7, 1, 1},
        {ITEM_PISTOL_AMMO,   8, 3, 8},
        {ITEM_WOOD,          3, 1, 2},
        {ITEM_ENERGY_DRINK,  6, 1, 1},
        {ITEM_DECOY,         2, 1, 1}
    },
    {   // Runner: cepat, loot ringan dan obat
        {ITEM_CLOTH,        18, 1, 2},
        {ITEM_BANDAGE,      16, 1, 2},
        {ITEM_POWDER,       12, 1, 3},
        {ITEM_SCRAP,        12, 1, 2},
        {ITEM_PISTOL_AMMO,  15, 4, 10},
        {ITEM_FOOD_CAN,      8, 1, 1},
        {ITEM_ELECTRONICS,   7, 1, 1},
        {ITEM_ANTIBIOTICS,   5, 1, 1},
        {ITEM_WATER_BOTTLE,  7, 1, 1},
        {ITEM_ENERGY_DRINK,  6, 1, 1}
    },
    {   // Crawler: elektronik, kadang blueprint
        {ITEM_SCRAP,        20, 1, 3},
        {ITEM_ELECTRONICS,  15, 1, 2},
        {ITEM_CLOTH,        12, 1, 2},
        {ITEM_BANDAGE,      10, 1, 1},
        {ITEM_CHEMICAL,      8, 1, 1},
        {ITEM_ANTIBIOTICS,   6, 1, 1},
        {ITEM_PISTOL_AMMO,   8, 2, 6},
        {ITEM_RIFLE_AMMO,    4, 2, 5},
        {ITEM_BLUEPRINT_DEFIB, 3, 1, 1},
        {ITEM_DECOY,         4, 1, 1}
    },
    {   // Brute: berat, bahan bangunan dan kit
        {ITEM_WOOD,         20, 2, 5},
        {ITEM_BARRICADE_KIT,18, 1, 1},
        {ITEM_SCRAP,        16, 2, 4},
        {ITEM_MEDKIT,       12, 1, 1},
        {ITEM_RIFLE_AMMO,    6, 3, 8},
        {ITEM_BLOOD_PACK,    7, 1, 1},
        {ITEM_BLUEPRINT_SUPPRESSOR, 3, 1, 1},
        {ITEM_POWDER,        6, 1, 3},
        {ITEM_TOOLBOX,       3, 1, 1},
        {ITEM_ADRENALINE,    4, 1, 1}
    },
    {   // Bloater: bahan kimia dan obat
        {ITEM_CHEMICAL,     25, 1, 3},
        {ITEM_HERB,         20, 1, 2},
        {ITEM_CLOTH,        12, 1, 2},
        {ITEM_BANDAGE,      10, 1, 2},
        {ITEM_ANTIBIOTICS,   8, 1, 1},
        {ITEM_BLOOD_PACK,    5, 1, 1},
        {ITEM_MEDKIT,        4, 1, 1},
        {ITEM_POWDER,        6, 1, 2},
        {ITEM_ENERGY_DRINK,  6, 1, 1},
        {ITEM_ADRENALINE,    3, 1, 1}
    },
    {   // Screamer: elektronik dan blueprint
        {ITEM_ELECTRONICS,  20, 1, 2},
        {ITEM_PISTOL_AMMO,  15, 4, 12},
        {ITEM_CLOTH,        12, 1, 2},
        {ITEM_SCRAP,        10, 1, 2},
        {ITEM_BANDAGE,       8, 1, 2},
        {ITEM_ANTIBIOTICS,   6, 1, 1},
        {ITEM_BLUEPRINT_SUPPRESSOR, 4, 1, 1},
        {ITEM_BLUEPRINT_DEFIB,      3, 1, 1},
        {ITEM_RIFLE_AMMO,    4, 2, 6},
        {ITEM_RADIO,         3, 1, 1}
    },
    {   // Stalker: loot medis dan amunisi rifle
        {ITEM_BLOOD_PACK,   14, 1, 1},
        {ITEM_ANTIBIOTICS,  12, 1, 1},
        {ITEM_RIFLE_AMMO,    9, 3, 8},
        {ITEM_DOCUMENT,     6, 1, 1},
        {ITEM_MEDKIT,        8, 1, 1},
        {ITEM_PISTOL_AMMO,   8, 4, 10},
        {ITEM_ELECTRONICS,   7, 1, 1},
        {ITEM_BLUEPRINT_SUPPRESSOR, 3, 1, 1},
        {ITEM_BLUEPRINT_DEFIB,      2, 1, 1},
        {ITEM_ADRENALINE,    5, 1, 1}
    },
    {   // Abomination: hadiah boss, semua slot terisi supaya tabel tetap penuh
        {ITEM_MEDKIT,        18, 1, 2},
        {ITEM_RIFLE_AMMO,    16, 8, 16},
        {ITEM_REPAIR_KIT,    14, 1, 1},
        {ITEM_VACCINE,       12, 1, 1},
        {ITEM_ADRENALINE,    12, 1, 2},
        {ITEM_TOOLBOX,       10, 1, 1},
        {ITEM_ELECTRONICS,   10, 2, 4},
        {ITEM_BLOOD_PACK,     8, 1, 2},
        {ITEM_SPECIMEN,       6, 1, 1},
        {ITEM_DOCUMENT,       4, 1, 1}
    }
};

/* Bonus langka: roll terpisah, peluang naik di malam hari dan zona tier tinggi. */
new const gRareLoot[LOOT_RARE_SLOTS][E_LOOT_ENTRY] =
{
    {ITEM_BLUEPRINT_SUPPRESSOR, 20, 1, 1},
    {ITEM_BLUEPRINT_DEFIB,      20, 1, 1},
    {ITEM_MEDKIT,               20, 1, 1},
    {ITEM_BLOOD_PACK,           16, 1, 2},
    {ITEM_RIFLE_AMMO,           12, 5, 12},
    {ITEM_BARRICADE_KIT,         8, 1, 1},
    {ITEM_LOCKPICK,             10, 1, 1},
    {ITEM_REPAIR_KIT,            6, 1, 1},
    {ITEM_RADIO,                 8, 1, 1},
    {ITEM_ADRENALINE,            8, 1, 1},
    {ITEM_TOOLBOX,               6, 1, 1},
    {ITEM_VACCINE,               4, 1, 1},
    {ITEM_DOCUMENT,              7, 1, 1},
    {ITEM_WAREHOUSE_KEY,         3, 1, 1},
    {ITEM_NOTEBOOK,              4, 1, 1},
    {ITEM_BAG_DAY,               5, 1, 1},
    {ITEM_BAG_HIKING,            3, 1, 1}
};

/* Peti darurat yang muncul sendiri di zona berbahaya. */
new const gNodeLoot[LOOT_NODE_SLOTS][E_LOOT_ENTRY] =
{
    {ITEM_FOOD_CAN,     22, 1, 2},
    {ITEM_WATER_BOTTLE, 20, 1, 2},
    {ITEM_SCRAP,        18, 1, 3},
    {ITEM_CLOTH,        12, 1, 2},
    {ITEM_BANDAGE,      10, 1, 1},
    {ITEM_PISTOL_AMMO,   8, 3, 9},
    {ITEM_ELECTRONICS,   5, 1, 1},
    {ITEM_MEDKIT,        3, 1, 1},
    {ITEM_RIFLE_AMMO,    2, 3, 8},
    {ITEM_BAG_SMALL,     3, 1, 1},
    {ITEM_JERRYCAN,      4, 1, 1},
    {ITEM_REPAIR_KIT,    4, 1, 1},
    {ITEM_ENERGY_DRINK,  8, 1, 2},
    {ITEM_DECOY,         5, 1, 1},
    {ITEM_PAPER,        10, 1, 2},
    {ITEM_EXTMAG,        3, 1, 1},
    {ITEM_SCOPE,         2, 1, 1},
    {ITEM_BOTTLE,        8, 1, 2},
    {ITEM_FLARE,         2, 1, 1}
};

/* ---------- Visual & mayat ---------- */

#define LOOT_KIND_ITEM   0
#define LOOT_KIND_CRATE  1

/* Model drop: JANGAN pakai 1225 (itu tong minyak) atau model item mentah
   (2045/2905/2906 = tong sampah/kontainer gudang). Semua drop memakai satu
   ikon koper lapangan 1210 supaya jelas terbaca sebagai loot, termasuk di
   layar HP. */
#define LOOT_CRATE_OBJECT_MODEL  1210   // peti darurat: koper lapangan
#define LOOT_CRATE_PICKUP_MODEL  1210
#define LOOT_ITEM_PICKUP_MODEL   1210   // ikon drop item (bukan model asli item)
#define LOOT_CORPSE_PICKUP_MODEL 1210
#define LOOT_NODE_LABEL_DIST     60.0

/* ---------- Tingkat peti ----------
   Peti biasa = loot harian. Peti LANGKA memakai tabel gRareLoot (blueprint,
   vaccine, bag hiking) dan ditandai emas di dunia + map supaya pemain tahu
   ada alasan untuk menyeberang zona. */
#define LOOT_TIER_NORMAL         0
#define LOOT_TIER_RARE           1
#define LOOT_NODE_RARE_CHANCE    14      // persen peti baru yang jadi langka
#define LOOT_NODE_RARE_CHANCE_NIGHT 22   // malam: hadiah lebih besar
#define LOOT_CRATE_ICON_NORMAL   1239
#define LOOT_CRATE_ICON_RARE     1240
#define LOOT_CRATE_COLOR_NORMAL  0x38BDF8FF
#define LOOT_CRATE_COLOR_RARE    0xFFD24AFF
#define LOOT_CRATE_LABEL_DIST    60.0
#define LOOT_CRATE_LABEL_DIST_RARE 90.0

#define LOOT_CORPSE_SLOTS        3
#define LOOT_MAX_CORPSES         24
#define LOOT_CORPSE_EXPIRE_MS    300000
#define LOOT_CORPSE_RANGE        3.0

/* Sampel zombie: bahan ekstraksi. Elite lebih padat sampel. */
#define SPECIMEN_NORMAL_CHANCE   14
#define SPECIMEN_ELITE_CHANCE    45
#define SPECIMEN_MAX_PER_CORPSE  1

/* Zona tier 2 dan 3 mengganti dua slot terakhir tabel dasar dengan loot yang lebih
   berharga. Jadi area berbahaya bukan hanya lebih sering langka, tapi isi drop-nya
   memang lebih baik.
   Bobot rifle ammo sudah diturunkan ~25% di semua tabel supaya amunisi rifle tetap
   terasa langka dibanding pistol ammo. */
new const gTierBonusLoot[2][2][E_LOOT_ENTRY] = {
    {
        {ITEM_MEDKIT,      12, 1, 1},
        {ITEM_RIFLE_AMMO,    8, 3, 8}
    },
    {
        {ITEM_BLOOD_PACK,  12, 1, 1},
        /* Suppressor SUDAH diimplementasi (craft + noise senyap): blueprint
           kembali jatuh sebagai loot bernilai. */
        {ITEM_BLUEPRINT_SUPPRESSOR, 6, 1, 1}
    }
};
