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

stock Lobby_ActiveList(lobbies[], maxLobbies)
{
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

stock Lobby_TeleportToHub(playerid)
{
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
