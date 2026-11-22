#if defined _ls_needs_included
    #endinput
#endif
#define _ls_needs_included

#define NEEDS_TICK_MS 5000
#define INFECTION_DAMAGE_PER_TICK 4.0

/* Laju per tick 5 detik. 100 poin = sekitar 83 menit (lapar), 60 menit (haus),
   69 menit (kandung kemih) supaya satu sesi main butuh bekal, bukan menit pertama. */
#define NEEDS_HUNGER_DECAY   (0.10)
#define NEEDS_THIRST_DECAY   (0.14)
#define NEEDS_BLADDER_GAIN   (0.12)

#define NEEDS_STARVE_DAMAGE  (0.8)
#define NEEDS_THIRST_DAMAGE  (1.2)
#define NEEDS_BLADDER_DAMAGE (0.6)
#define NEEDS_WARN_MS        (45000)

new NeedWarnMask[MAX_PLAYERS];

/* Clamp in-place by reference: return value lama (0/100/1) pernah membuat
   pemanggil yang mengabaikan return tidak meng-clamp sama sekali. */
stock Needs_Clamp(&Float:value)
{
    if (value < 0.0) value = 0.0;
    else if (value > 100.0) value = 100.0;
    return 1;
}

stock Float:Needs_GetHunger(playerid)
{
    return PlayerSurvival[playerid][psHunger];
}

stock Float:Needs_GetThirst(playerid)
{
    return PlayerSurvival[playerid][psThirst];
}

stock Float:Needs_GetBladder(playerid)
{
    return PlayerSurvival[playerid][psBladder];
}

stock Needs_ResetPlayer(playerid)
{
    PlayerSurvival[playerid][psHunger] = 100.0;
    PlayerSurvival[playerid][psThirst] = 100.0;
    PlayerSurvival[playerid][psBladder] = 0.0;
    PlayerSurvival[playerid][psLastNeedWarn] = 0;
    NeedWarnMask[playerid] = 0;
    return 1;
}

stock Needs_Warn(playerid, mask, const message[], color)
{
    if (NeedWarnMask[playerid] & mask) return 0;
    if (GetTickCount() - PlayerSurvival[playerid][psLastNeedWarn] < NEEDS_WARN_MS) return 0;

    NeedWarnMask[playerid] |= mask;
    PlayerSurvival[playerid][psLastNeedWarn] = GetTickCount();
    SendClientMessage(playerid, color, message);
    return 1;
}

/* Dipakai inventory: makanan mengisi perut, air mengisi haus.
   Bladder naik wajar karena minum; tidak ada item yang langsung mengosongkannya. */
stock bool:Needs_ConsumeItem(playerid, itemid)
{
    if (!IsPlayerConnected(playerid)) return false;

    switch (itemid)
    {
        case ITEM_FOOD_CAN:
        {
            PlayerSurvival[playerid][psHunger] += 30.0;
            PlayerSurvival[playerid][psThirst] += 3.0;
            Needs_Clamp(PlayerSurvival[playerid][psHunger]);
            Needs_Clamp(PlayerSurvival[playerid][psThirst]);
            SendClientMessage(playerid, UI_ACCENT_PRIMARY, "[TUBUH] Kenyang. Haus sedikit bertambah.");
            return true;
        }
        case ITEM_WATER_BOTTLE:
        {
            PlayerSurvival[playerid][psThirst] += 42.0;
            PlayerSurvival[playerid][psBladder] += 12.0;
            Needs_Clamp(PlayerSurvival[playerid][psThirst]);
            Needs_Clamp(PlayerSurvival[playerid][psBladder]);
            SendClientMessage(playerid, UI_ACCENT_PRIMARY, "[TUBUH] Segar. Kandung kemih terisi.");
            return true;
        }
    }
    return false;
}

stock bool:Needs_Urinate(playerid)
{
    if (!IsPlayerConnected(playerid)) return false;
    if (Player_IsDowned(playerid) || Player_IsTurned(playerid)) return false;
    if (PlayerSurvival[playerid][psBladder] < 12.0)
    {
        SendClientMessage(playerid, UI_TEXT_MUTED, "[TUBUH] Belum perlu kencing.");
        return false;
    }

    new Float:relief = PlayerSurvival[playerid][psBladder];
    PlayerSurvival[playerid][psBladder] = 0.0;

    new message[96];
    format(message, sizeof(message), "[TUBUH] Lega. Kandung kemih turun %.0f%%.", relief);
    SendClientMessage(playerid, UI_ACCENT_PRIMARY, message);
    Toast_Show(playerid, "LEGA", "Tekanan kandung kemih hilang.", UI_ACCENT_INFO);

    Log_Format(LOG_INFO, "[NEEDS] char=%d kencing %.0f", ActiveCharId[playerid], relief);
    return true;
}

public Needs_Tick()
{
    new perfStart = GetTickCount();
    foreach (new playerid : PlayerPool)
    {
        if (!IsPlayerConnected(playerid)) continue;

        if (Player_HasGrace(playerid)) continue;

        Wounds_Tick(playerid);

        new Float:infection = Player_GetInfection(playerid);
        if (infection > 0.0)
        {
            if (PlayerSurvival[playerid][psInfectionStage] == 2)
            {
                Damage_Apply(DAMAGE_KIND_PLAYER, playerid, INVALID_PLAYER_ID,
                    INFECTION_DAMAGE_PER_TICK, 0, 0);
            }

            if (PlayerSurvival[playerid][psInfectionStage] == 3 && !Player_IsDowned(playerid))
            {
                new Float:x, Float:y, Float:z;
                GetPlayerPos(playerid, x, y, z);

                Player_EnterDownedState(playerid);
                PlayerSurvival[playerid][psInfectionStage] = 3;

                /* Turned di dalam shelter tidak boleh melahirkan runner di VW
                   lobby pada koordinat langit shelter (NPC jatuh — audit #18):
                   spawn di kamp, di dunia utama. */
                new Float:turnX = x + 1.2, Float:turnY = y, Float:turnZ = z;
                if (PlayerSurvival[playerid][psInShelter])
                {
                    turnX = SPAWN_X;
                    turnY = SPAWN_Y;
                    turnZ = SPAWN_Z;
                }
                Zombie_Create(ZT_RUNNER, turnX, turnY, turnZ, Lobby_Of(playerid));
                Toast_Show(playerid, "TURNED", "Infeksi menyelesaikan tugasnya. Anda menjadi bagian dari horde.", UI_ACCENT_INFECTION);
            }
        }

        /* Beban berat mempercepat haus & lapar: alasan gameplay untuk naik tas,
           bukan cuma angka di dialog. */
        new Float:drainScale = Bag_IsOverloaded(playerid) ? 1.35 : 1.0;

        PlayerSurvival[playerid][psHunger] -= NEEDS_HUNGER_DECAY * drainScale;
        PlayerSurvival[playerid][psThirst] -= NEEDS_THIRST_DECAY * drainScale;
        PlayerSurvival[playerid][psBladder] += NEEDS_BLADDER_GAIN;
        Needs_Clamp(PlayerSurvival[playerid][psHunger]);
        Needs_Clamp(PlayerSurvival[playerid][psThirst]);
        Needs_Clamp(PlayerSurvival[playerid][psBladder]);

        if (Player_IsDowned(playerid)) continue;
        if (Player_IsTurned(playerid)) continue;

        if (PlayerSurvival[playerid][psHunger] <= 0.0)
        {
            Damage_Apply(DAMAGE_KIND_PLAYER, playerid, INVALID_PLAYER_ID, NEEDS_STARVE_DAMAGE, 0, 0);
            Needs_Warn(playerid, 1, "[TUBUH] Kelaparan. Cari makanan sekarang.", UI_ACCENT_DANGER);
        }
        else if (PlayerSurvival[playerid][psHunger] <= 20.0)
        {
            Needs_Warn(playerid, 1, "[TUBUH] Lapar. Makan sebelum tenaga habis.", UI_ACCENT_WARN);
        }
        else
        {
            NeedWarnMask[playerid] &= ~1;
        }

        if (PlayerSurvival[playerid][psThirst] <= 0.0)
        {
            Damage_Apply(DAMAGE_KIND_PLAYER, playerid, INVALID_PLAYER_ID, NEEDS_THIRST_DAMAGE, 0, 0);
            Needs_Warn(playerid, 2, "[TUBUH] Dehidrasi. Minum sekarang.", UI_ACCENT_DANGER);
        }
        else if (PlayerSurvival[playerid][psThirst] <= 25.0)
        {
            Needs_Warn(playerid, 2, "[TUBUH] Haus. Air minum menipis.", UI_ACCENT_WARN);
        }
        else
        {
            NeedWarnMask[playerid] &= ~2;
        }

        if (PlayerSurvival[playerid][psBladder] >= 100.0)
        {
            Damage_Apply(DAMAGE_KIND_PLAYER, playerid, INVALID_PLAYER_ID, NEEDS_BLADDER_DAMAGE, 0, 0);
            Needs_Warn(playerid, 4, "[TUBUH] Kandung kemih penuh. Ketik /kencing.", UI_ACCENT_DANGER);
        }
        else if (PlayerSurvival[playerid][psBladder] >= 80.0)
        {
            Needs_Warn(playerid, 4, "[TUBUH] Ingin kencing. Cari tempat, /kencing.", UI_ACCENT_WARN);
        }
        else
        {
            NeedWarnMask[playerid] &= ~4;
        }
    }
    Perf_Update(PERF_NEEDS, perfStart);
    return 1;
}

stock Needs_ResetForPlayer(playerid)
{
    if (!IsPlayerConnected(playerid)) return 0;
    PlayerSurvival[playerid][psInfection] = 0.0;
    PlayerSurvival[playerid][psInfectionStage] = 0;
    return 1;
}
