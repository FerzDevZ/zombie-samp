#if defined _ls_scent_included
    #endinput
#endif
#define _ls_scent_included

/* Kamuflase bau zombie: pemain melumuri tubuh dengan darah/isi perut zombie
   yang BARU mati supaya zombie tidak menganggapnya mangsa. Efeknya hilang
   begitu ia berlari, menembak, atau terluka — bau manusia menyebar lagi.

   Aturan desain:
   - Sumber harus bangkai zombie segar (< 90 detik) dan belum pernah dipakai.
   - Tidak ada item gratis: "biaya" adalah risiko mendekati bangkai.
   - Filter mangsa ada di Hash_NearestPlayer (satu titik, semua zombie).
   - Zombie yang SEDANG mengejar dilepas lewat ZombieAI_DropTargetOf. */

#define SCENT_DURATION_S        120
#define SCENT_RADIUS_M          3.0
#define SCENT_CORPSE_FRESH_MS   90000
#define SCENT_SPRINT_SPEED      0.30

new ScentUntil[MAX_PLAYERS];   // epoch detik; 0 = tidak aktif

forward bool:Scent_IsMasked(playerid);
stock bool:Scent_IsMasked(playerid)
{
    if (playerid < 0 || playerid >= MAX_PLAYERS) return false;
    if (ScentUntil[playerid] == 0) return false;
    return gettime() < ScentUntil[playerid];
}

/* Lepas semua zombie yang sedang mengejar pemain ini: kamuflase baru saja
   dipasang, zombie yang sudah di belakangnya tidak boleh terus mengejar. */
stock Scent_Apply(playerid)
{
    if (!Player_IsAlive(playerid))
    {
        Toast_Show(playerid, "BAU ZOMBIE", "Kamu tidak bisa melumuri tubuh dalam kondisi ini.", UI_ACCENT_WARN);
        return 0;
    }
    if (Scent_IsMasked(playerid))
    {
        Toast_Show(playerid, "BAU ZOMBIE", "Kamu sudah berlumur darah zombie.", UI_TEXT_MUTED);
        return 0;
    }
    if (Zone_IsHub(PlayerSurvival[playerid][psZone]))
    {
        Toast_Show(playerid, "BAU ZOMBIE", "Tidak ada gunanya menyamar di zona aman.", UI_ACCENT_WARN);
        return 0;
    }

    new slot = Corpse_Nearest(playerid, SCENT_RADIUS_M);
    if (slot < 0)
    {
        Toast_Show(playerid, "BAU ZOMBIE", "Tidak ada bangkai zombie di dekatmu. Bunuh satu dulu.", UI_ACCENT_WARN);
        return 0;
    }

    new createdTick = Corpse[slot][cpExpireTick] - LOOT_CORPSE_EXPIRE_MS;
    if (GetTickCount() - createdTick > SCENT_CORPSE_FRESH_MS)
    {
        Toast_Show(playerid, "BAU ZOMBIE", "Bangkainya terlalu lama — bau darahnya sudah hilang.", UI_ACCENT_WARN);
        return 0;
    }
    if (Corpse[slot][cpScentUsed])
    {
        Toast_Show(playerid, "BAU ZOMBIE", "Bangkai ini sudah dimanfaatkan orang lain.", UI_ACCENT_WARN);
        return 0;
    }

    Corpse[slot][cpScentUsed] = true;
    ScentUntil[playerid] = gettime() + SCENT_DURATION_S;
    ZombieAI_DropTargetOf(playerid);

    Toast_Show(playerid, "BAU ZOMBIE",
        "Kamuflase aktif 2 menit. Berjalan pelan — jangan lari, jangan menembak, jangan terluka.",
        UI_ACCENT_PRIMARY);
    Log_Format(LOG_INFO, "[SCENT] player %d melumuri diri dari bangkai #%d.", playerid, slot);
    return 1;
}

stock Scent_Break(playerid, const reason[])
{
    if (!Scent_IsMasked(playerid)) return 0;

    ScentUntil[playerid] = 0;
    Toast_Show(playerid, "KAMUFLASE PECAH", reason, UI_ACCENT_DANGER);
    Log_Format(LOG_INFO, "[SCENT] kamuflase player %d pecah: %s", playerid, reason);
    return 1;
}

/* Jalur 500 ms dari Player_Tick: kedaluwarsa + deteksi lari.
   Tidak ada logika gameplay lain di sini (invariant OnPlayerUpdate). */
stock Scent_Tick(playerid)
{
    if (ScentUntil[playerid] == 0) return 0;

    if (gettime() >= ScentUntil[playerid])
    {
        ScentUntil[playerid] = 0;
        Toast_Show(playerid, "BAU ZOMBIE", "Bau kamuflase memudar. Kamu manusia lagi di mata mereka.", UI_ACCENT_WARN);
        return 0;
    }

    if (IsPlayerInAnyVehicle(playerid)) return 0;

    new keys, updown, leftright;
    GetPlayerKeys(playerid, keys, updown, leftright);
    if ((keys & KEY_SPRINT) == 0) return 0;

    new Float:velX, Float:velY, Float:velZ;
    GetPlayerVelocity(playerid, velX, velY, velZ);
    if (VectorSize(velX, velY, velZ) > SCENT_SPRINT_SPEED)
    {
        Scent_Break(playerid, "Kamu berlari — bau manusiamu menyebar.");
    }
    return 1;
}

stock Scent_ResetPlayer(playerid)
{
    ScentUntil[playerid] = 0;
    return 1;
}

stock Scent_Selftest()
{
    /* Transisi murni tanpa pemain: state array bisa diuji langsung. */
    new bool:ok = !Scent_IsMasked(0);
    ScentUntil[0] = gettime() + 60;
    ok = ok && Scent_IsMasked(0);
    ScentUntil[0] = gettime() - 1;
    ok = ok && !Scent_IsMasked(0);
    ScentUntil[0] = 0;

    new bool:passed = ok;
    if (SCENT_DURATION_S != 120 || SCENT_RADIUS_M != 3.0) passed = false;
    Selftest_Record(passed,
        "kamuflase bau", ok ? "transisi mask/break/expiry konsisten" : "transisi state salah");
    return 1;
}
