#if defined _ls_deaths_included
    #endinput
#endif
#define _ls_deaths_included

/* Log kematian pemain (tabel `deaths`, migrasi 027).
   Sebelum ini OnPlayerDeath membuang killerid dan reason, jadi pemain tidak
   bisa tahu apa yang membunuhnya dan tidak ada bukti untuk mendeteksi kill
   farming. `kill_log` yang lama hanya mencatat korban zombie, bukan pemain. */

#define DEATH_KIND_ZOMBIE      0
#define DEATH_KIND_PLAYER      1
#define DEATH_KIND_ENVIRONMENT 2

#define DEATH_REASON_DROWN     53
#define DEATH_REASON_FALL      54

#define DEATH_SHOW_COUNT       3
#define DIALOG_DEATH_LOG       3410

/* Murni: dipakai OnPlayerDeath dan selftest. Aturan urutan penting —
   killer yang sah selalu menang, karena `reason` bisa saja 53/54 saat pemain
   didorong jatuh oleh pemain lain. */
stock Deaths_KindFor(bool:killerIsPlayer, reason)
{
    if (killerIsPlayer) return DEATH_KIND_PLAYER;
    if (reason == DEATH_REASON_DROWN || reason == DEATH_REASON_FALL) return DEATH_KIND_ENVIRONMENT;
    return DEATH_KIND_ZOMBIE;
}

stock Deaths_KindName(kind, dest[], size)
{
    switch (kind)
    {
        case DEATH_KIND_PLAYER: format(dest, size, "pemain");
        case DEATH_KIND_ENVIRONMENT: format(dest, size, "lingkungan");
        default: format(dest, size, "zombie");
    }
    return 1;
}

/* Pemain lain di sekitar layak melihat jejaknya, tapi jangan sampai log
   menulis baris untuk sesi yang belum memilih karakter. */
stock Deaths_Record(playerid, killerid, reason)
{
    if (playerid < 0 || playerid >= MAX_PLAYERS) return 0;
    if (ActiveCharId[playerid] <= 0) return 0;

    new bool:killerIsPlayer = false;
    new killerCharId = 0;
    if (killerid != INVALID_PLAYER_ID && IsPlayerConnected(killerid) && killerid != playerid)
    {
        if (Account_IsPlaying(killerid) && ActiveCharId[killerid] > 0)
        {
            killerIsPlayer = true;
            killerCharId = ActiveCharId[killerid];
        }
    }

    new kind = Deaths_KindFor(killerIsPlayer, reason);
    new Float:px, Float:py, Float:pz;
    GetPlayerPos(playerid, px, py, pz);

    new query[256];
    mysql_format(gSQL, query, sizeof(query),
        "INSERT INTO deaths (victim_char_id, killer_char_id, victim_kind, weapon, pos_x, pos_y, pos_z, created_at) \
         VALUES (%d, %d, %d, %d, %.2f, %.2f, %.2f, UNIX_TIMESTAMP())",
        ActiveCharId[playerid], killerCharId, kind, reason, px, py, pz);
    mysql_tquery(gSQL, query, "", "");

    /* Pesan langsung: pemain tahu penyebabnya tanpa harus membuka perintah. */
    if (kind == DEATH_KIND_PLAYER)
    {
        new killerName[MAX_PLAYER_NAME + 1], line[128];
        GetPlayerName(killerid, killerName, sizeof(killerName));
        format(line, sizeof(line), "[KEMATIAN] Kamu dibunuh %s. /kematian untuk riwayat.", killerName);
        SendClientMessage(playerid, UI_ACCENT_DANGER, line);
    }
    return 1;
}

stock Deaths_TimeAgo(createdAt, dest[], size)
{
    new seconds = gettime() - createdAt;
    if (seconds < 0) seconds = 0;

    if (seconds < 60) format(dest, size, "%d detik lalu", seconds);
    else if (seconds < 3600) format(dest, size, "%d menit lalu", seconds / 60);
    else if (seconds < 86400) format(dest, size, "%d jam lalu", seconds / 3600);
    else format(dest, size, "%d hari lalu", seconds / 86400);
    return 1;
}

stock Deaths_ShowLast(playerid)
{
    if (!Account_IsPlaying(playerid)) return 0;

    new query[384];
    mysql_format(gSQL, query, sizeof(query),
        "SELECT d.victim_kind, d.weapon, d.created_at, \
                IFNULL(v.name, '?') AS victim_name, IFNULL(k.name, '-') AS killer_name \
         FROM deaths d \
         LEFT JOIN characters v ON v.id = d.victim_char_id \
         LEFT JOIN characters k ON k.id = d.killer_char_id \
         WHERE d.victim_char_id = %d ORDER BY d.id DESC LIMIT %d",
        ActiveCharId[playerid], DEATH_SHOW_COUNT);
    mysql_pquery(gSQL, query, "Deaths_OnLastLoaded", "i", playerid);
    return 1;
}

forward Deaths_OnLastLoaded(playerid);

public Deaths_OnLastLoaded(playerid)
{
    if (!IsPlayerConnected(playerid)) return 0;

    new rows = cache_num_rows();
    if (rows == 0)
    {
        SendClientMessage(playerid, UI_TEXT_SECONDARY, "[KEMATIAN] Belum ada catatan kematian untuk karakter ini.");
        return 1;
    }

    new body[512], line[160], kindName[16], ago[32];
    body[0] = EOS;

    for (new row = 0; row < rows; row++)
    {
        new kind, weapon, createdAt, victimName[MAX_PLAYER_NAME + 1], killerName[MAX_PLAYER_NAME + 1];
        cache_get_value_name_int(row, "victim_kind", kind);
        cache_get_value_name_int(row, "weapon", weapon);
        cache_get_value_name_int(row, "created_at", createdAt);
        cache_get_value_name(row, "victim_name", victimName, sizeof(victimName));
        cache_get_value_name(row, "killer_name", killerName, sizeof(killerName));

        Deaths_KindName(kind, kindName, sizeof(kindName));
        Deaths_TimeAgo(createdAt, ago, sizeof(ago));

        if (kind == DEATH_KIND_PLAYER)
        {
            format(line, sizeof(line), "#%d oleh %s (senjata %d) - %s\n", row + 1, killerName, weapon, ago);
        }
        else
        {
            format(line, sizeof(line), "#%d oleh %s (senjata %d) - %s\n", row + 1, kindName, weapon, ago);
        }
        strcat(body, line, sizeof(body));
    }

    ShowPlayerDialog(playerid, DIALOG_DEATH_LOG, DIALOG_STYLE_MSGBOX,
        "Riwayat kematian", body, "Tutup", "");
    return 1;
}
