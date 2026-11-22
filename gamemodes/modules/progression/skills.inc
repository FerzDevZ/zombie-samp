#if defined _ls_skills_included
    #endinput
#endif
#define _ls_skills_included

/* Skill tree: 5 cabang, maks 3 level per cabang. Poin hanya datang dari level up,
   dan setiap efek di sini dipakai modul lain lewat multiplier - tidak ada jalur
   kedua yang mengubah damage/heal/noise secara langsung. */

#define MAX_SKILLS        5
#define SKILL_MAX_LEVEL   3
#define SKILL_XP_BASE     120
#define DIALOG_SKILL      2501

enum E_SKILL
{
    SKILL_MARKSMAN = 0,   // +5% damage ke zombie per level
    SKILL_TOUGHNESS,      // -5% damage diterima per level
    SKILL_SCAVENGER,      // +1 material ekstra saat loot, per level (dicek roll)
    SKILL_MEDIC,          // +25% efek heal per level
    SKILL_STEALTH         // -15% radius noise per level
};

forward Skills_OnPointsLoaded(playerid);
forward Skills_OnRowsLoaded(playerid);

new SkillLevel[MAX_PLAYERS][MAX_SKILLS];
new SkillPoints[MAX_PLAYERS];

new const gSkillName[MAX_SKILLS][] = {
    "Marksman", "Toughness", "Scavenger", "Medic", "Stealth"
};

new const gSkillEffect[MAX_SKILLS][] = {
    "+5% damage per level",
    "-5% damage masuk per level",
    "+1 material loot per level",
    "+25% efek heal per level",
    "-15% radius noise per level"
};

stock Skills_Reset(playerid)
{
    for (new skill = 0; skill < MAX_SKILLS; skill++) SkillLevel[playerid][skill] = 0;
    SkillPoints[playerid] = 0;
    return 1;
}

stock Skills_Load(playerid)
{
    Skills_Reset(playerid);
    if (!SQL_IsReady() || ActiveCharId[playerid] == 0) return 0;

    new query[160];
    mysql_format(gSQL, query, sizeof(query),
        "SELECT skill_points FROM characters WHERE id = %d LIMIT 1", ActiveCharId[playerid]);
    mysql_tquery(gSQL, query, "Skills_OnPointsLoaded", "i", playerid);

    mysql_format(gSQL, query, sizeof(query),
        "SELECT skill_id, level FROM character_skills WHERE char_id = %d", ActiveCharId[playerid]);
    mysql_tquery(gSQL, query, "Skills_OnRowsLoaded", "i", playerid);
    return 1;
}

public Skills_OnPointsLoaded(playerid)
{
    if (!IsPlayerConnected(playerid) || cache_num_rows() == 0) return 1;

    new points = 0;
    cache_get_value_name_int(0, "skill_points", points);
    SkillPoints[playerid] = points;
    return 1;
}

public Skills_OnRowsLoaded(playerid)
{
    if (!IsPlayerConnected(playerid)) return 1;

    new rows = cache_num_rows();
    for (new row = 0; row < rows; row++)
    {
        new skillId = 0, level = 0;
        cache_get_value_name_int(row, "skill_id", skillId);
        cache_get_value_name_int(row, "level", level);

        if (skillId < 0 || skillId >= MAX_SKILLS) continue;
        if (level < 0) level = 0;
        if (level > SKILL_MAX_LEVEL) level = SKILL_MAX_LEVEL;
        SkillLevel[playerid][skillId] = level;
    }
    return 1;
}

stock Skills_Save(playerid)
{
    if (!SQL_IsReady() || ActiveCharId[playerid] == 0) return 0;

    new query[384];
    mysql_format(gSQL, query, sizeof(query),
        "UPDATE characters SET skill_points = %d WHERE id = %d",
        SkillPoints[playerid], ActiveCharId[playerid]);
    mysql_tquery(gSQL, query, "", "");

    mysql_format(gSQL, query, sizeof(query),
        "REPLACE INTO character_skills (char_id, skill_id, level) VALUES (%d, 0, %d), (%d, 1, %d), (%d, 2, %d), (%d, 3, %d), (%d, 4, %d)",
        ActiveCharId[playerid], SkillLevel[playerid][0],
        ActiveCharId[playerid], SkillLevel[playerid][1],
        ActiveCharId[playerid], SkillLevel[playerid][2],
        ActiveCharId[playerid], SkillLevel[playerid][3],
        ActiveCharId[playerid], SkillLevel[playerid][4]);
    mysql_tquery(gSQL, query, "", "");
    return 1;
}

/* XP masuk hanya lewat sini: naik level, dapat 1 poin, dan langsung disimpan. */
stock Skills_AddXp(playerid, amount)
{
    if (amount <= 0) return 0;

    PlayerSurvival[playerid][psXp] += amount;

    new leveled = 0;
    /* Cap level 50 (DESIGN §10, audit D6): tanpa cap, level & poin skill
       tumbuh tanpa batas dan kehilangan arti. */
    while (PlayerSurvival[playerid][psLevel] < 50
        && PlayerSurvival[playerid][psXp] >= SKILL_XP_BASE * PlayerSurvival[playerid][psLevel])
    {
        PlayerSurvival[playerid][psXp] -= SKILL_XP_BASE * PlayerSurvival[playerid][psLevel];
        PlayerSurvival[playerid][psLevel]++;
        SkillPoints[playerid]++;
        leveled++;
        if (leveled > 8) break;   // guard: satu kill tidak boleh melompati banyak level
    }

    if (leveled > 0)
    {
        new message[128];
        format(message, sizeof(message), "[SKILL] Naik ke level %d. Poin skill +%d (total %d). Ketik /skill.",
            PlayerSurvival[playerid][psLevel], leveled, SkillPoints[playerid]);
        SendClientMessage(playerid, UI_ACCENT_PRIMARY, message);
        Toast_Show(playerid, "LEVEL UP", message, UI_ACCENT_PRIMARY);
        Skills_Save(playerid);
    }

    Party_ShareXp(playerid, amount);
    return 1;
}

stock Skills_SellBonus(playerid)
{
    return SkillLevel[playerid][_:SKILL_SCAVENGER] * 5;   // +5% harga jual per level
}

stock Float:Skills_DamageDealtMultiplier(playerid)
{
    return 1.0 + (0.05 * SkillLevel[playerid][_:SKILL_MARKSMAN]);
}

stock Float:Skills_DamageTakenMultiplier(playerid)
{
    return 1.0 - (0.05 * SkillLevel[playerid][_:SKILL_TOUGHNESS]);
}

stock Float:Skills_HealMultiplier(playerid)
{
    return 1.0 + (0.25 * SkillLevel[playerid][_:SKILL_MEDIC]);
}

stock Float:Skills_NoiseMultiplier(playerid)
{
    return 1.0 - (0.15 * SkillLevel[playerid][_:SKILL_STEALTH]);
}

stock Skills_LootBonus(playerid)
{
    return SkillLevel[playerid][_:SKILL_SCAVENGER];
}

stock Skills_Spend(playerid, skill)
{
    if (skill < 0 || skill >= MAX_SKILLS) return 0;

    if (SkillPoints[playerid] <= 0)
    {
        SendClientMessage(playerid, UI_ACCENT_WARN, "[SKILL] Tidak ada poin. Naik level dulu.");
        return 0;
    }
    if (SkillLevel[playerid][skill] >= SKILL_MAX_LEVEL)
    {
        SendClientMessage(playerid, UI_ACCENT_WARN, "[SKILL] Cabang itu sudah maksimal (3).");
        return 0;
    }

    SkillLevel[playerid][skill]++;
    SkillPoints[playerid]--;
    Skills_Save(playerid);

    new message[144];
    format(message, sizeof(message), "[SKILL] %s -> level %d. Poin tersisa %d.",
        gSkillName[skill], SkillLevel[playerid][skill], SkillPoints[playerid]);
    SendClientMessage(playerid, UI_ACCENT_PRIMARY, message);
    return 1;
}

/* Respec: semua level dikembalikan jadi poin, biaya Scrap 3. Tanpa ini, salah pilih
   cabang di awal membuat karakter terasa rusak permanen. */
stock Skills_Respec(playerid)
{
    new refund = 0;
    for (new skill = 0; skill < MAX_SKILLS; skill++) refund += SkillLevel[playerid][skill];

    if (refund == 0)
    {
        SendClientMessage(playerid, UI_ACCENT_WARN, "[SKILL] Belum ada level untuk dikembalikan.");
        return 0;
    }

    if (Inventory_CountItem(playerid, ITEM_SCRAP) < 3)
    {
        SendClientMessage(playerid, UI_ACCENT_WARN, "[SKILL] Respec butuh Scrap 3.");
        return 0;
    }

    if (!Inventory_Consume(playerid, ITEM_SCRAP, 3, "respec")) return 0;

    for (new skill = 0; skill < MAX_SKILLS; skill++) SkillLevel[playerid][skill] = 0;
    SkillPoints[playerid] += refund;
    Skills_Save(playerid);

    new message[160];
    format(message, sizeof(message), "[SKILL] Respec selesai: %d poin dikembalikan, total poin %d. Pakai /skill.",
        refund, SkillPoints[playerid]);
    SendClientMessage(playerid, UI_ACCENT_PRIMARY, message);
    Toast_Show(playerid, "RESPEC", message, UI_ACCENT_PRIMARY);
    return 1;
}

stock Skills_Show(playerid)
{
    new body[768], line[144];
    new dots[8];

    body[0] = EOS;
    for (new skill = 0; skill < MAX_SKILLS; skill++)
    {
        for (new dot = 0; dot < SKILL_MAX_LEVEL; dot++)
        {
            dots[dot] = (dot < SkillLevel[playerid][skill]) ? '#' : '-';
        }
        dots[SKILL_MAX_LEVEL] = EOS;

        format(line, sizeof(line), "%s [%s] - %s\n", gSkillName[skill], dots, gSkillEffect[skill]);
        strcat(body, line, sizeof(body));
    }

    new caption[64];
    format(caption, sizeof(caption), "Skill tree - poin %d | level %d",
        SkillPoints[playerid], PlayerSurvival[playerid][psLevel]);
    ShowPlayerDialog(playerid, DIALOG_SKILL, DIALOG_STYLE_LIST, caption, body, "Tambah", "Tutup");
    return 1;
}

stock Skills_OnDialogResponse(playerid, dialogid, response, listitem, const inputtext[])
{
    #pragma unused inputtext

    if (dialogid != DIALOG_SKILL) return 0;
    if (!response) return 1;

    Skills_Spend(playerid, listitem);
    Skills_Show(playerid);
    return 1;
}
