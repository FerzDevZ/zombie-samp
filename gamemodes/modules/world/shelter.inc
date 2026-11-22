#if defined _ls_shelter_included
    #endinput
#endif
#define _ls_shelter_included

/* Shelter pribadi: ruang custom 12x9 m dibangun dari objek dinamis di VW unik
   per base (interior 0, langit di atas LS). Zombie tidak bisa masuk; semua
   akses aman (stash/craft/pedagang) memakai Player_IsSafe di state.inc.
   Ruang dibangun per kunjungan dan dihancurkan saat keluar agar tidak bocor. */

#define SHELTER_INTERIOR    0
#define SHELTER_X           2496.0
#define SHELTER_Y           -1692.0
#define SHELTER_Z           1014.74
#define SHELTER_ANGLE       180.0
#define SHELTER_VW_BASE     1000

/* Ruang shelter didefinisikan di mapping.inc (include setelah file ini). */
forward ShelterRoom_Create(playerid);
forward ShelterRoom_Destroy(playerid);

/* VW shelter unik per base: baseId adalah id DB global, jadi tidak ada dua
   base berbagi dunia (formula lama baseId%400 bisa kolisi dalam satu lobby). */
stock Shelter_VwOf(baseId)
{
    return SHELTER_VW_BASE + baseId;
}

stock Shelter_Toggle(playerid)
{
    if (!IsPlayerConnected(playerid)) return 0;
    if (PlayerSurvival[playerid][psInShelter]) return Shelter_Exit(playerid);
    return Shelter_Enter(playerid);
}

stock Shelter_Enter(playerid)
{
    if (Base_Id(playerid) == 0)
    {
        Base_Load(playerid);
        SendClientMessage(playerid, UI_TEXT_MUTED, "[SHELTER] Menyiapkan base Anda, coba /shelter lagi.");
        return 0;
    }

    new Float:x, Float:y, Float:z, Float:angle;
    GetPlayerPos(playerid, x, y, z);
    GetPlayerFacingAngle(playerid, angle);

    PlayerSurvival[playerid][psShelterX] = x;
    PlayerSurvival[playerid][psShelterY] = y;
    PlayerSurvival[playerid][psShelterZ] = z;
    PlayerSurvival[playerid][psShelterA] = angle;
    PlayerSurvival[playerid][psInShelter] = 1;

    /* Ruang dibangun SEBELUM teleport supaya Streamer_UpdateEx memuatnya. */
    ShelterRoom_Create(playerid);
    Map_TeleportEx(playerid,
        Shelter_VwOf(Base_Id(playerid)),
        SHELTER_INTERIOR, SHELTER_X, SHELTER_Y, SHELTER_Z, SHELTER_ANGLE);
    AC_ClearSample(playerid);
    ShelterDoor_Create(playerid);

    Toast_Show(playerid, "SHELTER", "Aman di sini. /stash /craft /shop; keluar lewat pintu selatan atau /shelter.", UI_ACCENT_PRIMARY);
    SendClientMessage(playerid, UI_TEXT_SECONDARY, "[SHELTER] Zombie tidak bisa masuk. Barricade tetap bekerja di luar.");
    return 1;
}

stock Shelter_Exit(playerid)
{
    if (!PlayerSurvival[playerid][psInShelter]) return 0;

    PlayerSurvival[playerid][psInShelter] = 0;
    ShelterDoor_Destroy(playerid);
    ShelterRoom_Destroy(playerid);
    Lobby_ApplyWorld(playerid);
    SetPlayerInterior(playerid, 0);

    new Float:x = PlayerSurvival[playerid][psShelterX];
    new Float:y = PlayerSurvival[playerid][psShelterY];
    new Float:z = PlayerSurvival[playerid][psShelterZ];

    if (x == 0.0 && y == 0.0)
    {
        x = SPAWN_X;
        y = SPAWN_Y;
        z = SPAWN_Z;
    }

    SetPlayerPos(playerid, x, y, z);
    AC_ClearSample(playerid);
    SetPlayerFacingAngle(playerid, PlayerSurvival[playerid][psShelterA]);
    SetCameraBehindPlayer(playerid);

    Toast_Show(playerid, "KELUAR SHELTER", "Kembali ke jalanan. Hati-hati.", UI_TEXT_SECONDARY);
    return 1;
}

/* Dipanggil saat pemain disconnect/mati: shelter tidak boleh menyandera state. */
stock Shelter_Reset(playerid)
{
    ShelterDoor_Destroy(playerid);
    ShelterRoom_Destroy(playerid);
    PlayerSurvival[playerid][psInShelter] = 0;
    PlayerSurvival[playerid][psShelterX] = 0.0;
    PlayerSurvival[playerid][psShelterY] = 0.0;
    PlayerSurvival[playerid][psShelterZ] = 0.0;
    return 1;
}
