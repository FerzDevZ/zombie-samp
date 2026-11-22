#if defined _ls_menu_included
    #endinput
#endif
#define _ls_menu_included

/* Menu pusat: satu perintah yang bisa ditemukan pemain baru, berisi semua sistem
   yang sudah jalan. Tanpa ini fitur seperti skill tree dan shelter tidak pernah
   ditemukan karena tersembunyi di balik perintah yang harus ditebak. */

#define DIALOG_MENU       2601
#define DIALOG_MENU_ZONES 2602
#define DIALOG_MENU_HELP  2603

forward Menu_OnboardStep(playerid, step);

new bool:MenuOnboarded[MAX_PLAYERS];

stock Menu_Compass(Float:deltaX, Float:deltaY, dest[], size)
{
    // GTA: utara = -Y, timur = +X.
    new Float:angle = atan2(deltaX, -deltaY);
    if (angle < 0.0) angle += 360.0;

    if (angle >= 337.5 || angle < 22.5) format(dest, size, "utara");
    else if (angle < 67.5) format(dest, size, "timur laut");
    else if (angle < 112.5) format(dest, size, "timur");
    else if (angle < 157.5) format(dest, size, "tenggara");
    else if (angle < 202.5) format(dest, size, "selatan");
    else if (angle < 247.5) format(dest, size, "barat daya");
    else if (angle < 292.5) format(dest, size, "barat");
    else format(dest, size, "barat laut");
    return 1;
}

stock Menu_ShowHelp(playerid)
{
    ShowPlayerDialog(playerid, DIALOG_MENU_HELP, DIALOG_STYLE_MSGBOX, "Bantuan LAST STAND",
        "Y = interaksi hotspot (stash, meja craft, pedagang, ekstraksi)\n\
        /menu = menu pusat (tas, skill, craft, stash, shelter, zona)\n\
        /take = ambil loot di dekat kaki (wajib di mobile)\n\
        /geledah = buka mayat Brute/Stalker atau makam pemain dalam 3 meter\n\
        /kontrak = tiga target harian, hadiah uang + item\n\
        /ekstrak = serahkan minimal 3 sampel zombie di titik ekstraksi\n\
        /kencing = kosongkan kandung kemih (jangan tunggu penuh)\n\
        /garasi = beli & keluarkan kendaraan (harus di garasi kamp)\n\
        /simpan = simpan kendaraan ke garasi (dalam 12 m)\n\
        /isi [liter|kan] = isi tangki atau jerrycan di pom ($5/liter)\n\
        /lockpick /mesin /kunci = bobol, mesin, kunci kendaraan\n\
        /perbaiki = Repair Kit + Scrap 3 untuk kendaraan rusak\n\
        /sedot = pindahkan bensin kendaraan terbengkalai ke jerrycan\n\
        /zone = sebaran zombie + arah\n\
        /shelter = masuk safehouse pribadi\n\
        /build = bangun base (wall, gate, crate, bench, bed, turret)\n\
        /base = info base, /upgrade = naik tier, /bongkar = bongkar\n\
        /fix = perbaiki struktur base (Scrap 2)\n\
        /skill = belanjakan poin dari level up\n\
        /lobby = pindah sub-lobby (hanya di kamp)\n\n\
        Alur main: keluar kamp -> bunuh zombie -> naik level -> /skill -> craft -> ekstraksi.",
        "Tutup", "");
    return 1;
}

stock Menu_ShowZones(playerid)
{
    new body[1200], line[128], zoneLabel[48], compass[16];
    new Float:px, Float:py, Float:pz;
    GetPlayerPos(playerid, px, py, pz);

    body[0] = EOS;

    for (new zone = 0; zone < MAX_ZONE_COUNT; zone++)
    {
        Zone_GetName(zone, zoneLabel, sizeof(zoneLabel));
        if (zoneLabel[0] == EOS) continue;

        new Float:deltaX = gZones[zone][zoneX] - px;
        new Float:deltaY = gZones[zone][zoneY] - py;
        new Float:distance = floatsqroot(deltaX * deltaX + deltaY * deltaY);
        Menu_Compass(deltaX, deltaY, compass, sizeof(compass));

        format(line, sizeof(line), "%s%s | %d zombie | %d m %s\n",
            Zone_IsHub(zone) ? "*" : "", zoneLabel, ZoneZombieCount[Lobby_Of(playerid)][zone],
            floatround(distance), compass);
        strcat(body, line, sizeof(body));
    }

    strcat(body, "\n* = zona aman (kamp). Zombie respawn di zona lain, bukan di kamp.", sizeof(body));
    ShowPlayerDialog(playerid, DIALOG_MENU_ZONES, DIALOG_STYLE_MSGBOX,
        "Sebaran zombie per zona", body, "Tutup", "");
    return 1;
}

stock Menu_Show(playerid)
{
    if (!Account_IsPlaying(playerid)) return 0;

    new caption[96], body[384];
    format(caption, sizeof(caption), "LAST STAND - level %d, poin skill %d",
        PlayerSurvival[playerid][psLevel], SkillPoints[playerid]);

    new lobbyLabelName[32];
    Lobby_GetName(Lobby_Of(playerid), lobbyLabelName, sizeof(lobbyLabelName));

    format(body, sizeof(body),
        "Tas & pakai item\n\
        Skill tree (%d poin)\n\
        Meja crafting\n\
        Stash kamp (%d/%d slot)\n\
        Pedagang\n\
        Base: bangun / info\n\
        Shelter: %s\n\
        Ambil loot di dekat\n\
        Interaksi terdekat\n\
        Zona & sebaran zombie\n\
        Bantuan\n\
        Ganti lobby (%s)\n\
        Kontrak harian (%d siap)",
        SkillPoints[playerid],
        Stash_CountUsed(playerid),
        BaseBuild_StashCapacity(playerid),
        PlayerSurvival[playerid][psInShelter] ? "keluar" : "masuk",
        lobbyLabelName,
        Contracts_ReadyCount(playerid));

    ShowPlayerDialog(playerid, DIALOG_MENU, DIALOG_STYLE_LIST, caption, body, "Pilih", "Tutup");
    return 1;
}

public Menu_OnboardStep(playerid, step)
{
    if (!IsPlayerConnected(playerid)) return 0;
    if (!Account_IsPlaying(playerid)) return 0;

    switch (step)
    {
        case 0:
        {
            SendClientMessage(playerid, UI_ACCENT_INFO, "[MULAI] Tekan Y di hotspot, atau /menu untuk semua fitur.");
        }
        case 1:
        {
            SendClientMessage(playerid, UI_ACCENT_INFO, "[MULAI] Zombie ada di zona luar, bukan di kamp. Cek /zone atau /menu > Zona.");
            Toast_Show(playerid, "MULAI", "Keluar kamp, bunuh zombie, naik level.", UI_ACCENT_PRIMARY);
        }
        case 2:
        {
            SendClientMessage(playerid, UI_ACCENT_INFO, "[MULAI] Tiap level dapat 1 poin skill. Belanjakan di /skill (Marksman, Toughness, Scavenger, Medic, Stealth).");
            Toast_Show(playerid, "SKILL", "Level naik? Buka /skill sekarang.", UI_ACCENT_PRIMARY);
        }
    }
    return 1;
}

stock Menu_Onboarding(playerid)
{
    if (MenuOnboarded[playerid]) return 0;
    MenuOnboarded[playerid] = true;

    Menu_OnboardStep(playerid, 0);
    SetTimerEx("Menu_OnboardStep", 12000, false, "ii", playerid, 1);
    SetTimerEx("Menu_OnboardStep", 26000, false, "ii", playerid, 2);
    return 1;
}

stock Menu_OnDialogResponse(playerid, dialogid, response, listitem, const inputtext[])
{
    #pragma unused inputtext

    if (dialogid == DIALOG_MENU_ZONES || dialogid == DIALOG_MENU_HELP) return 1;
    if (dialogid != DIALOG_MENU) return 0;
    if (!response) return 1;

    switch (listitem)
    {
        case 0: InventoryUI_Show(playerid);
        case 1: Skills_Show(playerid);
        case 2: CraftUI_Show(playerid);
        case 3: Stash_Open(playerid);
        case 4: Vendor_ShowMenu(playerid);
        /* Urutan case WAJIB sama dengan urutan baris dialog di Menu_Show:
           0 tas, 1 skill, 2 craft, 3 stash, 4 pedagang, 5 base, 6 shelter,
           7 loot, 8 interaksi, 9 zona, 10 bantuan, 11 lobby, 12 kontrak. */
        case 5: BaseBuild_ShowMenu(playerid);
        case 6: Shelter_Toggle(playerid);
        case 7:
        {
            if (!Loot_TakeNearest(playerid, 4.0))
            {
                SendClientMessage(playerid, UI_TEXT_MUTED, "[LOOT] Tidak ada loot dalam 4 meter.");
            }
        }
        case 8: Interact_HandlePlayer(playerid);
        case 9: Menu_ShowZones(playerid);
        case 10: Menu_ShowHelp(playerid);
        case 11: Lobby_ShowSelect(playerid);
        case 12: Contracts_Show(playerid);
    }
    return 1;
}
