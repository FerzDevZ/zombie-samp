#if defined _ls_wounds_included
    #endinput
#endif
#define _ls_wounds_included

/* Luka terbuka adalah resource kedua setelah darah: setiap tumpukan berdarah
   tiap tick kebutuhan (5 detik), tidak hilang sendiri, dan hanya bisa ditutup
   dengan bandage (1) atau medkit (semua). Disimpan ke DB supaya logout bukan
   cara gratis menghapus luka. */

#define WOUND_DAMAGE_PER_STACK      (0.9)
#define WOUND_WARN_MASK             (8)

stock Wounds_Get(playerid)
{
    if (!IsPlayerConnected(playerid)) return 0;
    return PlayerSurvival[playerid][psWounds];
}

stock Wounds_Add(playerid, amount = 1)
{
    if (!IsPlayerConnected(playerid) || amount <= 0) return 0;
    if (Player_IsDowned(playerid) || Player_IsTurned(playerid)) return 0;

    new before = PlayerSurvival[playerid][psWounds];
    PlayerSurvival[playerid][psWounds] += amount;
    if (PlayerSurvival[playerid][psWounds] > MAX_WOUND_STACKS)
    {
        PlayerSurvival[playerid][psWounds] = MAX_WOUND_STACKS;
    }
    if (PlayerSurvival[playerid][psWounds] == before) return 0;

    if (PlayerSurvival[playerid][psWounds] >= MAX_WOUND_STACKS)
    {
        SendClientMessage(playerid, UI_ACCENT_DANGER,
            "[LUKA] Tiga luka terbuka. Perdarahan berat, pakai bandage sekarang.");
    }
    else
    {
        SendClientMessage(playerid, UI_ACCENT_WARN,
            "[LUKA] Luka terbuka. Darah menetes, pakai bandage.");
    }
    return 1;
}

stock Wounds_Remove(playerid, amount = 1)
{
    if (!IsPlayerConnected(playerid) || amount <= 0) return 0;

    new before = PlayerSurvival[playerid][psWounds];
    PlayerSurvival[playerid][psWounds] -= amount;
    if (PlayerSurvival[playerid][psWounds] < 0) PlayerSurvival[playerid][psWounds] = 0;

    if (PlayerSurvival[playerid][psWounds] == before) return 0;

    if (PlayerSurvival[playerid][psWounds] == 0)
    {
        SendClientMessage(playerid, UI_ACCENT_PRIMARY, "[LUKA] Semua luka tertutup. Perdarahan berhenti.");
    }
    return 1;
}

stock Wounds_Clear(playerid)
{
    if (!IsPlayerConnected(playerid)) return 0;
    PlayerSurvival[playerid][psWounds] = 0;
    return 1;
}

stock Wounds_Tick(playerid)
{
    if (!IsPlayerConnected(playerid)) return 0;
    if (Player_IsDowned(playerid) || Player_IsTurned(playerid)) return 0;

    new stacks = PlayerSurvival[playerid][psWounds];
    if (stacks <= 0) return 0;

    Damage_Apply(DAMAGE_KIND_PLAYER, playerid, INVALID_PLAYER_ID,
        WOUND_DAMAGE_PER_STACK * float(stacks), 0, 0);

    if (stacks >= MAX_WOUND_STACKS)
    {
        Needs_Warn(playerid, WOUND_WARN_MASK,
            "[LUKA] Perdarahan berat. Pakai bandage atau medkit.", UI_ACCENT_DANGER);
    }
    return 1;
}
