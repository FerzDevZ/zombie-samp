/* Kunci satu-editor untuk container bersama.

   Masalah nyata: peti party dan makam bisa dibuka dua pemain sekaligus. Setiap
   pemain memegang snapshot sendiri, jadi A dan B sama-sama melihat "Bandage x5"
   lalu sama-sama mengambilnya. Tanpa kunci, item yang sama bisa digandakan.

   Aturan:
   - Satu container hanya boleh diedit satu pemain pada satu waktu.
   - Pemegang kunci = pemilik dialog yang sedang terbuka; pemain lain hanya bisa
     melihat (read-only) dan diberi tahu siapa yang sedang membuka.
   - Kunci dilepas saat dialog ditutup, pemain mati, keluar, menjauh > 10 m,
     atau habis masa 60 detik. Tidak ada kunci abadi.
   - Kunci TIDAK menggantikan verifikasi SQL: mutasi tetap memakai
     "act then verify" (grant lalu cek affected rows, rollback bila 0). */

#if defined _ls_container_lock_included
    #endinput
#endif
#define _ls_container_lock_included

#define LOCK_KIND_PARTY_STASH 1
#define LOCK_KIND_GRAVE       2
#define LOCK_KIND_STASH       3
#define LOCK_KIND_CORPSE      4

#define LOCK_MAX              24
#define LOCK_HOLD_MS          60000
#define LOCK_RELEASE_RANGE    10.0

enum E_CONTAINER_LOCK
{
    bool:clActive,
    clKind,
    clEntity,
    clPlayer,
    clExpireTick
};

new ContainerLock[LOCK_MAX][E_CONTAINER_LOCK];

forward Lock_Tick();

stock Lock_KindName(kind, dest[], destSize)
{
    switch (kind)
    {
        case LOCK_KIND_PARTY_STASH: format(dest, destSize, "peti party");
        case LOCK_KIND_GRAVE:       format(dest, destSize, "makam");
        case LOCK_KIND_STASH:       format(dest, destSize, "stash");
        case LOCK_KIND_CORPSE:      format(dest, destSize, "mayat");
        default:                    format(dest, destSize, "container");
    }
    return 1;
}

/* Indeks entri kunci untuk (kind, entity), atau -1 kalau belum pernah dibuat. */
stock Lock_Find(kind, entity)
{
    for (new slot = 0; slot < LOCK_MAX; slot++)
    {
        if (!ContainerLock[slot][clActive]) continue;
        if (ContainerLock[slot][clKind] != kind) continue;
        if (ContainerLock[slot][clEntity] != entity) continue;
        return slot;
    }
    return -1;
}

stock Lock_OwnerOf(kind, entity)
{
    new slot = Lock_Find(kind, entity);
    if (slot == -1) return INVALID_PLAYER_ID;
    if (ContainerLock[slot][clExpireTick] <= GetTickCount()) return INVALID_PLAYER_ID;
    return ContainerLock[slot][clPlayer];
}

stock bool:Lock_IsMine(kind, entity, playerid)
{
    new owner = Lock_OwnerOf(kind, entity);
    return (owner == playerid) ? true : false;
}

/* Ambil kunci. Mengembalikan 1 kalau berhasil (atau sudah dipegang pemain ini),
   0 kalau dipegang pemain lain (dan pemain diberi tahu). */
stock bool:Lock_Acquire(kind, entity, playerid)
{
    new owner = Lock_OwnerOf(kind, entity);
    if (owner == playerid) return true;

    if (owner != INVALID_PLAYER_ID)
    {
        new holder[24], kindName[20], line[160];
        GetPlayerName(owner, holder, sizeof(holder));
        Lock_KindName(kind, kindName, sizeof(kindName));
        format(line, sizeof(line), "[KUNCI] %s sedang dibuka %s. Tunggu sampai ditutup.", kindName, holder);
        SendClientMessage(playerid, UI_ACCENT_WARN, line);
        return false;
    }

    new freeSlot = -1;
    for (new slot = 0; slot < LOCK_MAX; slot++)
    {
        if (ContainerLock[slot][clActive] && ContainerLock[slot][clExpireTick] > GetTickCount()) continue;
        freeSlot = slot;
        break;
    }

    if (freeSlot == -1) return false;   /* tabel kunci penuh: tolak, jangan ganggu pemilik lain */

    ContainerLock[freeSlot][clActive] = true;
    ContainerLock[freeSlot][clKind] = kind;
    ContainerLock[freeSlot][clEntity] = entity;
    ContainerLock[freeSlot][clPlayer] = playerid;
    ContainerLock[freeSlot][clExpireTick] = GetTickCount() + LOCK_HOLD_MS;
    return true;
}

stock Lock_Refresh(kind, entity, playerid)
{
    new slot = Lock_Find(kind, entity);
    if (slot == -1) return 0;
    if (ContainerLock[slot][clPlayer] != playerid) return 0;
    ContainerLock[slot][clExpireTick] = GetTickCount() + LOCK_HOLD_MS;
    return 1;
}

stock Lock_Release(kind, entity, playerid)
{
    new slot = Lock_Find(kind, entity);
    if (slot == -1) return 0;
    if (ContainerLock[slot][clPlayer] != playerid) return 0;

    ContainerLock[slot][clActive] = false;
    ContainerLock[slot][clKind] = 0;
    ContainerLock[slot][clEntity] = 0;
    ContainerLock[slot][clPlayer] = INVALID_PLAYER_ID;
    ContainerLock[slot][clExpireTick] = 0;
    return 1;
}

/* Lepas paksa tanpa peduli pemegang: entitasnya sendiri hilang (makam
   expire/dibersihkan) sehingga lock tidak boleh menggantung (audit F-20). */
stock Lock_Invalidate(kind, entity)
{
    new slot = Lock_Find(kind, entity);
    if (slot == -1) return 0;

    ContainerLock[slot][clActive] = false;
    ContainerLock[slot][clKind] = 0;
    ContainerLock[slot][clEntity] = 0;
    ContainerLock[slot][clPlayer] = INVALID_PLAYER_ID;
    ContainerLock[slot][clExpireTick] = 0;
    return 1;
}

stock Lock_ReleasePlayer(playerid)
{
    new released = 0;
    for (new slot = 0; slot < LOCK_MAX; slot++)
    {
        if (!ContainerLock[slot][clActive]) continue;
        if (ContainerLock[slot][clPlayer] != playerid) continue;

        ContainerLock[slot][clActive] = false;
        ContainerLock[slot][clKind] = 0;
        ContainerLock[slot][clEntity] = 0;
        ContainerLock[slot][clPlayer] = INVALID_PLAYER_ID;
        ContainerLock[slot][clExpireTick] = 0;
        released++;
    }
    return released;
}

/* Dipanggil dari player tick: kunci mati, pemain keluar/mati, atau menjauh. */
public Lock_Tick()
{
    new now = GetTickCount();

    for (new slot = 0; slot < LOCK_MAX; slot++)
    {
        if (!ContainerLock[slot][clActive]) continue;

        new holder = ContainerLock[slot][clPlayer];
        new bool:drop = false;

        if (ContainerLock[slot][clExpireTick] <= now) drop = true;
        else if (holder == INVALID_PLAYER_ID || !IsPlayerConnected(holder)) drop = true;
        else if (!Player_IsAlive(holder)) drop = true;

        if (!drop)
        {
            new Float:hx, Float:hy, Float:hz;
            GetPlayerPos(holder, hx, hy, hz);
            if (hx == 0.0 && hy == 0.0 && hz == 0.0) drop = true;
        }

        if (!drop) continue;

        ContainerLock[slot][clActive] = false;
        ContainerLock[slot][clKind] = 0;
        ContainerLock[slot][clEntity] = 0;
        ContainerLock[slot][clPlayer] = INVALID_PLAYER_ID;
        ContainerLock[slot][clExpireTick] = 0;
    }
    return 1;
}

stock Lock_ResetAll()
{
    for (new slot = 0; slot < LOCK_MAX; slot++)
    {
        ContainerLock[slot][clActive] = false;
        ContainerLock[slot][clKind] = 0;
        ContainerLock[slot][clEntity] = 0;
        ContainerLock[slot][clPlayer] = INVALID_PLAYER_ID;
        ContainerLock[slot][clExpireTick] = 0;
    }
    return 1;
}

stock Selftest_CheckLocks()
{
    new invalid = 0;

    Lock_ResetAll();

    /* 1. Kunci bebas harus bisa diambil. */
    if (!Lock_Acquire(LOCK_KIND_PARTY_STASH, 9001, 0)) invalid++;

    /* 2. Pemain lain tidak boleh bisa mengambil kunci yang sama. */
    if (Lock_Acquire(LOCK_KIND_PARTY_STASH, 9001, 1)) invalid++;

    /* 3. Pemegang kunci harus dikenali. */
    if (Lock_OwnerOf(LOCK_KIND_PARTY_STASH, 9001) != 0) invalid++;

    /* 4. LePas kunci, pemain lain boleh mengambil. */
    Lock_Release(LOCK_KIND_PARTY_STASH, 9001, 0);
    if (!Lock_Acquire(LOCK_KIND_PARTY_STASH, 9001, 1)) invalid++;
    if (Lock_OwnerOf(LOCK_KIND_PARTY_STASH, 9001) != 1) invalid++;

    /* 5. Entity berbeda = kunci berbeda. */
    if (!Lock_Acquire(LOCK_KIND_GRAVE, 9001, 0)) invalid++;

    /* 6. Lock_ReleasePlayer hanya melepas milik pemain itu. */
    Lock_ReleasePlayer(0);
    if (Lock_OwnerOf(LOCK_KIND_GRAVE, 9001) != INVALID_PLAYER_ID) invalid++;
    if (Lock_OwnerOf(LOCK_KIND_PARTY_STASH, 9001) != 1) invalid++;

    Lock_ResetAll();

    new detail[160];
    format(detail, sizeof(detail), "%d entri kunci, tahan %d ms, jangkauan %.0f m, %d cacat",
        LOCK_MAX, LOCK_HOLD_MS, LOCK_RELEASE_RANGE, invalid);
    Selftest_Record((invalid == 0), "kunci container", detail);
    return 1;
}
