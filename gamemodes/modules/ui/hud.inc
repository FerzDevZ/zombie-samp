#if defined _ls_hud_included
    #endinput
#endif
#define _ls_hud_included

new PlayerText:HudBrand[MAX_PLAYERS];
new PlayerText:HudVitals[MAX_PLAYERS];
new PlayerText:HudNeeds[MAX_PLAYERS];
new PlayerText:HudInfection[MAX_PLAYERS];
new PlayerText:HudZone[MAX_PLAYERS];
new PlayerText:HudLoadout[MAX_PLAYERS];
new PlayerText:HudProgress[MAX_PLAYERS];
new PlayerText:HudVehicle[MAX_PLAYERS];
new PlayerText:HudBoss[MAX_PLAYERS];
new PlayerText:HudRaid[MAX_PLAYERS];
new bool:PlayerHudVisible[MAX_PLAYERS];

/* Dirty-flag render: TextDrawSetString/Show hanya dipanggil kalau nilai
   berubah. Refresh penuh 500 ms x 200 pemain = ribuan paket TD sia-sia. */
new HudLastVitals[MAX_PLAYERS][48];
new HudLastNeeds[MAX_PLAYERS][48];
new HudLastInfect[MAX_PLAYERS][24];
new HudLastZone[MAX_PLAYERS][48];
new HudLastLoadout[MAX_PLAYERS][48];
new HudLastProgress[MAX_PLAYERS][64];
new HudLastVehicle[MAX_PLAYERS][64];
new HudLastBoss[MAX_PLAYERS][64];
new HudLastRaid[MAX_PLAYERS][64];
new HudLastNeedColor[MAX_PLAYERS];
new HudLastInfectColor[MAX_PLAYERS];
new bool:HudVehicleShown[MAX_PLAYERS];
new HudFuelColor[MAX_PLAYERS];
new HudFuelWarnStage[MAX_PLAYERS];
new bool:HudBossShown[MAX_PLAYERS];
new bool:HudRaidShown[MAX_PLAYERS];

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

enum E_HUD_FIT
{
    hudFitName[16],
    Float:hudFitX,
    Float:hudFitSizeX,
    hudFitWorstLen
};

new const gHudFitRows[][E_HUD_FIT] =
{
    { "HudBrand",      24.0, 0.22, 10 },
    { "HudVitals",     24.0, 0.20, 15 },
    { "HudNeeds",      24.0, 0.19, 28 },
    { "HudInfection",  24.0, 0.20, 11 },
    { "HudZone",       24.0, 0.18, 22 },
    { "HudProgress",   24.0, 0.20, 41 },
    { "HudLoadout",   496.0, 0.20, 19 },
    { "HudVehicle",   400.0, 0.17, 38 },
    { "HudBoss",      210.0, 0.26, 28 },
    { "HudRaid",      190.0, 0.24, 25 }
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

stock HUD_Create(playerid)
{
    HudBrand[playerid] = CreatePlayerTextDraw(playerid, 24.0, 300.0, "LAST_STAND");
    PlayerTextDrawLetterSize(playerid, HudBrand[playerid], 0.22, 0.9);
    PlayerTextDrawColor(playerid, HudBrand[playerid], UI_TEXT_PRIMARY);
    PlayerTextDrawSetOutline(playerid, HudBrand[playerid], 1);
    PlayerTextDrawFont(playerid, HudBrand[playerid], 2);

    HudVitals[playerid] = CreatePlayerTextDraw(playerid, 24.0, 314.0, "HP 100");
    PlayerTextDrawLetterSize(playerid, HudVitals[playerid], 0.2, 0.8);
    PlayerTextDrawColor(playerid, HudVitals[playerid], UI_ACCENT_PRIMARY);
    PlayerTextDrawSetOutline(playerid, HudVitals[playerid], 1);
    PlayerTextDrawFont(playerid, HudVitals[playerid], 1);

    HudNeeds[playerid] = CreatePlayerTextDraw(playerid, 24.0, 327.0, "LAP 100  HAUS 100  PIPIS 0");
    PlayerTextDrawLetterSize(playerid, HudNeeds[playerid], 0.19, 0.75);
    PlayerTextDrawColor(playerid, HudNeeds[playerid], UI_TEXT_SECONDARY);
    PlayerTextDrawSetOutline(playerid, HudNeeds[playerid], 1);
    PlayerTextDrawFont(playerid, HudNeeds[playerid], 1);

    HudInfection[playerid] = CreatePlayerTextDraw(playerid, 24.0, 340.0, "INFECT 0%");
    PlayerTextDrawLetterSize(playerid, HudInfection[playerid], 0.2, 0.8);
    PlayerTextDrawColor(playerid, HudInfection[playerid], UI_ACCENT_PRIMARY);
    PlayerTextDrawSetOutline(playerid, HudInfection[playerid], 1);
    PlayerTextDrawFont(playerid, HudInfection[playerid], 1);

    HudZone[playerid] = CreatePlayerTextDraw(playerid, 24.0, 353.0, "ZONE");
    PlayerTextDrawLetterSize(playerid, HudZone[playerid], 0.18, 0.75);
    PlayerTextDrawColor(playerid, HudZone[playerid], UI_TEXT_MUTED);
    PlayerTextDrawSetOutline(playerid, HudZone[playerid], 1);
    PlayerTextDrawFont(playerid, HudZone[playerid], 1);

    HudProgress[playerid] = CreatePlayerTextDraw(playerid, 24.0, 366.0, "LVL 1  SP 0  /skill");
    PlayerTextDrawLetterSize(playerid, HudProgress[playerid], 0.2, 0.8);
    PlayerTextDrawColor(playerid, HudProgress[playerid], UI_TEXT_SECONDARY);
    PlayerTextDrawSetOutline(playerid, HudProgress[playerid], 1);
    PlayerTextDrawFont(playerid, HudProgress[playerid], 1);

    /* x=496 (bukan 560): "$999999  SCRAP 999" = 17 karakter ~120 px, di 560
       melewati tepi 640 dan terpotong (terlihat di tools/hud_preview). Sama
       kelas dengan bug gauge BBM yang dulu dipindah 560 -> 440. */
    HudLoadout[playerid] = CreatePlayerTextDraw(playerid, 496.0, 330.0, "$0  SCRAP 0");
    PlayerTextDrawLetterSize(playerid, HudLoadout[playerid], 0.2, 0.8);
    PlayerTextDrawColor(playerid, HudLoadout[playerid], UI_TEXT_PRIMARY);
    PlayerTextDrawSetOutline(playerid, HudLoadout[playerid], 1);
    PlayerTextDrawFont(playerid, HudLoadout[playerid], 1);

    /* x=400 (bukan 440): cabang BBM rendah menyertakan kompas pom sehingga
       barisnya bisa 38 karakter ("BBM [##########] 100/100L pom 8000m TG").
       Di 440 itu meluber 28 px keluar layar. 400 + 228 px = 628, sisa 12 px. */
    HudVehicle[playerid] = CreatePlayerTextDraw(playerid, 400.0, 346.0, "BBM");
    PlayerTextDrawLetterSize(playerid, HudVehicle[playerid], 0.17, 0.72);
    PlayerTextDrawColor(playerid, HudVehicle[playerid], UI_ACCENT_INFO);
    PlayerTextDrawSetOutline(playerid, HudVehicle[playerid], 1);
    PlayerTextDrawFont(playerid, HudVehicle[playerid], 1);

    /* Bar HP boss: teks blok, gaya sama dengan HUD lain (tanpa box tambahan). */
    HudBoss[playerid] = CreatePlayerTextDraw(playerid, 210.0, 30.0, "ABOMINATION");
    PlayerTextDrawLetterSize(playerid, HudBoss[playerid], 0.26, 1.0);
    PlayerTextDrawColor(playerid, HudBoss[playerid], UI_ACCENT_DANGER);
    PlayerTextDrawSetOutline(playerid, HudBoss[playerid], 1);
    PlayerTextDrawFont(playerid, HudBoss[playerid], 2);

    /* Banner raid di atas bar boss (boss y=30). Keduanya bisa aktif bersamaan,
       jadi raid memakai y=16. Lebar ±30 karakter cukup untuk
       "RAID KAMP 12 ZOMBIE  2:59". */
    HudRaid[playerid] = CreatePlayerTextDraw(playerid, 190.0, 16.0, "RAID KAMP");
    PlayerTextDrawLetterSize(playerid, HudRaid[playerid], 0.24, 0.95);
    PlayerTextDrawColor(playerid, HudRaid[playerid], UI_ACCENT_DANGER);
    PlayerTextDrawSetOutline(playerid, HudRaid[playerid], 1);
    PlayerTextDrawFont(playerid, HudRaid[playerid], 2);

    PlayerTextDrawShow(playerid, HudBrand[playerid]);
    PlayerTextDrawShow(playerid, HudVitals[playerid]);
    PlayerTextDrawShow(playerid, HudNeeds[playerid]);
    PlayerTextDrawShow(playerid, HudInfection[playerid]);
    PlayerTextDrawShow(playerid, HudZone[playerid]);
    PlayerTextDrawShow(playerid, HudLoadout[playerid]);
    PlayerTextDrawShow(playerid, HudProgress[playerid]);
    PlayerHudVisible[playerid] = true;

    HudLastVitals[playerid][0] = EOS;
    HudLastNeeds[playerid][0] = EOS;
    HudLastInfect[playerid][0] = EOS;
    HudLastZone[playerid][0] = EOS;
    HudLastLoadout[playerid][0] = EOS;
    HudLastProgress[playerid][0] = EOS;
    HudLastVehicle[playerid][0] = EOS;
    HudFuelColor[playerid] = 0;
    HudFuelWarnStage[playerid] = 0;
    HudLastBoss[playerid][0] = EOS;
    HudLastRaid[playerid][0] = EOS;
    HudLastNeedColor[playerid] = 0;
    HudLastInfectColor[playerid] = 0;
    HudVehicleShown[playerid] = false;
    HudBossShown[playerid] = false;
    HudRaidShown[playerid] = false;
    return 1;
}

/* Client (terutama SA-MP Mobile) membuang PlayerTextDraw setelah respawn atau
   setelah pemain ganti karakter, sehingga HUD hilang permanen walau server
   mengira masih terpasang. Setiap spawn kita tampilkan ulang + reset cache teks
   supaya tick berikutnya langsung menulis nilai baru.

   force=false menghormati /hud: pemain yang menyembunyikan HUD tidak boleh
   dipaksa melihatnya lagi hanya karena mati. Tanpa guard ini, mati sekali
   membatalkan pilihan pemain secara diam-diam. */
stock HUD_Reshow(playerid, bool:force = false)
{
    if (!IsPlayerConnected(playerid)) return 0;
    if (!force && !PlayerHudVisible[playerid]) return 0;

    PlayerTextDrawShow(playerid, HudBrand[playerid]);
    PlayerTextDrawShow(playerid, HudVitals[playerid]);
    PlayerTextDrawShow(playerid, HudNeeds[playerid]);
    PlayerTextDrawShow(playerid, HudInfection[playerid]);
    PlayerTextDrawShow(playerid, HudZone[playerid]);
    PlayerTextDrawShow(playerid, HudLoadout[playerid]);
    PlayerTextDrawShow(playerid, HudProgress[playerid]);

    PlayerHudVisible[playerid] = true;
    HudLastVitals[playerid][0] = EOS;
    HudLastNeeds[playerid][0] = EOS;
    HudLastInfect[playerid][0] = EOS;
    HudLastZone[playerid][0] = EOS;
    HudLastLoadout[playerid][0] = EOS;
    HudLastProgress[playerid][0] = EOS;
    /* Banner yang hanya dirender saat aktif juga harus dipaksa tampil ulang:
       cache teks kosong + flag false membuat tick berikutnya menulis dan
       menampilkannya lagi kalau raid/boss memang masih berjalan. */
    HudLastBoss[playerid][0] = EOS;
    HudLastRaid[playerid][0] = EOS;
    HudBossShown[playerid] = false;
    HudRaidShown[playerid] = false;
    return 1;
}

stock HUD_Hide(playerid)
{
    if (!IsPlayerConnected(playerid)) return 0;

    PlayerTextDrawHide(playerid, HudBrand[playerid]);
    PlayerTextDrawHide(playerid, HudVitals[playerid]);
    PlayerTextDrawHide(playerid, HudNeeds[playerid]);
    PlayerTextDrawHide(playerid, HudInfection[playerid]);
    PlayerTextDrawHide(playerid, HudZone[playerid]);
    PlayerTextDrawHide(playerid, HudLoadout[playerid]);
    PlayerTextDrawHide(playerid, HudProgress[playerid]);
    PlayerTextDrawHide(playerid, HudVehicle[playerid]);
    PlayerTextDrawHide(playerid, HudBoss[playerid]);
    PlayerTextDrawHide(playerid, HudRaid[playerid]);

    PlayerHudVisible[playerid] = false;
    HudVehicleShown[playerid] = false;
    HudBossShown[playerid] = false;
    HudRaidShown[playerid] = false;
    return 1;
}

stock HUD_Destroy(playerid)
{
    PlayerTextDrawDestroy(playerid, HudBrand[playerid]);
    PlayerTextDrawDestroy(playerid, HudVitals[playerid]);
    PlayerTextDrawDestroy(playerid, HudNeeds[playerid]);
    PlayerTextDrawDestroy(playerid, HudVehicle[playerid]);

    PlayerTextDrawDestroy(playerid, HudBoss[playerid]);
    PlayerTextDrawDestroy(playerid, HudRaid[playerid]);
    PlayerTextDrawDestroy(playerid, HudInfection[playerid]);
    PlayerTextDrawDestroy(playerid, HudZone[playerid]);
    PlayerTextDrawDestroy(playerid, HudLoadout[playerid]);
    PlayerTextDrawDestroy(playerid, HudProgress[playerid]);
    PlayerHudVisible[playerid] = false;
    return 1;
}

stock HUD_Update(playerid)
{
    if (!IsPlayerConnected(playerid)) return 0;
    if (!PlayerHudVisible[playerid]) return 0;

    new hudLine[96];
    format(hudLine, sizeof(hudLine), "HP %.0f  ARM %.0f",
        Player_GetHealth(playerid), PlayerSurvival[playerid][psArmour]);
    HUD_SetIfChanged(playerid, HudVitals[playerid], hudLine, HudLastVitals[playerid], sizeof(HudLastVitals[]));

    new needColor = UI_TEXT_SECONDARY;
    if (PlayerSurvival[playerid][psHunger] <= 20.0
        || PlayerSurvival[playerid][psThirst] <= 20.0
        || PlayerSurvival[playerid][psBladder] >= 90.0)
    {
        needColor = UI_ACCENT_DANGER;
    }
    else if (PlayerSurvival[playerid][psHunger] <= 40.0
        || PlayerSurvival[playerid][psThirst] <= 40.0
        || PlayerSurvival[playerid][psBladder] >= 75.0)
    {
        needColor = UI_ACCENT_WARN;
    }

    format(hudLine, sizeof(hudLine), "LAP %.0f  HAUS %.0f  PIPIS %.0f",
        PlayerSurvival[playerid][psHunger], PlayerSurvival[playerid][psThirst],
        PlayerSurvival[playerid][psBladder]);
    HUD_SetIfChanged(playerid, HudNeeds[playerid], hudLine, HudLastNeeds[playerid], sizeof(HudLastNeeds[]));
    if (needColor != HudLastNeedColor[playerid])
    {
        HudLastNeedColor[playerid] = needColor;
        PlayerTextDrawColor(playerid, HudNeeds[playerid], needColor);
        PlayerTextDrawShow(playerid, HudNeeds[playerid]);
    }

    if (GetPlayerState(playerid) == PLAYER_STATE_DRIVER)
    {
        new vehicleid = GetPlayerVehicleID(playerid);
        new Float:fuel = Vehicle_GetFuel(vehicleid);
        new Float:tank = Vehicle_GetTank(vehicleid);
        new Float:percent = Vehicle_FuelPercent(vehicleid);

        /* Gauge blok 10 karakter: terbaca di layar HP tanpa perlu font khusus. */
        new bar[12];
        new filled = floatround(percent * 10.0);
        if (filled < 0) filled = 0;
        if (filled > 10) filled = 10;

        new index = 0;
        bar[index++] = '[';
        for (new cell = 0; cell < 10; cell++)
        {
            bar[index++] = (cell < filled) ? '#' : '-';
        }
        bar[index++] = ']';
        bar[index] = EOS;

        /* Baris sempit (±33 karakter). Saat BBM rendah, angka km diganti arah
           pom: informasi yang benar-benar dibutuhkan saat itu. */
        new fuelLine[96];
        if (fuel < 0.0)
        {
            format(fuelLine, sizeof(fuelLine), "TANPA BBM [----------]");
        }
        else if (percent <= 0.25)
        {
            new pumpPart[40];
            Garage_FuelCompass(playerid, pumpPart, sizeof(pumpPart), true);
            format(fuelLine, sizeof(fuelLine), "BBM %s %.0f/%.0fL %s", bar, fuel, tank, pumpPart);
        }
        else
        {
            format(fuelLine, sizeof(fuelLine), "BBM %s %.0f/%.0fL %.0fkm",
                bar, fuel, tank, Vehicle_RangeKm(vehicleid));
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
    else if (HudVehicleShown[playerid])
    {
        HudVehicleShown[playerid] = false;
        HudFuelWarnStage[playerid] = 0;
        PlayerTextDrawHide(playerid, HudVehicle[playerid]);
    }

    new Float:infection = Player_GetInfection(playerid);
    new infectionColor = UI_ACCENT_PRIMARY;
    if (infection >= 80.0) infectionColor = UI_ACCENT_DANGER;
    else if (infection >= 40.0) infectionColor = UI_ACCENT_WARN;

    format(hudLine, sizeof(hudLine), "INFECT %.0f%%", infection);
    HUD_SetIfChanged(playerid, HudInfection[playerid], hudLine, HudLastInfect[playerid], sizeof(HudLastInfect[]));
    if (infectionColor != HudLastInfectColor[playerid])
    {
        HudLastInfectColor[playerid] = infectionColor;
        PlayerTextDrawColor(playerid, HudInfection[playerid], infectionColor);
        PlayerTextDrawShow(playerid, HudInfection[playerid]);
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

    format(hudLine, sizeof(hudLine), "$%d  SCRAP %d",
        PlayerSurvival[playerid][psCash], Inventory_CountItem(playerid, ITEM_SCRAP));
    HUD_SetIfChanged(playerid, HudLoadout[playerid], hudLine, HudLastLoadout[playerid], sizeof(HudLastLoadout[]));

    // Progres skill harus terlihat tanpa membuka dialog: ini satu-satunya petunjuk
    // bahwa level dan poin skill bertambah.
    format(hudLine, sizeof(hudLine), "LVL %d  SP %d  SAMPEL %d  LUKA %d  /skill",
        PlayerSurvival[playerid][psLevel], SkillPoints[playerid],
        Inventory_CountItem(playerid, ITEM_SPECIMEN), Wounds_Get(playerid));
    HUD_SetIfChanged(playerid, HudProgress[playerid], hudLine, HudLastProgress[playerid], sizeof(HudLastProgress[]));

    new Float:bossHealth, Float:bossMax;
    if (Director_BossStatus(Lobby_Of(playerid), bossHealth, bossMax) && bossMax > 0.0)
    {
        new filled = floatround((bossHealth / bossMax) * 10.0);
        if (filled < 0) filled = 0;
        if (filled > 10) filled = 10;

        new bar[16];
        bar[0] = EOS;
        for (new index = 0; index < 10; index++)
        {
            strcat(bar, (index < filled) ? "#" : "-", sizeof(bar));
        }

        new bossLine[64];
        format(bossLine, sizeof(bossLine), "ABOMINATION %d%%  %s",
            floatround((bossHealth / bossMax) * 100.0), bar);
        HUD_SetIfChanged(playerid, HudBoss[playerid], bossLine, HudLastBoss[playerid], sizeof(HudLastBoss[]));
        if (!HudBossShown[playerid])
        {
            HudBossShown[playerid] = true;
            PlayerTextDrawShow(playerid, HudBoss[playerid]);
        }
    }
    else if (HudBossShown[playerid])
    {
        HudBossShown[playerid] = false;
        PlayerTextDrawHide(playerid, HudBoss[playerid]);
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
