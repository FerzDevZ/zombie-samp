#if defined _ls_build_edit_included
    #endinput
#endif
#define _ls_build_edit_included

/* Mode pratinjau & edit struktur base (ROADMAP baru, laporan pemain).

   Sebelum ini /bangun langsung memotong material lalu menaruh struktur 2,5 m di
   depan pemain berdasarkan sudut hadap SAAT ITU, tanpa pratinjau. Pemain tidak
   pernah melihat di mana bendanya akan berdiri — karena itu dilaporkan sebagai
   "item yang dibangun bergerak": hasilnya tidak sama dengan yang dibayangkan.

   Sekarang: pratinjau (ghost object) + panel TextDraw yang menunjukkan POSISI,
   ARAH (kemiringan), JARAK, dan TINGGI. Material baru dipotong saat PASANG, dan
   struktur yang sudah ada bisa digeser/diputar dengan panel yang sama. */

#define BUILD_EDIT_NONE     0
#define BUILD_EDIT_PLACE    1
#define BUILD_EDIT_MOVE     2

#define BUILD_EDIT_MIN_DIST     1.0
#define BUILD_EDIT_MAX_DIST     6.0
#define BUILD_EDIT_DEFAULT_DIST 2.5
#define BUILD_EDIT_STEP         0.5
#define BUILD_EDIT_ROT_STEP     15.0
#define BUILD_EDIT_RAISE_STEP   0.25
#define BUILD_EDIT_PICK_RANGE   8.0
#define BUILD_EDIT_KEY_DEBOUNCE 120

/* Panel sengaja di tengah-kanan: kolom kiri sudah dipakai HUD (x 24) dan
   kolom kanan bawah dipakai SCRAP/BBM (y >= 330), jadi tidak ada tabrakan. */
#define BUILD_EDIT_PANEL_X 310.0
#define BUILD_EDIT_PANEL_Y 120.0
#define BUILD_EDIT_PANEL_W 320.0
#define BUILD_EDIT_PANEL_H 64.0

enum E_BUILD_EDIT
{
    beMode,
    beType,
    beSlot,
    Float:beDist,
    Float:beX,
    Float:beY,
    Float:beZ,
    Float:beRot,
    Float:beRaise,
    STREAMER_TAG_OBJECT:beGhost,
    beLastKeyTick,
    bool:bePanelShown
};

new BuildEdit[MAX_PLAYERS][E_BUILD_EDIT];
new PlayerText:BuildEditBox[MAX_PLAYERS];
new PlayerText:BuildEditLine[MAX_PLAYERS];

/* Murni: dipakai perintah, tick, dan selftest. */
stock Float:BuildEdit_ClampDistance(Float:distance)
{
    if (distance < BUILD_EDIT_MIN_DIST) return BUILD_EDIT_MIN_DIST;
    if (distance > BUILD_EDIT_MAX_DIST) return BUILD_EDIT_MAX_DIST;
    return distance;
}

/* 0..360, tanpa nilai negatif: HUD menampilkan "ARAH -15" kalau tidak dijepit. */
stock Float:BuildEdit_NormalizeRot(Float:heading)
{
    while (heading < 0.0) heading += 360.0;
    while (heading >= 360.0) heading -= 360.0;
    return heading;
}

stock BuildEdit_ResetState(playerid)
{
    if (playerid < 0 || playerid >= MAX_PLAYERS) return 0;

    if (BuildEdit[playerid][beGhost] != STREAMER_TAG_OBJECT:0)
    {
        DestroyDynamicObject(BuildEdit[playerid][beGhost]);
    }

    BuildEdit[playerid][beMode] = BUILD_EDIT_NONE;
    BuildEdit[playerid][beType] = 0;
    BuildEdit[playerid][beSlot] = -1;
    BuildEdit[playerid][beDist] = BUILD_EDIT_DEFAULT_DIST;
    BuildEdit[playerid][beRaise] = 0.0;
    BuildEdit[playerid][beGhost] = STREAMER_TAG_OBJECT:0;
    BuildEdit[playerid][beLastKeyTick] = 0;

    if (BuildEdit[playerid][bePanelShown])
    {
        BuildEdit[playerid][bePanelShown] = false;
        if (BuildEditBox[playerid] != PlayerText:0)
        {
            PlayerTextDrawHide(playerid, BuildEditBox[playerid]);
            PlayerTextDrawHide(playerid, BuildEditLine[playerid]);
        }
    }
    return 1;
}

stock BuildEdit_EnsurePanel(playerid)
{
    if (BuildEditBox[playerid] != PlayerText:0) return 1;

    BuildEditBox[playerid] = CreatePlayerTextDraw(playerid, BUILD_EDIT_PANEL_X, BUILD_EDIT_PANEL_Y, " ");
    PlayerTextDrawLetterSize(playerid, BuildEditBox[playerid], 0.2, 0.8);
    PlayerTextDrawTextSize(playerid, BuildEditBox[playerid], BUILD_EDIT_PANEL_W, BUILD_EDIT_PANEL_H);
    PlayerTextDrawUseBox(playerid, BuildEditBox[playerid], 1);
    /* Kotak gelap semi-transparan: teks HUD lain terbukti tenggelam di latar
       terang (laporan "textdraw scrap di latar"). */
    PlayerTextDrawBoxColor(playerid, BuildEditBox[playerid], 0x0B0F14D8);
    PlayerTextDrawBackgroundColor(playerid, BuildEditBox[playerid], 0x00000000);
    PlayerTextDrawFont(playerid, BuildEditBox[playerid], 1);
    PlayerTextDrawSetOutline(playerid, BuildEditBox[playerid], 0);

    BuildEditLine[playerid] = CreatePlayerTextDraw(playerid, BUILD_EDIT_PANEL_X + 8.0, BUILD_EDIT_PANEL_Y + 4.0,
        "{EF4444}BANGUN{FFFFFF} WALL");
    PlayerTextDrawLetterSize(playerid, BuildEditLine[playerid], 0.19, 0.8);
    PlayerTextDrawColor(playerid, BuildEditLine[playerid], UI_TEXT_PRIMARY);
    PlayerTextDrawSetOutline(playerid, BuildEditLine[playerid], 1);
    PlayerTextDrawBackgroundColor(playerid, BuildEditLine[playerid], 0x00000000);
    PlayerTextDrawFont(playerid, BuildEditLine[playerid], 1);
    return 1;
}

stock BuildEdit_LabelFor(playerid, dest[], size)
{
    new type = BuildEdit[playerid][beType];
    if (type < 0 || type >= STRUCTURE_COUNT)
    {
        format(dest, size, "?");
        return 0;
    }
    format(dest, size, "%s", gStructures[type][stLabel]);
    return 1;
}

/* Panel pratinjau. Setiap angka di sini adalah angka yang BENAR-BENAR dipakai
   saat pasang, jadi tidak ada lagi selisih antara "yang saya lihat" dan
   "yang berdiri". */
stock BuildEdit_RefreshPanel(playerid)
{
    if (BuildEdit[playerid][beMode] == BUILD_EDIT_NONE) return 0;
    if (!IsPlayerConnected(playerid)) return 0;

    BuildEdit_EnsurePanel(playerid);

    new label[24], head[64], line[300];
    BuildEdit_LabelFor(playerid, label, sizeof(label));

    if (BuildEdit[playerid][beMode] == BUILD_EDIT_PLACE)
    {
        new tierNeed = gStructures[BuildEdit[playerid][beType]][stTier];
        new used = BaseBuild_CountType(Base_Id(playerid), BuildEdit[playerid][beType]);
        new limit = BaseBuild_Limit(BuildEdit[playerid][beType], Base_GetTier(playerid));
        format(head, sizeof(head), "{EF4444}BANGUN{FFFFFF} %s {64748B}(%d/%d, t%d)", label, used, limit, tierNeed);
    }
    else
    {
        format(head, sizeof(head), "{F59E0B}GESER{FFFFFF} %s {64748B}#%d", label, BuildEdit[playerid][beSlot]);
    }

    format(line, sizeof(line),
        "%s~n~{94A3B8}POS{FFFFFF} %.1f %.1f %.1f~n~{94A3B8}ARAH{FFFFFF} %.0f deg  {94A3B8}JARAK{FFFFFF} %.1fm  {94A3B8}NAIK{FFFFFF} %+.2fm~n~{F59E0B}ALT{FFFFFF} maju  {F59E0B}C{FFFFFF} putar  {F59E0B}Y{FFFFFF} pasang  {F59E0B}H{FFFFFF} batal",
        head,
        BuildEdit[playerid][beX], BuildEdit[playerid][beY], BuildEdit[playerid][beZ],
        BuildEdit[playerid][beRot],
        BuildEdit[playerid][beDist],
        BuildEdit[playerid][beRaise]);

    PlayerTextDrawSetString(playerid, BuildEditLine[playerid], line);
    PlayerTextDrawShow(playerid, BuildEditBox[playerid]);
    PlayerTextDrawShow(playerid, BuildEditLine[playerid]);
    BuildEdit[playerid][bePanelShown] = true;
    return 1;
}

stock BuildEdit_GhostModel(type)
{
    if (type < 0 || type >= STRUCTURE_COUNT) return BASE_WALL_OBJECT_MODEL;
    if (gStructures[type][stSolid]) return BASE_WALL_OBJECT_MODEL;
    if (gStructures[type][stPickupModel] > 0) return gStructures[type][stPickupModel];
    return BASE_WALL_OBJECT_MODEL;
}

stock BuildEdit_RefreshGhost(playerid)
{
    if (BuildEdit[playerid][beMode] == BUILD_EDIT_NONE) return 0;

    new Float:x = BuildEdit[playerid][beX];
    new Float:y = BuildEdit[playerid][beY];
    new Float:z = BuildEdit[playerid][beZ] - 0.7;
    new Float:rot = BuildEdit[playerid][beRot];

    /* Ghost selalu dibuat di world lobby pemain; objek dengan world salah
       tidak terlihat dan pemain mengira pratinjaunya rusak. */
    new world = Lobby_WorldById(Lobby_Of(playerid));

    if (BuildEdit[playerid][beGhost] == STREAMER_TAG_OBJECT:0)
    {
        BuildEdit[playerid][beGhost] = CreateDynamicObject(BuildEdit_GhostModel(BuildEdit[playerid][beType]),
            x, y, z, 0.0, 0.0, rot, .worldid = world);
    }
    else
    {
        SetDynamicObjectPos(BuildEdit[playerid][beGhost], x, y, z);
        SetDynamicObjectRot(BuildEdit[playerid][beGhost], 0.0, 0.0, rot);
    }

    BuildEdit_RefreshPanel(playerid);
    return 1;
}

/* Titik target mode BANGUN: 2,5 m (bisa diatur) di depan pemain, memakai sudut
   hadap saat ini. Tinggi memakai Z pemain + penyesuaian manual. */
stock BuildEdit_FollowPlayer(playerid)
{
    if (BuildEdit[playerid][beMode] != BUILD_EDIT_PLACE) return 0;
    if (!IsPlayerConnected(playerid)) return 0;

    new Float:x, Float:y, Float:z, Float:angle;
    GetPlayerPos(playerid, x, y, z);
    GetPlayerFacingAngle(playerid, angle);

    new Float:distance = BuildEdit_ClampDistance(BuildEdit[playerid][beDist]);
    BuildEdit[playerid][beDist] = distance;

    BuildEdit[playerid][beX] = x + floatsin(-angle, degrees) * distance;
    BuildEdit[playerid][beY] = y + floatcos(-angle, degrees) * distance;
    BuildEdit[playerid][beZ] = z + BuildEdit[playerid][beRaise];
    BuildEdit[playerid][beRot] = BuildEdit_NormalizeRot(angle);
    return 1;
}

stock BuildEdit_BeginPlace(playerid, type)
{
    if (!Account_IsPlaying(playerid)) return 0;
    if (type < 0 || type >= STRUCTURE_COUNT) return 0;

    if (!BaseBuild_EnsureLoaded(playerid))
    {
        SendClientMessage(playerid, UI_TEXT_MUTED, "[BUILD] Struktur base masih dimuat, ulangi sebentar lagi.");
        return 0;
    }

    BuildEdit_ResetState(playerid);

    BuildEdit[playerid][beMode] = BUILD_EDIT_PLACE;
    BuildEdit[playerid][beType] = type;
    BuildEdit[playerid][beSlot] = -1;
    BuildEdit[playerid][beDist] = BUILD_EDIT_DEFAULT_DIST;
    BuildEdit[playerid][beRaise] = 0.0;

    BuildEdit_FollowPlayer(playerid);
    BuildEdit_RefreshGhost(playerid);

    SendClientMessage(playerid, UI_ACCENT_INFO, "[BUILD] Pratinjau aktif. Arahkan badan, lalu Y untuk pasang (ALT maju, C putar, H batal).");
    SendClientMessage(playerid, UI_TEXT_MUTED, "[BUILD] Android tanpa tombol? /geser 0.5  /putar 15  /tinggi 0.25  /pasang  /batal");
    return 1;
}

stock BuildEdit_BeginMove(playerid)
{
    if (!Account_IsPlaying(playerid)) return 0;
    if (!BaseBuild_EnsureLoaded(playerid))
    {
        SendClientMessage(playerid, UI_TEXT_MUTED, "[BUILD] Struktur base masih dimuat, ulangi sebentar lagi.");
        return 0;
    }

    new Float:distance;
    new slot = BaseBuild_NearestOwnSlot(playerid, BUILD_EDIT_PICK_RANGE, distance);
    if (slot < 0)
    {
        new line[128];
        format(line, sizeof(line), "[BUILD] Tidak ada struktur base dalam %.0f meter.", BUILD_EDIT_PICK_RANGE);
        SendClientMessage(playerid, UI_ACCENT_WARN, line);
        return 0;
    }

    BuildEdit_ResetState(playerid);

    BuildEdit[playerid][beMode] = BUILD_EDIT_MOVE;
    BuildEdit[playerid][beSlot] = slot;
    BuildEdit[playerid][beType] = BaseStructure[slot][ssType];
    BuildEdit[playerid][beX] = BaseStructure[slot][ssX];
    BuildEdit[playerid][beY] = BaseStructure[slot][ssY];
    BuildEdit[playerid][beZ] = BaseStructure[slot][ssZ];
    BuildEdit[playerid][beRot] = BaseStructure[slot][ssRotZ];
    BuildEdit[playerid][beDist] = distance;
    BuildEdit[playerid][beRaise] = 0.0;

    BuildEdit_RefreshGhost(playerid);

    new label[24], line[144];
    BuildEdit_LabelFor(playerid, label, sizeof(label));
    format(line, sizeof(line), "[BUILD] Menggeser %s. Y simpan, H batal, ALT maju, C putar.", label);
    SendClientMessage(playerid, UI_ACCENT_INFO, line);
    return 1;
}

stock BuildEdit_AdjustDistance(playerid, Float:delta)
{
    if (BuildEdit[playerid][beMode] == BUILD_EDIT_NONE) return 0;
    if (BuildEdit[playerid][beMode] == BUILD_EDIT_MOVE) return BuildEdit_Nudge(playerid, delta);

    BuildEdit[playerid][beDist] = BuildEdit_ClampDistance(BuildEdit[playerid][beDist] + delta);
    BuildEdit_FollowPlayer(playerid);
    BuildEdit_RefreshGhost(playerid);
    return 1;
}

/* Mode GESER: dorong sejauh `meters` mengikuti arah hadap pemain. Struktur
   tidak boleh "menempel" ke pemain, jadi jarak minimum tetap dijaga. */
stock BuildEdit_Nudge(playerid, Float:meters)
{
    if (BuildEdit[playerid][beMode] != BUILD_EDIT_MOVE) return 0;

    new Float:angle;
    GetPlayerFacingAngle(playerid, angle);

    BuildEdit[playerid][beX] += floatsin(-angle, degrees) * meters;
    BuildEdit[playerid][beY] += floatcos(-angle, degrees) * meters;

    new Float:px, Float:py, Float:pz;
    GetPlayerPos(playerid, px, py, pz);
    BuildEdit[playerid][beDist] = VectorSize(BuildEdit[playerid][beX] - px,
        BuildEdit[playerid][beY] - py, 0.0);

    BuildEdit_RefreshGhost(playerid);
    return 1;
}

stock BuildEdit_Rotate(playerid, Float:step)
{
    if (BuildEdit[playerid][beMode] == BUILD_EDIT_NONE) return 0;

    BuildEdit[playerid][beRot] = BuildEdit_NormalizeRot(BuildEdit[playerid][beRot] + step);
    BuildEdit_RefreshGhost(playerid);
    return 1;
}

stock BuildEdit_Raise(playerid, Float:meters)
{
    if (BuildEdit[playerid][beMode] != BUILD_EDIT_PLACE) return 0;

    BuildEdit[playerid][beRaise] += meters;
    if (BuildEdit[playerid][beRaise] > 3.0) BuildEdit[playerid][beRaise] = 3.0;
    if (BuildEdit[playerid][beRaise] < -1.0) BuildEdit[playerid][beRaise] = -1.0;

    BuildEdit_FollowPlayer(playerid);
    BuildEdit_RefreshGhost(playerid);
    return 1;
}

stock BuildEdit_Confirm(playerid)
{
    new mode = BuildEdit[playerid][beMode];
    if (mode == BUILD_EDIT_NONE) return 0;

    new Float:x = BuildEdit[playerid][beX];
    new Float:y = BuildEdit[playerid][beY];
    new Float:z = BuildEdit[playerid][beZ];
    new Float:rot = BuildEdit[playerid][beRot];

    if (mode == BUILD_EDIT_PLACE)
    {
        new type = BuildEdit[playerid][beType];
        /* Semua validasi + biaya material tetap di BaseBuild_PlaceAt: satu jalur
           aturan untuk pratinjau dan jalur lama. */
        if (!BaseBuild_PlaceAt(playerid, type, x, y, z, rot)) return 0;
        BuildEdit_ResetState(playerid);
        return 1;
    }

    new slot = BuildEdit[playerid][beSlot];
    if (slot < 0 || slot >= BASE_MAX_STRUCTURES || !BaseStructure[slot][ssActive])
    {
        SendClientMessage(playerid, UI_ACCENT_WARN, "[BUILD] Struktur itu sudah tidak ada (dihancurkan?).");
        BuildEdit_ResetState(playerid);
        return 0;
    }

    if (BaseStructure[slot][ssBaseId] != Base_Id(playerid))
    {
        SendClientMessage(playerid, UI_ACCENT_DANGER, "[BUILD] Struktur itu bukan milik base kamu.");
        BuildEdit_ResetState(playerid);
        return 0;
    }

    if (!BaseBuild_IsSpotValid(playerid, x, y, z, slot))
    {
        SendClientMessage(playerid, UI_ACCENT_WARN, "[BUILD] Titik itu tidak valid, geser lagi atau batal.");
        return 0;
    }

    BaseBuild_MoveSlotTo(slot, x, y, z, rot);
    BuildEdit_ResetState(playerid);

    new label[24], line[128];
    BaseBuild_LabelForSlot(slot, label, sizeof(label));
    format(line, sizeof(line), "[BUILD] %s dipindahkan ke %.1f, %.1f (arah %.0f).", label, x, y, rot);
    SendClientMessage(playerid, UI_ACCENT_PRIMARY, line);
    Toast_Show(playerid, "EDIT BASE", line, UI_ACCENT_PRIMARY);
    return 1;
}

stock BuildEdit_Cancel(playerid, bool:notify = true)
{
    new wasActive = (BuildEdit[playerid][beMode] != BUILD_EDIT_NONE);
    BuildEdit_ResetState(playerid);

    if (wasActive && notify && IsPlayerConnected(playerid))
    {
        SendClientMessage(playerid, UI_TEXT_MUTED, "[BUILD] Pratinjau dibatalkan, tidak ada material yang terpakai.");
    }
    return wasActive;
}

/* Dipanggil dari Player_Tick (500 ms): mode BANGUN mengikuti pemain, mode GESER
   tidak (posisi absolut supaya bisa digeser sambil berjalan mengelilinginya). */
stock BuildEdit_TickPlayer(playerid)
{
    if (BuildEdit[playerid][beMode] == BUILD_EDIT_NONE) return 0;
    if (!IsPlayerConnected(playerid)) return 0;

    if (BuildEdit[playerid][beMode] == BUILD_EDIT_MOVE)
    {
        new slot = BuildEdit[playerid][beSlot];
        if (slot < 0 || slot >= BASE_MAX_STRUCTURES || !BaseStructure[slot][ssActive])
        {
            BuildEdit_Cancel(playerid, false);
            SendClientMessage(playerid, UI_ACCENT_WARN, "[BUILD] Struktur yang diedit sudah tidak ada.");
            return 0;
        }
        return BuildEdit_RefreshGhost(playerid);
    }

    BuildEdit_FollowPlayer(playerid);
    return BuildEdit_RefreshGhost(playerid);
}

/* Tombol fisik: ALT (maju), C (putar), Y (pasang), H (batal).
   Debounce 120 ms karena SA-MP mengirim key berulang selama ditahan. */
stock BuildEdit_OnKeyState(playerid, newkeys, oldkeys)
{
    if (BuildEdit[playerid][beMode] == BUILD_EDIT_NONE) return 0;
    if (GetTickCount() - BuildEdit[playerid][beLastKeyTick] < BUILD_EDIT_KEY_DEBOUNCE) return 1;

    if ((newkeys & KEY_WALK) && !(oldkeys & KEY_WALK))
    {
        BuildEdit[playerid][beLastKeyTick] = GetTickCount();
        BuildEdit_AdjustDistance(playerid, BUILD_EDIT_STEP);
        return 1;
    }

    if ((newkeys & KEY_CROUCH) && !(oldkeys & KEY_CROUCH))
    {
        BuildEdit[playerid][beLastKeyTick] = GetTickCount();
        BuildEdit_Rotate(playerid, BUILD_EDIT_ROT_STEP);
        return 1;
    }

    if ((newkeys & KEY_YES) && !(oldkeys & KEY_YES))
    {
        BuildEdit[playerid][beLastKeyTick] = GetTickCount();
        BuildEdit_Confirm(playerid);
        return 1;
    }

    if ((newkeys & KEY_NO) && !(oldkeys & KEY_NO))
    {
        BuildEdit[playerid][beLastKeyTick] = GetTickCount();
        BuildEdit_Cancel(playerid);
        return 1;
    }

    /* Mode aktif menelan tombol lain supaya pemain tidak menembak/interaksi
       tanpa sengaja saat sedang menaruh struktur. */
    return 1;
}

stock BuildEdit_IsActive(playerid)
{
    return (BuildEdit[playerid][beMode] != BUILD_EDIT_NONE) ? 1 : 0;
}
