#if defined _ls_structures_included
    #endinput
#endif
#define _ls_structures_included

/* Data statis Base Building. Semua harga, HP, dan batas ada di sini supaya
   balancing cukup mengubah satu tabel, bukan menyisir logika. */

#define BASE_MAX_STRUCTURES        40
#define BASE_PLOT_RADIUS           22.0
#define BASE_STRUCT_MIN_GAP        2.0
#define BASE_STRUCT_MIN_HUB_DIST   60.0
#define BASE_STRUCT_ZOMBIE_DPS     12
#define BASE_STRUCT_MAX_ATTACKERS  4
#define BASE_BUILD_COOLDOWN_MS     1500
#define BASE_STRUCT_REPAIR_SCRAP   2
#define BASE_STRUCT_REPAIR_HEALTH  400
#define BASE_STRUCT_DISMANTLE_PCT  50
#define BASE_TURRET_RANGE          30.0
#define BASE_TURRET_DAMAGE         8.0
#define BASE_TURRET_COOLDOWN_MS    2000
#define BASE_TURRET_NOISE_RADIUS   25.0
#define BASE_GENERATOR_FUEL_MAX     20.0
#define BASE_GENERATOR_JERRY_LITERS 20.0
#define BASE_GENERATOR_BURN_TICK    0.06
#define BASE_GENERATOR_REFILL_RANGE 6.0
#define BASE_GENERATOR_FUEL_SAVE_MS 60000
#define BASE_CRATE_SLOTS           20
#define BASE_STASH_MAX_SLOTS       120
#define BASE_MAX_TIER              3
#define BASE_TIER3_HEALTH_BONUS    25
#define BASE_WALL_OBJECT_MODEL     1225

enum E_STRUCTURE
{
    stLabel[24],
    stMatA,
    stAmtA,
    stMatB,
    stAmtB,
    stHealth,
    stTier,
    stLimit,
    stPickupModel,      // 0 = struktur solid (object + collision)
    bool:stSolid
};

#define STRUCT_WALL   0
#define STRUCT_GATE   1
#define STRUCT_CRATE  2
#define STRUCT_BENCH  3
#define STRUCT_BED    4
#define STRUCT_TURRET 5
#define STRUCT_GENERATOR 6
#define STRUCTURE_COUNT 7

new const gStructures[STRUCTURE_COUNT][E_STRUCTURE] = {
    {"Wall",    ITEM_SCRAP, 6, ITEM_WOOD,        4, 2000, 1, 12,   0, true},
    {"Gate",    ITEM_SCRAP, 4, ITEM_WOOD,        2, 1200, 1,  2,   0, true},
    {"Crate",   ITEM_SCRAP, 3, ITEM_WOOD,        5,  800, 1,  3, 1210, false},
    {"Bench",   ITEM_SCRAP, 5, ITEM_ELECTRONICS, 2,  900, 2,  1, 1239, false},
    {"Bed",     ITEM_SCRAP, 4, ITEM_CLOTH,       3,  600, 1,  1, 1240, false},
    {"Turret",  ITEM_SCRAP, 8, ITEM_ELECTRONICS, 4,  700, 3,  2, 1242, false},
    {"Generator", ITEM_SCRAP, 10, ITEM_ELECTRONICS, 6, 1200, 2, 1, 1233, false}
};

/* Model objek per tipe struktur.
   Sebelumnya SEMUA tipe memakai BASE_WALL_OBJECT_MODEL (1225 = barrelexpos,
   tong minyak MERAH): pilih "Wall" yang muncul tong merah, dan semua tipe lain
   ikut tong merah. Pratinjau (/bangun) dan hasil akhir sekarang memakai tabel
   yang SAMA, jadi tidak ada kejutan setelah /pasang.
   `docs/BASE_BUILDING.md` masih menyebut 1225 untuk Wall/Gate; tabel ini yang
   berlaku (dokumen diperbarui di commit yang sama). */
new const gStructureModels[STRUCTURE_COUNT] = {
    19447,  // Wall      - panel dinding, sumber material matte_slate di mapping.inc
    1495,   // Gate      - pintu garasi, sama dengan SHELTER_DOOR_MODEL
    2932,   // Crate     - peti, sama dengan peti airdrop di world/airdrop.inc
    1996,   // Bench     - bangku kerja
    1793,   // Bed       - kasur
    1997,   // Turret    - dudukan mesin
    1676    // Generator - mesin pompa, sudah dipakai world/garage.inc
};

/* Satu sumber kebenaran untuk model: dipakai CreateVisuals DAN pratinjau. */
stock BaseBuild_Model(type)
{
    if (type < 0 || type >= STRUCTURE_COUNT) return BASE_WALL_OBJECT_MODEL;
    return gStructureModels[type];
}

/* Batas wall naik 4 slot saat tier 2, sesuai rancangan di docs/BASE_BUILDING.md. */
stock BaseBuild_Limit(structure, tier)
{
    new limit = gStructures[structure][stLimit];
    if (structure == STRUCT_WALL && tier >= 2) limit += 4;
    return limit;
}

stock BaseBuild_Health(structure, tier)
{
    new health = gStructures[structure][stHealth];
    if (tier >= BASE_MAX_TIER) health += (health * BASE_TIER3_HEALTH_BONUS) / 100;
    return health;
}
