#if defined _ls_party_stash_log_included
    #endinput
#endif
#define _ls_party_stash_log_included

/* Riwayat peti party (ROADMAP 414).
   - Setiap take/put yang BERHASIL dicatat ke `party_stash_log` (bukan ke file
     log, karena log file ikut rotasi dan hilang dalam 7 hari).
   - Yang boleh melihat: petugas gudang + pemimpin party.
   - Query memakai mysql_pquery: murni baca, tidak boleh menahan antrean FIFO
     transaksi item (aturan yang sama seperti leaderboard). */

#define PARTY_STASH_LOG_SHOW 10
#define DIALOG_PARTY_STASH_LOG 3405

forward PartyStash_OnLogLoaded(playerid);

stock PartyStash_LogEntry(stashCharId, playerid, const action[], itemid, amount)
{
    if (!SQL_IsReady()) return 0;
    if (stashCharId <= 0) return 0;
    if (!IsPlayerConnected(playerid)) return 0;
    if (ActiveCharId[playerid] == 0) return 0;
    if (itemid <= 0 || amount <= 0) return 0;

    new actorName[MAX_PLAYER_NAME + 1];
    GetPlayerName(playerid, actorName, sizeof(actorName));

    new query[288];
    mysql_format(gSQL, query, sizeof(query),
        "INSERT INTO party_stash_log \
         (stash_char_id, actor_char_id, actor_name, action, item_id, amount, created_at) \
         VALUES (%d, %d, '%e', '%e', %d, %d, %d)",
        stashCharId, ActiveCharId[playerid], actorName, action, itemid, amount, gettime());
    mysql_tquery(gSQL, query, "", "");
    return 1;
}

stock PartyStash_ShowHistory(playerid)
{
    if (!IsPlayerConnected(playerid)) return 0;

    if (!PartyStash_IsQuartermaster(playerid))
    {
        SendClientMessage(playerid, UI_ACCENT_WARN, "[PETI] Riwayat hanya untuk petugas gudang dan pemimpin party.");
        return 0;
    }

    new stashChar = PartyStashChar[playerid];
    if (stashChar <= 0)
    {
        stashChar = ActiveCharId[playerid];
    }
    if (stashChar <= 0) return 0;

    new query[256];
    mysql_format(gSQL, query, sizeof(query),
        "SELECT actor_name, action, item_id, amount, created_at \
         FROM party_stash_log WHERE stash_char_id = %d \
         ORDER BY id DESC LIMIT %d",
        stashChar, PARTY_STASH_LOG_SHOW);
    mysql_pquery(gSQL, query, "PartyStash_OnLogLoaded", "i", playerid);
    return 1;
}

public PartyStash_OnLogLoaded(playerid)
{
    if (!IsPlayerConnected(playerid)) return 0;
    if (mysql_errno(gSQL) != 0) return 0;

    new rows = cache_num_rows();
    if (rows == 0)
    {
        SendClientMessage(playerid, UI_TEXT_MUTED, "[PETI] Belum ada riwayat pengambilan/penyimpanan.");
        return 1;
    }

    new body[1024], line[96];
    body[0] = EOS;
    strcat(body, "Pelaku\tAksi\tBarang\tWaktu\n", sizeof(body));

    new actorName[25], action[9], itemid, amount, createdAt, itemLabel[24], ago[24];
    for (new row = 0; row < rows; row++)
    {
        cache_get_value_name(row, "actor_name", actorName, sizeof(actorName));
        cache_get_value_name(row, "action", action, sizeof(action));
        cache_get_value_name_int(row, "item_id", itemid);
        cache_get_value_name_int(row, "amount", amount);
        cache_get_value_name_int(row, "created_at", createdAt);

        Item_GetName(itemid, itemLabel, sizeof(itemLabel));
        AdminAudit_TimeAgo(createdAt, ago, sizeof(ago));
        format(line, sizeof(line), "%s\t%s %s\tx%d\t%s\n", actorName,
            action, itemLabel, amount, ago);
        strcat(body, line, sizeof(body));
    }

    ShowPlayerDialog(playerid, DIALOG_PARTY_STASH_LOG, DIALOG_STYLE_TABLIST_HEADERS,
        "Riwayat Peti Party", body, "Tutup", "");
    return 1;
}


