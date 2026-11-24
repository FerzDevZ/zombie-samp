/* Panel superadmin penuh: semua aksi lewat dialog, tanpa mengetik command.

   Struktur: menu utama -> kategori -> aksi. Setiap aksi langsung jalan lalu panel
   induknya dibuka lagi, jadi admin bisa berurutan tanpa keluar dialog.

   Aturan yang dipegang (lihat MEMORY.md):
   - Baris dialog adalah SNAPSHOT id (player/account/item/zombie), bukan hitungan ulang
     saat respons; ini mencegah aksi salah sasaran kalau daftar berubah di belakang.
   - Angka tidak diketik: jumlah uang/XP/item selalu dari daftar preset.
   - Setiap aksi tetap memanggil fungsi ber-guard yang sama dengan versi command
     (`Admin_GiveItem`, `Admin_Teleport`, `Moderation_Ban`, dst.), jadi level akses,
     ledger, dan `AC_ClearSample` tidak bisa dilewati lewat panel. */

#if defined _ls_panel_included
    #endinput
#endif
#define _ls_panel_included

#define DIALOG_SA_MAIN          3501
#define DIALOG_SA_PLAYERS       3502
#define DIALOG_SA_PLAYER_ACT    3503
#define DIALOG_SA_ZOMBIES       3504
#define DIALOG_SA_ZOMBIE_COUNT  3505
#define DIALOG_SA_CATEGORIES    3506
#define DIALOG_SA_ITEMS         3507
#define DIALOG_SA_ITEM_AMOUNT   3508
#define DIALOG_SA_WORLD         3509
#define DIALOG_SA_HOURS         3510
#define DIALOG_SA_MODERATION    3511
#define DIALOG_SA_BANS          3512
#define DIALOG_SA_SERVER        3513
#define DIALOG_SA_TELEPORT      3514

#define SA_PLAYER_ROWS          32
#define SA_ITEM_ROWS            12
#define SA_BAN_ROWS             6

new SaTarget[MAX_PLAYERS];
new SaPlayerIds[MAX_PLAYERS][SA_PLAYER_ROWS];
new SaPlayerRows[MAX_PLAYERS];
/* Paging daftar pemain: maxplayers 200, satu halaman 30 baris + navigasi. */
#define SA_PLAYER_PAGE_SIZE 30
new SaPlayerPage[MAX_PLAYERS];
new SaZombieType[MAX_PLAYERS];
new SaCategory[MAX_PLAYERS];
new SaItemPage[MAX_PLAYERS];
new SaItemIds[MAX_PLAYERS][SA_ITEM_ROWS];
new SaItemRows[MAX_PLAYERS];
new SaBanAccount[MAX_PLAYERS][SA_BAN_ROWS];
new SaBanRows[MAX_PLAYERS];
new SaSelectedItem[MAX_PLAYERS];

forward Admin_PanelOnBansLoaded(adminId);

stock Admin_PanelReset(playerid)
{
    SaTarget[playerid] = INVALID_PLAYER_ID;
    SaPlayerRows[playerid] = 0;
    SaPlayerPage[playerid] = 0;
    SaZombieType[playerid] = _:ZT_WALKER;
    SaCategory[playerid] = _:ITEM_NONE;
    SaItemPage[playerid] = 0;
    SaItemRows[playerid] = 0;
    SaBanRows[playerid] = 0;
    SaSelectedItem[playerid] = ITEM_NONE_ID;
    return 1;
}

stock Admin_PanelMain(playerid)
{
    if (!IsServerAdmin(playerid))
    {
        SendClientMessage(playerid, UI_ACCENT_DANGER, "[SA] Panel ini khusus admin.");
        return 0;
    }

    SendClientMessage(playerid, UI_TEXT_MUTED, "[SA] Pilih baris lalu tombol Buka. Setiap aksi langsung jalan dan panel ini terbuka lagi.");
    SendClientMessage(playerid, UI_TEXT_MUTED, "[SA] Kolom kanan menjelaskan hasilnya. Tombol Tutup/Kembali tidak mengubah apa pun.");

    new body[768];
    format(body, sizeof(body),
        "Menu\tIsi\n\
         Pemain & akun\tKebal, uang, XP, item, ban (pilih orangnya dulu)\n\
         Zombie\tSpawn per tipe, jumlah, bersihkan, boss\n\
         Item\tBeri item per kategori (tanpa mengetik id)\n\
         Diri sendiri\tKebal, pulihkan, teleport hub\n\
         Dunia & event\tJam, supply drop, blood moon, horde\n\
         Teleport\tTujuan populer tanpa mengetik koordinat\n\
         Moderasi\tDaftar ban aktif, buka ban\n\
         Server\tRingkasan beban + selftest\n");

    ShowPlayerDialog(playerid, DIALOG_SA_MAIN, DIALOG_STYLE_TABLIST_HEADERS,
        "Superadmin Panel", body, "Buka", "Tutup");
    return 1;
}

stock Admin_PanelPlayers(playerid)
{
    if (!IsServerAdmin(playerid)) return 0;

    new body[1600], line[96];
    body[0] = EOS;
    SaPlayerRows[playerid] = 0;

    new totalOnline = 0;
    new windowStart = SaPlayerPage[playerid] * SA_PLAYER_PAGE_SIZE;
    foreach (new target : PlayerPool)
    {
        if (!Account_IsPlaying(target)) continue;
        new rowIndex = totalOnline;
        totalOnline++;

        if (rowIndex < windowStart) continue;
        if (SaPlayerRows[playerid] >= SA_PLAYER_PAGE_SIZE) continue;

        new targetName[MAX_PLAYER_NAME + 1];
        GetPlayerName(target, targetName, sizeof(targetName));

        new levelLabel[16];
        Admin_LevelName(AdminLevel[target], levelLabel, sizeof(levelLabel));

        format(line, sizeof(line), "ID %d | %s | HP %.0f | %s\n",
            target, targetName, Player_GetHealth(target), levelLabel);
        strcat(body, line, sizeof(body));

        SaPlayerIds[playerid][SaPlayerRows[playerid]] = target;
        SaPlayerRows[playerid]++;
    }

    if (SaPlayerRows[playerid] == 0)
    {
        if (SaPlayerPage[playerid] > 0)
        {
            SaPlayerPage[playerid] = 0;
            return Admin_PanelPlayers(playerid);
        }
        SendClientMessage(playerid, UI_TEXT_MUTED, "[SA] Tidak ada pemain yang login.");
        return 0;
    }

    /* Baris navigasi selalu di AKHIR supaya indeks snapshot tidak bergeser. */
    if (totalOnline > windowStart + SA_PLAYER_PAGE_SIZE)
        strcat(body, ">> Halaman berikutnya\n", sizeof(body));

    new title[64];
    format(title, sizeof(title), "Pilih Pemain (%d online, hal %d)", totalOnline, SaPlayerPage[playerid] + 1);
    ShowPlayerDialog(playerid, DIALOG_SA_PLAYERS, DIALOG_STYLE_LIST, title, body, "Pilih", "Kembali");
    return 1;
}

stock Admin_PanelPlayerActions(playerid)
{
    new target = SaTarget[playerid];
    if (!IsPlayerConnected(target))
    {
        SendClientMessage(playerid, UI_TEXT_MUTED, "[SA] Pemain itu sudah keluar.");
        return Admin_PanelPlayers(playerid);
    }

    new targetName[MAX_PLAYER_NAME + 1];
    GetPlayerName(target, targetName, sizeof(targetName));

    new body[1024];
    format(body, sizeof(body),
        "Aksi\tHasil saat dipencet\n\
         Kebal\t%s\n\
         Pulihkan penuh\tHP, armour, kebutuhan, luka\n\
         Tambah $1.000\tUang saat ini $%d\n\
         Tambah $10.000\t\n\
         Tambah $100.000\t\n\
         Uang jadi $0\t\n\
         Tambah 100 XP\t\n\
         Tambah 1.000 XP\t\n\
         Poin skill = 5\tSkill saat ini %d\n\
         Bekukan / lepas\t\n\
         Teleport ke dia\t\n\
         Tarik dia ke sini\t\n\
         Beri item\tPilih kategori lalu item\n\
         Tendang\t\n\
         Ban 30 menit\t\n\
         Ban permanen\t\n",
        AdminGod[target] ? "AKTIF" : "MATI",
        PlayerSurvival[target][psCash], SkillPoints[target]);

    new title[64];
    format(title, sizeof(title), "Aksi: %s (ID %d)", targetName, target);
    ShowPlayerDialog(playerid, DIALOG_SA_PLAYER_ACT, DIALOG_STYLE_TABLIST_HEADERS, title, body, "Jalankan", "Kembali");
    return 1;
}

stock Admin_PanelZombies(playerid)
{
    new body[768];
    format(body, sizeof(body),
        "Tipe\tSifat\n\
         Walker\tHP 60 | lambat\n\
         Runner\tHP 45 | cepat\n\
         Crawler\tHP 30 | merangkak\n\
         Brute\tHP 350 | tank\n\
         Bloater\tHP 120 | meledak\n\
         Screamer\tHP 70 | panggil horde\n\
         Stalker\tHP 90 | malam\n\
         Abomination (boss)\tHP 2400 | minTier 99\n\
         Bersihkan zombie\tHapus semua zombie di lobimu\n\
         Spawn horde\tHorde otomatis di sekitarmu\n");

    ShowPlayerDialog(playerid, DIALOG_SA_ZOMBIES, DIALOG_STYLE_TABLIST_HEADERS,
        "Zombie", body, "Pilih", "Kembali");
    return 1;
}

stock Admin_PanelZombieCount(playerid)
{
    new typeLabel[24];
    format(typeLabel, sizeof(typeLabel), "%s", gZombieProfiles[_:SaZombieType[playerid]][ztName]);

    new body[384];
    format(body, sizeof(body),
        "Jumlah\tEkor sekali spawn\n\
         1 ekor\tSatu target\n\
         3 ekor\tPatroli kecil\n\
         5 ekor\tTekanan sedang\n\
         10 ekor\tGelombang\n\
         25 ekor\tHorde besar\n\
         50 ekor\tUjian server\n");

    new title[64];
    format(title, sizeof(title), "Spawn %s | Aktif %d/%d", typeLabel, Zombie_GetActiveCount(), MAX_ZOMBIES);
    ShowPlayerDialog(playerid, DIALOG_SA_ZOMBIE_COUNT, DIALOG_STYLE_TABLIST_HEADERS, title, body, "Spawn", "Kembali");
    return 1;
}

stock Admin_PanelCategories(playerid)
{
    new body[512];
    format(body, sizeof(body),
        "Kategori\tIsi\n\
         Semua item\tDaftar penuh berhalaman\n\
         Consumable\tMakanan, medkit, obat\n\
         Material\tScrap, kain, elektronik, bahan\n\
         Tool\tPerkakas dan kit\n\
         Weapon\tSenjata\n\
         Blueprint\tResep dan cetak biru\n");

    new title[80];
    if (IsPlayerConnected(SaTarget[playerid]))
    {
        new targetName[MAX_PLAYER_NAME + 1];
        GetPlayerName(SaTarget[playerid], targetName, sizeof(targetName));
        format(title, sizeof(title), "Beri item ke %s", targetName);
    }
    else
    {
        format(title, sizeof(title), "Beri item ke diri sendiri");
    }

    ShowPlayerDialog(playerid, DIALOG_SA_CATEGORIES, DIALOG_STYLE_TABLIST_HEADERS, title, body, "Pilih", "Kembali");
    return 1;
}

stock Admin_PanelItems(playerid)
{
    new body[1400], line[96], itemLabel[24];
    body[0] = EOS;
    SaItemRows[playerid] = 0;

    new start = SaItemPage[playerid] * (SA_ITEM_ROWS - 1);
    new seen = 0;

    for (new itemid = 1; itemid < ITEM_COUNT; itemid++)
    {
        if (SaCategory[playerid] != _:ITEM_NONE && _:Item_GetCategory(itemid) != SaCategory[playerid]) continue;

        if (seen < start)
        {
            seen++;
            continue;
        }

        if (SaItemRows[playerid] >= SA_ITEM_ROWS - 1) break;

        Item_GetName(itemid, itemLabel, sizeof(itemLabel));
        format(line, sizeof(line), "%d | %s | %.1f kg\n", itemid, itemLabel, Item_GetWeight(itemid));
        strcat(body, line, sizeof(body));

        SaItemIds[playerid][SaItemRows[playerid]] = itemid;
        SaItemRows[playerid]++;
        seen++;
    }

    if (SaItemRows[playerid] == 0)
    {
        SendClientMessage(playerid, UI_TEXT_MUTED, "[SA] Tidak ada item di halaman ini.");
        return Admin_PanelCategories(playerid);
    }

    /* Baris navigasi selalu ada di akhir, jadi indeks item tidak bergeser. */
    format(body, sizeof(body), "%s>> Halaman berikutnya\n", body);

    new title[64];
    format(title, sizeof(title), "Item | halaman %d", SaItemPage[playerid] + 1);
    ShowPlayerDialog(playerid, DIALOG_SA_ITEMS, DIALOG_STYLE_LIST, title, body, "Pilih", "Kembali");
    return 1;
}

stock Admin_PanelItemAmount(playerid, itemid)
{
    new itemLabel[24];
    Item_GetName(itemid, itemLabel, sizeof(itemLabel));

    new body[384];
    format(body, sizeof(body), "Jumlah\tBerat total di tas\n1x\t%.1f kg\n5x\t%.1f kg\n10x\t%.1f kg\n25x\t%.1f kg\n50x\t%.1f kg\n",
        Item_GetWeight(itemid), Item_GetWeight(itemid) * 5.0,
        Item_GetWeight(itemid) * 10.0, Item_GetWeight(itemid) * 25.0,
        Item_GetWeight(itemid) * 50.0);

    new title[96];
    format(title, sizeof(title), "Jumlah %s", itemLabel);
    ShowPlayerDialog(playerid, DIALOG_SA_ITEM_AMOUNT, DIALOG_STYLE_TABLIST_HEADERS, title, body, "Beri", "Kembali");
    return 1;
}

stock Admin_PanelWorld(playerid)
{
    new body[768];
    format(body, sizeof(body),
        "Aksi\tHasil\n\
         Supply drop\tEvent 0 | peti bantuan\n\
         Blood moon\tEvent 1 | zombie lebih agresif\n\
         Quiet hour\tEvent 2 | jeda tekanan\n\
         Spawn horde\tHorde di sekitarmu sekarang\n\
         Jam 06:00\tPagi\n\
         Jam 12:00\tSiang\n\
         Jam 18:00\tSore\n\
         Jam 22:00\tMalam\n\
         Jam 00:00\tTengah malam\n");

    new title[64];
    format(title, sizeof(title), "Dunia & Event | jam %02d", World_GetHour());
    ShowPlayerDialog(playerid, DIALOG_SA_WORLD, DIALOG_STYLE_TABLIST_HEADERS, title, body, "Jalankan", "Kembali");
    return 1;
}

stock Admin_PanelModeration(playerid)
{
    new body[512];
    format(body, sizeof(body),
        "Aksi\tIsi\n\
         Daftar ban aktif\t6 terakhir, klik untuk membuka\n\
         Info akun pemain\tLevel admin, status ban\n");

    ShowPlayerDialog(playerid, DIALOG_SA_MODERATION, DIALOG_STYLE_TABLIST_HEADERS,
        "Moderasi", body, "Pilih", "Kembali");
    return 1;
}

stock Admin_PanelBans(playerid)
{
    new query[320];
    format(query, sizeof(query),
        "SELECT b.account_id, a.username, b.reason, b.expires_at \
         FROM bans b JOIN accounts a ON a.id = b.account_id \
         WHERE (b.expires_at = 0 OR b.expires_at > %d) \
         ORDER BY b.created_at DESC LIMIT %d",
        gettime(), SA_BAN_ROWS);
    mysql_tquery(gSQL, query, "Admin_PanelOnBansLoaded", "i", playerid);
    return 1;
}

public Admin_PanelOnBansLoaded(adminId)
{
    if (!IsPlayerConnected(adminId)) return 0;

    SaBanRows[adminId] = 0;

    if (mysql_errno(gSQL) != 0)
    {
        SendClientMessage(adminId, UI_ACCENT_DANGER, "[SA] Gagal membaca daftar ban.");
        return 0;
    }

    if (cache_num_rows() == 0)
    {
        SendClientMessage(adminId, UI_TEXT_MUTED, "[SA] Tidak ada ban aktif.");
        return Admin_PanelModeration(adminId);
    }

    new body[1024], line[128];
    body[0] = EOS;

    for (new row = 0; row < cache_num_rows() && row < SA_BAN_ROWS; row++)
    {
        new username[32], reason[101], expiresAt, accountDbId;
        cache_get_value_name(row, "username", username, sizeof(username));
        cache_get_value_name(row, "reason", reason, sizeof(reason));
        cache_get_value_name_int(row, "expires_at", expiresAt);
        cache_get_value_name_int(row, "account_id", accountDbId);

        new status[32];
        if (expiresAt == 0) format(status, sizeof(status), "permanen");
        else format(status, sizeof(status), "%d menit lagi", (expiresAt - gettime()) / 60);

        format(line, sizeof(line), "%s | %s | %s\n", username, status, reason);
        strcat(body, line, sizeof(body));

        SaBanAccount[adminId][SaBanRows[adminId]] = accountDbId;
        SaBanRows[adminId]++;
    }

    ShowPlayerDialog(adminId, DIALOG_SA_BANS, DIALOG_STYLE_LIST,
        "Ban aktif | klik untuk buka", body, "Buka ban", "Kembali");
    return 1;
}

stock Admin_PanelUnbanAccount(adminId, accountDbId)
{
    if (!IsServerSuperAdmin(adminId)) return 0;
    if (accountDbId <= 0) return 0;

    new query[224];
    format(query, sizeof(query), "UPDATE accounts SET is_banned = 0 WHERE id = %d", accountDbId);
    mysql_tquery(gSQL, query, "", "");

    format(query, sizeof(query),
        "UPDATE bans SET expires_at = %d WHERE account_id = %d AND (expires_at = 0 OR expires_at > %d)",
        gettime(), accountDbId, gettime());
    mysql_tquery(gSQL, query, "", "");

    new line[96];
    format(line, sizeof(line), "[SA] Ban akun #%d dibuka.", accountDbId);
    SendClientMessage(adminId, UI_ACCENT_PRIMARY, line);
    Log_Format(LOG_INFO, "[SA] unban account=%d oleh account=%d", accountDbId, AccountId[adminId]);
    return 1;
}

stock Admin_PanelHorde(adminId)
{
    if (!IsServerSuperAdmin(adminId)) return 0;

    new Float:x, Float:y, Float:z;
    GetPlayerPos(adminId, x, y, z);
    new lobbyId = Lobby_Of(adminId);
    new const E_ZOMBIE_TYPE:hordeTypes[8] = {ZT_WALKER, ZT_WALKER, ZT_RUNNER, ZT_RUNNER, ZT_CRAWLER, ZT_BLOATER, ZT_SCREAMER, ZT_BRUTE};
    new spawned = 0;

    for (new slot = 0; slot < 8; slot++)
    {
        if (Zombie_GetActiveCount() >= MAX_ZOMBIES - 1) break;
        if (Director_SpawnNearPoint(E_ZOMBIE_TYPE:hordeTypes[slot], x, y, z, 18.0, lobbyId, 6.0) == INVALID_ZOMBIE) continue;
        spawned++;
    }

    new line[96];
    format(line, sizeof(line), "[SA] Horde %d zombie dipanggil di lobi %d.", spawned, lobbyId);
    SendClientMessage(adminId, UI_ACCENT_WARN, line);
    if (spawned > 0) Director_NotifyLobby(lobbyId, "HORDE", "Kawanan zombie mendekat.", UI_ACCENT_DANGER);
    return spawned;
}

stock Admin_PanelServer(playerid)
{
    new online = 0;
    foreach (new onlineId : PlayerPool) if (Account_IsPlaying(onlineId)) online++;

    new body[1024];
    format(body, sizeof(body),
        "Info\tNilai\n\
         Pemain login\t%d\n\
         Zombie aktif\t%d / %d\n\
         Waktu dunia\t%02d:00 (%s)\n\
         Cuaca\totomatis dari jam\n\
         Kebal admin\t%s\n\
         Selftest\tJalankan pemeriksaan penuh\n",
        online, Zombie_GetActiveCount(), MAX_ZOMBIES,
        World_GetHour(), World_IsNight() ? "malam" : "siang",
        AdminGod[playerid] ? "AKTIF" : "MATI");

    ShowPlayerDialog(playerid, DIALOG_SA_SERVER, DIALOG_STYLE_TABLIST_HEADERS,
        "Server", body, "Jalankan", "Kembali");
    return 1;
}

stock Admin_PanelSelfTools(playerid)
{
    SaTarget[playerid] = playerid;
    Admin_PanelPlayerActions(playerid);
    return 1;
}

stock Admin_PanelTeleport(playerid)
{
    /* Helper teleport butuh superadmin: gate menu harus sama, kalau tidak
       admin level 1 melihat menu yang semua aksinya no-op senyap. */
    if (!IsServerSuperAdmin(playerid))
    {
        SendClientMessage(playerid, UI_ACCENT_DANGER, "[SA] Menu teleport khusus superadmin.");
        return 0;
    }

    new body[768];
    body[0] = EOS;
    strcat(body, "Tujuan\tKoordinat / efek\n", sizeof(body));
    strcat(body, "Hub / spawn utama\t1685, -2333, 13\n", sizeof(body));
    strcat(body, "Gudang penyimpanan\t1662, -2318, 13\n", sizeof(body));
    strcat(body, "Meja crafting\t1666, -2320, 13\n", sizeof(body));
    strcat(body, "Vendor\t1670, -2324, 13\n", sizeof(body));
    strcat(body, "Titik ekstraksi\t1697, -2349, 13.5\n", sizeof(body));
    strcat(body, "Shelter pribadi\tRuang custom base kamu (punya base dulu)\n", sizeof(body));
    strcat(body, "Base sendiri\tPetak base yang kamu miliki\n", sizeof(body));
    strcat(body, "Kendaraan terdekat: isi bensin\tRadius 12 m\n", sizeof(body));
    strcat(body, "Kendaraan terdekat: perbaiki penuh\tButuh Toolbox di tas\n", sizeof(body));

    ShowPlayerDialog(playerid, DIALOG_SA_TELEPORT, DIALOG_STYLE_TABLIST_HEADERS,
        "Teleport & Kendaraan", body, "Jalankan", "Kembali");
    return 1;
}

stock Admin_PanelNearestVehicle(playerid)
{
    if (GetPlayerState(playerid) == PLAYER_STATE_DRIVER) return GetPlayerVehicleID(playerid);

    new Float:playerX, Float:playerY, Float:playerZ;
    GetPlayerPos(playerid, playerX, playerY, playerZ);

    new best = INVALID_VEHICLE_ID;
    new Float:bestDistance = 12.0;

    for (new vehicleid = 1; vehicleid < MAX_VEHICLES; vehicleid++)
    {
        if (!GetVehicleModel(vehicleid)) continue;

        new Float:vehicleX, Float:vehicleY, Float:vehicleZ;
        GetVehiclePos(vehicleid, vehicleX, vehicleY, vehicleZ);
        new Float:distance = floatsqroot((vehicleX - playerX) * (vehicleX - playerX) + (vehicleY - playerY) * (vehicleY - playerY));

        if (distance <= bestDistance)
        {
            bestDistance = distance;
            best = vehicleid;
        }
    }
    return best;
}

/* ---------- dispatcher ---------- */

stock Admin_PanelResponse(playerid, dialogid, response, listitem)
{
    /* Gate SATU pintu untuk semua halaman panel: tanpa ini, case seperti
       SA_WORLD bisa di-spoofer client (dialogid 3509) untuk memicu event
       tanpa hak admin (temuan audit F-06). IsServerAdmin kini juga menuntut
       sesi playing. */
    if (!IsServerAdmin(playerid)) return 0;

    switch (dialogid)
    {
        case DIALOG_SA_MAIN:
        {
            if (!IsServerAdmin(playerid)) return 0;
            if (!response) return 1;

            switch (listitem)
            {
                case 0:
                {
                    SaPlayerPage[playerid] = 0;
                    Admin_PanelPlayers(playerid);
                }
                case 1: Admin_PanelZombies(playerid);
                case 2: Admin_PanelCategories(playerid);
                case 3: Admin_PanelSelfTools(playerid);
                case 4: Admin_PanelWorld(playerid);
                case 5: Admin_PanelTeleport(playerid);
                case 6: Admin_PanelModeration(playerid);
                case 7: Admin_PanelServer(playerid);
            }
            return 1;
        }

        case DIALOG_SA_PLAYERS:
        {
            if (!response)
            {
                SaPlayerPage[playerid] = 0;
                return Admin_PanelMain(playerid);
            }
            /* Baris navigasi di akhir: indeks == jumlah baris snapshot. */
            if (listitem == SaPlayerRows[playerid])
            {
                SaPlayerPage[playerid]++;
                return Admin_PanelPlayers(playerid);
            }
            if (listitem < 0 || listitem >= SaPlayerRows[playerid]) return Admin_PanelPlayers(playerid);

            SaTarget[playerid] = SaPlayerIds[playerid][listitem];
            Admin_PanelPlayerActions(playerid);
            return 1;
        }

        case DIALOG_SA_PLAYER_ACT:
        {
            if (!response) return Admin_PanelPlayers(playerid);

            new target = SaTarget[playerid];
            if (!IsPlayerConnected(target)) return Admin_PanelPlayers(playerid);

            switch (listitem)
            {
                case 0: Admin_GodToggle(playerid, target);
                case 1: Admin_FullRestore(playerid, target);
                case 2: Admin_AddCash(playerid, target, 1000);
                case 3: Admin_AddCash(playerid, target, 10000);
                case 4: Admin_AddCash(playerid, target, 100000);
                case 5: Admin_SetCash(playerid, target, 0);
                case 6: Admin_AddXp(playerid, target, 100);
                case 7: Admin_AddXp(playerid, target, 1000);
                case 8: Admin_SetSkillPoints(playerid, target, 5);
                case 9: Admin_FreezeToggle(playerid, target);
                case 10: Admin_TeleportToPlayer(playerid, target);
                case 11: Admin_BringPlayer(playerid, target);
                case 12:
                {
                    Admin_PanelCategories(playerid);
                    return 1;
                }
                case 13:
                {
                    /* Kick lewat panel wajib gate admin + anti ban-admin:
                       level 1 tidak boleh menendang sesama admin/superadmin. */
                    if (!IsServerAdmin(playerid)) return 1;
                    if (AdminLevel[target] >= ADMIN_LEVEL_ADMIN && !IsServerSuperAdmin(playerid))
                    {
                        SendClientMessage(playerid, UI_ACCENT_DANGER, "[SA] Hanya superadmin yang bisa menendang admin.");
                        return 1;
                    }
                    new targetName[MAX_PLAYER_NAME + 1];
                    GetPlayerName(target, targetName, sizeof(targetName));
                    SendClientMessage(playerid, UI_ACCENT_WARN, "[SA] Menendang pemain...");
                    Log_Format(LOG_INFO, "[SA] kick account=%d oleh account=%d", AccountId[target], AccountId[playerid]);
                    SetTimerEx("Moderation_KickTarget", 800, false, "ii", target, AccountId[target]);
                    return Admin_PanelPlayers(playerid);
                }
                case 14: Moderation_Ban(playerid, target, 30, "dari panel superadmin");
                case 15: Moderation_Ban(playerid, target, 0, "dari panel superadmin");
                default: return 1;
            }

            Admin_PanelPlayerActions(playerid);
            return 1;
        }

        case DIALOG_SA_ZOMBIES:
        {
            if (!response) return Admin_PanelMain(playerid);

            if (listitem == 8)
            {
                Admin_ClearZombies(playerid);
                Admin_PanelZombies(playerid);
                return 1;
            }

            if (listitem == 9)
            {
                Admin_PanelHorde(playerid);
                Admin_PanelZombies(playerid);
                return 1;
            }

            if (listitem >= 0 && listitem < _:ZT_BOSS + 1)
            {
                SaZombieType[playerid] = listitem;
                Admin_PanelZombieCount(playerid);
            }
            return 1;
        }

        case DIALOG_SA_ZOMBIE_COUNT:
        {
            if (!response) return Admin_PanelZombies(playerid);

            new const counts[6] = {1, 3, 5, 10, 25, 50};
            if (listitem < 0 || listitem > 5) return 1;

            Admin_SpawnZombies(playerid, E_ZOMBIE_TYPE:SaZombieType[playerid], counts[listitem]);
            Admin_PanelZombieCount(playerid);
            return 1;
        }

        case DIALOG_SA_CATEGORIES:
        {
            if (!response) return Admin_PanelMain(playerid);

            switch (listitem)
            {
                case 0: SaCategory[playerid] = _:ITEM_NONE;
                case 1: SaCategory[playerid] = _:ITEM_CONSUMABLE;
                case 2: SaCategory[playerid] = _:ITEM_MATERIAL;
                case 3: SaCategory[playerid] = _:ITEM_TOOL;
                case 4: SaCategory[playerid] = _:ITEM_WEAPON;
                case 5: SaCategory[playerid] = _:ITEM_BLUEPRINT;
                default: return 1;
            }

            SaItemPage[playerid] = 0;
            Admin_PanelItems(playerid);
            return 1;
        }

        case DIALOG_SA_ITEMS:
        {
            if (!response) return Admin_PanelCategories(playerid);

            if (listitem == SaItemRows[playerid])
            {
                SaItemPage[playerid]++;
                Admin_PanelItems(playerid);
                return 1;
            }

            if (listitem < 0 || listitem >= SaItemRows[playerid]) return 1;
            SaSelectedItem[playerid] = SaItemIds[playerid][listitem];
            Admin_PanelItemAmount(playerid, SaSelectedItem[playerid]);
            return 1;
        }

        case DIALOG_SA_ITEM_AMOUNT:
        {
            if (!response) return Admin_PanelItems(playerid);

            new target = IsPlayerConnected(SaTarget[playerid]) ? SaTarget[playerid] : playerid;
            new const amounts[5] = {1, 5, 10, 25, 50};
            if (listitem < 0 || listitem > 4) return 1;

            if (!Item_IsValid(SaSelectedItem[playerid])) return Admin_PanelCategories(playerid);
            Admin_GiveItem(playerid, target, SaSelectedItem[playerid], amounts[listitem]);
            Admin_PanelItems(playerid);
            return 1;
        }

        case DIALOG_SA_WORLD:
        {
            if (!response) return Admin_PanelMain(playerid);

            switch (listitem)
            {
                case 0: Event_Run(0);
                case 1: Event_Run(1);
                case 2: Event_Run(2);
                case 3: Admin_PanelHorde(playerid);
                case 4: Admin_SetHour(playerid, 6);
                case 5: Admin_SetHour(playerid, 12);
                case 6: Admin_SetHour(playerid, 18);
                case 7: Admin_SetHour(playerid, 22);
                case 8: Admin_SetHour(playerid, 0);
                default: return 1;
            }

            Admin_PanelWorld(playerid);
            return 1;
        }

        case DIALOG_SA_TELEPORT:
        {
            if (!IsServerAdmin(playerid)) return 0;
            if (!response) return Admin_PanelMain(playerid);

            switch (listitem)
            {
                case 0: Admin_Teleport(playerid, playerid, 1685.0, -2333.0, 13.0);
                case 1: Admin_Teleport(playerid, playerid, 1662.0, -2318.0, 13.0);
                case 2: Admin_Teleport(playerid, playerid, 1666.0, -2320.0, 13.0);
                case 3: Admin_Teleport(playerid, playerid, 1670.0, -2324.0, 13.0);
                case 4: Admin_Teleport(playerid, playerid, 1697.0, -2349.0, 13.5);
                case 5:
                {
                    /* Shelter = ruang custom VW unik per base (bukan koordinat
                       telanjang di VW lobby — itu jatuh dari langit, audit #6). */
                    Shelter_Enter(playerid);
                }
                case 6:
                {
                    new Float:plotX, Float:plotY, Float:plotZ;
                    Base_GetPlot(playerid, plotX, plotY, plotZ);
                    if (plotX == 0.0 && plotY == 0.0)
                    {
                        SendClientMessage(playerid, UI_ACCENT_WARN, "[SA] Kamu belum punya base.");
                        return Admin_PanelTeleport(playerid);
                    }
                    Admin_Teleport(playerid, playerid, plotX, plotY, plotZ);
                }
                case 7:
                {
                    new vehicleid = Admin_PanelNearestVehicle(playerid);
                    if (vehicleid == INVALID_VEHICLE_ID)
                    {
                        SendClientMessage(playerid, UI_ACCENT_WARN, "[SA] Tidak ada kendaraan dalam 12 m.");
                        return Admin_PanelTeleport(playerid);
                    }
                    Vehicle_AddFuel(vehicleid, 100.0);
                    SendClientMessage(playerid, UI_ACCENT_PRIMARY, "[SA] Tangki kendaraan diisi penuh.");
                }
                case 8:
                {
                    if (Admin_PanelNearestVehicle(playerid) == INVALID_VEHICLE_ID)
                    {
                        SendClientMessage(playerid, UI_ACCENT_WARN, "[SA] Tidak ada kendaraan dalam 12 m.");
                        return Admin_PanelTeleport(playerid);
                    }
                    /* Perbaikan penuh normal butuh Toolbox; admin diberi 1 lalu langsung
                       terpakai oleh proses repair, jadi saldo item bersih nol. */
                    Inventory_Grant(playerid, ITEM_TOOLBOX, 1, "admin_repair_temp");
                    Garage_RepairNearestEx(playerid, true);
                }
                default: return 1;
            }

            Admin_PanelTeleport(playerid);
            return 1;
        }

        case DIALOG_SA_MODERATION:
        {
            if (!response) return Admin_PanelMain(playerid);

            if (listitem == 0) Admin_PanelBans(playerid);
            else Admin_PanelPlayers(playerid);
            return 1;
        }

        case DIALOG_SA_BANS:
        {
            if (!response) return Admin_PanelModeration(playerid);
            if (listitem < 0 || listitem >= SaBanRows[playerid]) return Admin_PanelBans(playerid);

            Admin_PanelUnbanAccount(playerid, SaBanAccount[playerid][listitem]);
            Admin_PanelBans(playerid);
            return 1;
        }

        case DIALOG_SA_SERVER:
        {
            if (!response) return Admin_PanelMain(playerid);
            if (listitem == 5)
            {
                Selftest_Run(playerid);
                return 1;
            }
            Admin_PanelServer(playerid);
            return 1;
        }
    }
    return 0;
}