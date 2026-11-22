#if defined _ls_player_state_included
    #endinput
#endif
#define _ls_player_state_included

#define MAX_WOUND_STACKS (3)

/* Forward paling awal untuk fungsi item yang dipakai lintas modul (state.inc
   sendiri memakai Grant untuk refund revive). Definisi di invpers.inc. */
forward bool:Inventory_Grant(playerid, itemid, amount, const reason[], durability = WEAPON_CONDITION_MAX);
forward bool:Inventory_Consume(playerid, itemid, amount, const reason[]);
/* Dipakai bag/hotbar/container_lock (include sebelum account_core/selftest):
   forward di sini menghilangkan warning 208 reparse. */
forward bool:Account_IsPlaying(playerid);
forward Selftest_Record(bool:passed, const name[], const detail[]);
/* Dipakai craft_ui (include sebelum base_build). */
forward bool:BaseBuild_HasNearby(playerid, type, Float:range);

enum E_PLAYER_SURVIVAL
{
    Float:psHealth,
    Float:psArmour,
    Float:psInfection,          // 0.0 .. 100.0
    psInfectionStage,
    Float:psHunger,             // 100 = kenyang, 0 = kelaparan
    Float:psThirst,             // 100 = terhidrasi, 0 = dehidrasi
    Float:psBladder,            // 0 = kosong, 100 = harus kencing
    psWounds,                   // 0..3 luka terbuka, perdarahan tiap tick
    psLastNeedWarn,           // 0 aman, 1 hallucination, 2 kritis, 3 turned
    psZone,
    psDowned,                   // 1 = menunggu revive
    psDownedUntil,
    psCash,
    psScrap,
    psLevel,
    psXp,
    psLastNoiseTick,
    psLastDamageTick,
    psLastAttacker,
    psGraceUntil,               // proteksi pemain baru
    psDeaths,
    psExtractions,
    psInShelter,                // 1 = di dalam shelter pribadi (virtual world terpisah)
    Float:psShelterX,
    Float:psShelterY,
    Float:psShelterZ,
    Float:psShelterA,
    psStreakCount,              // kill streak 60 detik (tidak persisten)
    psStreakTick
};

new PlayerSurvival[MAX_PLAYERS][E_PLAYER_SURVIVAL];

/* Leaderboard JAM & RANK sebelumnya mati: kolom playtime_sec/rank_points
   ada di DB tapi tak pernah ditulis (audit T8). Mirror memori + akumulasi
   di autosave + award dari aktivitas berisiko. */
new PlaytimeSec[MAX_PLAYERS];
new RankPoints[MAX_PLAYERS];

/* Channel revive (audit D2): menolong teman butuh 6 detik berdiri dekat,
   bisa diinterupsi damage/jarak — bukan instan. Bandage dikonsumsi di awal,
   dikembalikan bila channel batal. */
new ReviveTarget[MAX_PLAYERS];
new ReviveStartTick[MAX_PLAYERS];
new Float:ReviveStartHealth[MAX_PLAYERS];

/* Pity specimen (audit U1): dijamin jatuh setelah 5 kill tanpa specimen. */
new SpecimenPity[MAX_PLAYERS];
/* Diminishing XP tier-1 (audit D6): anti-farm kill zona awal tanpa taper. */
new TaperKills[MAX_PLAYERS];
new TaperStart[MAX_PLAYERS];

/* didefinisikan di player/party.inc (include lebih lambat). */
forward bool:Party_InParty(playerid);
new Iterator:PlayerPool<MAX_PLAYERS>;

stock Player_InitSurvival(playerid)
{
    PlaytimeSec[playerid] = 0;
    RankPoints[playerid] = 0;
    ReviveTarget[playerid] = INVALID_PLAYER_ID;
    ReviveStartTick[playerid] = 0;
    ReviveStartHealth[playerid] = 0.0;
    SpecimenPity[playerid] = 0;
    TaperKills[playerid] = 0;
    TaperStart[playerid] = 0;
    PlayerSurvival[playerid][psHealth] = FULL_HP;
    PlayerSurvival[playerid][psArmour] = 0.0;
    PlayerSurvival[playerid][psInfection] = 0.0;
    PlayerSurvival[playerid][psInfectionStage] = 0;
    PlayerSurvival[playerid][psHunger] = 100.0;
    PlayerSurvival[playerid][psThirst] = 100.0;
    PlayerSurvival[playerid][psBladder] = 0.0;
    PlayerSurvival[playerid][psWounds] = 0;
    PlayerSurvival[playerid][psLastNeedWarn] = 0;
    PlayerSurvival[playerid][psZone] = HUB_ZONE_ID - 1;
    PlayerSurvival[playerid][psDowned] = 0;
    PlayerSurvival[playerid][psDownedUntil] = 0;
    PlayerSurvival[playerid][psCash] = 500;
    PlayerSurvival[playerid][psScrap] = 0;
    PlayerSurvival[playerid][psLevel] = 1;
    PlayerSurvival[playerid][psXp] = 0;
    PlayerSurvival[playerid][psLastNoiseTick] = 0;
    PlayerSurvival[playerid][psLastDamageTick] = 0;
    PlayerSurvival[playerid][psLastAttacker] = INVALID_PLAYER_ID;
    PlayerSurvival[playerid][psGraceUntil] = GetTickCount() + PLAYER_GRACE_MS;
    PlayerSurvival[playerid][psDeaths] = 0;
    PlayerSurvival[playerid][psExtractions] = 0;
    PlayerSurvival[playerid][psInShelter] = 0;
    PlayerSurvival[playerid][psShelterX] = 0;
    PlayerSurvival[playerid][psShelterY] = 0.0;
    PlayerSurvival[playerid][psShelterZ] = 0.0;
    PlayerSurvival[playerid][psStreakCount] = 0;
    PlayerSurvival[playerid][psStreakTick] = 0;
    PlayerSurvival[playerid][psShelterA] = 0.0;
    return 1;
}

stock Float:Player_GetHealth(playerid)
{
    return PlayerSurvival[playerid][psHealth];
}

stock Float:Player_GetInfection(playerid)
{
    return PlayerSurvival[playerid][psInfection];
}

stock Player_GetZone(playerid)
{
    return PlayerSurvival[playerid][psZone];
}

stock bool:Player_IsDowned(playerid)
{
    return PlayerSurvival[playerid][psDowned] != 0;
}

stock bool:Player_IsTurned(playerid)
{
    return PlayerSurvival[playerid][psInfectionStage] == 3;
}

stock bool:Player_IsAlive(playerid)
{
    if (!IsPlayerConnected(playerid)) return false;
    if (Player_IsDowned(playerid)) return false;
    if (Player_IsTurned(playerid)) return false;
    return PlayerSurvival[playerid][psHealth] > 0.0;
}

stock bool:Player_HasGrace(playerid)
{
    return GetTickCount() < PlayerSurvival[playerid][psGraceUntil];
}

/* Server is the only authority on player health: never trust the client value. */
stock Player_ApplyDamage(playerid, Float:amount, attackerid = INVALID_PLAYER_ID, bool:applyInfection = false)
{
    if (!IsPlayerConnected(playerid)) return 0;
    if (amount <= 0.0) return 0;
    if (Player_IsDowned(playerid) || Player_IsTurned(playerid)) return 0;

    new Float:armour = PlayerSurvival[playerid][psArmour];
    if (armour > 0.0)
    {
        new Float:absorbed = amount * 0.6;
        if (absorbed > armour) absorbed = armour;
        armour -= absorbed;
        amount -= absorbed;
        PlayerSurvival[playerid][psArmour] = armour;
        SetPlayerArmour(playerid, armour);
    }

    PlayerSurvival[playerid][psHealth] -= amount;
    PlayerSurvival[playerid][psLastDamageTick] = GetTickCount();
    PlayerSurvival[playerid][psLastAttacker] = attackerid;

    if (applyInfection)
    {
        Player_AddInfection(playerid, 12.0 + float(random(13)));
    }

    if (PlayerSurvival[playerid][psHealth] <= 0.0)
    {
        PlayerSurvival[playerid][psHealth] = 0.0;
        SetPlayerHealth(playerid, 0.0);
        return 1;
    }

    SetPlayerHealth(playerid, PlayerSurvival[playerid][psHealth]);
    return 1;
}

stock Player_Heal(playerid, Float:amount)
{
    if (!IsPlayerConnected(playerid)) return 0;
    if (Player_IsDowned(playerid) || Player_IsTurned(playerid)) return 0;

    PlayerSurvival[playerid][psHealth] += amount;
    if (PlayerSurvival[playerid][psHealth] > FULL_HP) PlayerSurvival[playerid][psHealth] = FULL_HP;

    SetPlayerHealth(playerid, PlayerSurvival[playerid][psHealth]);
    return 1;
}

stock Player_AddInfection(playerid, Float:amount)
{
    if (!IsPlayerConnected(playerid)) return 0;
    if (Player_IsTurned(playerid)) return 0;

    PlayerSurvival[playerid][psInfection] += amount;
    if (PlayerSurvival[playerid][psInfection] > 100.0) PlayerSurvival[playerid][psInfection] = 100.0;

    new stage = 0;
    if (PlayerSurvival[playerid][psInfection] >= 100.0) stage = 3;
    else if (PlayerSurvival[playerid][psInfection] >= 80.0) stage = 2;
    else if (PlayerSurvival[playerid][psInfection] >= 40.0) stage = 1;

    PlayerSurvival[playerid][psInfectionStage] = stage;
    return stage;
}

stock Player_CureInfection(playerid, Float:amount)
{
    if (!IsPlayerConnected(playerid)) return 0;
    PlayerSurvival[playerid][psInfection] -= amount;
    if (PlayerSurvival[playerid][psInfection] < 0.0) PlayerSurvival[playerid][psInfection] = 0.0;

    /* Hitung ulang stage di sini, bukan lewat Player_AddInfection(0):
       AddInfection menolak pemain turned, jadi stage 3 tidak pernah turun
       kalau cure dipanggil saat infeksi >= 100. */
    new stage = 0;
    if (PlayerSurvival[playerid][psInfection] >= 100.0) stage = 3;
    else if (PlayerSurvival[playerid][psInfection] >= 80.0) stage = 2;
    else if (PlayerSurvival[playerid][psInfection] >= 40.0) stage = 1;
    PlayerSurvival[playerid][psInfectionStage] = stage;
    return 1;
}

stock Player_EnterDownedState(playerid)
{
    if (!IsPlayerConnected(playerid)) return 0;
    if (Player_IsDowned(playerid)) return 0;

    PlayerSurvival[playerid][psDowned] = 1;
    /* Solo bleed-out lebih cepat (25 dtk) daripada party (45 dtk): downed
       tanpa tim bukan jeda aman panjang (audit D2). */
    PlayerSurvival[playerid][psDownedUntil] = GetTickCount() + (Party_InParty(playerid) ? 45000 : 25000);
    PlayerSurvival[playerid][psHealth] = 0.0;

    SetPlayerHealth(playerid, 1.0);
    ApplyAnimation(playerid, "CRASH", "crsl_slow_fwd", 4.0, false, false, false, false, 0, 1);
    SendClientMessage(playerid, UI_ACCENT_DANGER,
        "[LAST STAND] Anda downed. Anda punya 45 detik untuk di-revive party.");
    return 1;
}

stock Player_Revive(playerid)
{
    if (!IsPlayerConnected(playerid)) return 0;
    if (!Player_IsDowned(playerid)) return 0;

    PlayerSurvival[playerid][psDowned] = 0;
    PlayerSurvival[playerid][psHealth] = 35.0;
    PlayerSurvival[playerid][psDownedUntil] = 0;

    /* Revive menstabilkan infeksi di bawah ambang DoT/turned: tanpa ini pemain
       stage 3 langsung turned lagi di Needs_Tick berikutnya dan spawn runner
       baru tiap revive (loop farm zombie). */
    if (PlayerSurvival[playerid][psInfection] >= 80.0)
    {
        Player_CureInfection(playerid, PlayerSurvival[playerid][psInfection] - 79.0);
    }

    SetPlayerHealth(playerid, 35.0);
    ClearAnimations(playerid);
    SendClientMessage(playerid, UI_ACCENT_PRIMARY, "[LAST STAND] Anda di-revive. Bergerak hati-hati.");
    return 1;
}

/* Channel revive: dipanggil tiap Player_Tick (500 ms). Batal bila penolong
   bergerak menjauh, terkena damage, downed/mati, atau target sudah tidak
   downed (ditolong orang lain / mati). Bandage dikembalikan saat batal. */
stock Player_ReviveCancel(playerid, const reason[])
{
    if (ReviveTarget[playerid] == INVALID_PLAYER_ID) return 0;
    ReviveTarget[playerid] = INVALID_PLAYER_ID;
    Inventory_Grant(playerid, ITEM_BANDAGE, 1, "revive_refund");
    new line[128];
    format(line, sizeof(line), "[REVIVE] Dibatalkan: %s. Bandage dikembalikan.", reason);
    SendClientMessage(playerid, UI_ACCENT_WARN, line);
    return 1;
}

stock Player_ReviveChannelTick(playerid)
{
    new target = ReviveTarget[playerid];
    if (target == INVALID_PLAYER_ID) return 0;

    if (!IsPlayerConnected(target) || !Player_IsDowned(target))
        return Player_ReviveCancel(playerid, "target sudah tidak downed");
    if (!Account_IsPlaying(playerid) || Player_IsDowned(playerid) || !Player_IsAlive(playerid))
        return Player_ReviveCancel(playerid, "kamu tidak bisa menolong sekarang");
    if (Lobby_Of(target) != Lobby_Of(playerid))
        return Player_ReviveCancel(playerid, "target pindah dunia");
    if (PlayerSurvival[playerid][psHealth] < ReviveStartHealth[playerid] - 0.5)
        return Player_ReviveCancel(playerid, "kamu terkena damage");

    new Float:px, Float:py, Float:pz, Float:tx, Float:ty, Float:tz;
    GetPlayerPos(playerid, px, py, pz);
    GetPlayerPos(target, tx, ty, tz);
    if (VectorSize(tx - px, ty - py, tz - pz) > 3.5)
        return Player_ReviveCancel(playerid, "kamu bergerak menjauh");

    if (GetTickCount() - ReviveStartTick[playerid] >= REVIVE_CHANNEL_MS)
    {
        ReviveTarget[playerid] = INVALID_PLAYER_ID;
        Player_Revive(target);
        Skills_AddXp(playerid, 15);

        new helperName[MAX_PLAYER_NAME + 1], line[128];
        GetPlayerName(playerid, helperName, sizeof(helperName));
        format(line, sizeof(line), "[REVIVE] %s menolongmu bangkit. Bandage terpakai.", helperName);
        SendClientMessage(target, UI_ACCENT_PRIMARY, line);
        SendClientMessage(playerid, UI_ACCENT_PRIMARY, "[REVIVE] Temanmu selamat. +15 XP.");
        Toast_Show(playerid, "REVIVE", "Temanmu bangkit. +15 XP.", UI_ACCENT_PRIMARY);
    }
    return 1;
}

/* Hub dan shelter sama-sama zona aman: tidak ada zombie, tidak ada PvP,
   dan stash/craft/pedagang boleh dipakai. */
stock bool:Player_IsSafe(playerid)
{
    if (!IsPlayerConnected(playerid)) return false;
    return (Zone_IsHub(Player_GetZone(playerid)) || PlayerSurvival[playerid][psInShelter]) ? true : false;
}
