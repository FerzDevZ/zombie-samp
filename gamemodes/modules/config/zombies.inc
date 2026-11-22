#if defined _ls_zombies_config_included
    #endinput
#endif
#define _ls_zombies_config_included

enum E_ZOMBIE_TYPE
{
    ZT_WALKER = 0,
    ZT_RUNNER,
    ZT_CRAWLER,
    ZT_BRUTE,
    ZT_BLOATER,
    ZT_SCREAMER,
    ZT_STALKER,
    ZT_BOSS
};

enum E_ZOMBIE_PROFILE
{
    ztName[16],
    ztSkin,
    Float:ztHealth,
    Float:ztWalkSpeed,
    Float:ztRunSpeed,
    Float:ztDamage,
    ztAttackDelayMs,
    ztLootItemId,
    ztLootChance,
    ztMinTier,
    ztIsNocturnal,
    ztMeleeStyle,          // FIGHT_STYLE_*: gaya pukulan menentukan bentuk serangan
    Float:ztBodyRadius
};

// Baseline tuning lives here and nowhere else: designers change this table only.
new const gZombieProfiles[MAX_ZOMBIE_TYPES][E_ZOMBIE_PROFILE] = {
    {"Walker",    // 0
        1, 60.0, 0.1552086, 0.1552086, 8.0, 1900, 1, 40, 1, false, 16, 1.0},
    {"Runner",    // 1
        2, 45.0, 0.3000000, 0.5644400, 6.0, 1500, 2, 30, 1, false, 7, 1.0},
    {"Crawler",   // 2
        3, 30.0, 0.1552086, 0.2000000, 12.0, 2100, 3, 5, 2, false, 15, 0.8},
    {"Brute",     // 3
        4, 350.0, 0.2000000, 0.3000000, 35.0, 2600, 4, 60, 3, false, 15, 1.4},
    {"Bloater",   // 4
        5, 120.0, 0.1552086, 0.2000000, 40.0, 3200, 5, 70, 2, false, 16, 1.2},
    {"Screamer",  // 5
        6, 80.0, 0.2500000, 0.3500000, 4.0, 2400, 6, 50, 2, false, 4, 1.0},
    {"Stalker",   // 6
        7, 70.0, 0.3500000, 0.9267840, 18.0, 1700, 7, 20, 3, true, 16, 0.9},
    // Abomination: boss malam. minTier 99 = tidak pernah dipilih spawn rutin,
    // hanya lewat Director_BossTick. Loot specimen dijamin (chance 100).
    {"Abomination",
        8, 2400.0, 0.2000000, 0.3200000, 45.0, 2600, 21, 100, 99, true, 15, 1.8}
};

/* Gaya pukulan disimpan per tipe supaya gerombolan tidak memukul serempak
   dengan animasi tinju yang sama (terlihat seperti tawuran, bukan zombie). */
stock ZombieType_GetMeleeStyle(E_ZOMBIE_TYPE:type)
{
    if (_:type < 0 || _:type >= MAX_ZOMBIE_TYPES) return FIGHT_STYLE_NORMAL;
    return gZombieProfiles[_:type][ztMeleeStyle];
}

stock Float:ZombieType_GetRunSpeed(E_ZOMBIE_TYPE:type)
{
    if (_:type < 0 || _:type >= MAX_ZOMBIE_TYPES) return 0.5644400;
    return gZombieProfiles[_:type][ztRunSpeed];
}

stock Float:ZombieType_GetWalkSpeed(E_ZOMBIE_TYPE:type)
{
    if (_:type < 0 || _:type >= MAX_ZOMBIE_TYPES) return 0.1552086;
    return gZombieProfiles[_:type][ztWalkSpeed];
}

stock ZombieType_IsNocturnal(E_ZOMBIE_TYPE:type)
{
    if (_:type < 0 || _:type >= MAX_ZOMBIE_TYPES) return 0;
    return gZombieProfiles[_:type][ztIsNocturnal];
}
