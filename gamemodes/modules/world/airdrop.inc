#if defined _ls_airdrop_included
    #endinput
#endif
#define _ls_airdrop_included

/* Airdrop militer berkala: pesawat kargo lewat, menjatuhkan peti suplai
   bersuar asap merah di lokasi acak di zona berbahaya. Peti jadi umpan PvP
   sekaligus menarik gerombolan zombie ke titik jatuh.

   Mesin fase berbasis gettime() (bukan GetTickCount — wrap 24,8 hari):
   IDLE -> INBOUND (pesawat lewat) -> FALLING (peti + parasut turun)
        -> LANDED (asap merah + loot + horde) -> cleanup -> IDLE.

   Satu airdrop aktif untuk seluruh server: satu titik fokus pertempuran,
   bukan hujan peti yang memecah pemain. */

#define AIRDROP_INTERVAL_S      600
#define AIRDROP_INBOUND_S       45
#define AIRDROP_FALL_S          8
#define AIRDROP_LINGER_S        240
#define AIRDROP_LOOT_COUNT      5
#define AIRDROP_ALTITUDE        120.0
#define AIRDROP_HORDE_COUNT     12
#define AIRDROP_NOISE_RADIUS    120.0

#define AIRDROP_STATE_IDLE      0
#define AIRDROP_STATE_INBOUND   1
#define AIRDROP_STATE_FALLING   2
#define AIRDROP_STATE_LANDED    3

enum E_AIRDROP
{
    aState,
    aZone,
    aLobby,
    Float:aX,
    Float:aY,
    Float:aZ,
    aPhaseUntil,        // epoch batas fase INBOUND/FALLING
    aExpireAt,          // epoch cleanup LANDED
    STREAMER_TAG_OBJECT:aPlane,
    STREAMER_TAG_OBJECT:aCrate,
    STREAMER_TAG_OBJECT:aChute,
    STREAMER_TAG_OBJECT:aSmoke,
    STREAMER_TAG_MAP_ICON:aIcon
}

new gAirdrop[E_AIRDROP];
new AirdropNextAt = 0;

forward Airdrop_Tick();

/* Posisi airdrop untuk penunjuk arah (/gps). Hanya sah saat fase aktif:
   sesudah LANDED + cleanup, koordinat lama tidak boleh dikejar pemain. */
stock bool:Airdrop_GetPos(&Float:x, &Float:y, &Float:z)
{
    if (gAirdrop[aState] == AIRDROP_STATE_IDLE) return false;
    x = gAirdrop[aX];
    y = gAirdrop[aY];
    z = gAirdrop[aZ];
    return true;
}

stock Airdrop_PickZone()
{
    /* Kumpulkan kandidat non-hub dulu, lalu pilih acak — random bias atas
       indeks membuat zona awal lebih sering kena. */
    new candidates[MAX_ZONE_COUNT], count = 0;
    for (new zoneid = 0; zoneid < MAX_ZONE_COUNT; zoneid++)
    {
        if (gZones[zoneid][zoneRadius] <= 0.0) continue;
        if (gZones[zoneid][zoneIsHub]) continue;
        candidates[count++] = zoneid;
    }
    if (count == 0) return -1;
    return candidates[random(count)];
}

stock Airdrop_PhaseName(airdropState, dest[], size)
{
    switch (airdropState)
    {
        case AIRDROP_STATE_IDLE:    format(dest, size, "IDLE");
        case AIRDROP_STATE_INBOUND: format(dest, size, "INBOUND");
        case AIRDROP_STATE_FALLING: format(dest, size, "FALLING");
        case AIRDROP_STATE_LANDED:  format(dest, size, "LANDED");
        default: format(dest, size, "?");
    }
    return 1;
}

stock Airdrop_PickLobby()
{
    /* Lobby dengan pemain terbanyak menang: airdrop di lobby kosong hanya
       memboros objek dan tak pernah diperebutkan. */
    new bestLobby = -1, bestCount = 0;
    for (new lobbyId = 0; lobbyId < LobbyCount; lobbyId++)
    {
        new count = Lobby_CountPlayers(lobbyId, true);
        if (count > bestCount)
        {
            bestCount = count;
            bestLobby = lobbyId;
        }
    }
    return (bestCount > 0) ? bestLobby : -1;
}

stock Airdrop_DestroyObjects()
{
    if (gAirdrop[aPlane] != STREAMER_TAG_OBJECT:0) { DestroyDynamicObject(gAirdrop[aPlane]); gAirdrop[aPlane] = STREAMER_TAG_OBJECT:0; }
    if (gAirdrop[aCrate] != STREAMER_TAG_OBJECT:0) { DestroyDynamicObject(gAirdrop[aCrate]); gAirdrop[aCrate] = STREAMER_TAG_OBJECT:0; }
    if (gAirdrop[aChute] != STREAMER_TAG_OBJECT:0) { DestroyDynamicObject(gAirdrop[aChute]); gAirdrop[aChute] = STREAMER_TAG_OBJECT:0; }
    if (gAirdrop[aSmoke] != STREAMER_TAG_OBJECT:0) { DestroyDynamicObject(gAirdrop[aSmoke]); gAirdrop[aSmoke] = STREAMER_TAG_OBJECT:0; }
    if (gAirdrop[aIcon] != STREAMER_TAG_MAP_ICON:0) { DestroyDynamicMapIcon(gAirdrop[aIcon]); gAirdrop[aIcon] = STREAMER_TAG_MAP_ICON:0; }
    return 1;
}

stock Airdrop_Start()
{
    new zone = Airdrop_PickZone();
    new lobbyId = Airdrop_PickLobby();
    if (zone < 0 || lobbyId < 0)
    {
        AirdropNextAt = gettime() + AIRDROP_INTERVAL_S;
        return 0;
    }

    /* Titik jatuh acak di dalam 60% radius zona: tidak pernah persis di
       tengah (mudah ditebak), tidak pernah di luar zona. */
    new Float:angle = float(random(360)) * 0.0174532925;
    new Float:dist = gZones[zone][zoneRadius] * 0.6 * float(random(100)) / 100.0;
    new Float:x = gZones[zone][zoneX] + dist * floatcos(angle, degrees);
    new Float:y = gZones[zone][zoneY] + dist * floatsin(angle, degrees);

    /* Peti wajib mendarat di tanah — pola sama dengan Event_SupplyDrop. */
    new Float:groundX, Float:groundY, Float:groundZ;
    new Float:z = gZones[zone][zoneZ];
    if (CA_RayCastLine(x, y, z + AIRDROP_ALTITUDE, x, y, z - 40.0,
        groundX, groundY, groundZ) != 0)
    {
        z = groundZ + 0.2;
    }

    gAirdrop[aState] = AIRDROP_STATE_INBOUND;
    gAirdrop[aZone] = zone;
    gAirdrop[aLobby] = lobbyId;
    gAirdrop[aX] = x;
    gAirdrop[aY] = y;
    gAirdrop[aZ] = z;
    gAirdrop[aPhaseUntil] = gettime() + AIRDROP_INBOUND_S;
    gAirdrop[aExpireAt] = 0;

    new worldId = Lobby_WorldById(lobbyId);

    /* Pesawat kargo melintas: dari luar zona menuju titik jatuh, terus ke
       seberang. Kecepatan 26.7 m/s = 800 m dalam 30 detik < INBOUND_S. */
    new Float:planeX = x - 400.0, Float:planeY = y - 400.0;
    gAirdrop[aPlane] = CreateDynamicObject(592, planeX, planeY, z + AIRDROP_ALTITUDE,
        0.0, 0.0, 45.0, worldId);
    MoveDynamicObject(gAirdrop[aPlane], x + 400.0, y + 400.0, z + AIRDROP_ALTITUDE, 26.7);

    new zoneLabel[MAX_ZONE_NAME], message[160];
    Zone_GetName(zone, zoneLabel, sizeof(zoneLabel));
    format(message, sizeof(message),
        "Pesawat kargo terdeteksi. Peti suplai jatuh di %s dalam %d detik. Gerombolan akan datang.",
        zoneLabel, AIRDROP_INBOUND_S);
    Director_NotifyLobby(lobbyId, "AIRDROP MILITER", message, UI_ACCENT_WARN);
    Log_Format(LOG_INFO, "[AIRDROP] mulai: zone=%d lobby=%d di %.1f,%.1f,%.1f", zone, lobbyId, x, y, z);
    return 1;
}

stock Airdrop_BeginFalling()
{
    new worldId = Lobby_WorldById(gAirdrop[aLobby]);

    if (gAirdrop[aPlane] != STREAMER_TAG_OBJECT:0)
    {
        DestroyDynamicObject(gAirdrop[aPlane]);
        gAirdrop[aPlane] = STREAMER_TAG_OBJECT:0;
    }

    gAirdrop[aCrate] = CreateDynamicObject(2932, gAirdrop[aX], gAirdrop[aY],
        gAirdrop[aZ] + AIRDROP_ALTITUDE, 0.0, 0.0, 0.0, worldId);
    gAirdrop[aChute] = CreateDynamicObject(371, gAirdrop[aX], gAirdrop[aY],
        gAirdrop[aZ] + AIRDROP_ALTITUDE + 3.0, 0.0, 0.0, 0.0, worldId);
    MoveDynamicObject(gAirdrop[aCrate], gAirdrop[aX], gAirdrop[aY], gAirdrop[aZ],
        AIRDROP_ALTITUDE / float(AIRDROP_FALL_S));
    MoveDynamicObject(gAirdrop[aChute], gAirdrop[aX], gAirdrop[aY],
        gAirdrop[aZ] + 3.0, AIRDROP_ALTITUDE / float(AIRDROP_FALL_S));

    gAirdrop[aState] = AIRDROP_STATE_FALLING;
    gAirdrop[aPhaseUntil] = gettime() + AIRDROP_FALL_S;
    return 1;
}

stock Airdrop_Land()
{
    new worldId = Lobby_WorldById(gAirdrop[aLobby]);

    if (gAirdrop[aChute] != STREAMER_TAG_OBJECT:0)
    {
        DestroyDynamicObject(gAirdrop[aChute]);
        gAirdrop[aChute] = STREAMER_TAG_OBJECT:0;
    }

    /* Asap merah + ikon peta: dua penunjuk, satu untuk mata satu untuk map. */
    gAirdrop[aSmoke] = CreateDynamicObject(354, gAirdrop[aX], gAirdrop[aY],
        gAirdrop[aZ] + 1.0, 0.0, 0.0, 0.0, worldId);
    gAirdrop[aIcon] = CreateDynamicMapIcon(gAirdrop[aX], gAirdrop[aY], gAirdrop[aZ],
        0, 0xFF4C3CFF, worldId);

    /* Suplai militer tersebar di sekitar peti: petinya penanda, lootnya
       di tanah — tidak perlu sistem interaksi baru. */
    new lootItems[AIRDROP_LOOT_COUNT] = {ITEM_RIFLE_AMMO, ITEM_RIFLE_AMMO, ITEM_MEDKIT, ITEM_BANDAGE, ITEM_ANTIBIOTICS};
    for (new i = 0; i < AIRDROP_LOOT_COUNT; i++)
    {
        new Float:sx = gAirdrop[aX] + float(random(9) - 4);
        new Float:sy = gAirdrop[aY] + float(random(9) - 4);
        Loot_Spawn(sx, sy, gAirdrop[aZ], lootItems[i], (lootItems[i] == ITEM_RIFLE_AMMO) ? 30 : 1,
            INVALID_PLAYER_ID, gAirdrop[aLobby]);
    }

    /* Dua sinyal ke gerombolan: horde terjadwal ke zona + noise besar dari
       titik jatuh untuk zombie yang sudah dekat. */
    Director_TriggerHorde(gAirdrop[aZone], AIRDROP_HORDE_COUNT, gAirdrop[aLobby]);
    Noise_EmitAtPosition(gAirdrop[aX], gAirdrop[aY], gAirdrop[aZ], AIRDROP_NOISE_RADIUS, gAirdrop[aLobby]);

    gAirdrop[aState] = AIRDROP_STATE_LANDED;
    gAirdrop[aExpireAt] = gettime() + AIRDROP_LINGER_S;

    new zoneLabel[MAX_ZONE_NAME], message[128];
    Zone_GetName(gAirdrop[aZone], zoneLabel, sizeof(zoneLabel));
    format(message, sizeof(message), "Peti mendarat di %s. Asap merah menyala — rebutan sebelum zombie datang.",
        zoneLabel);
    Director_NotifyLobby(gAirdrop[aLobby], "AIRDROP MILITER", message, UI_ACCENT_DANGER);
    Log_Format(LOG_INFO, "[AIRDROP] mendarat: loot %d item, horde %d zombie.", AIRDROP_LOOT_COUNT, AIRDROP_HORDE_COUNT);
    return 1;
}

stock Airdrop_Cleanup(const reason[])
{
    Airdrop_DestroyObjects();
    gAirdrop[aState] = AIRDROP_STATE_IDLE;
    AirdropNextAt = gettime() + AIRDROP_INTERVAL_S;
    Log_Format(LOG_INFO, "[AIRDROP] selesai: %s", reason);
    return 1;
}

public Airdrop_Tick()
{
    if (AirdropNextAt == 0) AirdropNextAt = gettime() + AIRDROP_INTERVAL_S;

    switch (gAirdrop[aState])
    {
        case AIRDROP_STATE_IDLE:
        {
            if (gettime() >= AirdropNextAt) Airdrop_Start();
        }
        case AIRDROP_STATE_INBOUND:
        {
            if (gettime() >= gAirdrop[aPhaseUntil]) Airdrop_BeginFalling();
        }
        case AIRDROP_STATE_FALLING:
        {
            if (gettime() >= gAirdrop[aPhaseUntil]) Airdrop_Land();
        }
        case AIRDROP_STATE_LANDED:
        {
            if (gettime() >= gAirdrop[aExpireAt]) Airdrop_Cleanup("asap padam, peti dianggap habis");
        }
    }
    return 1;
}

stock Airdrop_Force(playerid)
{
    if (gAirdrop[aState] != AIRDROP_STATE_IDLE)
    {
        SendClientMessage(playerid, UI_ACCENT_WARN, "[AIRDROP] Masih ada airdrop berjalan.");
        return 0;
    }
    AirdropNextAt = gettime();
    SendClientMessage(playerid, UI_ACCENT_PRIMARY, "[AIRDROP] Dipaksa oleh admin — pesawat dikirim.");
    return Airdrop_Start();
}

stock Airdrop_Selftest()
{
    /* Zona acak tidak boleh pernah hub, dan nama fase harus konsisten. */
    new bool:ok = true;
    for (new i = 0; i < 30; i++)
    {
        new zone = Airdrop_PickZone();
        if (zone < 0 || Zone_IsHub(zone)) { ok = false; break; }
    }

    new phaseName[12];
    Airdrop_PhaseName(AIRDROP_STATE_LANDED, phaseName, sizeof(phaseName));
    ok = ok && strcmp(phaseName, "LANDED") == 0;

    new bool:passed = ok;
    Selftest_Record(passed,
        "airdrop militer", ok ? "zona acak bebas hub + fase konsisten" : "pemilihan zona/fase salah");
    return 1;
}
