#if defined _ls_zones_included
    #endinput
#endif
#define _ls_zones_included

enum E_ZONE_DATA
{
    zoneName[MAX_ZONE_NAME],
    Float:zoneX,
    Float:zoneY,
    Float:zoneZ,
    Float:zoneRadius,
    zoneTier,
    bool:zonePvP,
    bool:zoneIsHub,
    zoneAmbientTarget       // ambient zombie population target
};

/* Approximate vanilla coordinates: verified against the in-game map, tune freely. */
new const gZones[MAX_ZONE_COUNT][E_ZONE_DATA] = {
    {"Quarantine Camp",  1685.0, -2333.0,  13.0, 180.0, 0, false, true,   0},
    {"Idlewood",         1900.0, -1200.0,  20.0, 250.0, 1, false, false, 10},
    {"Ganton",           2200.0, -2000.0,  13.0, 200.0, 1, false, false,  8},
    {"Downtown LS",      1300.0, -1700.0,  20.0, 300.0, 2, true,  false, 14},
    {"LS Sewers",        1350.0, -1600.0, -20.0, 150.0, 2, false, false,  8},
    {"Angel Pine",      -2100.0, -2400.0,  30.0, 350.0, 1, false, false,  8},
    {"Abandoned Airfield", 400.0, 2450.0,  16.0, 350.0, 3, true,  false, 12},
    {"Bone County Quarry", -500.0, 1400.0, 20.0, 300.0, 3, true,  false, 12},
    {"Camp Outskirts",   1780.0, -2160.0, 13.0, 150.0, 1, false, false, 10},
    {"",                 0.0,    0.0,    0.0,   0.0, 0, false, false,  0}
};

/* Titik tengah zona: dipakai GPS dan laporan "zona terdekat" supaya tidak ada
   yang menyalin koordinat dari tabel gZones di modul lain. */
stock Zone_GetCenter(zoneid, &Float:x, &Float:y, &Float:z)
{
    if (zoneid < 0 || zoneid >= MAX_ZONE_COUNT) return 0;
    x = gZones[zoneid][zoneX];
    y = gZones[zoneid][zoneY];
    z = gZones[zoneid][zoneZ];
    return 1;
}

stock Zone_NearestToPlayer(playerid)
{
    new Float:px, Float:py, Float:pz;
    GetPlayerPos(playerid, px, py, pz);
    new bestZone = -1;
    new Float:bestDist = 999999.0;
    for (new zoneid = 0; zoneid < MAX_ZONE_COUNT; zoneid++)
    {
        if (gZones[zoneid][zoneName][0] == EOS) continue;
        new Float:dx = gZones[zoneid][zoneX] - px;
        new Float:dy = gZones[zoneid][zoneY] - py;
        new Float:dist = dx * dx + dy * dy;
        if (dist < bestDist)
        {
            bestDist = dist;
            bestZone = zoneid;
        }
    }
    return bestZone;
}

stock Zone_GetIdAt(Float:x, Float:y, Float:z)
{
    for (new zoneid = 0; zoneid < MAX_ZONE_COUNT; zoneid++)
    {
        if (gZones[zoneid][zoneRadius] <= 0.0) continue;

        new Float:dz = gZones[zoneid][zoneZ] - z;
        if (dz < -80.0 || dz > 80.0) continue;

        new Float:dx = gZones[zoneid][zoneX] - x;
        new Float:dy = gZones[zoneid][zoneY] - y;
        if ((dx * dx + dy * dy) <= (gZones[zoneid][zoneRadius] * gZones[zoneid][zoneRadius]))
        {
            return zoneid;
        }
    }
    return INVALID_ZONE;
}

stock Zone_IsHub(zoneid)
{
    if (zoneid < 0 || zoneid >= MAX_ZONE_COUNT) return 0;
    return gZones[zoneid][zoneIsHub];
}

stock Zone_IsPvP(zoneid)
{
    if (zoneid < 0 || zoneid >= MAX_ZONE_COUNT) return 1;
    return gZones[zoneid][zonePvP];
}

stock Zone_GetTier(zoneid)
{
    if (zoneid < 0 || zoneid >= MAX_ZONE_COUNT) return 0;
    return gZones[zoneid][zoneTier];
}

stock Zone_GetName(zoneid, dest[], size)
{
    if (zoneid < 0 || zoneid >= MAX_ZONE_COUNT)
    {
        strcpy(dest, "Wasteland", size);
        return 1;
    }
    strcpy(dest, gZones[zoneid][zoneName], size);
    return 1;
}

stock Zone_GetAmbientTarget(zoneid)
{
    if (zoneid < 0 || zoneid >= MAX_ZONE_COUNT) return 0;
    return gZones[zoneid][zoneAmbientTarget];
}

/* Zone_IsNight() lama dihapus: memakai jam nyata gettime(), bukan waktu dunia
   game, dan tidak punya pemanggil. Satu-satunya sumber malam: World_IsNight(). */
