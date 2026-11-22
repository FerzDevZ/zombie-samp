#if defined _ls_stats_included
    #endinput
#endif
#define _ls_stats_included

/* /stats membaca kill_log per karakter: zona paling mematikan, senjata paling efektif,
   dan tipe zombie yang paling banyak dibunuh. Satu query UNION, satu dialog. */

#define DIALOG_STATS 2901

new StatsZoneCount[MAX_PLAYERS][MAX_ZONE_COUNT];
new StatsWeaponCount[MAX_PLAYERS][47];
new StatsTypeCount[MAX_PLAYERS][MAX_ZOMBIE_TYPES];
new bool:StatsPending[MAX_PLAYERS];

forward Stats_OnRowsLoaded(playerid);

stock Stats_Show(playerid)
{
    if (!Account_IsPlaying(playerid)) return 0;
    if (ActiveCharId[playerid] == 0) return 0;
    if (!SQL_IsReady())
    {
        SendClientMessage(playerid, UI_ACCENT_WARN, "[STATS] Database belum siap.");
        return 0;
    }

    for (new zone = 0; zone < MAX_ZONE_COUNT; zone++) StatsZoneCount[playerid][zone] = 0;
    for (new weapon = 0; weapon < 47; weapon++) StatsWeaponCount[playerid][weapon] = 0;
    for (new type = 0; type < MAX_ZOMBIE_TYPES; type++) StatsTypeCount[playerid][type] = 0;

    StatsPending[playerid] = true;

    new query[768];
    mysql_format(gSQL, query, sizeof(query),
        "SELECT 0 AS kind, zone_id AS k, COUNT(*) AS c FROM kill_log WHERE killer_id = %d GROUP BY zone_id \
         UNION ALL SELECT 1, weapon_id, COUNT(*) FROM kill_log WHERE killer_id = %d GROUP BY weapon_id \
         UNION ALL SELECT 2, zombie_type, COUNT(*) FROM kill_log WHERE killer_id = %d GROUP BY zombie_type",
        ActiveCharId[playerid], ActiveCharId[playerid], ActiveCharId[playerid]);
    mysql_pquery(gSQL, query, "Stats_OnRowsLoaded", "i", playerid);
    return 1;
}

stock Stats_TopLabel(counts[], size, out[], outSize)
{
    new bestIndex = -1, bestCount = 0;

    for (new index = 0; index < size; index++)
    {
        if (counts[index] <= bestCount) continue;
        bestCount = counts[index];
        bestIndex = index;
    }

    if (bestIndex < 0)
    {
        format(out, outSize, "-");
        return 0;
    }

    counts[bestIndex] = 0;
    return bestIndex + 1;
}

public Stats_OnRowsLoaded(playerid)
{
    if (!IsPlayerConnected(playerid)) return 0;
    if (!StatsPending[playerid]) return 0;

    StatsPending[playerid] = false;

    if (mysql_errno(gSQL) != 0)
    {
        Log_Format(LOG_WARN, "[STATS] query gagal player=%d errno=%d", playerid, mysql_errno(gSQL));
        SendClientMessage(playerid, UI_ACCENT_WARN, "[STATS] Query gagal, coba lagi.");
        return 0;
    }

    new rows = cache_num_rows();
    new totalKills = 0;

    for (new row = 0; row < rows; row++)
    {
        new kind = 0, key = 0, count = 0;
        cache_get_value_name_int(row, "kind", kind);
        cache_get_value_name_int(row, "k", key);
        cache_get_value_name_int(row, "c", count);

        totalKills += count;

        switch (kind)
        {
            case 0:
            {
                if (key >= 0 && key < MAX_ZONE_COUNT) StatsZoneCount[playerid][key] += count;
            }
            case 1:
            {
                if (key >= 0 && key < 47) StatsWeaponCount[playerid][key] += count;
            }
            case 2:
            {
                if (key >= 0 && key < MAX_ZOMBIE_TYPES) StatsTypeCount[playerid][key] += count;
            }
        }
    }

    new body[1600], line[192], label[48], zoneLabelName[MAX_ZONE_NAME];

    format(body, sizeof(body), "Total zombie dibunuh: %d\nMati: %d | Level: %d\n\n",
        totalKills, PlayerSurvival[playerid][psDeaths], PlayerSurvival[playerid][psLevel]);

    strcat(body, "Zona paling sering:\n", sizeof(body));
    for (new rank = 0; rank < 3; rank++)
    {
        new zoneIndex = Stats_TopLabel(StatsZoneCount[playerid], MAX_ZONE_COUNT, label, sizeof(label));
        if (zoneIndex == 0) break;

        Zone_GetName(zoneIndex - 1, zoneLabelName, sizeof(zoneLabelName));
        format(line, sizeof(line), "  %d. %s\n", rank + 1, zoneLabelName);
        strcat(body, line, sizeof(body));
    }

    strcat(body, "\nSenjata terpakai:\n", sizeof(body));
    for (new rank = 0; rank < 3; rank++)
    {
        new weaponIndex = Stats_TopLabel(StatsWeaponCount[playerid], 47, label, sizeof(label));
        if (weaponIndex == 0) break;

        new weaponName[32];
        new weaponId = weaponIndex - 1;
        if (weaponId == 0) format(weaponName, sizeof(weaponName), "Tanpa senjata");
        else if (!GetWeaponName(weaponId, weaponName, sizeof(weaponName))) format(weaponName, sizeof(weaponName), "Senjata %d", weaponId);

        format(line, sizeof(line), "  %d. %s\n", rank + 1, weaponName);
        strcat(body, line, sizeof(body));
    }

    strcat(body, "\nTipe zombie:\n", sizeof(body));
    for (new rank = 0; rank < 3; rank++)
    {
        new typeIndex = Stats_TopLabel(StatsTypeCount[playerid], MAX_ZOMBIE_TYPES, label, sizeof(label));
        if (typeIndex == 0) break;

        format(line, sizeof(line), "  %d. %s\n", rank + 1, gZombieProfiles[typeIndex - 1][ztName]);
        strcat(body, line, sizeof(body));
    }

    if (totalKills == 0)
    {
        strcat(body, "\nBelum ada data. Bunuh zombie di luar kamp untuk mengisi statistik.", sizeof(body));
    }

    ShowPlayerDialog(playerid, DIALOG_STATS, DIALOG_STYLE_MSGBOX, "Statistik karakter", body, "Tutup", "");
    return 1;
}
