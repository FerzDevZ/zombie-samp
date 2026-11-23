#if defined _ls_lobbies_included
    #endinput
#endif
#define _ls_lobbies_included

#define DIALOG_LOBBY 3001

new LobbySelectionPending[MAX_PLAYERS];

stock Lobby_Of(playerid)
{
    if (playerid < 0 || playerid >= MAX_PLAYERS) return 0;
    if (PlayerLobby[playerid] < 0 || PlayerLobby[playerid] >= MAX_LOBBIES) return 0;
    return PlayerLobby[playerid];
}

stock Lobby_WorldOf(playerid)
{
    return Lobby_WorldById(Lobby_Of(playerid));
}

stock Lobby_WorldById(lobbyId)
{
    return Lobby_GetWorld(lobbyId);
}

stock Lobby_CountPlayers(lobbyId, bool:aliveOnly)
{
    new count = 0;
    foreach (new playerid : PlayerPool)
    {
        if (!IsPlayerConnected(playerid)) continue;
        if (IsPlayerNPC(playerid)) continue;
        if (Lobby_Of(playerid) != lobbyId) continue;
        if (aliveOnly && !Player_IsAlive(playerid)) continue;
        count++;
    }
    return count;
}

stock Lobby_IsActive(lobbyId)
{
    return Lobby_IsValid(lobbyId) && Lobby_CountPlayers(lobbyId, false) > 0;
}

stock Lobby_CountOnline()
{
    new count = 0;
    foreach (new playerid : PlayerPool)
    {
        if (!IsPlayerConnected(playerid)) continue;
        if (IsPlayerNPC(playerid)) continue;
        count++;
    }
    return count;
}

stock bool:Lobby_IsPermanentNight(lobbyId)
{
    return lobbyId >= 0 && lobbyId < MAX_LOBBIES && LobbyPermanentNight[lobbyId];
}

/* SetWorldTime berlaku untuk seluruh server, jadi lobby malam permanen memakai
   SetPlayerTime per pemain. Pemain di lobby lain dikembalikan ke jam dunia
   supaya tidak terjebak malam setelah pindah dari Nightfall. */
stock Lobby_ApplyClock(playerid)
{
    if (!IsPlayerConnected(playerid)) return 0;

    if (Lobby_IsPermanentNight(Lobby_Of(playerid)))
    {
        SetPlayerTime(playerid, NIGHTFALL_HOUR, 0);
        return 1;
    }

    SetPlayerTime(playerid, World_GetHour(), 0);
    return 0;
}

stock Lobby_ReapplyClocks()
{
    foreach (new playerid : PlayerPool)
    {
        Lobby_ApplyClock(playerid);
    }
    return 1;
}

stock Lobby_Unlock(lobbyId, const reason[])
{
    if (lobbyId < 0 || lobbyId >= MAX_LOBBIES) return 0;

    if (lobbyId >= LobbyCount)
    {
        /* Hanya slot berikutnya yang boleh dibuka: melompati slot membuat
           dialog /lobby menampilkan entri kosong di tengah daftar. */
        if (lobbyId != LobbyCount) return 0;
        LobbyCount++;
    }

    LobbyPermanentNight[lobbyId] = (lobbyId == NIGHTFALL_LOBBY_ID);
    LobbyNightfallVotes = 0;
    LobbyNightfallIdle = 0;

    new lobbyLabel[32], message[160];
    Lobby_GetName(lobbyId, lobbyLabel, sizeof(lobbyLabel));
    format(message, sizeof(message), "[LOBBY] %s dibuka (%s).", lobbyLabel, reason);
    Log_Info(message);

    foreach (new playerid : PlayerPool)
    {
        if (!IsPlayerConnected(playerid) || IsPlayerNPC(playerid)) continue;
        SendClientMessage(playerid, Lobby_GetAccent(lobbyId), message);
        Toast_Show(playerid, lobbyLabel, message, Lobby_GetAccent(lobbyId));
    }
    return 1;
}

stock Lobby_Lock(lobbyId, const reason[])
{
    /* Lobby 0 adalah default: menutupnya membuat Lobby_Of jatuh ke id yang
       tidak valid dan semua pemain kehilangan dunia. Hanya ekor daftar boleh
       ditutup supaya tidak ada lubang di tengah. */
    if (lobbyId <= 0 || lobbyId >= LobbyCount) return 0;
    if (lobbyId != LobbyCount - 1) return 0;
    if (Lobby_CountPlayers(lobbyId, false) > 0) return 0;

    new lobbyLabel[32], message[160];
    Lobby_GetName(lobbyId, lobbyLabel, sizeof(lobbyLabel));

    LobbyCount--;
    LobbyPermanentNight[lobbyId] = false;
    LobbyNightfallVotes = 0;
    LobbyNightfallIdle = 0;

    format(message, sizeof(message), "[LOBBY] %s ditutup (%s).", lobbyLabel, reason);
    Log_Info(message);

    foreach (new playerid : PlayerPool)
    {
        if (!IsPlayerConnected(playerid) || IsPlayerNPC(playerid)) continue;
        SendClientMessage(playerid, UI_TEXT_MUTED, message);
    }
    return 1;
}

/* Dipanggil Director_Report (60 dtk). Hysteresis dua arah: butuh beberapa
   periode berturut-turut supaya lonjakan singkat tidak membuka/menutup lobby
   dan pemain tidak terlempar keluar-masuk dunia. */
stock Lobby_AutoTick()
{
    new online = Lobby_CountOnline();

    if (!Lobby_IsValid(NIGHTFALL_LOBBY_ID))
    {
        if (online < NIGHTFALL_UNLOCK_PLAYERS)
        {
            LobbyNightfallVotes = 0;
            return 0;
        }

        LobbyNightfallVotes++;
        if (LobbyNightfallVotes < NIGHTFALL_CONFIRM_TICKS) return 0;

        new reason[48];
        format(reason, sizeof(reason), "%d pemain online", online);
        return Lobby_Unlock(NIGHTFALL_LOBBY_ID, reason);
    }

    if (Lobby_CountPlayers(NIGHTFALL_LOBBY_ID, false) > 0)
    {
        LobbyNightfallIdle = 0;
        return 1;
    }

    LobbyNightfallIdle++;
    if (LobbyNightfallIdle < NIGHTFALL_IDLE_TICKS) return 1;
    if (online >= NIGHTFALL_UNLOCK_PLAYERS) return 1;

    new reason[48];
    format(reason, sizeof(reason), "kosong %d menit", NIGHTFALL_IDLE_TICKS);
    return Lobby_Lock(NIGHTFALL_LOBBY_ID, reason);
}

stock Lobby_ActiveList(lobbies[], maxLobbies){
    new count = 0;
    for (new lobbyId = 0; lobbyId < MAX_LOBBIES; lobbyId++)
    {
        if (count >= maxLobbies) break;
        if (!Lobby_IsActive(lobbyId)) continue;
        lobbies[count] = lobbyId;
        count++;
    }
    return count;
}

/* Virtual world menentukan apa yang dilihat pemain. Shelter punya world sendiri
   per lobby supaya interior base tidak bertabrakan antar dunia. */
stock Lobby_ApplyWorld(playerid)
{
    if (!IsPlayerConnected(playerid)) return 0;

    /* Jam diterapkan lebih dulu: lobby Nightfall memakai SetPlayerTime sendiri,
       termasuk saat pemain ada di shelter lobbynya. */
    Lobby_ApplyClock(playerid);

    if (PlayerSurvival[playerid][psInShelter])
    {
        new baseId = Base_Id(playerid);
        SetPlayerVirtualWorld(playerid, Shelter_VwOf(baseId));
        return 1;
    }

    SetPlayerInterior(playerid, 0);
    SetPlayerVirtualWorld(playerid, Lobby_WorldOf(playerid));
    return 1;
}

stock Lobby_TeleportToHub(playerid){
    SetPlayerPos(playerid, SPAWN_X, SPAWN_Y, SPAWN_Z);
    AC_ClearSample(playerid);
    SetPlayerFacingAngle(playerid, SPAWN_ANGLE);
    SetCameraBehindPlayer(playerid);
    return 1;
}

stock Lobby_Set(playerid, lobbyId)
{
    if (!Lobby_IsValid(lobbyId)) return 0;
    if (Lobby_Of(playerid) == lobbyId)
    {
        SendClientMessage(playerid, UI_ACCENT_WARN, "[LOBBY] Kamu sudah di lobby ini.");
        return 0;
    }
    if (!Player_IsSafe(playerid) || PlayerSurvival[playerid][psInShelter])
    {
        SendClientMessage(playerid, UI_ACCENT_WARN, "[LOBBY] Pindah lobby hanya bisa di kamp, bukan saat di shelter.");
        return 0;
    }

    PlayerLobby[playerid] = lobbyId;
    Lobby_ApplyWorld(playerid);
    Lobby_TeleportToHub(playerid);

    if (SQL_IsReady() && ActiveCharId[playerid] > 0)
    {
        new query[96];
        mysql_format(gSQL, query, sizeof(query),
            "UPDATE characters SET lobby_id = %d WHERE id = %d", lobbyId, ActiveCharId[playerid]);
        mysql_tquery(gSQL, query, "", "");
    }

    /* Base ikut pindah lobby (ROADMAP 306): kalau tidak, struktur base tetap di
       world lobby lama dan pemain kehilangan kampnya sendiri. */
    if (Base_Id(playerid) > 0 && Base_GetLobby(playerid) != lobbyId)
    {
        new baseId = Base_Id(playerid);
        BaseBuild_MigrateLobby(baseId, lobbyId);

        if (SQL_IsReady())
        {
            new query[96];
            mysql_format(gSQL, query, sizeof(query),
                "UPDATE bases SET lobby_id = %d WHERE id = %d", lobbyId, baseId);
            mysql_tquery(gSQL, query, "", "");
        }

        BaseBuild_EnsureLoaded(playerid);
        SendClientMessage(playerid, UI_TEXT_MUTED, "[BASE] Base ikut pindah ke lobby ini.");
    }

    new lobbyLabel[32], message[144];
    Lobby_GetName(lobbyId, lobbyLabel, sizeof(lobbyLabel));
    format(message, sizeof(message), "[LOBBY] Kamu masuk %s. Karakter, item, dan progres tetap sama.", lobbyLabel);
    SendClientMessage(playerid, Lobby_GetAccent(lobbyId), message);
    Toast_Show(playerid, lobbyLabel, message, Lobby_GetAccent(lobbyId));
    return 1;
}

stock Lobby_ShowSelect(playerid)
{
    if (!Account_IsPlaying(playerid)) return 0;
    if (!Player_IsSafe(playerid) || PlayerSurvival[playerid][psInShelter])
    {
        SendClientMessage(playerid, UI_ACCENT_WARN, "[LOBBY] Kembali ke kamp dulu untuk pindah lobby.");
        return 0;
    }

    new body[1024], line[192], lobbyLabel[32], tagline[96];
    new current = Lobby_Of(playerid);

    for (new lobbyId = 0; lobbyId < LobbyCount; lobbyId++)
    {
        Lobby_GetName(lobbyId, lobbyLabel, sizeof(lobbyLabel));
        Lobby_GetTagline(lobbyId, tagline, sizeof(tagline));
        format(line, sizeof(line), "%s%s  (%d pemain)\n  %s\n",
            (lobbyId == current) ? "{47C46A}* " : "  ", lobbyLabel,
            Lobby_CountPlayers(lobbyId, false), tagline);
        strcat(body, line, sizeof(body));
    }

    strcat(body, "\nPindah lobby hanya di kamp. Progres karakter tidak hilang.", sizeof(body));
    ShowPlayerDialog(playerid, DIALOG_LOBBY, DIALOG_STYLE_LIST, "Pilih lobby", body, "Masuk", "Tutup");
    LobbySelectionPending[playerid] = 1;
    return 1;
}

stock Lobby_OnDialogResponse(playerid, dialogid, response, listitem)
{
    if (dialogid != DIALOG_LOBBY) return 0;
    if (!LobbySelectionPending[playerid]) return 0;
    LobbySelectionPending[playerid] = 0;

    if (!response) return 1;
    if (listitem < 0 || listitem >= LobbyCount) return 1;

    Lobby_Set(playerid, listitem);
    return 1;
}
