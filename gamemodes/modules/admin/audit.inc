/* Audit log admin (ROADMAP 389/402/430).
   Ditulis ke tabel `admin_actions` (migrasi 022) supaya riwayat tidak hilang saat
   file log dirotasi. Semua panggilan memakai mysql_format '%e' agar nama pemain
   dengan tanda kutip tidak merusak query. */


#if defined _ls_audit_included
    #endinput
#endif
#define _ls_audit_included

forward AdminAudit_Log(adminId, const action[], targetAccount, const detail[]);

stock AdminAudit_Log(adminId, const action[], targetAccount, const detail[])
{
    if (!SQL_IsReady()) return 0;

    new adminName[MAX_PLAYER_NAME + 1];
    if (IsPlayerConnected(adminId))
    {
        GetPlayerName(adminId, adminName, sizeof(adminName));
    }
    else
    {
        format(adminName, sizeof(adminName), "console");
    }

    new actionText[33];
    format(actionText, sizeof(actionText), "%s", action);

    new detailText[ADMIN_AUDIT_DETAIL_MAX + 1];
    format(detailText, sizeof(detailText), "%s", detail);

    new query[420];
    mysql_format(gSQL, query, sizeof(query),
        "INSERT INTO admin_actions (account_id, admin_name, action, target_account, detail, created_at) \
         VALUES (%d, '%e', '%e', %d, '%e', %d)",
        (IsPlayerConnected(adminId) ? AccountId[adminId] : 0),
        adminName, actionText, targetAccount, detailText, gettime());
    mysql_tquery(gSQL, query, "", "");

    /* Tetap ke file log: kalau DB sedang mati, jejaknya masih ada. */
    Log_Format(LOG_INFO, "[AUDIT] %s oleh %s target_account=%d detail=%s",
        actionText, adminName, targetAccount, detailText);
    return 1;
}

stock AdminAudit_ShowRecent(adminId, limit)
{
    if (!IsServerAdmin(adminId))
    {
        SendClientMessage(adminId, UI_ACCENT_WARN, "[AUDIT] Hanya admin.");
        return 0;
    }
    if (!SQL_IsReady())
    {
        SendClientMessage(adminId, UI_ACCENT_WARN, "[AUDIT] Database belum siap.");
        return 0;
    }
    if (limit < 1) limit = 15;
    if (limit > 50) limit = 50;

    new query[256];
    mysql_format(gSQL, query, sizeof(query),
        "SELECT admin_name, action, detail, created_at FROM admin_actions \
         ORDER BY id DESC LIMIT %d", limit);
    mysql_pquery(gSQL, query, "AdminAudit_OnRecentLoaded", "d", adminId);
    return 1;
}

forward AdminAudit_OnRecentLoaded(adminId);

public AdminAudit_OnRecentLoaded(adminId)
{
    if (!IsPlayerConnected(adminId)) return 0;

    new rows = cache_num_rows();
    if (rows == 0)
    {
        SendClientMessage(adminId, UI_TEXT_MUTED, "[AUDIT] Belum ada catatan.");
        return 0;
    }

    SendClientMessage(adminId, UI_ACCENT_PRIMARY, "[AUDIT] Catatan admin terbaru:");

    new adminName[25], action[33], detail[ADMIN_AUDIT_DETAIL_MAX + 1], created;
    for (new row = 0; row < rows; row++)
    {
        cache_get_value_name(row, "admin_name", adminName, sizeof(adminName));
        cache_get_value_name(row, "action", action, sizeof(action));
        cache_get_value_name(row, "detail", detail, sizeof(detail));
        cache_get_value_int(row, "created_at", created);

        new when[24];
        AdminAudit_TimeAgo(created, when, sizeof(when));

        new line[224];
        format(line, sizeof(line), "%s | %s %s — %s", adminName, action, detail, when);
        SendClientMessage(adminId, UI_TEXT_MUTED, line);
    }
    return 1;
}

stock AdminAudit_TimeAgo(epoch, dest[], size)
{
    new diff = gettime() - epoch;
    if (diff < 0) diff = 0;

    if (diff < 60) format(dest, size, "%d detik lalu", diff);
    else if (diff < 3600) format(dest, size, "%d menit lalu", diff / 60);
    else if (diff < 86400) format(dest, size, "%d jam lalu", diff / 3600);
    else format(dest, size, "%d hari lalu", diff / 86400);
    return 1;
}
