#if defined _ls_weapons_included
    #endinput
#endif
#define _ls_weapons_included

stock Float:Weapon_GetMaxRange(weaponid)
{
    switch (weaponid)
    {
        case 0, 1: return 3.0;                        // fist / brass knuckles
        case 2..9, 15: return 3.2;                    // melee
        case 22, 23, 24: return 65.0;                 // pistols
        case 25, 26, 27: return 55.0;                 // shotguns
        case 28, 29, 32: return 95.0;                 // smg
        case 30, 31: return 160.0;                    // rifles
        case 33, 34: return 320.0;                    // sniper
        case 35..40: return 60.0;                     // launchers / misc
    }
    return 40.0;
}

stock Float:Weapon_GetNoiseRadius(weaponid)
{
    switch (weaponid)
    {
        case 0..15: return NOISE_MELEE;
        case 22, 23, 24: return NOISE_SHOT_PISTOL;
        case 25, 26, 27: return NOISE_SHOT_SHOTGUN;
        case 28, 29, 32: return NOISE_SHOT_SMG;
        case 30, 31, 33, 34: return NOISE_SHOT_RIFLE;
        case 35..40: return 120.0;
    }
    return 30.0;
}

stock bool:Weapon_IsMelee(weaponid)
{
    return (weaponid >= 0 && weaponid <= 15);
}

/* Damage the weapon deals per hit, used for zombie attacks and melee validation. */
stock Float:Weapon_GetBaseDamage(weaponid)
{
    switch (weaponid)
    {
        case 0: return 4.0;
        case 1: return 6.0;
        case 2: return 12.0;
        case 3: return 10.0;
        case 4: return 8.0;
        case 5: return 14.0;
        case 6: return 18.0;
        case 7: return 16.0;
        case 8: return 20.0;
        case 9: return 22.0;
        case 15: return 30.0;
        case 22: return 25.0;
        case 23: return 28.0;
        case 24: return 55.0;
        case 25: return 45.0;
        case 26: return 80.0;
        case 27: return 70.0;
        case 28: return 26.0;
        case 29: return 30.0;
        case 32: return 24.0;
        case 30: return 40.0;
        case 31: return 45.0;
        case 33: return 90.0;
        case 34: return 120.0;
        default: return 20.0;
    }
    return 20.0;
}
