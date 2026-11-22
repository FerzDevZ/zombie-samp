#if defined _ls_mapping_included
    #endinput
#endif
#define _ls_mapping_included

/* Mapping modern untuk kamp dan shelter: teleport anti-jatuh-tembus (preload
   streamer), cap objek klien, retexture + signage kamp, actor pedagang, dan
   pintu fisik shelter. Semua koordinat dekorasi terpusat di sini supaya bisa
   dituning hasil playtest tanpa menyentuh logika gameplay. */

#define MAP_VISIBLE_OBJECTS     920     // sisakan headroom 80 slot untuk prop lampiran
#define MAP_PRELOAD_MS          600     // waktu stream sebelum gravitasi aktif

/* Kamp: baris hotspot stash/craft/shop menghadap selatan (y lebih kecil). */
#define CAMP_WALL_Y             -2326.5
#define CAMP_WALL_Z             13.9
#define CAMP_SIGN_X             1676.0
#define CAMP_SIGN_Y             -2326.0
#define CAMP_SIGN_Z             14.4
#define CAMP_BARRIER_Z          12.9

/* Pintu shelter (interior custom 0): panel di dinding selatan ruang shelter.
   Auto-buka saat pemain dekat; melangkah keluar pintu = kembali ke dunia. */
#define SHELTER_DOOR_MODEL      1495
#define SHELTER_DOOR_X          2496.8
#define SHELTER_DOOR_Y          -1687.5
#define SHELTER_DOOR_Z          1014.1
#define SHELTER_DOOR_RZ_CLOSED  0.0
#define SHELTER_DOOR_RZ_OPEN    95.0
#define SHELTER_DOOR_SPEED      1.6
#define SHELTER_DOOR_OPEN_DIST  2.5
#define SHELTER_DOOR_CLOSE_DIST 4.5

/* Ruang shelter custom: 12x9 m di interior 0 (langit di atas LS), dibangun
   per kunjungan di VW base. Model 19447 = panel dinding 3 m yang sudah
   terverifikasi di kamp (diputar rx=90 untuk lantai/plafon). */
#define SHELTER_ROOM_FLOOR_Z    1014.0
#define SHELTER_ROOM_WALL_Z     1015.6
#define SHELTER_ROOM_CEIL_Z     1017.3
#define SHELTER_ROOM_MIN_X      2490.0
#define SHELTER_ROOM_MAX_X      2502.0
#define SHELTER_ROOM_MIN_Y      -1696.5
#define SHELTER_ROOM_MAX_Y      -1687.5
#define SHELTER_EXIT_Y          -1686.6
#define SHELTER_FALL_Z          1012.0
#define SHELTER_ROOM_MAX_OBJS   48

new MapObjectCount = 0;
new MapActorCount = 0;

new STREAMER_TAG_OBJECT:ShelterDoorObj[MAX_PLAYERS];
new bool:ShelterDoorOpen[MAX_PLAYERS];
new STREAMER_TAG_OBJECT:ShelterRoomObjs[MAX_PLAYERS][SHELTER_ROOM_MAX_OBJS];
new ShelterRoomCount[MAX_PLAYERS];

/* ---- Material presets (ARGB, bukan RGBA) ---- */

stock Map_ApplyMaterial(STREAMER_TAG_OBJECT:obj, materialIndex, const preset[])
{
    if (!strcmp(preset, "matte_slate", true))
        SetDynamicObjectMaterial(obj, materialIndex, 19447, "wall_textures", "concrete_01", 0xFF141923);
    else if (!strcmp(preset, "dark_marble", true))
        SetDynamicObjectMaterial(obj, materialIndex, 16150, "ufo_bar", "marble_01", 0xFF2B313C);
    else if (!strcmp(preset, "brushed_metal", true))
        SetDynamicObjectMaterial(obj, materialIndex, 19353, "metal_tex", "metal_dark", 0xFF3A4252);
    else if (!strcmp(preset, "warm_oak", true))
        SetDynamicObjectMaterial(obj, materialIndex, 14826, "int_kitch", "cj_wood5", 0xFFD9C2A3);
    return 1;
}

stock Map_ApplySignage(STREAMER_TAG_OBJECT:obj, const text[])
{
    SetDynamicObjectMaterialText(obj, 0, text,
        OBJECT_MATERIAL_SIZE_512x128,
        "Segoe UI", 48, 1,
        0xFF10B981,          // emerald: warna "zona aman"
        0x88000000,          // latar hitam semi transparan
        OBJECT_MATERIAL_TEXT_ALIGN_CENTER);
    return 1;
}

/* ---- Teleport aman: stream objek tujuan SEBELUM gravitasi jalan ---- */

stock Map_TeleportEx(playerid, worldid, interiorid, Float:x, Float:y, Float:z, Float:angle)
{
    SetPlayerVirtualWorld(playerid, worldid);
    SetPlayerInterior(playerid, interiorid);

    // freezeplayer = 1: pemain dibekukan selama objek sekitar tujuan di-stream.
    Streamer_UpdateEx(playerid, x, y, z, worldid, interiorid, STREAMER_TYPE_OBJECT, MAP_PRELOAD_MS, 1);

    SetPlayerPos(playerid, x, y, z + 0.15);
    SetPlayerFacingAngle(playerid, angle);
    SetCameraBehindPlayer(playerid);
    return 1;
}

/* ---- Kamp: dinding latar, papan neon, barikade, actor pedagang ---- */

stock Map_BuildCamp()
{
    new STREAMER_TAG_OBJECT:obj;

    // Latar beton gelap di belakang baris hotspot (5 segmen dinding 3 m).
    for (new segment = 0; segment < 5; segment++)
    {
        obj = CreateDynamicObject(19447,
            1659.0 + (segment * 3.0), CAMP_WALL_Y, CAMP_WALL_Z,
            0.0, 0.0, 0.0);
        Map_ApplyMaterial(obj, 0, "matte_slate");
        MapObjectCount++;
    }

    // Papan neon "SAFE ZONE" di sisi timur baris hotspot.
    obj = CreateDynamicObject(19477, CAMP_SIGN_X, CAMP_SIGN_Y, CAMP_SIGN_Z, 0.0, 0.0, 270.0);
    Map_ApplySignage(obj, "SAFE ZONE");
    MapObjectCount++;

    // Barikade jalan di mulut kamp (jalan masuk dari selatan).
    CreateDynamicObject(1427, 1683.0, -2336.0, CAMP_BARRIER_Z, 0.0, 0.0, 0.0);
    CreateDynamicObject(1427, 1687.0, -2336.0, CAMP_BARRIER_Z, 0.0, 0.0, 0.0);
    MapObjectCount += 2;

    // Actor pedagang di hotspot shop: berdiri menghadap baris hotspot.
    new STREAMER_TAG_ACTOR:vendor = CreateDynamicActor(211,
        1671.6, -2325.2, 12.38, 90.0,
        .invulnerable = true, .health = 100.0,
        .worldid = -1, .interiorid = -1,
        .streamdistance = 50.0);
    ApplyDynamicActorAnimation(vendor, "DEALER", "DEALER_IDLE", 4.1, 1, 0, 0, 0, 0);
    CreateDynamic3DTextLabel("Pedagang\n{Y} tekan Y", UI_ACCENT_PRIMARY,
        1671.6, -2325.2, 13.4, 15.0, .testlos = 1);
    MapActorCount++;

    // Penjaga stash: berdiri diam menghadap titik stash.
    CreateDynamicActor(16,
        1663.5, -2319.5, 12.38, 135.0,
        .invulnerable = true, .health = 100.0,
        .worldid = -1, .interiorid = -1,
        .streamdistance = 50.0);
    CreateDynamic3DTextLabel("Penjaga Stash\n{Y} tekan Y", UI_ACCENT_PRIMARY,
        1663.5, -2319.5, 13.4, 15.0, .testlos = 1);
    MapActorCount++;

    // Perekrut rekrutan: berdiri di sisi utara hotspot, menghadap jalan kamp.
    CreateDynamicActor(287,
        1669.4, -2316.4, 12.38, 180.0,
        .invulnerable = true, .health = 100.0,
        .worldid = -1, .interiorid = -1,
        .streamdistance = 50.0);
    CreateDynamic3DTextLabel("Perekrut Rekrutan\n{Y} tekan Y / {FFFFFF}/rekrut", UI_ACCENT_PRIMARY,
        1669.4, -2316.4, 13.4, 15.0, .testlos = 1);
    MapActorCount++;

    Log_Format(LOG_INFO, "[MAP] kamp dimuat: %d objek dekorasi, %d actor.", MapObjectCount, MapActorCount);
    return 1;
}

stock Map_Init()
{
    Streamer_SetVisibleItems(STREAMER_TYPE_OBJECT, MAP_VISIBLE_OBJECTS);
    Map_BuildCamp();
    return 1;
}

/* ---- Pintu fisik shelter: dibuat per kunjungan di virtual world base ---- */

stock ShelterDoor_Destroy(playerid)
{
    if (ShelterDoorObj[playerid] == STREAMER_TAG_OBJECT:0) return 0;

    DestroyDynamicObject(ShelterDoorObj[playerid]);
    ShelterDoorObj[playerid] = STREAMER_TAG_OBJECT:0;
    ShelterDoorOpen[playerid] = false;
    return 1;
}

stock ShelterDoor_Create(playerid)
{
    ShelterDoor_Destroy(playerid);

    new worldid = Shelter_VwOf(Base_Id(playerid));
    ShelterDoorObj[playerid] = CreateDynamicObject(SHELTER_DOOR_MODEL,
        SHELTER_DOOR_X, SHELTER_DOOR_Y, SHELTER_DOOR_Z,
        0.0, 0.0, SHELTER_DOOR_RZ_CLOSED,
        .worldid = worldid, .interiorid = SHELTER_INTERIOR, .streamdistance = 60.0);
    ShelterDoorOpen[playerid] = false;
    return 1;
}

/* Dipanggil dari Player_Tick hanya saat pemain ada di dalam shelter. */
stock ShelterDoor_Tick(playerid)
{
    if (ShelterDoorObj[playerid] == STREAMER_TAG_OBJECT:0) return 0;

    new Float:px, Float:py, Float:pz;
    GetPlayerPos(playerid, px, py, pz);

    /* Batas ruang: lewat pintu selatan = keluar sukarela; jatuh/keluar petak
       = penyelamatan darurat. Tanpa ini pemain bisa jatuh dari langit interior 0. */
    if (py > SHELTER_EXIT_Y || pz < SHELTER_FALL_Z
        || px < SHELTER_ROOM_MIN_X - 1.0 || px > SHELTER_ROOM_MAX_X + 1.0)
    {
        Shelter_Exit(playerid);
        return 1;
    }

    new Float:dx = px - SHELTER_DOOR_X;
    new Float:dy = py - SHELTER_DOOR_Y;
    new Float:distance = floatsqroot(dx * dx + dy * dy);

    if (!ShelterDoorOpen[playerid] && distance <= SHELTER_DOOR_OPEN_DIST)
    {
        ShelterDoorOpen[playerid] = true;
        // Gotcha engine: rotasi murni butuh delta Z kecil (0.001) supaya interpolasi jalan.
        MoveDynamicObject(ShelterDoorObj[playerid],
            SHELTER_DOOR_X, SHELTER_DOOR_Y, SHELTER_DOOR_Z + 0.001, SHELTER_DOOR_SPEED,
            0.0, 0.0, SHELTER_DOOR_RZ_OPEN);
        PlayerPlaySound(playerid, 1141, 0.0, 0.0, 0.0);
        return 1;
    }

    if (ShelterDoorOpen[playerid] && distance >= SHELTER_DOOR_CLOSE_DIST)
    {
        ShelterDoorOpen[playerid] = false;
        MoveDynamicObject(ShelterDoorObj[playerid],
            SHELTER_DOOR_X, SHELTER_DOOR_Y, SHELTER_DOOR_Z, SHELTER_DOOR_SPEED,
            0.0, 0.0, SHELTER_DOOR_RZ_CLOSED);
        return 1;
    }
    return 0;
}

stock ShelterDoor_IsPlaced(playerid)
{
    return (ShelterDoorObj[playerid] != STREAMER_TAG_OBJECT:0);
}

/* ---- Ruang shelter custom: lantai, dinding, plafon, furnitur ---- */

stock ShelterRoom_Add(playerid, model, Float:x, Float:y, Float:z, Float:rx, Float:ry, Float:rz, const material[])
{
    if (ShelterRoomCount[playerid] >= SHELTER_ROOM_MAX_OBJS) return 0;

    new worldid = Shelter_VwOf(Base_Id(playerid));
    new STREAMER_TAG_OBJECT:obj = CreateDynamicObject(model, x, y, z, rx, ry, rz,
        .worldid = worldid, .interiorid = SHELTER_INTERIOR, .streamdistance = 60.0);
    if (material[0] != EOS) Map_ApplyMaterial(obj, 0, material);

    ShelterRoomObjs[playerid][ShelterRoomCount[playerid]] = obj;
    ShelterRoomCount[playerid]++;
    return 1;
}

stock ShelterRoom_Destroy(playerid)
{
    for (new index = 0; index < ShelterRoomCount[playerid]; index++)
    {
        DestroyDynamicObject(ShelterRoomObjs[playerid][index]);
        ShelterRoomObjs[playerid][index] = STREAMER_TAG_OBJECT:0;
    }
    ShelterRoomCount[playerid] = 0;
    return 1;
}

stock ShelterRoom_Create(playerid)
{
    ShelterRoom_Destroy(playerid);

    /* Lantai + plafon: panel 3 m diputar rata (rx 90), grid 4x3 = 12x9 m. */
    for (new col = 0; col < 4; col++)
    {
        for (new row = 0; row < 3; row++)
        {
            new Float:tileX = SHELTER_ROOM_MIN_X + 1.5 + (col * 3.0);
            new Float:tileY = SHELTER_ROOM_MIN_Y + 1.5 + (row * 3.0);
            ShelterRoom_Add(playerid, 19447, tileX, tileY, SHELTER_ROOM_FLOOR_Z, 90.0, 0.0, 0.0, "warm_oak");
            ShelterRoom_Add(playerid, 19447, tileX, tileY, SHELTER_ROOM_CEIL_Z, 90.0, 0.0, 0.0, "matte_slate");
        }
    }

    /* Dinding utara (penuh) dan selatan (celah 3 m untuk pintu di x 2496-2499). */
    for (new col = 0; col < 4; col++)
    {
        new Float:wallX = SHELTER_ROOM_MIN_X + 1.5 + (col * 3.0);
        ShelterRoom_Add(playerid, 19447, wallX, SHELTER_ROOM_MIN_Y, SHELTER_ROOM_WALL_Z, 0.0, 0.0, 0.0, "matte_slate");
        if (col != 2)
            ShelterRoom_Add(playerid, 19447, wallX, SHELTER_ROOM_MAX_Y, SHELTER_ROOM_WALL_Z, 0.0, 0.0, 0.0, "matte_slate");
    }

    /* Dinding timur dan barat. */
    for (new row = 0; row < 3; row++)
    {
        new Float:wallY = SHELTER_ROOM_MIN_Y + 1.5 + (row * 3.0);
        ShelterRoom_Add(playerid, 19447, SHELTER_ROOM_MIN_X, wallY, SHELTER_ROOM_WALL_Z, 0.0, 0.0, 90.0, "matte_slate");
        ShelterRoom_Add(playerid, 19447, SHELTER_ROOM_MAX_X, wallY, SHELTER_ROOM_WALL_Z, 0.0, 0.0, 90.0, "matte_slate");
    }

    /* Furnitur: peti penyimpanan (model crate terverifikasi) + papan nama. */
    ShelterRoom_Add(playerid, 1210, 2491.2, -1695.3, 1014.6, 0.0, 0.0, 20.0, "");
    ShelterRoom_Add(playerid, 1210, 2492.3, -1695.5, 1014.6, 0.0, 0.0, -15.0, "");
    ShelterRoom_Add(playerid, 1210, 2500.8, -1695.2, 1014.6, 0.0, 0.0, 40.0, "");

    new STREAMER_TAG_OBJECT:sign = CreateDynamicObject(19477,
        2496.0, -1696.3, 1016.0, 0.0, 0.0, 180.0,
        .worldid = Shelter_VwOf(Base_Id(playerid)), .interiorid = SHELTER_INTERIOR);
    Map_ApplySignage(sign, "SHELTER PRIBADI");
    if (ShelterRoomCount[playerid] < SHELTER_ROOM_MAX_OBJS)
    {
        ShelterRoomObjs[playerid][ShelterRoomCount[playerid]] = sign;
        ShelterRoomCount[playerid]++;
    }
    return 1;
}
