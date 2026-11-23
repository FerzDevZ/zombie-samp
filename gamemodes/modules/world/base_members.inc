#if defined _ls_base_members_included
    #endinput
#endif
#define _ls_base_members_included

/* Base milik party (ROADMAP 222).
   - `bases.char_id` tetap pemilik (role 2); anggota disimpan di `base_members`.
   - Peringkat: 0 member (boleh masuk/ambil dari peti), 1 officer (boleh buka
     gate + bangun), 2 owner (semua + atur anggota).
   - Gate hanya bisa dibuka peringkat 1 ke atas: ini yang dimaksud "izin gate".
   - Keanggotaan dimuat sekali saat login (`BaseMembers_Load`), bukan per tick. */

#define BASE_ROLE_MEMBER   0
#define BASE_ROLE_OFFICER  1
#define BASE_ROLE_OWNER    2

#define BASE_MEMBERS_MAX   6
#define BASE_GATE_MIN_ROLE BASE_ROLE_OFFICER

new BaseMemberBase[MAX_PLAYERS];
new BaseMemberRole[MAX_PLAYERS];

forward BaseMembers_OnRowsLoaded(playerid);

stock BaseMembers_ResetPlayer(playerid)
{
    if (playerid < 0 || playerid >= MAX_PLAYERS) return 0;
    BaseMemberBase[playerid] = 0;
    BaseMemberRole[playerid] = -1;
    return 1;
}

/* Dipanggil setelah Base_Load: pemilik tidak perlu baris di tabel anggota,
   tapi anggota butuh base_id sebelum bisa membangun atau membuka gate. */
stock BaseMembers_Load(playerid)
{
    BaseMembers_ResetPlayer(playerid);
    if (!SQL_IsReady() || ActiveCharId[playerid] == 0) return 0;

    new query[220];
    mysql_format(gSQL, query, sizeof(query),
        "SELECT base_id, role FROM base_members WHERE char_id = %d \
         UNION ALL \
         SELECT id, %d FROM bases WHERE char_id = %d AND id NOT IN \
           (SELECT base_id FROM base_members WHERE char_id = %d) \
         ORDER BY role DESC LIMIT 1",
        ActiveCharId[playerid], BASE_ROLE_OWNER, ActiveCharId[playerid], ActiveCharId[playerid]);
    mysql_tquery(gSQL, query, "BaseMembers_OnRowsLoaded", "i", playerid);
    return 1;
}

public BaseMembers_OnRowsLoaded(playerid)
{
    if (!IsPlayerConnected(playerid)) return 0;
    if (mysql_errno(gSQL) != 0) return 0;
    if (cache_num_rows() == 0) return 0;

    cache_get_value_name_int(0, "base_id", BaseMemberBase[playerid]);
    cache_get_value_name_int(0, "role", BaseMemberRole[playerid]);
    return 1;
}

stock bool:BaseMembers_IsMember(playerid, baseId)
{
    if (playerid < 0 || playerid >= MAX_PLAYERS) return false;
    if (baseId <= 0) return false;
    if (BaseMemberBase[playerid] != baseId) return false;
    return (BaseMemberRole[playerid] >= BASE_ROLE_MEMBER);
}

stock BaseMembers_Role(playerid, baseId)
{
    if (!BaseMembers_IsMember(playerid, baseId)) return -1;
    return BaseMemberRole[playerid];
}

/* Satu tempat untuk semua gerbang izin: dipakai gate, build, dan dialog. */
stock bool:BaseMembers_HasRole(playerid, baseId, minRole)
{
    new role = BaseMembers_Role(playerid, baseId);
    if (role < 0) return false;
    return (role >= minRole);
}

/* Aturan murni tanpa state pemain: dipakai gate dan bisa diuji selftest
   tanpa menyentuh data pemain yang sedang online. */
stock bool:BaseMembers_RoleAllowsGate(role)
{
    return (role >= BASE_GATE_MIN_ROLE && role <= BASE_ROLE_OWNER);
}

stock bool:BaseMembers_CanOperateGate(playerid, baseId)
{
    return BaseMembers_RoleAllowsGate(BaseMembers_Role(playerid, baseId));
}

/* Menambah anggota: hanya owner/officer base itu, maksimum BASE_MEMBERS_MAX,
   dan target tidak boleh sudah menjadi anggota base lain. */
stock BaseMembers_Add(actorId, targetId, role)
{
    if (!IsPlayerConnected(actorId) || !IsPlayerConnected(targetId)) return 0;
    if (IsPlayerNPC(targetId)) return 0;
    if (!Account_IsPlaying(targetId)) return 0;

    new baseId = BaseMemberBase[actorId];
    if (baseId <= 0) return 0;

    if (!BaseMembers_HasRole(actorId, baseId, BASE_ROLE_OFFICER))
    {
        SendClientMessage(actorId, UI_ACCENT_WARN, "[BASE] Hanya pemilik/officer yang bisa menambah anggota.");
        return 0;
    }

    if (BaseMemberBase[targetId] == baseId)
    {
        SendClientMessage(actorId, UI_ACCENT_WARN, "[BASE] Pemain itu sudah anggota base ini.");
        return 0;
    }

    if (BaseMemberBase[targetId] > 0)
    {
        SendClientMessage(actorId, UI_ACCENT_WARN, "[BASE] Pemain itu sudah anggota base lain.");
        return 0;
    }

    if (role < BASE_ROLE_MEMBER) role = BASE_ROLE_MEMBER;
    if (role > BASE_ROLE_OFFICER) role = BASE_ROLE_OFFICER;   /* owner tidak bisa dibagi */

    new query[256];
    mysql_format(gSQL, query, sizeof(query),
        "INSERT INTO base_members (base_id, char_id, role, added_by, added_at) \
         VALUES (%d, %d, %d, %d, %d) \
         ON DUPLICATE KEY UPDATE role = VALUES(role), added_by = VALUES(added_by)",
        baseId, ActiveCharId[targetId], role, ActiveCharId[actorId], gettime());
    mysql_tquery(gSQL, query, "", "");

    BaseMemberBase[targetId] = baseId;
    BaseMemberRole[targetId] = role;

    new actorName[MAX_PLAYER_NAME + 1], targetName[MAX_PLAYER_NAME + 1], line[176];
    GetPlayerName(actorId, actorName, sizeof(actorName));
    GetPlayerName(targetId, targetName, sizeof(targetName));
    format(line, sizeof(line), "[BASE] %s menambahkan %s sebagai %s.",
        actorName, targetName, (role == BASE_ROLE_OFFICER) ? "officer" : "member");
    SendClientMessage(actorId, UI_ACCENT_PRIMARY, line);
    SendClientMessage(targetId, UI_ACCENT_PRIMARY, line);

    AdminAudit_Log(actorId, "base_member_add", AccountId[targetId], line);
    return 1;
}

stock BaseMembers_Remove(actorId, targetId)
{
    if (!IsPlayerConnected(actorId) || !IsPlayerConnected(targetId)) return 0;

    new baseId = BaseMemberBase[actorId];
    if (baseId <= 0 || BaseMemberBase[targetId] != baseId) return 0;

    if (!BaseMembers_HasRole(actorId, baseId, BASE_ROLE_OFFICER)
        && actorId != targetId)
    {
        SendClientMessage(actorId, UI_ACCENT_WARN, "[BASE] Hanya pemilik/officer yang bisa mengeluarkan anggota.");
        return 0;
    }

    if (BaseMemberRole[targetId] == BASE_ROLE_OWNER)
    {
        SendClientMessage(actorId, UI_ACCENT_WARN, "[BASE] Pemilik base tidak bisa dikeluarkan.");
        return 0;
    }

    new query[160];
    mysql_format(gSQL, query, sizeof(query),
        "DELETE FROM base_members WHERE base_id = %d AND char_id = %d",
        baseId, ActiveCharId[targetId]);
    mysql_tquery(gSQL, query, "", "");

    BaseMembers_ResetPlayer(targetId);

    new line[176];
    new targetName[MAX_PLAYER_NAME + 1];
    GetPlayerName(targetId, targetName, sizeof(targetName));
    format(line, sizeof(line), "[BASE] %s dikeluarkan dari base.", targetName);
    SendClientMessage(actorId, UI_ACCENT_PRIMARY, line);
    SendClientMessage(targetId, UI_ACCENT_WARN, line);

    AdminAudit_Log(actorId, "base_member_remove", AccountId[targetId], line);
    return 1;
}

stock BaseMembers_Show(playerid)
{
    new baseId = BaseMemberBase[playerid];
    if (baseId <= 0)
    {
        SendClientMessage(playerid, UI_TEXT_MUTED, "[BASE] Kamu belum punya / ikut base. /base untuk membuat.");
        return 0;
    }

    new query[224];
    mysql_format(gSQL, query, sizeof(query),
        "SELECT c.name, bm.role FROM base_members bm \
         JOIN characters c ON c.id = bm.char_id \
         WHERE bm.base_id = %d ORDER BY bm.role DESC, c.name LIMIT %d",
        baseId, BASE_MEMBERS_MAX);
    mysql_pquery(gSQL, query, "BaseMembers_OnListLoaded", "i", playerid);
    return 1;
}

forward BaseMembers_OnListLoaded(playerid);

public BaseMembers_OnListLoaded(playerid)
{
    if (!IsPlayerConnected(playerid)) return 0;
    if (mysql_errno(gSQL) != 0) return 0;

    new line[128];
    format(line, sizeof(line), "[BASE] Anggota base (kamu: %s):",
        (BaseMemberRole[playerid] == BASE_ROLE_OWNER) ? "owner"
        : (BaseMemberRole[playerid] == BASE_ROLE_OFFICER) ? "officer" : "member");
    SendClientMessage(playerid, UI_ACCENT_PRIMARY, line);

    new rows = cache_num_rows();
    new memberName[25], role;
    for (new row = 0; row < rows; row++)
    {
        cache_get_value_name(row, "name", memberName, sizeof(memberName));
        cache_get_value_name_int(row, "role", role);
        format(line, sizeof(line), "  %s — %s", memberName,
            (role == BASE_ROLE_OWNER) ? "owner" : (role == BASE_ROLE_OFFICER) ? "officer" : "member");
        SendClientMessage(playerid, UI_TEXT_SECONDARY, line);
    }

    if (rows == 0)
    {
        SendClientMessage(playerid, UI_TEXT_MUTED, "  (belum ada anggota; /base add [playerid])");
    }
    return 1;
}
