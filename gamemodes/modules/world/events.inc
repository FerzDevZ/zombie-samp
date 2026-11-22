#if defined _ls_events_included
    #endinput
#endif
#define _ls_events_included

/* Event sosial: satu event tiap 5 menit, diumumkan ke semua orang, dan efeknya
   memakai sistem yang sudah ada (loot, director, noise). Tidak ada event yang
   memberi item gratis tanpa risiko.

   Jadwal memakai gettime() (epoch detik), BUKAN GetTickCount(): tick ms wrap
   setelah ~24,8 hari dan bisa membuat event berhenti atau double-trigger. */

#define EVENT_INTERVAL_S    300
#define EVENT_BLOOD_MOON_S  180
#define EVENT_QUIET_S       120
#define EVENT_SUPPLY_CRATES 3

forward Event_Tick();

new EventNextTick = 0;
new EventBloodMoonUntil = 0;
new EventQuietUntil = 0;
new EventLastId = -1;

stock Event_IsBloodMoon()
{
    return (EventBloodMoonUntil != 0 && gettime() < EventBloodMoonUntil) ? 1 : 0;
}

stock Float:Event_NoiseMultiplier()
{
    if (EventQuietUntil != 0 && gettime() < EventQuietUntil) return 0.5;
    if (Event_IsBloodMoon()) return 1.25;
    return 1.0;
}

stock Event_BloodMoon()
{
    EventBloodMoonUntil = gettime() + EVENT_BLOOD_MOON_S;

    /* Per lobby: zona terpadat DI LOBBY ITU yang diserang. Versi lama
       menghitung pemain lintas lobby lalu menembak horde ke semua dunia —
       lobby sepi bisa kena horde untuk pemain di lobby lain. */
    new totalTargeted = 0;
    for (new lobbyId = 0; lobbyId < LobbyCount; lobbyId++)
    {
        if (Lobby_CountPlayers(lobbyId, false) == 0) continue;

        new bestZone = -1, bestCount = 0;
        for (new zone = 0; zone < MAX_ZONE_COUNT; zone++)
        {
            if (Zone_IsHub(zone)) continue;

            new count = 0;
            foreach (new playerid : PlayerPool)
            {
                if (Lobby_Of(playerid) != lobbyId) continue;
                if (PlayerSurvival[playerid][psZone] == zone) count++;
            }
            if (count > bestCount)
            {
                bestCount = count;
                bestZone = zone;
            }
        }
        if (bestZone < 0 || bestCount == 0) continue;

        Director_TriggerHorde(bestZone, 12, lobbyId);
        totalTargeted += bestCount;

        new zoneLabel[MAX_ZONE_NAME], message[192];
        Zone_GetName(bestZone, zoneLabel, sizeof(zoneLabel));
        format(message, sizeof(message), "Blood Moon: horde mengejar %d pemain di %s. Noise +25%%.",
            bestCount, zoneLabel);
        Director_NotifyLobby(lobbyId, "BLOOD MOON", message, UI_ACCENT_DANGER);
    }

    if (totalTargeted == 0)
    {
        Toast_NotifyAll("BLOOD MOON", "Tidak ada yang di luar. Bulan tetap terbit.", UI_ACCENT_DANGER);
    }
    return 1;
}

stock Event_QuietHour()
{
    EventQuietUntil = gettime() + EVENT_QUIET_S;
    Toast_NotifyAll("JAM TENANG", "Radius noise -50% selama 2 menit. Waktu untuk bergerak.", UI_ACCENT_PRIMARY);
    return 1;
}

stock Event_SupplyDrop()
{
    new spawnedCrates = 0;
    new dropItem[3] = {ITEM_MEDKIT, ITEM_RIFLE_AMMO, ITEM_BANDAGE};
    new rolled = random(3);

    foreach (new playerid : PlayerPool)
    {
        if (spawnedCrates >= EVENT_SUPPLY_CRATES) break;
        if (!Player_IsAlive(playerid)) continue;
        if (Player_IsSafe(playerid)) continue;   // drop hanya di zona berbahaya

        new Float:x, Float:y, Float:z;
        GetPlayerPos(playerid, x, y, z);

        new Float:spreadX = x + float(random(14) - 7);
        new Float:spreadY = y + float(random(14) - 7);

        /* Peti wajib mendarat di tanah: tanpa raycast peti bisa melayang di
           atas jembatan atau tenggelam di bawah terrain. */
        new Float:groundX, Float:groundY, Float:groundZ;
        new Float:dropZ = z;
        if (CA_RayCastLine(spreadX, spreadY, z + 60.0, spreadX, spreadY, z - 60.0,
            groundX, groundY, groundZ) != 0)
        {
            dropZ = groundZ + 0.2;
        }

        Loot_Spawn(spreadX, spreadY, dropZ, dropItem[rolled], 1 + random(2), INVALID_PLAYER_ID, Lobby_Of(playerid));
        spawnedCrates++;
    }

    if (spawnedCrates == 0)
    {
        Toast_NotifyAll("SUPPLY DROP", "Pesawat lewat tanpa menjatuhkan apa pun. Tidak ada yang di luar.", UI_TEXT_MUTED);
        return 0;
    }

    new message[128];
    format(message, sizeof(message), "Supply drop: %d peti jatuh di sekitar pemain yang sedang di luar.", spawnedCrates);
    Toast_NotifyAll("SUPPLY DROP", message, UI_ACCENT_PRIMARY);
    return 1;
}

stock Event_Run(eventId)
{
    switch (eventId)
    {
        case 0: return Event_SupplyDrop();
        case 1: return Event_BloodMoon();
        case 2: return Event_QuietHour();
    }
    return 0;
}

stock Event_Force(playerid)
{
    new eventId = random(3);
    EventLastId = eventId;
    EventNextTick = gettime() + EVENT_INTERVAL_S;

    new message[96];
    format(message, sizeof(message), "[EVENT] Dipaksa oleh admin. Event id %d.", eventId);
    SendClientMessage(playerid, UI_TEXT_MUTED, message);

    return Event_Run(eventId);
}

public Event_Tick()
{
    new perfStart = GetTickCount();
    new now = gettime();
    if (EventNextTick == 0)
    {
        EventNextTick = now + EVENT_INTERVAL_S;
        return 1;
    }
    if (now < EventNextTick) return 1;

    EventNextTick = now + EVENT_INTERVAL_S;

    new eventId = random(3);
    if (eventId == EventLastId) eventId = (eventId + 1) % 3;   // jangan dua kali sama berturut-turut
    EventLastId = eventId;

    new eventResult = Event_Run(eventId);
    Perf_Update(PERF_EVENT, perfStart);
    return eventResult;
}
