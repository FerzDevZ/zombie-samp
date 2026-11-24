/* NPC rekrutan (survivor companion) — 1 per karakter, disimpan di tabel `companions`.

   Aturan desain:
   - Companion adalah FCNPC netral, BUKAN zombie: `FCNPC_OnDeath` harus dicek dulu lewat
     `Companion_OnNpcDeath` sebelum `Zombie_HookDeath` (lihat main.pwn).
   - Damage companion ke zombie dikirim tanpa attacker (`INVALID_PLAYER_ID`) supaya tidak
     jadi jalur AFK-farming XP/kontrak. Loot tetap jatuh normal karena drop tidak butuh killer.
   - Companion tidak pernah di-set health 0 saat tumbang; status tumbang memakai flag
     `CompDowned` agar callback kematian FCNPC tidak salah dianggap zombie mati. */

#if defined _ls_companion_included
    #endinput
#endif
#define _ls_companion_included

#define COMPANION_MAX           (MAX_PLAYERS)
#define COMPANION_MAX_HP        100.0
#define COMPANION_REVIVE_HP     50.0
#define COMPANION_SKIN          285
#define COMPANION_WEAPON        24

#define COMPANION_FOLLOW_DIST   6.0
#define COMPANION_ENGAGE_RANGE  26.0
#define COMPANION_FIRE_MS       900
#define COMPANION_DAMAGE        13.0
#define COMPANION_NOISE_RADIUS  7.0
#define COMPANION_MELEE_RANGE   2.6
#define COMPANION_MELEE_HIT     6.0
#define COMPANION_DOWNED_TIMEOUT_MS 180000
#define COMPANION_REPATH_MS     1500
#define COMPANION_ENTER_MS      2500   /* jeda antar percobaan naik kendaraan */
#define COMPANION_MAX_SEATS     10

#define COMPANION_RECRUIT_CASH  600
#define COMPANION_RECRUIT_FOOD  2
#define COMPANION_RECRUIT_WATER 1

#define COMPANION_MODE_FOLLOW   0
#define COMPANION_MODE_HOLD     1

#define COMPANION_RECRUITER_X   1669.4
#define COMPANION_RECRUITER_Y   -2316.4
#define COMPANION_RECRUITER_Z   12.38
#define COMPANION_RECRUITER_RANGE 5.0

#define DIALOG_COMPANION        3301

/* Tier perlengkapan: dibeli lewat /teman upgrade, memakai bahan dari crafting. */
#define COMPANION_TIER_MAX      2
#define COMPANION_TIER1_HP      160.0
#define COMPANION_TIER2_HP      220.0
#define COMPANION_TIER1_DAMAGE  19.0
#define COMPANION_TIER2_DAMAGE  26.0
#define COMPANION_TIER1_CASH    400
#define COMPANION_TIER2_CASH    900
#define COMPANION_TIER1_CLOTH   6
#define COMPANION_TIER1_ELEC    2
#define COMPANION_TIER2_REPAIR  1
#define COMPANION_TIER2_ELEC    4
#define COMPANION_TIER2_AMMO    10

/* Perlengkapan per tier (ROADMAP 365): senjata naik, armour ikut, dan rekrutan
   membawakan loot yang dia lewati. Armour harus diterapkan ulang setiap spawn
   karena FCNPC kehilangan armor saat respawn. */
#define COMPANION_WEAPON_T1     31
#define COMPANION_WEAPON_T2     34
#define COMPANION_ARMOUR_T1     25.0
#define COMPANION_ARMOUR_T2     50.0
#define COMPANION_PICKUP_MS     3000
#define COMPANION_PICKUP_RANGE  4.0

new CompNextPickupTick[MAX_PLAYERS];

forward Companion_Spawn(playerid);
forward Companion_Despawn(playerid);
forward Companion_Delete(playerid);
forward Companion_GoDown(playerid, const reason[]);
forward Companion_Recruit(playerid);
forward Companion_Revive(playerid);
forward Companion_Dismiss(playerid);

new CompNpc[MAX_PLAYERS] = {INVALID_PLAYER_ID, ...};
new Float:CompHealth[MAX_PLAYERS];
new CompMode[MAX_PLAYERS];
new CompTier[MAX_PLAYERS];
new bool:CompHas[MAX_PLAYERS];
new bool:CompDowned[MAX_PLAYERS];
new CompDownedTick[MAX_PLAYERS];
new CompNextFireTick[MAX_PLAYERS];
new CompNextRepathTick[MAX_PLAYERS];
new CompNextEnterTick[MAX_PLAYERS];
new CompVehicle[MAX_PLAYERS];
new CompPendingSeat[MAX_PLAYERS];
new CompEnterSince[MAX_PLAYERS];   /* kendaraan yang sedang dinaiki NPC */
new CompName[MAX_PLAYERS][24];
/* NPC yang mati dari engine TIDAK dihancurkan di dalam FCNPC_OnDeath (re-entrancy);
   id-nya dititipkan di sini lalu dihancurkan tick berikutnya. */
new CompDeadNpc[MAX_PLAYERS] = {INVALID_PLAYER_ID, ...};

/* Kursi penumpang bebas: bitmask kursi terpakai, 0 = kursi pengemudi.
   Dipisah sebagai fungsi murni supaya logika kursi bisa diuji tanpa kendaraan. */
stock Companion_PickSeat(seats, occupiedMask)
{
    if (seats > COMPANION_MAX_SEATS) seats = COMPANION_MAX_SEATS;
    for (new seat = 1; seat < seats; seat++)
    {
        if (occupiedMask & (1 << seat)) continue;
        return seat;
    }
    return -1;
}

/* Bitmask kursi yang sudah terisi pemain lain atau rekrutan lain. */
stock Companion_OccupiedMask(vehicleid)
{
    new mask = 0;
    foreach (new other : PlayerPool)
    {
        if (!IsPlayerConnected(other)) continue;
        if (!IsPlayerInVehicle(other, vehicleid)) continue;
        new seat = GetPlayerVehicleSeat(other);
        if (seat > 0 && seat < COMPANION_MAX_SEATS) mask |= (1 << seat);
    }
    foreach (new other : PlayerPool)
    {
        if (!CompHas[other] || !FCNPC_IsValid(CompNpc[other])) continue;
        if (FCNPC_GetVehicleID(CompNpc[other]) != vehicleid) continue;
        new seat = FCNPC_GetVehicleSeat(CompNpc[other]);
        if (seat > 0 && seat < COMPANION_MAX_SEATS) mask |= (1 << seat);
    }
    return mask;
}

stock Float:Companion_MaxHealth(playerid)
{
    switch (CompTier[playerid])
    {
        case 2: return COMPANION_TIER2_HP;
        case 1: return COMPANION_TIER1_HP;
    }
    return COMPANION_MAX_HP;
}

/* Murni per tier supaya bisa diuji selftest tanpa menyentuh state pemain. */
stock Companion_WeaponForTier(tier)
{
    switch (tier)
    {
        case 2: return COMPANION_WEAPON_T2;
        case 1: return COMPANION_WEAPON_T1;
    }
    return COMPANION_WEAPON;
}

stock Float:Companion_ArmourForTier(tier)
{
    switch (tier)
    {
        case 2: return COMPANION_ARMOUR_T2;
        case 1: return COMPANION_ARMOUR_T1;
    }
    return 0.0;
}

stock Companion_Weapon(playerid)
{
    return Companion_WeaponForTier(CompTier[playerid]);
}

stock Float:Companion_Armour(playerid)
{
    return Companion_ArmourForTier(CompTier[playerid]);
}

/* Satu tempat penerapan perlengkapan: spawn, upgrade, dan pemulihan. */
stock Companion_ApplyGear(playerid)
{
    new npcid = CompNpc[playerid];
    if (!FCNPC_IsValid(npcid)) return 0;

    FCNPC_SetWeapon(npcid, Companion_Weapon(playerid));
    FCNPC_SetAmmo(npcid, 9999);
    FCNPC_SetWeaponAccuracy(npcid, Companion_Weapon(playerid), 0.85 + 0.05 * float(CompTier[playerid]));
    FCNPC_SetArmour(npcid, Companion_Armour(playerid));
    return 1;
}

stock Float:Companion_ShotDamage(playerid)
{
    switch (CompTier[playerid])
    {
        case 2: return COMPANION_TIER2_DAMAGE;
        case 1: return COMPANION_TIER1_DAMAGE;
    }
    return COMPANION_DAMAGE;
}


stock Companion_Init()
{
    for (new playerid = 0; playerid < MAX_PLAYERS; playerid++)
    {
        CompNpc[playerid] = INVALID_PLAYER_ID;
        CompHealth[playerid] = COMPANION_MAX_HP;
        CompMode[playerid] = COMPANION_MODE_FOLLOW;
        CompTier[playerid] = 0;
        CompHas[playerid] = false;
        CompDowned[playerid] = false;
        CompDownedTick[playerid] = 0;
        CompNextFireTick[playerid] = 0;
        CompNextRepathTick[playerid] = 0;
        CompNextEnterTick[playerid] = 0;
        CompVehicle[playerid] = 0;
        CompPendingSeat[playerid] = -1;
        CompEnterSince[playerid] = 0;
        CompName[playerid][0] = EOS;
        CompDeadNpc[playerid] = INVALID_PLAYER_ID;
    }
    return 1;
}

stock Companion_OwnerOf(npcid)
{
    for (new playerid = 0; playerid < MAX_PLAYERS; playerid++)
    {
        if (CompNpc[playerid] == npcid) return playerid;
    }
    return INVALID_PLAYER_ID;
}

stock bool:Companion_IsUp(playerid)
{
    if (!CompHas[playerid]) return false;
    if (CompDowned[playerid]) return false;
    if (!IsPlayerConnected(playerid)) return false;
    return true;
}

stock Companion_Spawn(playerid)
{
    if (!CompHas[playerid]) return 0;
    if (!IsPlayerConnected(playerid)) return 0;
    if (CompDowned[playerid]) return 0;
    if (FCNPC_IsValid(CompNpc[playerid])) return 1;

    new Float:x, Float:y, Float:z, Float:angle;
    GetPlayerPos(playerid, x, y, z);
    GetPlayerFacingAngle(playerid, angle);

    if (!Npc_SlotAvailable())
    {
        Npc_BudgetWarn("companion");
        return 0;
    }

    new name[32];
    format(name, sizeof(name), "ls_comp_%d_%d", playerid, GetTickCount());

    new npcid = FCNPC_Create(name);
    if (npcid < 0 || npcid == INVALID_PLAYER_ID)
    {
        Npc_BudgetWarn("companion");
        Log_Format(LOG_WARN, "[COMPANION] FCNPC_Create gagal playerid=%d", playerid);
        return 0;
    }

    Npc_HideNameTag(npcid);
    FCNPC_Spawn(npcid, COMPANION_SKIN,
        x - floatsin(-angle, degrees) * 1.6,
        y - floatcos(-angle, degrees) * 1.6,
        z);
    FCNPC_SetVirtualWorld(npcid, Lobby_WorldById(Lobby_Of(playerid)));
    FCNPC_SetHealth(npcid, CompHealth[playerid]);
    FCNPC_SetInvulnerable(npcid, false);
    FCNPC_SetSpeed(npcid, 0.75);

    CompNpc[playerid] = npcid;
    Companion_ApplyGear(playerid);
    CompNextPickupTick[playerid] = 0;
    CompNextFireTick[playerid] = 0;
    CompNextRepathTick[playerid] = 0;
    CompNextEnterTick[playerid] = 0;
    CompVehicle[playerid] = 0;
    return 1;
}

stock Companion_Despawn(playerid)
{
    if (!FCNPC_IsValid(CompNpc[playerid]))
    {
        CompNpc[playerid] = INVALID_PLAYER_ID;
        return 0;
    }

    FCNPC_Destroy(CompNpc[playerid]);
    CompNpc[playerid] = INVALID_PLAYER_ID;
    return 1;
}

stock Companion_Save(playerid)
{
    if (ActiveCharId[playerid] <= 0) return 0;

    new query[256];
    format(query, sizeof(query),
        "INSERT INTO companions (char_id, name, health, mode, tier) VALUES (%d, '%s', %.2f, %d, %d) \
         ON DUPLICATE KEY UPDATE name = VALUES(name), health = VALUES(health), mode = VALUES(mode), tier = VALUES(tier)",
        ActiveCharId[playerid], (CompName[playerid][0] == EOS) ? "Rekrutan" : CompName[playerid],
        CompHealth[playerid], CompMode[playerid], CompTier[playerid]);
    mysql_tquery(gSQL, query, "", "");
    return 1;
}

stock Companion_Delete(playerid)
{
    if (ActiveCharId[playerid] <= 0) return 0;

    new query[128];
    format(query, sizeof(query), "DELETE FROM companions WHERE char_id = %d", ActiveCharId[playerid]);
    mysql_tquery(gSQL, query, "", "");
    return 1;
}

forward Companion_Tick();
public Companion_Tick()
{
    /* foreach PlayerPool, bukan for 1000 slot (audit performa F15). */
    foreach (new playerid : PlayerPool)
    {
        if (CompNpc[playerid] == INVALID_PLAYER_ID && !CompHas[playerid]) continue;
        Companion_TickPlayer(playerid);
    }
    return 1;
}

stock Companion_TickPlayer(playerid)
{
    if (!CompHas[playerid]) return 0;

    if (!IsPlayerConnected(playerid) || !Account_IsPlaying(playerid))
    {
        Companion_Despawn(playerid);
        return 0;
    }

    /* Tumbang: hitung mundur, dijemput pemilik atau hilang permanen. */
    if (CompDowned[playerid])
    {
        if (FCNPC_IsValid(CompDeadNpc[playerid]))
        {
            FCNPC_Destroy(CompDeadNpc[playerid]);
        }
        CompDeadNpc[playerid] = INVALID_PLAYER_ID;

        Companion_Despawn(playerid);
        if (GetTickCount() - CompDownedTick[playerid] >= COMPANION_DOWNED_TIMEOUT_MS)
        {
            CompHas[playerid] = false;
            Companion_Delete(playerid);
            if (IsPlayerConnected(playerid))
            {
                Toast_Show(playerid, "Rekrutan hilang", "Tidak dijemput tepat waktu.", UI_ACCENT_DANGER);
                SendClientMessage(playerid, UI_ACCENT_DANGER,
                    "[REKRUTAN] Rekrutan tidak tertolong dan hilang. Rekrut lagi di kamp.");
            }
        }
        return 1;
    }

    if (Zombie_GetActiveCount() >= MAX_ZOMBIES - 1)
    {
        Companion_Despawn(playerid);
        return 1;
    }

    if (!FCNPC_IsValid(CompNpc[playerid])) Companion_Spawn(playerid);
    if (!FCNPC_IsValid(CompNpc[playerid])) return 0;

    new npcid = CompNpc[playerid];
    new now = GetTickCount();
    new lobby = Lobby_Of(playerid);

    new Float:nx, Float:ny, Float:nz, Float:px, Float:py, Float:pz;
    FCNPC_GetPosition(npcid, nx, ny, nz);

    /* ---- Kendaraan: rekrutan ikut naik saat pemilik menyetir ----
       Sebelum ini NPC hanya berjalan kaki sehingga tertinggal di belakang
       begitu pemain menyetir. Kursi dipilih dari kursi penumpang yang bebas. */
    new npcVehicle = FCNPC_GetVehicleID(npcid);
    /* Pemain sebagai penumpang pun tetap dianggap "di kendaraan": rekrutan
       tidak boleh keluar hanya karena pemiliknya bukan pengemudi. */
    new bool:ownerDriving = (IsPlayerInAnyVehicle(playerid) != 0);
    if (ownerDriving)
    {
        new ownerVehicle = GetPlayerVehicleID(playerid);
        if (npcVehicle != ownerVehicle)
        {
            /* FCNPC_EnterVehicle DIAM-DIAM gagal bila NPC lebih dari
               MAX_DISTANCE_TO_ENTER_VEHICLE (30 m di sumber plugin). Itu
               penyebab "log bilang naik, NPC tetap di jalan". Jadi jarak
               diukur dulu; terlalu jauh = langsung tempatkan di kursi. */
            new Float:vx, Float:vy, Float:vz;
            GetVehiclePos(ownerVehicle, vx, vy, vz);
            new Float:seatGap = floatsqroot((nx - vx) * (nx - vx) + (ny - vy) * (ny - vy) + (nz - vz) * (nz - vz));

            if (now >= CompNextEnterTick[playerid])
            {
                CompNextEnterTick[playerid] = now + COMPANION_ENTER_MS;
                new seat = Companion_PickSeat(VehicleSeatsTotal(GetVehicleModel(ownerVehicle)),
                    Companion_OccupiedMask(ownerVehicle));
                if (seat >= 0)
                {
                    if (seatGap <= 28.0)
                    {
                        FCNPC_EnterVehicle(npcid, ownerVehicle, seat, FCNPC_MOVE_TYPE_RUN);
                        CompPendingSeat[playerid] = seat;
                        CompEnterSince[playerid] = now;
                    }
                    else
                    {
                        /* Sudah ditinggal mobil: animasi masuk mustahil, jadi
                           dipindahkan langsung supaya benar-benar di dalam. */
                        FCNPC_PutInVehicle(npcid, ownerVehicle, seat);
                        CompPendingSeat[playerid] = -1;
                        Log_Format(LOG_INFO,
                            "companion: npc %d ditempatkan di kendaraan %d kursi %d (jarak %.0f m)",
                            npcid, ownerVehicle, seat, seatGap);
                    }
                    CompVehicle[playerid] = ownerVehicle;
                }
            }

            /* Verifikasi: animasi masuk butuh waktu. Bila 4 detik belum masuk,
               paksa pindah ke kursi (jangan biarkan NPC tertinggal). */
            if (CompPendingSeat[playerid] >= 0 && now - CompEnterSince[playerid] > 4000)
            {
                FCNPC_PutInVehicle(npcid, ownerVehicle, CompPendingSeat[playerid]);
                Log_Format(LOG_WARN,
                    "companion: npc %d gagal animasi masuk, dipindah paksa ke kendaraan %d",
                    npcid, ownerVehicle);
                CompPendingSeat[playerid] = -1;
            }
        }
        else
        {
            FCNPC_Stop(npcid);          /* jangan jalan kaki di dalam mobil */
            if (CompPendingSeat[playerid] >= 0)
            {
                CompPendingSeat[playerid] = -1;
                Toast_Show(playerid, "Rekrutan", "Ikut naik kendaraan.", UI_ACCENT_PRIMARY);
            }
        }
    }
    else if (npcVehicle != INVALID_VEHICLE_ID && CompVehicle[playerid] != 0)
    {
        /* Pemilik turun: rekrutan keluar lalu kembali mengikuti. */
        FCNPC_ExitVehicle(npcid);
        CompVehicle[playerid] = 0;
    }

    /* Rekrutan membawakan loot yang dilewatinya (ROADMAP 365). Dibatasi satu
       ambilan per 3 detik supaya tidak menyapu seluruh medan dalam satu tick. */
    if (now - CompNextPickupTick[playerid] >= 0)
    {
        CompNextPickupTick[playerid] = now + COMPANION_PICKUP_MS;
        if (Loot_TakeNearPoint(playerid, nx, ny, nz, COMPANION_PICKUP_RANGE))
        {
            Toast_Show(playerid, "Rekrutan", "Membawakan barang untukmu.", UI_ACCENT_PRIMARY);
        }
    }

    /* Zombie yang menempel mencakar rekrutan (simulasi melee, zombie tidak menarget NPC). */
    new Float:leech = 0.0;
    foreach (new zombieid : ZombiePool)
    {
        if (!Zombie_IsActive(zombieid)) continue;
        if (Zombie_GetLobby(zombieid) != lobby) continue;

        new Float:zx, Float:zy, Float:zz;
        FCNPC_GetPosition(zombieid, zx, zy, zz);
        if (VectorSize(zx - nx, zy - ny, zz - nz) <= COMPANION_MELEE_RANGE) leech += COMPANION_MELEE_HIT;
    }

    if (leech > 0.0)
    {
        CompHealth[playerid] -= leech;
        if (CompHealth[playerid] <= 0.0) return Companion_GoDown(playerid, "dikeroyok zombie");
    }

    /* Target tembak: zombie terdekat di lobby yang sama. */
    new target = INVALID_PLAYER_ID;
    new Float:bestDist = COMPANION_ENGAGE_RANGE;

    foreach (new zombieid : ZombiePool)
    {
        if (!Zombie_IsActive(zombieid)) continue;
        if (Zombie_GetLobby(zombieid) != lobby) continue;

        new Float:zx, Float:zy, Float:zz;
        FCNPC_GetPosition(zombieid, zx, zy, zz);
        new Float:dist = VectorSize(zx - nx, zy - ny, zz - nz);
        if (dist <= bestDist)
        {
            bestDist = dist;
            target = zombieid;
        }
    }

    if (target != INVALID_PLAYER_ID)
    {
        new Float:targetX, Float:targetY, Float:targetZ;
        FCNPC_GetPosition(target, targetX, targetY, targetZ);
        FCNPC_SetAngleToPos(npcid, targetX, targetY);
        FCNPC_Stop(npcid);

        if (now >= CompNextFireTick[playerid])
        {
            CompNextFireTick[playerid] = now + COMPANION_FIRE_MS;

            /* Tanpa attacker: kill tidak memberi XP/kontrak (anti AFK farming), loot tetap jatuh. */
            Zombie_ApplyDamage(target, INVALID_PLAYER_ID, Companion_ShotDamage(playerid), COMPANION_WEAPON);
            Noise_EmitAtPosition(nx, ny, nz, COMPANION_NOISE_RADIUS, lobby);
        }
        return 1;
    }

    GetPlayerPos(playerid, px, py, pz);
    new Float:distToPlayer = VectorSize(px - nx, py - ny, pz - nz);

    if (CompMode[playerid] == COMPANION_MODE_HOLD)
    {
        if (distToPlayer > COMPANION_ENGAGE_RANGE * 2.0)
        {
            CompMode[playerid] = COMPANION_MODE_FOLLOW;
        }
        else
        {
            FCNPC_Stop(npcid);
            return 1;
        }
    }

    /* Saat ikut di kendaraan, jalur kaki dilewati sepenuhnya. */
    if (FCNPC_GetVehicleID(npcid) != INVALID_VEHICLE_ID) return 1;

    if (distToPlayer > COMPANION_FOLLOW_DIST && now >= CompNextRepathTick[playerid])
    {
        CompNextRepathTick[playerid] = now + COMPANION_REPATH_MS;
        FCNPC_GoToPlayer(npcid, playerid, FCNPC_MOVE_TYPE_RUN, 0.75,
            FCNPC_MOVE_MODE_COLANDREAS, FCNPC_MOVE_PATHFINDING_RAYCAST, 2.0, true, 1.0, 2.5, 250);
    }
    else if (distToPlayer <= COMPANION_FOLLOW_DIST)
    {
        FCNPC_Stop(npcid);
    }
    return 1;
}

stock Companion_GoDown(playerid, const reason[])
{
    CompHealth[playerid] = 0.0;
    CompDowned[playerid] = true;
    CompDownedTick[playerid] = GetTickCount();
    Companion_Despawn(playerid);

    new message[128];
    format(message, sizeof(message), "[REKRUTAN] %s tumbang (%s). /teman revive dalam 3 menit.",
        (CompName[playerid][0] == EOS) ? "Rekrutan" : CompName[playerid], reason);
    SendClientMessage(playerid, UI_ACCENT_DANGER, message);
    Toast_Show(playerid, "Rekrutan tumbang", "Pakai /teman revive (butuh 1 perban).", UI_ACCENT_DANGER);
    return 1;
}

stock Companion_OnNpcDeath(npcid)
{
    new owner = Companion_OwnerOf(npcid);
    if (owner == INVALID_PLAYER_ID) return 0;

    /* Jangan destroy di sini: pindahkan ke tick. */
    CompDeadNpc[owner] = npcid;
    CompNpc[owner] = INVALID_PLAYER_ID;
    Companion_GoDown(owner, "tewas");
    return 1;
}

stock Companion_Load(playerid)
{
    if (ActiveCharId[playerid] <= 0) return 0;

    new query[160];
    format(query, sizeof(query),
        "SELECT name, health, mode, tier FROM companions WHERE char_id = %d LIMIT 1", ActiveCharId[playerid]);
    mysql_tquery(gSQL, query, "Companion_OnRowLoaded", "d", playerid);
    return 1;
}

forward Companion_OnRowLoaded(playerid);
public Companion_OnRowLoaded(playerid)
{
    if (!IsPlayerConnected(playerid)) return 0;

    CompNpc[playerid] = INVALID_PLAYER_ID;
    CompDowned[playerid] = false;

    if (cache_num_rows() == 0)
    {
        CompHas[playerid] = false;
        return 0;
    }

    cache_get_value_name(0, "name", CompName[playerid], 24);
    cache_get_value_name_float(0, "health", CompHealth[playerid]);
    new mode;
    cache_get_value_name_int(0, "mode", mode);
    CompMode[playerid] = (mode == COMPANION_MODE_HOLD) ? COMPANION_MODE_HOLD : COMPANION_MODE_FOLLOW;

    new tier;
    cache_get_value_name_int(0, "tier", tier);
    if (tier < 0) tier = 0;
    if (tier > COMPANION_TIER_MAX) tier = COMPANION_TIER_MAX;
    CompTier[playerid] = tier;

    CompHas[playerid] = true;
    if (CompHealth[playerid] <= 0.0 || CompHealth[playerid] > Companion_MaxHealth(playerid))
    {
        CompHealth[playerid] = Companion_MaxHealth(playerid);
    }

    Companion_Spawn(playerid);
    SendClientMessage(playerid, UI_ACCENT_PRIMARY,
        "[REKRUTAN] Rekrutan setia menunggumu. /teman untuk perintah.");
    return 1;
}

stock Companion_OnDisconnect(playerid)
{
    if (CompHas[playerid] && ActiveCharId[playerid] > 0) Companion_Save(playerid);

    Companion_Despawn(playerid);
    CompHas[playerid] = false;
    CompDowned[playerid] = false;
    CompHealth[playerid] = COMPANION_MAX_HP;
    CompTier[playerid] = 0;
    CompName[playerid][0] = EOS;
    return 1;
}

/* Dipanggil dispatcher saat pemain menekan Y. Mengembalikan 1 hanya kalau
   pemain memang berdiri di papan rekrutmen, supaya Y di tempat lain tetap
   menjalankan aksi lain (loot, mayat, hotspot). */
stock Companion_TryRecruitInteract(playerid)
{
    if (!IsPlayerConnected(playerid)) return 0;
    if (!Account_IsPlaying(playerid)) return 0;
    if (PlayerSurvival[playerid][psInShelter]) return 0;

    new Float:x, Float:y, Float:z;
    GetPlayerPos(playerid, x, y, z);

    if (VectorSize(x - COMPANION_RECRUITER_X, y - COMPANION_RECRUITER_Y, z - COMPANION_RECRUITER_Z)
        > COMPANION_RECRUITER_RANGE)
    {
        return 0;
    }

    Companion_Recruit(playerid);
    return 1;
}

stock Companion_Recruit(playerid)
{
    if (!Account_IsPlaying(playerid)) return 0;

    if (CompHas[playerid])
    {
        SendClientMessage(playerid, UI_ACCENT_DANGER, "[REKRUTAN] Kamu sudah punya rekrutan. /teman untuk mengatur.");
        return 0;
    }

    new Float:x, Float:y, Float:z;
    GetPlayerPos(playerid, x, y, z);

    if (!Player_IsSafe(playerid)
        || VectorSize(x - COMPANION_RECRUITER_X, y - COMPANION_RECRUITER_Y, z - COMPANION_RECRUITER_Z)
            > COMPANION_RECRUITER_RANGE)
    {
        SendClientMessage(playerid, UI_TEXT_MUTED, "[REKRUTAN] Datangi perekrut di kamp dulu (dekat papan rekrutmen).");
        return 0;
    }

    if (PlayerSurvival[playerid][psCash] < COMPANION_RECRUIT_CASH)
    {
        new message[128];
        format(message, sizeof(message), "[REKRUTAN] Butuh $%d (kamu punya $%d).",
            COMPANION_RECRUIT_CASH, PlayerSurvival[playerid][psCash]);
        SendClientMessage(playerid, UI_ACCENT_DANGER, message);
        return 0;
    }

    if (Inventory_CountItem(playerid, ITEM_FOOD_CAN) < COMPANION_RECRUIT_FOOD
        || Inventory_CountItem(playerid, ITEM_WATER_BOTTLE) < COMPANION_RECRUIT_WATER)
    {
        new message[144];
        format(message, sizeof(message), "[REKRUTAN] Butuh %d makanan kaleng + %d air mineral sebagai bekal.",
            COMPANION_RECRUIT_FOOD, COMPANION_RECRUIT_WATER);
        SendClientMessage(playerid, UI_ACCENT_DANGER, message);
        return 0;
    }

    if (Zombie_GetActiveCount() >= MAX_ZOMBIES - 1)
    {
        SendClientMessage(playerid, UI_TEXT_MUTED, "[REKRUTAN] Terlalu banyak zombie aktif. Coba lagi.");
        return 0;
    }

    if (!Inventory_Consume(playerid, ITEM_FOOD_CAN, COMPANION_RECRUIT_FOOD, "companion_recruit_food")
        || !Inventory_Consume(playerid, ITEM_WATER_BOTTLE, COMPANION_RECRUIT_WATER, "companion_recruit_water"))
    {
        Inventory_Grant(playerid, ITEM_FOOD_CAN, COMPANION_RECRUIT_FOOD, "companion_recruit_food_refund");
        Inventory_Grant(playerid, ITEM_WATER_BOTTLE, COMPANION_RECRUIT_WATER, "companion_recruit_water_refund");
        SendClientMessage(playerid, UI_ACCENT_DANGER, "[REKRUTAN] Bekal gagal diambil, uang tidak dipotong.");
        return 0;
    }

    PlayerSurvival[playerid][psCash] -= COMPANION_RECRUIT_CASH;

    CompHas[playerid] = true;
    CompDowned[playerid] = false;
    CompHealth[playerid] = COMPANION_MAX_HP;
    CompMode[playerid] = COMPANION_MODE_FOLLOW;
    CompTier[playerid] = 0;
    format(CompName[playerid], 24, "Rekrutan");

    Companion_Save(playerid);
    Companion_Spawn(playerid);

    new message[128];
    format(message, sizeof(message),
        "[REKRUTAN] %s ikut bertarung. -$%d. /teman untuk perintah, /teman revive bila tumbang.",
        CompName[playerid], COMPANION_RECRUIT_CASH);
    SendClientMessage(playerid, UI_ACCENT_PRIMARY, message);
    Toast_Show(playerid, "Rekrutan bergabung", "/teman untuk perintah.", UI_ACCENT_PRIMARY);
    Skills_AddXp(playerid, 10);
    Log_Format(LOG_INFO, "[COMPANION] char=%d rekrut rekrutan baru", ActiveCharId[playerid]);
    return 1;
}

stock Companion_Dismiss(playerid)
{
    if (!CompHas[playerid])
    {
        SendClientMessage(playerid, UI_TEXT_MUTED, "[REKRUTAN] Kamu tidak punya rekrutan.");
        return 0;
    }

    Companion_Despawn(playerid);
    CompHas[playerid] = false;
    CompDowned[playerid] = false;
    CompHealth[playerid] = COMPANION_MAX_HP;
    CompName[playerid][0] = EOS;
    Companion_Delete(playerid);

    SendClientMessage(playerid, UI_ACCENT_DANGER, "[REKRUTAN] Rekrutan berpisah dan pergi. Slot rekrutmen terbuka.");
    return 1;
}

stock Companion_Revive(playerid)
{
    if (!CompHas[playerid] || !CompDowned[playerid])
    {
        SendClientMessage(playerid, UI_TEXT_MUTED, "[REKRUTAN] Tidak ada rekrutan yang tumbang.");
        return 0;
    }

    if (Inventory_CountItem(playerid, ITEM_BANDAGE) < 1)
    {
        SendClientMessage(playerid, UI_ACCENT_DANGER, "[REKRUTAN] Butuh 1 perban untuk menolong rekrutan.");
        return 0;
    }

    if (!Inventory_Consume(playerid, ITEM_BANDAGE, 1, "companion_revive"))
    {
        SendClientMessage(playerid, UI_ACCENT_DANGER, "[REKRUTAN] Perban gagal dipakai.");
        return 0;
    }

    CompDowned[playerid] = false;
    CompDownedTick[playerid] = 0;
    CompHealth[playerid] = COMPANION_REVIVE_HP;

    new Float:x, Float:y, Float:z;
    GetPlayerPos(playerid, x, y, z);
    new lobby = Lobby_Of(playerid);

    if (!Npc_SlotAvailable())
    {
        Npc_BudgetWarn("companion_revive");
        SendClientMessage(playerid, UI_ACCENT_DANGER, "[REKRUTAN] Server penuh NPC. Coba lagi sebentar, perban belum dipakai.");
        return 1;
    }

    new name[32];
    format(name, sizeof(name), "ls_comp_%d_%d", playerid, GetTickCount());
    new npcid = FCNPC_Create(name);

    if (npcid < 0 || npcid == INVALID_PLAYER_ID)
    {
        /* Perban sudah dipakai: kembalikan supaya pemain tidak dirugikan. */
        Inventory_Grant(playerid, ITEM_BANDAGE, 1, "companion_revive_refund");
        CompDowned[playerid] = true;
        CompDownedTick[playerid] = GetTickCount();
        SendClientMessage(playerid, UI_ACCENT_DANGER, "[REKRUTAN] Gagal menghidupkan ulang. Perban dikembalikan.");
        return 0;
    }

    Npc_HideNameTag(npcid);
    FCNPC_Spawn(npcid, COMPANION_SKIN, x + 1.2, y, z);
    FCNPC_SetVirtualWorld(npcid, Lobby_WorldById(lobby));
    FCNPC_SetHealth(npcid, COMPANION_REVIVE_HP);
    FCNPC_SetInvulnerable(npcid, false);
    FCNPC_SetSpeed(npcid, 0.75);
    FCNPC_SetWeapon(npcid, COMPANION_WEAPON);
    FCNPC_SetAmmo(npcid, 9999);

    CompNpc[playerid] = npcid;
    Companion_Save(playerid);

    SendClientMessage(playerid, UI_ACCENT_PRIMARY, "[REKRUTAN] Rekrutan sadar dan siap bertarung lagi.");
    Toast_Show(playerid, "Rekrutan hidup", "HP 50, kembali mengikuti.", UI_ACCENT_PRIMARY);
    return 1;
}

stock Companion_Upgrade(playerid)
{
    if (!CompHas[playerid])
    {
        SendClientMessage(playerid, UI_TEXT_MUTED, "[REKRUTAN] Kamu tidak punya rekrutan.");
        return 0;
    }

    if (CompTier[playerid] >= COMPANION_TIER_MAX)
    {
        SendClientMessage(playerid, UI_ACCENT_PRIMARY, "[REKRUTAN] Perlengkapannya sudah maksimal.");
        return 0;
    }

    new neededCash, needCloth, needElec, needRepair, needAmmo;
    if (CompTier[playerid] == 0)
    {
        neededCash = COMPANION_TIER1_CASH;
        needCloth = COMPANION_TIER1_CLOTH;
        needElec = COMPANION_TIER1_ELEC;
    }
    else
    {
        neededCash = COMPANION_TIER2_CASH;
        needRepair = COMPANION_TIER2_REPAIR;
        needElec = COMPANION_TIER2_ELEC;
        needAmmo = COMPANION_TIER2_AMMO;
    }

    /* Validasi SEMUA bahan sebelum satu pun diambil, supaya tidak ada potongan separuh. */
    new haveCloth = Inventory_CountItem(playerid, ITEM_CLOTH);
    new haveElec = Inventory_CountItem(playerid, ITEM_ELECTRONICS);
    new haveRepair = Inventory_CountItem(playerid, ITEM_REPAIR_KIT);
    new haveAmmo = Inventory_CountItem(playerid, ITEM_RIFLE_AMMO);

    if (PlayerSurvival[playerid][psCash] < neededCash
        || haveCloth < needCloth || haveElec < needElec
        || haveRepair < needRepair || haveAmmo < needAmmo)
    {
        new message[224];
        format(message, sizeof(message),
            "[REKRUTAN] Tier %d butuh $%d, kain %d/%d, elektronik %d/%d, repair kit %d/%d, amunisi rifle %d/%d.",
            CompTier[playerid] + 1, neededCash,
            haveCloth, needCloth, haveElec, needElec,
            haveRepair, needRepair, haveAmmo, needAmmo);
        SendClientMessage(playerid, UI_ACCENT_DANGER, message);
        return 0;
    }

    new bool:consumed = true;
    if (needCloth > 0 && !Inventory_Consume(playerid, ITEM_CLOTH, needCloth, "companion_tier_cloth")) consumed = false;
    if (consumed && needElec > 0 && !Inventory_Consume(playerid, ITEM_ELECTRONICS, needElec, "companion_tier_elec")) consumed = false;
    if (consumed && needRepair > 0 && !Inventory_Consume(playerid, ITEM_REPAIR_KIT, needRepair, "companion_tier_repair")) consumed = false;
    if (consumed && needAmmo > 0 && !Inventory_Consume(playerid, ITEM_RIFLE_AMMO, needAmmo, "companion_tier_ammo")) consumed = false;

    if (!consumed)
    {
        /* Refund apa pun yang sudah terlanjur diambil. */
        if (needCloth > 0) Inventory_Grant(playerid, ITEM_CLOTH, needCloth, "companion_tier_refund_cloth");
        if (needElec > 0) Inventory_Grant(playerid, ITEM_ELECTRONICS, needElec, "companion_tier_refund_elec");
        if (needRepair > 0) Inventory_Grant(playerid, ITEM_REPAIR_KIT, needRepair, "companion_tier_refund_repair");
        if (needAmmo > 0) Inventory_Grant(playerid, ITEM_RIFLE_AMMO, needAmmo, "companion_tier_refund_ammo");
        SendClientMessage(playerid, UI_ACCENT_DANGER, "[REKRUTAN] Bahan gagal diambil, uang tidak dipotong.");
        return 0;
    }

    PlayerSurvival[playerid][psCash] -= neededCash;
    CompTier[playerid]++;

    /* Perlengkapan baru memulihkan dia sepenuhnya dan memperbaiki senjata. */
    CompHealth[playerid] = Companion_MaxHealth(playerid);
    if (FCNPC_IsValid(CompNpc[playerid]))
    {
        FCNPC_SetHealth(CompNpc[playerid], CompHealth[playerid]);
        Companion_ApplyGear(playerid);
    }

    Companion_Save(playerid);
    Skills_AddXp(playerid, 15);

    new message[176];
    format(message, sizeof(message),
        "[REKRUTAN] Tier %d siap: HP %.0f, damage %.0f. -$%d.",
        CompTier[playerid], Companion_MaxHealth(playerid), Companion_ShotDamage(playerid), neededCash);
    SendClientMessage(playerid, UI_ACCENT_PRIMARY, message);
    Toast_Show(playerid, "Rekrutan naik tier", message, UI_ACCENT_PRIMARY);
    Log_Format(LOG_INFO, "[COMPANION] char=%d tier=%d", ActiveCharId[playerid], CompTier[playerid]);
    return 1;
}

stock Companion_Show(playerid)
{
    if (!Account_IsPlaying(playerid)) return 0;

    if (!CompHas[playerid])
    {
        ShowPlayerDialog(playerid, DIALOG_COMPANION, DIALOG_STYLE_MSGBOX, "Rekrutan",
            "Kamu belum punya rekrutan.\n\n\
            Pergi ke perekrut di kamp (dekat papan rekrutmen) lalu /rekrut.\n\
            Biaya: $600 + 2 makanan kaleng + 1 air mineral.",
            "Tutup", "");
        return 1;
    }

    new title[64], body[512], line[128];
    format(title, sizeof(title), "Rekrutan | HP %.0f/%.0f | Tier %d | %s",
        CompHealth[playerid], Companion_MaxHealth(playerid), CompTier[playerid],
        CompDowned[playerid] ? "TUMBANG" : ((CompMode[playerid] == COMPANION_MODE_HOLD) ? "JAGA" : "IKUT"));

    format(line, sizeof(line), "Biarkan dia ikut bergerak\n");
    format(body, sizeof(body), "%s", line);
    format(line, sizeof(line), "Tunggu/jaga di posisi ini\n");
    strcat(body, line, sizeof(body));
    format(line, sizeof(line), "Tolong dia (butuh 1 perban)\n");
    strcat(body, line, sizeof(body));
    format(line, sizeof(line), "Tingkatkan perlengkapan (tier %d/2)\n", CompTier[playerid]);
    strcat(body, line, sizeof(body));
    format(line, sizeof(line), "Bubarkan rekrutan\n");
    strcat(body, line, sizeof(body));

    ShowPlayerDialog(playerid, DIALOG_COMPANION, DIALOG_STYLE_LIST, title, body, "Pilih", "Tutup");
    return 1;
}

stock Companion_OnDialogResponse(playerid, dialogid, response, listitem, const inputtext[])
{
    #pragma unused inputtext

    if (dialogid != DIALOG_COMPANION) return 0;
    if (!response) return 1;

    if (!CompHas[playerid])
    {
        SendClientMessage(playerid, UI_TEXT_MUTED, "[REKRUTAN] Datangi perekrut di kamp lalu /rekrut.");
        return 1;
    }

    switch (listitem)
    {
        case 0:
        {
            CompMode[playerid] = COMPANION_MODE_FOLLOW;
            Companion_Spawn(playerid);
            SendClientMessage(playerid, UI_ACCENT_PRIMARY, "[REKRUTAN] Dia mengikuti kamu lagi.");
        }
        case 1:
        {
            CompMode[playerid] = COMPANION_MODE_HOLD;
            Companion_Despawn(playerid);
            Companion_Spawn(playerid);
            Companion_Save(playerid);
            SendClientMessage(playerid, UI_ACCENT_PRIMARY, "[REKRUTAN] Dia berjaga di titik ini.");
        }
        case 2: Companion_Revive(playerid);
        case 3: Companion_Upgrade(playerid);
        case 4: Companion_Dismiss(playerid);
        default: return 1;
    }
    return 1;
}

stock Companion_VehicleSelftest()
{
    new bool:ok = true;

    /* Kursi: pengemudi 0, jadi penumpang pertama = 1. */
    ok = ok && Companion_PickSeat(4, 0) == 1;
    ok = ok && Companion_PickSeat(4, (1 << 1)) == 2;
    ok = ok && Companion_PickSeat(4, (1 << 1) | (1 << 2) | (1 << 3)) == -1;
    ok = ok && Companion_PickSeat(2, 0) == 1;
    ok = ok && Companion_PickSeat(1, 0) == -1;     /* hanya pengemudi */

    /* Tabel kursi: nilai acuan model nyata. */
    ok = ok && VehicleSeatsTotal(411) == 2;        /* Infernus */
    ok = ok && VehicleSeatsTotal(431) >= 8;        /* Bus */
    ok = ok && VehiclePassengerSeats(481) == 0;    /* BMX tanpa penumpang */

    new detail[144];
    format(detail, sizeof(detail), "infernus=%d bus=%d bmx_penumpang=%d kursi(4,kosong)=%d",
        VehicleSeatsTotal(411), VehicleSeatsTotal(431),
        VehiclePassengerSeats(481), Companion_PickSeat(4, 0));
    Selftest_Record(ok, "rekrutan ikut kendaraan", detail);
    return 1;
}
