#if defined _ls_spatial_hash_included
    #endinput
#endif
#define _ls_spatial_hash_included

/* Flat grids of iterators: O(1) neighbour lookup instead of scanning the whole pool.
   Zombie grid uses 100 m cells, player grid uses 200 m cells. */

new Iterator:HashCellZombies[HASH_ZOMBIE_SLOTS]<MAX_ZOMBIE_SLOTS>;
forward bool:Player_IsAlive(playerid);
forward bool:Player_IsSafe(playerid);

new Iterator:HashCellPlayers[HASH_PLAYER_SLOTS]<MAX_PLAYERS>;

new HashZombieCell[MAX_ZOMBIE_SLOTS];
new bool:HashZombieIndexed[MAX_ZOMBIE_SLOTS];

new HashPlayerCell[MAX_PLAYERS];
new bool:HashPlayerIndexed[MAX_PLAYERS];

forward Zombie_OnNoiseHeard(zombieid, Float:x, Float:y, Float:z);

/* Array iterator tidak menginisialisasi dirinya sendiri seperti Iterator
   tunggal. Tanpa Iter_Init, foreach pada sel hash selalu kosong: zombie tidak
   pernah melihat pemain dan sistem noise tidak pernah membangunkan siapa pun. */
stock Hash_Init()
{
    Iter_Init(HashCellZombies);
    Iter_Init(HashCellPlayers);
    return 1;
}

stock Hash_ZombieCellIndex(Float:x, Float:y)
{
    new cellX = floatround((x - HASH_MIN_COORD) / HASH_CELL_SIZE, floatround_floor);
    new cellY = floatround((y - HASH_MIN_COORD) / HASH_CELL_SIZE, floatround_floor);

    if (cellX < 0) cellX = 0;
    if (cellX >= HASH_CELLS) cellX = HASH_CELLS - 1;
    if (cellY < 0) cellY = 0;
    if (cellY >= HASH_CELLS) cellY = HASH_CELLS - 1;

    return cellX * HASH_CELLS + cellY;
}

stock Hash_PlayerCellIndex(Float:x, Float:y)
{
    new cellX = floatround((x - HASH_MIN_COORD) / 200.0, floatround_floor);
    new cellY = floatround((y - HASH_MIN_COORD) / 200.0, floatround_floor);

    if (cellX < 0) cellX = 0;
    if (cellX >= HASH_PLAYER_CELLS) cellX = HASH_PLAYER_CELLS - 1;
    if (cellY < 0) cellY = 0;
    if (cellY >= HASH_PLAYER_CELLS) cellY = HASH_PLAYER_CELLS - 1;

    return cellX * HASH_PLAYER_CELLS + cellY;
}

stock Hash_ZombieAdd(zombieid, Float:x, Float:y)
{
    if (zombieid < 0 || zombieid >= MAX_ZOMBIE_SLOTS) return 0;
    if (HashZombieIndexed[zombieid]) return 1;

    HashZombieCell[zombieid] = Hash_ZombieCellIndex(x, y);
    HashZombieIndexed[zombieid] = true;
    Iter_Add(HashCellZombies[HashZombieCell[zombieid]], zombieid);
    return 1;
}

stock Hash_ZombieRemove(zombieid)
{
    if (zombieid < 0 || zombieid >= MAX_ZOMBIE_SLOTS) return 0;
    if (!HashZombieIndexed[zombieid]) return 1;

    Iter_Remove(HashCellZombies[HashZombieCell[zombieid]], zombieid);
    HashZombieIndexed[zombieid] = false;
    return 1;
}

stock Hash_ZombieUpdate(zombieid, Float:x, Float:y)
{
    if (zombieid < 0 || zombieid >= MAX_ZOMBIE_SLOTS) return 0;
    if (!HashZombieIndexed[zombieid]) return Hash_ZombieAdd(zombieid, x, y);

    new cell = Hash_ZombieCellIndex(x, y);
    if (cell == HashZombieCell[zombieid]) return 1;

    Iter_Remove(HashCellZombies[HashZombieCell[zombieid]], zombieid);
    HashZombieCell[zombieid] = cell;
    Iter_Add(HashCellZombies[cell], zombieid);
    return 1;
}

stock Hash_PlayerUpdate(playerid, Float:x, Float:y)
{
    if (playerid < 0 || playerid >= MAX_PLAYERS) return 0;

    new cell = Hash_PlayerCellIndex(x, y);

    if (HashPlayerIndexed[playerid])
    {
        if (cell == HashPlayerCell[playerid]) return 1;
        Iter_Remove(HashCellPlayers[HashPlayerCell[playerid]], playerid);
    }

    HashPlayerCell[playerid] = cell;
    HashPlayerIndexed[playerid] = true;
    Iter_Add(HashCellPlayers[cell], playerid);
    return 1;
}

stock Hash_PlayerRemove(playerid)
{
    if (playerid < 0 || playerid >= MAX_PLAYERS) return 0;
    if (!HashPlayerIndexed[playerid]) return 1;

    Iter_Remove(HashCellPlayers[HashPlayerCell[playerid]], playerid);
    HashPlayerIndexed[playerid] = false;
    return 1;
}

/* Closest connected player within scanRadius, or INVALID_PLAYER_ID. */
stock Hash_NearestPlayer(Float:x, Float:y, Float:z, Float:scanRadius, &Float:outDistance, lobbyId = -1)
{
    new bestPlayer = INVALID_PLAYER_ID;
    outDistance = 99999.0;

    new baseCell = Hash_PlayerCellIndex(x, y);
    new centerX = baseCell / HASH_PLAYER_CELLS;
    new centerY = baseCell % HASH_PLAYER_CELLS;
    new span = floatround(scanRadius / 200.0, floatround_ceil) + 1;

    for (new cellX = centerX - span; cellX <= centerX + span; cellX++)
    {
        if (cellX < 0 || cellX >= HASH_PLAYER_CELLS) continue;

        for (new cellY = centerY - span; cellY <= centerY + span; cellY++)
        {
            if (cellY < 0 || cellY >= HASH_PLAYER_CELLS) continue;

            new cellIndex = cellX * HASH_PLAYER_CELLS + cellY;

            foreach (new candidate : HashCellPlayers[cellIndex])
            {
                if (!IsPlayerConnected(candidate)) continue;
                if (IsPlayerNPC(candidate)) continue;
                if (!Player_IsAlive(candidate)) continue;
                if (Player_IsSafe(candidate)) continue;   // shelter/hub bukan target zombie
                if (lobbyId >= 0 && Lobby_Of(candidate) != lobbyId) continue; // zombie tidak menyeberang lobby

                new Float:px, Float:py, Float:pz;
                GetPlayerPos(candidate, px, py, pz);

                if (pz - z > 25.0 || pz - z < -25.0) continue;

                new Float:dist = VectorSize(px - x, py - y, 0.0);
                if (dist > scanRadius) continue;

                if (dist < outDistance)
                {
                    outDistance = dist;
                    bestPlayer = candidate;
                }
            }
        }
    }
    return bestPlayer;
}

/* Every loud action wakes zombies inside the noise radius. */
stock Hash_TriggerNoise(Float:x, Float:y, Float:z, Float:radius, lobbyId = -1)
{
    new centerCell = Hash_ZombieCellIndex(x, y);
    new centerX = centerCell / HASH_CELLS;
    new centerY = centerCell % HASH_CELLS;
    new span = floatround(radius / HASH_CELL_SIZE, floatround_ceil) + 1;
    new notified = 0;

    for (new cellX = centerX - span; cellX <= centerX + span; cellX++)
    {
        if (cellX < 0 || cellX >= HASH_CELLS) continue;

        for (new cellY = centerY - span; cellY <= centerY + span; cellY++)
        {
            if (cellY < 0 || cellY >= HASH_CELLS) continue;

            new cellIndex = cellX * HASH_CELLS + cellY;

            foreach (new zombieid : HashCellZombies[cellIndex])
            {
                if (lobbyId >= 0 && Zombie_GetLobby(zombieid) != lobbyId) continue;
                Zombie_OnNoiseHeard(zombieid, x, y, z);
                notified++;
            }
        }
    }
    return notified;
}

stock Hash_CountZombiesInCell(Float:x, Float:y)
{
    new cellIndex = Hash_ZombieCellIndex(x, y);
    new total = 0;

    foreach (new zombieid : HashCellZombies[cellIndex])
    {
        #pragma unused zombieid
        total++;
    }
    return total;
}
