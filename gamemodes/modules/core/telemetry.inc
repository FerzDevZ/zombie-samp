#if defined _ls_telemetry_included
    #endinput
#endif
#define _ls_telemetry_included

/* Telemetri permainan: satu baris DB per kejadian penting.
   Tanpa ini, balancing hanya tebakan; dengan ini, /stats dan laporan staff punya dasar. */

stock Telemetry_LogZombieKill(playerid, E_ZOMBIE_TYPE:type, weaponId, Float:distance)
{
    if (!SQL_IsReady() || ActiveCharId[playerid] == 0) return 0;

    if (distance < 0.0) distance = 0.0;
    if (distance > 300.0) distance = 300.0;

    new query[320], msBuf[16];
    EpochMs(msBuf, sizeof(msBuf));
    mysql_format(gSQL, query, sizeof(query),
        "INSERT INTO kill_log (killer_id, victim_id, victim_kind, zombie_type, weapon_id, dist_m, zone_id, created_at) VALUES (%d, 0, 2, %d, %d, %f, %d, %s)",
        ActiveCharId[playerid], _:type, weaponId, distance,
        (Player_GetZone(playerid) < 0) ? 255 : Player_GetZone(playerid), msBuf);
    mysql_tquery(gSQL, query, "", "");
    return 1;
}
