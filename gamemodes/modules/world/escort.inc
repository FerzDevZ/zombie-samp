#if defined _ls_escort_included
    #endinput
#endif
#define _ls_escort_included

/* Escort trader: misi singkat yang memaksa pemain bergerak dari zona bahaya
   kembali ke kamp bersama NPC. NPC tidak memakai jalur khusus; dia mengikuti
   pemain, sedangkan gerombolan tetap mengejar pemain. Ini membuat misi bisa
   diselesaikan tanpa pathfinding baru, tetapi tetap berisiko. */

#define ESCORT_COOLDOWN_MS       600000
#define ESCORT_FAIL_COOLDOWN_MS  180000
#define ESCORT_MAX_DURATION_MS   480000
#define ESCORT_FAIL_DIST         45.0
#define ESCORT_GOAL_DIST         12.0
#define ESCORT_NPC_HEALTH        100.0
#define ESCORT_ZOMBIE_RANGE      3.0
#define ESCORT_ZOMBIE_DAMAGE     16.0
#define ESCORT_REWARD_CASH       750
#define ESCORT_REWARD_SCRAP      8
#define ESCORT_REWARD_XP         40
#define ESCORT_NPC_SKIN          28
#define ESCORT_MAX_ACTIVE        2
#define ESCORT_HEAT_INTERVAL_MS  4000
#define ESCORT_HEAT_RADIUS       20.0

new bool:EscortActive[MAX_LOBBIES];
new EscortNpc[MAX_LOBBIES];
new EscortPlayer[MAX_LOBBIES];
new Float:EscortHealth[MAX_LOBBIES];
new EscortStartedTick[MAX_LOBBIES];
new EscortReadyTick[MAX_LOBBIES];
new EscortLastHeatTick[MAX_LOBBIES];

forward Escort_Tick();

stock Escort_Init()
{
    for (new lobby = 0; lobby < MAX_LOBBIES; lobby++)
    {
        EscortActive[lobby] = false;
        EscortNpc[lobby] = INVALID_PLAYER_ID;
        EscortPlayer[lobby] = INVALID_PLAYER_ID;
        EscortHealth[lobby] = ESCORT_NPC_HEALTH;
        EscortStartedTick[lobby] = 0;
        EscortReadyTick[lobby] = 0;
        EscortLastHeatTick[lobby] = 0;
    }
    return 1;
}

stock bool:Escort_IsNpc(npcid)
{
    for (new lobby = 0; lobby < MAX_LOBBIES; lobby++)
    {
        if (!EscortActive[lobby]) continue;
        if (EscortNpc[lobby] == npcid) return true;
    }
    return false;
}

stock Escort_DestroyNpc(npcid)
{
    if (npcid == INVALID_PLAYER_ID) return 0;
    if (FCNPC_IsValid(npcid)) FCNPC_Destroy(npcid);
    return 1;
}

stock Escort_Fail(lobby, const reason[])
{
    if (lobby < 0 || lobby >= MAX_LOBBIES) return 0;
    if (!EscortActive[lobby]) return 0;

    new npcid = EscortNpc[lobby];
    new carrier = EscortPlayer[lobby];

    EscortActive[lobby] = false;
    EscortNpc[lobby] = INVALID_PLAYER_ID;
    EscortPlayer[lobby] = INVALID_PLAYER_ID;
    EscortHealth[lobby] = ESCORT_NPC_HEALTH;
    EscortStartedTick[lobby] = 0;
    EscortReadyTick[lobby] = GetTickCount() + ESCORT_FAIL_COOLDOWN_MS;
    EscortLastHeatTick[lobby] = 0;
    Escort_DestroyNpc(npcid);

    if (IsPlayerConnected(carrier))
    {
        new line[144];
        format(line, sizeof(line), "[ESCORT] Gagal: %s. Trader hilang, coba lagi beberapa menit.", reason);
        SendClientMessage(carrier, UI_ACCENT_DANGER, line);
        Toast_Show(carrier, "ESCORT GAGAL", reason, UI_ACCENT_DANGER);
    }

    Log_Format(LOG_WARN, "[ESCORT] lobby=%d gagal: %s", lobby, reason);
    return 1;
}

stock Escort_Complete(lobby)
{
    if (lobby < 0 || lobby >= MAX_LOBBIES) return 0;
    if (!EscortActive[lobby]) return 0;

    new npcid = EscortNpc[lobby];
    new carrier = EscortPlayer[lobby];

    EscortActive[lobby] = false;
    EscortNpc[lobby] = INVALID_PLAYER_ID;
    EscortPlayer[lobby] = INVALID_PLAYER_ID;
    EscortHealth[lobby] = ESCORT_NPC_HEALTH;
    EscortStartedTick[lobby] = 0;
    EscortReadyTick[lobby] = GetTickCount() + ESCORT_COOLDOWN_MS;
    EscortLastHeatTick[lobby] = 0;
    Escort_DestroyNpc(npcid);

    if (!IsPlayerConnected(carrier)) return 1;

    /* Grant-first: medkit masuk tas DULU sebelum cash (audit F7). Tas penuh =
       medkit hangus dengan pesan jelas, bukan cash masuk tanpa jejak.
       Scrap = item sungguhan (E1). Blueprint suppressor dihapus dari reward
       sampai fiturnya ada (item mati — D3); diganti electronics. */
    if (!Inventory_Grant(carrier, ITEM_MEDKIT, 2, "escort"))
    {
        SendClientMessage(carrier, UI_ACCENT_WARN, "[ESCORT] Tas penuh: 2 medkit hangus. Kosongkan tas sebelum escort berikutnya.");
    }
    PlayerSurvival[carrier][psCash] += ESCORT_REWARD_CASH;
    Inventory_Grant(carrier, ITEM_SCRAP, ESCORT_REWARD_SCRAP, "escort_scrap");
    if (random(100) < 25) Inventory_Grant(carrier, ITEM_BLUEPRINT_SUPPRESSOR, 1, "escort");
    Skills_AddXp(carrier, ESCORT_REWARD_XP);

    new line[160];
    format(line, sizeof(line),
        "[ESCORT] Trader selamat. +$%d, +%d scrap, 2 medkit, +%d XP.",
        ESCORT_REWARD_CASH, ESCORT_REWARD_SCRAP, ESCORT_REWARD_XP);
    SendClientMessage(carrier, UI_ACCENT_PRIMARY, line);
    Toast_Show(carrier, "ESCORT BERHASIL", "Trader membayar dan kembali ke kamp.", UI_ACCENT_PRIMARY);

    Log_Format(LOG_INFO, "[ESCORT] char=%d selesai lobby=%d reward=$%d",
        ActiveCharId[carrier], lobby, ESCORT_REWARD_CASH);
    return 1;
}

stock Escort_Start(playerid)
{
    if (!IsPlayerConnected(playerid) || !Account_IsPlaying(playerid)) return 0;
    if (Player_IsDowned(playerid) || !Player_IsAlive(playerid)) return 0;

    /* Mulai harus >= 150 m dari pusat kamp: start tepat di tepi safe zone
       membuat trader spawn di samping pemain lalu jalan balik 12 m = selesai
       30 detik tanpa risiko (audit F7). */
    new Float:startX, Float:startY, Float:startZ;
    GetPlayerPos(playerid, startX, startY, startZ);
    if (VectorSize(startX - 1685.0, startY - (-2333.0), startZ - 13.5) < 150.0)
    {
        SendClientMessage(playerid, UI_ACCENT_WARN, "[ESCORT] Trader menunggu di luar kamp. Menjauh minimal 150 m dari pusat kamp dulu.");
        return 0;
    }

    new lobby = Lobby_Of(playerid);
    if (!Lobby_IsValid(lobby)) lobby = 0;

    /* Batas global escort aktif dipakai, bukan konstanta mati: NPC trader
       menambah beban sync FCNPC dan heat, jadi jumlahnya dibatasi server-wide. */
    if (!EscortActive[lobby])
    {
        new activeEscorts = 0;
        for (new lobbyIndex = 0; lobbyIndex < MAX_LOBBIES; lobbyIndex++)
        {
            if (EscortActive[lobbyIndex]) activeEscorts++;
        }
        if (activeEscorts >= ESCORT_MAX_ACTIVE)
        {
            SendClientMessage(playerid, UI_ACCENT_WARN, "[ESCORT] Semua trader sedang diantar orang lain. Coba lagi nanti.");
            return 0;
        }
    }

    if (EscortActive[lobby])
    {
        new carrier = EscortPlayer[lobby];
        if (carrier == playerid)
        {
            new npcid = EscortNpc[lobby];
            new Float:px, Float:py, Float:pz, Float:nx, Float:ny, Float:nz;
            GetPlayerPos(playerid, px, py, pz);
            if (FCNPC_IsValid(npcid)) FCNPC_GetPosition(npcid, nx, ny, nz);
            new line[144];
            format(line, sizeof(line), "[ESCORT] Trader mengikuti Anda. Jarak %.1f m. Antar ke kamp.",
                VectorSize(px - nx, py - ny, pz - nz));
            SendClientMessage(playerid, UI_ACCENT_INFO, line);
        }
        else
        {
            SendClientMessage(playerid, UI_TEXT_MUTED, "[ESCORT] Sudah ada escort aktif di lobby ini.");
        }
        return 0;
    }

    new now = GetTickCount();
    if (now < EscortReadyTick[lobby])
    {
        new seconds = (EscortReadyTick[lobby] - now + 999) / 1000;
        new line[96];
        format(line, sizeof(line), "[ESCORT] Trader sedang istirahat. Coba lagi dalam %d detik.", seconds);
        SendClientMessage(playerid, UI_TEXT_MUTED, line);
        return 0;
    }

    if (Player_IsSafe(playerid))
    {
        SendClientMessage(playerid, UI_TEXT_MUTED, "[ESCORT] Cari trader di luar kamp. Escort dimulai dari zona bahaya.");
        return 0;
    }

    if (Zombie_GetActiveCount() >= MAX_ZOMBIES - 1)
    {
        SendClientMessage(playerid, UI_TEXT_MUTED, "[ESCORT] Terlalu banyak zombie aktif. Coba lagi sebentar.");
        return 0;
    }

    if (!Npc_SlotAvailable())
    {
        Npc_BudgetWarn("escort");
        SendClientMessage(playerid, UI_TEXT_MUTED, "[ESCORT] Slot NPC server penuh. Coba lagi sebentar.");
        return 0;
    }

    new Float:x, Float:y, Float:z;
    GetPlayerPos(playerid, x, y, z);
    new name[32];
    format(name, sizeof(name), "ls_trader_%d_%d", lobby, GetTickCount());
    new npcid = FCNPC_Create(name);
    if (npcid == INVALID_PLAYER_ID || npcid < 0)
    {
        SendClientMessage(playerid, UI_ACCENT_DANGER, "[ESCORT] Trader tidak bisa muncul. Coba lagi.");
        Log_Format(LOG_WARN, "[ESCORT] FCNPC_Create gagal lobby=%d", lobby);
        return 0;
    }

    Npc_HideNameTag(npcid);
    FCNPC_Spawn(npcid, ESCORT_NPC_SKIN, x + 1.6, y, z);
    FCNPC_SetVirtualWorld(npcid, Lobby_WorldById(lobby));
    FCNPC_SetHealth(npcid, ESCORT_NPC_HEALTH);
    FCNPC_SetInvulnerable(npcid, false);
    FCNPC_SetSpeed(npcid, 0.72);
    FCNPC_GoToPlayer(npcid, playerid, FCNPC_MOVE_TYPE_RUN, 0.72,
        FCNPC_MOVE_MODE_COLANDREAS, FCNPC_MOVE_PATHFINDING_RAYCAST, 2.0, true, 1.0, 1.5, 250);

    EscortActive[lobby] = true;
    EscortNpc[lobby] = npcid;
    EscortPlayer[lobby] = playerid;
    EscortHealth[lobby] = ESCORT_NPC_HEALTH;
    EscortStartedTick[lobby] = now;
    EscortLastHeatTick[lobby] = now;

    SendClientMessage(playerid, UI_ACCENT_PRIMARY,
        "[ESCORT] Trader ikut. Antar dia ke kamp; jangan biarkan zombie mendekat. /escort untuk status.");
    Log_Format(LOG_INFO, "[ESCORT] char=%d mulai lobby=%d npc=%d", ActiveCharId[playerid], lobby, npcid);
    return 1;
}

public Escort_Tick()
{
    for (new lobby = 0; lobby < MAX_LOBBIES; lobby++)
    {
        if (!EscortActive[lobby]) continue;

        new npcid = EscortNpc[lobby];
        new carrier = EscortPlayer[lobby];
        if (!IsPlayerConnected(carrier) || !Player_IsAlive(carrier) || !FCNPC_IsValid(npcid) || FCNPC_IsDead(npcid))
        {
            Escort_Fail(lobby, "trader atau pengantar tewas");
            continue;
        }

        new Float:px, Float:py, Float:pz, Float:nx, Float:ny, Float:nz;
        GetPlayerPos(carrier, px, py, pz);
        FCNPC_GetPosition(npcid, nx, ny, nz);

        if (VectorSize(px - nx, py - ny, pz - nz) > ESCORT_FAIL_DIST)
        {
            Escort_Fail(lobby, "trader tertinggal terlalu jauh");
            continue;
        }

        if (GetTickCount() - EscortStartedTick[lobby] > ESCORT_MAX_DURATION_MS)
        {
            Escort_Fail(lobby, "waktu habis");
            continue;
        }

        if (Zone_IsHub(Player_GetZone(carrier)) && VectorSize(px - nx, py - ny, pz - nz) <= ESCORT_GOAL_DIST)
        {
            Escort_Complete(lobby);
            continue;
        }

        /* Pemain boleh menembak trader; health asli NPC adalah sumber kebenaran
           sebelum keroyokan zombie ditambahkan, supaya health tidak melompat naik. */
        new Float:liveHealth = FCNPC_GetHealth(npcid);
        if (liveHealth < EscortHealth[lobby]) EscortHealth[lobby] = liveHealth;

        new attackers = 0;
        foreach (new zombieid : ZombiePool)
        {
            if (!Zombie_IsActive(zombieid)) continue;
            if (Zombie_GetLobby(zombieid) != lobby) continue;

            new Float:zx, Float:zy, Float:zz;
            FCNPC_GetPosition(zombieid, zx, zy, zz);
            if (VectorSize(zx - nx, zy - ny, zz - nz) <= ESCORT_ZOMBIE_RANGE)
            {
                attackers++;
                if (attackers >= 3) break;
            }
        }

        if (attackers > 0)
        {
            EscortHealth[lobby] -= ESCORT_ZOMBIE_DAMAGE * float(attackers);
            if (EscortHealth[lobby] <= 0.0)
            {
                Escort_Fail(lobby, "trader dikeroyok zombie");
                continue;
            }
            FCNPC_SetHealth(npcid, EscortHealth[lobby]);
            if (!EscortActive[lobby]) continue;
        }

        if (GetTickCount() - EscortLastHeatTick[lobby] >= ESCORT_HEAT_INTERVAL_MS)
        {
            EscortLastHeatTick[lobby] = GetTickCount();
            Horde_AddHeat(nx, ny, nz, ESCORT_HEAT_RADIUS);
        }

        FCNPC_GoToPlayer(npcid, carrier, FCNPC_MOVE_TYPE_RUN, 0.72,
            FCNPC_MOVE_MODE_COLANDREAS, FCNPC_MOVE_PATHFINDING_RAYCAST, 2.0, true, 1.0, 1.5, 250);
    }
    return 1;
}

stock Escort_OnNpcDeath(npcid)
{
    for (new lobby = 0; lobby < MAX_LOBBIES; lobby++)
    {
        if (!EscortActive[lobby]) continue;
        if (EscortNpc[lobby] != npcid) continue;
        Escort_Fail(lobby, "trader tewas");
        return 1;
    }
    return 0;
}

stock Escort_CancelForPlayer(playerid, const reason[])
{
    for (new lobby = 0; lobby < MAX_LOBBIES; lobby++)
    {
        if (!EscortActive[lobby]) continue;
        if (EscortPlayer[lobby] != playerid) continue;
        Escort_Fail(lobby, reason);
    }
    return 1;
}

stock Escort_Shutdown()
{
    for (new lobby = 0; lobby < MAX_LOBBIES; lobby++)
    {
        if (!EscortActive[lobby]) continue;
        EscortActive[lobby] = false;
        Escort_DestroyNpc(EscortNpc[lobby]);
        EscortNpc[lobby] = INVALID_PLAYER_ID;
        EscortPlayer[lobby] = INVALID_PLAYER_ID;
    }
    return 1;
}
