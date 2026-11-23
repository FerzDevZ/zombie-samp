#if defined _ls_hud_included
    #endinput
#endif
#define _ls_hud_included

/* Preferensi /settings (settings.inc di-include setelah file ini). */
forward Float:Settings_HudScale(playerid);
forward bool:Settings_BarDetailEnabled(playerid);
forward bool:Settings_ContrastEnabled(playerid);
forward bool:Settings_PromptEnabled(playerid);
forward bool:HUD_BarVisible(playerid, index);
forward bool:Settings_TdMobileSafe(playerid);

/* ============================================================ HUD v2 ======
   Desain (audit UX 2026): teks polos tujuh baris terbaca seperti dump debug.
   v2 menggantinya dengan:
     - kartu kaca rounded (2 strip + 4 chit, pola sama dengan panel bangun)
     - bar HP/ARM/INF berlabel + 3 micro-bar kebutuhan (LAP/HAUS/PIP)
     - kolom kanan sejajar (SCRAP/SAMPEL dan BBM satu x, satu lebar)
     - prompt interaksi "[Y] <nama>" di bawah-tengah saat dekat hotspot
   Semua posisi divalidasi gHudFitRows (string TERBURUK) + generate.py. ==== */

new PlayerText:HudCard[MAX_PLAYERS][6];
new PlayerText:HudCardAccent[MAX_PLAYERS];
new PlayerText:HudBrand[MAX_PLAYERS];
new PlayerText:HudZone[MAX_PLAYERS];
new PlayerText:HudHpName[MAX_PLAYERS];
new PlayerText:HudHpVal[MAX_PLAYERS];
new PlayerText:HudArmName[MAX_PLAYERS];
new PlayerText:HudArmVal[MAX_PLAYERS];
new PlayerText:HudInfName[MAX_PLAYERS];
new PlayerText:HudInfVal[MAX_PLAYERS];
new PlayerText:HudLapName[MAX_PLAYERS];
new PlayerText:HudLapVal[MAX_PLAYERS];
new PlayerText:HudHausName[MAX_PLAYERS];
new PlayerText:HudHausVal[MAX_PLAYERS];
new PlayerText:HudPipName[MAX_PLAYERS];
new PlayerText:HudPipVal[MAX_PLAYERS];
new PlayerText:HudProgress[MAX_PLAYERS];
new PlayerText:HudLoadout[MAX_PLAYERS];
new PlayerText:HudVehicle[MAX_PLAYERS];
new PlayerText:HudBoss[MAX_PLAYERS];
new PlayerText:HudRaid[MAX_PLAYERS];
new PlayerText:HudPromptBg[MAX_PLAYERS];
new PlayerText:HudPromptText[MAX_PLAYERS];
new bool:PlayerHudVisible[MAX_PLAYERS];

/* Bar index: 0 HP, 1 ARM, 2 INF, 3 LAP, 4 HAUS, 5 PIP, 6 BBM/GENSET, 7 BOSS. */
#define HUD_BAR_HP     0
#define HUD_BAR_ARM    1
#define HUD_BAR_INF    2
#define HUD_BAR_LAP    3
#define HUD_BAR_HAUS   4
#define HUD_BAR_PIP    5
#define HUD_BAR_VEH    6
#define HUD_BAR_BOSS   7
#define HUD_BAR_COUNT  8

new PlayerText:HudBarBg[MAX_PLAYERS][HUD_BAR_COUNT];
new PlayerText:HudBarFg[MAX_PLAYERS][HUD_BAR_COUNT];
new HudBarLastWidth[MAX_PLAYERS][HUD_BAR_COUNT];
new HudBarLastColor[MAX_PLAYERS][HUD_BAR_COUNT];

/* Lebar/tinggi bar harus sama dengan angka di HUD_CreateBar — generate.py dan
   gallery.py membaca literal yang sama, jadi jangan ubah satu sisi saja. */
new const Float:gHudBarW[HUD_BAR_COUNT] = {104.0, 104.0, 104.0, 40.0, 40.0, 40.0, 118.0, 258.0};
new const Float:gHudBarH[HUD_BAR_COUNT] = {5.0, 5.0, 5.0, 4.0, 4.0, 4.0, 5.0, 6.0};

/* Bar bersegmen gaya game modern: fill kontinu dipotong strip warna latar.
   0 = kontinu (micro-bar). Maksimum 10 segmen -> paling banyak 9 celah.
   Bar yang bersegmen: HP, ARM, INF, BBM/GENSET, BOSS (urutan HUD_SEG_BARS). */
#define HUD_SEG_MAX_GAPS 9
#define HUD_SEG_GAP_W    1.6
new const gHudSegCount[5] = {10, 10, 10, 10, 10};
new PlayerText:HudBarGap[MAX_PLAYERS][5][HUD_SEG_MAX_GAPS];

#define HUD_PROMPT_MAX_LABEL 20
#define HUD_PROMPT_DISTANCE  1.5

/* Dirty-flag render: TextDrawSetString/Show hanya dipanggil kalau nilai
   berubah. Refresh penuh 500 ms x 200 pemain = ribuan paket TD sia-sia. */
new HudLastZone[MAX_PLAYERS][48];
new HudLastHpVal[MAX_PLAYERS][12];
new HudLastArmVal[MAX_PLAYERS][12];
new HudLastInfVal[MAX_PLAYERS][8];
new HudLastLapVal[MAX_PLAYERS][8];
new HudLastHausVal[MAX_PLAYERS][8];
new HudLastPipVal[MAX_PLAYERS][8];
new HudLastLoadout[MAX_PLAYERS][48];
new HudLastProgress[MAX_PLAYERS][64];
new HudLastVehicle[MAX_PLAYERS][64];
new HudLastPrompt[MAX_PLAYERS][40];
new HudLastBoss[MAX_PLAYERS][64];
new HudLastRaid[MAX_PLAYERS][64];
new HudLastInfectColor[MAX_PLAYERS];
new bool:HudVehicleShown[MAX_PLAYERS];
new bool:HudVehBarShown[MAX_PLAYERS];
new HudFuelColor[MAX_PLAYERS];
new HudFuelWarnStage[MAX_PLAYERS];
new bool:HudBossShown[MAX_PLAYERS];
new bool:HudRaidShown[MAX_PLAYERS];
new bool:HudPromptShown[MAX_PLAYERS];

stock HUD_SetIfChanged(playerid, PlayerText:td, const text[], lastText[], lastSize)
{
    if (!strcmp(text, lastText, false)) return 0;
    /* strcpy, bukan format("%s"): jalur panas 2400 salinan/dtk (audit F7). */
    strcpy(lastText, text, lastSize);
    PlayerTextDrawSetString(playerid, td, text);
    return 1;
}

/* TextDraw tidak punya perataan kanan, jadi setiap baris HUD hanya aman kalau
   diuji dengan string TERBURUK-nya, bukan placeholder pendek. Dua bug lama
   (gauge BBM 560->440, muatan 560->496, BBM 440->400) semuanya lolos karena
   tidak ada gate ini. Rasio 35.3 px per LetterSize X dipakai juga oleh
   tools/hud_preview/generate.py; tabel di bawah HARUS sama dengan angka di
   HUD_Create dan skrip itu membandingkan keduanya (gagal bila menyimpang). */
#define HUD_SCREEN_WIDTH 640.0
#define HUD_CHAR_RATIO   35.3

/* Tiga gaya HUD (docs/UIUX_PLAN.md). Mobile terdeteksi otomatis dari versi
   klien; pemain tetap bisa override lewat /hudgaya 1|2|3. Skala menyusutkan
   SEMUA koordinat menuju pojok kiri-ATAS sehingga kartu keluar dari zona
   joystick (y 288 -> 202) dan tombol sentuh kanan-bawah di Android. Elemen
   yang harus tetap di tengah layar memakai HUD_XC. */
/* Konstanta gaya + state global ada di config/constants.inc (dibutuhkan
   build_edit.inc yang di-include lebih awal). */


stock Float:HUD_ScaleOf(playerid)
{
    switch (gHudGaya[playerid])
    {
        case HUD_GAYA_MOBILE: return HUD_SCALE_MOBILE * Settings_HudScale(playerid);
        case HUD_GAYA_MINIMAL: return HUD_SCALE_MINIMAL * Settings_HudScale(playerid);
    }
    return 1.0 * Settings_HudScale(playerid);
}

/* Koordinat biasa: menyusut menuju pojok kiri-atas. */
stock Float:HUD_X(playerid, Float:x) { return x * HUD_ScaleOf(playerid); }
stock Float:HUD_Y(playerid, Float:y) { return y * HUD_ScaleOf(playerid); }

/* Koordinat elemen center (boss/raid/prompt): tetap di tengah horizontal. */
stock Float:HUD_XC(playerid, Float:x)
{
    return 320.0 - (320.0 - x) * HUD_ScaleOf(playerid);
}

/* LetterSize/ukuran ikut skala. */
stock Float:HUD_F(playerid, Float:value) { return value * HUD_ScaleOf(playerid); }

/* Gaya minimalis: tanpa kartu kaca dan tanpa kotak — hanya bar + angka. */
stock bool:HUD_GayaHasCard(playerid) { return gHudGaya[playerid] != HUD_GAYA_MINIMAL; }
stock bool:HUD_GayaHasBox(playerid) { return gHudGaya[playerid] != HUD_GAYA_MINIMAL; }

enum E_HUD_FIT
{
    hudFitName[16],
    Float:hudFitX,
    Float:hudFitSizeX,
    hudFitWorstLen
};

new const gHudFitRows[][E_HUD_FIT] =
{
    { "HudBrand",      26.0, 0.20, 10 },
    { "HudZone",      150.0, 0.16, 22 },
    { "HudHpName",     26.0, 0.16,  2 },
    { "HudHpVal",     162.0, 0.16,  7 },
    { "HudArmName",    26.0, 0.16,  3 },
    { "HudArmVal",    162.0, 0.16,  7 },
    { "HudInfName",    26.0, 0.16,  3 },
    { "HudInfVal",    162.0, 0.16,  4 },
    { "HudLapName",    26.0, 0.15,  3 },
    { "HudLapVal",     92.0, 0.15,  3 },
    { "HudHausName",  112.0, 0.15,  4 },
    { "HudHausVal",   178.0, 0.15,  3 },
    { "HudPipName",   198.0, 0.15,  3 },
    { "HudPipVal",    264.0, 0.15,  3 },
    { "HudProgress",   26.0, 0.18, 30 },
    { "HudLoadout",   352.0, 0.20, 22 },
    { "HudVehicle",   352.0, 0.17, 27 },
    { "HudBoss",      210.0, 0.26, 16 },
    { "HudRaid",      190.0, 0.24, 25 },
    { "HudPromptText",320.0, 0.18, 23 }
};

stock bool:HUD_WorstCaseFits(offender[], size)
{
    offender[0] = EOS;
    for (new row = 0; row < sizeof(gHudFitRows); row++)
    {
        new Float:width = float(gHudFitRows[row][hudFitWorstLen])
            * gHudFitRows[row][hudFitSizeX] * HUD_CHAR_RATIO;
        if (gHudFitRows[row][hudFitX] + width <= HUD_SCREEN_WIDTH) continue;
        format(offender, size, "%s", gHudFitRows[row][hudFitName]);
        return false;
    }
    return true;
}

/* Permukaan kotak polos: satu textdraw box, tanpa sprite. Dipakai saat mode
   mobile-safe aktif (permintaan pemain: layout Android berantakan/tumpang). */
stock PlayerText:TD_BoxCreate(playerid, Float:x, Float:y, Float:width, Float:height, color)
{
    new PlayerText:td = CreatePlayerTextDraw(playerid, x, y, " ");
    PlayerTextDrawFont(playerid, td, 1);
    PlayerTextDrawLetterSize(playerid, td, 0.0, 0.0);
    PlayerTextDrawTextSize(playerid, td, width, height);
    PlayerTextDrawColor(playerid, td, 0xFFFFFFFF);
    PlayerTextDrawBackgroundColor(playerid, td, 0x00000000);
    PlayerTextDrawUseBox(playerid, td, 1);
    PlayerTextDrawBoxColor(playerid, td, color);
    PlayerTextDrawSetOutline(playerid, td, 0);
    PlayerTextDrawSetShadow(playerid, td, 0);
    PlayerTextDrawSetSelectable(playerid, td, false);
    return td;
}

/* Kartu kaca rounded: 2 strip LD_SPAC + 4 chit sudut. Persis pola panel bangun
   (BuildEdit_CreateCard) — PlayerTextDrawBoxColor TIDAK punya sudut bulat.
   Mode mobile-safe: satu box utuh, 5 slot sisa dibuat berukuran nol supaya
   pola Show/Hide/Destroy (yang mengiterasi 6 slot) tetap sah. */
stock HUD_CreateCard(playerid, Float:x, Float:y, Float:width, Float:height, Float:radius)
{
    if (Settings_TdMobileSafe(playerid))
    {
        HudCard[playerid][0] = TD_BoxCreate(playerid, x, y, width, height, UI_BG_GLASS_DARK);
        for (new slot = 1; slot < 6; slot++)
        {
            HudCard[playerid][slot] = TD_BoxCreate(playerid, x, y, 0.0, 0.0, 0x00000000);
        }
        return 1;
    }

    new Float:rx = radius * TD_ASPECT_16_9;
    new Float:diamX = rx * 2.0;
    new Float:diamY = radius * 2.0;

    HudCard[playerid][0] = CreatePlayerTextDraw(playerid, x, y + radius, "LD_SPAC:white");
    PlayerTextDrawFont(playerid, HudCard[playerid][0], TEXT_DRAW_FONT_SPRITE_DRAW);
    PlayerTextDrawTextSize(playerid, HudCard[playerid][0], width, height - diamY);
    PlayerTextDrawColor(playerid, HudCard[playerid][0], UI_BG_GLASS_DARK);
    PlayerTextDrawSetSelectable(playerid, HudCard[playerid][0], false);

    HudCard[playerid][1] = CreatePlayerTextDraw(playerid, x + rx, y, "LD_SPAC:white");
    PlayerTextDrawFont(playerid, HudCard[playerid][1], TEXT_DRAW_FONT_SPRITE_DRAW);
    PlayerTextDrawTextSize(playerid, HudCard[playerid][1], width - diamX, height);
    PlayerTextDrawColor(playerid, HudCard[playerid][1], UI_BG_GLASS_DARK);
    PlayerTextDrawSetSelectable(playerid, HudCard[playerid][1], false);

    new Float:cornerX[4];
    new Float:cornerY[4];
    cornerX[0] = x;                      cornerY[0] = y;
    cornerX[1] = x + width - diamX;      cornerY[1] = y;
    cornerX[2] = x;                      cornerY[2] = y + height - diamY;
    cornerX[3] = x + width - diamX;      cornerY[3] = y + height - diamY;

    for (new corner = 0; corner < 4; corner++)
    {
        HudCard[playerid][2 + corner] = CreatePlayerTextDraw(playerid, cornerX[corner], cornerY[corner], "LD_BEAT:chit");
        PlayerTextDrawFont(playerid, HudCard[playerid][2 + corner], TEXT_DRAW_FONT_SPRITE_DRAW);
        PlayerTextDrawTextSize(playerid, HudCard[playerid][2 + corner], diamX, diamY);
        PlayerTextDrawColor(playerid, HudCard[playerid][2 + corner], UI_BG_GLASS_DARK);
        PlayerTextDrawSetSelectable(playerid, HudCard[playerid][2 + corner], false);
    }
    return 1;
}

stock HUD_CreateBar(playerid, index, Float:x, Float:y, trackColor, gapColor, segSlot = -1)
{
    new Float:barW = gHudBarW[index] * HUD_ScaleOf(playerid);
    new Float:barH = gHudBarH[index] * HUD_ScaleOf(playerid);
    new Float:gapW = HUD_SEG_GAP_W * HUD_ScaleOf(playerid);

    /* Mobile-safe: bar = dua box. Lebar isi diatur HUD_SetBar lewat TextSize. */
    if (Settings_TdMobileSafe(playerid))
    {
        HudBarBg[playerid][index] = TD_BoxCreate(playerid, x, y, barW, barH, trackColor);
        HudBarFg[playerid][index] = TD_BoxCreate(playerid, x, y, 0.0, barH, gapColor);
        HudBarLastWidth[playerid][index] = -1;
        HudBarLastColor[playerid][index] = 0;
        if (segSlot >= 0)
        {
            new segments = gHudSegCount[segSlot];
            new Float:step = barW / float(segments);
            for (new cut = 1; cut < segments && cut <= HUD_SEG_MAX_GAPS; cut++)
            {
                new Float:gapX = x + float(cut) * step - gapW * 0.5;
                HudBarGap[playerid][segSlot][cut - 1] = TD_BoxCreate(playerid, gapX, y, gapW, barH, gapColor);
            }
        }
        return 1;
    }

    HudBarBg[playerid][index] = CreatePlayerTextDraw(playerid, x, y, "LD_SPAC:white");
    PlayerTextDrawFont(playerid, HudBarBg[playerid][index], TEXT_DRAW_FONT_SPRITE_DRAW);
    PlayerTextDrawTextSize(playerid, HudBarBg[playerid][index], barW, barH);
    PlayerTextDrawColor(playerid, HudBarBg[playerid][index], trackColor);
    PlayerTextDrawSetSelectable(playerid, HudBarBg[playerid][index], false);

    HudBarFg[playerid][index] = CreatePlayerTextDraw(playerid, x, y, "LD_SPAC:white");
    PlayerTextDrawFont(playerid, HudBarFg[playerid][index], TEXT_DRAW_FONT_SPRITE_DRAW);
    PlayerTextDrawTextSize(playerid, HudBarFg[playerid][index], 0.0, barH);
    PlayerTextDrawColor(playerid, HudBarFg[playerid][index], UI_ACCENT_PRIMARY);
    PlayerTextDrawSetSelectable(playerid, HudBarFg[playerid][index], false);

    /* Celah segmen dibuat SETELAH fill supaya tergambar di atasnya. Warna celah
       = warna latar bar (kartu / kotak BBM / dunia) supaya terlihat seperti
       potongan, bukan garis tambahan. */
    if (segSlot >= 0)
    {
        new segments = gHudSegCount[segSlot];
        new Float:step = barW / float(segments);
        for (new cut = 1; cut < segments && cut <= HUD_SEG_MAX_GAPS; cut++)
        {
            new Float:gapX = x + float(cut) * step - gapW * 0.5;
            HudBarGap[playerid][segSlot][cut - 1] = CreatePlayerTextDraw(playerid, gapX, y, "LD_SPAC:white");
            PlayerTextDrawFont(playerid, HudBarGap[playerid][segSlot][cut - 1], TEXT_DRAW_FONT_SPRITE_DRAW);
            PlayerTextDrawTextSize(playerid, HudBarGap[playerid][segSlot][cut - 1], gapW, barH);
            PlayerTextDrawColor(playerid, HudBarGap[playerid][segSlot][cut - 1], gapColor);
            PlayerTextDrawSetSelectable(playerid, HudBarGap[playerid][segSlot][cut - 1], false);
        }
    }

    HudBarLastWidth[playerid][index] = 0;
    HudBarLastColor[playerid][index] = UI_ACCENT_PRIMARY;
    return 1;
}

/* Bar kondisional (BBM/GENSET, boss) harus tampil-hilang BERSAMA konteksnya —
   bar yang mengambang tanpa label adalah sampah visual. segSlot < 0 = tanpa celah. */
stock HUD_ShowBar(playerid, index, segSlot = -1)
{
    if (!HUD_BarVisible(playerid, index)) return 0;
    PlayerTextDrawShow(playerid, HudBarBg[playerid][index]);
    PlayerTextDrawShow(playerid, HudBarFg[playerid][index]);
    if (segSlot >= 0)
    {
        for (new cut = 0; cut < HUD_SEG_MAX_GAPS; cut++)
        {
            if (HudBarGap[playerid][segSlot][cut] != PlayerText:0)
                PlayerTextDrawShow(playerid, HudBarGap[playerid][segSlot][cut]);
        }
    }
    return 1;
}

/* Bar detail (kendaraan & boss) bisa disembunyikan lewat /settings; bar
   lain selalu tampil. Guard mencegah sentuhan ke textdraw yang tidak dibuat. */
stock bool:HUD_BarVisible(playerid, index)
{
    if (index == HUD_BAR_VEH || index == HUD_BAR_BOSS)
    {
        return Settings_BarDetailEnabled(playerid);
    }
    return true;
}

stock HUD_HideBar(playerid, index, segSlot = -1)
{
    if (!HUD_BarVisible(playerid, index)) return 0;
    PlayerTextDrawHide(playerid, HudBarBg[playerid][index]);
    PlayerTextDrawHide(playerid, HudBarFg[playerid][index]);
    if (segSlot >= 0)
    {
        for (new cut = 0; cut < HUD_SEG_MAX_GAPS; cut++)
        {
            if (HudBarGap[playerid][segSlot][cut] != PlayerText:0)
                PlayerTextDrawHide(playerid, HudBarGap[playerid][segSlot][cut]);
        }
    }
    return 1;
}

/* Lebar bar berubah hanya saat piksel berubah; warna hanya saat berbeda.
   TextSize 0 menyembunyikan sprite (bar kosong) tanpa Hide/Show ping-pong. */
stock HUD_SetBar(playerid, index, Float:ratio, color)
{
    /* Mode kontras tinggi (/settings): merah & oranye bar diganti warna
       yang lebih terbaca bagi mata lelah / buta warna parsial. */
    if (Settings_ContrastEnabled(playerid))
    {
        if (color == UI_ACCENT_DANGER) color = 0xFF9500FF;
        else if (color == UI_ACCENT_WARN) color = 0xFFD60AFF;
    }
    if (!HUD_BarVisible(playerid, index)) return 0;
    if (ratio < 0.0) ratio = 0.0;
    if (ratio > 1.0) ratio = 1.0;

    new width = floatround(gHudBarW[index] * HUD_ScaleOf(playerid) * ratio);
    if (width != HudBarLastWidth[playerid][index])
    {
        HudBarLastWidth[playerid][index] = width;
        PlayerTextDrawTextSize(playerid, HudBarFg[playerid][index], float(width),
            gHudBarH[index] * HUD_ScaleOf(playerid));
        PlayerTextDrawShow(playerid, HudBarFg[playerid][index]);
    }
    if (color != HudBarLastColor[playerid][index])
    {
        HudBarLastColor[playerid][index] = color;
        PlayerTextDrawColor(playerid, HudBarFg[playerid][index], color);
        if (Settings_TdMobileSafe(playerid))
        {
            /* Box textdraw: warna isi ada di BoxColor, bukan text color. */
            PlayerTextDrawBoxColor(playerid, HudBarFg[playerid][index], color);
        }
        PlayerTextDrawShow(playerid, HudBarFg[playerid][index]);
    }
    return 1;
}

stock HUD_Create(playerid)
{
    /* Gaya dipilih sekali per sesi: otomatis mobile bila klien Android,
       override lewat /hudgaya tetap hidup sampai disconnect. */
    if (gHudGaya[playerid] == 0)
    {
        gHudGaya[playerid] = gHudMobileClient[playerid] ? HUD_GAYA_MOBILE : HUD_GAYA_DEFAULT;
    }

    /* Kartu kiri: x 16..304, y 288..388 — di atas panel bangun (y>=330 kanan).
       Skala menyusutkan semua koordinat menuju kiri-atas (mobile aman). */
    if (HUD_GayaHasCard(playerid))
    {
        HUD_CreateCard(playerid, HUD_X(playerid, 16.0), HUD_Y(playerid, 288.0),
            HUD_F(playerid, 288.0), HUD_F(playerid, 100.0), HUD_F(playerid, 2.0));
    }

    if (HUD_GayaHasCard(playerid))
    {
        if (Settings_TdMobileSafe(playerid))
        {
            HudCardAccent[playerid] = TD_BoxCreate(playerid, HUD_X(playerid, 18.0), HUD_Y(playerid, 289.0),
                HUD_F(playerid, 3.0), HUD_F(playerid, 98.0), UI_ACCENT_PRIMARY);
        }
        else HudCardAccent[playerid] = CreatePlayerTextDraw(playerid, HUD_X(playerid, 18.0), HUD_Y(playerid, 288.0), "LD_SPAC:white");
        PlayerTextDrawFont(playerid, HudCardAccent[playerid], TEXT_DRAW_FONT_SPRITE_DRAW);
        PlayerTextDrawTextSize(playerid, HudCardAccent[playerid], HUD_F(playerid, 284.0), HUD_F(playerid, 2.0));
        PlayerTextDrawColor(playerid, HudCardAccent[playerid], UI_ACCENT_PRIMARY);
        PlayerTextDrawSetSelectable(playerid, HudCardAccent[playerid], false);
    }

    HudBrand[playerid] = CreatePlayerTextDraw(playerid, HUD_X(playerid, 26.0), HUD_Y(playerid, 294.0), "LAST_STAND");
    PlayerTextDrawLetterSize(playerid, HudBrand[playerid], HUD_F(playerid, 0.20), HUD_F(playerid, 0.85));
    PlayerTextDrawColor(playerid, HudBrand[playerid], UI_TEXT_PRIMARY);
    PlayerTextDrawSetOutline(playerid, HudBrand[playerid], 1);
    PlayerTextDrawFont(playerid, HudBrand[playerid], 2);

    /* Zona pindah ke baris brand: hemat satu baris, tetap terbaca. */
    HudZone[playerid] = CreatePlayerTextDraw(playerid, HUD_X(playerid, 150.0), HUD_Y(playerid, 296.0), "ZONE");
    PlayerTextDrawLetterSize(playerid, HudZone[playerid], HUD_F(playerid, 0.16), HUD_F(playerid, 0.75));
    PlayerTextDrawColor(playerid, HudZone[playerid], UI_TEXT_SECONDARY);
    PlayerTextDrawSetOutline(playerid, HudZone[playerid], 1);
    PlayerTextDrawFont(playerid, HudZone[playerid], 1);

    HudHpName[playerid] = CreatePlayerTextDraw(playerid, HUD_X(playerid, 26.0), HUD_Y(playerid, 310.0), "HP");
    PlayerTextDrawLetterSize(playerid, HudHpName[playerid], HUD_F(playerid, 0.16), HUD_F(playerid, 0.8));
    PlayerTextDrawColor(playerid, HudHpName[playerid], UI_TEXT_SECONDARY);
    PlayerTextDrawSetOutline(playerid, HudHpName[playerid], 1);
    PlayerTextDrawFont(playerid, HudHpName[playerid], 1);

    HudArmName[playerid] = CreatePlayerTextDraw(playerid, HUD_X(playerid, 26.0), HUD_Y(playerid, 322.0), "ARM");
    PlayerTextDrawLetterSize(playerid, HudArmName[playerid], HUD_F(playerid, 0.16), HUD_F(playerid, 0.8));
    PlayerTextDrawColor(playerid, HudArmName[playerid], UI_TEXT_SECONDARY);
    PlayerTextDrawSetOutline(playerid, HudArmName[playerid], 1);
    PlayerTextDrawFont(playerid, HudArmName[playerid], 1);

    HudInfName[playerid] = CreatePlayerTextDraw(playerid, HUD_X(playerid, 26.0), HUD_Y(playerid, 334.0), "INF");
    PlayerTextDrawLetterSize(playerid, HudInfName[playerid], HUD_F(playerid, 0.16), HUD_F(playerid, 0.8));
    PlayerTextDrawColor(playerid, HudInfName[playerid], UI_TEXT_SECONDARY);
    PlayerTextDrawSetOutline(playerid, HudInfName[playerid], 1);
    PlayerTextDrawFont(playerid, HudInfName[playerid], 1);

    HudHpVal[playerid] = CreatePlayerTextDraw(playerid, HUD_X(playerid, 162.0), HUD_Y(playerid, 310.0), "100/100");
    PlayerTextDrawLetterSize(playerid, HudHpVal[playerid], HUD_F(playerid, 0.16), HUD_F(playerid, 0.8));
    PlayerTextDrawColor(playerid, HudHpVal[playerid], UI_TEXT_PRIMARY);
    PlayerTextDrawSetOutline(playerid, HudHpVal[playerid], 1);
    PlayerTextDrawFont(playerid, HudHpVal[playerid], 1);

    HudArmVal[playerid] = CreatePlayerTextDraw(playerid, HUD_X(playerid, 162.0), HUD_Y(playerid, 322.0), "0/100");
    PlayerTextDrawLetterSize(playerid, HudArmVal[playerid], HUD_F(playerid, 0.16), HUD_F(playerid, 0.8));
    PlayerTextDrawColor(playerid, HudArmVal[playerid], UI_TEXT_PRIMARY);
    PlayerTextDrawSetOutline(playerid, HudArmVal[playerid], 1);
    PlayerTextDrawFont(playerid, HudArmVal[playerid], 1);

    HudInfVal[playerid] = CreatePlayerTextDraw(playerid, HUD_X(playerid, 162.0), HUD_Y(playerid, 334.0), "0%");
    PlayerTextDrawLetterSize(playerid, HudInfVal[playerid], HUD_F(playerid, 0.16), HUD_F(playerid, 0.8));
    PlayerTextDrawColor(playerid, HudInfVal[playerid], UI_TEXT_PRIMARY);
    PlayerTextDrawSetOutline(playerid, HudInfVal[playerid], 1);
    PlayerTextDrawFont(playerid, HudInfVal[playerid], 1);

    HudLapName[playerid] = CreatePlayerTextDraw(playerid, HUD_X(playerid, 26.0), HUD_Y(playerid, 346.0), "LAP");
    PlayerTextDrawLetterSize(playerid, HudLapName[playerid], HUD_F(playerid, 0.15), HUD_F(playerid, 0.75));
    PlayerTextDrawColor(playerid, HudLapName[playerid], UI_TEXT_SECONDARY);
    PlayerTextDrawSetOutline(playerid, HudLapName[playerid], 1);
    PlayerTextDrawFont(playerid, HudLapName[playerid], 1);

    HudHausName[playerid] = CreatePlayerTextDraw(playerid, HUD_X(playerid, 112.0), HUD_Y(playerid, 346.0), "HAUS");
    PlayerTextDrawLetterSize(playerid, HudHausName[playerid], HUD_F(playerid, 0.15), HUD_F(playerid, 0.75));
    PlayerTextDrawColor(playerid, HudHausName[playerid], UI_TEXT_SECONDARY);
    PlayerTextDrawSetOutline(playerid, HudHausName[playerid], 1);
    PlayerTextDrawFont(playerid, HudHausName[playerid], 1);

    HudPipName[playerid] = CreatePlayerTextDraw(playerid, HUD_X(playerid, 198.0), HUD_Y(playerid, 346.0), "PIP");
    PlayerTextDrawLetterSize(playerid, HudPipName[playerid], HUD_F(playerid, 0.15), HUD_F(playerid, 0.75));
    PlayerTextDrawColor(playerid, HudPipName[playerid], UI_TEXT_SECONDARY);
    PlayerTextDrawSetOutline(playerid, HudPipName[playerid], 1);
    PlayerTextDrawFont(playerid, HudPipName[playerid], 1);

    HudLapVal[playerid] = CreatePlayerTextDraw(playerid, HUD_X(playerid, 92.0), HUD_Y(playerid, 346.0), "100");
    PlayerTextDrawLetterSize(playerid, HudLapVal[playerid], HUD_F(playerid, 0.15), HUD_F(playerid, 0.75));
    PlayerTextDrawColor(playerid, HudLapVal[playerid], UI_TEXT_PRIMARY);
    PlayerTextDrawSetOutline(playerid, HudLapVal[playerid], 1);
    PlayerTextDrawFont(playerid, HudLapVal[playerid], 1);

    HudHausVal[playerid] = CreatePlayerTextDraw(playerid, HUD_X(playerid, 178.0), HUD_Y(playerid, 346.0), "100");
    PlayerTextDrawLetterSize(playerid, HudHausVal[playerid], HUD_F(playerid, 0.15), HUD_F(playerid, 0.75));
    PlayerTextDrawColor(playerid, HudHausVal[playerid], UI_TEXT_PRIMARY);
    PlayerTextDrawSetOutline(playerid, HudHausVal[playerid], 1);
    PlayerTextDrawFont(playerid, HudHausVal[playerid], 1);

    HudPipVal[playerid] = CreatePlayerTextDraw(playerid, HUD_X(playerid, 264.0), HUD_Y(playerid, 346.0), "0");
    PlayerTextDrawLetterSize(playerid, HudPipVal[playerid], HUD_F(playerid, 0.15), HUD_F(playerid, 0.75));
    PlayerTextDrawColor(playerid, HudPipVal[playerid], UI_TEXT_PRIMARY);
    PlayerTextDrawSetOutline(playerid, HudPipVal[playerid], 1);
    PlayerTextDrawFont(playerid, HudPipVal[playerid], 1);

    HUD_CreateBar(playerid, HUD_BAR_HP, HUD_X(playerid, 52.0), HUD_Y(playerid, 312.0), UI_BG_ELEVATED, UI_BG_GLASS_DARK, 0);
    HUD_CreateBar(playerid, HUD_BAR_ARM, HUD_X(playerid, 52.0), HUD_Y(playerid, 324.0), UI_BG_ELEVATED, UI_BG_GLASS_DARK, 1);
    HUD_CreateBar(playerid, HUD_BAR_INF, HUD_X(playerid, 52.0), HUD_Y(playerid, 336.0), UI_BG_ELEVATED, UI_BG_GLASS_DARK, 2);
    HUD_CreateBar(playerid, HUD_BAR_LAP, HUD_X(playerid, 48.0), HUD_Y(playerid, 348.0), UI_BG_ELEVATED, UI_BG_GLASS_DARK);
    HUD_CreateBar(playerid, HUD_BAR_HAUS, HUD_X(playerid, 134.0), HUD_Y(playerid, 348.0), UI_BG_ELEVATED, UI_BG_GLASS_DARK);
    HUD_CreateBar(playerid, HUD_BAR_PIP, HUD_X(playerid, 220.0), HUD_Y(playerid, 348.0), UI_BG_ELEVATED, UI_BG_GLASS_DARK);
    /* Bar BBM di dalam kotak kanan (x 352..628): bar mulai x=500. Celah warna
       kotak 0x0B0F14B4 supaya potongan menyatu dengan latar kotak. */
    if (HUD_BarVisible(playerid, HUD_BAR_VEH))
        HUD_CreateBar(playerid, HUD_BAR_VEH, HUD_X(playerid, 500.0), HUD_Y(playerid, 338.0), UI_BG_ELEVATED, 0x0B0F14B4, 3);
    /* Bar boss di bawah teks ABOMINATION (y=30): celah gelap di atas dunia. */
    if (HUD_BarVisible(playerid, HUD_BAR_BOSS))
        HUD_CreateBar(playerid, HUD_BAR_BOSS, HUD_XC(playerid, 210.0), HUD_Y(playerid, 46.0), UI_BG_ELEVATED, 0x0B0F1499, 4);

    HudProgress[playerid] = CreatePlayerTextDraw(playerid, HUD_X(playerid, 26.0), HUD_Y(playerid, 360.0), "LVL 1  SP 0  /skill");
    PlayerTextDrawLetterSize(playerid, HudProgress[playerid], HUD_F(playerid, 0.18), HUD_F(playerid, 0.75));
    PlayerTextDrawColor(playerid, HudProgress[playerid], UI_TEXT_SECONDARY);
    PlayerTextDrawSetOutline(playerid, HudProgress[playerid], 1);
    PlayerTextDrawFont(playerid, HudProgress[playerid], 1);

    /* Kolom kanan sejajar: SCRAP/SAMPEL dan BBM satu x (352) satu lebar (276).
       Dulu 452 vs 400 — dua kotak melayang dengan tepi yang tidak segaris. */
    HudLoadout[playerid] = CreatePlayerTextDraw(playerid, HUD_X(playerid, 352.0), HUD_Y(playerid, 318.0), "SCRAP 0  SAMPEL 0");
    PlayerTextDrawLetterSize(playerid, HudLoadout[playerid], HUD_F(playerid, 0.2), HUD_F(playerid, 0.8));
    PlayerTextDrawTextSize(playerid, HudLoadout[playerid], HUD_F(playerid, 276.0), HUD_F(playerid, 12.0));
    PlayerTextDrawUseBox(playerid, HudLoadout[playerid], HUD_GayaHasBox(playerid) ? 1 : 0);
    PlayerTextDrawBoxColor(playerid, HudLoadout[playerid], 0x0B0F14B4);
    PlayerTextDrawBackgroundColor(playerid, HudLoadout[playerid], 0x00000000);
    PlayerTextDrawColor(playerid, HudLoadout[playerid], UI_TEXT_PRIMARY);
    PlayerTextDrawSetOutline(playerid, HudLoadout[playerid], 1);
    PlayerTextDrawFont(playerid, HudLoadout[playerid], 1);

    HudVehicle[playerid] = CreatePlayerTextDraw(playerid, HUD_X(playerid, 352.0), HUD_Y(playerid, 334.0), "BBM");
    PlayerTextDrawLetterSize(playerid, HudVehicle[playerid], HUD_F(playerid, 0.17), HUD_F(playerid, 0.72));
    PlayerTextDrawTextSize(playerid, HudVehicle[playerid], HUD_F(playerid, 276.0), HUD_F(playerid, 12.0));
    PlayerTextDrawUseBox(playerid, HudVehicle[playerid], HUD_GayaHasBox(playerid) ? 1 : 0);
    PlayerTextDrawBoxColor(playerid, HudVehicle[playerid], 0x0B0F14B4);
    PlayerTextDrawBackgroundColor(playerid, HudVehicle[playerid], 0x00000000);
    PlayerTextDrawColor(playerid, HudVehicle[playerid], UI_ACCENT_INFO);
    PlayerTextDrawSetOutline(playerid, HudVehicle[playerid], 1);
    PlayerTextDrawFont(playerid, HudVehicle[playerid], 1);

    /* Bar HP boss: teks blok, gaya sama dengan HUD lain (tanpa box tambahan). */
    HudBoss[playerid] = CreatePlayerTextDraw(playerid, HUD_XC(playerid, 210.0), HUD_Y(playerid, 30.0), "ABOMINATION");
    PlayerTextDrawLetterSize(playerid, HudBoss[playerid], HUD_F(playerid, 0.26), HUD_F(playerid, 1.0));
    PlayerTextDrawColor(playerid, HudBoss[playerid], UI_ACCENT_DANGER);
    PlayerTextDrawSetOutline(playerid, HudBoss[playerid], 1);
    PlayerTextDrawFont(playerid, HudBoss[playerid], 2);

    /* Banner raid di atas bar boss (boss y=30). Keduanya bisa aktif bersamaan,
       jadi raid memakai y=16. Lebar ±30 karakter cukup untuk
       "RAID KAMP 12 ZOMBIE  2:59". */
    HudRaid[playerid] = CreatePlayerTextDraw(playerid, HUD_XC(playerid, 190.0), HUD_Y(playerid, 16.0), "RAID KAMP");
    PlayerTextDrawLetterSize(playerid, HudRaid[playerid], HUD_F(playerid, 0.24), HUD_F(playerid, 0.95));
    PlayerTextDrawColor(playerid, HudRaid[playerid], UI_ACCENT_DANGER);
    PlayerTextDrawSetOutline(playerid, HudRaid[playerid], 1);
    PlayerTextDrawFont(playerid, HudRaid[playerid], 2);

    /* Prompt interaksi gaya FiveM: pil di bawah-tengah saat dekat hotspot.
       Teks center (align 2) berarti x=320 adalah TENGAH pil. */
    if (HUD_GayaHasBox(playerid))
    {
        if (Settings_TdMobileSafe(playerid))
        {
            HudPromptBg[playerid] = TD_BoxCreate(playerid, HUD_XC(playerid, 110.0), HUD_Y(playerid, 412.0),
                HUD_F(playerid, 220.0), HUD_F(playerid, 20.0), 0x0B0F14E6);
        }
        else
        {
        HudPromptBg[playerid] = CreatePlayerTextDraw(playerid, HUD_XC(playerid, 210.0), HUD_Y(playerid, 414.0), "LD_SPAC:white");
        PlayerTextDrawFont(playerid, HudPromptBg[playerid], TEXT_DRAW_FONT_SPRITE_DRAW);
        PlayerTextDrawTextSize(playerid, HudPromptBg[playerid], HUD_F(playerid, 220.0), HUD_F(playerid, 18.0));
        PlayerTextDrawColor(playerid, HudPromptBg[playerid], 0x0B0F14E6);
        PlayerTextDrawSetSelectable(playerid, HudPromptBg[playerid], false);
        }
    }

    HudPromptText[playerid] = CreatePlayerTextDraw(playerid, HUD_XC(playerid, 320.0), HUD_Y(playerid, 418.0), "Y");
    PlayerTextDrawLetterSize(playerid, HudPromptText[playerid], HUD_F(playerid, 0.18), HUD_F(playerid, 0.8));
    PlayerTextDrawAlignment(playerid, HudPromptText[playerid], 2);
    PlayerTextDrawColor(playerid, HudPromptText[playerid], UI_TEXT_PRIMARY);
    PlayerTextDrawSetOutline(playerid, HudPromptText[playerid], 1);
    PlayerTextDrawFont(playerid, HudPromptText[playerid], 1);

    if (HudCard[playerid][0] != PlayerText:0)
    {
        PlayerTextDrawShow(playerid, HudCard[playerid][0]);
        PlayerTextDrawShow(playerid, HudCard[playerid][1]);
        PlayerTextDrawShow(playerid, HudCard[playerid][2]);
        PlayerTextDrawShow(playerid, HudCard[playerid][3]);
        PlayerTextDrawShow(playerid, HudCard[playerid][4]);
        PlayerTextDrawShow(playerid, HudCard[playerid][5]);
    }
    if (HudCardAccent[playerid] != PlayerText:0)
        PlayerTextDrawShow(playerid, HudCardAccent[playerid]);
    PlayerTextDrawShow(playerid, HudBrand[playerid]);
    PlayerTextDrawShow(playerid, HudZone[playerid]);
    PlayerTextDrawShow(playerid, HudHpName[playerid]);
    PlayerTextDrawShow(playerid, HudArmName[playerid]);
    PlayerTextDrawShow(playerid, HudInfName[playerid]);
    PlayerTextDrawShow(playerid, HudLapName[playerid]);
    PlayerTextDrawShow(playerid, HudHausName[playerid]);
    PlayerTextDrawShow(playerid, HudPipName[playerid]);
    PlayerTextDrawShow(playerid, HudLoadout[playerid]);
    PlayerTextDrawShow(playerid, HudProgress[playerid]);
    PlayerHudVisible[playerid] = true;

    HudLastZone[playerid][0] = EOS;
    HudLastHpVal[playerid][0] = EOS;
    HudLastArmVal[playerid][0] = EOS;
    HudLastInfVal[playerid][0] = EOS;
    HudLastLapVal[playerid][0] = EOS;
    HudLastHausVal[playerid][0] = EOS;
    HudLastPipVal[playerid][0] = EOS;
    HudLastLoadout[playerid][0] = EOS;
    HudLastProgress[playerid][0] = EOS;
    HudLastVehicle[playerid][0] = EOS;
    HudLastPrompt[playerid][0] = EOS;
    HudFuelColor[playerid] = 0;
    HudFuelWarnStage[playerid] = 0;
    HudLastInfectColor[playerid] = 0;
    HudVehicleShown[playerid] = false;
    HudVehBarShown[playerid] = false;
    HudBossShown[playerid] = false;
    HudRaidShown[playerid] = false;
    HudPromptShown[playerid] = false;
    HUD_ResetCaches(playerid);   /* wajib: textdraw baru, cache lama tidak sah */
    /* Jejak bukti: kalau pemain melaporkan "scrap 0 padahal ada", log ini
       menunjukkan mode renderer + gaya yang dipakai saat HUD dibangun. */
    new hudDiag[96];
    format(hudDiag, sizeof(hudDiag), "[HUD] char %d dibangun: gaya=%d mode=%s skala=%d",
        ActiveCharId[playerid], gHudGaya[playerid],
        Settings_TdMobileSafe(playerid) ? "mobile-safe" : "pc-sprite",
        floatround(Settings_HudScale(playerid) * 100.0));
    Log_Info(hudDiag);
    return 1;
}

/* Client (terutama SA-MP Mobile) membuang PlayerTextDraw setelah respawn atau
   setelah pemain ganti karakter, sehingga HUD hilang permanen walau server
   mengira masih terpasang. Setiap spawn kita tampilkan ulang + reset cache teks
   supaya tick berikutnya langsung menulis nilai baru.

   force=false menghormati /hud: pemain yang menyembunyikan HUD tidak boleh
   dipaksa melihatnya lagi hanya karena mati. Tanpa guard ini, mati sekali
   membatalkan pilihan pemain secara diam-diam. */
/* Cache teks HUD harus direset SETIAP kali textdraw dibuat ulang. Tanpa ini,
   HUD_Create memasang string awal ("SCRAP 0  SAMPEL 0") sementara cache masih
   menyimpan nilai lama, sehingga HUD_SetIfChanged menganggap "tidak berubah"
   dan angka nyata tidak pernah ditulis — inilah akar laporan pemain
   "scrap 0 padahal di tas ada 50" setelah ganti gaya HUD / /settings. */
stock HUD_ResetCaches(playerid)
{
    HudLastZone[playerid][0] = EOS;
    HudLastLoadout[playerid][0] = EOS;
    HudLastProgress[playerid][0] = EOS;
    HudLastVehicle[playerid][0] = EOS;
    HudLastBoss[playerid][0] = EOS;
    HudLastRaid[playerid][0] = EOS;
    HudLastPrompt[playerid][0] = EOS;
    HudLastHpVal[playerid][0] = EOS;
    HudLastArmVal[playerid][0] = EOS;
    HudLastInfVal[playerid][0] = EOS;
    HudLastLapVal[playerid][0] = EOS;
    HudLastHausVal[playerid][0] = EOS;
    HudLastPipVal[playerid][0] = EOS;
    for (new bar = 0; bar < HUD_BAR_COUNT; bar++)
    {
        HudBarLastWidth[playerid][bar] = -1;
        HudBarLastColor[playerid][bar] = 0;
    }
    HudVehicleShown[playerid] = false;
    HudBossShown[playerid] = false;
    HudRaidShown[playerid] = false;
    HudPromptShown[playerid] = false;
    return 1;
}

stock HUD_Reshow(playerid, bool:force = false)
{
    if (!IsPlayerConnected(playerid)) return 0;
    if (!force && !PlayerHudVisible[playerid]) return 0;

    if (HudCard[playerid][0] != PlayerText:0)
        for (new card = 0; card < 6; card++) PlayerTextDrawShow(playerid, HudCard[playerid][card]);
    if (HudCardAccent[playerid] != PlayerText:0)
        PlayerTextDrawShow(playerid, HudCardAccent[playerid]);
    PlayerTextDrawShow(playerid, HudBrand[playerid]);
    PlayerTextDrawShow(playerid, HudZone[playerid]);
    PlayerTextDrawShow(playerid, HudHpName[playerid]);
    PlayerTextDrawShow(playerid, HudArmName[playerid]);
    PlayerTextDrawShow(playerid, HudInfName[playerid]);
    PlayerTextDrawShow(playerid, HudLapName[playerid]);
    PlayerTextDrawShow(playerid, HudHausName[playerid]);
    PlayerTextDrawShow(playerid, HudPipName[playerid]);
    for (new bar = 0; bar < 6; bar++)
    {
        PlayerTextDrawShow(playerid, HudBarBg[playerid][bar]);
        PlayerTextDrawShow(playerid, HudBarFg[playerid][bar]);
    }
    /* Hanya segmen HP/ARM/INF (slot 0-2) yang statis; bar BBM/boss kondisional. */
    for (new seg = 0; seg < 3; seg++)
    {
        for (new cut = 0; cut < HUD_SEG_MAX_GAPS; cut++)
        {
            if (HudBarGap[playerid][seg][cut] != PlayerText:0)
                PlayerTextDrawShow(playerid, HudBarGap[playerid][seg][cut]);
        }
    }
    PlayerTextDrawShow(playerid, HudLoadout[playerid]);
    PlayerTextDrawShow(playerid, HudProgress[playerid]);

    PlayerHudVisible[playerid] = true;
    HudLastZone[playerid][0] = EOS;
    HudLastHpVal[playerid][0] = EOS;
    HudLastArmVal[playerid][0] = EOS;
    HudLastInfVal[playerid][0] = EOS;
    HudLastLapVal[playerid][0] = EOS;
    HudLastHausVal[playerid][0] = EOS;
    HudLastPipVal[playerid][0] = EOS;
    HudLastLoadout[playerid][0] = EOS;
    HudLastProgress[playerid][0] = EOS;
    /* Banner yang hanya dirender saat aktif juga harus dipaksa tampil ulang:
       cache teks kosong + flag false membuat tick berikutnya menulis dan
       menampilkannya lagi kalau raid/boss memang masih berjalan. */
    HudLastBoss[playerid][0] = EOS;
    HudLastRaid[playerid][0] = EOS;
    HudLastPrompt[playerid][0] = EOS;
    /* Cache bar di-reset agar tick berikutnya menulis ulang lebar + warna
       (client baru saja membuang semua TD; nilai lama tidak bisa dipercaya). */
    for (new bar = 0; bar < HUD_BAR_COUNT; bar++)
    {
        HudBarLastWidth[playerid][bar] = -1;
        HudBarLastColor[playerid][bar] = 0;
    }
    HudVehicleShown[playerid] = false;
    HudBossShown[playerid] = false;
    HudRaidShown[playerid] = false;
    HudPromptShown[playerid] = false;
    return 1;
}

stock HUD_Hide(playerid)
{
    if (!IsPlayerConnected(playerid)) return 0;

    if (HudCard[playerid][0] != PlayerText:0)
        for (new card = 0; card < 6; card++) PlayerTextDrawHide(playerid, HudCard[playerid][card]);
    if (HudCardAccent[playerid] != PlayerText:0)
        PlayerTextDrawHide(playerid, HudCardAccent[playerid]);
    PlayerTextDrawHide(playerid, HudBrand[playerid]);
    PlayerTextDrawHide(playerid, HudZone[playerid]);
    PlayerTextDrawHide(playerid, HudHpName[playerid]);
    PlayerTextDrawHide(playerid, HudArmName[playerid]);
    PlayerTextDrawHide(playerid, HudInfName[playerid]);
    PlayerTextDrawHide(playerid, HudLapName[playerid]);
    PlayerTextDrawHide(playerid, HudHausName[playerid]);
    PlayerTextDrawHide(playerid, HudPipName[playerid]);
    for (new bar = 0; bar < HUD_BAR_COUNT; bar++)
    {
        if (!HUD_BarVisible(playerid, bar)) continue;   /* bar detail dimatikan */
        PlayerTextDrawHide(playerid, HudBarBg[playerid][bar]);
        PlayerTextDrawHide(playerid, HudBarFg[playerid][bar]);
    }
    for (new seg = 0; seg < 5; seg++)
    {
        for (new cut = 0; cut < HUD_SEG_MAX_GAPS; cut++)
        {
            if (HudBarGap[playerid][seg][cut] != PlayerText:0)
                PlayerTextDrawHide(playerid, HudBarGap[playerid][seg][cut]);
        }
    }
    PlayerTextDrawHide(playerid, HudLoadout[playerid]);
    PlayerTextDrawHide(playerid, HudProgress[playerid]);
    PlayerTextDrawHide(playerid, HudVehicle[playerid]);
    PlayerTextDrawHide(playerid, HudBoss[playerid]);
    PlayerTextDrawHide(playerid, HudRaid[playerid]);
    if (HudPromptBg[playerid] != PlayerText:0)
        PlayerTextDrawHide(playerid, HudPromptBg[playerid]);
    PlayerTextDrawHide(playerid, HudPromptText[playerid]);

    PlayerHudVisible[playerid] = false;
    HudVehicleShown[playerid] = false;
    HudBossShown[playerid] = false;
    HudRaidShown[playerid] = false;
    HudPromptShown[playerid] = false;
    return 1;
}

stock HUD_Destroy(playerid)
{
    if (HudCard[playerid][0] != PlayerText:0)
        for (new card = 0; card < 6; card++) PlayerTextDrawDestroy(playerid, HudCard[playerid][card]);
    if (HudCardAccent[playerid] != PlayerText:0)
        PlayerTextDrawDestroy(playerid, HudCardAccent[playerid]);
    PlayerTextDrawDestroy(playerid, HudBrand[playerid]);
    PlayerTextDrawDestroy(playerid, HudZone[playerid]);
    PlayerTextDrawDestroy(playerid, HudHpName[playerid]);
    PlayerTextDrawDestroy(playerid, HudHpVal[playerid]);
    PlayerTextDrawDestroy(playerid, HudArmName[playerid]);
    PlayerTextDrawDestroy(playerid, HudArmVal[playerid]);
    PlayerTextDrawDestroy(playerid, HudInfName[playerid]);
    PlayerTextDrawDestroy(playerid, HudInfVal[playerid]);
    PlayerTextDrawDestroy(playerid, HudLapName[playerid]);
    PlayerTextDrawDestroy(playerid, HudLapVal[playerid]);
    PlayerTextDrawDestroy(playerid, HudHausName[playerid]);
    PlayerTextDrawDestroy(playerid, HudHausVal[playerid]);
    PlayerTextDrawDestroy(playerid, HudPipName[playerid]);
    PlayerTextDrawDestroy(playerid, HudPipVal[playerid]);
    for (new bar = 0; bar < HUD_BAR_COUNT; bar++)
    {
        if (!HUD_BarVisible(playerid, bar)) continue;   /* tidak pernah dibuat */
        PlayerTextDrawDestroy(playerid, HudBarBg[playerid][bar]);
        PlayerTextDrawDestroy(playerid, HudBarFg[playerid][bar]);
    }
    for (new seg = 0; seg < 5; seg++)
    {
        for (new cut = 0; cut < HUD_SEG_MAX_GAPS; cut++)
        {
            if (HudBarGap[playerid][seg][cut] != PlayerText:0)
                PlayerTextDrawDestroy(playerid, HudBarGap[playerid][seg][cut]);
        }
    }
    PlayerTextDrawDestroy(playerid, HudProgress[playerid]);
    PlayerTextDrawDestroy(playerid, HudLoadout[playerid]);
    PlayerTextDrawDestroy(playerid, HudVehicle[playerid]);
    PlayerTextDrawDestroy(playerid, HudBoss[playerid]);
    PlayerTextDrawDestroy(playerid, HudRaid[playerid]);
    if (HudPromptBg[playerid] != PlayerText:0)
        PlayerTextDrawDestroy(playerid, HudPromptBg[playerid]);
    PlayerTextDrawDestroy(playerid, HudPromptText[playerid]);
    PlayerHudVisible[playerid] = false;
    return 1;
}

/* Prompt hanya boleh tampil saat TIDAK membangun: Y di mode bangun berarti
   pasang struktur, bukan membuka pintu yang kebetulan di depan (dispatcher.inc
   sudah memprioritaskan BuildEdit_OnKeyState; HUD mengikuti aturan yang sama). */
stock HUD_UpdatePrompt(playerid)
{
    new hudLine[48];
    hudLine[0] = EOS;

    if (!BuildEdit_IsActive(playerid))
    {
        new Float:promptDistance;
        new promptSlot = Interact_NearestForPlayer(playerid, promptDistance);
        if (promptSlot >= 0 && promptDistance <= HUD_PROMPT_DISTANCE && Settings_PromptEnabled(playerid))
        {
            /* Label interact berformat "Nama\n{Y} tekan Y" — ambil baris pertama
               saja dan batasi panjangnya agar pil tidak meluber. */
            new labelPart[32];
            strmid(labelPart, InteractPoint[promptSlot][iaLabelText], 0, HUD_PROMPT_MAX_LABEL, sizeof(labelPart));
            new newline = strfind(labelPart, "\n");
            if (newline >= 0) labelPart[newline] = EOS;
            format(hudLine, sizeof(hudLine), "Y  %s", labelPart);
        }
    }

    if (hudLine[0] != EOS)
    {
        HUD_SetIfChanged(playerid, HudPromptText[playerid], hudLine, HudLastPrompt[playerid], sizeof(HudLastPrompt[]));
        if (!HudPromptShown[playerid])
        {
            HudPromptShown[playerid] = true;
            if (HudPromptBg[playerid] != PlayerText:0)
                PlayerTextDrawShow(playerid, HudPromptBg[playerid]);
            PlayerTextDrawShow(playerid, HudPromptText[playerid]);
        }
    }
    else if (HudPromptShown[playerid])
    {
        HudPromptShown[playerid] = false;
        HudLastPrompt[playerid][0] = EOS;
        if (HudPromptBg[playerid] != PlayerText:0)
            PlayerTextDrawHide(playerid, HudPromptBg[playerid]);
        PlayerTextDrawHide(playerid, HudPromptText[playerid]);
    }
    return 1;
}

stock HUD_Update(playerid)
{
    if (!IsPlayerConnected(playerid)) return 0;
    if (!PlayerHudVisible[playerid]) return 0;

    new hudLine[96];

    /* ---- baris statistik + bar ------------------------------------------ */
    new Float:playerHealth = Player_GetHealth(playerid);
    new Float:playerArmour = PlayerSurvival[playerid][psArmour];
    new Float:infection = Player_GetInfection(playerid);

    new hpColor = UI_ACCENT_PRIMARY;
    if (playerHealth <= 30.0) hpColor = UI_ACCENT_DANGER;
    else if (playerHealth <= 60.0) hpColor = UI_ACCENT_WARN;
    HUD_SetBar(playerid, HUD_BAR_HP, playerHealth / 100.0, hpColor);

    format(hudLine, sizeof(hudLine), "%.0f/100", playerHealth);
    HUD_SetIfChanged(playerid, HudHpVal[playerid], hudLine, HudLastHpVal[playerid], sizeof(HudLastHpVal[]));

    HUD_SetBar(playerid, HUD_BAR_ARM, playerArmour / 100.0, UI_ACCENT_INFO);
    format(hudLine, sizeof(hudLine), "%.0f/100", playerArmour);
    HUD_SetIfChanged(playerid, HudArmVal[playerid], hudLine, HudLastArmVal[playerid], sizeof(HudLastArmVal[]));

    new infectionColor = UI_ACCENT_PRIMARY;
    if (infection >= 80.0) infectionColor = UI_ACCENT_DANGER;
    else if (infection >= 40.0) infectionColor = UI_ACCENT_WARN;
    HUD_SetBar(playerid, HUD_BAR_INF, infection / 100.0, infectionColor);
    HudLastInfectColor[playerid] = infectionColor;

    format(hudLine, sizeof(hudLine), "%.0f%%", infection);
    HUD_SetIfChanged(playerid, HudInfVal[playerid], hudLine, HudLastInfVal[playerid], sizeof(HudLastInfVal[]));

    /* ---- kebutuhan: LAP/HAUS turun = bar menyusut, PIP sebaliknya -------- */
    new Float:hunger = PlayerSurvival[playerid][psHunger];
    new Float:thirst = PlayerSurvival[playerid][psThirst];
    new Float:bladder = PlayerSurvival[playerid][psBladder];

    new needColor = UI_ACCENT_PRIMARY;
    if (hunger <= 20.0) needColor = UI_ACCENT_DANGER;
    else if (hunger <= 40.0) needColor = UI_ACCENT_WARN;
    HUD_SetBar(playerid, HUD_BAR_LAP, hunger / 100.0, needColor);
    format(hudLine, sizeof(hudLine), "%.0f", hunger);
    HUD_SetIfChanged(playerid, HudLapVal[playerid], hudLine, HudLastLapVal[playerid], sizeof(HudLastLapVal[]));

    needColor = UI_ACCENT_PRIMARY;
    if (thirst <= 20.0) needColor = UI_ACCENT_DANGER;
    else if (thirst <= 40.0) needColor = UI_ACCENT_WARN;
    HUD_SetBar(playerid, HUD_BAR_HAUS, thirst / 100.0, needColor);
    format(hudLine, sizeof(hudLine), "%.0f", thirst);
    HUD_SetIfChanged(playerid, HudHausVal[playerid], hudLine, HudLastHausVal[playerid], sizeof(HudLastHausVal[]));

    needColor = UI_ACCENT_PRIMARY;
    if (bladder >= 90.0) needColor = UI_ACCENT_DANGER;
    else if (bladder >= 75.0) needColor = UI_ACCENT_WARN;
    HUD_SetBar(playerid, HUD_BAR_PIP, (100.0 - bladder) / 100.0, needColor);
    format(hudLine, sizeof(hudLine), "%.0f", bladder);
    HUD_SetIfChanged(playerid, HudPipVal[playerid], hudLine, HudLastPipVal[playerid], sizeof(HudLastPipVal[]));

    /* ---- prompt interaksi ------------------------------------------------ */
    HUD_UpdatePrompt(playerid);

    /* Dulu hanya PLAYER_STATE_DRIVER, jadi penumpang tidak pernah melihat BBM
       sama sekali (dilaporkan "tidak ada textdraw bensin"). Sekarang semua
       penghuni kendaraan melihat tangki yang sama. */
    if (IsPlayerInAnyVehicle(playerid))
    {
        new vehicleid = GetPlayerVehicleID(playerid);
        new Float:fuel = Vehicle_GetFuel(vehicleid);
        new Float:tank = Vehicle_GetTank(vehicleid);
        new Float:percent = Vehicle_FuelPercent(vehicleid);

        /* Gauge ASCII [####----] diganti bar bersegmen sungguhan: label + angka
           di kiri, bar 118 px di kanan kotak (x 500..618). Saat BBM <= 25% bar
           disembunyikan dan teks memakai lebar penuh untuk arah pom. */
        new fuelLine[96];

        if (fuel < 0.0)
        {
            format(fuelLine, sizeof(fuelLine), "BBM 0%%  TANPA BBM");
            HUD_SetBar(playerid, HUD_BAR_VEH, 0.0, UI_ACCENT_DANGER);
        }
        else if (percent <= 0.25)
        {
            new pumpPart[40];
            Garage_FuelCompass(playerid, pumpPart, sizeof(pumpPart), true);
            format(fuelLine, sizeof(fuelLine), "BBM %d%%  %.0f/%.0fL %s",
                floatround(percent * 100.0), fuel, tank, pumpPart);
        }
        else
        {
            format(fuelLine, sizeof(fuelLine), "BBM %d%%  %.0f/%.0fL %.0fkm",
                floatround(percent * 100.0), fuel, tank, Vehicle_RangeKm(vehicleid));
        }

        HUD_SetIfChanged(playerid, HudVehicle[playerid], fuelLine, HudLastVehicle[playerid], sizeof(HudLastVehicle[]));

        new fuelColor = UI_ACCENT_PRIMARY;
        if (fuel < 0.0 || percent <= 0.08) fuelColor = UI_ACCENT_DANGER;
        else if (percent <= 0.25) fuelColor = UI_ACCENT_WARN;
        if (fuelColor != HudFuelColor[playerid])
        {
            HudFuelColor[playerid] = fuelColor;
            PlayerTextDrawColor(playerid, HudVehicle[playerid], fuelColor);
            PlayerTextDrawShow(playerid, HudVehicle[playerid]);
        }

        /* Bar hanya saat BBM aman (>25%): saat rendah teks pom pakai lebar penuh
           dan track kosong justru terlihat seperti bug — bar hilang utuh. */
        if (fuel >= 0.0 && percent > 0.25)
        {
            HUD_SetBar(playerid, HUD_BAR_VEH, percent, fuelColor);
            if (!HudVehBarShown[playerid])
            {
                HudVehBarShown[playerid] = true;
                HUD_ShowBar(playerid, HUD_BAR_VEH, 3);
            }
        }
        else if (HudVehBarShown[playerid])
        {
            HudVehBarShown[playerid] = false;
            HUD_HideBar(playerid, HUD_BAR_VEH, 3);
        }

        /* Peringatan sekali per ambang: bunyi + pesan pendek. Tanpa ini pemain
           baru tahu tangki habis setelah mesin mati di tengah horde. */
        new warnStage = 0;
        if (fuel < 0.0) warnStage = 0;
        else if (percent <= 0.08) warnStage = 2;
        else if (percent <= 0.25) warnStage = 1;

        if (warnStage > HudFuelWarnStage[playerid])
        {
            HudFuelWarnStage[playerid] = warnStage;
            PlayerPlaySound(playerid, 1054, 0.0, 0.0, 0.0);
            if (warnStage == 1)
            {
                SendClientMessage(playerid, UI_ACCENT_WARN, "[BBM] Tangki tinggal seperempat. Isi di pom kamp (/isi) atau pakai jerrycan.");
            }
            else
            {
                SendClientMessage(playerid, UI_ACCENT_DANGER, "[BBM] Hampir kering! /isi sekarang, mesin bisa mati kapan saja.");
            }
        }
        else if (percent > 0.30)
        {
            HudFuelWarnStage[playerid] = 0;
        }

        if (!HudVehicleShown[playerid])
        {
            HudVehicleShown[playerid] = true;
            PlayerTextDrawShow(playerid, HudVehicle[playerid]);
        }
    }
    else
    {
        /* Di luar kendaraan: satu-satunya bahan bakar yang relevan adalah
           generator base sendiri. Tanpa ini pemain hanya tahu isi tangki genset
           lewat label 3D, padahal kehabisan BBM mematikan turret. */
        new generatorSlot = BaseBuild_GeneratorSlot(Base_Id(playerid));
        new bool:showGenerator = false;

        if (generatorSlot >= 0 && BaseStructure[generatorSlot][ssActive])
        {
            new Float:px, Float:py, Float:pz;
            GetPlayerPos(playerid, px, py, pz);
            if (VectorSize(px - BaseStructure[generatorSlot][ssX],
                py - BaseStructure[generatorSlot][ssY],
                pz - BaseStructure[generatorSlot][ssZ]) <= 12.0)
            {
                showGenerator = true;

                new Float:gensetFuel = BaseStructure[generatorSlot][ssFuel];
                new gensetLine[96];
                if (gensetFuel <= 0.0)
                {
                    format(gensetLine, sizeof(gensetLine), "GENSET 0%%  MATI");
                }
                else
                {
                    format(gensetLine, sizeof(gensetLine), "GENSET %d%%  %.0f/%.0fL",
                        floatround((gensetFuel / BASE_GENERATOR_FUEL_MAX) * 100.0),
                        gensetFuel, BASE_GENERATOR_FUEL_MAX);
                }

                HUD_SetIfChanged(playerid, HudVehicle[playerid], gensetLine,
                    HudLastVehicle[playerid], sizeof(HudLastVehicle[]));

                new gensetColor = UI_ACCENT_PRIMARY;
                if (gensetFuel <= 0.0) gensetColor = UI_ACCENT_DANGER;
                else if (gensetFuel <= BASE_GENERATOR_FUEL_MAX * 0.25) gensetColor = UI_ACCENT_WARN;

                if (gensetColor != HudFuelColor[playerid])
                {
                    HudFuelColor[playerid] = gensetColor;
                    PlayerTextDrawColor(playerid, HudVehicle[playerid], gensetColor);
                }
                HUD_SetBar(playerid, HUD_BAR_VEH,
                    (gensetFuel <= 0.0) ? 0.0 : (gensetFuel / BASE_GENERATOR_FUEL_MAX), gensetColor);
                if (gensetFuel > 0.0)
                {
                    if (!HudVehBarShown[playerid])
                    {
                        HudVehBarShown[playerid] = true;
                        HUD_ShowBar(playerid, HUD_BAR_VEH, 3);
                    }
                }
                else if (HudVehBarShown[playerid])
                {
                    HudVehBarShown[playerid] = false;
                    HUD_HideBar(playerid, HUD_BAR_VEH, 3);
                }
            }
        }

        if (showGenerator)
        {
            if (!HudVehicleShown[playerid])
            {
                HudVehicleShown[playerid] = true;
                PlayerTextDrawShow(playerid, HudVehicle[playerid]);
                HUD_ShowBar(playerid, HUD_BAR_VEH, 3);
            }
        }
        else if (HudVehicleShown[playerid])
        {
            HudVehicleShown[playerid] = false;
            HudFuelWarnStage[playerid] = 0;
            PlayerTextDrawHide(playerid, HudVehicle[playerid]);
            HudVehBarShown[playerid] = false;
            HUD_HideBar(playerid, HUD_BAR_VEH, 3);
        }
    }

    new zoneLabel[48], zoneLine[64];
    new zoneId = Player_GetZone(playerid);
    Zone_GetName(zoneId, zoneLabel, sizeof(zoneLabel));
    /* Pill THREAT (audit U3): noise adalah pilar utama — pemain harus bisa
       MELIHAT konsekuensi tembakan mereka tanpa /diag. */
    new Float:zoneHeat = Horde_GetZoneHeat(zoneId, Lobby_Of(playerid));
    if (zoneHeat < 10.0) format(zoneLine, sizeof(zoneLine), "%s [AMAN]", zoneLabel);
    else if (zoneHeat < 30.0) format(zoneLine, sizeof(zoneLine), "%s [WASPADA]", zoneLabel);
    else format(zoneLine, sizeof(zoneLine), "%s [BAHAYA]", zoneLabel);
    HUD_SetIfChanged(playerid, HudZone[playerid], zoneLine, HudLastZone[playerid], sizeof(HudLastZone[]));

    /* Uang sengaja tidak ditampilkan (permintaan pemain). Yang tersisa adalah
       dua angka yang benar-benar dipakai saat membangun dan bertahan. */
    format(hudLine, sizeof(hudLine), "SCRAP %d  SAMPEL %d",
        Inventory_CountItem(playerid, ITEM_SCRAP), Inventory_CountItem(playerid, ITEM_SPECIMEN));
    HUD_SetIfChanged(playerid, HudLoadout[playerid], hudLine, HudLastLoadout[playerid], sizeof(HudLastLoadout[]));

    // Progres skill harus terlihat tanpa membuka dialog: ini satu-satunya petunjuk
    // bahwa level dan poin skill bertambah. SAMPEL dihapus — sudah ada di kolom
    // kanan; dobel di dua baris adalah noise (audit UX).
    format(hudLine, sizeof(hudLine), "LVL %d  SP %d  LUKA %d  /skill",
        PlayerSurvival[playerid][psLevel], SkillPoints[playerid], Wounds_Get(playerid));
    HUD_SetIfChanged(playerid, HudProgress[playerid], hudLine, HudLastProgress[playerid], sizeof(HudLastProgress[]));

    new Float:bossHealth, Float:bossMax;
    if (Director_BossStatus(Lobby_Of(playerid), bossHealth, bossMax) && bossMax > 0.0)
    {
        /* Gauge ASCII diganti bar bersegmen 258 px di bawah nama boss. */
        new bossLine[64];
        format(bossLine, sizeof(bossLine), "ABOMINATION %d%%",
            floatround((bossHealth / bossMax) * 100.0));
        HUD_SetIfChanged(playerid, HudBoss[playerid], bossLine, HudLastBoss[playerid], sizeof(HudLastBoss[]));
        HUD_SetBar(playerid, HUD_BAR_BOSS, bossHealth / bossMax, UI_ACCENT_DANGER);
        if (!HudBossShown[playerid])
        {
            HudBossShown[playerid] = true;
            PlayerTextDrawShow(playerid, HudBoss[playerid]);
            HUD_ShowBar(playerid, HUD_BAR_BOSS, 4);
        }
    }
    else if (HudBossShown[playerid])
    {
        HudBossShown[playerid] = false;
        PlayerTextDrawHide(playerid, HudBoss[playerid]);
        HUD_HideBar(playerid, HUD_BAR_BOSS, 4);
    }

    /* Banner raid: satu-satunya umpan balik bertahan selain toast lobby yang
       cepat hilang. Jumlah zombie dibaca dari cache Horde_Tick (5 dtk), jadi
       tidak ada pemindaian ZombiePool di jalur 500 ms ini. */
    new raidZombies, raidSeconds;
    if (Horde_GetRaidStatus(Lobby_Of(playerid), raidZombies, raidSeconds))
    {
        new raidLine[64];
        format(raidLine, sizeof(raidLine), "RAID KAMP %d ZOMBIE  %d:%02d",
            raidZombies, raidSeconds / 60, raidSeconds % 60);
        HUD_SetIfChanged(playerid, HudRaid[playerid], raidLine, HudLastRaid[playerid], sizeof(HudLastRaid[]));
        if (!HudRaidShown[playerid])
        {
            HudRaidShown[playerid] = true;
            PlayerTextDrawShow(playerid, HudRaid[playerid]);
        }
    }
    else if (HudRaidShown[playerid])
    {
        HudRaidShown[playerid] = false;
        PlayerTextDrawHide(playerid, HudRaid[playerid]);
    }
    return 1;
}

public HUD_Tick()
{
    new perfStart = GetTickCount();
    foreach (new playerid : PlayerPool)
    {
        HUD_Update(playerid);
    }
    Perf_Update(PERF_HUD, perfStart);
    return 1;
}
