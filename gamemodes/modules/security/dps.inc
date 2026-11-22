#if defined _ls_dps_included
    #endinput
#endif
#define _ls_dps_included

/* Deteksi DPS-spike: batas jumlah damage yang boleh keluar dari satu pemain dalam
   1 detik. Normalnya pistol ~50, rifle ~90; di atas 320 berarti ada macro, damage
   multiplier ilegal, atau injeksi packet. */

#define DPS_WINDOW_MS   1000
#define DPS_MAX_DAMAGE  320.0

new Float:DpsWindowDamage[MAX_PLAYERS];
new DpsWindowStart[MAX_PLAYERS];

stock Dps_Reset(playerid)
{
    DpsWindowDamage[playerid] = 0.0;
    DpsWindowStart[playerid] = 0;
    return 1;
}

/* Dipanggil pipeline sebelum damage zombie diterapkan. false = damage dibatalkan. */
stock bool:Dps_TrackPlayerDamage(attackerId, Float:amount, weaponId)
{
    if (attackerId == INVALID_PLAYER_ID || !IsPlayerConnected(attackerId)) return true;
    if (amount <= 0.0) return true;

    new now = GetTickCount();
    if (DpsWindowStart[attackerId] == 0 || now - DpsWindowStart[attackerId] > DPS_WINDOW_MS)
    {
        DpsWindowStart[attackerId] = now;
        DpsWindowDamage[attackerId] = 0.0;
    }

    DpsWindowDamage[attackerId] += amount;
    if (DpsWindowDamage[attackerId] <= DPS_MAX_DAMAGE) return true;

    new payload[64];
    format(payload, sizeof(payload), "dmg=%.0f weapon=%d", DpsWindowDamage[attackerId], weaponId);
    Anticheat_Flag(attackerId, "DPS_SPIKE", payload);

    // Reset supaya satu spike tidak memicu flag berulang tiap tembakan.
    DpsWindowDamage[attackerId] = 0.0;
    DpsWindowStart[attackerId] = now;
    return false;
}
