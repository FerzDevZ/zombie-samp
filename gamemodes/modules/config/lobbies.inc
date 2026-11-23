#if defined _ls_lobbies_config_included
    #endinput
#endif
#define _ls_lobbies_config_included

/* Sub-lobby: satu IP, satu proses server, beberapa dunia terpisah.
   Pemisahan memakai virtual world, jadi pemain di lobby berbeda tidak saling melihat
   dan zombie tiap lobby berdiri sendiri. Satu karakter bisa pindah lobby di hub. */

#define MAX_LOBBIES        3
#define LOBBY_WORLD_BASE   100

enum E_LOBBY_DATA
{
    lobbyName[32],
    lobbyTagline[72],
    lobbyWorld,
    lobbyZombieShare,      // persen jatah budget zombie saat lobby ini aktif
    Float:lobbyZombieSpeed,
    Float:lobbyZombieDamage,
    lobbyAccent
};

new const gLobbies[MAX_LOBBIES][E_LOBBY_DATA] = {
    { "Quarantine", "Normal. Zombie standar, loot normal, PvP mati.", 100, 60, 1.00, 1.00, 0x47C46AFF },
    { "Outbreak",   "Hardcore. Zombie lebih cepat dan memukul 35% lebih keras.", 200, 60, 1.15, 1.35, 0xE0574FFF },
    { "Nightfall",  "Malam permanen. Gelap, sunyi, stalker berkeliaran.", 300, 60, 1.10, 1.20, 0x8A6BE8FF }
};

new LobbyCount = 2;          // Nightfall disiapkan, diaktifkan otomatis saat populasi cukup.

/* Nightfall tidak dibuka manual: dibuka saat populasi cukup, ditutup hanya
   ketika lobby itu kosong. Ambang dan hysteresis di sini supaya penyetelan
   tidak perlu menyentuh logika. */
#define NIGHTFALL_LOBBY_ID        2
#define NIGHTFALL_UNLOCK_PLAYERS  20      // total pemain non-NPC online
#define NIGHTFALL_CONFIRM_TICKS   3       // 3 x 60 dtk bertahan sebelum dibuka
#define NIGHTFALL_IDLE_TICKS      10      // 10 x 60 dtk kosong sebelum ditutup
#define NIGHTFALL_HOUR            22      // jam permanen lobby Nightfall

new LobbyNightfallVotes = 0;      // hitungan berturut-turut di atas ambang
new LobbyNightfallIdle = 0;       // hitungan berturut-turut lobby kosong
new bool:LobbyPermanentNight[MAX_LOBBIES];

forward Lobby_ApplyClock(playerid);
forward Lobby_ReapplyClocks();
forward Lobby_AutoTick();
forward bool:Lobby_IsPermanentNight(lobbyId);
forward Lobby_Unlock(lobbyId, const reason[]);
forward Lobby_Lock(lobbyId, const reason[]);

/* Dipakai lintas modul yang di-include lebih awal (spatial hash, zombie, director). */
new PlayerLobby[MAX_PLAYERS];

forward Lobby_Of(playerid);
forward Lobby_WorldOf(playerid);
forward Lobby_WorldById(lobbyId);
forward Lobby_CountPlayers(lobbyId, bool:aliveOnly);
forward Lobby_ApplyWorld(playerid);
forward Lobby_IsActive(lobbyId);

stock bool:Lobby_IsValid(lobbyId)
{
    return lobbyId >= 0 && lobbyId < MAX_LOBBIES && lobbyId < LobbyCount;
}

stock Lobby_GetName(lobbyId, destination[], size)
{
    if (!Lobby_IsValid(lobbyId))
    {
        format(destination, size, "-");
        return 0;
    }
    format(destination, size, "%s", gLobbies[lobbyId][lobbyName]);
    return 1;
}

stock Lobby_GetTagline(lobbyId, destination[], size)
{
    if (!Lobby_IsValid(lobbyId))
    {
        format(destination, size, "-");
        return 0;
    }
    format(destination, size, "%s", gLobbies[lobbyId][lobbyTagline]);
    return 1;
}

stock Lobby_GetWorld(lobbyId)
{
    if (lobbyId < 0 || lobbyId >= MAX_LOBBIES) return LOBBY_WORLD_BASE;
    return gLobbies[lobbyId][lobbyWorld];
}

stock Float:Lobby_GetZombieSpeed(lobbyId)
{
    if (lobbyId < 0 || lobbyId >= MAX_LOBBIES) return 1.0;
    return gLobbies[lobbyId][lobbyZombieSpeed];
}

stock Float:Lobby_GetZombieDamage(lobbyId)
{
    if (lobbyId < 0 || lobbyId >= MAX_LOBBIES) return 1.0;
    return gLobbies[lobbyId][lobbyZombieDamage];
}

stock Lobby_GetAccent(lobbyId)
{
    if (lobbyId < 0 || lobbyId >= MAX_LOBBIES) return UI_ACCENT_PRIMARY;
    return gLobbies[lobbyId][lobbyAccent];
}

stock Lobby_GetZombieShare(lobbyId)
{
    if (lobbyId < 0 || lobbyId >= MAX_LOBBIES) return 100;
    return gLobbies[lobbyId][lobbyZombieShare];
}