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
#define NPC_FAIL_COOLDOWN_MS 10000
#define NPC_BUDGET_WARN_MS 10000

static NpcBudgetLastWarnTick = 0;

/* Sensus slot = pemain nyata + NPC milik kita.
   Dua hitungan sebelumnya sama-sama salah:
   - IsPlayerConnected saja: NPC FCNPC tidak selalu connected, jadi guard
     meloloskan create saat pool penuh (`FCNPC_Create gagal ... poolsize=399
     pakai=300`, 22-11 07:51).
   - GetPlayerPoolSize()+1 saja: FCNPC memakai id tinggi, poolsize langsung
     399 walau pemain 1, sehingga semua zombie ditolak (soak 08:11, zombie 0).
   Karena itu NPC dihitung dari counter sendiri, bukan dari API pemain. */
forward Zombie_NpcCountForBudget();

/* Setiap FCNPC_Create yang gagal tetap memakan satu id pemain di server:
   poolsize naik ke 399 walau pemain+NPC baru 369 (soak 22-11 11:22, 79 kali
   gagal). Karena itu kegagalan dicatat sebagai slot hilang dan pembuatan NPC
   dihentikan sementara, bukan dicoba ulang tiap tick. */
static NpcLostSlots = 0;
static NpcCreateBlockedUntil = 0;

stock Npc_NoteCreateFailure()
{
    if (NpcLostSlots < 200) NpcLostSlots++;
    NpcCreateBlockedUntil = GetTickCount() + NPC_FAIL_COOLDOWN_MS;
    return 1;
}

stock Npc_LostSlots()
{
    return NpcLostSlots;
}

stock bool:Npc_CreatePaused()
{
    return (GetTickCount() < NpcCreateBlockedUntil);
}

stock Npc_OccupiedSlots()
{
    new players = 0;
    new highestSlot = GetPlayerPoolSize();
    for (new slot = 0; slot <= highestSlot; slot++)
    {
        if (IsPlayerConnected(slot)) players++;
    }
    return players + Zombie_NpcCountForBudget() + NpcLostSlots;
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

/* NPC adalah player slot di sisi server, jadi klien menggambar nametag di atas
   mereka — termasuk di dalam mobil, tempat nametag muncul di TENGAH layar
   ("ls_comp_0_101562 (278)") dan terbaca pemain sebagai bug HUD. Nametag NPC
   memang tidak pernah berguna: zombie dan rekrutan dikenali dari bentuknya. */
stock Npc_HideNameTag(npcid)
{
    if (npcid < 0 || npcid == INVALID_PLAYER_ID) return 0;

    for (new playerid = 0; playerid < MAX_PLAYERS; playerid++)
    {
        if (!IsPlayerConnected(playerid)) continue;
        ShowPlayerNameTagForPlayer(playerid, npcid, false);
        SetPlayerMarkerForPlayer(playerid, npcid, 0x00000000);
    }
    return 1;
}

/* Dipanggil saat pemain connect: nametag NPC yang SUDAH ada ikut disembunyikan.
   NPC yang lahir belakangan ditangani Npc_HideNameTag saat dibuat. */
stock Npc_HideAllNameTags(playerid)
{
    for (new npcid = 0; npcid < MAX_PLAYERS; npcid++)
    {
        if (!IsPlayerConnected(npcid)) continue;
        if (!IsPlayerNPC(npcid)) continue;
        ShowPlayerNameTagForPlayer(playerid, npcid, false);
        SetPlayerMarkerForPlayer(playerid, npcid, 0x00000000);
    }
    return 1;
}
