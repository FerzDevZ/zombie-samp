#if defined _ls_toast_included
    #endinput
#endif
#define _ls_toast_included

/* MVP toast: one-line chat feedback. Replaced by the Dark Slate Glass TextDraw
   notification stack once the HUD module lands in Phase 3. */
stock Toast_Show(playerid, const title[], const body[], color)
{
    if (!IsPlayerConnected(playerid)) return 0;

    new line[192];
    format(line, sizeof(line), "[%s] %s", title, body);
    SendClientMessage(playerid, color, line);
    return 1;
}

stock Toast_NotifyAll(const title[], const body[], color)
{
    new line[192];
    format(line, sizeof(line), "[%s] %s", title, body);
    SendClientMessageToAll(color, line);
    return 1;
}
