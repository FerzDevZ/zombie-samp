#if defined _ls_distraction_included
    #endinput
#endif
#define _ls_distraction_included

/* Perangkap Suara & Umpan (fitur #17): lempar petasan, botol kaca, atau flare
   untuk mengalihkan perhatian gerombolan zombie saat menyelinap.

   Semua punya wujud fisik nyata (invariant pengguna):
     - Petasan  : objek 345 terbang -> ledakan kecil + asap 354 + 3 dentuman.
     - Botol    : objek 1543 terbang -> pecah, botol tersisa di tanah 90 s.
     - Flare    : objek 354 terbang -> terus terbakar 30 s, asap + bunyi periodik.

   Pengalihan zombie lewat jalur yang SAMA dengan semua suara server:
   Noise_EmitAtPosition -> Horde_AddHeat (spatial hash, murah). Tidak ada
   logika gameplay di OnPlayerUpdate. */

#define DIST_COOLDOWN_MS     5000
#define DIST_THROW_MS        900        /* durasi lintasan di udara */
#define DISTRACTION_MAX_FLARES 8
#define DIST_FLARE_BURN_S    30
#define DIST_FLARE_PULSE_S   2
#define DIST_FLARE_RADIUS    35.0
#define DIST_PETASAN_RADIUS  45.0
#define DIST_BOTOL_RADIUS    25.0
#define DIST_PETASAN_RANGE   12.0
#define DIST_BOTOL_RANGE     14.0
#define DIST_FLARE_RANGE     16.0

#define DIALOG_DISTRACTION   2965

new gDistCooldown[MAX_PLAYERS];                  /* tick saat boleh lempar lagi */
new STREAMER_TAG_OBJECT:gDistFlyer[MAX_PLAYERS]; /* objek yang sedang terbang */
new gDistFlyerItem[MAX_PLAYERS];                 /* item yang dilempar */
new Float:gDistLandX[MAX_PLAYERS];
new Float:gDistLandY[MAX_PLAYERS];
new Float:gDistLandZ[MAX_PLAYERS];
new gDistLandLobby[MAX_PLAYERS];

new STREAMER_TAG_OBJECT:gDistFlareObj[DISTRACTION_MAX_FLARES];
new Float:gDistFlareX[DISTRACTION_MAX_FLARES];
new Float:gDistFlareY[DISTRACTION_MAX_FLARES];
new Float:gDistFlareZ[DISTRACTION_MAX_FLARES];
new gDistFlareLobby[DISTRACTION_MAX_FLARES];
new gDistFlareUntil[DISTRACTION_MAX_FLARES];     /* epoch detik habis terbakar */

forward bool:Distraction_KindFromItem(itemid);

stock bool:Distraction_KindFromItem(itemid)
{
    return (itemid == ITEM_FIRECRACKER || itemid == ITEM_BOTTLE || itemid == ITEM_FLARE) ? true : false;
}

stock Distraction_CooldownLeft(playerid)
{
    new left = gDistCooldown[playerid] - GetTickCount();
    return (left > 0) ? left : 0;
}

/* ------------------------------------------------------------- /lempar */

stock Distraction_ShowMenu(playerid)
{
    if (!Account_IsPlaying(playerid)) return 0;

    new row[96], body[384];
    new petasan = Inventory_CountItem(playerid, ITEM_FIRECRACKER);
    new botol = Inventory_CountItem(playerid, ITEM_BOTTLE);
    new flare = Inventory_CountItem(playerid, ITEM_FLARE);

    body[0] = EOS;
    strcat(body, "Barang\tStok\tEfek\n", sizeof(body));

    format(row, sizeof(row), "Petasan\t%d\tSuara keras 45 m\n", petasan);
    strcat(body, row, sizeof(body));
    format(row, sizeof(row), "Botol Kaca\t%d\tPecah 25 m\n", botol);
    strcat(body, row, sizeof(body));
    format(row, sizeof(row), "Flare\t%d\tTerbakar 30 detik\n", flare);
    strcat(body, row, sizeof(body));

    ShowPlayerDialog(playerid, DIALOG_DISTRACTION, DIALOG_STYLE_TABLIST_HEADERS,
        "Lempar Perangkap Suara", body, "Lempar", "Tutup");
    return 1;
}

stock Distraction_ThrowFromRow(playerid, row)
{
    new itemid;
    switch (row)
    {
        case 0: itemid = ITEM_FIRECRACKER;
        case 1: itemid = ITEM_BOTTLE;
        case 2: itemid = ITEM_FLARE;
        default: return 0;
    }

    if (Inventory_CountItem(playerid, itemid) < 1)
    {
        SendClientMessage(playerid, UI_ACCENT_WARN, "[LEMPAR] Barang itu tidak ada di tas.");
        return 0;
    }

    new cooldown = Distraction_CooldownLeft(playerid);
    if (cooldown > 0)
    {
        new notice[64];
        format(notice, sizeof(notice), "[LEMPAR] Tunggu %d detik lagi.", (cooldown + 999) / 1000);
        SendClientMessage(playerid, UI_ACCENT_WARN, notice);
        return 0;
    }

    if (gDistFlyer[playerid] != STREAMER_TAG_OBJECT:0)
    {
        SendClientMessage(playerid, UI_ACCENT_WARN, "[LEMPAR] Lemparan sebelumnya masih di udara.");
        return 0;
    }

    if (!Account_IsPlaying(playerid)) return 0;

    /* Jangan buang barang di hub: Horde_AddHeat mengabaikan hub, jadi
       lemparan jadi sia-sia. Tolong pemain sebelum barang habis. */
    new Float:px, Float:py, Float:pz;
    GetPlayerPos(playerid, px, py, pz);
    if (Zone_IsHub(Zone_GetIdAt(px, py, pz)))
    {
        SendClientMessage(playerid, UI_ACCENT_WARN, "[LEMPAR] Di hub aman, zombie tidak akan tertarik. Keluar dulu.");
        return 0;
    }

    /* Arah hadap + jarak per jenis barang. */
    new Float:heading, Float:distance;
    GetPlayerFacingAngle(playerid, heading);
    switch (itemid)
    {
        case ITEM_FIRECRACKER: distance = DIST_PETASAN_RANGE;
        case ITEM_BOTTLE:      distance = DIST_BOTOL_RANGE;
        default:               distance = DIST_FLARE_RANGE;
    }
    new Float:tx = px + floatsin(heading, degrees) * distance;
    new Float:ty = py + floatcos(heading, degrees) * distance;

    /* Snap ke tanah lewat raycast collision (pola sama dengan papan pengumuman). */
    new Float:hitX, Float:hitY, Float:hitZ;
    new Float:tz = pz;
    if (CA_RayCastLine(tx, ty, pz + 2.0, tx, ty, pz - 60.0, hitX, hitY, hitZ) != 0)
    {
        tx = hitX;
        ty = hitY;
        tz = hitZ;
    }

    /* Objek terbang: dari tangan pemain menuju titik jatuh. */
    new Float:ox, Float:oy, Float:oz;
    GetPlayerPos(playerid, ox, oy, oz);
    new model = Item_GetModel(itemid);
    gDistFlyer[playerid] = CreateDynamicObject(model, ox, oy, oz + 1.2, 0.0, 0.0, 0.0);
    if (gDistFlyer[playerid] == STREAMER_TAG_OBJECT:0)
    {
        SendClientMessage(playerid, UI_ACCENT_WARN, "[LEMPAR] Gagal melempar. Coba lagi.");
        return 0;
    }
    MoveDynamicObject(gDistFlyer[playerid], tx, ty, tz + 0.4, distance / 0.9);

    Inventory_Consume(playerid, itemid, 1, "lempar perangkap suara");
    PlayerInventoryDirty[playerid] = true;

    gDistFlyerItem[playerid] = itemid;
    gDistLandX[playerid] = tx;
    gDistLandY[playerid] = ty;
    gDistLandZ[playerid] = tz;
    gDistLandLobby[playerid] = Lobby_Of(playerid);
    gDistCooldown[playerid] = GetTickCount() + DIST_COOLDOWN_MS;
    SetTimerEx("Distraction_Land", DIST_THROW_MS, false, "i", playerid);
    return 1;
}

forward Distraction_Land(playerid);

public Distraction_Land(playerid)
{
    if (!IsPlayerConnected(playerid)) return 0;

    new itemid = gDistFlyerItem[playerid];
    if (gDistFlyer[playerid] != STREAMER_TAG_OBJECT:0)
    {
        DestroyDynamicObject(gDistFlyer[playerid]);
        gDistFlyer[playerid] = STREAMER_TAG_OBJECT:0;
    }
    gDistFlyerItem[playerid] = ITEM_NONE_ID;

    new Float:x = gDistLandX[playerid];
    new Float:y = gDistLandY[playerid];
    new Float:z = gDistLandZ[playerid];
    new lobby = gDistLandLobby[playerid];

    switch (itemid)
    {
        case ITEM_FIRECRACKER:
        {
            /* 3 dentuman: langsung, +0.6 s, +1.2 s. Wujud: asap kecil. */
            new smoke = CreateDynamicObject(354, x, y, z + 0.5, 0.0, 0.0, 0.0);
            Noise_EmitAtPosition(x, y, z, DIST_PETASAN_RADIUS, lobby);
            SetTimerEx("Distraction_Pop", 600, false, "ifffi", playerid, x, y, z, lobby);
            SetTimerEx("Distraction_Pop", 1200, false, "ifffi", playerid, x, y, z, lobby);
            if (smoke != STREAMER_TAG_OBJECT:0)
            {
                SetTimerEx("Distraction_ClearSmoke", 2500, false, "d", smoke);
            }
        }
        case ITEM_BOTTLE:
        {
            /* Botol pecah: bunyi sekali, pecahan tersisa di tanah 90 detik. */
            new bottle = CreateDynamicObject(1543, x, y, z + 0.15, 0.0, random(180) - 90.0, 0.0);
            Noise_EmitAtPosition(x, y, z, DIST_BOTOL_RADIUS, lobby);
            if (bottle != STREAMER_TAG_OBJECT:0)
            {
                SetTimerEx("Distraction_ClearBottle", 90000, false, "d", bottle);
            }
        }
        case ITEM_FLARE:
        {
            Distraction_SpawnFlare(x, y, z, lobby);
            Noise_EmitAtPosition(x, y, z, DIST_FLARE_RADIUS, lobby);
        }
    }
    return 1;
}

forward Distraction_Pop(playerid, Float:x, Float:y, Float:z, lobby);

public Distraction_Pop(playerid, Float:x, Float:y, Float:z, lobby)
{
    #pragma unused playerid
    Noise_EmitAtPosition(x, y, z, DIST_PETASAN_RADIUS, lobby);
    return 1;
}

forward Distraction_ClearSmoke(STREAMER_TAG_OBJECT:objectId);

public Distraction_ClearSmoke(STREAMER_TAG_OBJECT:objectId)
{
    if (objectId != STREAMER_TAG_OBJECT:0) DestroyDynamicObject(objectId);
    return 1;
}

forward Distraction_ClearBottle(STREAMER_TAG_OBJECT:objectId);

public Distraction_ClearBottle(STREAMER_TAG_OBJECT:objectId)
{
    if (objectId != STREAMER_TAG_OBJECT:0) DestroyDynamicObject(objectId);
    return 1;
}

/* ------------------------------------------------------------ flare */

stock Distraction_SpawnFlare(Float:x, Float:y, Float:z, lobby)
{
    /* Cari slot flare kosong; kalau penuh, pakai yang paling cepat habis. */
    new slot = -1, oldest = 0;
    for (new idx = 0; idx < DISTRACTION_MAX_FLARES; idx++)
    {
        if (gDistFlareObj[idx] == STREAMER_TAG_OBJECT:0)
        {
            slot = idx;
            break;
        }
        if (oldest == 0 || gDistFlareUntil[idx] < gDistFlareUntil[oldest])
        {
            oldest = idx;
        }
    }
    if (slot == -1)
    {
        slot = oldest;
        if (gDistFlareObj[slot] != STREAMER_TAG_OBJECT:0)
        {
            DestroyDynamicObject(gDistFlareObj[slot]);
            gDistFlareObj[slot] = STREAMER_TAG_OBJECT:0;
        }
    }

    gDistFlareObj[slot] = CreateDynamicObject(354, x, y, z + 0.4, 0.0, 0.0, 0.0);
    gDistFlareX[slot] = x;
    gDistFlareY[slot] = y;
    gDistFlareZ[slot] = z;
    gDistFlareLobby[slot] = lobby;
    gDistFlareUntil[slot] = gettime() + DIST_FLARE_BURN_S;
    return 1;
}

/* Dipanggil dari timer global 2 detik (main.pwn): flare menyala bunyi berkala. */
stock Distraction_Tick()
{
    new now = gettime();
    for (new idx = 0; idx < DISTRACTION_MAX_FLARES; idx++)
    {
        if (gDistFlareObj[idx] == STREAMER_TAG_OBJECT:0) continue;

        if (now >= gDistFlareUntil[idx])
        {
            DestroyDynamicObject(gDistFlareObj[idx]);
            gDistFlareObj[idx] = STREAMER_TAG_OBJECT:0;
            gDistFlareUntil[idx] = 0;
            continue;
        }

        /* Bunyi flare menarik zombie di radius 35 m sepanjang terbakar. */
        Noise_EmitAtPosition(gDistFlareX[idx], gDistFlareY[idx], gDistFlareZ[idx],
            DIST_FLARE_RADIUS, gDistFlareLobby[idx]);
    }
    return 1;
}

stock Distraction_ResetPlayer(playerid)
{
    gDistCooldown[playerid] = 0;
    gDistFlyerItem[playerid] = ITEM_NONE_ID;
    if (gDistFlyer[playerid] != STREAMER_TAG_OBJECT:0)
    {
        DestroyDynamicObject(gDistFlyer[playerid]);
        gDistFlyer[playerid] = STREAMER_TAG_OBJECT:0;
    }
    return 1;
}

/* ------------------------------------------------------------ dialog */

forward Distraction_OnDialogResponse(playerid, dialogid, response, listitem, inputtext[]);

public Distraction_OnDialogResponse(playerid, dialogid, response, listitem, inputtext[])
{
    #pragma unused inputtext

    if (dialogid == DIALOG_DISTRACTION)
    {
        if (response) Distraction_ThrowFromRow(playerid, listitem);
        return 1;
    }
    return 0;
}

/* ------------------------------------------------------------ selftest */

stock Distraction_Selftest()
{
    new bool:ok = true;

    /* Item perangkap terdaftar dan punya model wujud. */
    ok = ok && Item_IsValid(ITEM_FIRECRACKER) && Item_GetModel(ITEM_FIRECRACKER) != 0;
    ok = ok && Item_IsValid(ITEM_BOTTLE) && Item_GetModel(ITEM_BOTTLE) != 0;
    ok = ok && Item_IsValid(ITEM_FLARE) && Item_GetModel(ITEM_FLARE) != 0;

    /* Pemetaan item -> perangkap. */
    ok = ok && Distraction_KindFromItem(ITEM_FIRECRACKER);
    ok = ok && Distraction_KindFromItem(ITEM_BOTTLE);
    ok = ok && Distraction_KindFromItem(ITEM_FLARE);
    ok = ok && !Distraction_KindFromItem(ITEM_DECOY);
    ok = ok && !Distraction_KindFromItem(ITEM_SCRAP);

    /* Slot flare: alokasi, geser oldest saat penuh, bersih. */
    for (new idx = 0; idx < DISTRACTION_MAX_FLARES; idx++)
    {
        gDistFlareObj[idx] = STREAMER_TAG_OBJECT:0;
        gDistFlareUntil[idx] = 0;
    }
    new allocated = 0;
    for (new idx = 0; idx < DISTRACTION_MAX_FLARES; idx++)
    {
        gDistFlareObj[idx] = STREAMER_TAG_OBJECT:1;
        gDistFlareUntil[idx] = 1000 + idx;
        allocated++;
    }
    ok = ok && allocated == DISTRACTION_MAX_FLARES;
    /* SpawnFlare memilih oldest (until terkecil) saat penuh — simulasi pilih. */
    new oldest = 0;
    for (new idx = 1; idx < DISTRACTION_MAX_FLARES; idx++)
    {
        if (gDistFlareUntil[idx] < gDistFlareUntil[oldest]) oldest = idx;
    }
    ok = ok && oldest == 0;
    for (new idx = 0; idx < DISTRACTION_MAX_FLARES; idx++)
    {
        gDistFlareObj[idx] = STREAMER_TAG_OBJECT:0;
        gDistFlareUntil[idx] = 0;
    }

    /* Cooldown mati saat nol. */
    gDistCooldown[0] = GetTickCount() - 1;
    ok = ok && Distraction_CooldownLeft(0) == 0;

    Selftest_Record(ok, "perangkap suara",
        ok ? "petasan/botol/flare + slot flare + cooldown konsisten" : "distraction gagal");
    return 1;
}
