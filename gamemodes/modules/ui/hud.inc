#if defined _ls_hud_included
    #endinput
#endif
#define _ls_hud_included

/* Preferensi /settings (settings.inc di-include setelah file ini). */
forward Float:Settings_HudScale(playerid);
forward bool:Settings_BarDetailEnabled(playerid);
forward bool:Settings_ModulEnabled(playerid, bit);
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
/* Dua baris tambahan (permintaan pemain): uang tunai & jam server. Keduanya
   dipakai terus-menerus saat bermain (belanja/loot vs janji bertemu pemain
   lain) sehingga layak selalu tampil, bukan disembunyikan di dialog. */
new PlayerText:HudMoney[MAX_PLAYERS];
new PlayerText:HudClock[MAX_PLAYERS];
new HudLastMoney[MAX_PLAYERS][24];
new HudLastClock[MAX_PLAYERS][16];
new PlayerText:HudVehicle[MAX_PLAYERS];
new PlayerText:HudVehCard[MAX_PLAYERS];
/* Kolom kendaraan (kanan, x 352..628): identitas mobil, kecepatan, bahan bakar
   dan GPS. Empat baris ini SATU kelompok visual yang terpisah dari panel
   pemain (x 16..304) — tiga gaya HUD mengubah permukaan & gauge-nya, bukan
   menyembunyikan baris. */
new PlayerText:HudVehInfo[MAX_PLAYERS];
new PlayerText:HudSpeedVal[MAX_PLAYERS];
new PlayerText:HudSpeedUnit[MAX_PLAYERS];
new PlayerText:HudGpsText[MAX_PLAYERS];
new PlayerText:HudGpsCard[MAX_PLAYERS];
new HudLastVehInfo[MAX_PLAYERS][48];
new HudLastSpeed[MAX_PLAYERS][8];
new HudLastGps[MAX_PLAYERS][72];
new bool:HudVehInfoShown[MAX_PLAYERS];
new bool:HudSpeedShown[MAX_PLAYERS];
new bool:HudGpsShown[MAX_PLAYERS];
new bool:HudGpsBarShown[MAX_PLAYERS];
new gHudWriteCount;
new gHudSetCalls;
new gHudSetDbg[MAX_PLAYERS];
new gHudTickCount;
new PlayerText:HudBoss[MAX_PLAYERS];
new PlayerText:HudRaid[MAX_PLAYERS];
new PlayerText:HudPromptBg[MAX_PLAYERS];
new PlayerText:HudPromptText[MAX_PLAYERS];
new bool:PlayerHudVisible[MAX_PLAYERS];

/* Bar index: 0 HP, 1 ARM, 2 INF, 3 LAP, 4 HAUS, 5 PIP, 6 BBM/GENSET, 7 BOSS,
   8 KMH (speedometer), 9 GPS (progres menuju tujuan). */
#define HUD_BAR_HP     0
#define HUD_BAR_ARM    1
#define HUD_BAR_INF    2
#define HUD_BAR_LAP    3
#define HUD_BAR_HAUS   4
#define HUD_BAR_PIP    5
#define HUD_BAR_VEH    6
#define HUD_BAR_BOSS   7
#define HUD_BAR_KMH    8
#define HUD_BAR_GPS    9
#define HUD_BAR_COUNT  10

new PlayerText:HudBarBg[MAX_PLAYERS][HUD_BAR_COUNT];
new PlayerText:HudBarFg[MAX_PLAYERS][HUD_BAR_COUNT];
new HudBarLastWidth[MAX_PLAYERS][HUD_BAR_COUNT];
new HudBarLastColor[MAX_PLAYERS][HUD_BAR_COUNT];

/* Lebar/tinggi bar harus sama dengan angka di HUD_CreateBar — generate.py dan
   gallery.py membaca literal yang sama, jadi jangan ubah satu sisi saja. */
new const Float:gHudBarW[HUD_BAR_COUNT] = {104.0, 104.0, 104.0, 40.0, 40.0, 40.0, 264.0, 258.0, 264.0, 264.0};
new const Float:gHudBarH[HUD_BAR_COUNT] = {5.0, 5.0, 5.0, 4.0, 4.0, 4.0, 5.0, 6.0, 4.0, 4.0};

/* Bar bersegmen gaya game modern: fill kontinu dipotong strip warna latar.
   0 = kontinu (micro-bar). Maksimum 10 segmen -> paling banyak 9 celah.
   Bar yang bersegmen: HP, ARM, INF, BBM/GENSET, BOSS (urutan HUD_SEG_BARS). */
#define HUD_SEG_MAX_GAPS 9
#define HUD_SEG_GAP_W    1.6
/* segSlot: HP 0, ARM 1, INF 2, VEH 3, BOSS 4, KMH 5, GPS 6. */
new const gHudSegCount[7] = {10, 10, 10, 10, 10, 12, 10};
new PlayerText:HudBarGap[MAX_PLAYERS][7][HUD_SEG_MAX_GAPS];

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

/* Pembanding/pengisi string manual. `strcmp` dan YSI `strcpy` sama-sama
   bermasalah di jalur ini: `strcmp` menerima sentinel `EOS` sebagai karakter
   pembanding, dan `strcpy` YSI adalah makro `strcat` yang mengosongkan dest
   (bisa merusak saat dest == sumber). Dua helper ini hanya membandingkan /
   menyalin sampai terminator pertama, jadi hasilnya tidak bergantung pada
   perilaku pustaka luar. */
stock bool:HUD_TextSame(const a[], const b[], size)
{
    for (new i = 0; i < size; i++)
    {
        if (a[i] != b[i]) return false;
        if (a[i] == EOS) return true;
    }
    return true;
}

stock HUD_TextStore(dest[], const src[], size)
{
    for (new i = 0; i < size - 1; i++)
    {
        dest[i] = src[i];
        if (src[i] == EOS) return 1;
    }
    dest[size - 1] = EOS;
    return 1;
}

stock HUD_SetIfChanged(playerid, PlayerText:td, const text[], lastText[], lastSize)
{
    gHudSetCalls++;

    /* Jejak dua panggilan pertama per pemain. Tanpa ini "string benar tapi
       layar tidak berubah" hanya bisa ditebak — dan tebakan itu sudah dua kali
       salah (strcmp vs sentinel EOS, lalu strcpy YSI yang mengosongkan dest). */
    if (gHudSetDbg[playerid] < 2)
    {
        gHudSetDbg[playerid]++;
        Log_Format(LOG_INFO, "hudset %d: td=%d baru='%s' lama='%s' sama=%d",
            playerid, _:td, text, lastText, HUD_TextSame(text, lastText, lastSize));
    }

    if (HUD_TextSame(text, lastText, lastSize)) return 0;

    HUD_TextStore(lastText, text, lastSize);
    PlayerTextDrawSetString(playerid, td, text);
    /* Menulis string TIDAK menampilkan textdraw. Tanpa baris ini setiap baris
       dinamis (HP/ARM/INF/LAP/HAUS/PIP/FUEL) tetap tak terlihat walau isinya
       benar — persis laporan pemain "angka HUD tidak muncul". */
    PlayerTextDrawShow(playerid, td);
    gHudWriteCount++;
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

/* Geometri baris kendaraan (BBM). Angka ini dipakai HUD_Create, HUD_Update,
   dan audit /tddebug — satu sumber kebenaran, tidak boleh diduplikasi. */
/* Baris kendaraan DUA TINGKAT: teks di atas (satu baris penuh), bar di
   bawahnya selebar kartu. Tata letak satu baris (teks kiri + bar kanan)
   menyisakan separuh kartu kosong saat BBM rendah (teks pom panjang) dan
   membuat bar terlihat "hilang" — dilaporkan sebagai "box BBM kacau". */
/* ---- Kolom kendaraan (kanan) ------------------------------------------
   Satu kelompok, tiga permukaan berbeda:
     gaya 1 PENUH   : kartu kaca + bar KMH + bar FUEL + bar GPS
     gaya 2 MOBILE  : kartu datar + bar FUEL (teks lebih besar 10%)
     gaya 3 MINIMAL : TANPA kartu & tanpa bar KMH/GPS, hanya teks + bar FUEL
   Tidak ada baris yang disembunyikan oleh gaya: yang berbeda hanya
   permukaannya. */
#define HUD_VEHC_X          352.0
#define HUD_VEHC_W          276.0
#define HUD_VEHC_TXT_X      356.0
#define HUD_VEHC_TXT_W      264.0
#define HUD_VEHC_LETTER_X   0.16
/* Grid kolom kendaraan (font 1 letter Y 0.62 = tinggi baris ~6.5 px).
   SELURUH grup duduk di PITA BAWAH kanvas 640x480 (356..444), bukan di tengah:
   di layar Android yang 2340x1080 kanvas 640x480 hanya dipetakan 70%x75% dari
   layar (X 2.56 px/unit, Y 1.70 px/unit, rata atas), sehingga y=256 tampak
   "di tengah layar" dan menutupi jalan.
     356 .. 420  kartu A   info 359 | speed 367 | bar KMH 386 | FUEL 393 | bar 405
     422 .. 444  kartu B   GPS 425 | bar GPS 436
   Semua baris memakai LetterSize Y yang sama supaya garis dasar rata; hanya
   angka kecepatan yang lebih besar (font 2) sesuai skala tipografi HUD. */
#define HUD_VEHC_CARD_Y     356.0
#define HUD_VEHC_CARD_H     64.0
#define HUD_VEHC_INFO_Y     359.0
#define HUD_VEHC_SPEED_Y    367.0
#define HUD_VEHC_SPEED_LETTER_X 0.34
#define HUD_VEHC_KMH_Y      386.0
#define HUD_VEHC_UNIT_X     396.0
#define HUD_VEHC_FUEL_Y     393.0
#define HUD_VEHC_FUELBAR_Y  405.0
#define HUD_VEHC_GPS_Y      425.0
#define HUD_VEHC_GPSBAR_Y   436.0
#define HUD_VEHC_GPSCARD_Y  422.0
#define HUD_VEHC_GPSCARD_H  22.0

/* Skala kecepatan: velocity SA-MP x 180 = km/jam yang masuk akal (Infernus
   ~180 km/jam di jalan lurus). 240 = batas atas bar supaya tidak pernah
   melewati lebar kartu. */
#define HUD_SPEED_MAX       240
#define HUD_SPEED_WARN      140

/* Pengali LetterSize per gaya: mobile sedikit lebih besar (jarak baca di HP),
   minimal sedikit lebih kecil (teks tanpa kartu tidak boleh terasa penuh). */
stock Float:HUD_GayaLetterMul(playerid)
{
    switch (gHudGaya[playerid])
    {
        case HUD_GAYA_MOBILE:  return 1.10;
        case HUD_GAYA_MINIMAL: return 0.92;
    }
    return 1.0;
}

/* Bar KMH/GPS hanya milik gaya yang punya kartu (1 dan 2). Gaya minimal tetap
   punya bar FUEL: itu alat keselamatan, bukan hiasan. */
stock bool:HUD_GayaVehBars(playerid)
{
    return gHudGaya[playerid] != HUD_GAYA_MINIMAL;
}

/* Geometri baris FUEL (dipakai juga oleh gate 640 px). Dulu kartu ini duduk di x 352..628 —
   melayang di tengah kanan layar dan terbaca sebagai "kotak kosong di tengah"
   di ketiga gaya HUD. Sekarang ia menempel tepat di bawah panel status supaya
   selalu satu kolom visual dengan HP/ARM/LAP/UANG/JAM. */
#define HUD_VEH_CARD_X      HUD_VEHC_X
#define HUD_VEH_CARD_W      HUD_VEHC_W
#define HUD_VEH_CARD_Y      HUD_VEHC_CARD_Y
#define HUD_VEH_CARD_H      HUD_VEHC_CARD_H
#define HUD_VEH_TEXT_X      HUD_VEHC_TXT_X
#define HUD_VEH_TEXT_Y      HUD_VEHC_FUEL_Y
#define HUD_VEH_TEXT_W      HUD_VEHC_TXT_W
#define HUD_VEH_LETTER_X    HUD_VEHC_LETTER_X
#define HUD_VEH_BAR_X       HUD_VEHC_TXT_X
#define HUD_VEH_BAR_Y       HUD_VEHC_FUELBAR_Y
#define HUD_VEH_BAR_W       HUD_VEHC_TXT_W

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
/* KOMPENSASI TARIK HORIZONTAL ------------------------------------------
   Klien SA-MP memetakan kanvas 640x480 ke SELURUH layar. Di PC 16:9 tarikan
   x hanya 1.2x, tetapi di HP 20:9 (2400x1080) menjadi 1.52x — diukur langsung
   dari screenshot pemain: panel 288x100 unit tampil 1052x241 px (skala_x 3.65,
   skala_y 2.41). Akibatnya SEMUA teks jadi 1.5x lebih lebar dari perhitungan
   kanvas, menabrak bar dan keluar kotak — inilah "HUD kacau" di Android.
   Karena X dan Y bisa diatur terpisah, kita perkecil SEMUA geometri horizontal
   sebesar faktor kompensasi; klien lalu meregangkannya kembali ke proporsi asli.
   Nilai default: 0.66 untuk klien mobile, 1.00 untuk PC. Pemain bisa
   menyesuaikan sendiri lewat /hudlebar (50-100). */
new Float:gHudXLebar[MAX_PLAYERS];
/* Penanda pemain sudah menimpa lebar manual lewat /hudlebar. Tanpa penanda ini
   deteksi klien tidak bisa memperbaiki nilai karena HUD_Create sudah terlanjur
   menulis 1.00 sebelum deteksi berjalan (urutan OnPlayerConnect). */
new bool:gHudXLebarManual[MAX_PLAYERS];
new gHudSkin[MAX_PLAYERS];          /* design/theme HUD aktif (HUD_SKIN_*) */
#define HUD_XLEBAR_MOBILE 0.66
#define HUD_XLEBAR_PC     1.00

/* Deteksi klien untuk memilih kompensasi awal. Versi klien dicatat ke log
   supaya tim bisa memastikan string apa yang dilaporkan klien mobile. */
stock HUD_DetectClient(playerid)
{
    new version[64];
    GetPlayerVersion(playerid, version, sizeof(version));

    /* Klien SAMP Unity / SA-MP Mobile TIDAK mengirim string versi (kosong).
       Terbukti dari log: "klien 237: versi '' mobile=0 lebar=1.00" — karena itu
       kompensasi tidak pernah aktif dan HUD tetap melar 1.5x di HP. Klien PC
       resmi selalu melaporkan "0.3.7-R5-..." sehingga versi kosong/tak dikenal
       aman dianggap mobile. */
    new bool:mobile = false;
    if (strlen(version) == 0) mobile = true;
    else if (strfind(version, "unity", true) != -1) mobile = true;
    else if (strfind(version, "android", true) != -1) mobile = true;
    else if (strfind(version, "ios", true) != -1) mobile = true;
    else if (strfind(version, "mobile", true) != -1) mobile = true;
    else if (strfind(version, "0.3", true) == -1) mobile = true;

    gHudMobileClient[playerid] = mobile;
    if (!gHudXLebarManual[playerid])
    {
        gHudXLebar[playerid] = mobile ? HUD_XLEBAR_MOBILE : HUD_XLEBAR_PC;
    }

    Log_Format(LOG_INFO, "klien %d: versi '%s' mobile=%d lebar=%.2f manual=%d",
        playerid, version, mobile, gHudXLebar[playerid], gHudXLebarManual[playerid]);
    return mobile;
}

stock Float:HUD_XComp(playerid)
{
    if (gHudXLebar[playerid] <= 0.0) return 1.0;
    return gHudXLebar[playerid];
}

stock HUD_XLebarSet(playerid, value)
{
    if (value < 50) value = 50;
    if (value > 100) value = 100;
    gHudXLebar[playerid] = float(value) / 100.0;
    gHudXLebarManual[playerid] = true;
    return value;
}

stock Float:HUD_X(playerid, Float:x) { return x * HUD_ScaleOf(playerid) * HUD_XComp(playerid); }
stock Float:HUD_Y(playerid, Float:y) { return y * HUD_ScaleOf(playerid); }

/* Koordinat elemen center (boss/raid/prompt): tetap di tengah horizontal. */
stock Float:HUD_XC(playerid, Float:x)
{
    return 320.0 - (320.0 - x) * HUD_ScaleOf(playerid) * HUD_XComp(playerid);
}

/* LetterSize/ukuran ikut skala. HUD_F untuk sumbu Y dan tinggi; HUD_FX untuk
   sumbu X dan lebar (ikut kompensasi tarik horizontal). */
stock Float:HUD_F(playerid, Float:value) { return value * HUD_ScaleOf(playerid); }
stock Float:HUD_FX(playerid, Float:value) { return value * HUD_ScaleOf(playerid) * HUD_XComp(playerid); }

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
    { "HudMoney",      26.0, 0.16, 12 },
    { "HudClock",     120.0, 0.16,  9 },
    { "HudVehicle",   356.0, 0.16, 28 },
    { "HudVehInfo",   356.0, 0.16, 19 },
    { "HudSpeedVal",  356.0, 0.34,  3 },
    { "HudSpeedUnit", 396.0, 0.16,  4 },
    { "HudGpsText",   356.0, 0.16, 26 },
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

/* ------------------------------------------------------------- design HUD */

/* Satu tabel data mengendalikan SELURUH palet kartu HUD. Menambah design baru
   = menambah satu baris di sini; tidak ada warna yang ditulis langsung di
   HUD_Create lagi (supaya tidak ada elemen yang tertinggal memakai palet lama). */
enum E_HUD_SKIN
{
    skinName[14],
    skinCard,          /* warna permukaan kartu   */
    skinTrack,         /* warna track bar         */
    skinTextPri,
    skinTextSec,
    skinAccent,
    Float:skinLetterMul,
    bool:skinSolidBar, /* true = bar tanpa celah segmen */
    bool:skinStrip     /* true = garis aksen tipis di atas kartu */
}

new const gHudSkins[HUD_SKIN_COUNT + 1][E_HUD_SKIN] =
{
    /* 0 tidak dipakai (auto diresolusi sebelum create) */
    { "AUTO",     0x0B0F19E6, 0x1E293BFF, 0xF8FAFCFF, 0x94A3B8FF, 0x10B981FF, 1.00, false, true },
    { "KACA",     0x0B0F19E6, 0x1E293BFF, 0xF8FAFCFF, 0x94A3B8FF, 0x10B981FF, 1.00, false, true },
    { "NEON",     0x06141BF0, 0x0E3A46FF, 0xECFEFFFF, 0x67E8F9FF, 0x22D3EEFF, 1.04, true,  true },
    { "TACTICAL", 0x1A1C14F2, 0x33351FFF, 0xF5F3E7FF, 0xC9C4A0FF, 0xF59E0BFF, 1.00, true,  false },
    { "TERANG",   0xE8E4D9F7, 0xBBB5A5FF, 0x1F2937FF, 0x4B5563FF, 0x0F766EFF, 1.00, true,  false }
};

stock HUD_SkinIdx(playerid)
{
    new skin = gHudSkin[playerid];
    if (skin < HUD_SKIN_KACA || skin > HUD_SKIN_COUNT) return HUD_SKIN_DEFAULT;
    return skin;
}

stock HUD_SkinName(playerid, dest[], size)
{
    format(dest, size, "%s", gHudSkins[HUD_SkinIdx(playerid)][skinName]);
    return 1;
}

/* Pemetaan warna aksen akal sehat: bar yang dulu memakai UI_ACCENT_* tetap
   punya makna (ok/warn/danger) tetapi warnanya ikut design aktif. */
stock HUD_SkinBarColor(playerid, color)
{
    new skin = HUD_SkinIdx(playerid);
    if (color == UI_ACCENT_PRIMARY || color == UI_ACCENT_INFO) return gHudSkins[skin][skinAccent];
    if (color == UI_ACCENT_WARN) return gHudSkins[skin][skinAccent];
    return color;   /* danger tetap merah: arti keselamatan tidak boleh hilang */
}

stock HUD_CreateCard(playerid, Float:x, Float:y, Float:width, Float:height, Float:radius, color = UI_BG_GLASS_DARK)
{
    if (Settings_TdMobileSafe(playerid))
    {
        HudCard[playerid][0] = TD_BoxCreate(playerid, x, y, width, height, color);
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
    PlayerTextDrawColor(playerid, HudCard[playerid][0], color);
    PlayerTextDrawSetSelectable(playerid, HudCard[playerid][0], false);

    HudCard[playerid][1] = CreatePlayerTextDraw(playerid, x + rx, y, "LD_SPAC:white");
    PlayerTextDrawFont(playerid, HudCard[playerid][1], TEXT_DRAW_FONT_SPRITE_DRAW);
    PlayerTextDrawTextSize(playerid, HudCard[playerid][1], width - diamX, height);
    PlayerTextDrawColor(playerid, HudCard[playerid][1], color);
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
        PlayerTextDrawColor(playerid, HudCard[playerid][2 + corner], color);
        PlayerTextDrawSetSelectable(playerid, HudCard[playerid][2 + corner], false);
    }
    return 1;
}

stock HUD_CreateBar(playerid, index, Float:x, Float:y, trackColor, gapColor, segSlot = -1)
{
    /* Skin bisa memaksa bar SOLID (tanpa celah segmen) dan mengganti warna
       track/celah: celah memakai warna kartu supaya terbaca sebagai potongan. */
    new sk = HUD_SkinIdx(playerid);
    trackColor = gHudSkins[sk][skinTrack];
    gapColor = gHudSkins[sk][skinCard];
    if (gHudSkins[sk][skinSolidBar]) segSlot = -1;

    new Float:barW = gHudBarW[index] * HUD_ScaleOf(playerid) * HUD_XComp(playerid);
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
    /* Bar bahan bakar tidak mengikuti preferensi "detail": tanpa bar, pemain
       hanya punya angka dan tidak bisa menilai sisa bensin sekilas — keluhan
       "ga tau sisa berapa bensinnya". Bar boss tetap mengikuti preferensi. */
    if (index == HUD_BAR_VEH) return true;
    if (index == HUD_BAR_BOSS) return Settings_BarDetailEnabled(playerid);
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
    color = HUD_SkinBarColor(playerid, color);
    if (!HUD_BarVisible(playerid, index)) return 0;
    if (ratio < 0.0) ratio = 0.0;
    if (ratio > 1.0) ratio = 1.0;

    new width = floatround(gHudBarW[index] * HUD_ScaleOf(playerid) * HUD_XComp(playerid) * ratio);
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
        if (!gHudXLebarManual[playerid])
        {
            gHudXLebar[playerid] = gHudMobileClient[playerid] ? HUD_XLEBAR_MOBILE : HUD_XLEBAR_PC;
        }
    }

    /* Design HUD: 0 (auto) sudah diresolusi Settings_Apply sebelum create. */
    new skIdx = HUD_SkinIdx(playerid);

    /* Kartu kiri: x 16..304, y 328..428 — pita bawah kiri kanvas, bukan tengah layar.
       Skala menyusutkan semua koordinat menuju kiri-atas (mobile aman). */
    if (HUD_GayaHasCard(playerid))
    {
        HUD_CreateCard(playerid, HUD_X(playerid, 16.0), HUD_Y(playerid, 328.0),
            HUD_FX(playerid, 288.0), HUD_F(playerid, 100.0), HUD_F(playerid, 2.0),
            gHudSkins[skIdx][skinCard]);
    }

    if (HUD_GayaHasCard(playerid))
    {
        if (Settings_TdMobileSafe(playerid))
        {
            HudCardAccent[playerid] = TD_BoxCreate(playerid, HUD_X(playerid, 18.0), HUD_Y(playerid, 329.0),
                HUD_F(playerid, 3.0), HUD_F(playerid, 98.0), gHudSkins[skIdx][skinAccent]);
        }
        else HudCardAccent[playerid] = CreatePlayerTextDraw(playerid, HUD_X(playerid, 18.0), HUD_Y(playerid, 328.0), "LD_SPAC:white");
        PlayerTextDrawFont(playerid, HudCardAccent[playerid], TEXT_DRAW_FONT_SPRITE_DRAW);
        PlayerTextDrawTextSize(playerid, HudCardAccent[playerid], HUD_FX(playerid, 284.0), HUD_F(playerid, 2.0));
        PlayerTextDrawColor(playerid, HudCardAccent[playerid], gHudSkins[skIdx][skinAccent]);
        PlayerTextDrawSetSelectable(playerid, HudCardAccent[playerid], false);
    }

    HudBrand[playerid] = CreatePlayerTextDraw(playerid, HUD_X(playerid, 26.0), HUD_Y(playerid, 334.0), "LAST_STAND");
    PlayerTextDrawLetterSize(playerid, HudBrand[playerid], HUD_FX(playerid, 0.20), HUD_F(playerid, 0.85));
    PlayerTextDrawColor(playerid, HudBrand[playerid], gHudSkins[skIdx][skinTextPri]);
    PlayerTextDrawSetOutline(playerid, HudBrand[playerid], 1);
    PlayerTextDrawFont(playerid, HudBrand[playerid], 2);

    /* Zona pindah ke baris brand: hemat satu baris, tetap terbaca. */
    HudZone[playerid] = CreatePlayerTextDraw(playerid, HUD_X(playerid, 150.0), HUD_Y(playerid, 336.0), "ZONE");
    PlayerTextDrawLetterSize(playerid, HudZone[playerid], HUD_FX(playerid, 0.16), HUD_F(playerid, 0.75));
    PlayerTextDrawColor(playerid, HudZone[playerid], gHudSkins[skIdx][skinTextSec]);
    PlayerTextDrawSetOutline(playerid, HudZone[playerid], 1);
    PlayerTextDrawFont(playerid, HudZone[playerid], 1);

    HudHpName[playerid] = CreatePlayerTextDraw(playerid, HUD_X(playerid, 26.0), HUD_Y(playerid, 350.0), "HP");
    PlayerTextDrawLetterSize(playerid, HudHpName[playerid], HUD_FX(playerid, 0.16), HUD_F(playerid, 0.8));
    PlayerTextDrawColor(playerid, HudHpName[playerid], gHudSkins[skIdx][skinTextSec]);
    PlayerTextDrawSetOutline(playerid, HudHpName[playerid], 1);
    PlayerTextDrawFont(playerid, HudHpName[playerid], 1);

    HudArmName[playerid] = CreatePlayerTextDraw(playerid, HUD_X(playerid, 26.0), HUD_Y(playerid, 362.0), "ARM");
    PlayerTextDrawLetterSize(playerid, HudArmName[playerid], HUD_FX(playerid, 0.16), HUD_F(playerid, 0.8));
    PlayerTextDrawColor(playerid, HudArmName[playerid], gHudSkins[skIdx][skinTextSec]);
    PlayerTextDrawSetOutline(playerid, HudArmName[playerid], 1);
    PlayerTextDrawFont(playerid, HudArmName[playerid], 1);

    HudInfName[playerid] = CreatePlayerTextDraw(playerid, HUD_X(playerid, 26.0), HUD_Y(playerid, 374.0), "INF");
    PlayerTextDrawLetterSize(playerid, HudInfName[playerid], HUD_FX(playerid, 0.16), HUD_F(playerid, 0.8));
    PlayerTextDrawColor(playerid, HudInfName[playerid], gHudSkins[skIdx][skinTextSec]);
    PlayerTextDrawSetOutline(playerid, HudInfName[playerid], 1);
    PlayerTextDrawFont(playerid, HudInfName[playerid], 1);

    HudHpVal[playerid] = CreatePlayerTextDraw(playerid, HUD_X(playerid, 162.0), HUD_Y(playerid, 350.0), "100/100");
    PlayerTextDrawLetterSize(playerid, HudHpVal[playerid], HUD_FX(playerid, 0.16), HUD_F(playerid, 0.8));
    PlayerTextDrawColor(playerid, HudHpVal[playerid], gHudSkins[skIdx][skinTextPri]);
    PlayerTextDrawSetOutline(playerid, HudHpVal[playerid], 1);
    PlayerTextDrawFont(playerid, HudHpVal[playerid], 1);

    HudArmVal[playerid] = CreatePlayerTextDraw(playerid, HUD_X(playerid, 162.0), HUD_Y(playerid, 362.0), "0/100");
    PlayerTextDrawLetterSize(playerid, HudArmVal[playerid], HUD_FX(playerid, 0.16), HUD_F(playerid, 0.8));
    PlayerTextDrawColor(playerid, HudArmVal[playerid], gHudSkins[skIdx][skinTextPri]);
    PlayerTextDrawSetOutline(playerid, HudArmVal[playerid], 1);
    PlayerTextDrawFont(playerid, HudArmVal[playerid], 1);

    HudInfVal[playerid] = CreatePlayerTextDraw(playerid, HUD_X(playerid, 162.0), HUD_Y(playerid, 374.0), "0%");
    PlayerTextDrawLetterSize(playerid, HudInfVal[playerid], HUD_FX(playerid, 0.16), HUD_F(playerid, 0.8));
    PlayerTextDrawColor(playerid, HudInfVal[playerid], gHudSkins[skIdx][skinTextPri]);
    PlayerTextDrawSetOutline(playerid, HudInfVal[playerid], 1);
    PlayerTextDrawFont(playerid, HudInfVal[playerid], 1);

    HudLapName[playerid] = CreatePlayerTextDraw(playerid, HUD_X(playerid, 26.0), HUD_Y(playerid, 386.0), "LAP");
    PlayerTextDrawLetterSize(playerid, HudLapName[playerid], HUD_FX(playerid, 0.15), HUD_F(playerid, 0.75));
    PlayerTextDrawColor(playerid, HudLapName[playerid], gHudSkins[skIdx][skinTextSec]);
    PlayerTextDrawSetOutline(playerid, HudLapName[playerid], 1);
    PlayerTextDrawFont(playerid, HudLapName[playerid], 1);

    HudHausName[playerid] = CreatePlayerTextDraw(playerid, HUD_X(playerid, 112.0), HUD_Y(playerid, 386.0), "HAUS");
    PlayerTextDrawLetterSize(playerid, HudHausName[playerid], HUD_FX(playerid, 0.15), HUD_F(playerid, 0.75));
    PlayerTextDrawColor(playerid, HudHausName[playerid], gHudSkins[skIdx][skinTextSec]);
    PlayerTextDrawSetOutline(playerid, HudHausName[playerid], 1);
    PlayerTextDrawFont(playerid, HudHausName[playerid], 1);

    HudPipName[playerid] = CreatePlayerTextDraw(playerid, HUD_X(playerid, 198.0), HUD_Y(playerid, 386.0), "PIP");
    PlayerTextDrawLetterSize(playerid, HudPipName[playerid], HUD_FX(playerid, 0.15), HUD_F(playerid, 0.75));
    PlayerTextDrawColor(playerid, HudPipName[playerid], gHudSkins[skIdx][skinTextSec]);
    PlayerTextDrawSetOutline(playerid, HudPipName[playerid], 1);
    PlayerTextDrawFont(playerid, HudPipName[playerid], 1);

    HudLapVal[playerid] = CreatePlayerTextDraw(playerid, HUD_X(playerid, 92.0), HUD_Y(playerid, 386.0), "100");
    PlayerTextDrawLetterSize(playerid, HudLapVal[playerid], HUD_FX(playerid, 0.15), HUD_F(playerid, 0.75));
    PlayerTextDrawColor(playerid, HudLapVal[playerid], gHudSkins[skIdx][skinTextPri]);
    PlayerTextDrawSetOutline(playerid, HudLapVal[playerid], 1);
    PlayerTextDrawFont(playerid, HudLapVal[playerid], 1);

    HudHausVal[playerid] = CreatePlayerTextDraw(playerid, HUD_X(playerid, 178.0), HUD_Y(playerid, 386.0), "100");
    PlayerTextDrawLetterSize(playerid, HudHausVal[playerid], HUD_FX(playerid, 0.15), HUD_F(playerid, 0.75));
    PlayerTextDrawColor(playerid, HudHausVal[playerid], gHudSkins[skIdx][skinTextPri]);
    PlayerTextDrawSetOutline(playerid, HudHausVal[playerid], 1);
    PlayerTextDrawFont(playerid, HudHausVal[playerid], 1);

    HudPipVal[playerid] = CreatePlayerTextDraw(playerid, HUD_X(playerid, 264.0), HUD_Y(playerid, 386.0), "0");
    PlayerTextDrawLetterSize(playerid, HudPipVal[playerid], HUD_FX(playerid, 0.15), HUD_F(playerid, 0.75));
    PlayerTextDrawColor(playerid, HudPipVal[playerid], gHudSkins[skIdx][skinTextPri]);
    PlayerTextDrawSetOutline(playerid, HudPipVal[playerid], 1);
    PlayerTextDrawFont(playerid, HudPipVal[playerid], 1);

    /* ---- kartu kolom kendaraan (kanan) ---------------------------------
       Kartu A menampung identitas mobil + kecepatan + bahan bakar.
       Kartu B khusus GPS, berdiri sendiri, supaya saat pemain turun dari mobil
       tidak tersisa kartu kosong setinggi 64 px.
       Gaya MINIMAL tidak memakai kartu sama sekali (HUD_GayaHasCard). */
    if (HUD_GayaHasCard(playerid))
    {
        new Float:cardX = HUD_X(playerid, HUD_VEH_CARD_X);
        new Float:cardY = HUD_Y(playerid, HUD_VEH_CARD_Y);
        new Float:cardW = HUD_FX(playerid, HUD_VEH_CARD_W);
        new Float:cardH = HUD_F(playerid, HUD_VEH_CARD_H);
        new Float:gpsY = HUD_Y(playerid, HUD_VEHC_GPSCARD_Y);
        new Float:gpsH = HUD_F(playerid, HUD_VEHC_GPSCARD_H);

        if (Settings_TdMobileSafe(playerid))
        {
            HudVehCard[playerid] = TD_BoxCreate(playerid, cardX, cardY, cardW, cardH, 0x0B0F14B4);
            HudGpsCard[playerid] = TD_BoxCreate(playerid, cardX, gpsY,
                HUD_FX(playerid, HUD_VEHC_W), gpsH, 0x0B0F14B4);
        }
        else
        {
            HudVehCard[playerid] = CreatePlayerTextDraw(playerid, cardX, cardY, "LD_SPAC:white");
            PlayerTextDrawFont(playerid, HudVehCard[playerid], 4);
            PlayerTextDrawTextSize(playerid, HudVehCard[playerid], cardW, cardH);
            PlayerTextDrawColor(playerid, HudVehCard[playerid], 0x0B0F14B4);
            PlayerTextDrawSetOutline(playerid, HudVehCard[playerid], 0);
            PlayerTextDrawBackgroundColor(playerid, HudVehCard[playerid], 0x00000000);

            HudGpsCard[playerid] = CreatePlayerTextDraw(playerid, cardX, gpsY, "LD_SPAC:white");
            PlayerTextDrawFont(playerid, HudGpsCard[playerid], 4);
            PlayerTextDrawTextSize(playerid, HudGpsCard[playerid], HUD_FX(playerid, HUD_VEHC_W), gpsH);
            PlayerTextDrawColor(playerid, HudGpsCard[playerid], 0x0B0F14B4);
            PlayerTextDrawSetOutline(playerid, HudGpsCard[playerid], 0);
            PlayerTextDrawBackgroundColor(playerid, HudGpsCard[playerid], 0x00000000);
        }
    }

    HUD_CreateBar(playerid, HUD_BAR_HP, HUD_X(playerid, 52.0), HUD_Y(playerid, 352.0), gHudSkins[skIdx][skinTrack], gHudSkins[skIdx][skinCard], 0);
    HUD_CreateBar(playerid, HUD_BAR_ARM, HUD_X(playerid, 52.0), HUD_Y(playerid, 364.0), gHudSkins[skIdx][skinTrack], gHudSkins[skIdx][skinCard], 1);
    HUD_CreateBar(playerid, HUD_BAR_INF, HUD_X(playerid, 52.0), HUD_Y(playerid, 376.0), gHudSkins[skIdx][skinTrack], gHudSkins[skIdx][skinCard], 2);
    HUD_CreateBar(playerid, HUD_BAR_LAP, HUD_X(playerid, 48.0), HUD_Y(playerid, 388.0), gHudSkins[skIdx][skinTrack], gHudSkins[skIdx][skinCard]);
    HUD_CreateBar(playerid, HUD_BAR_HAUS, HUD_X(playerid, 134.0), HUD_Y(playerid, 388.0), gHudSkins[skIdx][skinTrack], gHudSkins[skIdx][skinCard]);
    HUD_CreateBar(playerid, HUD_BAR_PIP, HUD_X(playerid, 220.0), HUD_Y(playerid, 388.0), gHudSkins[skIdx][skinTrack], gHudSkins[skIdx][skinCard]);
    /* Baris kendaraan dua tingkat di dalam kartu 352..628 (y 328..354):
         teks : x 356 .. 620 pada y 331
         bar  : x 356 .. 620 pada y 344 (lebar penuh, tidak pernah bertumpuk
                dengan teks karena beda baris) */
    if (HUD_BarVisible(playerid, HUD_BAR_VEH))
        HUD_CreateBar(playerid, HUD_BAR_VEH, HUD_X(playerid, HUD_VEH_BAR_X),
            HUD_Y(playerid, HUD_VEH_BAR_Y), gHudSkins[skIdx][skinTrack], 0x0B0F14B4, 3);
    /* Speedometer & GPS hanya punya bar di gaya berkartu. Gaya minimal tetap
       menampilkan angkanya sebagai teks. */
    if (HUD_GayaVehBars(playerid))
    {
        if (HUD_BarVisible(playerid, HUD_BAR_KMH))
            HUD_CreateBar(playerid, HUD_BAR_KMH, HUD_X(playerid, HUD_VEHC_TXT_X),
                HUD_Y(playerid, HUD_VEHC_KMH_Y), gHudSkins[skIdx][skinTrack], 0x0B0F14B4, 5);
        if (HUD_BarVisible(playerid, HUD_BAR_GPS))
            HUD_CreateBar(playerid, HUD_BAR_GPS, HUD_X(playerid, HUD_VEHC_TXT_X),
                HUD_Y(playerid, HUD_VEHC_GPSBAR_Y), gHudSkins[skIdx][skinTrack], 0x0B0F14B4, 6);
    }
    /* Bar boss di bawah teks ABOMINATION (y=30): celah gelap di atas dunia. */
    if (HUD_BarVisible(playerid, HUD_BAR_BOSS))
        HUD_CreateBar(playerid, HUD_BAR_BOSS, HUD_XC(playerid, 210.0), HUD_Y(playerid, 46.0), gHudSkins[skIdx][skinTrack], 0x0B0F1499, 4);

    HudProgress[playerid] = CreatePlayerTextDraw(playerid, HUD_X(playerid, 26.0), HUD_Y(playerid, 400.0), "LVL 1  SP 0  /skill");
    PlayerTextDrawLetterSize(playerid, HudProgress[playerid], HUD_FX(playerid, 0.18), HUD_F(playerid, 0.75));
    PlayerTextDrawColor(playerid, HudProgress[playerid], gHudSkins[skIdx][skinTextSec]);
    PlayerTextDrawSetOutline(playerid, HudProgress[playerid], 1);
    PlayerTextDrawFont(playerid, HudProgress[playerid], 1);

    HudMoney[playerid] = CreatePlayerTextDraw(playerid, HUD_X(playerid, 26.0), HUD_Y(playerid, 412.0), "UANG $0");
    PlayerTextDrawLetterSize(playerid, HudMoney[playerid], HUD_FX(playerid, 0.16), HUD_F(playerid, 0.78));
    PlayerTextDrawColor(playerid, HudMoney[playerid], UI_ACCENT_WARN);
    PlayerTextDrawSetOutline(playerid, HudMoney[playerid], 1);
    PlayerTextDrawFont(playerid, HudMoney[playerid], 1);

    HudClock[playerid] = CreatePlayerTextDraw(playerid, HUD_X(playerid, 120.0), HUD_Y(playerid, 412.0), "JAM 00:00");
    PlayerTextDrawLetterSize(playerid, HudClock[playerid], HUD_FX(playerid, 0.16), HUD_F(playerid, 0.78));
    PlayerTextDrawColor(playerid, HudClock[playerid], gHudSkins[skIdx][skinTextSec]);
    PlayerTextDrawSetOutline(playerid, HudClock[playerid], 1);
    PlayerTextDrawFont(playerid, HudClock[playerid], 1);

    /* ---- baris 1: identitas kendaraan ----------------------------------
       PENTING: textdraw TEKS tidak boleh diberi TextSize. Klien mobile memakai
       TextSize.x sebagai lebar kotak pembungkus, sehingga dengan LetterSize.x
       0.16 teks "MOBIL 518 TERBUKA" pecah jadi 3 baris dan "FUEL 48% 33/70L
       6km" jadi 4 baris (terbukti di screenshot Android 2340x1080). Baris HUD
       lain yang tidak memanggil TextSize selalu tampil satu baris. */
    HudVehInfo[playerid] = CreatePlayerTextDraw(playerid, HUD_X(playerid, HUD_VEHC_TXT_X),
        HUD_Y(playerid, HUD_VEHC_INFO_Y), "KENDARAAN");
    PlayerTextDrawLetterSize(playerid, HudVehInfo[playerid], HUD_FX(playerid, HUD_VEHC_LETTER_X * HUD_GayaLetterMul(playerid)), HUD_F(playerid, 0.62));
    PlayerTextDrawUseBox(playerid, HudVehInfo[playerid], 0);
    PlayerTextDrawBackgroundColor(playerid, HudVehInfo[playerid], 0x00000000);
    PlayerTextDrawColor(playerid, HudVehInfo[playerid], gHudSkins[skIdx][skinTextPri]);
    PlayerTextDrawSetOutline(playerid, HudVehInfo[playerid], 1);
    PlayerTextDrawFont(playerid, HudVehInfo[playerid], 1);

    /* ---- baris 2: kecepatan (angka besar + satuan) ---------------------- */
    HudSpeedVal[playerid] = CreatePlayerTextDraw(playerid, HUD_X(playerid, HUD_VEHC_TXT_X),
        HUD_Y(playerid, HUD_VEHC_SPEED_Y), "0");
    PlayerTextDrawLetterSize(playerid, HudSpeedVal[playerid], HUD_FX(playerid, HUD_VEHC_SPEED_LETTER_X), HUD_F(playerid, 1.30));
    PlayerTextDrawUseBox(playerid, HudSpeedVal[playerid], 0);
    PlayerTextDrawBackgroundColor(playerid, HudSpeedVal[playerid], 0x00000000);
    PlayerTextDrawColor(playerid, HudSpeedVal[playerid], gHudSkins[skIdx][skinAccent]);
    PlayerTextDrawSetOutline(playerid, HudSpeedVal[playerid], 1);
    PlayerTextDrawFont(playerid, HudSpeedVal[playerid], 2);

    HudSpeedUnit[playerid] = CreatePlayerTextDraw(playerid, HUD_X(playerid, HUD_VEHC_UNIT_X),
        HUD_Y(playerid, HUD_VEHC_SPEED_Y + 6.0), "km/j");
    PlayerTextDrawLetterSize(playerid, HudSpeedUnit[playerid], HUD_FX(playerid, HUD_VEHC_LETTER_X * HUD_GayaLetterMul(playerid)), HUD_F(playerid, 0.62));
    PlayerTextDrawBackgroundColor(playerid, HudSpeedUnit[playerid], 0x00000000);
    PlayerTextDrawColor(playerid, HudSpeedUnit[playerid], gHudSkins[skIdx][skinTextPri]);
    PlayerTextDrawSetOutline(playerid, HudSpeedUnit[playerid], 1);
    PlayerTextDrawFont(playerid, HudSpeedUnit[playerid], 1);

    /* ---- baris 3: bahan bakar ------------------------------------------ */
    HudVehicle[playerid] = CreatePlayerTextDraw(playerid, HUD_X(playerid, HUD_VEH_TEXT_X),
        HUD_Y(playerid, HUD_VEH_TEXT_Y), "FUEL");
    PlayerTextDrawLetterSize(playerid, HudVehicle[playerid], HUD_FX(playerid, HUD_VEHC_LETTER_X * HUD_GayaLetterMul(playerid)), HUD_F(playerid, 0.62));
    PlayerTextDrawUseBox(playerid, HudVehicle[playerid], 0);
    PlayerTextDrawBoxColor(playerid, HudVehicle[playerid], 0x0B0F14B4);
    PlayerTextDrawBackgroundColor(playerid, HudVehicle[playerid], 0x00000000);
    PlayerTextDrawColor(playerid, HudVehicle[playerid], gHudSkins[skIdx][skinAccent]);
    PlayerTextDrawSetOutline(playerid, HudVehicle[playerid], 1);
    PlayerTextDrawFont(playerid, HudVehicle[playerid], 1);

    /* ---- baris 4: GPS --------------------------------------------------- */
    HudGpsText[playerid] = CreatePlayerTextDraw(playerid, HUD_X(playerid, HUD_VEHC_TXT_X),
        HUD_Y(playerid, HUD_VEHC_GPS_Y), "GPS");
    PlayerTextDrawLetterSize(playerid, HudGpsText[playerid], HUD_FX(playerid, HUD_VEHC_LETTER_X * HUD_GayaLetterMul(playerid)), HUD_F(playerid, 0.70));
    PlayerTextDrawUseBox(playerid, HudGpsText[playerid], 0);
    PlayerTextDrawBackgroundColor(playerid, HudGpsText[playerid], 0x00000000);
    PlayerTextDrawColor(playerid, HudGpsText[playerid], gHudSkins[skIdx][skinAccent]);
    PlayerTextDrawSetOutline(playerid, HudGpsText[playerid], 1);
    PlayerTextDrawFont(playerid, HudGpsText[playerid], 1);

    /* Bar HP boss: teks blok, gaya sama dengan HUD lain (tanpa box tambahan). */
    Log_Format(LOG_INFO, "hud %d kolom kendaraan gaya %d: kartu %.0f..%.0f y%.0f..%.0f | info y%.0f speed y%.0f fuel y%.0f/bar y%.0f gps y%.0f/bar y%.0f",
        playerid, gHudGaya[playerid],
        HUD_VEH_CARD_X, HUD_VEH_CARD_X + HUD_VEH_CARD_W,
        HUD_VEH_CARD_Y, HUD_VEH_CARD_Y + HUD_VEH_CARD_H,
        HUD_VEHC_INFO_Y, HUD_VEHC_SPEED_Y, HUD_VEH_TEXT_Y, HUD_VEH_BAR_Y,
        HUD_VEHC_GPS_Y, HUD_VEHC_GPSBAR_Y);

    HudBoss[playerid] = CreatePlayerTextDraw(playerid, HUD_XC(playerid, 210.0), HUD_Y(playerid, 30.0), "ABOMINATION");
    PlayerTextDrawLetterSize(playerid, HudBoss[playerid], HUD_FX(playerid, 0.26), HUD_F(playerid, 1.0));
    PlayerTextDrawColor(playerid, HudBoss[playerid], UI_ACCENT_DANGER);
    PlayerTextDrawSetOutline(playerid, HudBoss[playerid], 1);
    PlayerTextDrawFont(playerid, HudBoss[playerid], 2);

    /* Banner raid di atas bar boss (boss y=30). Keduanya bisa aktif bersamaan,
       jadi raid memakai y=16. Lebar ±30 karakter cukup untuk
       "RAID KAMP 12 ZOMBIE  2:59". */
    HudRaid[playerid] = CreatePlayerTextDraw(playerid, HUD_XC(playerid, 190.0), HUD_Y(playerid, 16.0), "RAID KAMP");
    PlayerTextDrawLetterSize(playerid, HudRaid[playerid], HUD_FX(playerid, 0.24), HUD_F(playerid, 0.95));
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
        HudPromptBg[playerid] = CreatePlayerTextDraw(playerid, HUD_XC(playerid, 210.0), HUD_Y(playerid, 448.0), "LD_SPAC:white");
        PlayerTextDrawFont(playerid, HudPromptBg[playerid], TEXT_DRAW_FONT_SPRITE_DRAW);
        PlayerTextDrawTextSize(playerid, HudPromptBg[playerid], HUD_FX(playerid, 220.0), HUD_F(playerid, 18.0));
        PlayerTextDrawColor(playerid, HudPromptBg[playerid], 0x0B0F14E6);
        PlayerTextDrawSetSelectable(playerid, HudPromptBg[playerid], false);
        }
    }

    HudPromptText[playerid] = CreatePlayerTextDraw(playerid, HUD_XC(playerid, 320.0), HUD_Y(playerid, 418.0), "Y");
    PlayerTextDrawLetterSize(playerid, HudPromptText[playerid], HUD_FX(playerid, 0.18), HUD_F(playerid, 0.8));
    PlayerTextDrawAlignment(playerid, HudPromptText[playerid], 2);
    PlayerTextDrawColor(playerid, HudPromptText[playerid], gHudSkins[skIdx][skinTextPri]);
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
    PlayerTextDrawShow(playerid, HudHpVal[playerid]);
    PlayerTextDrawShow(playerid, HudArmVal[playerid]);
    PlayerTextDrawShow(playerid, HudInfVal[playerid]);
    PlayerTextDrawShow(playerid, HudLapVal[playerid]);
    PlayerTextDrawShow(playerid, HudHausVal[playerid]);
    PlayerTextDrawShow(playerid, HudPipVal[playerid]);
    PlayerTextDrawShow(playerid, HudProgress[playerid]);
    PlayerTextDrawShow(playerid, HudMoney[playerid]);
    PlayerTextDrawShow(playerid, HudClock[playerid]);
    PlayerHudVisible[playerid] = true;

    HudLastZone[playerid][0] = EOS;
    HudLastMoney[playerid][0] = EOS;
    HudLastClock[playerid][0] = EOS;
    HudLastHpVal[playerid][0] = EOS;
    HudLastArmVal[playerid][0] = EOS;
    HudLastInfVal[playerid][0] = EOS;
    HudLastLapVal[playerid][0] = EOS;
    HudLastHausVal[playerid][0] = EOS;
    HudLastPipVal[playerid][0] = EOS;
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
/* ==== audit posisi/lebar (dipanggil /tddebug) ===========================
   Setiap baris HUD diukur dengan tabel fonts.dat lalu dibandingkan dengan
   batas wilayahnya sendiri. Angka "batas" adalah tepi kiri elemen tetangga,
   jadi hasilnya langsung menunjukkan tabrakan, bukan perkiraan. */
stock HUD_AuditRow(playerid, const name[], Float:x, Float:letterX, const text[], Float:cellWidth)
{
    new Float:width = TD_TextWidth(text, letterX);
    new verdict[24];
    if (width > cellWidth) format(verdict, sizeof(verdict), "LEBIH %.0f", width - cellWidth);
    else format(verdict, sizeof(verdict), "OK");
    if (cellWidth <= 0.0) format(verdict, sizeof(verdict), "OK");

    new line[144];
    format(line, sizeof(line), "[TDD] %s x=%.0f lebar=%.0f sel=%.0f %s",
        name, x, width, cellWidth, verdict);
    SendClientMessage(playerid, (width > cellWidth && cellWidth > 0.0) ? UI_ACCENT_DANGER : UI_ACCENT_PRIMARY, line);
    Log_Format(LOG_INFO, "tdaudit %d: %s", playerid, line);
    return 1;
}

stock HUD_AuditRows(playerid)
{
    if (!IsPlayerConnected(playerid)) return 0;

    SendClientMessage(playerid, UI_ACCENT_WARN, "[TDD] audit baris: nama x=posisi lebar=hasil ukur sel=batas wilayah");
    HUD_AuditRow(playerid, "Brand",  26.0, 0.18, "LAST_STAND", 150.0);
    HUD_AuditRow(playerid, "Zona",  150.0, 0.18, "Bandara Lama [WASPADA]", 220.0);
    HUD_AuditRow(playerid, "HpVal", 162.0, 0.18, "100/100", 150.0);
    /* Kolom kendaraan: keempat baris harus berhenti sebelum tepi kanan kartu. */
    HUD_AuditRow(playerid, "FUEL-teks", HUD_VEH_TEXT_X * HUD_XComp(playerid),
        HUD_VEH_LETTER_X * HUD_XComp(playerid),
        "FUEL 12%  12/80L pom 240m BD",
        (HUD_VEH_TEXT_W - 8.0) * HUD_XComp(playerid));
    HUD_AuditRow(playerid, "MOBIL-teks", HUD_VEHC_TXT_X * HUD_XComp(playerid),
        HUD_VEHC_LETTER_X * HUD_XComp(playerid),
        "MOBIL 611  LS-1234  TERKUNCI",
        (HUD_VEHC_TXT_W - 8.0) * HUD_XComp(playerid));
    HUD_AuditRow(playerid, "GPS-teks", HUD_VEHC_TXT_X * HUD_XComp(playerid),
        HUD_VEHC_LETTER_X * HUD_XComp(playerid),
        "GPS 1234m TL  REKANPARTY",
        (HUD_VEHC_TXT_W - 8.0) * HUD_XComp(playerid));
    HUD_AuditRow(playerid, "Progress", 26.0, 0.18, "LVL 99  SP 999  LUKA 9  /skill", 300.0);
    HUD_AuditRow(playerid, "Boss", 210.0, 0.26, "ABOMINATION 100%", 0.0);
    HUD_AuditRow(playerid, "Raid", 190.0, 0.24, "RAID KAMP 14 ZOMBIE  3:00", 0.0);
    HUD_AuditRow(playerid, "Prompt", 320.0, 0.18, "Y  Ekstraksi sampel air", 0.0);

    new line[128];
    format(line, sizeof(line), "[TDD] FUEL: kartu %.0f..%.0f, teks y%.0f batas %.0f, bar x%.0f..%.0f y%.0f",
        HUD_VEH_CARD_X, HUD_VEH_CARD_X + HUD_VEH_CARD_W, HUD_VEH_TEXT_Y,
        HUD_VEH_TEXT_W - 8.0, HUD_VEH_BAR_X, HUD_VEH_BAR_X + HUD_VEH_BAR_W, HUD_VEH_BAR_Y);
    SendClientMessage(playerid, UI_ACCENT_INFO, line);
    return 1;
}

/* /huddiag — bukti isi string yang BENAR-BENAR dikirim ke klien.
   Dipakai saat pemain melaporkan "angka tidak muncul": kalau baris di sini
   benar, masalahnya di klien (klip/alpha), bukan di logika HUD. */
/* /hudlebar <50-100> — kalibrasi tarik horizontal. HUD dibangun ulang supaya
   pemain langsung melihat hasilnya tanpa relog. */
stock HUD_LebarShow(playerid)
{
    new line[144];
    format(line, sizeof(line), "[HUD] lebar sekarang %d%% (mobile default %.0f%%, PC 100%%). Ubah: /hudlebar 66",
        floatround(HUD_XComp(playerid) * 100.0), HUD_XLEBAR_MOBILE * 100.0);
    SendClientMessage(playerid, UI_ACCENT_INFO, line);
    SendClientMessage(playerid, UI_ACCENT_PRIMARY,
        "[HUD] 100 = apa adanya (PC). 66 = kompensasi HP 20:9. Kalau teks menabrak bar, turunkan 5 poin.");
    return 1;
}

stock HUD_LebarApply(playerid, value)
{
    new applied = HUD_XLebarSet(playerid, value);
    HUD_Destroy(playerid);
    HUD_Create(playerid);
    HUD_Reshow(playerid, true);

    new line[96];
    format(line, sizeof(line), "[HUD] lebar horizontal %d%%. Kalau masih melar, coba /hudlebar %d.",
        applied, applied - 5);
    SendClientMessage(playerid, UI_ACCENT_PRIMARY, line);
    Log_Format(LOG_INFO, "hudlebar %d: %d%%", playerid, applied);
    return applied;
}

stock HUD_Diag(playerid)
{
    if (!IsPlayerConnected(playerid)) return 0;

    new line[160];
    format(line, sizeof(line), "[DIAG] tampil=%d inCar=%d pool=%d anggota=%d hudLast=\"%s\"",
        PlayerHudVisible[playerid], IsPlayerInAnyVehicle(playerid),
        Iter_Count(PlayerPool), Iter_Contains(PlayerPool, playerid), HudLastVehicle[playerid]);
    SendClientMessage(playerid, UI_ACCENT_INFO, line);
    Log_Format(LOG_INFO, "huddiag %d: tampil=%d inCar=%d fuel=\"%s\" hp=\"%s\" arm=\"%s\"",
        playerid, PlayerHudVisible[playerid], IsPlayerInAnyVehicle(playerid),
        HudLastVehicle[playerid], HudLastHpVal[playerid], HudLastArmVal[playerid]);

    format(line, sizeof(line), "[DIAG] HP=%.0f/100 ARM=%.0f/100 LAP=%.0f",
        Player_GetHealth(playerid), PlayerSurvival[playerid][psArmour],
        PlayerSurvival[playerid][psHunger]);
    SendClientMessage(playerid, UI_ACCENT_PRIMARY, line);

    if (IsPlayerInAnyVehicle(playerid))
    {
        new vehicleid = GetPlayerVehicleID(playerid);
        new Float:fuel = Vehicle_GetFuel(vehicleid);
        new Float:percent = Vehicle_FuelPercent(vehicleid);

        new fuelLine[80];
        format(fuelLine, sizeof(fuelLine), "FUEL %d%%  %.0f/%.0fL %.0fkm",
            floatround(percent * 100.0), fuel, Vehicle_GetTank(vehicleid), Vehicle_RangeKm(vehicleid));
        new Float:diagLetterX = HUD_VEH_LETTER_X * HUD_ScaleOf(playerid) * HUD_XComp(playerid);
        new Float:textBudget = (HUD_VEH_TEXT_W - 8.0)
            * HUD_ScaleOf(playerid) * HUD_XComp(playerid);
        TD_FitText(fuelLine, sizeof(fuelLine), fuelLine, diagLetterX, textBudget);
        format(line, sizeof(line), "[DIAG] fuel %.1fL (%.0f%%) lebar=%d%% baris=\"%s\" (%.0f/%.0f satuan)",
            fuel, percent * 100.0, floatround(HUD_XComp(playerid) * 100.0), fuelLine,
            TD_TextWidth(fuelLine, diagLetterX), textBudget);
        SendClientMessage(playerid, UI_ACCENT_PRIMARY, line);
    }
    return 1;
}

/* Uji cache baris HUD. Dua bug nyata bersembunyi di sini: `strcmp` yang
   membandingkan sentinel `EOS` sebagai karakter, dan `strcpy` YSI yang
   mengosongkan dest. Keduanya berujung sama — teks benar, layar kosong. */
/* Tiga gaya HUD harus benar-benar berbeda permukaannya, tetapi TIDAK boleh
   menyembunyikan baris keselamatan. Selftest ini mengunci keduanya. */
stock HUD_GayaSelftest()
{
    new bool:ok = true;
    new saved = gHudGaya[0];

    gHudGaya[0] = HUD_GAYA_PENUH;
    new Float:fullMul = HUD_GayaLetterMul(0);
    if (!HUD_GayaHasCard(0)) ok = false;
    if (!HUD_GayaVehBars(0)) ok = false;

    gHudGaya[0] = HUD_GAYA_MOBILE;
    new Float:mobileMul = HUD_GayaLetterMul(0);
    if (!HUD_GayaHasCard(0)) ok = false;
    if (mobileMul <= fullMul) ok = false;
    if (!HUD_BarVisible(0, HUD_BAR_VEH)) ok = false;

    gHudGaya[0] = HUD_GAYA_MINIMAL;
    new Float:minimalMul = HUD_GayaLetterMul(0);
    if (HUD_GayaHasCard(0)) ok = false;
    if (HUD_GayaVehBars(0)) ok = false;
    if (minimalMul >= fullMul) ok = false;
    /* Gaya minimal kehilangan kartu dan bar KMH/GPS, tetapi bar bahan bakar
       dan baris teksnya wajib tetap ada: menghilangkan bar BBM untuk menandai
       "gaya minimal" akan menyembunyikan satu-satunya alat ukur keselamatan. */
    if (!HUD_BarVisible(0, HUD_BAR_VEH)) ok = false;

    gHudGaya[0] = saved;

    Selftest_Record(ok, "tiga gaya HUD kendaraan",
        ok ? "kartu/bar/letter beda per gaya, baris FUEL tetap tampil"
           : "gaya HUD menyembunyikan baris atau tidak membedakan permukaan");
    return ok;
}

/* Design HUD: tabel harus lengkap dan pemetaan warna harus menjaga arti
   keselamatan (merah tetap merah di semua design). */
stock HUD_SkinSelftest()
{
    new bool:ok = true;

    ok = ok && HUD_SKIN_COUNT == 4;
    for (new skin = HUD_SKIN_KACA; skin <= HUD_SKIN_COUNT; skin++)
    {
        ok = ok && gHudSkins[skin][skinName][0] != EOS;
        /* alpha kartu wajib > 0 (kartu transparan total = kartu tidak ada) */
        ok = ok && ((gHudSkins[skin][skinCard] & 0x000000FF) > 0x40);
        ok = ok && gHudSkins[skin][skinLetterMul] >= 0.90 && gHudSkins[skin][skinLetterMul] <= 1.10;
        /* warna teks harus beda dengan kartu, kalau tidak tulisan tak terlihat */
        ok = ok && gHudSkins[skin][skinTextPri] != gHudSkins[skin][skinCard];
    }

    /* Di luar rentang -> design default, bukan warna transparan. */
    gHudSkin[0] = 99;
    ok = ok && HUD_SkinIdx(0) == HUD_SKIN_DEFAULT;
    gHudSkin[0] = HUD_SKIN_TERANG;
    ok = ok && HUD_SkinIdx(0) == HUD_SKIN_TERANG;
    ok = ok && HUD_SkinBarColor(0, UI_ACCENT_INFO) == gHudSkins[HUD_SKIN_TERANG][skinAccent];
    ok = ok && HUD_SkinBarColor(0, UI_ACCENT_DANGER) == UI_ACCENT_DANGER;
    gHudSkin[0] = HUD_SKIN_DEFAULT;

    Selftest_Record(ok, "design HUD",
        ok ? "4 design valid, warna teks != kartu, merah bahaya tidak diubah"
           : "tabel design tidak lengkap atau pemetaan warna merusak arti bahaya");
    return ok;
}

stock HUD_TextCacheSelftest()
{
    new bool:ok = true;
    new a[32] = "FUEL 55%  43/80L 8km";
    new b[32] = "FUEL 55%  43/80L 8km";
    new c[32] = "FUEL 54%  43/80L 8km";
    new empty[32];

    ok = ok && HUD_TextSame(a, b, sizeof(a));          /* identik */
    ok = ok && !HUD_TextSame(a, c, sizeof(a));         /* beda satu digit */
    ok = ok && !HUD_TextSame(a, empty, sizeof(a));     /* kosong != berisi */
    ok = ok && HUD_TextSame(empty, empty, sizeof(empty));

    /* Penyalinan harus benar-benar mengisi cache: kegagalan di sini membuat
       SetIfChanged menulis ulang tiap 500 ms tanpa pernah mengubah layar. */
    HUD_TextStore(empty, a, sizeof(empty));
    ok = ok && HUD_TextSame(empty, a, sizeof(empty));

    /* Alias (dest == sumber) tidak boleh merusak isi. */
    HUD_TextStore(a, a, sizeof(a));
    ok = ok && HUD_TextSame(a, b, sizeof(a));

    /* Batas ukuran: dest 8 sel tidak boleh meluber, harus ter-terminasi. */
    new small[8];
    HUD_TextStore(small, c, sizeof(small));
    ok = ok && small[sizeof(small) - 1] == EOS;
    ok = ok && strlen(small) <= sizeof(small) - 1;

    Selftest_Record(ok, "cache baris HUD",
        ok ? "banding + salin + alias + batas ukuran benar" : "pembanding/penyalin string HUD salah");
    return ok;
}

stock HUD_MeasureSelftest()
{
    /* Tabel fonts.dat: lebar '0'=23, 'B'=22, spasi=15, '%'=34.
       Nilai ini yang membuat perhitungan lebar bisa dipercaya. */
    new bool:ok = true;
    ok = ok && TD_CharWidthOrZero('0') == 23;
    ok = ok && TD_CharWidthOrZero('A') == 22;
    ok = ok && TD_CharWidthOrZero('B') == 19;
    ok = ok && TD_CharWidthOrZero(' ') == 15;
    ok = ok && TD_CharWidthOrZero('%') == 34;
    /* Lebar "FUEL" dihitung dari tabel yang sama dengan pengukurnya — bukan
       angka hafalan. Kalau fonts.dat berubah, uji ini ikut bergerak. */
    new Float:fuelWidth = TD_TextWidth("FUEL", 0.17);
    new Float:fuelExpected = float(TD_CharWidthOrZero('F') + TD_CharWidthOrZero('U')
        + TD_CharWidthOrZero('E') + TD_CharWidthOrZero('L')) * 0.17;
    ok = ok && (fuelWidth - fuelExpected < 0.01) && (fuelExpected - fuelWidth < 0.01);

    /* Teks FUEL wajib muat sebelum bar mulai. */
    new Float:textWidth = TD_TextWidth("FUEL 12%  12/80L pom 240m BD", HUD_VEH_LETTER_X);
    ok = ok && textWidth <= (HUD_VEH_TEXT_W - 8.0);

    new detail[144];
    /* REGRESI nyata: TD_FitText dipanggil dengan dest == text. YSI mendefinisikan
       ulang strcpy sebagai strcat((dest[0]='\0', dest), src), jadi versi lama
       mengosongkan string lalu menyalin dari dirinya sendiri -> baris BBM jadi
       kosong dan tidak pernah tampil. Uji ini mengunci perilaku alias. */
    new aliasLine[80];
    format(aliasLine, sizeof(aliasLine), "FUEL 54%%  43.9/80L 120km");
    TD_FitText(aliasLine, sizeof(aliasLine), aliasLine, HUD_VEH_LETTER_X, 140.0);
    new bool:aliasOk = (strlen(aliasLine) > 4) && !strcmp(aliasLine, "FUEL 54%  43.9/80L 120km", false);

    new shortLine[32];
    format(shortLine, sizeof(shortLine), "FUEL sangat panjang sekali 999km");
    TD_FitText(shortLine, sizeof(shortLine), shortLine, HUD_VEH_LETTER_X, 40.0);
    new bool:trimOk = (strlen(shortLine) > 0) && (TD_TextWidth(shortLine, HUD_VEH_LETTER_X) <= 40.0);
    ok = ok && aliasOk && trimOk;

    format(detail, sizeof(detail), "fonts.dat '0'=23 'A'=22 'B'=19 spasi=15; FUEL %.0f/%.0f satuan (teks<batas baris)",
        textWidth, HUD_VEH_TEXT_W - 8.0);
    Selftest_Record(ok, "pengukur lebar textdraw", detail);
    return 1;
}

stock HUD_ResetCaches(playerid)
{
    gHudSetDbg[playerid] = 0;
    HudLastZone[playerid][0] = EOS;
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
    HudLastVehInfo[playerid][0] = EOS;
    HudLastSpeed[playerid][0] = EOS;
    HudLastGps[playerid][0] = EOS;
    HudVehicleShown[playerid] = false;
    HudVehInfoShown[playerid] = false;
    HudSpeedShown[playerid] = false;
    HudGpsShown[playerid] = false;
    HudGpsBarShown[playerid] = false;
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
    PlayerTextDrawShow(playerid, HudHpVal[playerid]);
    PlayerTextDrawShow(playerid, HudArmVal[playerid]);
    PlayerTextDrawShow(playerid, HudInfVal[playerid]);
    PlayerTextDrawShow(playerid, HudLapVal[playerid]);
    PlayerTextDrawShow(playerid, HudHausVal[playerid]);
    PlayerTextDrawShow(playerid, HudPipVal[playerid]);
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
    PlayerTextDrawShow(playerid, HudProgress[playerid]);
    PlayerTextDrawShow(playerid, HudMoney[playerid]);
    PlayerTextDrawShow(playerid, HudClock[playerid]);
    /* Kartu BBM TIDAK ditampilkan di sini: saat pemain turun dari kendaraan
       kartu kosong itu terlihat sebagai "kotak hitam tanpa isi". Kartu hanya
       muncul bersama baris BBM di HUD_Update. */

    PlayerHudVisible[playerid] = true;
    HudLastZone[playerid][0] = EOS;
    HudLastHpVal[playerid][0] = EOS;
    HudLastArmVal[playerid][0] = EOS;
    HudLastInfVal[playerid][0] = EOS;
    HudLastLapVal[playerid][0] = EOS;
    HudLastHausVal[playerid][0] = EOS;
    HudLastPipVal[playerid][0] = EOS;
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
    for (new seg = 0; seg < 7; seg++)
    {
        for (new cut = 0; cut < HUD_SEG_MAX_GAPS; cut++)
        {
            if (HudBarGap[playerid][seg][cut] != PlayerText:0)
                PlayerTextDrawHide(playerid, HudBarGap[playerid][seg][cut]);
        }
    }
    PlayerTextDrawHide(playerid, HudProgress[playerid]);
    PlayerTextDrawHide(playerid, HudMoney[playerid]);
    PlayerTextDrawHide(playerid, HudClock[playerid]);
    PlayerTextDrawHide(playerid, HudVehicle[playerid]);
    PlayerTextDrawHide(playerid, HudVehInfo[playerid]);
    PlayerTextDrawHide(playerid, HudSpeedVal[playerid]);
    PlayerTextDrawHide(playerid, HudSpeedUnit[playerid]);
    PlayerTextDrawHide(playerid, HudGpsText[playerid]);
    if (HudVehCard[playerid] != PlayerText:0) PlayerTextDrawHide(playerid, HudVehCard[playerid]);
    if (HudGpsCard[playerid] != PlayerText:0) PlayerTextDrawHide(playerid, HudGpsCard[playerid]);
    PlayerTextDrawHide(playerid, HudBoss[playerid]);
    PlayerTextDrawHide(playerid, HudRaid[playerid]);
    if (HudPromptBg[playerid] != PlayerText:0)
        PlayerTextDrawHide(playerid, HudPromptBg[playerid]);
    PlayerTextDrawHide(playerid, HudPromptText[playerid]);

    PlayerHudVisible[playerid] = false;
    HudVehicleShown[playerid] = false;
    HudVehInfoShown[playerid] = false;
    HudSpeedShown[playerid] = false;
    HudGpsShown[playerid] = false;
    HudGpsBarShown[playerid] = false;
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
    for (new seg = 0; seg < 7; seg++)
    {
        for (new cut = 0; cut < HUD_SEG_MAX_GAPS; cut++)
        {
            if (HudBarGap[playerid][seg][cut] != PlayerText:0)
                PlayerTextDrawDestroy(playerid, HudBarGap[playerid][seg][cut]);
            HudBarGap[playerid][seg][cut] = PlayerText:0;
        }
    }
    PlayerTextDrawDestroy(playerid, HudProgress[playerid]);
    PlayerTextDrawDestroy(playerid, HudMoney[playerid]);
    PlayerTextDrawDestroy(playerid, HudClock[playerid]);
    PlayerTextDrawDestroy(playerid, HudVehicle[playerid]);
    PlayerTextDrawDestroy(playerid, HudVehInfo[playerid]);
    PlayerTextDrawDestroy(playerid, HudSpeedVal[playerid]);
    PlayerTextDrawDestroy(playerid, HudSpeedUnit[playerid]);
    PlayerTextDrawDestroy(playerid, HudGpsText[playerid]);
    if (HudVehCard[playerid] != PlayerText:0)
    {
        PlayerTextDrawDestroy(playerid, HudVehCard[playerid]);
        HudVehCard[playerid] = PlayerText:0;
    }
    if (HudGpsCard[playerid] != PlayerText:0)
    {
        PlayerTextDrawDestroy(playerid, HudGpsCard[playerid]);
        HudGpsCard[playerid] = PlayerText:0;
    }
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

new bool:gHudUpdLogged[MAX_PLAYERS];

stock HUD_Update(playerid)
{
    /* Jejak pertama per pemain: memisahkan tiga sebab "HUD tidak pernah
       berubah" — tidak terhubung, HUD tidak ditandai tampil, atau NPC.
       Tanpa ini diagnosa hanya menebak (log hudtick hanya menunjukkan total). */
    if (!gHudUpdLogged[playerid])
    {
        gHudUpdLogged[playerid] = true;
        Log_Format(LOG_INFO, "hudupd %d: connected=%d hudVisible=%d npc=%d",
            playerid, IsPlayerConnected(playerid), PlayerHudVisible[playerid],
            IsPlayerNPC(playerid));
    }
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
            /* Hanya terjadi kalau pool 64 slot tangki habis atau model memang
               tanpa tangki (kereta/tram). "0%" menyesatkan — pakai tanda ".". */
            format(fuelLine, sizeof(fuelLine), "FUEL -  TANPA TANGKI");
        }
        else if (percent <= 0.25)
        {
            new pumpPart[40];
            Garage_FuelCompass(playerid, pumpPart, sizeof(pumpPart), true);
            format(fuelLine, sizeof(fuelLine), "FUEL %d%%  %.0f/%.0fL %s",
                floatround(percent * 100.0), fuel, tank, pumpPart);
        }
        else
        {
            format(fuelLine, sizeof(fuelLine), "FUEL %d%%  %.0f/%.0fL %.0fkm",
                floatround(percent * 100.0), fuel, tank, Vehicle_RangeKm(vehicleid));
        }

        /* Teks wajib muat di wilayah teks (140 satuan) selama bar tampil;
           saat bar disembunyikan, wilayah teks memakai seluruh lebar kotak.
           Diukur dengan tabel fonts.dat, bukan perkiraan jumlah karakter. */
        new Float:vehLetterX = HUD_VEH_LETTER_X * HUD_ScaleOf(playerid) * HUD_XComp(playerid);
        new Float:textBudget = (HUD_VEH_TEXT_W - 8.0) * HUD_ScaleOf(playerid) * HUD_XComp(playerid);
        TD_FitText(fuelLine, sizeof(fuelLine), fuelLine, vehLetterX, textBudget);

        /* Penjaga: kalau string tiba-tiba pendek (mis. hanya "BBM"), catat ke
           log supaya penyebabnya terlihat dari server, bukan dari tebakan. */
        if (strlen(fuelLine) < 6)
        {
            Log_Format(LOG_WARN, "fuel %d: string pendek '%s' (len=%d budget=%.0f lebar=%.0f)",
                playerid, fuelLine, strlen(fuelLine), textBudget,
                TD_TextWidth(fuelLine, vehLetterX));
        }
        else if (HudLastVehicle[playerid][0] == EOS)
        {
            Log_Format(LOG_INFO, "fuel %d: '%s' (len=%d budget=%.0f lebar=%.0f)",
                playerid, fuelLine, strlen(fuelLine), textBudget,
                TD_TextWidth(fuelLine, vehLetterX));
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

        /* Bar SELALU tampil selama kendaraan punya tangki. Dulu bar disembunyikan
           saat sisa <= 25%, dan itu justru menghapus satu-satunya cara pemain
           melihat sisa bensin ("ga tau sisa berapa bensinnya"). Level rendah
           tetap terbaca jelas dari warna bahaya + bar yang memendek. */
        if (fuel >= 0.0)
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
                SendClientMessage(playerid, UI_ACCENT_WARN, "[FUEL] Tangki tinggal seperempat. Isi di pom kamp (/isi) atau pakai jerrycan.");
            }
            else
            {
                SendClientMessage(playerid, UI_ACCENT_DANGER, "[FUEL] Hampir kering! /isi sekarang, mesin bisa mati kapan saja.");
            }
        }
        else if (percent > 0.30)
        {
            HudFuelWarnStage[playerid] = 0;
        }

        /* Baris identitas + speedometer kolom kanan. Plat dibaca dari data
           kendaraan milik server (native SA-MP tidak punya getter plat);
           status kunci datang dari Vehicle_IsLocked. */
        new bool:allowInfo = Settings_ModulEnabled(playerid, SETTINGS_MODUL_VEH);
        new bool:allowSpeed = Settings_ModulEnabled(playerid, SETTINGS_MODUL_SPEEDO);
        new lockText[12];
        format(lockText, sizeof(lockText), "%s", Vehicle_IsLocked(vehicleid) ? "TERKUNCI" : "TERBUKA");

        new infoLine[64];
        format(infoLine, sizeof(infoLine), "MOBIL %d  %s",
            GetVehicleModel(vehicleid), lockText);
        if (allowInfo)
        {
            HUD_SetIfChanged(playerid, HudVehInfo[playerid], infoLine,
                HudLastVehInfo[playerid], sizeof(HudLastVehInfo[]));
        }
        else if (HudVehInfoShown[playerid])
        {
            HudVehInfoShown[playerid] = false;
            PlayerTextDrawHide(playerid, HudVehInfo[playerid]);
        }

        new Float:velX, Float:velY, Float:velZ;
        GetVehicleVelocity(vehicleid, velX, velY, velZ);
        new kmh = floatround(floatsqroot(velX * velX + velY * velY + velZ * velZ) * 180.0);
        if (kmh < 0) kmh = 0;
        if (kmh > HUD_SPEED_MAX) kmh = HUD_SPEED_MAX;

        new speedLine[8];
        format(speedLine, sizeof(speedLine), "%d", kmh);
        if (allowSpeed)
        {
            HUD_SetIfChanged(playerid, HudSpeedVal[playerid], speedLine,
                HudLastSpeed[playerid], sizeof(HudLastSpeed[]));
        }
        else if (HudSpeedShown[playerid])
        {
            HudSpeedShown[playerid] = false;
            PlayerTextDrawHide(playerid, HudSpeedVal[playerid]);
            PlayerTextDrawHide(playerid, HudSpeedUnit[playerid]);
        }

        /* Speedometer: angka selalu tampil, bar hanya di gaya berkartu. */
        new speedColor = UI_ACCENT_PRIMARY;
        if (kmh >= HUD_SPEED_MAX) speedColor = UI_ACCENT_DANGER;
        else if (kmh >= HUD_SPEED_WARN) speedColor = UI_ACCENT_WARN;
        if (speedColor != HudFuelColor[playerid] && !HudVehicleShown[playerid])
            HudFuelColor[playerid] = speedColor;
        PlayerTextDrawColor(playerid, HudSpeedVal[playerid], speedColor);

        if (HUD_GayaVehBars(playerid) && allowSpeed)
        {
            if (HUD_BarVisible(playerid, HUD_BAR_KMH))
                HUD_SetBar(playerid, HUD_BAR_KMH, float(kmh) / float(HUD_SPEED_MAX), speedColor);
        }

        if (!HudVehicleShown[playerid])
        {
            HudVehicleShown[playerid] = true;
            PlayerTextDrawShow(playerid, HudVehicle[playerid]);
            if (allowInfo)
            {
                HudVehInfoShown[playerid] = true;
                PlayerTextDrawShow(playerid, HudVehInfo[playerid]);
            }
            if (allowSpeed)
            {
                HudSpeedShown[playerid] = true;
                PlayerTextDrawShow(playerid, HudSpeedVal[playerid]);
                PlayerTextDrawShow(playerid, HudSpeedUnit[playerid]);
                if (HUD_GayaVehBars(playerid) && HUD_BarVisible(playerid, HUD_BAR_KMH))
                    HUD_ShowBar(playerid, HUD_BAR_KMH, 5);
            }
            if (HudVehCard[playerid] != PlayerText:0)
                PlayerTextDrawShow(playerid, HudVehCard[playerid]);
            /* Satu baris per masuk kendaraan: cukup untuk membuktikan baris FUEL
               benar-benar tampil dan berapa angka yang dipakai, tanpa membanjiri
               log tiap 500 ms. */
            Log_Format(LOG_INFO, "hudin %d: veh=%d model=%d fuel=%.1f/%.0fL pct=%.0f%% teks='%s' bar=%d card=%d",
                playerid, vehicleid, GetVehicleModel(vehicleid), fuel, tank, percent * 100.0,
                HudLastVehicle[playerid], HudVehBarShown[playerid], _:HudVehCard[playerid]);
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
                if (HudVehCard[playerid] != PlayerText:0)
                    PlayerTextDrawShow(playerid, HudVehCard[playerid]);
                HUD_ShowBar(playerid, HUD_BAR_VEH, 3);
            }
        }
        else if (HudVehicleShown[playerid])
        {
            HudVehicleShown[playerid] = false;
            HudFuelWarnStage[playerid] = 0;
            PlayerTextDrawHide(playerid, HudVehicle[playerid]);
            PlayerTextDrawHide(playerid, HudVehInfo[playerid]);
            PlayerTextDrawHide(playerid, HudSpeedVal[playerid]);
            PlayerTextDrawHide(playerid, HudSpeedUnit[playerid]);
            HudVehInfoShown[playerid] = false;
            HudSpeedShown[playerid] = false;
            if (HUD_GayaVehBars(playerid))
                HUD_HideBar(playerid, HUD_BAR_KMH, 5);
            if (HudVehCard[playerid] != PlayerText:0)
                PlayerTextDrawHide(playerid, HudVehCard[playerid]);
            HudVehBarShown[playerid] = false;
            HUD_HideBar(playerid, HUD_BAR_VEH, 3);
        }
    }

    /* ---- GPS ------------------------------------------------------------
       Berjalan di dalam maupun di luar kendaraan: tujuan tetap ditunjuk saat
       pemain turun untuk mengambil barang atau kabur dari horde. */
    Gps_Tick(playerid);
    if (Gps_HasTarget(playerid) && Settings_ModulEnabled(playerid, SETTINGS_MODUL_GPS))
    {
        new gpsName[GPS_MAX_NAME];
        new gpsWord[3];
        new gpsLine[72];
        Gps_GetName(playerid, gpsName, sizeof(gpsName));
        Gps_DirectionWord(playerid, gpsWord, sizeof(gpsWord));
        /* Nama dibatasi 12 karakter: nickname party bisa 24 karakter dan akan
           memotong baris di tepi kartu. Jarak + arah tetap jadi informasi utama. */
        format(gpsLine, sizeof(gpsLine), "GPS %.0fm %s  %.12s",
            Gps_Distance(playerid), gpsWord, gpsName);
        HUD_SetIfChanged(playerid, HudGpsText[playerid], gpsLine,
            HudLastGps[playerid], sizeof(HudLastGps[]));

        if (HUD_GayaVehBars(playerid) && HUD_BarVisible(playerid, HUD_BAR_GPS))
        {
            HUD_SetBar(playerid, HUD_BAR_GPS, Gps_Progress(playerid), UI_ACCENT_INFO);
            if (!HudGpsBarShown[playerid])
            {
                HudGpsBarShown[playerid] = true;
                HUD_ShowBar(playerid, HUD_BAR_GPS, 6);
            }
        }

        if (!HudGpsShown[playerid])
        {
            HudGpsShown[playerid] = true;
            PlayerTextDrawShow(playerid, HudGpsText[playerid]);
            if (HudGpsCard[playerid] != PlayerText:0)
                PlayerTextDrawShow(playerid, HudGpsCard[playerid]);
        }
    }
    else if (HudGpsShown[playerid])
    {
        HudGpsShown[playerid] = false;
        PlayerTextDrawHide(playerid, HudGpsText[playerid]);
        if (HudGpsCard[playerid] != PlayerText:0)
            PlayerTextDrawHide(playerid, HudGpsCard[playerid]);
        if (HudGpsBarShown[playerid])
        {
            HudGpsBarShown[playerid] = false;
            HUD_HideBar(playerid, HUD_BAR_GPS, 6);
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

    // Progres skill harus terlihat tanpa membuka dialog: ini satu-satunya petunjuk
    // bahwa level dan poin skill bertambah. SAMPEL dihapus — sudah ada di kolom
    // kanan; dobel di dua baris adalah noise (audit UX).
    format(hudLine, sizeof(hudLine), "LVL %d  SP %d  LUKA %d  /skill",
        PlayerSurvival[playerid][psLevel], SkillPoints[playerid], Wounds_Get(playerid));
    HUD_SetIfChanged(playerid, HudProgress[playerid], hudLine, HudLastProgress[playerid], sizeof(HudLastProgress[]));

    new moneyLine[24];
    format(moneyLine, sizeof(moneyLine), "UANG $%d", GetPlayerMoney(playerid));
    HUD_SetIfChanged(playerid, HudMoney[playerid], moneyLine, HudLastMoney[playerid], sizeof(HudLastMoney[]));

    new hour, minute, second;
    gettime(hour, minute, second);
    new clockLine[16];
    format(clockLine, sizeof(clockLine), "JAM %02d:%02d", hour, minute);
    HUD_SetIfChanged(playerid, HudClock[playerid], clockLine, HudLastClock[playerid], sizeof(HudLastClock[]));

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
    new ticked = 0;
    /* Pemain nyata pertama dipakai sebagai sampel log. Dulu log selalu membaca
       slot 0, dan slot 0 biasanya NPC — akibatnya baris FUEL pemain sungguhan
       tidak pernah tercatat walau HUD-nya bermasalah. */
    new realPlayer = -1;
    for (new playerid = 0; playerid < MAX_PLAYERS; playerid++)
    {
        if (!IsPlayerConnected(playerid)) continue;
        if (realPlayer < 0 && !IsPlayerNPC(playerid)) realPlayer = playerid;
        /* Pemulihan mandiri: kalau textdraw HUD jelas sudah dibuat tapi flag
           tampil mati (pernah dilaporkan "HUD beku di angka awal"), nyalakan
           kembali dan catat — supaya tick tetap menulis tanpa perlu relog. */
        if (!PlayerHudVisible[playerid] && HudHpVal[playerid] != PlayerText:0
            && !IsPlayerNPC(playerid))
        {
            PlayerHudVisible[playerid] = true;
            Log_Format(LOG_WARN, "hudrepair %d: flag tampil dipulihkan (textdraw ada)", playerid);
        }
        HUD_Update(playerid);
        ticked++;
    }
    Perf_Update(PERF_HUD, perfStart);

    /* Denyut bukti: kalau pemain melaporkan "HUD tidak berubah", dua angka di
       sini langsung memisahkan sebabnya — tick tidak jalan (pemain=0) atau
       tidak ada baris yang ditulis (tulis tidak naik). */
    gHudTickCount++;
    if ((gHudTickCount % 24) == 0)
    {
        Log_Format(LOG_INFO, "hudtick %d: pemain=%d panggil=%d tulis=%d fuel='%s' hp='%s' sampel=%d",
            gHudTickCount, ticked, gHudSetCalls, gHudWriteCount,
            (realPlayer >= 0) ? HudLastVehicle[realPlayer] : "",
            (realPlayer >= 0) ? HudLastHpVal[realPlayer] : "", realPlayer);
    }
    return 1;
}
