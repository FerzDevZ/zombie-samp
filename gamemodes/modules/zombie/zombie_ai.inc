#if defined _ls_zombie_ai_included
    #endinput
#endif
#define _ls_zombie_ai_included

/* The AI relies on FCNPC's native navigation (Raycast pathfinding + ColAndreas height
   mode) so zombies actually walk around walls, fences and stairs instead of pushing
   into geometry. Our job is deciding *what* they want to do and when. */

enum E_ZOMBIE_BEHAVIOUR
{
    ZB_IDLE = 0,
    ZB_INVESTIGATE,
    ZB_CHASE,
    ZB_ATTACK
};

enum E_ZOMBIE_AI
{
    zaiTarget,
    E_ZOMBIE_BEHAVIOUR:zaiBehaviour,
    Float:zaiLastX,
    Float:zaiLastY,
    Float:zaiLastZ,
    zaiLastRepathTick,
    zaiNextAttackTick,
    Float:zaiLastTargetX,
    Float:zaiLastTargetY,
    bool:zaiHasTargetPos,
    zaiLastStuckTick,
    zaiStuckStrikes,
    bool:zaiHasNoisePoint,
    Float:zaiNoiseX,
    Float:zaiNoiseY,
    Float:zaiNoiseZ,
    zaiNoiseTick,
    zaiNextScreamTick,
    bool:zaiHasAnchor,
    Float:zaiAnchorX,
    Float:zaiAnchorY,
    Float:zaiAnchorZ
}

new ZombieAI[MAX_ZOMBIE_SLOTS][E_ZOMBIE_AI];
new ZombieSliceCursor = 0;

stock ZombieAI_Reset(zombieid)
{
    if (zombieid < 0 || zombieid >= MAX_ZOMBIE_SLOTS) return 0;

    ZombieAI[zombieid][zaiTarget] = INVALID_PLAYER_ID;
    ZombieAI[zombieid][zaiBehaviour] = ZB_IDLE;
    ZombieAI[zombieid][zaiLastX] = 0.0;
    ZombieAI[zombieid][zaiLastY] = 0.0;
    ZombieAI[zombieid][zaiLastZ] = 0.0;
    ZombieAI[zombieid][zaiLastRepathTick] = 0;
    ZombieAI[zombieid][zaiNextAttackTick] = 0;
    ZombieAI[zombieid][zaiLastTargetX] = 0.0;
    ZombieAI[zombieid][zaiLastTargetY] = 0.0;
    ZombieAI[zombieid][zaiHasTargetPos] = false;
    ZombieAI[zombieid][zaiLastStuckTick] = 0;
    ZombieAI[zombieid][zaiStuckStrikes] = 0;
    ZombieAI[zombieid][zaiHasNoisePoint] = false;
    ZombieAI[zombieid][zaiNoiseX] = 0.0;
    ZombieAI[zombieid][zaiNoiseY] = 0.0;
    ZombieAI[zombieid][zaiNoiseZ] = 0.0;
    ZombieAI[zombieid][zaiNoiseTick] = 0;
    ZombieAI[zombieid][zaiNextScreamTick] = 0;
    ZombieAI[zombieid][zaiHasAnchor] = false;
    ZombieAI[zombieid][zaiAnchorX] = 0.0;
    ZombieAI[zombieid][zaiAnchorY] = 0.0;
    ZombieAI[zombieid][zaiAnchorZ] = 0.0;
    return 1;
}

stock ZombieAI_SetTarget(zombieid, playerid)
{
    if (zombieid < 0 || zombieid >= MAX_ZOMBIE_SLOTS) return 0;
    ZombieAI[zombieid][zaiTarget] = playerid;
    if (playerid != INVALID_PLAYER_ID)
    {
        ZombieAI[zombieid][zaiBehaviour] = ZB_CHASE;
        ZombieAI[zombieid][zaiLastRepathTick] = 0;
        ZombieAI[zombieid][zaiHasTargetPos] = false;
    }
    return 1;
}

/* Called from the spatial hash whenever any action emits noise. */
public Zombie_OnNoiseHeard(zombieid, Float:x, Float:y, Float:z)
{
    if (!Zombie_IsActive(zombieid)) return 0;

    new Float:zx, Float:zy, Float:zz;
    FCNPC_GetPosition(zombieid, zx, zy, zz);

    new Float:distance = VectorSize(x - zx, y - zy, z - zz);

    // Zombies closer to a sound react harder; far ones only turn towards it.
    if (distance <= 20.0)
    {
        ZombieAI[zombieid][zaiHasNoisePoint] = true;
        ZombieAI[zombieid][zaiNoiseX] = x;
        ZombieAI[zombieid][zaiNoiseY] = y;
        ZombieAI[zombieid][zaiNoiseZ] = z;
        ZombieAI[zombieid][zaiNoiseTick] = GetTickCount();
        ZombieAI[zombieid][zaiBehaviour] = ZB_INVESTIGATE;
    }
    else if (distance <= 60.0 && ZombieAI[zombieid][zaiBehaviour] == ZB_IDLE)
    {
        ZombieAI[zombieid][zaiHasNoisePoint] = true;
        ZombieAI[zombieid][zaiNoiseX] = x;
        ZombieAI[zombieid][zaiNoiseY] = y;
        ZombieAI[zombieid][zaiNoiseZ] = z;
        ZombieAI[zombieid][zaiNoiseTick] = GetTickCount();
        ZombieAI[zombieid][zaiBehaviour] = ZB_INVESTIGATE;
    }
    return 1;
}

/* Screamers pull the horde: this is what makes ignoring them a mistake. */
stock ZombieAI_Scream(zombieid, Float:x, Float:y, Float:z)
{
    new cellIndex = Hash_ZombieCellIndex(x, y);
    new centerX = cellIndex / HASH_CELLS;
    new centerY = cellIndex % HASH_CELLS;
    new summoned = 0;

    for (new cellX = centerX - 1; cellX <= centerX + 1; cellX++)
    {
        if (cellX < 0 || cellX >= HASH_CELLS) continue;
        for (new cellY = centerY - 1; cellY <= centerY + 1; cellY++)
        {
            if (cellY < 0 || cellY >= HASH_CELLS) continue;

            new neighbourCell = cellX * HASH_CELLS + cellY;

            foreach (new other : HashCellZombies[neighbourCell])
            {
                if (other == zombieid) continue;
                if (!Zombie_IsActive(other)) continue;
                if (ZombieAI[other][zaiBehaviour] == ZB_CHASE) continue;

                ZombieAI[other][zaiHasNoisePoint] = true;
                ZombieAI[other][zaiNoiseX] = x;
                ZombieAI[other][zaiNoiseY] = y;
                ZombieAI[other][zaiNoiseZ] = z;
                ZombieAI[other][zaiNoiseTick] = GetTickCount();
                ZombieAI[other][zaiBehaviour] = ZB_INVESTIGATE;
                summoned++;
            }
        }
    }
    return summoned;
}

/* Bloater death is an area denial event; Brutes chip barricades later. */
public ZombieAI_OnZombieKilled(zombieid, E_ZOMBIE_TYPE:type, Float:x, Float:y, Float:z)
{
    #pragma unused zombieid

    if (_:type != _:ZT_BLOATER) return 0;

    Hash_TriggerNoise(x, y, z, 25.0);

    foreach (new playerid : PlayerPool)
    {
        if (!IsPlayerConnected(playerid)) continue;
        if (!Player_IsAlive(playerid)) continue;
        if (Lobby_Of(playerid) != Zombie_GetLobby(zombieid)) continue;

        new Float:px, Float:py, Float:pz;
        GetPlayerPos(playerid, px, py, pz);
        if (VectorSize(px - x, py - y, pz - z) > 4.5) continue;

        Damage_Apply(DAMAGE_KIND_PLAYER, playerid, INVALID_PLAYER_ID, 40.0, 0, 3);
        if (random(100) < 50) Wounds_Add(playerid, 1);
        SendClientMessage(playerid, UI_ACCENT_DANGER,
            "[LAST STAND] Gas asam Bloater meledak. Jauhi mayatnya.");
    }
    return 1;
}

stock ZombieAI_AcquireTarget(Float:x, Float:y, Float:z, lobbyId)
{
    new Float:distance;
    new candidate = Hash_NearestPlayer(x, y, z, ZOMBIE_LOD_CHASE_DIST, distance, lobbyId);
    if (candidate == INVALID_PLAYER_ID) return INVALID_PLAYER_ID;

    if (!Player_IsAlive(candidate)) return INVALID_PLAYER_ID;
    if (Zone_IsHub(Player_GetZone(candidate))) return INVALID_PLAYER_ID;

    return candidate;
}

/* Setiap zombie dapat satu posisi di lingkaran sekeliling pemain, jadi gerombolan
   mengepung dari banyak arah alih-alih menumpuk di satu titik lalu saling pukul. */
stock ZombieAI_MoveToRing(zombieid, target, E_ZOMBIE_TYPE:type)
{
    new Float:px, Float:py, Float:pz;
    GetPlayerPos(target, px, py, pz);

    new Float:zx, Float:zy, Float:zz;
    FCNPC_GetPosition(zombieid, zx, zy, zz);

    new Float:bearing = float((zombieid * 47) % 360);
    new Float:ringRadius = ZOMBIE_APPROACH_DIST + 0.6;

    new Float:tx = px + floatsin(bearing, degrees) * ringRadius;
    new Float:ty = py + floatcos(bearing, degrees) * ringRadius;

    new Float:speed = Float:gZombieProfiles[_:type][ztRunSpeed] * Lobby_GetZombieSpeed(Zombie_GetLobby(zombieid));
    new moveType = (speed <= 0.35) ? FCNPC_MOVE_TYPE_WALK : FCNPC_MOVE_TYPE_RUN;

    /* Kalau garis ke titik tujuan benar-benar bebas, tidak perlu raycast
       pathfinding. Ini memotong biaya ColAndreas untuk gerombolan jarak dekat. */
#if ZOMBIE_PATHFINDING_RAYCAST
    new pathMode = FCNPC_MOVE_PATHFINDING_RAYCAST;
    if (VectorSize(tx - zx, ty - zy, 0.0) <= ZOMBIE_DIRECT_PATH_DIST
        && LOS_IsClear(zx, zy, zz + 0.5, tx, ty, pz + 0.5))
    {
        pathMode = FCNPC_MOVE_PATHFINDING_NONE;
    }
#else
    new pathMode = FCNPC_MOVE_PATHFINDING_NONE;
#endif

    FCNPC_GoTo(zombieid, tx, ty, pz,
        moveType,
        speed,
        FCNPC_MOVE_MODE_COLANDREAS,
        pathMode,
        0.7,
        true,
        0.5,
        250);
    return 1;
}

stock ZombieAI_MoveToPlayer(zombieid, target, E_ZOMBIE_TYPE:type, bool:usePathfinding)
{
#if ZOMBIE_PATHFINDING_RAYCAST
    new pathfindingMode = usePathfinding ? FCNPC_MOVE_PATHFINDING_RAYCAST : FCNPC_MOVE_PATHFINDING_NONE;
#else
    new pathfindingMode = FCNPC_MOVE_PATHFINDING_NONE;
#endif
    new Float:speed = Float:gZombieProfiles[_:type][ztRunSpeed] * Lobby_GetZombieSpeed(Zombie_GetLobby(zombieid));

    // Pelan = animasi jalan (shamble). Memaksa RUN untuk zombie lambat membuat
    // kakinya berjalan di tempat alias meluncur.
    new moveType = (speed <= 0.35) ? FCNPC_MOVE_TYPE_WALK : FCNPC_MOVE_TYPE_RUN;

    FCNPC_GoToPlayer(zombieid, target,
        moveType,
        speed,
        FCNPC_MOVE_MODE_COLANDREAS,
        pathfindingMode,
        ZOMBIE_APPROACH_DIST,
        true,
        1.0,
        1.5,
        250);
    return 1;
}

stock ZombieAI_TryAttack(zombieid, target, E_ZOMBIE_TYPE:type, Float:zx, Float:zy, Float:zz)
{
    if (GetTickCount() < ZombieAI[zombieid][zaiNextAttackTick]) return 0;

    new Float:px, Float:py, Float:pz;
    GetPlayerPos(target, px, py, pz);

    new Float:distance = VectorSize(px - zx, py - zy, pz - zz);
    if (distance > ZOMBIE_ATTACK_RANGE) return 0;
    ZombieStats_NoteInRange();
    if (Lobby_Of(target) != Zombie_GetLobby(zombieid)) return 0;

    // No cheap hits through walls: the same raycast that validates player shots.
    if (!LOS_IsClear(zx, zy, zz + 0.6, px, py, pz + 0.5))
    {
        ZombieStats_NoteLosFail();
        return 0;
    }

    // Stop first: a walking NPC swings at air and the hit never lands.
    FCNPC_Stop(zombieid);
    FCNPC_SetAngleToPlayer(zombieid, target);
    FCNPC_MeleeAttack(zombieid, 0, true);
    ZombieAI[zombieid][zaiLastRepathTick] = GetTickCount();

    // Jitter supaya gerombolan tidak memukul di tick yang sama seperti barisan penari.
    new delay = gZombieProfiles[_:type][ztAttackDelayMs];
    ZombieAI[zombieid][zaiNextAttackTick] = GetTickCount() + delay + random(delay / 2);

    new Float:damage = Float:gZombieProfiles[_:type][ztDamage] * Lobby_GetZombieDamage(Zombie_GetLobby(zombieid));
    Damage_Apply(DAMAGE_KIND_PLAYER, target, INVALID_PLAYER_ID, damage, 0, 1);
    ZombieStats_NoteAttack();

    /* Tekanan infeksi sesuai DESIGN §7: tiap gigitan berpeluang besar menular
       (kode lama 18% x 8-17 = ~1/8 tekanan desain; turned praktis tak
       terjangkau — audit D1). Bloater selalu menular. */
    new Float:infectionChance = (_:type == _:ZT_BLOATER) ? 100.0 : 60.0;
    if (float(random(100)) < infectionChance)
    {
        Player_AddInfection(target, 12.0 + float(random(14)));
    }

    new woundChance = 24;
    if (_:type == _:ZT_STALKER) woundChance = 42;
    if (_:type == _:ZT_BRUTE) woundChance = 55;
    if (_:type == _:ZT_BLOATER) woundChance = 32;
    if (random(100) < woundChance)
    {
        Wounds_Add(target, (_:type == _:ZT_BRUTE && random(100) < 25) ? 2 : 1);
    }

    if (_:type == _:ZT_SCREAMER && GetTickCount() >= ZombieAI[zombieid][zaiNextScreamTick])
    {
        ZombieAI[zombieid][zaiNextScreamTick] = GetTickCount() + 15000;
        ZombieAI_Scream(zombieid, zx, zy, zz);
        foreach (new nearbyPlayer : PlayerPool)
        {
            if (!IsPlayerConnected(nearbyPlayer)) continue;
            if (Lobby_Of(nearbyPlayer) != Zombie_GetLobby(zombieid)) continue;
            SendClientMessage(nearbyPlayer, UI_ACCENT_WARN, "[LAST STAND] Screamer memanggil gerombolan!");
        }
    }
    return 1;
}

/* One zombie thinks at most 5x per second (200 ms slices, 5 buckets). */
stock ZombieAI_Process(zombieid)
{
    if (!Zombie_IsActive(zombieid)) return 0;

    new E_ZOMBIE_TYPE:type = E_ZOMBIE_TYPE:Zombie_GetType(zombieid);
    new Float:zx, Float:zy, Float:zz;
    FCNPC_GetPosition(zombieid, zx, zy, zz);

    Hash_ZombieUpdate(zombieid, zx, zy);

    new Float:nearestDistance;
    new nearestPlayer = Hash_NearestPlayer(zx, zy, zz, ZOMBIE_LOD_CHASE_DIST, nearestDistance, Zombie_GetLobby(zombieid));
    ZombieStats_NoteProximity((nearestPlayer == INVALID_PLAYER_ID) ? 9999.0 : nearestDistance,
        nearestPlayer != INVALID_PLAYER_ID);

    /* Zombie raid terikat ke base sasarannya: kalau pemain terdekat di luar tali,
       ia kembali menggerus dinding, bukan berjalan 200 m mengejar orang lain. */
    if (ZombieAI[zombieid][zaiHasAnchor] && nearestPlayer != INVALID_PLAYER_ID
        && nearestDistance > BASE_RAID_LEASH)
    {
        nearestPlayer = INVALID_PLAYER_ID;
    }

    // LOD: far away zombies are parked so the AMX budget stays inside the tick target.
    if (nearestPlayer == INVALID_PLAYER_ID || nearestDistance > ZOMBIE_LOD_CHASE_DIST)
    {
        ZombieAI[zombieid][zaiTarget] = INVALID_PLAYER_ID;

        // A noise from across the street still pulls the zombie in: only go idle when
        // there is nothing worth walking to.
        if (ZombieAI[zombieid][zaiHasAnchor])
        {
            ZombieAI_ReturnToAnchor(zombieid, type, zx, zy, zz);
            return 1;
        }

        if (ZombieAI[zombieid][zaiHasNoisePoint])
        {
            ZombieAI_Investigate(zombieid, type, zx, zy, zz);
            return 1;
        }

        if (ZombieAI[zombieid][zaiBehaviour] != ZB_IDLE)
        {
            ZombieAI[zombieid][zaiBehaviour] = ZB_IDLE;
            FCNPC_Stop(zombieid);
        }
        return 1;
    }

    if (nearestDistance <= ZOMBIE_LOD_CHASE_DIST && nearestDistance > ZOMBIE_LOD_FULL_DIST)
    {
        // CHASE LOD: straight pursuit without pathfinding cost.
        new target = ZombieAI[zombieid][zaiTarget];
        if (target == INVALID_PLAYER_ID || !Player_IsAlive(target))
        {
            target = nearestPlayer;
            ZombieAI_SetTarget(zombieid, target);
        }

        new Float:cx, Float:cy, Float:cz;
        GetPlayerPos(target, cx, cy, cz);
        new bool:targetMoved = !ZombieAI[zombieid][zaiHasTargetPos]
            || VectorSize(cx - ZombieAI[zombieid][zaiLastTargetX],
                cy - ZombieAI[zombieid][zaiLastTargetY], 0.0) >= ZOMBIE_REPATH_MIN_MOVE;

        if (targetMoved
            && GetTickCount() - ZombieAI[zombieid][zaiLastRepathTick] > ZOMBIE_CHASE_REPATH_MS)
        {
            ZombieAI[zombieid][zaiLastRepathTick] = GetTickCount();
            ZombieAI[zombieid][zaiLastTargetX] = cx;
            ZombieAI[zombieid][zaiLastTargetY] = cy;
            ZombieAI[zombieid][zaiHasTargetPos] = true;
            ZombieAI_MoveToPlayer(zombieid, target, type, false);
        }
        return 1;
    }

    // FULL LOD: target validation, attack or navigate.
    new target = ZombieAI[zombieid][zaiTarget];
    if (target != INVALID_PLAYER_ID && (!IsPlayerConnected(target) || !Player_IsAlive(target)))
    {
        target = INVALID_PLAYER_ID;
    }

    if (target == INVALID_PLAYER_ID)
    {
        target = ZombieAI_AcquireTarget(zx, zy, zz, Zombie_GetLobby(zombieid));
        ZombieAI_SetTarget(zombieid, target);
    }

    if (target != INVALID_PLAYER_ID)
    {
        if (ZombieAI_TryAttack(zombieid, target, type, zx, zy, zz))
        {
            ZombieAI[zombieid][zaiBehaviour] = ZB_ATTACK;
            return 1;
        }

        /* Sudah di jangkauan pukul tapi masih cooldown: diam dan hadap pemain.
           Dulu zombie disuruh jalan ke ring setiap cooldown habis, sehingga ia
           maju-mundur dan animasinya terlihat patah. */
        new Float:px, Float:py, Float:pz;
        GetPlayerPos(target, px, py, pz);
        if (VectorSize(px - zx, py - zy, pz - zz) <= ZOMBIE_ATTACK_RANGE
            && LOS_IsClear(zx, zy, zz + 0.6, px, py, pz + 0.5))
        {
            FCNPC_SetAngleToPlayer(zombieid, target);
            ZombieAI[zombieid][zaiBehaviour] = ZB_ATTACK;
            return 1;
        }

        new bool:targetMoved = !ZombieAI[zombieid][zaiHasTargetPos]
            || VectorSize(px - ZombieAI[zombieid][zaiLastTargetX],
                py - ZombieAI[zombieid][zaiLastTargetY], 0.0) >= ZOMBIE_REPATH_MIN_MOVE;

        if (targetMoved
            && GetTickCount() - ZombieAI[zombieid][zaiLastRepathTick] > ZOMBIE_REPATH_MS)
        {
            ZombieAI[zombieid][zaiLastRepathTick] = GetTickCount();
            ZombieAI[zombieid][zaiLastTargetX] = px;
            ZombieAI[zombieid][zaiLastTargetY] = py;
            ZombieAI[zombieid][zaiHasTargetPos] = true;
            ZombieAI_MoveToRing(zombieid, target, type);
        }

        ZombieAI[zombieid][zaiBehaviour] = ZB_CHASE;
        ZombieAI_CheckStuck(zombieid, zx, zy, zz);
        return 1;
    }

    // Nobody visible: walk to the last noise point, then idle.
    if (ZombieAI[zombieid][zaiHasNoisePoint])
    {
        ZombieAI_Investigate(zombieid, type, zx, zy, zz);
        return 1;
    }

    ZombieAI[zombieid][zaiBehaviour] = ZB_IDLE;
    return 1;
}

/* Ikat zombie ke satu titik (base sasaran raid). Selama terikat, ia hanya mengejar
   pemain dalam BASE_RAID_LEASH meter; kalau kosong, ia kembali ke titik itu dan
   dinding base digerus oleh BaseBuild_ZombieTick yang sudah ada. */
stock ZombieAI_SetAnchor(zombieid, Float:x, Float:y, Float:z)
{
    if (zombieid < 0 || zombieid >= MAX_ZOMBIE_SLOTS) return 0;

    ZombieAI[zombieid][zaiHasAnchor] = true;
    ZombieAI[zombieid][zaiAnchorX] = x;
    ZombieAI[zombieid][zaiAnchorY] = y;
    ZombieAI[zombieid][zaiAnchorZ] = z;
    ZombieAI[zombieid][zaiHasNoisePoint] = false;
    return 1;
}

stock ZombieAI_ClearAnchor(zombieid)
{
    if (zombieid < 0 || zombieid >= MAX_ZOMBIE_SLOTS) return 0;
    ZombieAI[zombieid][zaiHasAnchor] = false;
    return 1;
}

stock bool:ZombieAI_HasAnchor(zombieid)
{
    if (zombieid < 0 || zombieid >= MAX_ZOMBIE_SLOTS) return false;
    return ZombieAI[zombieid][zaiHasAnchor];
}

stock ZombieAI_ReturnToAnchor(zombieid, E_ZOMBIE_TYPE:type, Float:zx, Float:zy, Float:zz)
{
    new Float:dx = ZombieAI[zombieid][zaiAnchorX] - zx;
    new Float:dy = ZombieAI[zombieid][zaiAnchorY] - zy;
    new Float:dz = ZombieAI[zombieid][zaiAnchorZ] - zz;

    ZombieAI[zombieid][zaiTarget] = INVALID_PLAYER_ID;

    if (VectorSize(dx, dy, dz) <= BASE_RAID_HOLD_DIST)
    {
        ZombieAI[zombieid][zaiBehaviour] = ZB_IDLE;
        FCNPC_Stop(zombieid);
        return 1;
    }

    ZombieAI[zombieid][zaiBehaviour] = ZB_INVESTIGATE;

    if (GetTickCount() - ZombieAI[zombieid][zaiLastRepathTick] < ZOMBIE_REPATH_MS) return 1;
    ZombieAI[zombieid][zaiLastRepathTick] = GetTickCount();

    FCNPC_GoTo(zombieid,
        ZombieAI[zombieid][zaiAnchorX],
        ZombieAI[zombieid][zaiAnchorY],
        ZombieAI[zombieid][zaiAnchorZ],
        FCNPC_MOVE_TYPE_WALK,
        Float:gZombieProfiles[_:type][ztWalkSpeed],
        FCNPC_MOVE_MODE_COLANDREAS,
        FCNPC_MOVE_PATHFINDING_RAYCAST,
        2.0,
        true,
        0.0);
    return 1;
}

/* Shared by both LOD levels: a heard sound is a promise the zombie keeps. */
stock ZombieAI_Investigate(zombieid, E_ZOMBIE_TYPE:type, Float:zx, Float:zy, Float:zz)
{
    if (GetTickCount() - ZombieAI[zombieid][zaiNoiseTick] > ZOMBIE_NOISE_MEMORY_MS)
    {
        ZombieAI[zombieid][zaiHasNoisePoint] = false;
        ZombieAI[zombieid][zaiBehaviour] = ZB_IDLE;
        FCNPC_Stop(zombieid);
        return 0;
    }

    new Float:dx = ZombieAI[zombieid][zaiNoiseX] - zx;
    new Float:dy = ZombieAI[zombieid][zaiNoiseY] - zy;

    if (VectorSize(dx, dy, 0.0) < 2.0)
    {
        ZombieAI[zombieid][zaiHasNoisePoint] = false;
        ZombieAI[zombieid][zaiBehaviour] = ZB_IDLE;
        FCNPC_Stop(zombieid);
        return 1;
    }

    ZombieAI[zombieid][zaiBehaviour] = ZB_INVESTIGATE;

    if (GetTickCount() - ZombieAI[zombieid][zaiLastRepathTick] > ZOMBIE_REPATH_MS)
    {
        ZombieAI[zombieid][zaiLastRepathTick] = GetTickCount();

        new pathMode = FCNPC_MOVE_PATHFINDING_RAYCAST;
        if (VectorSize(dx, dy, 0.0) <= ZOMBIE_DIRECT_PATH_DIST
            && LOS_IsClear(zx, zy, zz + 0.5,
                ZombieAI[zombieid][zaiNoiseX], ZombieAI[zombieid][zaiNoiseY],
                ZombieAI[zombieid][zaiNoiseZ] + 0.5))
        {
            pathMode = FCNPC_MOVE_PATHFINDING_NONE;
        }

        FCNPC_GoTo(zombieid,
            ZombieAI[zombieid][zaiNoiseX],
            ZombieAI[zombieid][zaiNoiseY],
            ZombieAI[zombieid][zaiNoiseZ],
            FCNPC_MOVE_TYPE_WALK,
            Float:gZombieProfiles[_:type][ztWalkSpeed],
            FCNPC_MOVE_MODE_COLANDREAS,
            pathMode,
            2.0,
            true,
            0.0,
            250);
    }

    ZombieAI_CheckStuck(zombieid, zx, zy, zz);
    return 1;
}

stock ZombieAI_CheckStuck(zombieid, Float:x, Float:y, Float:z)
{
    if (GetTickCount() - ZombieAI[zombieid][zaiLastStuckTick] < ZOMBIE_STUCK_CHECK_MS) return 0;
    ZombieAI[zombieid][zaiLastStuckTick] = GetTickCount();

    new Float:moved = VectorSize(x - ZombieAI[zombieid][zaiLastX],
                                y - ZombieAI[zombieid][zaiLastY],
                                z - ZombieAI[zombieid][zaiLastZ]);

    ZombieAI[zombieid][zaiLastX] = x;
    ZombieAI[zombieid][zaiLastY] = y;
    ZombieAI[zombieid][zaiLastZ] = z;

    if (moved >= ZOMBIE_STUCK_MIN_MOVE)
    {
        ZombieAI[zombieid][zaiStuckStrikes] = 0;
        return 0;
    }

    ZombieAI[zombieid][zaiStuckStrikes]++;

    // A stuck zombie never stares at a wall: it re-paths, then gets recycled.
    if (ZombieAI[zombieid][zaiStuckStrikes] >= 3)
    {
        ZombieStats_NoteRecycle();
        Log_Format(LOG_WARN, "[ZOMBIE] npc %d stuck 3x, didaur ulang", zombieid);
        new Float:sx, Float:sy, Float:sz;
        FCNPC_GetPosition(zombieid, sx, sy, sz);
        Zombie_QueueDestroy(zombieid);
        return 1;
    }

    ZombieAI[zombieid][zaiLastRepathTick] = 0;
    return 1;
}

/* Timer entry point: 200 ms, five buckets so each zombie thinks once per second. */
public Zombie_Tick()
{
    new perfStart = GetTickCount();

    /* Antrean hapus diproses di titik aman ini, bukan di dalam callback. */
    if (ZombieDestroyCount > 0)
    {
        Zombie_FlushDestroyQueue();
    }

    new bucket = ZombieSliceCursor % 5;
    ZombieSliceCursor++;

    foreach (new zombieid : ZombiePool)
    {
        if ((zombieid % 5) != bucket) continue;
        ZombieAI_Process(zombieid);
    }

    Perf_Update(PERF_ZOMBIE, perfStart);
    return 1;
}

stock Zombie_HookReachDestination(zombieid)
{
    if (!Zombie_IsActive(zombieid)) return 0;
    ZombieAI[zombieid][zaiHasNoisePoint] = false;
    return 1;
}
