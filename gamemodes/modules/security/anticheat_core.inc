#if defined _ls_anticheat_included
    #endinput
#endif
#define _ls_anticheat_included

new AnticheatFlagCount[MAX_PLAYERS];
new AnticheatLastFlagTick[MAX_PLAYERS];

/* Every rejection with a real payload reaches this function: it is the audit trail
   staff reads before banning anyone. */
stock Anticheat_Flag(playerid, const flagType[], const payload[])
{
    if (!IsPlayerConnected(playerid)) return 0;

    AnticheatFlagCount[playerid]++;
    AnticheatLastFlagTick[playerid] = GetTickCount();

    new name[MAX_PLAYER_NAME + 1];
    GetPlayerName(playerid, name, sizeof(name));

    new line[320];
    format(line, sizeof(line), "%s player=%d name=%s payload=%s",
        flagType, playerid, name, payload);
    Log_Format(LOG_WARN, "[AC] %s", line);
    return 1;
}

stock Anticheat_GetFlagCount(playerid)
{
    return AnticheatFlagCount[playerid];
}
