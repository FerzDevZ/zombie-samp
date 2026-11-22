#if defined _ls_perf_included
    #endinput
#endif
#define _ls_perf_included

/* Pengukuran biaya tiap timer gameplay. GetTickCount hanya punya resolusi 1 ms,
   jadi angka ini untuk mencari lonjakan (peak), bukan profiler instruksi. */

#define PERF_ZOMBIE 0
#define PERF_DIRECTOR 1
#define PERF_BASE 2
#define PERF_HUD 3
#define PERF_PLAYER 4
#define PERF_LOOT 5
#define PERF_VEHICLE 6
#define PERF_NEEDS 7
#define PERF_EVENT 8
#define PERF_HORDE 9
#define PERF_AUTOSAVE 10
#define PERF_SYSTEM_COUNT 11
#define PERF_WINDOW_MS 60000

new PerfLastMs[PERF_SYSTEM_COUNT];
new PerfPeakMs[PERF_SYSTEM_COUNT];
new PerfWindowStartTick = 0;

stock Perf_GetLast(system)
{
    if (system < 0 || system >= PERF_SYSTEM_COUNT) return 0;
    return PerfLastMs[system];
}

stock Perf_GetPeak(system)
{
    if (system < 0 || system >= PERF_SYSTEM_COUNT) return 0;
    return PerfPeakMs[system];
}

stock Perf_Update(system, startTick)
{
    if (system < 0 || system >= PERF_SYSTEM_COUNT) return 0;
    if (startTick == 0) return 0;

    new elapsed = GetTickCount() - startTick;
    if (elapsed < 0) elapsed = 0;

    PerfLastMs[system] = elapsed;
    if (elapsed > PerfPeakMs[system]) PerfPeakMs[system] = elapsed;

    new now = GetTickCount();
    if (PerfWindowStartTick == 0)
    {
        PerfWindowStartTick = now;
        return 1;
    }

    if (now - PerfWindowStartTick < PERF_WINDOW_MS) return 1;

    new statAttacks, statRecycled, statInRange, statLosFail;
    new statSampled = ZombieStats_Sampled();
    new statTargets = ZombieStats_WithTarget();
    new Float:statNearest = ZombieStats_NearestDist();
    ZombieStats_TakeWindow(statAttacks, statRecycled, statInRange, statLosFail);

    Log_Format(LOG_INFO,
        "[PERF] tick ms (akhir/puncak): zombie=%d/%d director=%d/%d base=%d/%d hud=%d/%d player=%d/%d loot=%d/%d veh=%d/%d needs=%d/%d event=%d/%d horde=%d/%d save=%d/%d | zombie: alive=%d lahir=%d mati=%d atk=%d macet=%d jangkau=%d los_gagal=%d sampel=%d target=%d dekat=%.1f",
        PerfLastMs[PERF_ZOMBIE], PerfPeakMs[PERF_ZOMBIE],
        PerfLastMs[PERF_DIRECTOR], PerfPeakMs[PERF_DIRECTOR],
        PerfLastMs[PERF_BASE], PerfPeakMs[PERF_BASE],
        PerfLastMs[PERF_HUD], PerfPeakMs[PERF_HUD],
        PerfLastMs[PERF_PLAYER], PerfPeakMs[PERF_PLAYER],
        PerfLastMs[PERF_LOOT], PerfPeakMs[PERF_LOOT],
        PerfLastMs[PERF_VEHICLE], PerfPeakMs[PERF_VEHICLE],
        PerfLastMs[PERF_NEEDS], PerfPeakMs[PERF_NEEDS],
        PerfLastMs[PERF_EVENT], PerfPeakMs[PERF_EVENT],
        PerfLastMs[PERF_HORDE], PerfPeakMs[PERF_HORDE],
        PerfLastMs[PERF_AUTOSAVE], PerfPeakMs[PERF_AUTOSAVE],
        ZombieStats_Alive(), ZombieStats_SpawnedTotal(), ZombieStats_DestroyedTotal(),
        statAttacks, statRecycled, statInRange, statLosFail,
        statSampled, statTargets, statNearest);

    for (new index = 0; index < PERF_SYSTEM_COUNT; index++)
    {
        PerfPeakMs[index] = 0;
    }
    PerfWindowStartTick = now;
    return 1;
}
