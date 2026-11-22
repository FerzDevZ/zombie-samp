#if defined _ls_zombie_core_included
    #endinput
#endif
#define _ls_zombie_core_included

enum E_ZOMBIE_ENTITY
{
    bool:zeActive,
    bool:zeDead,
    E_ZOMBIE_TYPE:zeType,
    Float:zeHealth,
    Float:zeMaxHealth,
    zeLastAttacker,
    zeSpawnTick,
    zeDeathTick,
    zeLobby,
    zeGeneration
};

new ZombieEntity[MAX_ZOMBIE_SLOTS][E_ZOMBIE_ENTITY];
new Iterator:ZombiePool<MAX_ZOMBIE_SLOTS>;
new ZombieActiveCount = 0;
/* Generasi naik tiap spawn: timer cleanup lama tidak boleh menghancurkan
   zombie baru yang kebetulan memakai ulang id FCNPC yang sama. */
new ZombieGenerationCounter = 0;

forward Zombie_OnDeathCleanup(zombieid, generation);
forward ZombieAI_Reset(zombieid);
forward ZombieAI_OnZombieKilled(zombieid, E_ZOMBIE_TYPE:type, Float:x, Float:y, Float:z);
forward Loot_DropFromZombie(E_ZOMBIE_TYPE:type, Float:x, Float:y, Float:z, lobbyId);

stock Zombie_IsActive(zombieid)
{
    if (zombieid < 0 || zombieid >= MAX_ZOMBIE_SLOTS) return 0;
    return ZombieEntity[zombieid][zeActive] && !ZombieEntity[zombieid][zeDead];
}

stock Zombie_GetType(zombieid)
{
    return _:ZombieEntity[zombieid][zeType];
}

stock Float:Zombie_GetHealth(zombieid)
{
    if (!Zombie_IsActive(zombieid)) return 0.0;
    return ZombieEntity[zombieid][zeHealth];
}

stock Float:Zombie_GetMaxHealth(zombieid)
{
    if (!Zombie_IsActive(zombieid)) return 0.0;
    if (ZombieEntity[zombieid][zeMaxHealth] <= 0.0) return 1.0;
    return ZombieEntity[zombieid][zeMaxHealth];
}

stock Zombie_GetLobby(zombieid)
{
    if (zombieid < 0 || zombieid >= MAX_ZOMBIE_SLOTS) return 0;
    return ZombieEntity[zombieid][zeLobby];
}

stock Zombie_GetActiveCount()
{
    return ZombieActiveCount;
}

/* Semakin ramai server, semakin mahal tiap NPC mengirim sync. Turunkan rate
   hanya saat perlu; satu pemain tetap mendapat 80 ms agar animasi tetap halus. */
new ZombieSyncRate = FCNPC_SYNC_RATE_MS;
new ZombieSyncRateTick = 0;

stock Zombie_UpdateSyncRate()
{
    if (GetTickCount() - ZombieSyncRateTick < 5000) return 0;
    ZombieSyncRateTick = GetTickCount();

    /* Yang membebani server bukan jumlah pemain total, tapi berapa NPC
       ter-stream ke berapa pemain. Jadi ukur kepadatan zona terpadat. */
    new zoneCount[MAX_ZONE_COUNT];
    new dense = 0;
    foreach (new playerid : PlayerPool)
    {
        if (!IsPlayerConnected(playerid)) continue;
        if (IsPlayerNPC(playerid)) continue;
        new zone = Player_GetZone(playerid);
        if (zone < 0 || zone >= MAX_ZONE_COUNT) continue;
        zoneCount[zone]++;
        if (zoneCount[zone] > dense) dense = zoneCount[zone];
    }

    new rate = FCNPC_SYNC_RATE_MS;
    if (dense >= FCNPC_DENSE_THRESHOLD)
    {
        rate = FCNPC_SYNC_RATE_DENSE_MS;
    }
    else if (dense >= FCNPC_CROWD_THRESHOLD)
    {
        rate = FCNPC_SYNC_RATE_CROWD_MS;
    }

    if (rate == ZombieSyncRate) return 0;

    ZombieSyncRate = rate;
    FCNPC_SetUpdateRate(rate);
    Log_Format(LOG_INFO, "[PERF] sync NPC disesuaikan ke %d ms.", rate);
    return 1;
}

/* Setiap FCNPC_Create/FCNPC_Destroy membuat samp03svr mengalokasikan lalu
   menghapus satu entri pemain. Alokasi itu tidak kembali penuh (~0,8 MB per
   siklus, terukur di soak 8 menit: 130 siklus = 163 MB), jadi NPC zombie
   diparkir di dunia terpisah dan dihidupkan lagi saat spawn berikutnya. */
#define ZOMBIE_POOL_PARKED_WORLD   (60000)
/* Pool parkir berbagi slot pemain dengan bot/actor. Tanpa batas, 91 NPC parkir
   pernah memakan 293/300 slot (soak 22-11 07:12) dan server mati saat slot
   habis. 48 cukup untuk menutup churn director tanpa mengunci slot pemain. */
#define ZOMBIE_PARK_LIMIT          48

new ZombieParkedIds[MAX_ZOMBIE_SLOTS];
new ZombieParkedCount = 0;

forward Zombie_QueueDestroy(zombieid);

stock Zombie_Park(zombieid)
{
    if (ZombieParkedCount >= ZOMBIE_PARK_LIMIT)
    {
        /* Buang lewat antrean, bukan FCNPC_Destroy langsung: destroy dari dalam
           callback/timer trim memicu SIGSEGV _IO_vsprintf (invariant Destroy/Deferred). */
        Zombie_QueueDestroy(zombieid);
        return 0;
    }

    /* Dunia 60000 tidak pernah dipakai lobby, jadi NPC parkir tidak di-stream
       ke siapa pun: tidak ada biaya sync saat menganggur.
       JANGAN menurunkan darah ke 0: itu memicu jalur kematian samp03svr yang
       menghapus pemain sambil streamer masih memegang id-nya (SIGSEGV
       `_IO_vsprintf`, dump 17:43/17:51/18:49 dengan log `[death] ls_zm_... 255`
       tepat sebelumnya). NPC parkir tetap hidup, hanya dipindah dan dihentikan. */
    FCNPC_Stop(zombieid);
    FCNPC_SetVirtualWorld(zombieid, ZOMBIE_POOL_PARKED_WORLD);
    FCNPC_SetPosition(zombieid, 0.0, 0.0, -200.0);
    ZombieParkedIds[ZombieParkedCount++] = zombieid;
    return 1;
}

/* Jumlah NPC zombie yang memegang slot pemain: aktif + parkir.
   Dipakai npc_budget.inc untuk menghitung slot bebas tanpa API pemain. */
stock Zombie_NpcCountForBudget()
{
    return ZombieActiveCount + ZombieParkedCount;
}

stock Zombie_TakeParked()
{
    if (ZombieParkedCount <= 0) return INVALID_PLAYER_ID;
    return ZombieParkedIds[--ZombieParkedCount];
}

stock Zombie_Create(E_ZOMBIE_TYPE:type, Float:x, Float:y, Float:z, lobbyId = 0)
{
#if ZOMBIE_SPAWN_DISABLED
    #pragma unused type, x, y, z, lobbyId
    return INVALID_ZOMBIE;
#endif
    if (ZombieActiveCount >= MAX_ZOMBIES)
    {
        Log_Format(LOG_WARN, "[ZOMBIE] budget penuh (%d), spawn ditolak", MAX_ZOMBIES);
        return INVALID_ZOMBIE;
    }

    new zombieid = Zombie_TakeParked();
    new reused = (zombieid != INVALID_PLAYER_ID);

    if (!reused)
    {
        /* Slot pemain dan NPC berbagi pool yang sama: membuat NPC tanpa slot bebas
           membuat FCNPC setengah jadi dan RakNet abort (assertion objectExists). */
        if (Npc_CreatePaused() || !Npc_SlotAvailable())
        {
            Npc_BudgetWarn("zombie");
            return INVALID_ZOMBIE;
        }

        new name[32];
        format(name, sizeof(name), "ls_zm_%d_%d", GetTickCount(), random(99999));

        zombieid = FCNPC_Create(name);
        if (zombieid == INVALID_PLAYER_ID || zombieid < 0 || zombieid >= MAX_ZOMBIE_SLOTS)
        {
            Log_Format(LOG_ERROR, "[ZOMBIE] FCNPC_Create gagal untuk tipe %d (max=%d poolsize=%d pakai=%d parkir=%d hilang=%d nama='%s')",
                _:type, GetMaxPlayers(), GetPlayerPoolSize(), Npc_OccupiedSlots(), ZombieParkedCount, Npc_LostSlots(), name);
            Npc_NoteCreateFailure();
            return INVALID_ZOMBIE;
        }
    }

    /* Reuse tetap harus mengganti kulit: tipe zombie menentukan model. */
    FCNPC_SetSkin(zombieid, gZombieProfiles[_:type][ztSkin]);

    if (reused)
    {
        FCNPC_SetVirtualWorld(zombieid, 0);
        FCNPC_SetPosition(zombieid, x, y, z);
        FCNPC_Respawn(zombieid);
    }
    else
    {
        FCNPC_Spawn(zombieid, gZombieProfiles[_:type][ztSkin], x, y, z);
    }
    ZombieStats_NoteSpawn();

    ZombieEntity[zombieid][zeActive] = true;
    ZombieEntity[zombieid][zeDead] = false;
    ZombieEntity[zombieid][zeType] = type;
    ZombieEntity[zombieid][zeHealth] = gZombieProfiles[_:type][ztHealth];
    ZombieEntity[zombieid][zeMaxHealth] = gZombieProfiles[_:type][ztHealth];
    ZombieEntity[zombieid][zeLastAttacker] = INVALID_PLAYER_ID;
    ZombieEntity[zombieid][zeSpawnTick] = GetTickCount();
    ZombieEntity[zombieid][zeDeathTick] = 0;
    ZombieEntity[zombieid][zeGeneration] = ++ZombieGenerationCounter;

    if (!Lobby_IsValid(lobbyId)) lobbyId = 0;
    ZombieEntity[zombieid][zeLobby] = lobbyId;
    FCNPC_SetVirtualWorld(zombieid, Lobby_WorldById(lobbyId));

    FCNPC_SetHealth(zombieid, gZombieProfiles[_:type][ztHealth]);
    FCNPC_SetInvulnerable(zombieid, false);
    FCNPC_SetFightingStyle(zombieid, gZombieProfiles[_:type][ztMeleeStyle]);
    FCNPC_SetSpeed(zombieid, Float:gZombieProfiles[_:type][ztWalkSpeed] * Lobby_GetZombieSpeed(lobbyId));

    ZombieActiveCount++;
    Iter_Add(ZombiePool, zombieid);
    Hash_ZombieAdd(zombieid, x, y);

    ZombieAI_Reset(zombieid);
    return zombieid;
}

/* Destroy langsung dari dalam callback FCNPC/streamer membuat samp03svr
   menghapus pemain saat strukturnya masih dipakai (crash _IO_vsprintf).
   Semua permintaan hapus masuk antrean dan dieksekusi di Zombie_Tick. */
new ZombieDestroyQueue[MAX_ZOMBIE_SLOTS];
new ZombieDestroyCount = 0;

stock Zombie_QueueDestroy(zombieid)
{
    if (zombieid < 0 || zombieid >= MAX_ZOMBIE_SLOTS) return 0;
    if (!ZombieEntity[zombieid][zeActive]) return 0;
    if (ZombieDestroyCount >= MAX_ZOMBIE_SLOTS) return 0;

    for (new queued = 0; queued < ZombieDestroyCount; queued++)
    {
        if (ZombieDestroyQueue[queued] == zombieid) return 1;
    }

    ZombieDestroyQueue[ZombieDestroyCount++] = zombieid;

    /* Keluarkan dari pool sekarang agar AI tidak menyentuh NPC yang menunggu
       dihapus; slotnya baru bebas setelah Zombie_DestroyNow jalan. */
    Hash_ZombieRemove(zombieid);
    Iter_Remove(ZombiePool, zombieid);
    return 1;
}

stock Zombie_FlushDestroyQueue()
{
    for (new queued = 0; queued < ZombieDestroyCount; queued++)
    {
        Zombie_DestroyNow(ZombieDestroyQueue[queued]);
    }
    ZombieDestroyCount = 0;
    return 1;
}

stock Zombie_DestroyNow(zombieid)
{
    if (zombieid < 0 || zombieid >= MAX_ZOMBIE_SLOTS) return 0;
    if (!ZombieEntity[zombieid][zeActive]) return 0;

    Hash_ZombieRemove(zombieid);
    Iter_Remove(ZombiePool, zombieid);
    ZombieAI_Reset(zombieid);

    /* Tandai mati lebih dulu: menurunkan darah ke 0 memicu OnNPCDeath, dan
       callback itu tidak boleh mengira zombie ini masih aktif. */
    ZombieEntity[zombieid][zeActive] = false;
    ZombieEntity[zombieid][zeDead] = false;

    if (FCNPC_IsValid(zombieid))
    {
        Zombie_Park(zombieid);
    }

    ZombieStats_NoteDestroy();
    ZombieActiveCount--;

    if (ZombieActiveCount < 0) ZombieActiveCount = 0;
    return 1;
}

stock bool:Zombie_ApplyDamage(zombieid, attackerid, Float:amount, weaponId)
{
    if (!Zombie_IsActive(zombieid)) return false;

    ZombieEntity[zombieid][zeHealth] -= amount;
    if (attackerid != INVALID_PLAYER_ID)
    {
        ZombieEntity[zombieid][zeLastAttacker] = attackerid;
    }

    if (ZombieEntity[zombieid][zeHealth] > 0.0)
    {
        FCNPC_SetHealth(zombieid, ZombieEntity[zombieid][zeHealth]);
        return true;
    }

    ZombieEntity[zombieid][zeHealth] = 0.0;
    ZombieEntity[zombieid][zeDead] = true;
    ZombieEntity[zombieid][zeDeathTick] = GetTickCount();

    FCNPC_ApplyAnimation(zombieid, "PED", "KO_skid_front", 4.0, 0, 1, 1, 1, 0);

    new Float:x, Float:y, Float:z;
    FCNPC_GetPosition(zombieid, x, y, z);

    new killer = ZombieEntity[zombieid][zeLastAttacker];
    /* Kredit kill HANYA bila killing blow dari pemain (audit F-06): tag 1
       tembakan lalu companion/turret yang menghabisi tidak boleh membayar
       XP/kontrak penuh ke pemain (AFK-farm). Loot tetap jatuh. */
    if (attackerid != INVALID_PLAYER_ID && IsPlayerConnected(attackerid))
    {
        /* Jarak dihitung sebelum mayat dibersihkan: dipakai untuk statistik senjata. */
        new Float:killerX, Float:killerY, Float:killerZ, Float:killDistance;
        GetPlayerPos(attackerid, killerX, killerY, killerZ);
        killDistance = VectorSize(x - killerX, y - killerY, z - killerZ);
        Zombie_AwardKill(attackerid, ZombieEntity[zombieid][zeType], weaponId, killDistance);
        Contracts_OnZombieKill(attackerid, ZombieEntity[zombieid][zeType]);
    }

    /* Specimen tidak jatuh bila pembunuh terakhir ada di hub/safe zone
       (menembak dari tepi kamp = farming tanpa risiko — audit F1) atau
       tidak ada tag pemain sama sekali (kill murni NPC). */
    Loot_DropFromZombie(ZombieEntity[zombieid][zeType], x, y, z, ZombieEntity[zombieid][zeLobby], killer);
    ZombieAI_OnZombieKilled(zombieid, ZombieEntity[zombieid][zeType], x, y, z);

    SetTimerEx("Zombie_OnDeathCleanup", 4000, false, "ii", zombieid, ZombieEntity[zombieid][zeGeneration]);
    return true;
}

public Zombie_OnDeathCleanup(zombieid, generation)
{
    if (zombieid < 0 || zombieid >= MAX_ZOMBIE_SLOTS) return 0;
    if (!ZombieEntity[zombieid][zeActive]) return 0;
    if (ZombieEntity[zombieid][zeGeneration] != generation) return 0;
    return Zombie_QueueDestroy(zombieid);
}

stock Zombie_AwardKill(playerid, E_ZOMBIE_TYPE:type, weaponId, Float:killDistance)
{
    if (!IsPlayerConnected(playerid)) return 0;
    if (Zone_IsHub(Player_GetZone(playerid))) return 0;   // zero farming in the hub

    new xp = 10 + (_:type * 5);

    /* Diminishing returns XP tier-1 (audit D6): >10 kill/5 menit = -20%,
       >20 = -40%. Farming zona awal tanpa taper membanjiri XP. */
    if (Zone_GetTier(Player_GetZone(playerid)) <= 1)
    {
        new taperNow = GetTickCount();
        if (taperNow - TaperStart[playerid] > 300000)
        {
            TaperStart[playerid] = taperNow;
            TaperKills[playerid] = 0;
        }
        TaperKills[playerid]++;
        if (TaperKills[playerid] > 20) xp = xp * 60 / 100;
        else if (TaperKills[playerid] > 10) xp = xp * 80 / 100;
    }

    /* Boss malam: hadiah terpisah dan sekali saja, tidak ikut skala tipe biasa. */
    if (type == ZT_BOSS)
    {
        Inventory_Grant(playerid, ITEM_SCRAP, 3, "boss_scrap");
        PlayerSurvival[playerid][psCash] += ZOMBIE_BOSS_REWARD_CASH;
        Skills_AddXp(playerid, ZOMBIE_BOSS_REWARD_XP);
        RankPoints[playerid] += 20;
        Toast_Show(playerid, "ABOMINATION MATI", "+$250, +60 XP, +3 scrap.", UI_ACCENT_PRIMARY);
        Director_OnBossKilled();
    }

    /* Scrap = ITEM_SCRAP sungguhan (bukan psScrap, mata uang mati yang tak
       pernah bisa dibelanjakan — audit E1). */
    Inventory_Grant(playerid, ITEM_SCRAP, 1 + random(2) + Skills_LootBonus(playerid), "kill_scrap");
    Skills_AddXp(playerid, xp);

    /* Kill streak 60 detik: 5 kill berturut = bonus XP + toast. */
    new now = GetTickCount();
    if (now - PlayerSurvival[playerid][psStreakTick] < 60000)
    {
        PlayerSurvival[playerid][psStreakCount]++;
    }
    else
    {
        PlayerSurvival[playerid][psStreakCount] = 1;
    }
    PlayerSurvival[playerid][psStreakTick] = now;

    if (PlayerSurvival[playerid][psStreakCount] == 5)
    {
        Skills_AddXp(playerid, 15);
        Toast_Show(playerid, "RAMPAGE", "5 zombie dalam 1 menit. +15 XP.", UI_ACCENT_PRIMARY);
    }
    else if (PlayerSurvival[playerid][psStreakCount] == 10)
    {
        Skills_AddXp(playerid, 30);
        Inventory_Grant(playerid, ITEM_SCRAP, 2, "streak_scrap");
        Toast_Show(playerid, "PEMBANTAIAN", "10 zombie dalam 1 menit. +30 XP, +2 scrap.", UI_ACCENT_PRIMARY);
    }

    Telemetry_LogZombieKill(playerid, type, weaponId, killDistance);
    Tutorial_OnZombieKill(playerid);
    return 1;
}

/* FCNPC fires this before applying its own damage: the server decides, not the engine. */
stock Zombie_HookTakeDamage(zombieid, issuerid, Float:amount, weaponid, bodypart)
{
    if (!Zombie_IsActive(zombieid)) return 0;

    // FCNPC melee has a cone: zombies packed together hit each other. The engine
    // never decides damage between our own NPCs.
    if (issuerid != INVALID_PLAYER_ID && IsPlayerNPC(issuerid)) return 0;

    new Float:damage = amount;
    Damage_Apply(DAMAGE_KIND_ZOMBIE, zombieid, issuerid, damage, weaponid, bodypart);
    return 0;
}

stock Zombie_HookDeath(zombieid)
{
    if (zombieid < 0 || zombieid >= MAX_ZOMBIE_SLOTS) return 0;

    if (ZombieEntity[zombieid][zeActive] && !ZombieEntity[zombieid][zeDead])
    {
        ZombieEntity[zombieid][zeDead] = true;
        SetTimerEx("Zombie_OnDeathCleanup", 4000, false, "ii", zombieid, ZombieEntity[zombieid][zeGeneration]);
    }
    return 1;
}

