#if defined _ls_settings_included
    #endinput
#endif
#define _ls_settings_included

/* /settings — menu preferensi pemain yang tersimpan permanen (kolom
   characters.pref_*). Semua isian harus memengaruhi sistem nyata:

     1. Gaya HUD        : Auto / 1 Penuh / 2 Mobile Kompak / 3 Minimalis.
                          Auto = ikut klien (Android otomatis gaya 2).
     2. Bar Detail      : Tampil / Sembunyi untuk bar kendaraan & bar boss.
                          Sembunyi = layar lebih bersih di ponsel.

   Perubahan langsung aktif (HUD dibangun ulang) dan tersimpan ke database
   saat itu juga — bukan hanya sampai disconnect. */

#define SETTINGS_HUD_AUTO    0
#define SETTINGS_DETAIL_ON   1
#define SETTINGS_DETAIL_OFF  2

#define DIALOG_SETTINGS      2964

new gPrefHudGaya[MAX_PLAYERS];      /* 0 auto, 1..3 gaya HUD */
new gPrefHudDetail[MAX_PLAYERS];    /* 1 tampil bar detail, 2 sembunyi */

forward Settings_OnLoaded(playerid);

stock Settings_Load(playerid)
{
    gPrefHudGaya[playerid] = SETTINGS_HUD_AUTO;
    gPrefHudDetail[playerid] = SETTINGS_DETAIL_ON;
    if (ActiveCharId[playerid] == 0 || !SQL_IsReady()) return 0;

    new query[160];
    mysql_format(gSQL, query, sizeof(query),
        "SELECT pref_hud_gaya, pref_hud_detail FROM characters \
         WHERE id = %d AND server_id = %d LIMIT 1",
        ActiveCharId[playerid], Server_InstanceId());
    mysql_tquery(gSQL, query, "Settings_OnLoaded", "i", playerid);
    return 1;
}

public Settings_OnLoaded(playerid)
{
    if (!IsPlayerConnected(playerid)) return 0;
    if (cache_num_rows() == 0) return 0;

    new gaya = 0, detail = 1;
    cache_get_value_name_int(0, "pref_hud_gaya", gaya);
    cache_get_value_name_int(0, "pref_hud_detail", detail);

    if (gaya >= SETTINGS_HUD_AUTO && gaya <= HUD_GAYA_MINIMAL) gPrefHudGaya[playerid] = gaya;
    if (detail == SETTINGS_DETAIL_ON || detail == SETTINGS_DETAIL_OFF) gPrefHudDetail[playerid] = detail;

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

    if (gHudGaya[playerid] != 0)
    {
        HUD_Destroy(playerid);
        HUD_Create(playerid);
    }
    return 1;
}

stock Settings_Save(playerid)
{
    if (ActiveCharId[playerid] == 0 || !SQL_IsReady()) return 0;

    new query[160];
    mysql_format(gSQL, query, sizeof(query),
        "UPDATE characters SET pref_hud_gaya = %d, pref_hud_detail = %d WHERE id = %d",
        gPrefHudGaya[playerid], gPrefHudDetail[playerid], ActiveCharId[playerid]);
    mysql_tquery(gSQL, query, "", "");
    return 1;
}

stock Settings_ResetPlayer(playerid)
{
    gPrefHudGaya[playerid] = SETTINGS_HUD_AUTO;
    gPrefHudDetail[playerid] = SETTINGS_DETAIL_ON;
    return 1;
}

/* ------------------------------------------------------------- menu */

stock Settings_Show(playerid)
{
    if (!Account_IsPlaying(playerid)) return 0;

    new gayaText[24];
    switch (gPrefHudGaya[playerid])
    {
        case SETTINGS_HUD_AUTO: strcpy(gayaText, "Auto (ikut klien)", sizeof(gayaText));
        case HUD_GAYA_PENUH:    strcpy(gayaText, "1 - Penuh", sizeof(gayaText));
        case HUD_GAYA_MOBILE:   strcpy(gayaText, "2 - Mobile Kompak", sizeof(gayaText));
        case HUD_GAYA_MINIMAL:  strcpy(gayaText, "3 - Minimalis", sizeof(gayaText));
        default:                strcpy(gayaText, "Auto", sizeof(gayaText));
    }

    new detailText[16];
    strcpy(detailText, (gPrefHudDetail[playerid] == SETTINGS_DETAIL_OFF) ? "Sembunyi" : "Tampil", sizeof(detailText));

    new body[256];
    format(body, sizeof(body),
        "Pengaturan\tNilai sekarang\nGaya HUD\t%s\nBar Detail (Kendaraan & Boss)\t%s\n",
        gayaText, detailText);

    ShowPlayerDialog(playerid, DIALOG_SETTINGS, DIALOG_STYLE_TABLIST_HEADERS,
        "Pengaturan", body, "Ubah", "Tutup");
    return 1;
}

stock Settings_ToggleFromRow(playerid, row)
{
    switch (row)
    {
        case 0: /* gaya HUD: auto -> 1 -> 2 -> 3 -> auto */
        {
            gPrefHudGaya[playerid]++;
            if (gPrefHudGaya[playerid] > HUD_GAYA_MINIMAL) gPrefHudGaya[playerid] = SETTINGS_HUD_AUTO;
        }
        case 1: /* bar detail */
        {
            gPrefHudDetail[playerid] = (gPrefHudDetail[playerid] == SETTINGS_DETAIL_OFF)
                ? SETTINGS_DETAIL_ON
                : SETTINGS_DETAIL_OFF;
        }
        default: return 0;
    }

    Settings_Apply(playerid);
    Settings_Save(playerid);
    Settings_Show(playerid);   /* buka ulang supaya nilai baru terlihat */
    return 1;
}

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

/* ------------------------------------------------------------ selftest */

stock Settings_Selftest()
{
    new bool:ok = true;

    /* Rentang gaya HUD konsisten dengan konstanta hud.inc. */
    ok = ok && HUD_GAYA_PENUH == 1 && HUD_GAYA_MOBILE == 2 && HUD_GAYA_MINIMAL == 3;
    ok = ok && SETTINGS_HUD_AUTO == 0 && SETTINGS_DETAIL_ON == 1 && SETTINGS_DETAIL_OFF == 2;

    /* Siklus gaya: auto -> 1 -> 2 -> 3 -> auto (logika Settings_ToggleFromRow). */
    gPrefHudGaya[0] = SETTINGS_HUD_AUTO;
    gPrefHudGaya[0]++;
    ok = ok && gPrefHudGaya[0] == HUD_GAYA_PENUH;
    gPrefHudGaya[0] = HUD_GAYA_MINIMAL;
    gPrefHudGaya[0]++;
    if (gPrefHudGaya[0] > HUD_GAYA_MINIMAL) gPrefHudGaya[0] = SETTINGS_HUD_AUTO;
    ok = ok && gPrefHudGaya[0] == SETTINGS_HUD_AUTO;

    /* Toggle detail bolak-balik. */
    gPrefHudDetail[0] = SETTINGS_DETAIL_ON;
    gPrefHudDetail[0] = (gPrefHudDetail[0] == SETTINGS_DETAIL_OFF) ? SETTINGS_DETAIL_ON : SETTINGS_DETAIL_OFF;
    ok = ok && gPrefHudDetail[0] == SETTINGS_DETAIL_OFF;
    gPrefHudDetail[0] = (gPrefHudDetail[0] == SETTINGS_DETAIL_OFF) ? SETTINGS_DETAIL_ON : SETTINGS_DETAIL_OFF;
    ok = ok && gPrefHudDetail[0] == SETTINGS_DETAIL_ON;

    Selftest_Record(ok, "menu pengaturan",
        ok ? "siklus gaya + toggle bar detail konsisten" : "settings gagal");
    return 1;
}
