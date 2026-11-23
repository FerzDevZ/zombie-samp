#if defined _ls_party_included
    #endinput
#endif
#define _ls_party_included

/* Party co-op: satu ketua, maksimal 4 anggota. Efek nyata yang dijamin:
   - damage antar anggota tidak pernah masuk (friendly fire mati),
   - 20% XP pemain dibagikan ke anggota lain yang sedekat <=60 m di lobby sama,
   - panel /party menampilkan HP + jarak + status tumbang anggota.
   Stash sengaja TIDAK dibagi: StashContents di-key playerid, jadi berbagi peti
   butuh kontainer bersama terpisah + migrasi, bukan sekadar flag. */

#define PARTY_MAX_MEMBERS        4
#define PARTY_XP_SHARE_RADIUS    60.0
#define PARTY_XP_SHARE_PERCENT   20
#define PARTY_INVITE_TTL_MS      30000
#define PARTY_INVITE_RANGE       8.0
#define DIALOG_PARTY             3201
#define DIALOG_PARTY_INVITE      3202

new PartyLeader[MAX_PLAYERS];
new bool:PartyMember[MAX_PLAYERS];
new PartyInviteFrom[MAX_PLAYERS];
new PartyInviteTick[MAX_PLAYERS];
new bool:PartyXpGuard = false;

/* Snapshot isi panel: kalau anggota keluar/masuk saat dialog terbuka, index aksi
   tidak boleh bergeser (klik "Undang" tidak boleh jadi "Keluar"). */
new PartyPanelRows[MAX_PLAYERS];
new PartyPanelMember[MAX_PLAYERS][PARTY_MAX_MEMBERS];

stock Party_InitAll()
{
    for (new playerid = 0; playerid < MAX_PLAYERS; playerid++)
    {
        PartyLeader[playerid] = INVALID_PLAYER_ID;
        PartyMember[playerid] = false;
        PartyInviteFrom[playerid] = INVALID_PLAYER_ID;
        PartyInviteTick[playerid] = 0;
        PartyPanelRows[playerid] = 0;
        for (new index = 0; index < PARTY_MAX_MEMBERS; index++)
        {
            PartyPanelMember[playerid][index] = INVALID_PLAYER_ID;
        }
    }
    return 1;
}

stock bool:Party_InParty(playerid)
{
    if (playerid < 0 || playerid >= MAX_PLAYERS) return false;
    return PartyMember[playerid];
}

stock bool:Party_IsLeader(playerid)
{
    if (!Party_InParty(playerid)) return false;
    return PartyLeader[playerid] == playerid;
}

stock Party_LeaderOf(playerid)
{
    if (!Party_InParty(playerid)) return INVALID_PLAYER_ID;
    return PartyLeader[playerid];
}

stock Party_Count(leader)
{
    new count = 0;
    for (new member = 0; member < MAX_PLAYERS; member++)
    {
        if (!PartyMember[member]) continue;
        if (PartyLeader[member] != leader) continue;
        if (!IsPlayerConnected(member)) continue;
        count++;
    }
    return count;
}

/* Slot pemain ke-index pada urutan body dialog, dipakai untuk baris anggota. */
stock Party_MemberSlotAt(leader, index)
{
    new seen = 0;
    for (new member = 0; member < MAX_PLAYERS; member++)
    {
        if (!PartyMember[member]) continue;
        if (PartyLeader[member] != leader) continue;
        if (!IsPlayerConnected(member)) continue;

        if (seen == index) return member;
        seen++;
    }
    return INVALID_PLAYER_ID;
}

stock bool:Party_IsSameParty(first, second)
{
    if (first == second) return false;
    if (!Party_InParty(first) || !Party_InParty(second)) return false;
    return PartyLeader[first] == PartyLeader[second];
}

/* Marker anggota party di radar (ROADMAP: "butuh slot blip tambahan"; ternyata
   tidak perlu blip — SetPlayerMarkerForPlayer sudah mewarnai titik radar dan
   nametag sekaligus). Diterapkan hanya saat keanggotaan berubah, bukan per tick. */
#define PARTY_MARKER_COLOR 0x47C46AFF

stock Party_ApplyMarkersFor(playerid)
{
    if (!IsPlayerConnected(playerid)) return 0;

    foreach (new other : PlayerPool)
    {
        if (other == playerid) continue;
        if (!IsPlayerConnected(other)) continue;

        if (Party_IsSameParty(playerid, other))
        {
            SetPlayerMarkerForPlayer(playerid, other, PARTY_MARKER_COLOR);
        }
        else
        {
            /* Kembalikan ke warna asli pemain, bukan warna paksa: tanpa ini
               marker tetap hijau setelah keluar party. */
            SetPlayerMarkerForPlayer(playerid, other, GetPlayerColor(other));
        }
    }
    return 1;
}

stock Party_RefreshMarkers()
{
    foreach (new playerid : PlayerPool)
    {
        if (!IsPlayerConnected(playerid)) continue;
        if (IsPlayerNPC(playerid)) continue;
        Party_ApplyMarkersFor(playerid);
    }
    return 1;
}


stock Party_Notify(leader, const message[], color)
{
    for (new member = 0; member < MAX_PLAYERS; member++)
    {
        if (!PartyMember[member]) continue;
        if (PartyLeader[member] != leader) continue;
        if (!IsPlayerConnected(member)) continue;
        SendClientMessage(member, color, message);
    }
    return 1;
}

stock Party_Reset(playerid)
{
    if (playerid < 0 || playerid >= MAX_PLAYERS) return 0;

    PartyLeader[playerid] = INVALID_PLAYER_ID;
    PartyMember[playerid] = false;
    PartyInviteFrom[playerid] = INVALID_PLAYER_ID;
    PartyInviteTick[playerid] = 0;
    Party_RefreshMarkers();
    return 1;
}

stock Party_Disband(leader, const reason[])
{
    if (leader < 0 || leader >= MAX_PLAYERS) return 0;

    new message[144];
    format(message, sizeof(message), "[PARTY] Party bubar: %s", reason);

    for (new member = 0; member < MAX_PLAYERS; member++)
    {
        if (!PartyMember[member]) continue;
        if (PartyLeader[member] != leader) continue;

        Party_Reset(member);
        if (IsPlayerConnected(member)) SendClientMessage(member, UI_ACCENT_WARN, message);
    }
    return 1;
}

stock Party_Leave(playerid, const reason[])
{
    if (!Party_InParty(playerid)) return 0;

    new leader = PartyLeader[playerid];

    if (leader == playerid)
    {
        Party_Disband(playerid, reason);
        Party_Reset(playerid);
        return 1;
    }

    Party_Reset(playerid);
    SendClientMessage(playerid, UI_ACCENT_WARN, "[PARTY] Kamu keluar dari party.");

    if (IsPlayerConnected(leader))
    {
        new memberName[MAX_PLAYER_NAME];
        GetPlayerName(playerid, memberName, sizeof(memberName));

        new message[144];
        format(message, sizeof(message), "[PARTY] %s keluar dari party (%s).", memberName, reason);
        Party_Notify(leader, message, UI_ACCENT_WARN);
    }
    return 1;
}

stock Party_Create(playerid)
{
    if (!Account_IsPlaying(playerid))
    {
        SendClientMessage(playerid, UI_ACCENT_WARN, "[PARTY] Login dan masuk karakter dulu.");
        return 0;
    }

    if (Party_InParty(playerid))
    {
        SendClientMessage(playerid, UI_ACCENT_WARN, "[PARTY] Kamu sudah di party. Ketik /party.");
        return 0;
    }

    PartyMember[playerid] = true;
    PartyLeader[playerid] = playerid;
    Party_RefreshMarkers();

    SendClientMessage(playerid, UI_ACCENT_PRIMARY,
        "[PARTY] Party dibuat. Undang anggota dengan /invite [playerid] atau /party.");
    return 1;
}

stock Party_Show(playerid)
{
    if (!Party_InParty(playerid))
    {
        ShowPlayerDialog(playerid, DIALOG_PARTY, DIALOG_STYLE_LIST, "Party (belum ikut)",
            "Buat party baru\n",
            "Pilih", "Tutup");
        return 1;
    }

    new leader = PartyLeader[playerid];
    new count = Party_Count(leader);

    new Float:px, Float:py, Float:pz;
    GetPlayerPos(playerid, px, py, pz);

    /* DIALOG_STYLE_LIST: setiap baris bisa dipilih, jadi header TIDAK boleh masuk body
       (kalau masuk, index aksi bergeser). Info ringkas ditaruh di judul dialog. */
    new body[1024], line[128], memberName[MAX_PLAYER_NAME];
    body[0] = EOS;

    new rowIndex = 0;
    for (new index = 0; index < PARTY_MAX_MEMBERS; index++)
    {
        PartyPanelMember[playerid][index] = INVALID_PLAYER_ID;
    }

    for (new member = 0; member < MAX_PLAYERS; member++)
    {
        if (!PartyMember[member]) continue;
        if (PartyLeader[member] != leader) continue;
        if (!IsPlayerConnected(member)) continue;

        GetPlayerName(member, memberName, sizeof(memberName));

        if (Player_IsDowned(member))
        {
            format(line, sizeof(line), "%s%s | TUMBANG\n",
                (member == leader) ? "[KETUA] " : "", memberName);
        }
        else
        {
            new Float:mx, Float:my, Float:mz;
            GetPlayerPos(member, mx, my, mz);
            new Float:distance = VectorSize(mx - px, my - py, mz - pz);

            format(line, sizeof(line), "%s%s | %.0f HP | %.0f m\n",
                (member == leader) ? "[KETUA] " : "", memberName,
                PlayerSurvival[member][psHealth], distance);
        }

        strcat(body, line, sizeof(body));

        if (rowIndex < PARTY_MAX_MEMBERS) PartyPanelMember[playerid][rowIndex] = member;
        rowIndex++;
    }

    PartyPanelRows[playerid] = rowIndex;

    format(line, sizeof(line), "Undang pemain terdekat\nKeluar dari party\nBubarkan party (ketua)\n");
    strcat(body, line, sizeof(body));

    new title[64];
    format(title, sizeof(title), "Party %d/%d | XP %d%% dalam %.0f m",
        count, PARTY_MAX_MEMBERS, PARTY_XP_SHARE_PERCENT, PARTY_XP_SHARE_RADIUS);
    ShowPlayerDialog(playerid, DIALOG_PARTY, DIALOG_STYLE_LIST, title, body, "Pilih", "Tutup");
    return 1;
}

stock Party_Invite(playerid, target)
{
    if (!Party_InParty(playerid))
    {
        SendClientMessage(playerid, UI_ACCENT_WARN, "[PARTY] Buat party dulu: /party.");
        return 0;
    }

    if (!Party_IsLeader(playerid))
    {
        SendClientMessage(playerid, UI_ACCENT_WARN, "[PARTY] Hanya ketua party yang boleh mengundang.");
        return 0;
    }

    if (target == INVALID_PLAYER_ID || target == playerid || !IsPlayerConnected(target))
    {
        SendClientMessage(playerid, UI_ACCENT_WARN, "[PARTY] Pemain tidak valid.");
        return 0;
    }

    if (!Account_IsPlaying(target))
    {
        SendClientMessage(playerid, UI_ACCENT_WARN, "[PARTY] Pemain itu belum masuk karakter.");
        return 0;
    }

    if (Party_InParty(target))
    {
        SendClientMessage(playerid, UI_ACCENT_WARN, "[PARTY] Pemain itu sudah punya party.");
        return 0;
    }

    if (Party_Count(playerid) >= PARTY_MAX_MEMBERS)
    {
        SendClientMessage(playerid, UI_ACCENT_WARN, "[PARTY] Party penuh (maksimal 4).");
        return 0;
    }

    /* Undangan id eksplisit wajib satu lobby dan dekat, sama seperti undangan
       terdekat: tanpa ini party bisa lintas lobby (XP share & peti jadi kacau). */
    if (Lobby_Of(target) != Lobby_Of(playerid))
    {
        SendClientMessage(playerid, UI_ACCENT_WARN, "[PARTY] Pemain itu berada di lobby berbeda.");
        return 0;
    }
    new Float:inviteX, Float:inviteY, Float:inviteZ, Float:targetX, Float:targetY, Float:targetZ;
    GetPlayerPos(playerid, inviteX, inviteY, inviteZ);
    GetPlayerPos(target, targetX, targetY, targetZ);
    if (VectorSize(targetX - inviteX, targetY - inviteY, targetZ - inviteZ) > PARTY_INVITE_RANGE)
    {
        SendClientMessage(playerid, UI_ACCENT_WARN, "[PARTY] Pemain itu terlalu jauh untuk diundang.");
        return 0;
    }

    PartyInviteFrom[target] = playerid;
    PartyInviteTick[target] = GetTickCount();

    new leaderName[MAX_PLAYER_NAME];
    GetPlayerName(playerid, leaderName, sizeof(leaderName));

    new body[160];
    format(body, sizeof(body), "%s mengundangmu ke party. Terima?", leaderName);
    ShowPlayerDialog(target, DIALOG_PARTY_INVITE, DIALOG_STYLE_MSGBOX, "Undangan party", body,
        "Terima", "Tolak");

    new message[144];
    format(message, sizeof(message), "[PARTY] Undangan dikirim ke %d.", target);
    SendClientMessage(playerid, UI_ACCENT_PRIMARY, message);
    return 1;
}

stock Party_InviteNearest(playerid)
{
    if (!Party_InParty(playerid))
    {
        SendClientMessage(playerid, UI_ACCENT_WARN, "[PARTY] Buat party dulu: /party.");
        return 0;
    }

    if (!Party_IsLeader(playerid))
    {
        SendClientMessage(playerid, UI_ACCENT_WARN, "[PARTY] Hanya ketua party yang boleh mengundang.");
        return 0;
    }

    new Float:px, Float:py, Float:pz;
    GetPlayerPos(playerid, px, py, pz);

    new bestTarget = INVALID_PLAYER_ID;
    new Float:bestDistance = PARTY_INVITE_RANGE;

    foreach (new candidate : PlayerPool)
    {
        if (candidate == playerid) continue;
        if (!Account_IsPlaying(candidate)) continue;
        if (Party_InParty(candidate)) continue;
        if (Lobby_Of(candidate) != Lobby_Of(playerid)) continue;

        new Float:cx, Float:cy, Float:cz;
        GetPlayerPos(candidate, cx, cy, cz);
        new Float:distance = VectorSize(cx - px, cy - py, cz - pz);
        if (distance > bestDistance) continue;

        bestDistance = distance;
        bestTarget = candidate;
    }

    if (bestTarget == INVALID_PLAYER_ID)
    {
        SendClientMessage(playerid, UI_ACCENT_WARN, "[PARTY] Tidak ada pemain bebas party dalam 8 m.");
        return 0;
    }

    return Party_Invite(playerid, bestTarget);
}

stock Party_Accept(playerid)
{
    new leader = PartyInviteFrom[playerid];

    if (leader == INVALID_PLAYER_ID
        || GetTickCount() - PartyInviteTick[playerid] > PARTY_INVITE_TTL_MS
        || !IsPlayerConnected(leader)
        || !Party_IsLeader(leader))
    {
        PartyInviteFrom[playerid] = INVALID_PLAYER_ID;
        SendClientMessage(playerid, UI_ACCENT_WARN, "[PARTY] Undangan sudah kedaluwarsa.");
        return 0;
    }

    if (Party_InParty(playerid))
    {
        SendClientMessage(playerid, UI_ACCENT_WARN, "[PARTY] Kamu sudah punya party.");
        return 0;
    }

    if (Party_Count(leader) >= PARTY_MAX_MEMBERS)
    {
        PartyInviteFrom[playerid] = INVALID_PLAYER_ID;
        SendClientMessage(playerid, UI_ACCENT_WARN, "[PARTY] Party tujuan sudah penuh.");
        return 0;
    }

    PartyMember[playerid] = true;
    PartyLeader[playerid] = leader;
    PartyInviteFrom[playerid] = INVALID_PLAYER_ID;

    new memberName[MAX_PLAYER_NAME];
    GetPlayerName(playerid, memberName, sizeof(memberName));

    new message[144];
    format(message, sizeof(message), "[PARTY] %s bergabung.", memberName);
    Party_Notify(leader, message, UI_ACCENT_PRIMARY);
    return 1;
}

stock Party_Kick(playerid, target)
{
    if (!Party_IsLeader(playerid))
    {
        SendClientMessage(playerid, UI_ACCENT_WARN, "[PARTY] Hanya ketua yang boleh menendang.");
        return 0;
    }

    if (!Party_IsSameParty(playerid, target))
    {
        SendClientMessage(playerid, UI_ACCENT_WARN, "[PARTY] Pemain itu bukan anggota party-mu.");
        return 0;
    }

    new targetName[MAX_PLAYER_NAME];
    GetPlayerName(target, targetName, sizeof(targetName));

    Party_Reset(target);
    SendClientMessage(target, UI_ACCENT_WARN, "[PARTY] Kamu dikeluarkan dari party.");

    new message[144];
    format(message, sizeof(message), "[PARTY] %s dikeluarkan.", targetName);
    Party_Notify(playerid, message, UI_ACCENT_WARN);
    return 1;
}

/* XP dibagi ke anggota lain yang dekat. Guard mencegah rekursi tak terbatas:
   Skills_AddXp -> Party_ShareXp -> Skills_AddXp anggota. */
stock Party_ShareXp(playerid, amount)
{
    if (PartyXpGuard) return 0;
    if (amount < 5) return 0;
    if (!Party_InParty(playerid)) return 0;

    new leader = PartyLeader[playerid];

    new Float:px, Float:py, Float:pz;
    GetPlayerPos(playerid, px, py, pz);

    new shared = 0;
    PartyXpGuard = true;

    for (new member = 0; member < MAX_PLAYERS; member++)
    {
        if (member == playerid) continue;
        if (!PartyMember[member]) continue;
        if (PartyLeader[member] != leader) continue;
        if (!IsPlayerConnected(member)) continue;
        if (!Player_IsAlive(member)) continue;
        if (Lobby_Of(member) != Lobby_Of(playerid)) continue;

        new Float:mx, Float:my, Float:mz;
        GetPlayerPos(member, mx, my, mz);
        if (VectorSize(mx - px, my - py, mz - pz) > PARTY_XP_SHARE_RADIUS) continue;

        new share = (amount * PARTY_XP_SHARE_PERCENT) / 100;
        if (share < 1) share = 1;

        Skills_AddXp(member, share);
        shared++;
    }

    PartyXpGuard = false;
    return shared;
}

stock Party_OnDisconnect(playerid)
{
    if (!Party_InParty(playerid)) return Party_Reset(playerid);

    Party_Leave(playerid, "keluar dari server");
    return Party_Reset(playerid);
}

stock Party_OnDialogResponse(playerid, dialogid, response, listitem, const inputtext[])
{
    #pragma unused inputtext

    if (dialogid == DIALOG_PARTY_INVITE)
    {
        if (response) Party_Accept(playerid);
        else
        {
            PartyInviteFrom[playerid] = INVALID_PLAYER_ID;
            SendClientMessage(playerid, UI_TEXT_SECONDARY, "[PARTY] Undangan ditolak.");
        }
        return 1;
    }

    if (dialogid != DIALOG_PARTY) return 0;
    if (!response) return 1;

    if (!Party_InParty(playerid))
    {
        if (listitem == 0) Party_Create(playerid);
        return 1;
    }

    new rows = PartyPanelRows[playerid];

    if (listitem < rows)
    {
        /* Baris anggota: ketua bisa menendang langsung dari panel. Target diambil dari
           snapshot saat dialog dibuat, bukan dari urutan yang bisa berubah. */
        if (!Party_IsLeader(playerid)) return 1;

        new memberSlot = PartyPanelMember[playerid][listitem];
        if (memberSlot == INVALID_PLAYER_ID || memberSlot == playerid) return 1;
        if (!IsPlayerConnected(memberSlot)) return 1;

        Party_Kick(playerid, memberSlot);
        return 1;
    }

    if (listitem == rows)
    {
        Party_InviteNearest(playerid);
        return 1;
    }

    if (listitem == rows + 1)
    {
        Party_Leave(playerid, "memilih keluar");
        return 1;
    }

    if (listitem == rows + 2)
    {
        if (!Party_IsLeader(playerid))
        {
            SendClientMessage(playerid, UI_ACCENT_WARN, "[PARTY] Hanya ketua yang boleh membubarkan.");
            return 1;
        }
        Party_Disband(playerid, "dibubarkan ketua");
        return 1;
    }
    return 1;
}
