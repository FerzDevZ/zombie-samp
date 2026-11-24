#if defined _ls_build_edit_included
    #endinput
#endif
#define _ls_build_edit_included

/* Preferensi sisi panel mobile (/settings, settings.inc di-include nanti). */
forward Float:Settings_BuildPanelX(playerid);
forward bool:Settings_TdMobileSafe(playerid);
/* hud.inc di-include SETELAH file ini: deklarasi box helper wajib di sini. */
forward PlayerText:TD_BoxCreate(playerid, Float:x, Float:y, Float:width, Float:height, color);

/* Editor struktur base — UI klik-able gaya FiveM/NoPixel.

   Mode ini menggantikan jalur lama yang "menaruh struktur lalu berharap":
   pemain melihat ghost object + kartu gelap dengan tombol yang bisa DIKLIK
   (SelectTextDraw), berisi X/Y/Z, putar kiri/kanan, maju/mundur, naik/turun,
   dan langkah presisi. Tombol fisik ALT/C/Y/H tetap ada untuk pemain yang
   tidak memakai mouse.

   Semua tombol memakai sprite sebagai hitbox (LD_SPAC:white / LD_BEAT:chit)
   dan label non-selectable, mengikuti aturan hitbox FiveM: satu selectable
   per tombol, jadi kursor tidak pernah "berebut" antara label dan latar. */

#define BUILD_EDIT_NONE     0
#define BUILD_EDIT_PLACE    1
#define BUILD_EDIT_MOVE     2

#define BUILD_EDIT_MIN_DIST     1.0
#define BUILD_EDIT_MAX_DIST     6.0
#define BUILD_EDIT_DEFAULT_DIST 2.5
#define BUILD_EDIT_PICK_RANGE   8.0
#define BUILD_EDIT_KEY_DEBOUNCE 120
#define BUILD_EDIT_MAX_SIDE     6.0
#define BUILD_EDIT_MAX_RAISE    3.0
#define BUILD_EDIT_MIN_RAISE    (-1.0)

/* Panel: kanan atas, tidak menabrak HUD kiri (x 24) maupun baris SCRAP/BBM
   (y >= 330). Lebar 294 px menyisakan 10 px dari tepi 640 pada skala acuan. */
#define BUILD_EDIT_PANEL_X 334.0
#define BUILD_EDIT_PANEL_Y 40.0
#define BUILD_EDIT_PANEL_W 294.0
#define BUILD_EDIT_PANEL_H 210.0

/* Layout panel per gaya (docs/UIUX_PLAN.md). PC memakai literal di atas;
   Mobile Kompak menyusutkan panel 0.70 dan ditempelkan ke kanan-atas agar
   keluar dari joystick (kiri-bawah) dan tombol sentuh (kanan-bawah) Android.
   Tombol mobile TIDAK ikut menyusut penuh: jari butuh sasaran >= 16 px,
   jadi tinggi tombol ditahan dan baris diberi jarak lebih longgar. */
/* MARKER-A */
/* MARKER-B */
enum E_BE_LAYOUT {
    blMobile,
    Float:blX,
    Float:blY,
    Float:blW,
    Float:blH,
    Float:blBtnW,
    Float:blBtnH,
    Float:blRowStep,
    Float:blInfoStep,
    Float:blColL,
    Float:blColR,
    Float:blPasangX,
    Float:blPasangW,
    Float:blPasangY,
    Float:blPasangH,
    Float:blHintY,
    Float:blFontRow,
    Float:blScale
}
new gBeLayout[MAX_PLAYERS][E_BE_LAYOUT];

/* Ukuran panel bangun. Pemain melaporkan panel menutupi layar sehingga susah
   menaruh dinding: satu tombol UKURAN + /panel kecil|besar menyusutkan seluruh
   panel. Tombol tetap punya lantai ukuran supaya masih bisa disentuh di HP. */
#define BE_PANEL_SCALE_COUNT 4
new const Float:BE_PANEL_SCALES[BE_PANEL_SCALE_COUNT] = {1.00, 0.80, 0.62, 0.48};
new Float:BuildEditPanelScale[MAX_PLAYERS];
new bool:BuildEditPanelHidden[MAX_PLAYERS];

stock BuildEdit_ComputeLayout(playerid)
{
    if (BuildEditPanelScale[playerid] <= 0.0) BuildEditPanelScale[playerid] = 1.0;

    if (gHudGaya[playerid] == HUD_GAYA_MOBILE)
    {
        gBeLayout[playerid][blMobile] = 1;
        gBeLayout[playerid][blX] = Settings_BuildPanelX(playerid);   /* /settings: kanan/kiri */
        gBeLayout[playerid][blY] = 28.0;
        gBeLayout[playerid][blW] = 206.0;
        gBeLayout[playerid][blH] = 224.0;
        gBeLayout[playerid][blBtnW] = 38.0;
        gBeLayout[playerid][blBtnH] = 16.0;
        gBeLayout[playerid][blRowStep] = 22.0;
        gBeLayout[playerid][blInfoStep] = 13.0;
        gBeLayout[playerid][blColL] = 116.0;
        gBeLayout[playerid][blColR] = 160.0;
        gBeLayout[playerid][blPasangX] = 10.0;
        gBeLayout[playerid][blPasangW] = 90.0;
        gBeLayout[playerid][blPasangY] = 170.0;
        gBeLayout[playerid][blPasangH] = 22.0;
        gBeLayout[playerid][blHintY] = 200.0;
        gBeLayout[playerid][blFontRow] = 0.13;
        BuildEdit_ApplyScale(playerid, 16.0, 34.0, 150.0, 132.0);
        return 1;
    }

    gBeLayout[playerid][blMobile] = false;
    gBeLayout[playerid][blX] = BUILD_EDIT_PANEL_X;
    gBeLayout[playerid][blY] = BUILD_EDIT_PANEL_Y;
    gBeLayout[playerid][blW] = BUILD_EDIT_PANEL_W;
    gBeLayout[playerid][blH] = BUILD_EDIT_PANEL_H;
    gBeLayout[playerid][blBtnW] = 40.0;
    gBeLayout[playerid][blBtnH] = 12.0;
    gBeLayout[playerid][blRowStep] = 16.0;
    gBeLayout[playerid][blInfoStep] = 16.0;
    gBeLayout[playerid][blColL] = 186.0;
    gBeLayout[playerid][blColR] = 228.0;
    gBeLayout[playerid][blPasangX] = 12.0;
    gBeLayout[playerid][blPasangW] = 130.0;
    gBeLayout[playerid][blPasangY] = 142.0;
    gBeLayout[playerid][blPasangH] = 20.0;
    gBeLayout[playerid][blHintY] = 168.0;
    gBeLayout[playerid][blFontRow] = 0.19;
    BuildEdit_ApplyScale(playerid, 12.0, 30.0, 176.0, 140.0);
    return 1;
}

/* Lantai ukuran (minBtnH/minBtnW/minW/minH) penting: menyusutkan tombol sampai
   < 16 px membuatnya tidak bisa disentuh di Android. */
stock BuildEdit_ApplyScale(playerid, Float:minBtnH, Float:minBtnW, Float:minW, Float:minH)
{
    new Float:scale = BuildEditPanelScale[playerid];
    gBeLayout[playerid][blScale] = scale;

    gBeLayout[playerid][blW] = gBeLayout[playerid][blW] * scale;
    if (gBeLayout[playerid][blW] < minW) gBeLayout[playerid][blW] = minW;

    gBeLayout[playerid][blH] = gBeLayout[playerid][blH] * scale;
    if (gBeLayout[playerid][blH] < minH) gBeLayout[playerid][blH] = minH;

    gBeLayout[playerid][blBtnW] = gBeLayout[playerid][blBtnW] * scale;
    if (gBeLayout[playerid][blBtnW] < minBtnW) gBeLayout[playerid][blBtnW] = minBtnW;

    gBeLayout[playerid][blBtnH] = gBeLayout[playerid][blBtnH] * scale;
    if (gBeLayout[playerid][blBtnH] < minBtnH) gBeLayout[playerid][blBtnH] = minBtnH;

    gBeLayout[playerid][blRowStep] = gBeLayout[playerid][blRowStep] * scale;
    if (gBeLayout[playerid][blRowStep] < gBeLayout[playerid][blBtnH] + 3.0)
        gBeLayout[playerid][blRowStep] = gBeLayout[playerid][blBtnH] + 3.0;

    gBeLayout[playerid][blInfoStep] = gBeLayout[playerid][blInfoStep] * scale;
    if (gBeLayout[playerid][blInfoStep] < 10.0) gBeLayout[playerid][blInfoStep] = 10.0;

    gBeLayout[playerid][blColL] = gBeLayout[playerid][blColL] * scale;
    gBeLayout[playerid][blColR] = gBeLayout[playerid][blColR] * scale;

    /* Tombol + tidak boleh melewati tepi kanan panel. */
    if (gBeLayout[playerid][blColR] + gBeLayout[playerid][blBtnW] > gBeLayout[playerid][blW] - 4.0)
    {
        new Float:overflow = (gBeLayout[playerid][blColR] + gBeLayout[playerid][blBtnW]) - (gBeLayout[playerid][blW] - 4.0);
        gBeLayout[playerid][blW] = gBeLayout[playerid][blW] + overflow;
    }

    gBeLayout[playerid][blPasangX] = gBeLayout[playerid][blPasangX] * scale;
    gBeLayout[playerid][blPasangW] = gBeLayout[playerid][blPasangW] * scale;
    gBeLayout[playerid][blPasangY] = gBeLayout[playerid][blPasangY] * scale;
    gBeLayout[playerid][blPasangH] = gBeLayout[playerid][blPasangH] * scale;
    if (gBeLayout[playerid][blPasangH] < minBtnH) gBeLayout[playerid][blPasangH] = minBtnH;

    /* Dua tombol pasang+batal harus muat berdampingan. */
    new Float:pairNeed = (gBeLayout[playerid][blPasangW] * 2.0) + (gBeLayout[playerid][blPasangX] * 2.0) + 4.0;
    if (gBeLayout[playerid][blW] < pairNeed) gBeLayout[playerid][blW] = pairNeed;

    /* Semua isi panel harus berhenti sebelum tepi bawah. */
    new Float:contentBottom = gBeLayout[playerid][blPasangY] + gBeLayout[playerid][blPasangH] + 26.0;
    if (gBeLayout[playerid][blH] < contentBottom) gBeLayout[playerid][blH] = contentBottom;

    gBeLayout[playerid][blHintY] = gBeLayout[playerid][blH] - 22.0;
    if (gBeLayout[playerid][blHintY] < gBeLayout[playerid][blPasangY] + gBeLayout[playerid][blPasangH] + 2.0)
        gBeLayout[playerid][blHintY] = gBeLayout[playerid][blPasangY] + gBeLayout[playerid][blPasangH] + 2.0;

    gBeLayout[playerid][blFontRow] = gBeLayout[playerid][blFontRow] * scale;
    if (gBeLayout[playerid][blFontRow] < 0.10) gBeLayout[playerid][blFontRow] = 0.10;
    return 1;
}

/* Tombol. Urutan ini juga urutan tampil di panel. */
#define BE_BTN_XMINUS   0
#define BE_BTN_XPLUS    1
#define BE_BTN_YMINUS   2
#define BE_BTN_YPLUS    3
#define BE_BTN_ZMINUS   4
#define BE_BTN_ZPLUS    5
#define BE_BTN_ROTLEFT  6
#define BE_BTN_ROTRIGHT 7
#define BE_BTN_DISTMIN  8
#define BE_BTN_DISTPLUS 9
#define BE_BTN_STEP     10
#define BE_BTN_CONFIRM  11
#define BE_BTN_CANCEL   12
#define BE_BTN_CLOSEMOUSE 13
#define BE_BTN_SIZE     14
#define BE_BTN_COUNT    15

/* Langkah presisi: 0 = halus, 1 = normal, 2 = kasar. Rotasi ikut supaya satu
   tombol cukup untuk "geser dikit" dan "geser jauh". */
#define BE_STEP_COUNT 3

enum E_BUILD_EDIT
{
    beMode,
    beType,
    beSlot,
    Float:beDist,
    Float:beSide,
    Float:beX,
    Float:beY,
    Float:beZ,
    Float:beRot,
    Float:beRaise,
    beStepStage,
    STREAMER_TAG_OBJECT:beGhost,
    beLastKeyTick,
    bool:bePanelShown,
    bool:beMouseOn
};

new BuildEdit[MAX_PLAYERS][E_BUILD_EDIT];

/* Override model untuk sesi ini saja (/model <id>). Dipakai saat pemain ingin
   siluet lain tanpa menunggu rebuild; nilai 0 = pakai gStructureModels. */
new BuildEditModelOverride[MAX_PLAYERS];

/* Kartu rounded: 2 strip + 4 sudut (teknik sprite, bukan box tajam). */
new PlayerText:BuildEditCard[MAX_PLAYERS][6];
new PlayerText:BuildEditAccent[MAX_PLAYERS];
new PlayerText:BuildEditTitle[MAX_PLAYERS];
new PlayerText:BuildEditSub[MAX_PLAYERS];
new PlayerText:BuildEditDivider[MAX_PLAYERS];
new PlayerText:BuildEditRow[MAX_PLAYERS][6];
new PlayerText:BuildEditHint[MAX_PLAYERS];
new PlayerText:BuildEditBtn[MAX_PLAYERS][BE_BTN_COUNT];
new PlayerText:BuildEditBtnText[MAX_PLAYERS][BE_BTN_COUNT];

forward BuildEdit_HidePanel(playerid);
forward BuildEdit_RefreshPanel(playerid);
forward BuildEdit_Nudge(playerid, Float:meters);

new const Float:gBeStepMeters[BE_STEP_COUNT] = {0.25, 0.5, 1.0};
new const Float:gBeStepDegrees[BE_STEP_COUNT] = {5.0, 15.0, 45.0};

stock Float:BuildEdit_StepMeters(playerid)
{
    new stage = BuildEdit[playerid][beStepStage];
    if (stage < 0 || stage >= BE_STEP_COUNT) stage = 1;
    return gBeStepMeters[stage];
}

stock Float:BuildEdit_StepDegrees(playerid)
{
    new stage = BuildEdit[playerid][beStepStage];
    if (stage < 0 || stage >= BE_STEP_COUNT) stage = 1;
    return gBeStepDegrees[stage];
}

/* ------------------------------------------------------------ nilai murni */

stock Float:BuildEdit_ClampDistance(Float:distance)
{
    if (distance < BUILD_EDIT_MIN_DIST) return BUILD_EDIT_MIN_DIST;
    if (distance > BUILD_EDIT_MAX_DIST) return BUILD_EDIT_MAX_DIST;
    return distance;
}

/* 0..360: panel menampilkan "ARAH -15" kalau tidak dijepit. */
stock Float:BuildEdit_NormalizeRot(Float:heading)
{
    while (heading < 0.0) heading += 360.0;
    while (heading >= 360.0) heading -= 360.0;
    return heading;
}

stock Float:BuildEdit_ClampValue(Float:value, Float:low, Float:high)
{
    if (value < low) return low;
    if (value > high) return high;
    return value;
}

/* ------------------------------------------------------------ siklus hidup */

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
    BuildEdit[playerid][beSide] = 0.0;
    BuildEdit[playerid][beRaise] = 0.0;
    BuildEdit[playerid][beStepStage] = 1;
    BuildEdit[playerid][beGhost] = STREAMER_TAG_OBJECT:0;
    BuildEdit[playerid][beLastKeyTick] = 0;
    BuildEditModelOverride[playerid] = 0;
    BuildEditPanelHidden[playerid] = false;

    BuildEdit_HidePanel(playerid);
    return 1;
}

stock BuildEdit_IsActive(playerid)
{
    return (BuildEdit[playerid][beMode] != BUILD_EDIT_NONE) ? 1 : 0;
}

/* ------------------------------------------------------------ kartu & tombol */

stock BuildEdit_CreateCard(playerid, Float:x, Float:y, Float:width, Float:height, Float:radius)
{
    if (Settings_TdMobileSafe(playerid))
    {
        BuildEditCard[playerid][0] = TD_BoxCreate(playerid, x, y, width, height, UI_BG_GLASS_DARK);
        for (new slot = 1; slot < 6; slot++)
        {
            BuildEditCard[playerid][slot] = TD_BoxCreate(playerid, x, y, 0.0, 0.0, 0x00000000);
        }
        return 1;
    }
    new Float:rx = radius * TD_ASPECT_16_9;
    new Float:diamX = rx * 2.0;
    new Float:diamY = radius * 2.0;

    BuildEditCard[playerid][0] = CreatePlayerTextDraw(playerid, x, y + radius, "LD_SPAC:white");
    PlayerTextDrawFont(playerid, BuildEditCard[playerid][0], TEXT_DRAW_FONT_SPRITE_DRAW);
    PlayerTextDrawTextSize(playerid, BuildEditCard[playerid][0], width, height - diamY);
    PlayerTextDrawColor(playerid, BuildEditCard[playerid][0], UI_BG_GLASS_DARK);
    PlayerTextDrawSetSelectable(playerid, BuildEditCard[playerid][0], false);

    BuildEditCard[playerid][1] = CreatePlayerTextDraw(playerid, x + rx, y, "LD_SPAC:white");
    PlayerTextDrawFont(playerid, BuildEditCard[playerid][1], TEXT_DRAW_FONT_SPRITE_DRAW);
    PlayerTextDrawTextSize(playerid, BuildEditCard[playerid][1], width - diamX, height);
    PlayerTextDrawColor(playerid, BuildEditCard[playerid][1], UI_BG_GLASS_DARK);
    PlayerTextDrawSetSelectable(playerid, BuildEditCard[playerid][1], false);

    new Float:cornerX[4];
    new Float:cornerY[4];
    cornerX[0] = x;                      cornerY[0] = y;
    cornerX[1] = x + width - diamX;      cornerY[1] = y;
    cornerX[2] = x;                      cornerY[2] = y + height - diamY;
    cornerX[3] = x + width - diamX;      cornerY[3] = y + height - diamY;

    for (new corner = 0; corner < 4; corner++)
    {
        BuildEditCard[playerid][2 + corner] = CreatePlayerTextDraw(playerid, cornerX[corner], cornerY[corner], "LD_BEAT:chit");
        PlayerTextDrawFont(playerid, BuildEditCard[playerid][2 + corner], TEXT_DRAW_FONT_SPRITE_DRAW);
        PlayerTextDrawTextSize(playerid, BuildEditCard[playerid][2 + corner], diamX, diamY);
        PlayerTextDrawColor(playerid, BuildEditCard[playerid][2 + corner], UI_BG_GLASS_DARK);
        PlayerTextDrawSetSelectable(playerid, BuildEditCard[playerid][2 + corner], false);
    }
    return 1;
}

/* Satu tombol = satu sprite selectable + satu label non-selectable.
   Membuat label ikut selectable adalah sumber klasik salah-klik di SA-MP. */
stock BuildEdit_CreateButton(playerid, index, Float:x, Float:y, Float:width, Float:height,
    const label[], background)
{
    if (Settings_TdMobileSafe(playerid))
    {
        BuildEditBtn[playerid][index] = TD_BoxCreate(playerid, x, y, width, height, background);
        PlayerTextDrawSetSelectable(playerid, BuildEditBtn[playerid][index], true);
    }
    else
    {
    BuildEditBtn[playerid][index] = CreatePlayerTextDraw(playerid, x, y, "LD_SPAC:white");
    PlayerTextDrawFont(playerid, BuildEditBtn[playerid][index], TEXT_DRAW_FONT_SPRITE_DRAW);
    PlayerTextDrawTextSize(playerid, BuildEditBtn[playerid][index], width, height);
    PlayerTextDrawColor(playerid, BuildEditBtn[playerid][index], background);
    PlayerTextDrawSetSelectable(playerid, BuildEditBtn[playerid][index], true);
    }

    BuildEditBtnText[playerid][index] = CreatePlayerTextDraw(playerid,
        x + (width / 2.0), y + (height / 2.0) - 4.6, label);
    PlayerTextDrawFont(playerid, BuildEditBtnText[playerid][index], 1);
    PlayerTextDrawAlignment(playerid, BuildEditBtnText[playerid][index], 2);
    PlayerTextDrawLetterSize(playerid, BuildEditBtnText[playerid][index], 0.18, 0.8);
    PlayerTextDrawColor(playerid, BuildEditBtnText[playerid][index], UI_TEXT_PRIMARY);
    PlayerTextDrawSetOutline(playerid, BuildEditBtnText[playerid][index], 0);
    PlayerTextDrawSetShadow(playerid, BuildEditBtnText[playerid][index], 0);
    PlayerTextDrawSetProportional(playerid, BuildEditBtnText[playerid][index], true);
    PlayerTextDrawSetSelectable(playerid, BuildEditBtnText[playerid][index], false);
    return 1;
}

stock BuildEdit_CreateRow(playerid, index, Float:x, Float:y)
{
    BuildEditRow[playerid][index] = CreatePlayerTextDraw(playerid, x, y, " ");
    PlayerTextDrawFont(playerid, BuildEditRow[playerid][index], 1);
    PlayerTextDrawLetterSize(playerid, BuildEditRow[playerid][index], gBeLayout[playerid][blFontRow], 0.85);
    PlayerTextDrawColor(playerid, BuildEditRow[playerid][index], UI_TEXT_SECONDARY);
    PlayerTextDrawSetOutline(playerid, BuildEditRow[playerid][index], 1);
    PlayerTextDrawSetShadow(playerid, BuildEditRow[playerid][index], 0);
    /* Monospace: angka X/Y/Z harus lurus antar-refresh, bukan bergeser. */
    PlayerTextDrawSetProportional(playerid, BuildEditRow[playerid][index], false);
    PlayerTextDrawSetSelectable(playerid, BuildEditRow[playerid][index], false);
    return 1;
}

stock BuildEdit_CreatePanel(playerid)
{
    if (BuildEditCard[playerid][0] != PlayerText:0) return 1;

    BuildEdit_ComputeLayout(playerid);
    new Float:x = gBeLayout[playerid][blX];
    new Float:y = gBeLayout[playerid][blY];
    new Float:panelW = gBeLayout[playerid][blW];
    new Float:panelH = gBeLayout[playerid][blH];
    new Float:btnW = gBeLayout[playerid][blBtnW];
    new Float:btnH = gBeLayout[playerid][blBtnH];
    new Float:rowStep = gBeLayout[playerid][blRowStep];
    new Float:infoStep = gBeLayout[playerid][blInfoStep];

    BuildEdit_CreateCard(playerid, x, y, panelW, panelH, 2.0);

    /* Garis aksen merah di tepi atas: ciri kartu FiveM, sekaligus penanda
       "ini mode bangun", bukan sekadar info. */
    BuildEditAccent[playerid] = CreatePlayerTextDraw(playerid, x + 2.0, y, "LD_SPAC:white");
    PlayerTextDrawFont(playerid, BuildEditAccent[playerid], TEXT_DRAW_FONT_SPRITE_DRAW);
    PlayerTextDrawTextSize(playerid, BuildEditAccent[playerid], panelW - 4.0, 2.0);
    PlayerTextDrawColor(playerid, BuildEditAccent[playerid], UI_ACCENT_DANGER);
    PlayerTextDrawSetSelectable(playerid, BuildEditAccent[playerid], false);

    BuildEditTitle[playerid] = CreatePlayerTextDraw(playerid, x + 12.0, y + 7.0, "PEMBANGUNAN");
    PlayerTextDrawFont(playerid, BuildEditTitle[playerid], 2);
    PlayerTextDrawLetterSize(playerid, BuildEditTitle[playerid], 0.22, 0.9);
    PlayerTextDrawColor(playerid, BuildEditTitle[playerid], UI_TEXT_PRIMARY);
    PlayerTextDrawSetOutline(playerid, BuildEditTitle[playerid], 1);
    PlayerTextDrawSetProportional(playerid, BuildEditTitle[playerid], true);
    PlayerTextDrawSetSelectable(playerid, BuildEditTitle[playerid], false);

    BuildEditSub[playerid] = CreatePlayerTextDraw(playerid, x + 12.0, y + 21.0, "WALL | TIER 1");
    PlayerTextDrawFont(playerid, BuildEditSub[playerid], 1);
    PlayerTextDrawLetterSize(playerid, BuildEditSub[playerid], 0.16, 0.8);
    PlayerTextDrawColor(playerid, BuildEditSub[playerid], UI_TEXT_MUTED);
    PlayerTextDrawSetOutline(playerid, BuildEditSub[playerid], 0);
    PlayerTextDrawSetProportional(playerid, BuildEditSub[playerid], true);
    PlayerTextDrawSetSelectable(playerid, BuildEditSub[playerid], false);

    BuildEditDivider[playerid] = CreatePlayerTextDraw(playerid, x + 12.0, y + 34.0, "LD_SPAC:white");
    PlayerTextDrawFont(playerid, BuildEditDivider[playerid], TEXT_DRAW_FONT_SPRITE_DRAW);
    PlayerTextDrawTextSize(playerid, BuildEditDivider[playerid], panelW - 24.0, 1.0);
    PlayerTextDrawColor(playerid, BuildEditDivider[playerid], UI_BG_ELEVATED);
    PlayerTextDrawSetSelectable(playerid, BuildEditDivider[playerid], false);

    for (new row = 0; row < 6; row++)
    {
        BuildEdit_CreateRow(playerid, row, x + 12.0, y + 40.0 + (float(row) * infoStep));
    }

    BuildEditHint[playerid] = CreatePlayerTextDraw(playerid, x + 12.0, y + gBeLayout[playerid][blHintY],
        "ESC keluar kursor | /panel lagi");
    PlayerTextDrawFont(playerid, BuildEditHint[playerid], 1);
    PlayerTextDrawLetterSize(playerid, BuildEditHint[playerid], 0.15, 0.75);
    PlayerTextDrawColor(playerid, BuildEditHint[playerid], UI_TEXT_MUTED);
    PlayerTextDrawSetOutline(playerid, BuildEditHint[playerid], 0);
    PlayerTextDrawSetProportional(playerid, BuildEditHint[playerid], true);
    PlayerTextDrawSetSelectable(playerid, BuildEditHint[playerid], false);

    /* Tombol sumbu: hitbox dari layout (PC 40x12, mobile 38x16), tidak bertumpuk. */
    new Float:leftX = x + gBeLayout[playerid][blColL];
    new Float:rightX = x + gBeLayout[playerid][blColR];
    new Float:rowY;

    for (new pair = 0; pair < 3; pair++)
    {
        rowY = y + 40.0 + (float(pair) * rowStep);
        BuildEdit_CreateButton(playerid, BE_BTN_XMINUS + (pair * 2), leftX, rowY, btnW, btnH, "-", UI_BG_ELEVATED);
        BuildEdit_CreateButton(playerid, BE_BTN_XPLUS + (pair * 2), rightX, rowY, btnW, btnH, "+", UI_BG_ELEVATED);
    }

    rowY = y + 40.0 + (3.0 * rowStep);
    BuildEdit_CreateButton(playerid, BE_BTN_ROTLEFT, leftX, rowY, btnW, btnH, "<", UI_BG_ELEVATED);
    BuildEdit_CreateButton(playerid, BE_BTN_ROTRIGHT, rightX, rowY, btnW, btnH, ">", UI_BG_ELEVATED);

    rowY = y + 40.0 + (4.0 * rowStep);
    BuildEdit_CreateButton(playerid, BE_BTN_DISTMIN, leftX, rowY, btnW, btnH, "-", UI_BG_ELEVATED);
    BuildEdit_CreateButton(playerid, BE_BTN_DISTPLUS, rightX, rowY, btnW, btnH, "+", UI_BG_ELEVATED);

    rowY = y + 40.0 + (5.0 * rowStep);
    BuildEdit_CreateButton(playerid, BE_BTN_STEP, leftX, rowY, btnW * 2.0 + 6.0, btnH, "LANGKAH", UI_BG_ELEVATED);

    BuildEdit_CreateButton(playerid, BE_BTN_CONFIRM, x + gBeLayout[playerid][blPasangX], y + gBeLayout[playerid][blPasangY],
        gBeLayout[playerid][blPasangW], gBeLayout[playerid][blPasangH], "PASANG", UI_ACCENT_PRIMARY);
    BuildEdit_CreateButton(playerid, BE_BTN_CANCEL, x + panelW - gBeLayout[playerid][blPasangW] - gBeLayout[playerid][blPasangX],
        y + gBeLayout[playerid][blPasangY], gBeLayout[playerid][blPasangW], gBeLayout[playerid][blPasangH], "BATAL", 0x7F1D1DF2);
    BuildEdit_CreateButton(playerid, BE_BTN_CLOSEMOUSE, x + panelW - 30.0, y + 6.0, 24.0, 14.0, "X", UI_BG_ELEVATED);
    BuildEdit_CreateButton(playerid, BE_BTN_SIZE, x + panelW - 74.0, y + 6.0, 40.0, 14.0, "UKURAN", UI_BG_ELEVATED);
    return 1;
}

/* Panel dihancurkan lalu dibuat ulang setiap ukuran berubah: geometri
   PlayerTextDraw tidak bisa diubah tanpa CreatePlayerTextDraw baru. */
stock BuildEdit_DestroyPanel(playerid)
{
    for (new card = 0; card < 6; card++)
    {
        if (BuildEditCard[playerid][card] != PlayerText:0)
        {
            PlayerTextDrawDestroy(playerid, BuildEditCard[playerid][card]);
        }
        BuildEditCard[playerid][card] = PlayerText:0;
    }

    if (BuildEditAccent[playerid] != PlayerText:0) PlayerTextDrawDestroy(playerid, BuildEditAccent[playerid]);
    if (BuildEditTitle[playerid] != PlayerText:0) PlayerTextDrawDestroy(playerid, BuildEditTitle[playerid]);
    if (BuildEditSub[playerid] != PlayerText:0) PlayerTextDrawDestroy(playerid, BuildEditSub[playerid]);
    if (BuildEditDivider[playerid] != PlayerText:0) PlayerTextDrawDestroy(playerid, BuildEditDivider[playerid]);
    if (BuildEditHint[playerid] != PlayerText:0) PlayerTextDrawDestroy(playerid, BuildEditHint[playerid]);

    BuildEditAccent[playerid] = PlayerText:0;
    BuildEditTitle[playerid] = PlayerText:0;
    BuildEditSub[playerid] = PlayerText:0;
    BuildEditDivider[playerid] = PlayerText:0;
    BuildEditHint[playerid] = PlayerText:0;

    for (new row = 0; row < 6; row++)
    {
        if (BuildEditRow[playerid][row] != PlayerText:0) PlayerTextDrawDestroy(playerid, BuildEditRow[playerid][row]);
        BuildEditRow[playerid][row] = PlayerText:0;
    }

    for (new button = 0; button < BE_BTN_COUNT; button++)
    {
        if (BuildEditBtn[playerid][button] != PlayerText:0) PlayerTextDrawDestroy(playerid, BuildEditBtn[playerid][button]);
        if (BuildEditBtnText[playerid][button] != PlayerText:0) PlayerTextDrawDestroy(playerid, BuildEditBtnText[playerid][button]);
        BuildEditBtn[playerid][button] = PlayerText:0;
        BuildEditBtnText[playerid][button] = PlayerText:0;
    }

    BuildEdit[playerid][bePanelShown] = false;
    return 1;
}

/* Setelah panel dibuat ulang, hitbox SelectTextDraw masih menunjuk TextDraw
   lama. Lepas lalu pilih ulang supaya tombol baru bisa diklik. */
stock BuildEdit_RebuildPanel(playerid)
{
    BuildEdit_DestroyPanel(playerid);
    BuildEdit_CreatePanel(playerid);
    BuildEdit_RefreshPanel(playerid);

    if (BuildEditPanelHidden[playerid]) return 1;

    if (BuildEdit[playerid][beMouseOn])
    {
        CancelSelectTextDraw(playerid);
        SelectTextDraw(playerid, 0xEF444488);
    }
    return 1;
}

stock BuildEdit_SetPanelHidden(playerid, bool:hidden)
{
    BuildEditPanelHidden[playerid] = hidden;

    if (hidden)
    {
        BuildEdit_HidePanel(playerid);
        SendClientMessage(playerid, UI_TEXT_MUTED, "[BUILD] Panel disembunyikan. /panel untuk memunculkan lagi.");
        return 1;
    }

    BuildEdit_RefreshPanel(playerid);
    SendClientMessage(playerid, UI_ACCENT_INFO, "[BUILD] Panel tampil lagi.");
    return 1;
}

stock BuildEdit_ScaleDown(playerid)
{
    if (!BuildEdit_IsActive(playerid)) return 0;

    new current = 0;
    for (new index = 0; index < BE_PANEL_SCALE_COUNT; index++)
    {
        if (floatabs(BE_PANEL_SCALES[index] - BuildEditPanelScale[playerid]) < 0.01) current = index;
    }

    if (current >= BE_PANEL_SCALE_COUNT - 1)
    {
        SendClientMessage(playerid, UI_TEXT_MUTED, "[BUILD] Panel sudah paling kecil.");
        return 1;
    }

    BuildEditPanelScale[playerid] = BE_PANEL_SCALES[current + 1];
    BuildEdit_RebuildPanel(playerid);

    new message[96];
    format(message, sizeof(message), "[BUILD] Ukuran panel %d%%.", floatround(BE_PANEL_SCALES[current + 1] * 100.0));
    SendClientMessage(playerid, UI_ACCENT_PRIMARY, message);
    return 1;
}

stock BuildEdit_ScaleUp(playerid)
{
    if (!BuildEdit_IsActive(playerid)) return 0;

    new current = BE_PANEL_SCALE_COUNT - 1;
    for (new index = 0; index < BE_PANEL_SCALE_COUNT; index++)
    {
        if (floatabs(BE_PANEL_SCALES[index] - BuildEditPanelScale[playerid]) < 0.01) current = index;
    }

    if (current <= 0)
    {
        SendClientMessage(playerid, UI_TEXT_MUTED, "[BUILD] Panel sudah paling besar.");
        return 1;
    }

    BuildEditPanelScale[playerid] = BE_PANEL_SCALES[current - 1];
    BuildEdit_RebuildPanel(playerid);

    new message[96];
    format(message, sizeof(message), "[BUILD] Ukuran panel %d%%.", floatround(BE_PANEL_SCALES[current - 1] * 100.0));
    SendClientMessage(playerid, UI_ACCENT_PRIMARY, message);
    return 1;
}

stock BuildEdit_CyclePanelScale(playerid)
{
    new current = 0;
    for (new index = 0; index < BE_PANEL_SCALE_COUNT; index++)
    {
        if (floatabs(BE_PANEL_SCALES[index] - BuildEditPanelScale[playerid]) < 0.01) current = index;
    }

    new next = (current + 1) % BE_PANEL_SCALE_COUNT;
    BuildEditPanelScale[playerid] = BE_PANEL_SCALES[next];
    BuildEdit_RebuildPanel(playerid);

    new message[96];
    format(message, sizeof(message), "[BUILD] Ukuran panel %d%%.", floatround(BE_PANEL_SCALES[next] * 100.0));
    SendClientMessage(playerid, UI_ACCENT_PRIMARY, message);
    return 1;
}

stock BuildEdit_ShowPanel(playerid)
{
    if (BuildEditPanelHidden[playerid]) return 0;

    for (new card = 0; card < 6; card++)
    {
        PlayerTextDrawShow(playerid, BuildEditCard[playerid][card]);
    }

    PlayerTextDrawShow(playerid, BuildEditAccent[playerid]);
    PlayerTextDrawShow(playerid, BuildEditTitle[playerid]);
    PlayerTextDrawShow(playerid, BuildEditSub[playerid]);
    PlayerTextDrawShow(playerid, BuildEditDivider[playerid]);
    PlayerTextDrawShow(playerid, BuildEditHint[playerid]);

    for (new row = 0; row < 6; row++)
    {
        PlayerTextDrawShow(playerid, BuildEditRow[playerid][row]);
    }

    for (new button = 0; button < BE_BTN_COUNT; button++)
    {
        PlayerTextDrawShow(playerid, BuildEditBtn[playerid][button]);
        PlayerTextDrawShow(playerid, BuildEditBtnText[playerid][button]);
    }
    return 1;
}

stock BuildEdit_HidePanel(playerid)
{
    if (playerid < 0 || playerid >= MAX_PLAYERS) return 0;

    if (BuildEdit[playerid][beMouseOn])
    {
        BuildEdit[playerid][beMouseOn] = false;
        CancelSelectTextDraw(playerid);
    }

    if (!BuildEdit[playerid][bePanelShown]) return 0;
    BuildEdit[playerid][bePanelShown] = false;

    for (new card = 0; card < 6; card++)
    {
        PlayerTextDrawHide(playerid, BuildEditCard[playerid][card]);
    }

    PlayerTextDrawHide(playerid, BuildEditAccent[playerid]);
    PlayerTextDrawHide(playerid, BuildEditTitle[playerid]);
    PlayerTextDrawHide(playerid, BuildEditSub[playerid]);
    PlayerTextDrawHide(playerid, BuildEditDivider[playerid]);
    PlayerTextDrawHide(playerid, BuildEditHint[playerid]);

    for (new row = 0; row < 6; row++)
    {
        PlayerTextDrawHide(playerid, BuildEditRow[playerid][row]);
    }

    for (new button = 0; button < BE_BTN_COUNT; button++)
    {
        PlayerTextDrawHide(playerid, BuildEditBtn[playerid][button]);
        PlayerTextDrawHide(playerid, BuildEditBtnText[playerid][button]);
    }
    return 1;
}

stock BuildEdit_SetMouse(playerid, bool:enabled)
{
    if (!BuildEdit_IsActive(playerid))
    {
        CancelSelectTextDraw(playerid);
        BuildEdit[playerid][beMouseOn] = false;
        return 0;
    }

    BuildEdit[playerid][beMouseOn] = enabled;

    if (enabled)
    {
        /* Warna hover: merah darah, konsisten dengan aksen kartu. */
        SelectTextDraw(playerid, 0xEF444488);
        PlayerTextDrawSetString(playerid, BuildEditHint[playerid], "X = tutup | /pasang atau tombol PASANG");
    }
    else
    {
        CancelSelectTextDraw(playerid);
        PlayerTextDrawSetString(playerid, BuildEditHint[playerid], "ALT maju | C putar | Y pasang");
    }
    PlayerTextDrawShow(playerid, BuildEditHint[playerid]);
    return 1;
}

/* ------------------------------------------------------------ geometri */

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

/* Target mode BANGUN: maju `beDist`, geser samping `beSide`, naik `beRaise`,
   semuanya relatif terhadap badan pemain. Arah hadap dipakai apa adanya supaya
   "putar badan = putar bangunan" tetap terasa natural, dan tombol rotasi
   menambahkan koreksi manual di atasnya. */
stock BuildEdit_FollowPlayer(playerid)
{
    if (BuildEdit[playerid][beMode] != BUILD_EDIT_PLACE) return 0;
    if (!IsPlayerConnected(playerid)) return 0;

    new Float:x, Float:y, Float:z, Float:angle;
    GetPlayerPos(playerid, x, y, z);
    GetPlayerFacingAngle(playerid, angle);

    new Float:distance = BuildEdit_ClampDistance(BuildEdit[playerid][beDist]);
    new Float:side = BuildEdit_ClampValue(BuildEdit[playerid][beSide], -BUILD_EDIT_MAX_SIDE, BUILD_EDIT_MAX_SIDE);
    BuildEdit[playerid][beDist] = distance;
    BuildEdit[playerid][beSide] = side;

    new Float:forwardX = floatsin(-angle, degrees);
    new Float:forwardY = floatcos(-angle, degrees);
    new Float:rightX = floatcos(-angle, degrees);
    new Float:rightY = -floatsin(-angle, degrees);

    BuildEdit[playerid][beX] = x + (forwardX * distance) + (rightX * side);
    BuildEdit[playerid][beY] = y + (forwardY * distance) + (rightY * side);
    BuildEdit[playerid][beZ] = z + BuildEdit[playerid][beRaise];
    BuildEdit[playerid][beRot] = BuildEdit_NormalizeRot(angle);
    return 1;
}

/* Pratinjau WAJIB memakai model yang sama dengan yang nanti dibangun.
   Dulu cabang stPickupModel membuat pratinjau peti tampil sebagai pickup
   koper, lalu hasil akhirnya tong minyak: dua benda berbeda untuk satu pilihan. */
stock BuildEdit_GhostModel(playerid, type)
{
    if (BuildEditModelOverride[playerid] > 0) return BuildEditModelOverride[playerid];
    return BaseBuild_Model(type);
}

stock BuildEdit_RefreshGhost(playerid, bool:forceStream = false)
{
    if (BuildEdit[playerid][beMode] == BUILD_EDIT_NONE) return 0;

    new Float:x = BuildEdit[playerid][beX];
    new Float:y = BuildEdit[playerid][beY];
    new Float:z = BuildEdit[playerid][beZ] - 0.7;
    new Float:rot = BuildEdit[playerid][beRot];

    /* Ghost harus hidup di world lobby pemain: world salah = pratinjau tak
       terlihat, pemain mengira fitur rusak. */
    new world = Lobby_WorldById(Lobby_Of(playerid));

    if (BuildEdit[playerid][beGhost] == STREAMER_TAG_OBJECT:0)
    {
        BuildEdit[playerid][beGhost] = CreateDynamicObject(BuildEdit_GhostModel(playerid, BuildEdit[playerid][beType]),
            x, y, z, 0.0, 0.0, rot, .worldid = world);
    }
    else
    {
        SetDynamicObjectPos(BuildEdit[playerid][beGhost], x, y, z);
        SetDynamicObjectRot(BuildEdit[playerid][beGhost], 0.0, 0.0, rot);
        /* Tanpa ini, posisi baru baru terkirim pada tick streamer berikutnya —
           pemain merasa object "tidak gerak real time" saat menekan tombol.
           Streamer_Update memaksa update object pemain ini SEKARANG. Dipanggil
           hanya saat ada input (tombol/kunci), bukan di tick 500 ms. */
        if (forceStream) Streamer_Update(playerid, STREAMER_TYPE_OBJECT);
    }

    BuildEdit_RefreshPanel(playerid);
    return 1;
}

/* Panel: setiap angka di sini adalah angka yang benar-benar dipakai saat
   PASANG, jadi tidak ada selisih "yang saya lihat" vs "yang berdiri". */
stock BuildEdit_RefreshPanel(playerid)
{
    if (BuildEdit[playerid][beMode] == BUILD_EDIT_NONE) return 0;
    if (!IsPlayerConnected(playerid)) return 0;

    BuildEdit_CreatePanel(playerid);

    new label[24], sub[72], rowText[64];
    BuildEdit_LabelFor(playerid, label, sizeof(label));

    new Float:stepMeters = BuildEdit_StepMeters(playerid);
    new Float:stepDegrees = BuildEdit_StepDegrees(playerid);

    /* Mobile: kolom info cuma ~104px (font 0.13) - pakai label ringkas
       supaya teks tidak meluber ke tombol < > (terbukti di galeri 13). */
    new bool:beMobile = (gBeLayout[playerid][blMobile] != 0);

    if (BuildEdit[playerid][beMode] == BUILD_EDIT_PLACE)
    {
        new tierNeed = gStructures[BuildEdit[playerid][beType]][stTier];
        new used = BaseBuild_CountType(Base_Id(playerid), BuildEdit[playerid][beType]);
        new limit = BaseBuild_Limit(BuildEdit[playerid][beType], Base_GetTier(playerid));
        format(sub, sizeof(sub), "%s | TIER %d | %d/%d", label, tierNeed, used, limit);

        if (beMobile)
        {
            format(rowText, sizeof(rowText), "SAMPING %s%.1f m",
                (BuildEdit[playerid][beSide] >= 0.0) ? "+" : "", BuildEdit[playerid][beSide]);
            PlayerTextDrawSetString(playerid, BuildEditRow[playerid][0], rowText);
            format(rowText, sizeof(rowText), "MAJU %.1f m", BuildEdit[playerid][beDist]);
            PlayerTextDrawSetString(playerid, BuildEditRow[playerid][1], rowText);
            format(rowText, sizeof(rowText), "NAIK %s%.1f m",
                (BuildEdit[playerid][beRaise] >= 0.0) ? "+" : "", BuildEdit[playerid][beRaise]);
            PlayerTextDrawSetString(playerid, BuildEditRow[playerid][2], rowText);
        }
        else
        {
        format(rowText, sizeof(rowText), "SAMPING %s%.2f m",
            (BuildEdit[playerid][beSide] >= 0.0) ? "+" : "", BuildEdit[playerid][beSide]);
        PlayerTextDrawSetString(playerid, BuildEditRow[playerid][0], rowText);
        format(rowText, sizeof(rowText), "MAJU    %.2f m", BuildEdit[playerid][beDist]);
        PlayerTextDrawSetString(playerid, BuildEditRow[playerid][1], rowText);
        format(rowText, sizeof(rowText), "NAIK    %s%.2f m",
            (BuildEdit[playerid][beRaise] >= 0.0) ? "+" : "", BuildEdit[playerid][beRaise]);
        PlayerTextDrawSetString(playerid, BuildEditRow[playerid][2], rowText);
        }
    }
    else
    {
        format(sub, sizeof(sub), "%s | MODE GESER | #%d", label, BuildEdit[playerid][beSlot]);

        if (beMobile)
        {
            format(rowText, sizeof(rowText), "X %.2f", BuildEdit[playerid][beX]);
            PlayerTextDrawSetString(playerid, BuildEditRow[playerid][0], rowText);
            format(rowText, sizeof(rowText), "Y %.2f", BuildEdit[playerid][beY]);
            PlayerTextDrawSetString(playerid, BuildEditRow[playerid][1], rowText);
            format(rowText, sizeof(rowText), "Z %.2f", BuildEdit[playerid][beZ]);
            PlayerTextDrawSetString(playerid, BuildEditRow[playerid][2], rowText);
        }
        else
        {
        format(rowText, sizeof(rowText), "X       %.2f", BuildEdit[playerid][beX]);
        PlayerTextDrawSetString(playerid, BuildEditRow[playerid][0], rowText);
        format(rowText, sizeof(rowText), "Y       %.2f", BuildEdit[playerid][beY]);
        PlayerTextDrawSetString(playerid, BuildEditRow[playerid][1], rowText);
        format(rowText, sizeof(rowText), "Z       %.2f", BuildEdit[playerid][beZ]);
        PlayerTextDrawSetString(playerid, BuildEditRow[playerid][2], rowText);
        }
    }

    if (beMobile)
    {
        format(rowText, sizeof(rowText), "ARAH %.0f deg", BuildEdit[playerid][beRot]);
        PlayerTextDrawSetString(playerid, BuildEditRow[playerid][3], rowText);

        format(rowText, sizeof(rowText), "JARAK %.1f m", BuildEdit[playerid][beDist]);
        PlayerTextDrawSetString(playerid, BuildEditRow[playerid][4], rowText);

        format(rowText, sizeof(rowText), "LANGKAH %.1f/%.0f", stepMeters, stepDegrees);
        PlayerTextDrawSetString(playerid, BuildEditRow[playerid][5], rowText);
    }
    else
    {
    format(rowText, sizeof(rowText), "ARAH    %.0f deg", BuildEdit[playerid][beRot]);
    PlayerTextDrawSetString(playerid, BuildEditRow[playerid][3], rowText);

    format(rowText, sizeof(rowText), "JARAK   %.2f m", BuildEdit[playerid][beDist]);
    PlayerTextDrawSetString(playerid, BuildEditRow[playerid][4], rowText);

    format(rowText, sizeof(rowText), "LANGKAH %.2f m / %.0f deg", stepMeters, stepDegrees);
    PlayerTextDrawSetString(playerid, BuildEditRow[playerid][5], rowText);
    }

    PlayerTextDrawSetString(playerid, BuildEditSub[playerid], sub);
    PlayerTextDrawSetString(playerid, BuildEditTitle[playerid], "PEMBANGUNAN");

    /* Warna tombol PASANG mengikuti kelayakan: hijau saat material cukup,
       merah saat tidak — pemain tahu sebelum menekan. */
    new confirmColor = UI_ACCENT_PRIMARY;
    if (BuildEdit[playerid][beMode] == BUILD_EDIT_PLACE)
    {
        if (!BaseBuild_MaterialsOk(playerid, BuildEdit[playerid][beType])) confirmColor = UI_ACCENT_DANGER;
    }
    else
    {
        confirmColor = UI_ACCENT_WARN;
    }
    PlayerTextDrawColor(playerid, BuildEditBtn[playerid][BE_BTN_CONFIRM], confirmColor);
    PlayerTextDrawShow(playerid, BuildEditBtn[playerid][BE_BTN_CONFIRM]);

    BuildEdit_ShowPanel(playerid);
    BuildEdit[playerid][bePanelShown] = true;
    return 1;
}

/* ------------------------------------------------------------ aksi */

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
    BuildEdit[playerid][beSide] = 0.0;
    BuildEdit[playerid][beRaise] = 0.0;
    BuildEdit[playerid][beStepStage] = 1;

    BuildEdit_FollowPlayer(playerid);
    BuildEdit_RefreshGhost(playerid);
    BuildEdit_SetMouse(playerid, true);

    SendClientMessage(playerid, UI_ACCENT_INFO, "[BUILD] Kursor aktif: klik tombol di panel. PASANG memotong material.");
    SendClientMessage(playerid, UI_TEXT_MUTED, "[BUILD] Tanpa mouse: ALT maju, C putar, Y pasang, H batal. /panel panggil kursor.");
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
    BuildEdit[playerid][beSide] = 0.0;
    BuildEdit[playerid][beRaise] = 0.0;
    BuildEdit[playerid][beStepStage] = 1;

    BuildEdit_RefreshGhost(playerid);
    BuildEdit_SetMouse(playerid, true);

    new label[24], line[144];
    BuildEdit_LabelFor(playerid, label, sizeof(label));
    format(line, sizeof(line), "[BUILD] Menggeser %s. PASANG menyimpan, BATAL membatalkan.", label);
    SendClientMessage(playerid, UI_ACCENT_INFO, line);
    return 1;
}

stock BuildEdit_AdjustDistance(playerid, Float:delta)
{
    if (BuildEdit[playerid][beMode] == BUILD_EDIT_NONE) return 0;

    if (BuildEdit[playerid][beMode] == BUILD_EDIT_MOVE)
    {
        return BuildEdit_Nudge(playerid, delta);
    }

    BuildEdit[playerid][beDist] = BuildEdit_ClampDistance(BuildEdit[playerid][beDist] + delta);
    BuildEdit_FollowPlayer(playerid);
    BuildEdit_RefreshGhost(playerid, true);
    return 1;
}

/* Mode GESER: dorong struktur sejauh `meters` mengikuti arah hadap pemain. */
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

    BuildEdit_RefreshGhost(playerid, true);
    return 1;
}

/* Tombol sumbu. Di mode BANGUN: X = samping, Y = maju/mundur, Z = naik/turun
   (relatif badan). Di mode GESER: X/Y/Z = koordinat dunia, sesuai yang tertulis
   di panel. */
stock BuildEdit_NudgeAxis(playerid, axis, Float:delta)
{
    if (BuildEdit[playerid][beMode] == BUILD_EDIT_NONE) return 0;

    if (BuildEdit[playerid][beMode] == BUILD_EDIT_PLACE)
    {
        switch (axis)
        {
            case 0: BuildEdit[playerid][beSide] = BuildEdit_ClampValue(
                BuildEdit[playerid][beSide] + delta, -BUILD_EDIT_MAX_SIDE, BUILD_EDIT_MAX_SIDE);
            case 1: BuildEdit[playerid][beDist] = BuildEdit_ClampDistance(BuildEdit[playerid][beDist] + delta);
            case 2: BuildEdit[playerid][beRaise] = BuildEdit_ClampValue(
                BuildEdit[playerid][beRaise] + delta, BUILD_EDIT_MIN_RAISE, BUILD_EDIT_MAX_RAISE);
        }
        BuildEdit_FollowPlayer(playerid);
        BuildEdit_RefreshGhost(playerid, true);
        return 1;
    }

    switch (axis)
    {
        case 0: BuildEdit[playerid][beX] += delta;
        case 1: BuildEdit[playerid][beY] += delta;
        case 2: BuildEdit[playerid][beZ] += delta;
    }

    new Float:px, Float:py, Float:pz;
    GetPlayerPos(playerid, px, py, pz);
    BuildEdit[playerid][beDist] = VectorSize(BuildEdit[playerid][beX] - px,
        BuildEdit[playerid][beY] - py, 0.0);

    BuildEdit_RefreshGhost(playerid, true);
    return 1;
}

stock BuildEdit_Rotate(playerid, Float:step)
{
    if (BuildEdit[playerid][beMode] == BUILD_EDIT_NONE) return 0;

    BuildEdit[playerid][beRot] = BuildEdit_NormalizeRot(BuildEdit[playerid][beRot] + step);
    BuildEdit_RefreshGhost(playerid, true);
    return 1;
}

stock BuildEdit_Raise(playerid, Float:meters)
{
    if (BuildEdit[playerid][beMode] != BUILD_EDIT_PLACE) return 0;

    BuildEdit[playerid][beRaise] = BuildEdit_ClampValue(
        BuildEdit[playerid][beRaise] + meters, BUILD_EDIT_MIN_RAISE, BUILD_EDIT_MAX_RAISE);
    BuildEdit_FollowPlayer(playerid);
    BuildEdit_RefreshGhost(playerid, true);
    return 1;
}

stock BuildEdit_CycleStep(playerid)
{
    if (BuildEdit[playerid][beMode] == BUILD_EDIT_NONE) return 0;

    BuildEdit[playerid][beStepStage] = (BuildEdit[playerid][beStepStage] + 1) % BE_STEP_COUNT;
    BuildEdit_RefreshPanel(playerid);
    return 1;
}

/* ------------------------------------------------------------ konfirmasi */

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
    format(line, sizeof(line), "%s dipindahkan ke %.1f, %.1f (arah %.0f).", label, x, y, rot);
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

/* ------------------------------------------------------------ klik & tick */

stock BuildEdit_FindButton(playerid, PlayerText:clicked)
{
    for (new button = 0; button < BE_BTN_COUNT; button++)
    {
        if (BuildEditBtn[playerid][button] == clicked) return button;
    }
    return -1;
}

stock BuildEdit_HandleButton(playerid, button)
{
    new Float:stepMeters = BuildEdit_StepMeters(playerid);
    new Float:stepDegrees = BuildEdit_StepDegrees(playerid);

    switch (button)
    {
        case BE_BTN_XMINUS:  BuildEdit_NudgeAxis(playerid, 0, -stepMeters);
        case BE_BTN_XPLUS:   BuildEdit_NudgeAxis(playerid, 0, stepMeters);
        case BE_BTN_YMINUS:  BuildEdit_NudgeAxis(playerid, 1, -stepMeters);
        case BE_BTN_YPLUS:   BuildEdit_NudgeAxis(playerid, 1, stepMeters);
        case BE_BTN_ZMINUS:  BuildEdit_NudgeAxis(playerid, 2, -stepMeters);
        case BE_BTN_ZPLUS:   BuildEdit_NudgeAxis(playerid, 2, stepMeters);
        case BE_BTN_ROTLEFT: BuildEdit_Rotate(playerid, -stepDegrees);
        case BE_BTN_ROTRIGHT: BuildEdit_Rotate(playerid, stepDegrees);
        case BE_BTN_DISTMIN: BuildEdit_AdjustDistance(playerid, -stepMeters);
        case BE_BTN_DISTPLUS: BuildEdit_AdjustDistance(playerid, stepMeters);
        case BE_BTN_STEP:    BuildEdit_CycleStep(playerid);
        case BE_BTN_CONFIRM: BuildEdit_Confirm(playerid);
        case BE_BTN_CANCEL:  BuildEdit_Cancel(playerid);
        case BE_BTN_CLOSEMOUSE: BuildEdit_Cancel(playerid);
        case BE_BTN_SIZE:    BuildEdit_CyclePanelScale(playerid);
        default: return 0;
    }
    return 1;
}

public OnPlayerClickPlayerTextDraw(playerid, PlayerText:playertextid)
{
    if (!BuildEdit_IsActive(playerid)) return 0;

    /* Hitbox yang tidak dikenal (mis. TextDraw milik UI lain) tidak ditelan,
       supaya dialog/inventory tetap bisa menerima kliknya sendiri. */
    new button = BuildEdit_FindButton(playerid, playertextid);
    if (button < 0) return 0;

    BuildEdit_HandleButton(playerid, button);
    return 1;
}

public OnPlayerClickTextDraw(playerid, Text:clickedid)
{
    if (!BuildEdit_IsActive(playerid)) return 0;

    /* ESC / klik area kosong: keluar dari mode kursor tapi pratinjau tetap
       hidup, jadi pemain bisa berjalan mengatur posisi lalu /panel lagi. */
    if (clickedid == INVALID_TEXT_DRAW && BuildEdit[playerid][beMouseOn])
    {
        BuildEdit_SetMouse(playerid, false);
        SendClientMessage(playerid, UI_TEXT_MUTED, "[BUILD] Kursor dilepas. /panel untuk memunculkannya lagi.");
        return 1;
    }
    return 1;
}

/* Mode BANGUN mengikuti pemain; mode GESER memakai posisi absolut supaya bisa
   diatur sambil berjalan mengelilingi struktur. */
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

/* Tombol fisik tetap jalan: ALT maju, C putar, Y pasang, H batal.
   Debounce 120 ms karena SA-MP mengirim key berulang selama ditahan. */
stock BuildEdit_OnKeyState(playerid, newkeys, oldkeys)
{
    if (BuildEdit[playerid][beMode] == BUILD_EDIT_NONE) return 0;
    if (GetTickCount() - BuildEdit[playerid][beLastKeyTick] < BUILD_EDIT_KEY_DEBOUNCE) return 1;

    if ((newkeys & KEY_WALK) && !(oldkeys & KEY_WALK))
    {
        BuildEdit[playerid][beLastKeyTick] = GetTickCount();
        BuildEdit_AdjustDistance(playerid, BuildEdit_StepMeters(playerid));
        return 1;
    }

    if ((newkeys & KEY_CROUCH) && !(oldkeys & KEY_CROUCH))
    {
        BuildEdit[playerid][beLastKeyTick] = GetTickCount();
        BuildEdit_Rotate(playerid, BuildEdit_StepDegrees(playerid));
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

/* /model <id>: ganti siluet pratinjau untuk sesi ini. Dipakai kalau model
   bawaan sebuah tipe tidak sesuai harapan; nilai tidak disimpan ke DB supaya
   tabel di config/structures.inc tetap sumber kebenaran. */
stock BuildEdit_SetModel(playerid, model)
{
    if (!BuildEdit_IsActive(playerid)) return 0;
    if (model <= 0 || model > 20000)
    {
        SendClientMessage(playerid, UI_ACCENT_WARN, "[BUILD] Id model tidak masuk akal.");
        return 0;
    }

    BuildEditModelOverride[playerid] = model;

    /* Ghost lama dimusnahkan supaya model baru benar-benar tercipta. */
    if (BuildEdit[playerid][beGhost] != STREAMER_TAG_OBJECT:0)
    {
        DestroyDynamicObject(BuildEdit[playerid][beGhost]);
        BuildEdit[playerid][beGhost] = STREAMER_TAG_OBJECT:0;
    }

    BuildEdit_RefreshGhost(playerid, true);

    new message[128];
    format(message, sizeof(message), "[BUILD] Siluet pratinjau -> model %d (sesi ini).", model);
    SendClientMessage(playerid, UI_ACCENT_PRIMARY, message);
    Log_Format(LOG_INFO, "[BUILD] model override player=%d type=%d model=%d",
        playerid, BuildEdit[playerid][beType], model);
    return 1;
}

/* Kandidat siluet: ukuran objek tidak bisa diubah di SA-MP 0.3.7 (tidak ada
   scale), jadi satu-satunya cara mengecilkan dinding adalah memilih model lain.
   Daftar ini memberi pilihan siap coba tanpa menunggu rebuild. */
stock BuildEdit_ShowModelList(playerid)
{
    SendClientMessage(playerid, UI_ACCENT_INFO, "[BUILD] Kandidat model (pakai /model <id>):");
    SendClientMessage(playerid, UI_TEXT_MUTED, "19447 panel dinding besar | 19477 papan tipis | 1495 pintu");
    SendClientMessage(playerid, UI_TEXT_MUTED, "970 sekat pendek | 1427 pembatas beton | 2932 peti | 1676 mesin");
    SendClientMessage(playerid, UI_TEXT_MUTED, "1225 tong | 1237 blok | 1431 ? | 1996 bangku | 1793 kasur");
    SendClientMessage(playerid, UI_TEXT_SECONDARY, "[BUILD] Model sekarang: lihat /model tanpa argumen.");
    return 1;
}

stock BuildEdit_ModelOf(playerid)
{
    return BuildEdit_GhostModel(playerid, BuildEdit[playerid][beType]);
}

/* /panel: panggil kursor tanpa mengubah posisi yang sudah diatur. */
stock BuildEdit_ToggleMouse(playerid)
{
    if (!BuildEdit_IsActive(playerid))
    {
        SendClientMessage(playerid, UI_ACCENT_INFO, "Tidak ada pratinjau. Mulai dengan /bangun atau /edit.");
        return 0;
    }

    BuildEdit_SetMouse(playerid, !BuildEdit[playerid][beMouseOn]);
    BuildEdit_RefreshPanel(playerid);
    return 1;
}
