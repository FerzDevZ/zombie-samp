/* Papan peringkat global.

   Satu query UNION ALL (bukan callback berantai) supaya urutan kategori tetap
   deterministik dan hanya satu round-trip ke database. Sumber data nyata:
   - kill_log jml kill zombie per karakter (victim_kind 2)
   - characters.cash, characters.playtime_sec, characters.rank_points
   - MUSIM: kill bulan kalender berjalan (created_at epoch ms >= awal bulan).
   - ARSIP: musim terakhir yang sudah diarsipkan `scripts/season-rollover.sh`
     ke tabel season_archive (migrasi 023). Tanpa baris arsip, kategori ini
     tidak muncul sama sekali — bukan menampilkan angka 0 palsu. */

#if defined _ls_leaderboard_included
    #endinput
#endif
#define _ls_leaderboard_included

#define TOP_LIMIT_PER_CATEGORY 5

stock Leaderboard_Show(playerid)
{
    new query[2048];

    format(query, sizeof(query),
        "(SELECT 'KILL' AS kat, c.name AS nama, COUNT(*) AS skor \
          FROM kill_log k JOIN characters c ON c.id = k.killer_id \
          WHERE k.victim_kind = 2 GROUP BY k.killer_id ORDER BY skor DESC LIMIT %d) \
         UNION ALL \
         (SELECT 'MUSIM' AS kat, c.name AS nama, COUNT(*) AS skor \
          FROM kill_log k JOIN characters c ON c.id = k.killer_id \
          WHERE k.victim_kind = 2 AND k.created_at >= UNIX_TIMESTAMP(DATE_FORMAT(NOW(), '%%Y-%%m-01')) * 1000 \
          GROUP BY k.killer_id ORDER BY skor DESC LIMIT %d) \
         UNION ALL \
         (SELECT 'UANG' AS kat, c.name AS nama, c.cash AS skor \
          FROM characters c ORDER BY c.cash DESC LIMIT %d) \
         UNION ALL \
         (SELECT 'JAM' AS kat, c.name AS nama, FLOOR(c.playtime_sec / 3600) AS skor \
          FROM characters c ORDER BY c.playtime_sec DESC LIMIT %d) \
         UNION ALL \
         (SELECT 'RANK' AS kat, c.name AS nama, c.rank_points AS skor \
          FROM characters c ORDER BY c.rank_points DESC LIMIT %d) \
         UNION ALL \
         (SELECT CONCAT('ARSIP ', s.season) AS kat, s.char_name AS nama, s.score AS skor \
          FROM season_archive s \
          WHERE s.category = 'kill' \
            AND s.season = (SELECT MAX(season) FROM season_archive) \
          ORDER BY s.rank_no LIMIT %d)",
        TOP_LIMIT_PER_CATEGORY, TOP_LIMIT_PER_CATEGORY, TOP_LIMIT_PER_CATEGORY,
        TOP_LIMIT_PER_CATEGORY, TOP_LIMIT_PER_CATEGORY, TOP_LIMIT_PER_CATEGORY);

    /* Read-only berat (UNION ALL + GROUP BY) lewat pquery: pool paralel,
       tidak mengantre di belakang write transaksi FIFO (audit performa F5). */
    mysql_pquery(gSQL, query, "Leaderboard_OnLoaded", "i", playerid);
    return 1;
}

forward Leaderboard_OnLoaded(playerid);
public Leaderboard_OnLoaded(playerid)
{
    if (!IsPlayerConnected(playerid)) return 0;

    if (mysql_errno(gSQL) != 0)
    {
        SendClientMessage(playerid, UI_ACCENT_DANGER, "[TOP] Gagal membaca papan peringkat.");
        Log_Format(LOG_ERROR, "[TOP] errno=%d", mysql_errno(gSQL));
        return 0;
    }

    if (cache_num_rows() == 0)
    {
        SendClientMessage(playerid, UI_TEXT_MUTED, "[TOP] Belum ada data.");
        return 0;
    }

    new lastCategory[16], line[160], rank = 0;
    lastCategory[0] = EOS;

    for (new row = 0; row < cache_num_rows(); row++)
    {
        new category[16], holderName[32];
        cache_get_value_name(row, "kat", category, sizeof(category));
        cache_get_value_name(row, "nama", holderName, sizeof(holderName));
        new score;

        if (strcmp(category, "JAM") == 0)
        {
            cache_get_value_name_int(row, "skor", score);
        }
        else
        {
            cache_get_value_name_int(row, "skor", score);
        }

        if (strcmp(category, lastCategory) != 0)
        {
            rank = 0;
            format(lastCategory, sizeof(lastCategory), "%s", category);

            if (strcmp(category, "KILL") == 0) SendClientMessage(playerid, UI_ACCENT_PRIMARY, "== KILL ZOMBIE (TOTAL) ==");
            else if (category[0] == 'A') SendClientMessage(playerid, UI_ACCENT_PRIMARY, "== KILL MUSIM LALU (ARSIP) ==");
            else if (strcmp(category, "MUSIM") == 0) SendClientMessage(playerid, UI_ACCENT_PRIMARY, "== KILL MUSIM INI (BULAN INI) ==");
            else if (strcmp(category, "UANG") == 0) SendClientMessage(playerid, UI_ACCENT_PRIMARY, "== UANG ==");
            else if (strcmp(category, "JAM") == 0) SendClientMessage(playerid, UI_ACCENT_PRIMARY, "== JAM MAIN ==");
            else SendClientMessage(playerid, UI_ACCENT_PRIMARY, "== RANK POINT ==");
        }

        rank++;
        if (strcmp(category, "UANG") == 0)
        {
            format(line, sizeof(line), "%d. %s - $%d", rank, holderName, score);
        }
        else if (strcmp(category, "JAM") == 0)
        {
            format(line, sizeof(line), "%d. %s - %d jam", rank, holderName, score);
        }
        else
        {
            format(line, sizeof(line), "%d. %s - %d", rank, holderName, score);
        }
        SendClientMessage(playerid, UI_TEXT_SECONDARY, line);
    }
    return 1;
}