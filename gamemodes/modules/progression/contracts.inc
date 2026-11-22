#if defined _ls_contracts_included
    #endinput
#endif
#define _ls_contracts_included

/* Kontrak harian: tiga target per karakter per hari.
   Deterministik dari (hari, char_id) supaya server restart tidak mengganti kontrak
   di tengah jalan. Progres disimpan throttled lewat autosave + langsung saat klaim. */

#define DIALOG_CONTRACT 2961
#define CONTRACT_SLOTS  4

#define CONTRACT_KIND_ANY       0
#define CONTRACT_KIND_RUNNER    1
#define CONTRACT_KIND_BRUTE     2
#define CONTRACT_KIND_SCREAMER  3
#define CONTRACT_KIND_DOCS      4

new bool:ContractLoaded[MAX_PLAYERS];
new ContractKind[MAX_PLAYERS][CONTRACT_SLOTS];
new ContractTarget[MAX_PLAYERS][CONTRACT_SLOTS];
new ContractProgress[MAX_PLAYERS][CONTRACT_SLOTS];
new ContractClaimed[MAX_PLAYERS][CONTRACT_SLOTS];
new ContractRewardCash[MAX_PLAYERS][CONTRACT_SLOTS];
new ContractRewardItem[MAX_PLAYERS][CONTRACT_SLOTS];
new ContractRewardAmount[MAX_PLAYERS][CONTRACT_SLOTS];
new bool:ContractDirty[MAX_PLAYERS];
new ContractDayId[MAX_PLAYERS];

forward Contracts_OnLoaded(playerid);

stock Contracts_DayId()
{
    return gettime() / 86400;
}

stock Contracts_KindName(kind, dest[], size)
{
    dest[0] = EOS;
    switch (kind)
    {
        case CONTRACT_KIND_ANY:      format(dest, size, "Zombie apa pun");
        case CONTRACT_KIND_RUNNER:   format(dest, size, "Runner");
        case CONTRACT_KIND_BRUTE:    format(dest, size, "Brute");
        case CONTRACT_KIND_SCREAMER: format(dest, size, "Screamer");
        case CONTRACT_KIND_DOCS:     format(dest, size, "Dokumen rahasia");
    }
    if (dest[0] == EOS) format(dest, size, "Zombie");
    return 1;
}

/* Satu sumber kebenaran untuk bentuk kontrak: dipakai saat load (INSERT IGNORE)
   dan tidak pernah memakai RNG runtime, jadi restart tidak mengubah target. */
stock Contracts_Generate(charId, dayId, slot, &kind, &target, &rewardCash, &rewardItem, &rewardAmount)
{
    new seed = (dayId * 2654435761 + charId * 40503 + slot * 7919) & 0x7FFFFFFF;

    kind = CONTRACT_KIND_ANY;
    if (slot == 1) kind = CONTRACT_KIND_RUNNER + (seed >> 4) % 2;
    else if (slot == 2)
    {
        new runnerKind = CONTRACT_KIND_RUNNER + (seed >> 4) % 2;
        kind = (runnerKind == CONTRACT_KIND_RUNNER) ? CONTRACT_KIND_BRUTE : CONTRACT_KIND_SCREAMER;
    }
    else if (slot == 3) kind = CONTRACT_KIND_DOCS;

    switch (kind)
    {
        case CONTRACT_KIND_ANY:      target = 15 + seed % 11;
        case CONTRACT_KIND_RUNNER:   target = 5 + seed % 7;
        case CONTRACT_KIND_BRUTE:    target = 1 + seed % 3;
        case CONTRACT_KIND_SCREAMER: target = 2 + seed % 4;
        case CONTRACT_KIND_DOCS:     target = 2 + seed % 3;
    }

    rewardCash = 150 + target * 12;
    rewardItem = ITEM_PISTOL_AMMO;
    rewardAmount = 12;

    switch (kind)
    {
        case CONTRACT_KIND_RUNNER:   { rewardItem = ITEM_BANDAGE; rewardAmount = 2; }
        case CONTRACT_KIND_BRUTE:    { rewardItem = ITEM_SCRAP; rewardAmount = 3; }
        case CONTRACT_KIND_SCREAMER: { rewardItem = ITEM_MEDKIT; rewardAmount = 1; }
        case CONTRACT_KIND_DOCS:
        {
            rewardCash = 260 + target * 45;
            rewardItem = ITEM_WAREHOUSE_KEY;
            rewardAmount = 1;
        }
    }
    return 1;
}

stock Contracts_Reset(playerid)
{
    ContractLoaded[playerid] = false;
    ContractDirty[playerid] = false;

    for (new slot = 0; slot < CONTRACT_SLOTS; slot++)
    {
        ContractKind[playerid][slot] = CONTRACT_KIND_ANY;
        ContractTarget[playerid][slot] = 0;
        ContractProgress[playerid][slot] = 0;
        ContractClaimed[playerid][slot] = 0;
        ContractRewardCash[playerid][slot] = 0;
        ContractRewardItem[playerid][slot] = ITEM_NONE_ID;
        ContractRewardAmount[playerid][slot] = 0;
    }
    return 1;
}

stock Contracts_Load(playerid)
{
    Contracts_Reset(playerid);
    if (!SQL_IsReady() || ActiveCharId[playerid] == 0) return 0;

    new charId = ActiveCharId[playerid];
    new dayId = Contracts_DayId();
    ContractDayId[playerid] = dayId;

    /* Isi tabel lebih dulu (idempotent), lalu baca satu kali. */
    new query[420];
    for (new slot = 0; slot < CONTRACT_SLOTS; slot++)
    {
        new kind, target, rewardCash, rewardItem, rewardAmount;
        Contracts_Generate(charId, dayId, slot, kind, target, rewardCash, rewardItem, rewardAmount);

        mysql_format(gSQL, query, sizeof(query),
            "INSERT IGNORE INTO contracts (char_id, day_id, slot, kind, target, progress, reward_cash, \
             reward_item, reward_amount, claimed, created_at) \
             VALUES (%d, %d, %d, %d, %d, 0, %d, %d, %d, 0, %d)",
            charId, dayId, slot, kind, target, rewardCash, rewardItem, rewardAmount, gettime());
        mysql_tquery(gSQL, query, "", "");
    }

    mysql_format(gSQL, query, sizeof(query),
        "SELECT slot, kind, target, progress, reward_cash, reward_item, reward_amount, claimed \
         FROM contracts WHERE char_id = %d AND day_id = %d",
        charId, dayId);
    mysql_tquery(gSQL, query, "Contracts_OnLoaded", "i", playerid);
    return 1;
}

public Contracts_OnLoaded(playerid)
{
    if (!IsPlayerConnected(playerid)) return 0;

    new rows = cache_num_rows();
    for (new row = 0; row < rows; row++)
    {
        new slot, kind, target, progress, rewardCash, rewardItem, rewardAmount, claimed;
        cache_get_value_name_int(row, "slot", slot);
        cache_get_value_name_int(row, "kind", kind);
        cache_get_value_name_int(row, "target", target);
        cache_get_value_name_int(row, "progress", progress);
        cache_get_value_name_int(row, "reward_cash", rewardCash);
        cache_get_value_name_int(row, "reward_item", rewardItem);
        cache_get_value_name_int(row, "reward_amount", rewardAmount);
        cache_get_value_name_int(row, "claimed", claimed);

        if (slot < 0 || slot >= CONTRACT_SLOTS) continue;

        ContractKind[playerid][slot] = kind;
        ContractTarget[playerid][slot] = target;
        ContractProgress[playerid][slot] = progress;
        ContractRewardCash[playerid][slot] = rewardCash;
        ContractRewardItem[playerid][slot] = rewardItem;
        ContractRewardAmount[playerid][slot] = rewardAmount;
        ContractClaimed[playerid][slot] = claimed;
    }

    /* Jaring pengaman: kalau INSERT IGNORE gagal, kontrak tetap ada di memori. */
    for (new slot = 0; slot < CONTRACT_SLOTS; slot++)
    {
        if (ContractTarget[playerid][slot] > 0) continue;

        new kind, target, rewardCash, rewardItem, rewardAmount;
        Contracts_Generate(ActiveCharId[playerid], ContractDayId[playerid], slot,
            kind, target, rewardCash, rewardItem, rewardAmount);

        ContractKind[playerid][slot] = kind;
        ContractTarget[playerid][slot] = target;
        ContractRewardCash[playerid][slot] = rewardCash;
        ContractRewardItem[playerid][slot] = rewardItem;
        ContractRewardAmount[playerid][slot] = rewardAmount;
    }

    ContractLoaded[playerid] = true;
    return 1;
}

stock Contracts_Save(playerid)
{
    if (!SQL_IsReady() || !ContractLoaded[playerid] || ActiveCharId[playerid] == 0) return 0;
    if (!ContractDirty[playerid]) return 0;

    new charId = ActiveCharId[playerid];
    new dayId = ContractDayId[playerid];
    new query[256];

    for (new slot = 0; slot < CONTRACT_SLOTS; slot++)
    {
        if (ContractTarget[playerid][slot] <= 0) continue;

        /* Upsert, bukan UPDATE polos: bila row DB gagal dibuat saat load
           (INSERT IGNORE hiccup), UPDATE cocok 0 row dan claimed hilang —
           relog lalu klaim ulang = dupe hadiah (audit T10). */
        mysql_format(gSQL, query, sizeof(query),
            "INSERT INTO contracts (char_id, day_id, slot, progress, claimed) VALUES (%d, %d, %d, %d, %d) \
             ON DUPLICATE KEY UPDATE progress = VALUES(progress), claimed = VALUES(claimed)",
            charId, dayId, slot, ContractProgress[playerid][slot], ContractClaimed[playerid][slot]);
        mysql_tquery(gSQL, query, "", "");
    }

    ContractDirty[playerid] = false;
    return 1;
}

stock Contracts_OnZombieKill(playerid, E_ZOMBIE_TYPE:type)
{
    if (!IsPlayerConnected(playerid)) return 0;
    if (!ContractLoaded[playerid]) return 0;
    if (Zone_IsHub(Player_GetZone(playerid))) return 0;   // sama dengan aturan XP: tidak bisa farming di kamp

    new changed = 0;
    for (new slot = 0; slot < CONTRACT_SLOTS; slot++)
    {
        new target = ContractTarget[playerid][slot];
        if (target <= 0) continue;
        if (ContractClaimed[playerid][slot]) continue;
        if (ContractProgress[playerid][slot] >= target) continue;

        new kind = ContractKind[playerid][slot];
        new matched = 0;

        switch (kind)
        {
            case CONTRACT_KIND_ANY: matched = 1;
            case CONTRACT_KIND_RUNNER: matched = (type == ZT_RUNNER);
            case CONTRACT_KIND_BRUTE: matched = (type == ZT_BRUTE);
            case CONTRACT_KIND_SCREAMER: matched = (type == ZT_SCREAMER);
        }

        if (!matched) continue;

        ContractProgress[playerid][slot]++;
        changed = 1;

        if (ContractProgress[playerid][slot] == target)
        {
            new message[128];
            format(message, sizeof(message), "[KONTRAK] Selesai! Buka /kontrak untuk klaim hadiah.");
            SendClientMessage(playerid, UI_ACCENT_PRIMARY, message);
        }
    }

    if (changed) ContractDirty[playerid] = true;
    return changed;
}

/* Progres kontrak dokumen naik saat pemain mendapat dokumen/buku catatan. */
stock Contracts_OnDocumentFound(playerid)
{
    if (!IsPlayerConnected(playerid)) return 0;
    if (!ContractLoaded[playerid]) return 0;

    new changed = 0;
    for (new slot = 0; slot < CONTRACT_SLOTS; slot++)
    {
        if (ContractKind[playerid][slot] != CONTRACT_KIND_DOCS) continue;
        if (ContractClaimed[playerid][slot]) continue;

        new target = ContractTarget[playerid][slot];
        if (target <= 0) continue;
        if (ContractProgress[playerid][slot] >= target) continue;

        ContractProgress[playerid][slot]++;
        changed = 1;

        if (ContractProgress[playerid][slot] == target)
        {
            SendClientMessage(playerid, UI_ACCENT_PRIMARY,
                "[KONTRAK] Dokumen lengkap! Buka /kontrak untuk klaim hadiah.");
        }
    }

    if (changed) ContractDirty[playerid] = true;
    return changed;
}

stock Contracts_ReadyCount(playerid)
{
    if (!ContractLoaded[playerid]) return 0;

    new ready = 0;
    for (new slot = 0; slot < CONTRACT_SLOTS; slot++)
    {
        if (ContractClaimed[playerid][slot]) continue;
        if (ContractTarget[playerid][slot] <= 0) continue;
        if (ContractProgress[playerid][slot] < ContractTarget[playerid][slot]) continue;
        ready++;
    }
    return ready;
}

stock Contracts_Show(playerid)
{
    if (!Account_IsPlaying(playerid)) return 0;

    if (!ContractLoaded[playerid])
    {
        SendClientMessage(playerid, UI_TEXT_MUTED, "[KONTRAK] Sedang memuat, coba lagi sebentar.");
        return 1;
    }

    new body[768], line[160];
    new contractName[32];

    format(body, sizeof(body), "{FFFFFF}Kontrak hari ini\n\n");

    for (new slot = 0; slot < CONTRACT_SLOTS; slot++)
    {
        Contracts_KindName(ContractKind[playerid][slot], contractName, sizeof(contractName));

        if (ContractClaimed[playerid][slot])
        {
            format(line, sizeof(line), "{8A8A8A}[SELESAI] %s x%d\n", contractName, ContractTarget[playerid][slot]);
        }
        else if (ContractProgress[playerid][slot] >= ContractTarget[playerid][slot])
        {
            format(line, sizeof(line), "{7ED957}[KLAIM] %s %d/%d\n", contractName,
                ContractProgress[playerid][slot], ContractTarget[playerid][slot]);
        }
        else
        {
            format(line, sizeof(line), "%s %d/%d\n", contractName,
                ContractProgress[playerid][slot], ContractTarget[playerid][slot]);
        }
        strcat(body, line, sizeof(body));
    }

    strcat(body, "\nHadiah: uang + item. Kontrak baru setiap hari.", sizeof(body));
    ShowPlayerDialog(playerid, DIALOG_CONTRACT, DIALOG_STYLE_LIST, "Kontrak harian", body, "Klaim", "Tutup");
    return 1;
}

stock Contracts_Claim(playerid, slot)
{
    if (!ContractLoaded[playerid]) return 0;
    if (slot < 0 || slot >= CONTRACT_SLOTS) return 0;
    if (ContractClaimed[playerid][slot]) return 0;

    new target = ContractTarget[playerid][slot];
    if (target <= 0) return 0;
    if (ContractProgress[playerid][slot] < target) return 0;

    /* Kontrak dokumen: progres dihitung per pickup, jadi 1 dokumen bisa
       di-drop/pickup berulang untuk menyelesaikan kontrak (audit F5).
       Klaim wajib MENYERAHKAN dokumen sejumlah target (konsumsi). */
    if (ContractKind[playerid][slot] == CONTRACT_KIND_DOCS)
    {
        if (Inventory_CountItem(playerid, ITEM_DOCUMENT) < target)
        {
            new docLine[112];
            format(docLine, sizeof(docLine), "[KONTRAK] Bawa %d dokumen fisik untuk diserahkan (punyamu %d).",
                target, Inventory_CountItem(playerid, ITEM_DOCUMENT));
            SendClientMessage(playerid, UI_ACCENT_WARN, docLine);
            return 0;
        }
        if (!Inventory_Consume(playerid, ITEM_DOCUMENT, target, "contract_docs"))
        {
            SendClientMessage(playerid, UI_ACCENT_WARN, "[KONTRAK] Dokumen tidak bisa diserahkan.");
            return 0;
        }
    }

    /* Hadiah item wajib masuk tas DULU sebelum klaim ditandai: kalau Grant
       gagal (tas penuh), kontrak tetap bisa diklaim ulang, bukan hangus. */
    if (Item_IsValid(ContractRewardItem[playerid][slot]))
    {
        if (!Inventory_Grant(playerid, ContractRewardItem[playerid][slot],
            ContractRewardAmount[playerid][slot], "contract"))
        {
            SendClientMessage(playerid, UI_ACCENT_WARN, "[KONTRAK] Tas penuh. Kosongkan slot lalu klaim lagi.");
            return 0;
        }
    }

    ContractClaimed[playerid][slot] = 1;
    ContractDirty[playerid] = true;

    PlayerSurvival[playerid][psCash] += ContractRewardCash[playerid][slot];
    RankPoints[playerid] += 5;

    Skills_AddXp(playerid, 15);
    Contracts_Save(playerid);

    new message[160];
    format(message, sizeof(message), "[KONTRAK] Selesai. +$%d, item %d x%d, +15 XP.",
        ContractRewardCash[playerid][slot], ContractRewardItem[playerid][slot],
        ContractRewardAmount[playerid][slot]);
    SendClientMessage(playerid, UI_ACCENT_PRIMARY, message);

    Log_Format(LOG_INFO, "[CONTRACT] char=%d slot=%d klaim uang=%d item=%d",
        ActiveCharId[playerid], slot, ContractRewardCash[playerid][slot],
        ContractRewardItem[playerid][slot]);
    return 1;
}

stock Contracts_OnDialogResponse(playerid, dialogid, response, listitem)
{
    if (dialogid != DIALOG_CONTRACT) return 0;
    if (!response) return 1;

    /* Baris 0=judul, 1=kosong, kontrak mulai index 2 (audit #3). */
    if (listitem < 2) return 1;
    if (!Contracts_Claim(playerid, listitem - 2))
    {
        SendClientMessage(playerid, UI_TEXT_MUTED, "[KONTRAK] Belum selesai atau sudah diklaim.");
    }
    else
    {
        Contracts_Show(playerid);
    }
    return 1;
}
