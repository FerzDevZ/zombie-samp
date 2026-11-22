/* Telemetri zombie yang dipakai untuk mengukur kualitas navigasi.
   Dua angka ini adalah bukti, bukan opini: gigitan yang mendarat dan zombie
   yang didaur ulang karena macet. Kalau raycast pathfinding dimatikan, gigitan
   harus tetap tinggi dan recycle tetap rendah, kalau tidak navigasi mundur. */
#if defined _zombie_stats_included
    #endinput
#endif
#define _zombie_stats_included

new gZombieAttacksLanded = 0;
new gZombieStuckRecycled = 0;
new gZombieSpawnedTotal = 0;
new gZombieDestroyedTotal = 0;
new gZombieInRange = 0;
new gZombieLosFail = 0;
new Float:gZombieNearestDist = 9999.0;
new gZombieWithTarget = 0;
new gZombieSampled = 0;

stock ZombieStats_NoteProximity(Float:distance, bool:hasTarget)
{
    if (distance < gZombieNearestDist) gZombieNearestDist = distance;
    if (hasTarget) gZombieWithTarget++;
    gZombieSampled++;
    return 1;
}

stock ZombieStats_NoteAttack()
{
    gZombieAttacksLanded++;
    return 1;
}

stock ZombieStats_NoteRecycle()
{
    gZombieStuckRecycled++;
    return 1;
}

stock ZombieStats_NoteInRange()
{
    gZombieInRange++;
    return 1;
}

stock ZombieStats_NoteLosFail()
{
    gZombieLosFail++;
    return 1;
}

stock ZombieStats_NoteSpawn()
{
    gZombieSpawnedTotal++;
    return 1;
}

stock ZombieStats_NoteDestroy()
{
    gZombieDestroyedTotal++;
    return 1;
}

stock ZombieStats_SpawnedTotal()
{
    return gZombieSpawnedTotal;
}

stock ZombieStats_DestroyedTotal()
{
    return gZombieDestroyedTotal;
}

stock ZombieStats_Alive()
{
    new alive = gZombieSpawnedTotal - gZombieDestroyedTotal;
    return (alive < 0) ? 0 : alive;
}

/* Mengambil angka jendela lalu mengosongkannya: tiap baris PERF melaporkan
   aktivitas semenit, bukan angka kumulatif yang tak bisa dibandingkan. */
stock Float:ZombieStats_NearestDist()
{
    return gZombieNearestDist;
}

stock ZombieStats_WithTarget()
{
    return gZombieWithTarget;
}

stock ZombieStats_Sampled()
{
    return gZombieSampled;
}

stock ZombieStats_TakeWindow(&attacks, &recycled, &inRange, &losFail)
{
    gZombieNearestDist = 9999.0;
    gZombieWithTarget = 0;
    gZombieSampled = 0;
    attacks = gZombieAttacksLanded;
    recycled = gZombieStuckRecycled;
    inRange = gZombieInRange;
    losFail = gZombieLosFail;
    gZombieAttacksLanded = 0;
    gZombieStuckRecycled = 0;
    gZombieInRange = 0;
    gZombieLosFail = 0;
    return 1;
}
