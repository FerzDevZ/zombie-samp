/*
    NPC_BUDGET — anggaran slot NPC (zombie, companion, escort).

    SA-MP memakai SATU pool slot untuk pemain DAN NPC (FCNPC memakai slot pemain).
    Bila slot penuh, FCNPC_Create gagal setelah sebagian state internal terbentuk,
    lalu RakNet abort saat slot dibersihkan:

        ../raknet/DS_OrderedList.h:185:
        OrderedList<PlayerID, PlayerIDAndIndex>::Remove:
        Assertion `objectExists==true' failed.   -> SIGABRT

    Crash itu terbukti pada uji beban 100 bot (runtime/s1/console.log, 13:35:15).
    Karena itu SEMUA pembuatan NPC wajib lewat Npc_SlotAvailable() dulu, dan
    budget director dibatasi slot bebas, bukan hanya MAX_ZOMBIES.
*/

#define NPC_SLOT_RESERVE 8
#define NPC_BUDGET_WARN_MS 10000

static NpcBudgetLastWarnTick = 0;

/* Sensus slot: hanya menghitung slot terpakai, bukan logika pemain.
   Batas loop = GetPlayerPoolSize() (slot tertinggi yang terpakai), bukan MAX_PLAYERS. */
stock Npc_OccupiedSlots()
{
    new highestSlot = GetPlayerPoolSize();
    new occupied = 0;
    for (new slot = 0; slot <= highestSlot; slot++)
    {
        if (IsPlayerConnected(slot)) occupied++;
    }
    return occupied;
}

stock Npc_SlotsFree()
{
    new freeSlots = GetMaxPlayers() - Npc_OccupiedSlots();
    if (freeSlots < 0) freeSlots = 0;
    return freeSlots;
}

stock bool:Npc_SlotAvailable()
{
    return Npc_SlotsFree() > NPC_SLOT_RESERVE;
}

/* Jatah NPC baru yang boleh dibuat saat ini (0 bila cadangan slot tersentuh). */
stock Npc_SpawnRoom()
{
    new freeSlots = Npc_SlotsFree();
    if (freeSlots <= NPC_SLOT_RESERVE) return 0;
    return freeSlots - NPC_SLOT_RESERVE;
}

/* Log ditahan 10 detik supaya horde yang gagal spawn tidak membanjiri log. */
stock Npc_BudgetWarn(const source[])
{
    new now = GetTickCount();
    if (NpcBudgetLastWarnTick != 0 && (now - NpcBudgetLastWarnTick) < NPC_BUDGET_WARN_MS) return 0;
    NpcBudgetLastWarnTick = now;

    Log_Format(LOG_WARN, "[NPC] slot penuh: %s ditolak (terpakai %d/%d, cadangan %d)",
        source, Npc_OccupiedSlots(), GetMaxPlayers(), NPC_SLOT_RESERVE);
    return 1;
}
