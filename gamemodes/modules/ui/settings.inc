#if defined _ls_settings_included
    #endinput
#endif
#define _ls_settings_included

/* /settings — menu preferensi pemain yang tersimpan permanen (kolom
   characters.pref_*). Semua isian harus memengaruhi sistem nyata:

     0. Gaya HUD              : Auto / 1 Penuh / 2 Mobile Kompak / 3 Minimalis.
     1. Bar Detail            : Tampil / Sembunyi bar kendaraan & boss.
     2. Skala HUD             : 80% .. 120% (perkalian skala semua textdraw).
     3. Mode Kontras          : merah/oranye bar diganti warna kontras tinggi.
     4. Efek Layar Darah      : flash merah layar saat terluka, bisa dimatikan.
     5. Petunjuk Interaksi    : pil "[Y] <aksi>" di atas HUD tampil/sembunyi.
     6. Bahasa UI             : Indonesia / English (menu settings + pasang/lepas;
                                fase 1 — modul lain menyusul).
     7. Tombol Bangun Mobile  : panel bangun di kanan / kiri layar.

   Perubahan langsung aktif dan tersimpan ke database saat itu juga. */

#define SETTINGS_HUD_AUTO    0
#define SETTINGS_FLAG_ON     1
#define SETTINGS_FLAG_OFF    2

#define SETTINGS_SCALE_MIN   0
#define SETTINGS_SCALE_MAX   4   /* 0.8 .. 1.2 langkah 0.1 */

/* SETTINGS_MODUL_* ada di config/constants.inc: hud.inc memakainya dan
   di-include sebelum berkas ini. */
#define LANG_ID 1
#define LANG_EN 2

#define DIALOG_SETTINGS      2964

new gPrefHudGaya[MAX_PLAYERS];      /* 0 auto, 1..3 gaya HUD */
new gPrefHudSkin[MAX_PLAYERS];      /* HUD_SKIN_AUTO / KACA / NEON / TACTICAL / TERANG */
new gPrefHudDetail[MAX_PLAYERS];    /* 1 tampil bar detail, 2 sembunyi */
new gPrefHudScale[MAX_PLAYERS];     /* indeks 0..4 -> 0.8..1.2 */
new gPrefContrast[MAX_PLAYERS];     /* 1 normal, 2 kontras tinggi */
new gPrefBloodFx[MAX_PLAYERS];      /* 1 aktif, 2 mati */
new gPrefPrompt[MAX_PLAYERS];       /* 1 tampil, 2 sembunyi */
new gPrefLang[MAX_PLAYERS];         /* LANG_ID / LANG_EN */
new gPrefBeSide[MAX_PLAYERS];       /* 1 kanan, 2 kiri */
new gPrefTdMode[MAX_PLAYERS];       /* TD_MODE_AUTO / PC / MOBILE */
new gPrefHudModul[MAX_PLAYERS];     /* bitmask kolom kendaraan (default semua) */

new PlayerText:gHudBloodFx[MAX_PLAYERS];

/* Baris kolom kendaraan aktif? Dipakai hud.inc dan dialog settings. Bit yang
   tidak dikenal diabaikan, jadi menambah baris ke-4 tidak memaksa migrasi. */
stock bool:Settings_ModulEnabled(playerid, bit)
{
    return (gPrefHudModul[playerid] & bit) != 0;
}

stock Settings_ModulToggle(playerid, bit)
{
    gPrefHudModul[playerid] ^= bit;
    return 1;
}


forward Settings_OnLoaded(playerid);

stock Settings_Load(playerid)
{
    gPrefHudGaya[playerid] = SETTINGS_HUD_AUTO;
    gPrefHudSkin[playerid] = HUD_SKIN_AUTO;
    gPrefHudDetail[playerid] = SETTINGS_FLAG_ON;
    gPrefHudScale[playerid] = 2;    /* 100% */
    gPrefContrast[playerid] = SETTINGS_FLAG_ON;
    gPrefBloodFx[playerid] = SETTINGS_FLAG_ON;
    gPrefPrompt[playerid] = SETTINGS_FLAG_ON;
    gPrefLang[playerid] = LANG_ID;
    gPrefBeSide[playerid] = SETTINGS_FLAG_ON;
    gPrefTdMode[playerid] = TD_MODE_AUTO;
    gPrefHudModul[playerid] = SETTINGS_MODUL_ALL;
    gTdMobileSafe[playerid] = gHudMobileClient[playerid];
    if (ActiveCharId[playerid] == 0 || !SQL_IsReady()) return 0;

    new query[224];
    mysql_format(gSQL, query, sizeof(query),
        "SELECT pref_hud_gaya, pref_hud_skin, pref_hud_detail, pref_hud_scale, pref_contrast, \
                pref_blood_fx, pref_prompt, pref_lang, pref_be_side, \
                pref_td_mode, pref_hud_modul \
         FROM characters WHERE id = %d AND server_id = %d LIMIT 1",
        ActiveCharId[playerid], Server_InstanceId());
    mysql_tquery(gSQL, query, "Settings_OnLoaded", "i", playerid);
    return 1;
}

public Settings_OnLoaded(playerid)
{
    if (!IsPlayerConnected(playerid)) return 0;
    if (cache_num_rows() == 0) return 0;

    new gaya = 0, skin = HUD_SKIN_AUTO, detail = 1, scale = 2, contrast = 1, blood = 1, prompt = 1, lang = 1, side = 1, tdmode = 0;
    cache_get_value_name_int(0, "pref_hud_gaya", gaya);
    cache_get_value_name_int(0, "pref_hud_skin", skin);
    cache_get_value_name_int(0, "pref_hud_detail", detail);
    cache_get_value_name_int(0, "pref_hud_scale", scale);
    cache_get_value_name_int(0, "pref_contrast", contrast);
    cache_get_value_name_int(0, "pref_blood_fx", blood);
    cache_get_value_name_int(0, "pref_prompt", prompt);
    cache_get_value_name_int(0, "pref_lang", lang);
    cache_get_value_name_int(0, "pref_be_side", side);
    cache_get_value_name_int(0, "pref_td_mode", tdmode);
    new modul = SETTINGS_MODUL_ALL;
    cache_get_value_name_int(0, "pref_hud_modul", modul);

    if (gaya >= SETTINGS_HUD_AUTO && gaya <= HUD_GAYA_MINIMAL) gPrefHudGaya[playerid] = gaya;
    if (skin >= HUD_SKIN_AUTO && skin <= HUD_SKIN_COUNT) gPrefHudSkin[playerid] = skin;
    if (detail == SETTINGS_FLAG_ON || detail == SETTINGS_FLAG_OFF) gPrefHudDetail[playerid] = detail;
    if (scale >= SETTINGS_SCALE_MIN && scale <= SETTINGS_SCALE_MAX) gPrefHudScale[playerid] = scale;
    if (contrast == SETTINGS_FLAG_ON || contrast == SETTINGS_FLAG_OFF) gPrefContrast[playerid] = contrast;
    if (blood == SETTINGS_FLAG_ON || blood == SETTINGS_FLAG_OFF) gPrefBloodFx[playerid] = blood;
    if (prompt == SETTINGS_FLAG_ON || prompt == SETTINGS_FLAG_OFF) gPrefPrompt[playerid] = prompt;
    if (lang == LANG_ID || lang == LANG_EN) gPrefLang[playerid] = lang;
    if (side == SETTINGS_FLAG_ON || side == SETTINGS_FLAG_OFF) gPrefBeSide[playerid] = side;
    if (tdmode >= TD_MODE_AUTO && tdmode <= TD_MODE_MOBILE) gPrefTdMode[playerid] = tdmode;
    /* Nilai di luar 0..7 berarti baris ditulis versi lain: pakai default. */
    gPrefHudModul[playerid] = (modul >= 0 && modul <= SETTINGS_MODUL_ALL) ? modul : SETTINGS_MODUL_ALL;
    gTdMobileSafe[playerid] = Settings_TdMobileSafe(playerid);

    /* HUD belum tentu dibuat saat data preferensi tiba (asinkron). Kalau
       sudah ada, bangun ulang supaya preferensi langsung terlihat. */
    Settings_Apply(playerid);
    return 1;
}

/* Terapkan preferensi ke HUD yang sudah berdiri (atau simpan untuk berikutnya). */
stock Settings_Apply(playerid)
{
    if (gPrefHudGaya[playerid] != SETTINGS_HUD_AUTO)
    {
        gHudGaya[playerid] = gPrefHudGaya[playerid];
    }
    else
    {
        /* Kembali ke perilaku auto: reset agar HUD_Create memilih ulang. */
        gHudGaya[playerid] = 0;
    }

    /* Design HUD harus diresolusi SEBELUM HUD_Create: seluruh palet kartu
       dibaca dari gHudSkin saat textdraw dibuat (bukan di-recolour belakangan). */
    if (gPrefHudSkin[playerid] == HUD_SKIN_AUTO)
    {
        gHudSkin[playerid] = gHudMobileClient[playerid] ? HUD_SKIN_NEON : HUD_SKIN_DEFAULT;
    }
    else gHudSkin[playerid] = gPrefHudSkin[playerid];

    /* Mode renderer harus disinkronkan SEBELUM HUD dibangun: HUD_Create membaca
       Settings_TdMobileSafe untuk memilih box vs sprite. */
    gTdMobileSafe[playerid] = Settings_TdMobileSafe(playerid);
    HUD_Destroy(playerid);
    HUD_Create(playerid);
    HUD_Reshow(playerid, true);
    return 1;
}

stock Settings_Save(playerid)
{
    if (ActiveCharId[playerid] == 0 || !SQL_IsReady()) return 0;

    new query[360];
    mysql_format(gSQL, query, sizeof(query),
        "UPDATE characters SET pref_hud_gaya = %d, pref_hud_skin = %d, pref_hud_detail = %d, \
         pref_hud_scale = %d, pref_contrast = %d, pref_blood_fx = %d, \
         pref_prompt = %d, pref_lang = %d, pref_be_side = %d, pref_td_mode = %d, \
         pref_hud_modul = %d WHERE id = %d",
        gPrefHudGaya[playerid], gPrefHudSkin[playerid], gPrefHudDetail[playerid], gPrefHudScale[playerid],
        gPrefContrast[playerid], gPrefBloodFx[playerid], gPrefPrompt[playerid],
        gPrefLang[playerid], gPrefBeSide[playerid], gPrefTdMode[playerid],
        gPrefHudModul[playerid], ActiveCharId[playerid]);
    mysql_tquery(gSQL, query, "", "");
    return 1;
}

stock Settings_ResetPlayer(playerid)
{
    gPrefHudGaya[playerid] = SETTINGS_HUD_AUTO;
    gPrefHudDetail[playerid] = SETTINGS_FLAG_ON;
    gPrefHudScale[playerid] = 2;
    gPrefContrast[playerid] = SETTINGS_FLAG_ON;
    gPrefBloodFx[playerid] = SETTINGS_FLAG_ON;
    gPrefPrompt[playerid] = SETTINGS_FLAG_ON;
    gPrefLang[playerid] = LANG_ID;
    gPrefBeSide[playerid] = SETTINGS_FLAG_ON;
    gPrefTdMode[playerid] = TD_MODE_AUTO;
    gTdMobileSafe[playerid] = gHudMobileClient[playerid];
    if (gHudBloodFx[playerid] != PlayerText:0)
    {
        PlayerTextDrawDestroy(playerid, gHudBloodFx[playerid]);
        gHudBloodFx[playerid] = PlayerText:0;
    }
    return 1;
}

/* ---------------------------------------------------- pembaca preferensi */

stock Float:Settings_HudScale(playerid)
{
    return 0.8 + 0.1 * float(gPrefHudScale[playerid]);
}

stock bool:Settings_BarDetailEnabled(playerid)
{
    return (gPrefHudDetail[playerid] != SETTINGS_FLAG_OFF) ? true : false;
}

stock bool:Settings_ContrastEnabled(playerid)
{
    return (gPrefContrast[playerid] == SETTINGS_FLAG_OFF) ? false : true;
}

stock bool:Settings_PromptEnabled(playerid)
{
    return (gPrefPrompt[playerid] != SETTINGS_FLAG_OFF) ? true : false;
}

stock Settings_Lang(playerid) return gPrefLang[playerid];

/* Salin teks sesuai bahasa pemain. Fase 1 i18n: dipakai menu settings dan
   dialog pasang/lepas modifikasi senjata. */
stock Settings_LCopy(playerid, dest[], destSize, const idText[], const enText[])
{
    dest[0] = EOS;
    strcat(dest, (gPrefLang[playerid] == LANG_EN) ? enText : idText, destSize);
    return 1;
}

/* Panel bangun mobile: 1 = kanan (default), 2 = kiri. */
/* ------------------------------------------------------------- mode renderer */
stock bool:Settings_TdMobileSafe(playerid)
{
    if (gPrefTdMode[playerid] == TD_MODE_MOBILE) return true;
    if (gPrefTdMode[playerid] == TD_MODE_PC) return false;
    return gHudMobileClient[playerid];      /* auto: ikut klien */
}

stock Float:Settings_BuildPanelX(playerid)
{
    return (gPrefBeSide[playerid] == SETTINGS_FLAG_OFF) ? 8.2 : 425.8;
}

/* ---------------------------------------------------- efek layar darah */

stock Settings_BloodFlash(playerid)
{
    if (gPrefBloodFx[playerid] != SETTINGS_FLAG_ON) return 0;

    if (gHudBloodFx[playerid] == PlayerText:0)
    {
        /* Box penuh, bukan sprite: klien Android menggambar sprite fullscreen
           terlalu besar sehingga menutupi HUD (laporan pemain). */
        gHudBloodFx[playerid] = TD_BoxCreate(playerid, 0.0, 0.0, 640.0, 480.0, 0x8B000060);
    }
    PlayerTextDrawShow(playerid, gHudBloodFx[playerid]);
    SetTimerEx("Settings_HideBloodFx", 400, false, "i", playerid);
    return 1;
}

forward Settings_HideBloodFx(playerid);

public Settings_HideBloodFx(playerid)
{
    if (!IsPlayerConnected(playerid)) return 0;
    if (gHudBloodFx[playerid] != PlayerText:0)
    {
        PlayerTextDrawHide(playerid, gHudBloodFx[playerid]);
    }
    return 1;
}

/* ------------------------------------------------------------- menu */

/* ------------------------------------------------------- tabel baris menu
   DIALOG_STYLE_TABLIST_HEADERS memakai baris PERTAMA sebagai judul kolom dan
   baris itu TIDAK bisa diklik. Menu ini dulu tidak punya baris judul, jadi
   setiap klik bergeser satu: menekan "Design HUD" mengubah "Gaya HUD".
   Sekarang ada baris judul DAN aksi dipilih lewat jenis baris (bukan nomor),
   sehingga menambah/menghapus baris tidak pernah menggeser fungsi. */

#define SET_ROW_GAYA        0
#define SET_ROW_SKIN        1
#define SET_ROW_DETAIL      2
#define SET_ROW_SCALE       3
#define SET_ROW_KONTRAST    4
#define SET_ROW_BLOOD       5
#define SET_ROW_PROMPT      6
#define SET_ROW_LANG        7
#define SET_ROW_BESIDE      8
#define SET_ROW_TDMODE      9
#define SET_ROW_MOD_VEH     10
#define SET_ROW_MOD_SPEEDO  11
#define SET_ROW_MOD_GPS     12
#define SET_ROW_COUNT       13

new gSetRowKind[MAX_PLAYERS][SET_ROW_COUNT];
new gSetRowCount[MAX_PLAYERS];

stock Settings_RowLabel(kind, dest[], size)
{
    switch (kind)
    {
        case SET_ROW_GAYA:        strcpy(dest, "Gaya HUD", size);
        case SET_ROW_SKIN:        strcpy(dest, "Design HUD", size);
        case SET_ROW_DETAIL:      strcpy(dest, "Bar Detail (Kendaraan & Boss)", size);
        case SET_ROW_SCALE:       strcpy(dest, "Skala HUD", size);
        case SET_ROW_KONTRAST:    strcpy(dest, "Mode Kontras", size);
        case SET_ROW_BLOOD:       strcpy(dest, "Efek Layar Darah", size);
        case SET_ROW_PROMPT:      strcpy(dest, "Petunjuk Interaksi", size);
        case SET_ROW_LANG:        strcpy(dest, "Bahasa UI", size);
        case SET_ROW_BESIDE:      strcpy(dest, "Tombol Bangun (Mobile)", size);
        case SET_ROW_TDMODE:      strcpy(dest, "Mode TextDraw", size);
        case SET_ROW_MOD_VEH:     strcpy(dest, "Baris MOBIL (model & kunci)", size);
        case SET_ROW_MOD_SPEEDO:  strcpy(dest, "Speedometer (km/j)", size);
        case SET_ROW_MOD_GPS:     strcpy(dest, "GPS / penunjuk arah", size);
        default:                  strcpy(dest, "-", size);
    }
    return 1;
}

stock Settings_RowValue(playerid, kind, dest[], size)
{
    switch (kind)
    {
        case SET_ROW_GAYA:
        {
            switch (gPrefHudGaya[playerid])
            {
                case HUD_GAYA_PENUH:   strcpy(dest, "1 - Penuh", size);
                case HUD_GAYA_MOBILE:  strcpy(dest, "2 - Mobile Kompak", size);
                case HUD_GAYA_MINIMAL: strcpy(dest, "3 - Minimalis", size);
                default:               strcpy(dest, "Auto (ikut klien)", size);
            }
        }
        case SET_ROW_SKIN:
        {
            if (gPrefHudSkin[playerid] == HUD_SKIN_AUTO) strcpy(dest, "Auto (ikut klien)", size);
            else format(dest, size, "%d - %s", gPrefHudSkin[playerid],
                gHudSkins[gPrefHudSkin[playerid]][skinName]);
        }
        case SET_ROW_DETAIL:
            strcpy(dest, (gPrefHudDetail[playerid] == SETTINGS_FLAG_OFF) ? "Sembunyi" : "Tampil", size);
        case SET_ROW_SCALE:
            format(dest, size, "%d%%", 80 + gPrefHudScale[playerid] * 10);
        case SET_ROW_KONTRAST:
            strcpy(dest, (gPrefContrast[playerid] == SETTINGS_FLAG_OFF) ? "Normal" : "Kontras Tinggi", size);
        case SET_ROW_BLOOD:
            strcpy(dest, (gPrefBloodFx[playerid] == SETTINGS_FLAG_OFF) ? "Mati" : "Aktif", size);
        case SET_ROW_PROMPT:
            strcpy(dest, (gPrefPrompt[playerid] == SETTINGS_FLAG_OFF) ? "Sembunyi" : "Tampil", size);
        case SET_ROW_LANG:
            strcpy(dest, (gPrefLang[playerid] == LANG_EN) ? "English" : "Indonesia", size);
        case SET_ROW_BESIDE:
            strcpy(dest, (gPrefBeSide[playerid] == SETTINGS_FLAG_OFF) ? "Kiri" : "Kanan", size);
        case SET_ROW_TDMODE:
        {
            switch (gPrefTdMode[playerid])
            {
                case TD_MODE_PC:     strcpy(dest, "PC (sprite)", size);
                case TD_MODE_MOBILE: strcpy(dest, "Mobile-safe (box)", size);
                default:             strcpy(dest, "Auto (ikut klien)", size);
            }
        }
        case SET_ROW_MOD_VEH:
            strcpy(dest, Settings_ModulEnabled(playerid, SETTINGS_MODUL_VEH) ? "Tampil" : "Sembunyi", size);
        case SET_ROW_MOD_SPEEDO:
            strcpy(dest, Settings_ModulEnabled(playerid, SETTINGS_MODUL_SPEEDO) ? "Tampil" : "Sembunyi", size);
        case SET_ROW_MOD_GPS:
            strcpy(dest, Settings_ModulEnabled(playerid, SETTINGS_MODUL_GPS) ? "Tampil" : "Sembunyi", size);
        default: strcpy(dest, "-", size);
    }
    return 1;
}

stock Settings_Show(playerid)
{
    if (!Account_IsPlaying(playerid)) return 0;

    new row[128], body[1280], label[40], value[28];

    /* Baris judul wajib: tanpa ini semua listitem bergeser satu. */
    body[0] = EOS;
    strcat(body, "Menu\tNilai\n", sizeof(body));

    gSetRowCount[playerid] = SET_ROW_COUNT;
    for (new slot = 0; slot < SET_ROW_COUNT; slot++)
    {
        new kind = slot;
        gSetRowKind[playerid][slot] = kind;
        Settings_RowLabel(kind, label, sizeof(label));
        Settings_RowValue(playerid, kind, value, sizeof(value));
        format(row, sizeof(row), "%s\t%s\n", label, value);
        strcat(body, row, sizeof(body));
    }

    ShowPlayerDialog(playerid, DIALOG_SETTINGS, DIALOG_STYLE_TABLIST_HEADERS,
        "Pengaturan", body, "Ubah", "Tutup");
    return 1;
}

stock Settings_ToggleFromRow(playerid, row)
{
    if (row < 0 || row >= gSetRowCount[playerid]) return 0;

    new kind = gSetRowKind[playerid][row];
    new msg[128], label[40], value[28];

    switch (kind)
    {
        case SET_ROW_GAYA:
        {
            gPrefHudGaya[playerid]++;
            if (gPrefHudGaya[playerid] > HUD_GAYA_MINIMAL) gPrefHudGaya[playerid] = SETTINGS_HUD_AUTO;
        }
        case SET_ROW_SKIN:
        {
            gPrefHudSkin[playerid]++;
            if (gPrefHudSkin[playerid] > HUD_SKIN_COUNT) gPrefHudSkin[playerid] = HUD_SKIN_AUTO;
        }
        case SET_ROW_DETAIL:
        {
            gPrefHudDetail[playerid] = (gPrefHudDetail[playerid] == SETTINGS_FLAG_OFF)
                ? SETTINGS_FLAG_ON : SETTINGS_FLAG_OFF;
        }
        case SET_ROW_SCALE:
        {
            gPrefHudScale[playerid]++;
            if (gPrefHudScale[playerid] > SETTINGS_SCALE_MAX) gPrefHudScale[playerid] = SETTINGS_SCALE_MIN;
        }
        case SET_ROW_KONTRAST:
        {
            gPrefContrast[playerid] = (gPrefContrast[playerid] == SETTINGS_FLAG_OFF)
                ? SETTINGS_FLAG_ON : SETTINGS_FLAG_OFF;
        }
        case SET_ROW_BLOOD:
        {
            gPrefBloodFx[playerid] = (gPrefBloodFx[playerid] == SETTINGS_FLAG_OFF)
                ? SETTINGS_FLAG_ON : SETTINGS_FLAG_OFF;
        }
        case SET_ROW_PROMPT:
        {
            gPrefPrompt[playerid] = (gPrefPrompt[playerid] == SETTINGS_FLAG_OFF)
                ? SETTINGS_FLAG_ON : SETTINGS_FLAG_OFF;
        }
        case SET_ROW_LANG:
        {
            gPrefLang[playerid] = (gPrefLang[playerid] == LANG_EN) ? LANG_ID : LANG_EN;
        }
        case SET_ROW_BESIDE:
        {
            gPrefBeSide[playerid] = (gPrefBeSide[playerid] == SETTINGS_FLAG_OFF)
                ? SETTINGS_FLAG_ON : SETTINGS_FLAG_OFF;
        }
        case SET_ROW_TDMODE:
        {
            gPrefTdMode[playerid]++;
            if (gPrefTdMode[playerid] > TD_MODE_MOBILE) gPrefTdMode[playerid] = TD_MODE_AUTO;
        }
        case SET_ROW_MOD_VEH:    Settings_ModulToggle(playerid, SETTINGS_MODUL_VEH);
        case SET_ROW_MOD_SPEEDO: Settings_ModulToggle(playerid, SETTINGS_MODUL_SPEEDO);
        case SET_ROW_MOD_GPS:    Settings_ModulToggle(playerid, SETTINGS_MODUL_GPS);
        default: return 0;
    }

    /* Umpan balik menyebut baris yang BENAR diubah, supaya pemain bisa
       mencocokkan klik dengan hasilnya (keluhan "dipencet beda hasilnya"). */
    Settings_RowLabel(kind, label, sizeof(label));
    Settings_RowValue(playerid, kind, value, sizeof(value));
    format(msg, sizeof(msg), "[SETELAN] %s -> %s", label, value);
    SendClientMessage(playerid, UI_ACCENT_PRIMARY, msg);
    Log_Format(LOG_INFO, "settings %d: baris %d kind=%d -> %s", playerid, row, kind, value);

    Settings_Apply(playerid);
    Settings_Save(playerid);
    Settings_Show(playerid);   /* buka ulang supaya nilai baru terlihat */
    return 1;
}

/* --------------------------------------------------------- /tddebug */

new PlayerText:gTdCalib[MAX_PLAYERS][6];
new bool:gTdCalibOn[MAX_PLAYERS];

/* Gambar pola kalibrasi 640x480: 4 sudut + titik tengah + label mode. Dipakai
   untuk membuktikan apakah klien (terutama Android) memetakan koordinat
   textdraw dengan benar, tanpa perlu menebak dari HUD yang padat. */
forward Settings_OnDialogResponse(playerid, dialogid, response, listitem, inputtext[]);

public Settings_OnDialogResponse(playerid, dialogid, response, listitem, inputtext[])
{
    #pragma unused inputtext

    if (dialogid == DIALOG_SETTINGS)
    {
        if (response) Settings_ToggleFromRow(playerid, listitem);
        return 1;
    }
    return 0;
}

/* --------------------------------------------------------- /tddebug */

stock Settings_TdDebug(playerid)
{
    if (!IsPlayerConnected(playerid)) return 0;

    new msg[144];
    if (gTdCalibOn[playerid])
    {
        for (new slot = 0; slot < 6; slot++)
        {
            if (gTdCalib[playerid][slot] != PlayerText:0)
            {
                PlayerTextDrawDestroy(playerid, gTdCalib[playerid][slot]);
                gTdCalib[playerid][slot] = PlayerText:0;
            }
        }
        gTdCalibOn[playerid] = false;
        SendClientMessage(playerid, UI_ACCENT_WARN, "[TDD] Pola kalibrasi dimatikan.");
        return 1;
    }

    gTdCalib[playerid][0] = TD_BoxCreate(playerid, 0.0, 0.0, 120.0, 24.0, 0x10B981AA);
    gTdCalib[playerid][1] = TD_BoxCreate(playerid, 520.0, 0.0, 120.0, 24.0, 0xF59E0BAA);
    gTdCalib[playerid][2] = TD_BoxCreate(playerid, 0.0, 456.0, 120.0, 24.0, 0xEF4444AA);
    gTdCalib[playerid][3] = TD_BoxCreate(playerid, 520.0, 456.0, 120.0, 24.0, 0x3B82F6AA);
    gTdCalib[playerid][4] = TD_BoxCreate(playerid, 310.0, 230.0, 20.0, 20.0, 0xFFFFFFFF);
    for (new slot = 0; slot < 5; slot++) PlayerTextDrawShow(playerid, gTdCalib[playerid][slot]);

    format(msg, sizeof(msg), "640x480 | MODE %s | GAYA %d | SKALA %d%% | SCRAP %d | SAMPEL %d",
        Settings_TdMobileSafe(playerid) ? "MOBILE-SAFE" : "PC-SPRITE",
        gHudGaya[playerid], 80 + gPrefHudScale[playerid] * 10,
        Inventory_CountItem(playerid, ITEM_SCRAP), Inventory_CountItem(playerid, ITEM_SPECIMEN));
    gTdCalib[playerid][5] = CreatePlayerTextDraw(playerid, 20.0, 30.0, msg);
    PlayerTextDrawFont(playerid, gTdCalib[playerid][5], 1);
    PlayerTextDrawLetterSize(playerid, gTdCalib[playerid][5], 0.22, 0.9);
    PlayerTextDrawColor(playerid, gTdCalib[playerid][5], 0xFFFFFFFF);
    PlayerTextDrawSetOutline(playerid, gTdCalib[playerid][5], 1);
    PlayerTextDrawBackgroundColor(playerid, gTdCalib[playerid][5], 0x000000AA);
    PlayerTextDrawSetSelectable(playerid, gTdCalib[playerid][5], false);
    PlayerTextDrawShow(playerid, gTdCalib[playerid][5]);
    gTdCalibOn[playerid] = true;

    HUD_AuditRows(playerid);

    SendClientMessage(playerid, UI_ACCENT_PRIMARY, "[TDD] Pola kalibrasi aktif. Merah=kiri-bawah, biru=kanan-bawah, putih=tengah.");
    Log_Info(msg);
    return 1;
}

stock Settings_TdDebugReset(playerid)
{
    for (new slot = 0; slot < 6; slot++)
    {
        if (gTdCalib[playerid][slot] != PlayerText:0)
        {
            PlayerTextDrawDestroy(playerid, gTdCalib[playerid][slot]);
            gTdCalib[playerid][slot] = PlayerText:0;
        }
    }
    gTdCalibOn[playerid] = false;
    return 1;
}

stock Settings_Selftest()
{
    new bool:ok = true;

    /* Setiap baris menu wajib punya label DAN nilai yang bisa dibaca. Dulu
       menu memakai DIALOG_STYLE_TABLIST_HEADERS tanpa baris judul, sehingga
       baris pertama dimakan sebagai header dan semua klik bergeser satu
       (menekan "Design HUD" mengubah "Gaya HUD"). */
    new rowLabel[40], rowValue[28];
    new bool:okMenu = true;
    for (new kind = 0; kind < SET_ROW_COUNT; kind++)
    {
        Settings_RowLabel(kind, rowLabel, sizeof(rowLabel));
        Settings_RowValue(0, kind, rowValue, sizeof(rowValue));
        okMenu = okMenu && rowLabel[0] != EOS && strcmp(rowLabel, "-") != 0;
        okMenu = okMenu && rowValue[0] != EOS && strcmp(rowValue, "-") != 0;
    }
    okMenu = okMenu && SET_ROW_SKIN == 1 && SET_ROW_TDMODE == 9 && SET_ROW_COUNT == 13;

    /* Rentang gaya HUD konsisten dengan konstanta hud.inc. */
    ok = ok && HUD_GAYA_PENUH == 1 && HUD_GAYA_MOBILE == 2 && HUD_GAYA_MINIMAL == 3;
    ok = ok && SETTINGS_HUD_AUTO == 0 && SETTINGS_FLAG_ON == 1 && SETTINGS_FLAG_OFF == 2;

    /* Siklus gaya: auto -> 1 -> 2 -> 3 -> auto (logika Settings_ToggleFromRow). */
    gPrefHudGaya[0] = SETTINGS_HUD_AUTO;
    gPrefHudGaya[0]++;
    ok = ok && gPrefHudGaya[0] == HUD_GAYA_PENUH;
    gPrefHudGaya[0] = HUD_GAYA_MINIMAL;
    gPrefHudGaya[0]++;
    if (gPrefHudGaya[0] > HUD_GAYA_MINIMAL) gPrefHudGaya[0] = SETTINGS_HUD_AUTO;
    ok = ok && gPrefHudGaya[0] == SETTINGS_HUD_AUTO;

    /* Toggle bar detail bolak-balik. */
    gPrefHudDetail[0] = SETTINGS_FLAG_ON;
    gPrefHudDetail[0] = (gPrefHudDetail[0] == SETTINGS_FLAG_OFF) ? SETTINGS_FLAG_ON : SETTINGS_FLAG_OFF;
    ok = ok && gPrefHudDetail[0] == SETTINGS_FLAG_OFF;
    gPrefHudDetail[0] = (gPrefHudDetail[0] == SETTINGS_FLAG_OFF) ? SETTINGS_FLAG_ON : SETTINGS_FLAG_OFF;
    ok = ok && gPrefHudDetail[0] == SETTINGS_FLAG_ON;

    /* Skala: indeks 0..4 -> 0.8..1.2 (bandingkan lewat x10 — float 0.1/0.4
       tidak eksak biner, jadi perbandingan langsung == 1.2 bisa gagal). */
    ok = ok && floatround(Settings_HudScale(0) * 10.0) == 8;
    ok = ok && floatround(Settings_HudScale(2) * 10.0) == 10;
    ok = ok && floatround(Settings_HudScale(4) * 10.0) == 12;

    /* Bahasa: helper menyalin teks sesuai preferensi. */
    new langBuf[32];
    gPrefLang[0] = LANG_ID;
    Settings_LCopy(0, langBuf, sizeof(langBuf), "Pengaturan", "Settings");
    ok = ok && !strcmp(langBuf, "Pengaturan");
    gPrefLang[0] = LANG_EN;
    Settings_LCopy(0, langBuf, sizeof(langBuf), "Pengaturan", "Settings");
    ok = ok && !strcmp(langBuf, "Settings");
    gPrefLang[0] = LANG_ID;

    /* Mode renderer textdraw: mobile-safe wajib mutlak saat dipilih manual,
       dan auto mengikuti deteksi klien (akar laporan layout Android rusak). */
    gPrefTdMode[0] = TD_MODE_MOBILE;
    ok = ok && Settings_TdMobileSafe(0) == true;
    gPrefTdMode[0] = TD_MODE_PC;
    ok = ok && Settings_TdMobileSafe(0) == false;
    gHudMobileClient[0] = true;
    gPrefTdMode[0] = TD_MODE_AUTO;
    ok = ok && Settings_TdMobileSafe(0) == true;
    gHudMobileClient[0] = false;
    ok = ok && Settings_TdMobileSafe(0) == false;

    /* REGRESI "nilai HUD basi setelah dibangun ulang": cache teks HUD harus
       kosong setelah HUD_Create, kalau tidak HUD_SetIfChanged melewati
       penulisan (dulu tampil "SCRAP 0 padahal ada 50"). */
    strcpy(HudLastVehicle[0], "BBM 0%", 48);
    strcpy(HudLastProgress[0], "LVL 1  SP 0  LUKA 0  /skill", 64);
    HUD_ResetCaches(0);
    ok = ok && HudLastVehicle[0][0] == EOS && HudLastProgress[0][0] == EOS;
    ok = ok && HudBarLastWidth[0][0] == -1;

    /* Panel bangun mobile: kanan vs kiri. */
    gPrefBeSide[0] = SETTINGS_FLAG_ON;
    ok = ok && Settings_BuildPanelX(0) == 425.8;
    gPrefBeSide[0] = SETTINGS_FLAG_OFF;
    ok = ok && Settings_BuildPanelX(0) == 8.2;
    gPrefBeSide[0] = SETTINGS_FLAG_ON;

    new bool:okGaya = (HUD_GAYA_PENUH == 1 && HUD_GAYA_MOBILE == 2 && HUD_GAYA_MINIMAL == 3
        && SETTINGS_HUD_AUTO == 0 && SETTINGS_FLAG_ON == 1 && SETTINGS_FLAG_OFF == 2);
    gPrefHudScale[0] = 0;
    new bool:okSkala = (floatround(Settings_HudScale(0) * 10.0) == 8);
    gPrefHudScale[0] = 2;
    okSkala = okSkala && floatround(Settings_HudScale(0) * 10.0) == 10;
    gPrefHudScale[0] = 4;
    okSkala = okSkala && floatround(Settings_HudScale(0) * 10.0) == 12;
    gPrefHudScale[0] = 2;
    new bool:okLang = (gPrefLang[0] == LANG_ID);
    new bool:okPanel = (Settings_BuildPanelX(0) == 425.8);
    Selftest_Record(okGaya, "menu pengaturan (gaya)",
        okGaya ? "rentang gaya + flag konsisten" : "gaya gagal");
    Selftest_Record(okSkala, "menu pengaturan (skala)",
        okSkala ? "0.8x..1.2x konsisten" : "skala gagal");
    Selftest_Record(okLang, "menu pengaturan (bahasa)",
        okLang ? "helper bahasa konsisten" : "bahasa gagal");
    Selftest_Record(okMenu, "menu setelan (baris)",
        okMenu ? "13 baris + baris judul, label & nilai terisi semua"
               : "baris menu tanpa label/nilai atau jumlah baris bergeser");
    Selftest_Record(okPanel, "menu pengaturan (panel)",
        okPanel ? "sisi panel kanan/kiri konsisten" : "panel gagal");
    return 1;
}
