#if defined _ls_tutorial_included
    #endinput
#endif
#define _ls_tutorial_included

/* Onboarding quest 5 langkah, persisten per karakter.
   Urutan sengaja mengikuti loop inti server: bunuh -> loot -> craft -> sampel -> ekstraksi.
   Setiap langkah selesai langsung di-UPDATE ke MySQL supaya reconnect tidak mengulang
   misi, dan reward diberikan tepat satu kali lewat ledger Inventory_Grant. */

#define TUT_STEP_NONE      0
#define TUT_STEP_KILL      1
#define TUT_STEP_LOOT      2
#define TUT_STEP_CRAFT     3
#define TUT_STEP_SAMPLE    4
#define TUT_STEP_EXTRACT   5
#define TUT_STEP_DONE      6

#define TUT_KILL_TARGET    3
#define TUT_LOOT_TARGET    1
#define TUT_CRAFT_TARGET   1
#define TUT_SAMPLE_TARGET  3
#define TUT_EXTRACT_TARGET 1

#define TUT_FINAL_CASH     300
#define TUT_FINAL_SCRAP    5

new TutorialStep[MAX_PLAYERS];
new TutorialCount[MAX_PLAYERS];

stock Tutorial_Reset(playerid)
{
    TutorialStep[playerid] = TUT_STEP_NONE;
    TutorialCount[playerid] = 0;
    return 1;
}

stock Tutorial_LoadFromDB(playerid, step, count)
{
    if (step < TUT_STEP_NONE || step > TUT_STEP_DONE) step = TUT_STEP_NONE;
    if (count < 0) count = 0;
    TutorialStep[playerid] = step;
    TutorialCount[playerid] = count;
    return 1;
}

stock Tutorial_Save(playerid)
{
    if (ActiveCharId[playerid] == 0) return 0;

    new query[160];
    mysql_format(gSQL, query, sizeof(query),
        "UPDATE characters SET tutorial_step = %d, tutorial_count = %d WHERE id = %d",
        TutorialStep[playerid], TutorialCount[playerid], ActiveCharId[playerid]);
    mysql_tquery(gSQL, query);
    return 1;
}

stock Tutorial_TargetForStep(step)
{
    switch (step)
    {
        case TUT_STEP_KILL:    return TUT_KILL_TARGET;
        case TUT_STEP_LOOT:    return TUT_LOOT_TARGET;
        case TUT_STEP_CRAFT:   return TUT_CRAFT_TARGET;
        case TUT_STEP_SAMPLE:  return TUT_SAMPLE_TARGET;
        case TUT_STEP_EXTRACT: return TUT_EXTRACT_TARGET;
    }
    return 0;
}

stock Tutorial_Label(step, output[], outputSize)
{
    switch (step)
    {
        case TUT_STEP_KILL:    format(output, outputSize, "Bunuh 3 zombie di luar kamp");
        case TUT_STEP_LOOT:    format(output, outputSize, "Ambil 1 loot dari peti atau mayat");
        case TUT_STEP_CRAFT:   format(output, outputSize, "Craft 1 item apa saja lewat /craft");
        case TUT_STEP_SAMPLE:  format(output, outputSize, "Kumpulkan 3 Zombie Specimen");
        case TUT_STEP_EXTRACT: format(output, outputSize, "Ekstraksi di titik heli lewat /ekstrak");
        default:               format(output, outputSize, "Tidak ada misi aktif");
    }
    return 1;
}

/* Reward diberikan lewat Inventory_Grant supaya masuk item_ledger seperti item lain.
   Return false = tas penuh: langkah TIDAK boleh maju sampai reward masuk. */
stock bool:Tutorial_GrantReward(playerid, step)
{
    switch (step)
    {
        case TUT_STEP_KILL:
        {
            if (!Inventory_Grant(playerid, ITEM_BANDAGE, 2, "tutorial")) return false;
            Toast_Show(playerid, "Misi selesai", "Reward: Bandage x2", UI_ACCENT_PRIMARY);
        }
        case TUT_STEP_LOOT:
        {
            if (!Inventory_Grant(playerid, ITEM_SCRAP, 3, "tutorial")) return false;
            Toast_Show(playerid, "Misi selesai", "Reward: Scrap x3", UI_ACCENT_PRIMARY);
        }
        case TUT_STEP_CRAFT:
        {
            if (!Inventory_Grant(playerid, ITEM_MEDKIT, 1, "tutorial")) return false;
            Toast_Show(playerid, "Misi selesai", "Reward: Medkit x1", UI_ACCENT_PRIMARY);
        }
        case TUT_STEP_SAMPLE:
        {
            if (!Inventory_Grant(playerid, ITEM_ANTIBIOTICS, 2, "tutorial")) return false;
            Toast_Show(playerid, "Misi selesai", "Reward: Antibiotics x2", UI_ACCENT_PRIMARY);
        }
        case TUT_STEP_EXTRACT:
        {
            if (!Inventory_Grant(playerid, ITEM_SCRAP, TUT_FINAL_SCRAP, "tutorial_scrap")) return false;
            PlayerSurvival[playerid][psCash] += TUT_FINAL_CASH;
            Toast_Show(playerid, "ONBOARDING SELESAI", "Reward: $300 + Scrap x5", UI_ACCENT_PRIMARY);
        }
    }
    return true;
}

/* Satu jalur untuk semua event: mulai misi kalau belum jalan, hitung, dan naik langkah. */
stock Tutorial_Bump(playerid, step, amount)
{
    if (!Account_IsPlaying(playerid)) return 0;
    if (amount <= 0) return 0;
    if (TutorialStep[playerid] == TUT_STEP_DONE) return 0;

    if (TutorialStep[playerid] == TUT_STEP_NONE) TutorialStep[playerid] = step;
    if (TutorialStep[playerid] != step) return 0;

    TutorialCount[playerid] += amount;

    new target = Tutorial_TargetForStep(step);
    if (TutorialCount[playerid] < target)
    {
        Tutorial_Save(playerid);
        return 0;
    }

    if (!Tutorial_GrantReward(playerid, step))
    {
        /* Reward gagal (tas penuh): tahan langkah di target, coba lagi pada
           aksi berikutnya. Jangan pernah memajukan misi tanpa memberi reward. */
        TutorialCount[playerid] = target;
        Tutorial_Save(playerid);
        SendClientMessage(playerid, UI_ACCENT_WARN, "[MISI] Tas penuh. Kosongkan slot; reward diberikan pada aksi berikutnya.");
        return 0;
    }
    TutorialCount[playerid] = 0;
    TutorialStep[playerid]++;

    /* Langkah SAMPLE: specimen yang SUDAH di tas ikut dihitung (audit U1) —
       pemain tidak perlu memungut ulang sampel yang sudah dibawa. */
    if (TutorialStep[playerid] == TUT_STEP_SAMPLE)
    {
        new carried = Inventory_CountItem(playerid, ITEM_SPECIMEN);
        TutorialCount[playerid] = (carried < TUT_SAMPLE_TARGET) ? carried : TUT_SAMPLE_TARGET;
    }

    new label[64], line[112];
    if (TutorialStep[playerid] == TUT_STEP_DONE)
    {
        format(line, sizeof(line), "[MISI] Semua misi onboarding selesai. Selamat bertahan, survivor.");
        SendClientMessage(playerid, UI_ACCENT_PRIMARY, line);
    }
    else
    {
        Tutorial_Label(TutorialStep[playerid], label, sizeof(label));
        format(line, sizeof(line), "[MISI] Langkah baru: %s. Cek progres dengan /misi.", label);
        SendClientMessage(playerid, UI_ACCENT_PRIMARY, line);
    }

    Tutorial_Save(playerid);
    return 1;
}

stock Tutorial_OnZombieKill(playerid)
{
    return Tutorial_Bump(playerid, TUT_STEP_KILL, 1);
}

stock Tutorial_OnLootTaken(playerid, itemid, amount)
{
    if (itemid == ITEM_SPECIMEN) return Tutorial_Bump(playerid, TUT_STEP_SAMPLE, amount);
    return Tutorial_Bump(playerid, TUT_STEP_LOOT, 1);
}

stock Tutorial_OnCraft(playerid)
{
    return Tutorial_Bump(playerid, TUT_STEP_CRAFT, 1);
}

stock Tutorial_OnExtracted(playerid)
{
    return Tutorial_Bump(playerid, TUT_STEP_EXTRACT, 1);
}

stock Tutorial_ShowStatus(playerid)
{
    new step = TutorialStep[playerid];
    new line[128];

    if (step == TUT_STEP_NONE)
    {
        SendClientMessage(playerid, UI_ACCENT_INFO, "[MISI] Kamu belum punya misi. Bunuh zombie, loot, atau craft untuk memulai.");
        return 1;
    }
    if (step == TUT_STEP_DONE)
    {
        SendClientMessage(playerid, UI_ACCENT_PRIMARY, "[MISI] Onboarding selesai. Misi lanjutan: /kontrak dan /escort.");
        return 1;
    }

    new label[64];
    Tutorial_Label(step, label, sizeof(label));
    format(line, sizeof(line), "[MISI] %s (%d/%d)", label, TutorialCount[playerid], Tutorial_TargetForStep(step));
    SendClientMessage(playerid, UI_ACCENT_INFO, line);
    return 1;
}

/* Ditampilkan sekali saat karakter dimuat dan belum punya misi. */
stock Tutorial_ShowIntro(playerid)
{
    if (TutorialStep[playerid] != TUT_STEP_NONE) return 0;

    SendClientMessage(playerid, UI_ACCENT_INFO, "[MISI] Kamu baru di zona karantina. Selesaikan 5 misi onboarding dengan /misi.");
    Toast_Show(playerid, "MISI ONBOARDING", "Ketik /misi untuk melihat target pertama.", UI_ACCENT_PRIMARY);
    return 1;
}
