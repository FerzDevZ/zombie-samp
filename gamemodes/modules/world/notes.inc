#if defined _ls_notes_included
    #endinput
#endif
#define _ls_notes_included

/* Catatan Kertas & Papan Pengumuman (fitur #34).

   Alur pemain:
     1. Dapat item Kertas dari loot node dunia.
     2. /tulis  -> dialog input, 1 kertas terpakai, pesan dibawa (draft 10 menit).
     3. /tempel -> dekat papan pengumuman, pesan jadi 3D text di papan + tersimpan DB.
     4. /baca   -> dekat papan, daftar pesan di papan itu, pilih untuk membaca.

   Papan berdiri di 4 zona (hub + 3 kota), objek dunia global (worldid -1).
   Pesan lapuk: kedaluwarsa 7 hari, dibersihkan saat boot. Maks 12 pesan per
   papan; yang tertua tergusur. */

#define NOTES_BOARD_COUNT   4
#define NOTES_PER_BOARD     12
#define NOTES_MAX           48      /* NOTES_BOARD_COUNT * NOTES_PER_BOARD */
#define NOTES_BODY_MAX      96
#define NOTES_AUTHOR_MAX    24
#define NOTES_EXPIRE_S      (7 * 24 * 3600)
#define NOTES_DRAFT_S       600     /* draft kertas kedaluwarsa 10 menit */
#define NOTES_COOLDOWN_MS   60000   /* anti-spam tempel per pemain */
#define NOTES_RANGE         3.5
#define NOTES_MIN_LEN       3
#define NOTES_BOARD_MODEL   2611    /* cork board / papan pengumuman */

#define DIALOG_NOTES_WRITE  2959
#define DIALOG_NOTES_LIST   2960
#define DIALOG_NOTES_READ   2961

enum E_NOTE
{
    nbDbId,
    nbBoard,
    nbChar,
    nbAuthor[NOTES_AUTHOR_MAX],
    nbBody[NOTES_BODY_MAX + 1],
    nbCreatedAt,
    STREAMER_TAG_3D_TEXT_LABEL:nbLabel
}

enum E_BOARD
{
    Float:bx,
    Float:by,
    Float:bz,
    bZoneName[MAX_ZONE_NAME],
    bObject
}

new gNotes[NOTES_MAX][E_NOTE];
new gBoards[NOTES_BOARD_COUNT][E_BOARD];
new gNoteDraft[MAX_PLAYERS][NOTES_BODY_MAX + 1];
new gNoteDraftUntil[MAX_PLAYERS];
new gNoteLastStick[MAX_PLAYERS];
new gNoteBoardSel[MAX_PLAYERS];

forward Notes_OnLoaded();
forward Notes_OnInserted(playerid);

/* ------------------------------------------------------------- sanitasi */

/* Buang karakter berbahaya untuk embed warna (~) dan kontrol; sisanya lolos.
   Murni string -> bisa diuji selftest tanpa dunia. */
stock Notes_Sanitize(const src[], dest[], size)
{
    dest[0] = EOS;
    new out = 0;
    for (new i = 0; src[i] != EOS && out < size - 1; i++)
    {
        new ch = src[i];
        if (ch < 32 || ch > 126) ch = ' ';
        if (ch == '~') ch = '-';
        dest[out++] = ch;
    }
    dest[out] = EOS;

    /* buang spasi di ujung */
    new len = strlen(dest);
    while (len > 0 && dest[len - 1] == ' ') dest[--len] = EOS;
    return 1;
}

/* ------------------------------------------------------------- papan */

stock Notes_BoardName(boardid, dest[], size)
{
    if (boardid < 0 || boardid >= NOTES_BOARD_COUNT)
    {
        strcpy(dest, "Papan", size);
        return 0;
    }
    strcpy(dest, gBoards[boardid][bZoneName], size);
    return 1;
}

stock Notes_NearestBoard(playerid, Float:range)
{
    new Float:px, Float:py, Float:pz;
    GetPlayerPos(playerid, px, py, pz);

    new best = -1;
    new Float:bestDist = range;
    for (new b = 0; b < NOTES_BOARD_COUNT; b++)
    {
        new Float:d = VectorSize(gBoards[b][bx] - px, gBoards[b][by] - py, gBoards[b][bz] - pz);
        if (d < bestDist)
        {
            bestDist = d;
            best = b;
        }
    }
    return best;
}

stock Notes_CountOnBoard(boardid)
{
    new count = 0;
    for (new i = 0; i < NOTES_MAX; i++)
    {
        if (gNotes[i][nbDbId] == 0) continue;
        if (gNotes[i][nbBoard] == boardid) count++;
    }
    return count;
}

stock Notes_FindOldestOnBoard(boardid)
{
    new oldest = -1;
    new oldestAt = 0x7FFFFFFF;
    for (new i = 0; i < NOTES_MAX; i++)
    {
        if (gNotes[i][nbDbId] == 0) continue;
        if (gNotes[i][nbBoard] != boardid) continue;
        if (gNotes[i][nbCreatedAt] < oldestAt)
        {
            oldestAt = gNotes[i][nbCreatedAt];
            oldest = i;
        }
    }
    return oldest;
}

stock Notes_FreeSlot()
{
    for (new i = 0; i < NOTES_MAX; i++)
    {
        if (gNotes[i][nbDbId] == 0) return i;
    }
    return -1;
}

/* ------------------------------------------------------------ label 3D */

stock Notes_DestroyLabel(index)
{
    if (index < 0 || index >= NOTES_MAX) return 0;
    if (gNotes[index][nbLabel] != STREAMER_TAG_3D_TEXT_LABEL:INVALID_3DTEXT_ID)
    {
        DestroyDynamic3DTextLabel(gNotes[index][nbLabel]);
        gNotes[index][nbLabel] = STREAMER_TAG_3D_TEXT_LABEL:INVALID_3DTEXT_ID;
    }
    return 1;
}

stock Notes_CreateLabel(index)
{
    if (index < 0 || index >= NOTES_MAX) return 0;
    Notes_DestroyLabel(index);

    new board = gNotes[index][nbBoard];
    new slot = 0;
    for (new i = 0; i < index; i++)
    {
        if (gNotes[i][nbDbId] != 0 && gNotes[i][nbBoard] == board) slot++;
    }

    /* sebar di permukaan papan: 3 kolom x 4 baris */
    new Float:ox = ((slot % 3) - 1) * 0.45;
    new Float:oz = 1.55 - ((slot / 3) * 0.38);

    new label[160];
    format(label, sizeof(label), "{F5F5F5}%s\n{A8B2C1}%s", gNotes[index][nbBody], gNotes[index][nbAuthor]);
    gNotes[index][nbLabel] = CreateDynamic3DTextLabel(label, -1,
        gBoards[board][bx] + ox, gBoards[board][by], gBoards[board][bz] + oz,
        6.0, .worldid = -1, .testlos = 0);
    return 1;
}

/* ---------------------------------------------------------------- boot */

stock Notes_Init()
{
    /* Papan: pusat 4 zona pemain. Z digenggam tanah lewat ColAndreas. */
    new const boardZones[NOTES_BOARD_COUNT] = { 0, 1, 2, 3 };  /* indeks gZones */
    for (new b = 0; b < NOTES_BOARD_COUNT; b++)
    {
        new zone = boardZones[b];
        new Float:x = gZones[zone][zoneX] + 12.0;
        new Float:y = gZones[zone][zoneY] + 12.0;
        new Float:z = gZones[zone][zoneZ] + 30.0;

        new Float:hitX, Float:hitY, Float:hitZ;
        if (CA_RayCastLine(x, y, z, x, y, z - 60.0, hitX, hitY, hitZ) != 0)
        {
            x = hitX;
            y = hitY;
            z = hitZ;
        }
        gBoards[b][bx] = x;
        gBoards[b][by] = y;
        gBoards[b][bz] = z;
        Zone_GetName(zone, gBoards[b][bZoneName], MAX_ZONE_NAME);
        gBoards[b][bObject] = CreateDynamicObject(NOTES_BOARD_MODEL, x, y, z + 1.0, 0.0, 0.0, 0.0, .worldid = -1);

        new sign[96];
        format(sign, sizeof(sign), "{10B981}PAPAN PENGUMUMAN\n{A8B2C1}%s\n{F5F5F5}/tulis lalu /tempel  |  /baca", gBoards[b][bZoneName]);
        CreateDynamic3DTextLabel(sign, -1, x, y, z + 2.2, 14.0, .worldid = -1, .testlos = 0);
    }

    for (new i = 0; i < NOTES_MAX; i++)
    {
        gNotes[i][nbDbId] = 0;
        gNotes[i][nbLabel] = STREAMER_TAG_3D_TEXT_LABEL:INVALID_3DTEXT_ID;
    }

    if (!SQL_IsReady()) return 0;

    new now = gettime();
    new cutoff = now - NOTES_EXPIRE_S;
    new serverId = Server_InstanceId();

    new query[192];
    mysql_format(gSQL, query, sizeof(query),
        "DELETE FROM notes WHERE server_id = %d AND created_at < %d",
        serverId, cutoff);
    mysql_tquery(gSQL, query, "", "");

    mysql_format(gSQL, query, sizeof(query),
        "SELECT id, board_id, char_id, author, body, created_at FROM notes \
         WHERE server_id = %d AND created_at >= %d ORDER BY created_at ASC LIMIT %d",
        serverId, cutoff, NOTES_MAX);
    mysql_tquery(gSQL, query, "Notes_OnLoaded", "");
    return 1;
}

public Notes_OnLoaded()
{
    new rows = cache_num_rows();
    for (new row = 0; row < rows && row < NOTES_MAX; row++)
    {
        new index = Notes_FreeSlot();
        if (index < 0) break;

        cache_get_value_name_int(row, "id", gNotes[index][nbDbId]);
        cache_get_value_name_int(row, "board_id", gNotes[index][nbBoard]);
        cache_get_value_name_int(row, "char_id", gNotes[index][nbChar]);
        cache_get_value_name(row, "author", gNotes[index][nbAuthor], NOTES_AUTHOR_MAX);
        cache_get_value_name(row, "body", gNotes[index][nbBody], NOTES_BODY_MAX + 1);
        cache_get_value_name_int(row, "created_at", gNotes[index][nbCreatedAt]);
        gNotes[index][nbLabel] = STREAMER_TAG_3D_TEXT_LABEL:INVALID_3DTEXT_ID;

        if (gNotes[index][nbBoard] < 0 || gNotes[index][nbBoard] >= NOTES_BOARD_COUNT)
        {
            gNotes[index][nbDbId] = 0;
            continue;
        }
        Notes_CreateLabel(index);
    }
    Log_Format(LOG_INFO, "[NOTES] %d pesan dimuat dari database.", rows);
    return 1;
}

/* ------------------------------------------------------------- /tulis */

stock Notes_StartWrite(playerid)
{
    if (!Account_IsPlaying(playerid)) return 0;
    if (gNoteDraftUntil[playerid] != 0)
    {
        Toast_Show(playerid, "KERTAS", "Kamu masih membawa kertas tertulis. /tempel dulu.", UI_ACCENT_WARN);
        return 0;
    }
    if (Inventory_CountItem(playerid, ITEM_PAPER) < 1)
    {
        SendClientMessage(playerid, UI_ACCENT_WARN, "[TULIS] Butuh 1 Kertas. Cari di peti-peti kota.");
        return 0;
    }

    ShowPlayerDialog(playerid, DIALOG_NOTES_WRITE, DIALOG_STYLE_INPUT,
        "Menulis Pesan (maks 96 huruf)",
        "Tulis pesanmu untuk penyintas lain:\nperingatan bahaya, info barter, atau kabar.",
        "Tulis", "Batal");
    return 1;
}

stock Notes_OnWriteInput(playerid, const inputtext[])
{
    new body[NOTES_BODY_MAX + 1];
    Notes_Sanitize(inputtext, body, sizeof(body));

    if (strlen(body) < NOTES_MIN_LEN)
    {
        Toast_Show(playerid, "KERTAS", "Terlalu pendek. Tulis sesuatu yang berguna.", UI_ACCENT_WARN);
        return 0;
    }
    if (Inventory_CountItem(playerid, ITEM_PAPER) < 1)
    {
        Toast_Show(playerid, "KERTAS", "Kertasnya hilang sebelum selesai ditulis.", UI_ACCENT_WARN);
        return 0;
    }

    Inventory_Consume(playerid, ITEM_PAPER, 1, "tulis pesan");
    strcpy(gNoteDraft[playerid], body, NOTES_BODY_MAX + 1);
    gNoteDraftUntil[playerid] = gettime() + NOTES_DRAFT_S;

    Toast_Show(playerid, "KERTAS TERTULIS", "Dekati papan pengumuman lalu /tempel (10 menit).", UI_ACCENT_PRIMARY);
    Log_Format(LOG_INFO, "[NOTES] char=%d menulis draft %d huruf.", ActiveCharId[playerid], strlen(body));
    return 1;
}

/* ------------------------------------------------------------- /tempel */

stock Notes_Stick(playerid)
{
    if (!Account_IsPlaying(playerid)) return 0;
    if (gNoteDraftUntil[playerid] == 0)
    {
        SendClientMessage(playerid, UI_TEXT_MUTED, "[TEMPEL] Kamu tidak membawa kertas tertulis. /tulis dulu.");
        return 0;
    }
    if (gettime() > gNoteDraftUntil[playerid])
    {
        gNoteDraft[playerid][0] = EOS;
        gNoteDraftUntil[playerid] = 0;
        SendClientMessage(playerid, UI_ACCENT_WARN, "[TEMPEL] Kertasnya robek di tangan. Tulis lagi.");
        return 0;
    }
    if (GetTickCount() - gNoteLastStick[playerid] < NOTES_COOLDOWN_MS)
    {
        SendClientMessage(playerid, UI_ACCENT_WARN, "[TEMPEL] Tanganmu masih lelah menempel. Tunggu sebentar.");
        return 0;
    }

    new board = Notes_NearestBoard(playerid, NOTES_RANGE);
    if (board < 0)
    {
        SendClientMessage(playerid, UI_TEXT_MUTED, "[TEMPEL] Tidak ada papan pengumuman di dekatmu.");
        return 0;
    }

    /* Papan penuh: pesan tertua tergusur (lapuk lebih dulu). */
    if (Notes_CountOnBoard(board) >= NOTES_PER_BOARD)
    {
        new old = Notes_FindOldestOnBoard(board);
        if (old >= 0)
        {
            new query[128];
            mysql_format(gSQL, query, sizeof(query),
                "DELETE FROM notes WHERE id = %d AND server_id = %d",
                gNotes[old][nbDbId], Server_InstanceId());
            mysql_tquery(gSQL, query, "", "");
            Notes_DestroyLabel(old);
            gNotes[old][nbDbId] = 0;
            Log_Format(LOG_INFO, "[NOTES] papan %d penuh, pesan lama tergusur.", board);
        }
    }

    new index = Notes_FreeSlot();
    if (index < 0)
    {
        Toast_Show(playerid, "PAPAN", "Semua papan penuh. Coba lagi nanti.", UI_ACCENT_WARN);
        return 0;
    }

    new author[NOTES_AUTHOR_MAX];
    GetPlayerName(playerid, author, sizeof(author));

    gNotes[index][nbDbId] = -1;             /* menunggu insert_id */
    gNotes[index][nbBoard] = board;
    gNotes[index][nbChar] = ActiveCharId[playerid];
    strcpy(gNotes[index][nbAuthor], author, NOTES_AUTHOR_MAX);
    strcpy(gNotes[index][nbBody], gNoteDraft[playerid], NOTES_BODY_MAX + 1);
    gNotes[index][nbCreatedAt] = gettime();
    gNotes[index][nbLabel] = STREAMER_TAG_3D_TEXT_LABEL:INVALID_3DTEXT_ID;

    new query[256];
    mysql_format(gSQL, query, sizeof(query),
        "INSERT INTO notes (server_id, board_id, char_id, author, body, created_at) \
         VALUES (%d, %d, %d, '%e', '%e', %d)",
        Server_InstanceId(), board, ActiveCharId[playerid], author,
        gNoteDraft[playerid], gNotes[index][nbCreatedAt]);
    mysql_tquery(gSQL, query, "Notes_OnInserted", "i", playerid);

    gNoteDraft[playerid][0] = EOS;
    gNoteDraftUntil[playerid] = 0;
    gNoteLastStick[playerid] = GetTickCount();

    new boardName[MAX_ZONE_NAME], line[128];
    Notes_BoardName(board, boardName, sizeof(boardName));
    format(line, sizeof(line), "[PAPAN] Pesanmu menempel di papan %s.", boardName);
    SendClientMessage(playerid, UI_ACCENT_PRIMARY, line);
    Log_Format(LOG_INFO, "[NOTES] char=%d menempel di papan %d.", ActiveCharId[playerid], board);
    return 1;
}

public Notes_OnInserted(playerid)
{
    if (!IsPlayerConnected(playerid)) return 0;

    /* Temukan slot yang menunggu insert_id, isi id database, bangun label. */
    for (new i = 0; i < NOTES_MAX; i++)
    {
        if (gNotes[i][nbDbId] != -1) continue;
        if (gNotes[i][nbChar] != ActiveCharId[playerid]) continue;

        gNotes[i][nbDbId] = cache_insert_id();
        Notes_CreateLabel(i);
        return 1;
    }
    return 0;
}

/* ------------------------------------------------------------- /baca */

stock Notes_ShowList(playerid)
{
    new board = Notes_NearestBoard(playerid, NOTES_RANGE);
    if (board < 0)
    {
        SendClientMessage(playerid, UI_TEXT_MUTED, "[BACA] Tidak ada papan pengumuman di dekatmu.");
        return 0;
    }

    new body[1024], line[160], rows = 0;
    body[0] = EOS;
    strcat(body, "Penulis\tPesan\n", sizeof(body));

    for (new i = 0; i < NOTES_MAX; i++)
    {
        if (gNotes[i][nbDbId] == 0 || gNotes[i][nbDbId] == -1) continue;
        if (gNotes[i][nbBoard] != board) continue;

        new preview[48];
        strcpy(preview, gNotes[i][nbBody], sizeof(preview));
        if (strlen(gNotes[i][nbBody]) > 40) strcat(preview, "...", sizeof(preview));
        format(line, sizeof(line), "%s\t%s\n", gNotes[i][nbAuthor], preview);
        strcat(body, line, sizeof(body));
        rows++;
    }

    if (rows == 0)
    {
        SendClientMessage(playerid, UI_TEXT_MUTED, "[BACA] Papannya kosong. Jadi yang pertama menulis.");
        return 0;
    }

    gNoteBoardSel[playerid] = board;
    new caption[64], boardName[MAX_ZONE_NAME];
    Notes_BoardName(board, boardName, sizeof(boardName));
    format(caption, sizeof(caption), "Papan %s (%d pesan)", boardName, rows);
    ShowPlayerDialog(playerid, DIALOG_NOTES_LIST, DIALOG_STYLE_TABLIST_HEADERS, caption, body, "Baca", "Tutup");
    return 1;
}

stock Notes_ShowRead(playerid, row)
{
    new board = gNoteBoardSel[playerid];
    new seen = 0;
    for (new i = 0; i < NOTES_MAX; i++)
    {
        if (gNotes[i][nbDbId] == 0 || gNotes[i][nbDbId] == -1) continue;
        if (gNotes[i][nbBoard] != board) continue;
        if (seen++ != row) continue;

        new age = gettime() - gNotes[i][nbCreatedAt];
        new ageText[32];
        if (age < 3600) format(ageText, sizeof(ageText), "%d menit lalu", age / 60 + 1);
        else if (age < 86400) format(ageText, sizeof(ageText), "%d jam lalu", age / 3600 + 1);
        else format(ageText, sizeof(ageText), "%d hari lalu", age / 86400 + 1);

        new body[256];
        format(body, sizeof(body), "{F5F5F5}%s\n\n{A8B2C1}oleh %s, %s", gNotes[i][nbBody], gNotes[i][nbAuthor], ageText);
        ShowPlayerDialog(playerid, DIALOG_NOTES_READ, DIALOG_STYLE_MSGBOX, "Pesan di Papan", body, "Tutup", "");
        return 1;
    }
    return 0;
}

forward Notes_OnDialogResponse(playerid, dialogid, response, listitem, inputtext[]);

public Notes_OnDialogResponse(playerid, dialogid, response, listitem, inputtext[])
{
    #pragma unused inputtext

    switch (dialogid)
    {
        case DIALOG_NOTES_WRITE:
        {
            if (response) Notes_OnWriteInput(playerid, inputtext);
            return 1;
        }
        case DIALOG_NOTES_LIST:
        {
            if (response) Notes_ShowRead(playerid, listitem);
            return 1;
        }
        case DIALOG_NOTES_READ:
        {
            Notes_ShowList(playerid);
            return 1;
        }
    }
    return 0;
}

/* ------------------------------------------------------- disconnect */

stock Notes_ResetPlayer(playerid)
{
    /* Kertas tertulis yang belum ditempel dikembalikan supaya tidak hilang
       cuma karena logout di tengah jalan. */
    if (gNoteDraftUntil[playerid] != 0 && gettime() <= gNoteDraftUntil[playerid]
        && Account_IsPlaying(playerid))
    {
        Inventory_Grant(playerid, ITEM_PAPER, 1, "draft notes batal");
    }
    gNoteDraft[playerid][0] = EOS;
    gNoteDraftUntil[playerid] = 0;
    gNoteLastStick[playerid] = 0;
    gNoteBoardSel[playerid] = -1;
    return 1;
}

/* ----------------------------------------------------------- selftest */

stock Notes_Selftest()
{
    new bool:ok = true;

    /* Item kertas terdaftar dan valid. */
    ok = ok && Item_IsValid(ITEM_PAPER);

    /* Sanitasi: embed warna dibuang, panjang dipotong, ujung rapi. */
    new dirty[160] = "halo~dunia  ";
    new clean[NOTES_BODY_MAX + 1];
    Notes_Sanitize(dirty, clean, sizeof(clean));
    ok = ok && strcmp(clean, "halo-dunia") == 0;

    new long[200];
    for (new i = 0; i < 199; i++) long[i] = 'a';
    long[199] = EOS;
    Notes_Sanitize(long, clean, sizeof(clean));
    ok = ok && strlen(clean) == NOTES_BODY_MAX;

    /* Empat papan terpasang dengan koordinat genggam tanah yang masuk akal. */
    ok = ok && NOTES_BOARD_COUNT == 4;
    for (new b = 0; b < NOTES_BOARD_COUNT; b++)
    {
        ok = ok && gBoards[b][bObject] != INVALID_OBJECT_ID;
        ok = ok && gBoards[b][bz] > -50.0 && gBoards[b][bz] < 200.0;
    }

    Selftest_Record(ok, "papan pengumuman",
        ok ? "kertas valid, sanitasi bersih, 4 papan berdiri" : "papan/sanitasi gagal");
    return 1;
}
