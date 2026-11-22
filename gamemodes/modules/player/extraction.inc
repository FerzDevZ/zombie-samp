#if defined _ls_extraction_included
    #endinput
#endif
#define _ls_extraction_included

/* Ekstraksi berbasis sampel.

   Titik ekstraksi tidak lagi menjadi mesin uang gratis: pemain harus membawa
   minimal tiga sampel zombie yang hanya didapat dari bunuh zombie di zona
   berbahaya. Sampel tidak bisa disimpan di stash, jadi tidak bisa ditimbun.
   Cooldown per lobby mencegah dua pemain bergantian memutar payout. */

#define EXTRACTION_X 1697.0
#define EXTRACTION_Y -2349.0
#define EXTRACTION_Z 13.5
#define EXTRACTION_RANGE 5.0

#define EXTRACTION_MIN_SAMPLES 3
#define EXTRACTION_MAX_SAMPLES 8
#define EXTRACTION_COOLDOWN_MS 90000
#define EXTRACTION_PAY_PER_SAMPLE 120
#define EXTRACTION_BASE_BONUS 100
#define EXTRACTION_BASE_HEAL 20.0
#define EXTRACTION_HEAL_PER_SAMPLE 6.0
#define EXTRACTION_BASE_CURE 10.0
#define EXTRACTION_CURE_PER_SAMPLE 4.0

new ExtractionReadyTick[MAX_LOBBIES];
/* Cooldown per-PEMAIN (selain per-lobby): titik ekstraksi berada di dalam
   radius hub, jadi tanpa ini satu pemain bisa memutar payout sendirian
   tanpa risiko (audit F1). */
new ExtractionPlayerReady[MAX_PLAYERS];

stock Extraction_IsAtPoint(playerid)
{
    new Float:x, Float:y, Float:z;
    GetPlayerPos(playerid, x, y, z);
    return VectorSize(x - EXTRACTION_X, y - EXTRACTION_Y, z - EXTRACTION_Z) <= EXTRACTION_RANGE;
}

stock Extraction_CooldownSeconds(playerid)
{
    new lobbyId = Lobby_Of(playerid);
    if (lobbyId < 0 || lobbyId >= MAX_LOBBIES) return 0;

    new remaining = ExtractionReadyTick[lobbyId] - GetTickCount();
    if (remaining <= 0) return 0;
    return (remaining / 1000) + 1;
}

stock Extraction_Try(playerid)
{
    if (!Account_IsPlaying(playerid)) return 0;
    if (PlayerSurvival[playerid][psDowned]) return 0;

    if (!Extraction_IsAtPoint(playerid))
    {
        SendClientMessage(playerid, UI_TEXT_MUTED, "[EKSTRAKSI] Berdiri di titik ekstraksi untuk memanggil heli.");
        return 0;
    }

    new cooldown = Extraction_CooldownSeconds(playerid);
    if (cooldown > 0)
    {
        new line[96];
        format(line, sizeof(line), "[EKSTRAKSI] Titik sedang dipakai. Coba lagi dalam %d detik.", cooldown);
        SendClientMessage(playerid, UI_ACCENT_WARN, line);
        return 0;
    }

    new playerRemaining = ExtractionPlayerReady[playerid] - GetTickCount();
    if (playerRemaining > 0)
    {
        new line[96];
        format(line, sizeof(line), "[EKSTRAKSI] Heli sedang sibuk. Coba lagi dalam %d detik.", (playerRemaining / 1000) + 1);
        SendClientMessage(playerid, UI_ACCENT_WARN, line);
        return 0;
    }

    new carried = Inventory_CountItem(playerid, ITEM_SPECIMEN);
    if (carried < EXTRACTION_MIN_SAMPLES)
    {
        new line[128];
        format(line, sizeof(line),
            "[EKSTRAKSI] Butuh minimal %d sampel zombie. Anda membawa %d.",
            EXTRACTION_MIN_SAMPLES, carried);
        SendClientMessage(playerid, UI_ACCENT_WARN, line);
        return 0;
    }

    new accepted = carried;
    if (accepted > EXTRACTION_MAX_SAMPLES) accepted = EXTRACTION_MAX_SAMPLES;

    if (!Inventory_Consume(playerid, ITEM_SPECIMEN, accepted, "extract"))
    {
        SendClientMessage(playerid, UI_ACCENT_DANGER, "[EKSTRAKSI] Sampel tidak bisa diserahkan.");
        return 0;
    }

    new payout = accepted * EXTRACTION_PAY_PER_SAMPLE + EXTRACTION_BASE_BONUS;
    new Float:heal = EXTRACTION_BASE_HEAL + float(accepted) * EXTRACTION_HEAL_PER_SAMPLE;
    new Float:cure = EXTRACTION_BASE_CURE + float(accepted) * EXTRACTION_CURE_PER_SAMPLE;

    PlayerSurvival[playerid][psCash] += payout;
    PlayerSurvival[playerid][psExtractions]++;
    RankPoints[playerid] += 10;
    Player_Heal(playerid, heal);
    Player_CureInfection(playerid, cure);

    /* Heli ekstraksi berisik: panas naik di zona titik ekstraksi sebelum teleport. */
    Horde_AddHeat(EXTRACTION_X, EXTRACTION_Y, EXTRACTION_Z, 14.0);

    new lobbyId = Lobby_Of(playerid);
    if (lobbyId >= 0 && lobbyId < MAX_LOBBIES)
        ExtractionReadyTick[lobbyId] = GetTickCount() + EXTRACTION_COOLDOWN_MS;
    ExtractionPlayerReady[playerid] = GetTickCount() + EXTRACTION_COOLDOWN_MS;

    new line[160];
    format(line, sizeof(line), "[EKSTRAKSI] %d sampel diserahkan. +$%d, +%.0f HP, +%.0f cure.",
        accepted, payout, heal, cure);
    SendClientMessage(playerid, UI_ACCENT_PRIMARY, line);
    Toast_Show(playerid, "Ekstraksi berhasil", line, UI_ACCENT_PRIMARY);

    Log_Format(LOG_INFO, "[EXTRACT] char=%d sampel=%d payout=%d lobby=%d",
        ActiveCharId[playerid], accepted, payout, lobbyId);
    Tutorial_OnExtracted(playerid);

    SetPlayerPos(playerid, SPAWN_X, SPAWN_Y, SPAWN_Z);
    AC_ClearSample(playerid);
    SetCameraBehindPlayer(playerid);
    return 1;
}
