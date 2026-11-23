/* LAST STAND: San Andreas Quarantine
   Extraction survival for SA-MP / open.mp, playable on PC and SA-MP Mobile.
   Module order matters: state -> inventory -> combat -> zombie -> world -> ui -> commands. */

/* Server: maxplayers 200 + maxnpc 200 = slot id 0..399. Default a_samp 1000
   memboroskan ~25 MB array global dan memperlambat loop (audit performa F6). */
#define MAX_PLAYERS 400
/* Heap+stack default compiler hanya 16 KB (4096 cells), padahal estimasi
   pemakaian maks kode ini ~20 KB (trim zombie 2x512 sel, snapshot trade,
   body dialog 4096) — collision diam-diam (audit performa F2).
   #pragma dynamic dalam CELLS: 262144 cells = 1 MB, terverifikasi laporan
   compiler "Stack/heap size: 1048576 bytes". */
#pragma dynamic 262144

#include <a_samp>
#include <YSI_Data\y_iterate>
#include <YSI_Coding\y_va>
#include <streamer>
#include <sscanf2>
#include "../include/a_mysql.inc"
#include "../include/samp_bcrypt.inc"

#include "../include/fcnpc.inc"
/* Warning 234 muncul dari fungsi stok vendored yang tidak pernah dipanggil
   proyek (CA_DestroyObject_DC memakai API Iter_SafeRemove lama). Disenyapkan
   di sini supaya gate "0 warning" tetap tegas untuk kode kita sendiri,
   tanpa mengubah file pihak ketiga. */
#pragma warning push
#pragma warning disable 234
#include "../include/colandreas.inc"
#pragma warning pop

#include "modules/config/constants.inc"
#include "modules/config/items.inc"
#include "modules/config/loot_tables.inc"
#include "modules/config/recipes.inc"
#include "modules/config/zombies.inc"
#include "modules/config/structures.inc"
#include "modules/config/lobbies.inc"

#include "modules/core/logger.inc"
#include "modules/core/instance.inc"
#include "modules/core/zombie_stats.inc"
#include "modules/core/perf.inc"
#include "modules/core/mysql.inc"
#include "modules/core/transaction.inc"
#include "modules/core/npc_budget.inc"
#include "modules/security/anticheat_core.inc"
#include "modules/core/spatial_hash.inc"
#include "modules/world/zones.inc"
#include "modules/world/daynight.inc"

#include "modules/ui/toast.inc"
#include "modules/player/state.inc"
#include "modules/admin/admin_state.inc"
#include "modules/inventory/inventory_core.inc"
#include "modules/inventory/bag.inc"
#include "modules/inventory/hotbar.inc"
#include "modules/inventory/container_lock.inc"
#include "modules/account/account_core.inc"
#include "modules/core/slotqueue.inc"
#include "modules/player/deaths.inc"
#include "modules/content/lore.inc"
#include "modules/account/char_create.inc"
#include "modules/admin/roles.inc"
#include "modules/admin/moderation.inc"
#include "modules/admin/audit.inc"
#include "modules/inventory/invpers.inc"
#include "modules/inventory/stash.inc"
#include "modules/player/tutorial.inc"
#include "modules/core/telemetry.inc"
#include "modules/progression/skills.inc"
#include "modules/progression/leaderboard.inc"
#include "modules/progression/contracts.inc"
#include "modules/player/party.inc"
#include "modules/player/party_stash.inc"
#include "modules/player/pstash_log.inc"
#include "modules/ui/craft_ui.inc"
#include "modules/economy/vendor.inc"
#include "modules/economy/trade.inc"
#include "modules/security/achecks.inc"
#include "modules/security/dps.inc"
#include "modules/account/account_auth.inc"

#include "modules/combat/damage_contract.inc"
#include "modules/combat/weapons.inc"
#include "modules/combat/noise.inc"
#include "modules/loot/drops.inc"
#include "modules/loot/nodes.inc"
#include "modules/loot/corpse.inc"
#include "modules/loot/grave.inc"

#include "modules/zombie/zombie_core.inc"
#include "modules/combat/damage_pipeline.inc"
#include "modules/player/wounds.inc"
#include "modules/zombie/zombie_ai.inc"
#include "modules/zombie/spawn_director.inc"
#include "modules/world/events.inc"
#include "modules/world/base_members.inc"
#include "modules/world/bases.inc"
#include "modules/world/base_build.inc"
#include "modules/zombie/horde.inc"
#include "modules/world/escort.inc"
#include "modules/world/companion.inc"
#include "modules/world/shelter.inc"
#include "modules/world/mapping.inc"
#include "modules/world/decor.inc"
#include "modules/world/lobbies.inc"
#include "modules/world/vehicles.inc"
#include "modules/world/garage.inc"
#include "modules/player/extraction.inc"

#include "modules/player/needs.inc"
#include "modules/interact/dispatcher.inc"
#include "modules/ui/hud.inc"
#include "modules/ui/inventory_ui.inc"
#include "modules/ui/menu.inc"
#include "modules/core/stats.inc"
#include "modules/core/selftest.inc"
#include "modules/admin/superadmin.inc"
#include "modules/admin/panel.inc"
#include "modules/admin/commands.inc"

forward Selftest_BcryptProbe();
/* Spawn pertama per sesi dapat 60 peluru; respawn hanya 12 (audit F3). */
new bool:FirstSpawnDone[MAX_PLAYERS];
forward BcryptProbe_OnHashed(playerid);
forward World_NearbyHint_Tick(playerid);

public World_NearbyHint_Tick(playerid)
{
    World_NearbyHint(playerid);
    return 1;
}
forward BcryptProbe_OnVerified(playerid, bool:success);
forward Zombie_Tick();
forward Director_Tick();
forward Director_NightWatch();
forward Needs_Tick();
forward HUD_Tick();
forward Player_Tick();
forward Escort_Tick();
new TimerHandles[20];

public OnGameModeInit()
{
    SetGameModeText("LAST STAND 0.1");
    Hash_Init();
    Escort_Init();
    Companion_Init();
    Admin_RolesInit();
    Party_InitAll();

    // Tanpa class, SA-MP menaruh pemain di 0,0,0 (tengah laut). Selalu spawn di kamp.
    AddPlayerClass(0, SPAWN_X, SPAWN_Y, SPAWN_Z, SPAWN_ANGLE, 0, 0, 0, 0, 0, 0);
    AddPlayerClass(1, SPAWN_X, SPAWN_Y, SPAWN_Z, SPAWN_ANGLE, 0, 0, 0, 0, 0, 0);

    SQL_Boot();
    if (SQL_IsReady()) Tx_Boot();

    /* Retensi telemetri (audit performa F5): kill_log bertambah 1 baris per
       kill dan sebelumnya tak pernah dipurge — query /top & /stats makin
       berat tanpa batas. kill_log 90 hari, flag anticheat 30 hari. */
    if (SQL_IsReady())
    {
        mysql_tquery(gSQL, "DELETE FROM kill_log WHERE created_at < (UNIX_TIMESTAMP() - 7776000) * 1000", "", "");
        mysql_tquery(gSQL, "DELETE FROM anticheat_flags WHERE created_at < (UNIX_TIMESTAMP() - 2592000) * 1000", "", "");
        /* Kontrak hari lama tidak berguna lagi (4 row/karakter/hari — audit T13). */
        mysql_tquery(gSQL, "DELETE FROM contracts WHERE day_id < (UNIX_TIMESTAMP() / 86400 - 7)", "", "");
        /* deaths: 90 hari, pola sama dengan kill_log — tanpa ini tabel tumbuh
           sebaris per kematian selamanya. */
        mysql_tquery(gSQL, "DELETE FROM deaths WHERE created_at < UNIX_TIMESTAMP() - 7776000", "", "");
    }

    if (CA_Init() == 0)
    {
        Log_Format(LOG_WARN, "[BOOT] ColAndreas tanpa data .cadb: raycast dimatikan, damage LOS selalu lolos.");
    }
    else
    {
        // Bukti nyata data collision terpakai, bukan sekadar plugin ter-load.
        new Float:probeX, Float:probeY, Float:probeZ;
        new probeModel = CA_RayCastLine(SPAWN_X, SPAWN_Y, SPAWN_Z + 5.0,
            SPAWN_X, SPAWN_Y, SPAWN_Z - 25.0, probeX, probeY, probeZ);
        Log_Format(LOG_INFO, "[BOOT] ColAndreas aktif: raycast uji model %d di z %.2f.", probeModel, probeZ);

        if (probeModel == 0)
        {
            Log_Format(LOG_WARN, "[BOOT] Raycast uji meleset: data .cadb mungkin beda versi dengan plugin.");
        }
    }

    FCNPC_UseMoveMode(FCNPC_MOVE_MODE_COLANDREAS);
    FCNPC_UseMovePathfinding(FCNPC_MOVE_PATHFINDING_RAYCAST);
    /* Default FCNPC 50 ms = 20 sync/detik per NPC. Dengan budget besar itu
       membanjiri koneksi mobile; 80 ms masih halus dan memotong ~40% paket. */
    FCNPC_SetUpdateRate(FCNPC_SYNC_RATE_MS);

    for (new slot = 0; slot < MAX_TEST_VEHICLES; slot++) TestVehicle[slot] = INVALID_VEHICLE_ID;

    ShowPlayerMarkers(PLAYER_MARKERS_MODE_OFF);
    DisableInteriorEnterExits();
    SetNameTagDrawDistance(40.0);
    SetWeather(1);
    SetWorldTime(WorldHour);

    Map_Init();
    Decor_Init();

    Interact_Create(INTERACT_EXTRACT, EXTRACTION_X, EXTRACTION_Y, EXTRACTION_Z, 3.0, "Ekstraksi\n{Y} tekan Y");
    Interact_Create(INTERACT_STASH, 1662.0, -2318.0, 13.5, 2.5, "Stash Kamp\n{Y} tekan Y");
    Interact_Create(INTERACT_CRAFT, 1666.0, -2320.0, 13.5, 2.5, "Meja Crafting\n{Y} tekan Y");
    Interact_Create(INTERACT_SHOP, 1670.0, -2324.0, 13.5, 2.5, "Pedagang\n{Y} tekan Y");
    Interact_Create(INTERACT_GARAGE, GARAGE_X, GARAGE_Y, GARAGE_Z, 3.0, "Garasi\n{Y} tekan Y");
    Interact_Create(INTERACT_FUEL, FUEL_PUMP_X, FUEL_PUMP_Y, FUEL_PUMP_Z, 3.0, "Pom Bensin\n{Y} tekan Y");
    Garage_CreateWorldObjects();

    TimerHandles[0] = SetTimer("Zombie_Tick", ZOMBIE_TICK_MS, true);
    TimerHandles[1] = SetTimer("Director_Tick", DIRECTOR_TICK_MS, true);
    TimerHandles[2] = SetTimer("Director_NightWatch", 60000, true);
    TimerHandles[3] = SetTimer("Needs_Tick", NEEDS_TICK_MS, true);
    TimerHandles[4] = SetTimer("HUD_Tick", 500, true);
    TimerHandles[5] = SetTimer("Player_Tick", 500, true);
    TimerHandles[6] = SetTimer("Loot_ExpireTick", 15000, true);
    TimerHandles[7] = SetTimer("World_Tick", WORLD_HOUR_TICK_MS, true);
    TimerHandles[8] = SetTimer("Account_AutosaveTick", AUTOSAVE_TICK_MS, true);
    TimerHandles[9] = SetTimer("AC_PositionTick", AC_SAMPLE_INTERVAL_MS, true);
    TimerHandles[10] = SetTimer("Base_Tick", 2000, true);
    TimerHandles[11] = SetTimer("Horde_Tick", HORDE_TICK_MS, true);
    TimerHandles[12] = SetTimer("Event_Tick", 5000, true);
    TimerHandles[13] = SetTimer("LootNode_Tick", 10000, true);
    TimerHandles[14] = SetTimer("Corpse_Tick", 30000, true);
    TimerHandles[15] = SetTimer("Grave_Tick", 30000, true);
    TimerHandles[16] = SetTimer("WildVeh_Tick", 20000, true);
    TimerHandles[17] = SetTimer("VehicleFuel_Tick", 2000, true);
    TimerHandles[18] = SetTimer("Escort_Tick", 2000, true);
    TimerHandles[19] = SetTimer("Companion_Tick", 2000, true);
    SetTimer("Queue_Tick", QUEUE_TICK_MS, true);

    SetTimer("Selftest_Boot", 8000, false);
    Selftest_BcryptProbe();
    SetTimer("Director_Report", 60000, true);

    // Barricade adalah objek dunia: muat sekali setelah koneksi MySQL siap.
    SetTimer("Base_BootLoad", 3000, false);
    SetTimer("Grave_BootLoad", 4000, false);

    Log_Format(LOG_INFO, "[BOOT] LAST STAND siap. Zombie max %d, budget %d+%d/pemain, sync %d ms, zone %d.",
        MAX_ZOMBIES, DIRECTOR_BUDGET_BASE, DIRECTOR_BUDGET_PER_PLAYER, FCNPC_SYNC_RATE_MS, MAX_ZONE_COUNT);

    new instanceName[64];
    GetServerVarAsString("hostname", instanceName, sizeof(instanceName));
    Log_Format(LOG_INFO, "[BOOT] instans: %s | port %d", instanceName, GetServerVarAsInt("port"));
    return 1;
}

public OnDialogResponse(playerid, dialogid, response, listitem, inputtext[])
{
    if (Account_OnDialogResponse(playerid, dialogid, response, listitem, inputtext)) return 1;
    if (InventoryUI_Handle(playerid, dialogid, response, listitem, inputtext)) return 1;
    if (Stash_OnDialogResponse(playerid, dialogid, response, listitem, inputtext)) return 1;
    if (Trade_OnDialogResponse(playerid, dialogid, response, listitem, inputtext)) return 1;
    if (Party_OnDialogResponse(playerid, dialogid, response, listitem, inputtext)) return 1;
    if (PartyStash_OnDialogResponse(playerid, dialogid, response, listitem, inputtext)) return 1;
    if (Admin_PanelResponse(playerid, dialogid, response, listitem)) return 1;
    if (Companion_OnDialogResponse(playerid, dialogid, response, listitem, inputtext)) return 1;
    if (CraftUI_OnDialogResponse(playerid, dialogid, response, listitem, inputtext)) return 1;
    if (Vendor_OnDialogResponse(playerid, dialogid, response, listitem, inputtext)) return 1;
    if (Skills_OnDialogResponse(playerid, dialogid, response, listitem, inputtext)) return 1;
    if (BaseBuild_OnDialogResponse(playerid, dialogid, response, listitem, inputtext)) return 1;
    if (Corpse_OnDialogResponse(playerid, dialogid, response, listitem)) return 1;
    /* makam & kontrak sebelumnya TIDAK dirouter: dialog terbuka tapi respons
       tidak pernah sampai ke handler (UI mati total — audit code-reviewer #1/#3). */
    if (Grave_OnDialogResponse(playerid, dialogid, response, listitem)) return 1;
    if (Contracts_OnDialogResponse(playerid, dialogid, response, listitem)) return 1;
    if (Lobby_OnDialogResponse(playerid, dialogid, response, listitem)) return 1;
    if (Garage_OnDialogResponse(playerid, dialogid, response, listitem)) return 1;
    if (Menu_OnDialogResponse(playerid, dialogid, response, listitem, inputtext)) return 1;
    return 0;
}

public OnGameModeExit()
{
    for (new timerIndex = 0; timerIndex < sizeof(TimerHandles); timerIndex++)
    {
        if (TimerHandles[timerIndex] != 0) KillTimer(TimerHandles[timerIndex]);
    }

    /* Shutdown tidak punya tick berikutnya: hapus langsung, bukan lewat antrean. */
    foreach (new zombieid : ZombiePool)
    {
        Zombie_DestroyNow(zombieid);
    }
    foreach (new playerid : PlayerPool)
    {
        /* Simpan progres terakhir sebelum koneksi ditutup: tanpa ini shutdown
           membuang sampai 2 menit progres (autosave berikutnya tidak terjadi). */
        if (Account_IsPlaying(playerid))
        {
            Contracts_Save(playerid);
            Companion_Save(playerid);
            Account_SaveCharacter(playerid);
        }
    }
    Escort_Shutdown();
    Vehicle_DestroyAll();
    /* Jangan close koneksi saat antrean save exit masih berjalan: worker
       thread bisa membuang query yang belum dieksekusi (audit T5). Tunggu
       drain (maks ~5 dtk) baru tutup. */
    SetTimerEx("SQL_DelayedShutdown", 500, false, "i", 0);
    Log_Info("[SHUTDOWN] semua zombie dibersihkan.");
    return 1;
}

forward SQL_DelayedShutdown(attempt);
public SQL_DelayedShutdown(attempt)
{
    if (mysql_unprocessed_queries(gSQL) > 0 && attempt < 10)
    {
        SetTimerEx("SQL_DelayedShutdown", 500, false, "i", attempt + 1);
        return 1;
    }
    Tx_Shutdown();
    SQL_Shutdown();
    Log_Info("[SHUTDOWN] antrean SQL terkuras, koneksi ditutup.");
    return 1;
}

public OnPlayerConnect(playerid)
{
    ConnectGen[playerid]++;
    FirstSpawnDone[playerid] = false;
    Player_InitSurvival(playerid);
    Needs_ResetPlayer(playerid);
    Inventory_Clear(playerid);
    HUD_Create(playerid);
    Account_ResetPlayer(playerid);
    Stash_Reset(playerid);
    Inventory_InitCondition(playerid);
    AC_ResetPlayer(playerid);
    Base_ResetPlayer(playerid);
    BaseBuild_ResetPlayer(playerid);
    Dps_Reset(playerid);
    Shelter_Reset(playerid);
    Skills_Reset(playerid);
    Corpse_ResetPlayer(playerid);
    Grave_ResetPlayer(playerid);
    Contracts_Reset(playerid);
    Garage_Reset(playerid);
    Queue_OnPlayerConnect(playerid);
    Tutorial_Reset(playerid);
    /* TradePartner default 0, sentinel INVALID_PLAYER_ID=65535: tanpa reset
       eksplisit setiap pemain baru dianggap sedang trade dengan playerid 0. */
    Trade_Reset(playerid);

    Iter_Add(PlayerPool, playerid);

    /* Jejak koneksi: dipakai saat pemain bilang "tidak bisa daftar" — kita bisa
       pastikan nick-nya benar-benar sampai ke server atau tidak. */
    new joinName[MAX_PLAYER_NAME + 1], joinIp[16], joinLine[128];
    GetPlayerName(playerid, joinName, sizeof(joinName));
    GetPlayerIp(playerid, joinIp, sizeof(joinIp));
    format(joinLine, sizeof(joinLine), "[JOIN] id %d nick '%s' ip %s", playerid, joinName, joinIp);
    /* joinLine adalah DATA, bukan format string: nick SA-MP boleh mengandung '%'
       (mis. "xx%sxx") — meneruskannya sebagai fmt membuat va_format membaca
       argumen sampah = crash server jarak jauh via nickname. */
    Log_Format(LOG_INFO, "%s", joinLine);

    SendClientMessage(playerid, UI_ACCENT_PRIMARY,
        "[LAST STAND] Selamat datang di San Andreas Quarantine.");
    SendClientMessage(playerid, UI_TEXT_SECONDARY,
        "Tembakan berisik menarik zombie. Tekan Y di hotspot, atau /interact di mobile.");

    Account_BeginAuth(playerid);
    return 1;
}

public OnPlayerDisconnect(playerid, reason)
{
    #pragma unused reason

    if (Account_IsPlaying(playerid))
    {
        Party_OnDisconnect(playerid);
        PartyStash_OnDisconnect(playerid);
        Companion_OnDisconnect(playerid);
        Admin_ResetPlayer(playerid);
        Admin_ResetSession(playerid);
        Admin_PanelReset(playerid);
        Trade_OnDisconnect(playerid);
        Garage_OnPlayerDisconnect(playerid);
        Contracts_Save(playerid);
        Account_SaveCharacter(playerid);
    }

    /* State pembuatan karakter selalu dibersihkan, termasuk pemain yang keluar di tengah wizard. */
    CharCreate_Reset(playerid);

    /* Objek shelter + pintu wajib dihancurkan saat disconnect: tanpa ini
       41 objek bocor di VW shelter sampai slot dipakai ulang (audit #12). */
    Shelter_Reset(playerid);

    /* Level & kunci admin tidak boleh terbawa ke pemain berikutnya di slot yang sama. */
    Admin_ResetPlayer(playerid);
    Admin_ResetKeyState(playerid);
    /* God/Frozen WAJIB reset tanpa syarat playing (audit F-19): superadmin
       bisa membekukan target pre-auth; tanpa ini penghuni slot berikutnya
       mewarisi god/freeze. */
    Admin_ResetSession(playerid);

    Hotbar_Reset(playerid);
    Lock_ReleasePlayer(playerid);
    Iter_Remove(PlayerPool, playerid);
    Hash_PlayerRemove(playerid);
    Bag_ResetPlayer(playerid);
    HUD_Destroy(playerid);
    return 1;
}

public OnVehicleDeath(vehicleid, killerid)
{
    #pragma unused killerid
    Vehicle_OnDeath(vehicleid);
    return 1;
}

public OnPlayerStateChange(playerid, newstate, oldstate)
{
    #pragma unused oldstate
    Vehicle_OnPlayerStateChange(playerid, newstate);
    return 1;
}

public OnPlayerRequestClass(playerid, classid)
{
    #pragma unused classid

    /* F4/paket request-class bisa dikirim client kapan saja. Tanpa gate ini
       pemain hidup/downed bisa respawn paksa: HP penuh, lolos penalti mati
       (makam tak pernah dibuat), reset sampel anticheat, ammo gratis.
       Hanya izinkan saat benar-benar mati (HP engine 0) atau belum playing
       (spawn hold pra-login). */
    if (Account_IsPlaying(playerid))
    {
        new Float:engineHp;
        GetPlayerHealth(playerid, engineHp);
        if (engineHp > 0.0) return 0;
    }

    SetSpawnInfo(playerid, 0, 287, 1685.0, -2333.0, 13.5, 180.0, 0, 0, 0, 0, 0, 0);
    SpawnPlayer(playerid);
    return 1;
}

public OnPlayerSpawn(playerid)
{
    // Spawn otomatis SA-MP terjadi sebelum login: tahan pemain di tempat.
    if (!Account_IsPlaying(playerid))
    {
        Account_LockPlayer(playerid);
        SetPlayerPos(playerid, SPAWN_X, SPAWN_Y, SPAWN_Z);
        SetPlayerFacingAngle(playerid, SPAWN_ANGLE);
        return 1;
    }

    PlayerSurvival[playerid][psHealth] = FULL_HP;
    PlayerSurvival[playerid][psDowned] = 0;
    /* Mati = infeksi bersih (audit D1/N1): kode lama mereset stage tapi NILAI
       infeksi tetap 100 → gigitan berikutnya langsung turned lagi (loop
       re-turn senyap). Penalti mati sudah berat (makam + biaya medis). */
    PlayerSurvival[playerid][psInfection] = 0.0;
    PlayerSurvival[playerid][psInfectionStage] = 0;
    Wounds_Clear(playerid);
    SetPlayerHealth(playerid, FULL_HP);

    Lobby_ApplyWorld(playerid);
    AC_ClearSample(playerid);
    LootNode_OnPlayerSpawn(playerid);

    // Tas di punggung harus ikut tersinkron setiap spawn (model hilang saat mati).
    Bag_UpdateVisual(playerid);

    // HUD harus tampil lagi setelah respawn: sebagian client membuang
    // PlayerTextDraw saat pemain mati/ganti karakter.
    HUD_Reshow(playerid);
    WorldHintRetry[playerid] = 0;

    // Kendaraan pertama dijamin ada di dekat pemain, bukan menunggu undian tick.
    WildVeh_EnsureNearLobby(Lobby_Of(playerid));
    WildVeh_GuaranteeSpawnCar(playerid);
    SetTimerEx("World_NearbyHint_Tick", 2500, false, "i", playerid);

    Menu_Onboarding(playerid);

    ResetPlayerWeapons(playerid);
    GivePlayerWeapon(playerid, 4, 1);
    /* 60 peluru hanya spawn pertama per sesi; respawn 12 (audit F3: refill
       60 tiap respawn = farm kematian ~$27rb/jam nilai ammo + tekanan
       amunisi DESIGN batal). */
    GivePlayerWeapon(playerid, 22, FirstSpawnDone[playerid] ? 12 : 60);

    /* Senjata tersimpan ikut kembali saat respawn (audit N2): tanpa ini
       mati = hilang semua senjata sampai relog — penalti tersembunyi di
       luar makam. Data dari save terakhir (autosave 2 menit). */
    if (FirstSpawnDone[playerid]) Weapons_Load(playerid);
    FirstSpawnDone[playerid] = true;

    SetPlayerSkillLevel(playerid, WEAPONSKILL_PISTOL, 200);

    new Float:x, Float:y, Float:z;
    GetPlayerPos(playerid, x, y, z);

    // Spawn pertama dan setiap respawn mendarat di kamp, bukan di titik acak server.
    if (x == 0.0 && y == 0.0)
    {
        SetPlayerPos(playerid, SPAWN_X, SPAWN_Y, SPAWN_Z);
        SetPlayerFacingAngle(playerid, SPAWN_ANGLE);
    }

    /* Bed di base memindahkan titik respawn, tapi hanya kalau base punya >= 2 wall. */
    new Float:bedX, Float:bedY, Float:bedZ;
    if (BaseBuild_BedPosition(playerid, bedX, bedY, bedZ))
    {
        SetPlayerPos(playerid, bedX, bedY, bedZ);
        SendClientMessage(playerid, UI_ACCENT_PRIMARY, "[BASE] Respawn di bed base sendiri.");
    }

    /* Rekrutan ikut spawn ulang bersama pemiliknya (data sudah dimuat saat login). */
    Companion_Spawn(playerid);
    return 1;
}

public OnPlayerDeath(playerid, killerid, reason)
{
    /* killerid/reason dulu dibuang di sini; sekarang dicatat supaya "kenapa
       saya mati" bisa dijawab dan kill farming punya bukti (tabel deaths). */
    Deaths_Record(playerid, killerid, reason);

    PlayerSurvival[playerid][psDeaths]++;
    PlayerSurvival[playerid][psDowned] = 0;
    PlayerSurvival[playerid][psHealth] = 0.0;
    PlayerSurvival[playerid][psStreakCount] = 0;
    Wounds_Clear(playerid);

    /* Biaya medis (DESIGN §11, sink berulang yang sebelumnya tidak ada —
       audit F3/E2): $100 + $25/level, cap $600, tidak membuat cash negatif. */
    new medicalFee = 100 + PlayerSurvival[playerid][psLevel] * 25;
    if (medicalFee > 600) medicalFee = 600;
    if (PlayerSurvival[playerid][psCash] > 0)
    {
        new paid = (medicalFee < PlayerSurvival[playerid][psCash]) ? medicalFee : PlayerSurvival[playerid][psCash];
        PlayerSurvival[playerid][psCash] -= paid;
        new feeLine[96];
        format(feeLine, sizeof(feeLine), "[MEDIS] Biaya evakuasi $%d dipotong dari cash.", paid);
        SendClientMessage(playerid, UI_ACCENT_WARN, feeLine);
    }

    /* Tas jatuh sebagai makam SEBELUM dibersihkan: tidak ada item yang hilang diam-diam. */
    Escort_CancelForPlayer(playerid, "pengantar tewas");
    /* Shelter dibersihkan saat mati: psInShelter yang tetap 1 membuat
       ShelterDoor_Tick melempar pemain yang sudah respawn kembali ke posisi
       pra-shelter (membatalkan respawn bed) + objek bocor (audit #12). */
    Shelter_Reset(playerid);
    Grave_CreateFromPlayer(playerid);
    Inventory_Clear(playerid);
    return 1;
}

/* Every hit lands here first: returning 0 stops the engine from applying its own
   damage, so server-side validation stays authoritative. */
public OnPlayerGiveDamage(playerid, damagedid, Float:amount, weaponid, bodypart)
{
    if (damagedid == INVALID_PLAYER_ID) return 0;
    if (IsPlayerNPC(playerid)) return 0;

    if (Party_IsSameParty(playerid, damagedid))
    {
        SendClientMessage(playerid, UI_ACCENT_WARN, "[PARTY] Friendly fire dimatikan untuk anggota party.");
        return 0;
    }

    Damage_Apply(DAMAGE_KIND_PLAYER, damagedid, playerid, amount, weaponid, bodypart);
    return 0;
}

public OnPlayerTakeDamage(playerid, issuerid, Float:amount, weaponid, bodypart)
{
    // NPC melee damage is already applied by our own attack resolution.
    if (issuerid != INVALID_PLAYER_ID && IsPlayerNPC(issuerid)) return 0;

    /* Semua damage engine (ledakan, tabrakan kendaraan, jatuh, api) WAJIB
       lewat Damage_Apply: return 1 membiarkan engine menerapkan damage
       langsung, melewati cek party/hub/grace/downed/god/cap (audit F-05).
       Return 0 = engine tidak menerapkan apa pun; Player_ApplyDamage yang
       menulis HP. */
    if (PlayerSurvival[playerid][psHealth] <= 0.0)
    {
        Player_EnterDownedState(playerid);
        return 0;
    }
    Damage_Apply(DAMAGE_KIND_PLAYER, playerid, issuerid, amount, weaponid, bodypart);
    return 0;
}

/* Gunfire is the loudest thing in the world: it is what wakes the horde. */
public OnPlayerWeaponShot(playerid, weaponid, hittype, hitid, Float:fX, Float:fY, Float:fZ)
{
    #pragma unused hittype, hitid, fX, fY, fZ

    if (weaponid == 0) return 1;

    /* Cek rapid-fire DULU sebelum noise/wear (audit F-15): tembakan ilegal
       tidak boleh membangunkan horde atau mengurangi kondisi senjata. */
    if (!AC_OnWeaponShot(playerid, weaponid)) return 0;
    Noise_EmitFromPlayer(playerid, weaponid);
    Inventory_DamageWeapon(playerid, weaponid);
    return 1;
}

public OnPlayerText(playerid, text[])
{
    #pragma unused text

    if (Moderation_IsMuted(playerid))
    {
        new line[112];
        format(line, sizeof(line), "[MUTE] Kamu dibisukan, sisa %d menit.",
            (Moderation_MuteSecondsLeft(playerid) + 59) / 60);
        SendClientMessage(playerid, UI_ACCENT_WARN, line);
        return 0;
    }

    if (!Player_IsAlive(playerid))
    {
        SendClientMessage(playerid, UI_TEXT_MUTED, "[LAST STAND] Anda tidak bisa bicara dalam kondisi ini.");
        return 0;
    }
    return 1;
}

/* Heartbeat: position hash, zone tracking and engine-health reconciliation only.
   No gameplay rules run here, keeping the tick budget predictable. */
public Player_Tick()
{
    new perfStart = GetTickCount();
    Zombie_UpdateSyncRate();
    Lock_Tick();

    foreach (new playerid : PlayerPool)
    {
        if (!IsPlayerConnected(playerid)) continue;

        new Float:x, Float:y, Float:z;
        GetPlayerPos(playerid, x, y, z);

        Hash_PlayerUpdate(playerid, x, y);
        PlayerSurvival[playerid][psZone] = Zone_GetIdAt(x, y, z);

        if (PlayerSurvival[playerid][psInShelter]) ShelterDoor_Tick(playerid);
        Bag_SpeedTick(playerid);   // berat tas: jalur 500 ms, bukan OnPlayerUpdate
        Player_ReviveChannelTick(playerid);

        new Float:engineHealth;
        GetPlayerHealth(playerid, engineHealth);

        if (GetTickCount() - PlayerSurvival[playerid][psLastDamageTick] > 600)
        {
            if (engineHealth < PlayerSurvival[playerid][psHealth] - 1.0)
            {
                PlayerSurvival[playerid][psHealth] = engineHealth;
            }
        }

        if (Player_IsDowned(playerid)
            && GetTickCount() - PlayerSurvival[playerid][psDownedUntil] > 0)  /* wrap-safe */
        {
            PlayerSurvival[playerid][psDowned] = 0;
            /* Timeout downed = mati: statistik kematian harus counting (audit D8). */
            PlayerSurvival[playerid][psDeaths]++;
            Grave_CreateFromPlayer(playerid);
            Inventory_Clear(playerid);
            ForceClassSelection(playerid);
            SetPlayerHealth(playerid, 0.0);
        }
    }

    Perf_Update(PERF_PLAYER, perfStart);
    return 1;
}

/* FCNPC hooks must live in the entry file: the plugin include rewrites these
   callback names with macros, and that rewriting only applies to the main source. */
public FCNPC_OnTakeDamage(npcid, issuerid, Float:amount, weaponid, bodypart)
{
    return Zombie_HookTakeDamage(npcid, issuerid, amount, weaponid, bodypart);
}

public FCNPC_OnDeath(npcid, killerid, reason)
{
    #pragma unused killerid, reason
    if (Escort_OnNpcDeath(npcid)) return 1;
    if (Companion_OnNpcDeath(npcid)) return 1;
    return Zombie_HookDeath(npcid);
}

public FCNPC_OnReachDestination(npcid)
{
    return Zombie_HookReachDestination(npcid);
}

public OnPlayerCommandText(playerid, cmdtext[])
{
    return Command_Process(playerid, cmdtext) ? 1 : 0;
}

