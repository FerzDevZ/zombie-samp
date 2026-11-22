#if defined _ls_anticheat_checks_included
    #endinput
#endif
#define _ls_anticheat_checks_included

/* Deteksi kecurangan berbasis bukti, bukan kecurigaan.
   Setiap temuan: 1 baris log, 1 baris DB (anticheat_flags), dan eskalasi bertingkat.
   Freeze lebih dulu, kick hanya setelah berulang dalam jendela waktu. */

new Float:ACLastSampleX[MAX_PLAYERS];
new Float:ACLastSampleY[MAX_PLAYERS];
new Float:ACLastSampleZ[MAX_PLAYERS];
new ACLastSampleTick[MAX_PLAYERS];
new bool:ACHasSample[MAX_PLAYERS];
new ACFlagWindowCount[MAX_PLAYERS];
new ACFlagWindowStart[MAX_PLAYERS];
new ACLastShotTick[MAX_PLAYERS][47];
new bool:ACFrozen[MAX_PLAYERS];

forward AC_Unfreeze(playerid);
forward AC_PositionTick();

stock AC_ResetPlayer(playerid)
{
    ACHasSample[playerid] = false;
    ACFlagWindowCount[playerid] = 0;
    ACFlagWindowStart[playerid] = 0;
    ACFrozen[playerid] = false;

    for (new weaponid = 0; weaponid < 47; weaponid++) ACLastShotTick[playerid][weaponid] = 0;
    return 1;
}

/* Teleport yang dilakukan server (masuk shelter, pindah lobby, ekstraksi, respawn)
   bukan kecurangan: baseline sampel harus dibuang supaya tidak tercatat TELEPORT. */
stock AC_ClearSample(playerid)
{
    if (playerid < 0 || playerid >= MAX_PLAYERS) return 0;
    ACHasSample[playerid] = false;
    return 1;
}

stock AC_RecordSample(playerid)
{
    GetPlayerPos(playerid, ACLastSampleX[playerid], ACLastSampleY[playerid], ACLastSampleZ[playerid]);
    ACLastSampleTick[playerid] = GetTickCount();
    ACHasSample[playerid] = true;
    return 1;
}

/* Setiap flag ditulis ke DB supaya staff bisa audit sebelum memutuskan apa pun. */
stock AC_StoreFlag(playerid, const flagType[], const payload[])
{
    if (!SQL_IsReady() || ActiveCharId[playerid] == 0) return 0;

    new query[512], msBuf[16];
    EpochMs(msBuf, sizeof(msBuf));
    mysql_format(gSQL, query, sizeof(query),
        "INSERT INTO anticheat_flags (char_id, flag_type, payload, created_at) VALUES (%d, '%e', '%e', %s)",
        ActiveCharId[playerid], flagType, payload, msBuf);
    mysql_tquery(gSQL, query, "", "");
    return 1;
}

stock AC_HandleFlag(playerid, const flagType[], const payload[])
{
    Anticheat_Flag(playerid, flagType, payload);
    AC_StoreFlag(playerid, flagType, payload);

    new now = GetTickCount();
    if (now - ACFlagWindowStart[playerid] > AC_FLAG_WINDOW_MS)
    {
        ACFlagWindowStart[playerid] = now;
        ACFlagWindowCount[playerid] = 0;
    }
    ACFlagWindowCount[playerid]++;

    if (ACFlagWindowCount[playerid] >= AC_KICK_THRESHOLD)
    {
        Log_Format(LOG_WARN, "[AC] player=%d melewati ambang (%d flag dalam %d ms), kick.",
            playerid, ACFlagWindowCount[playerid], AC_FLAG_WINDOW_MS);
        Kick(playerid);
        return 1;
    }

    if (ACFlagWindowCount[playerid] >= 3 && !ACFrozen[playerid])
    {
        ACFrozen[playerid] = true;
        TogglePlayerControllable(playerid, false);
        SetTimerEx("AC_Unfreeze", AC_WARN_FREEZE_MS, false, "i", playerid);
        SendClientMessage(playerid, UI_ACCENT_WARN, "[AC] Aktivitas tidak wajar terdeteksi. Kontrol dibekukan sebentar.");
    }
    return 1;
}

public AC_Unfreeze(playerid)
{
    if (!IsPlayerConnected(playerid)) return 0;
    ACFrozen[playerid] = false;
    /* Freeze ADMIN tidak boleh dibatalkan oleh unfreeze AC (audit F-16):
       cheater bisa sengaja memicu flag AC untuk melepas freeze admin. */
    if (Player_IsAlive(playerid) && !AdminFrozen[playerid]) TogglePlayerControllable(playerid, true);
    return 1;
}

/* Sampel posisi 1 detik: deteksi lompatan dan lari super cepat. */
public AC_PositionTick()
{
    foreach (new playerid : PlayerPool)
    {
        if (!Account_IsPlaying(playerid)) continue;
        if (!Player_IsAlive(playerid)) continue;
        /* Kendaraan TETAP disampel dengan ambang kendaraan (audit F-14). */
        new bool:inVehicle = (GetPlayerState(playerid) == PLAYER_STATE_DRIVER
            || GetPlayerState(playerid) == PLAYER_STATE_PASSENGER);
        if (GetTickCount() - PlayerSurvival[playerid][psGraceUntil] < 0) continue;  /* wrap-safe */
        if (ACFrozen[playerid]) continue;

        new Float:x, Float:y, Float:z;
        GetPlayerPos(playerid, x, y, z);

        if (!ACHasSample[playerid])
        {
            ACLastSampleX[playerid] = x;
            ACLastSampleY[playerid] = y;
            ACLastSampleZ[playerid] = z;
            ACLastSampleTick[playerid] = GetTickCount();
            ACHasSample[playerid] = true;
            continue;
        }

        new elapsed = GetTickCount() - ACLastSampleTick[playerid];
        if (elapsed < 250) continue;

        new Float:deltaX = x - ACLastSampleX[playerid];
        new Float:deltaY = y - ACLastSampleY[playerid];
        new Float:deltaZ = z - ACLastSampleZ[playerid];
        new Float:distance = floatsqroot(deltaX * deltaX + deltaY * deltaY + deltaZ * deltaZ);
        new Float:speed = distance / (float(elapsed) / 1000.0);

        ACLastSampleX[playerid] = x;
        ACLastSampleY[playerid] = y;
        ACLastSampleZ[playerid] = z;
        ACLastSampleTick[playerid] = GetTickCount();

        if (distance > (inVehicle ? AC_TELEPORT_DIST_VEH : AC_TELEPORT_DIST))
        {
            new payload[96];
            format(payload, sizeof(payload), "jarak=%.1f dalam=%dms veh=%d", distance, elapsed, inVehicle);
            AC_HandleFlag(playerid, "TELEPORT", payload);
            continue;
        }

        if (speed > (inVehicle ? AC_MAX_VEHICLE_SPEED : AC_MAX_FOOT_SPEED))
        {
            new payload[96];
            format(payload, sizeof(payload), "kecepatan=%.1f m/s jarak=%.1f veh=%d", speed, distance, inVehicle);
            AC_HandleFlag(playerid, "SPEED", payload);
        }
    }
    return 1;
}

/* Interval tembakan minimum per senjata; toleransi 20 ms untuk jitter jaringan. */
stock AC_MinimumShotInterval(weaponid)
{
    switch (weaponid)
    {
        case 22: return 110;   // pistol
        case 23: return 110;   // silenced pistol
        case 24: return 320;   // desert eagle
        case 25: return 650;   // shotgun
        case 26: return 900;   // sawnoff
        case 27: return 900;   // combat shotgun
        case 28: return 90;    // uzi
        case 29: return 90;    // mp5
        case 30: return 800;   // ak47
        case 31: return 800;   // m4
        case 32: return 90;    // tec9
        case 33: return 900;   // rifle
        case 34: return 1200;  // sniper
        /* Senjata berat sebelumnya tanpa cek interval (audit F-15):
           rapid-fire minigun/launcher lolos deteksi. */
        case 35: return 2500;  // rocket launcher
        case 36: return 2500;  // heat seeker
        case 37: return 120;   // flamethrower
        case 38: return 80;    // minigun
    }
    return 0;
}

stock AC_OnWeaponShot(playerid, weaponid)
{
    new minimum = AC_MinimumShotInterval(weaponid);
    if (minimum == 0) return 1;

    new now = GetTickCount();
    new last = ACLastShotTick[playerid][weaponid];
    ACLastShotTick[playerid][weaponid] = now;

    if (last == 0) return 1;

    new interval = now - last;
    if (interval + AC_RAPID_TOLERANCE_MS >= minimum) return 1;

    new payload[96];
    format(payload, sizeof(payload), "weapon=%d interval=%dms minimum=%dms", weaponid, interval, minimum);
    AC_HandleFlag(playerid, "RAPID_FIRE", payload);
    return 0;
}

stock AC_GetFlagWindowCount(playerid)
{
    if (GetTickCount() - ACFlagWindowStart[playerid] > AC_FLAG_WINDOW_MS) return 0;
    return ACFlagWindowCount[playerid];
}
