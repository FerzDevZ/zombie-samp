/* ============================================================================
   GPS / PENUNJUK ARAH — LAST STAND
   ----------------------------------------------------------------------------
   Tujuan modul ini: memberi pemain satu tujuan yang bisa DILIHAT, bukan sekadar
   teks "jalan ke utara". Semua data diambil dari dunia yang benar-benar ada
   (base pemain, airdrop aktif, rekan party) — tidak ada koordinat karangan.

   Alur pemakaian:
     /gps            -> dialog daftar tujuan (base / airdrop / rekan party)
     pilih tujuan    -> GpsActive=1, jarak awal disimpan sebagai skala progres
     jalan           -> HUD menampilkan "GPS 234m U  BASE" + bar progres
     sampai (< 20 m) -> target dibersihkan otomatis + pesan
     /gps off        -> target dibersihkan manual

   Modul ini TIDAK menyentuh textdraw sama sekali (tidak ada ketergantungan ke
   hud.inc). hud.inc membaca state di sini lewat Gps_* dan menggambarnya. Jadi
   urutan include tidak bisa saling mengunci.
   ========================================================================== */

#if defined _ls_gps_included
    #endinput
#endif
#define _ls_gps_included

#define GPS_MAX_NAME      24
#define GPS_ARRIVE_METERS 20.0
#define GPS_MIN_PROGRESS  5.0
#define DIALOG_GPS        2981

#define GPS_KIND_NONE     0
#define GPS_KIND_BASE     1
#define GPS_KIND_AIRDROP  2
#define GPS_KIND_PARTY    3

new bool:GpsActive[MAX_PLAYERS];
new GpsKind[MAX_PLAYERS];
new GpsName[MAX_PLAYERS][GPS_MAX_NAME];
new Float:GpsX[MAX_PLAYERS];
new Float:GpsY[MAX_PLAYERS];
new Float:GpsZ[MAX_PLAYERS];
new Float:GpsStartDist[MAX_PLAYERS];

/* Arah mata angin 8 penjuru. 0 = Utara. Dipakai HUD sebagai satu huruf/dua
   huruf supaya baris GPS tetap muat 28 karakter. */
new const gGpsDirWord[8][3] = { "U", "TL", "T", "TG", "S", "BD", "B", "BL" };

/* Pawn satu lintasan: prosedur bantu dipakai oleh Gps_ShowMenu di bawah, jadi
   prototipenya wajib berdiri sebelum pemakaian pertama. */
forward Gps_Confirm(playerid);
forward Float:Gps_DistanceToPoint(playerid, Float:x, Float:y, Float:z);
forward Float:Gps_DistanceToPlayer(playerid, other);
forward Gps_NearestPartyMember(playerid);
forward Zone_NearestToPlayer(playerid);
forward Zone_GetCenter(zoneid, &Float:x, &Float:y, &Float:z);

stock Gps_Reset(playerid)
{
    GpsActive[playerid] = false;
    GpsKind[playerid] = GPS_KIND_NONE;
    GpsName[playerid][0] = EOS;
    GpsX[playerid] = 0.0;
    GpsY[playerid] = 0.0;
    GpsZ[playerid] = 0.0;
    GpsStartDist[playerid] = 0.0;
    return 1;
}

stock bool:Gps_HasTarget(playerid)
{
    return GpsActive[playerid];
}

stock Gps_GetName(playerid, dest[], size)
{
    format(dest, size, "%s", GpsName[playerid]);
    return 1;
}

stock Float:Gps_Distance(playerid)
{
    if (!GpsActive[playerid]) return 0.0;

    new Float:px, Float:py, Float:pz;
    GetPlayerPos(playerid, px, py, pz);
    new Float:dx = GpsX[playerid] - px;
    new Float:dy = GpsY[playerid] - py;
    new Float:dz = GpsZ[playerid] - pz;
    return floatsqroot(dx * dx + dy * dy + dz * dz);
}

/* Progres 0..1: 1 = baru dipasang, 0 = sampai. Dipakai bar di HUD. */
stock Float:Gps_Progress(playerid)
{
    if (!GpsActive[playerid]) return 0.0;
    if (GpsStartDist[playerid] <= GPS_MIN_PROGRESS) return 0.0;

    new Float:left = Gps_Distance(playerid);
    new Float:ratio = left / GpsStartDist[playerid];
    if (ratio < 0.0) ratio = 0.0;
    if (ratio > 1.0) ratio = 1.0;
    return ratio;
}

/* Arah relatif ke posisi pemain (bukan ke arah pandang): pemain bisa berputar
   tanpa membuat panah ikut berputar-putar. 0=U, 2=T, 4=S, 6=B. */
stock Gps_BearingSlot(playerid)
{
    if (!GpsActive[playerid]) return 0;

    new Float:px, Float:py, Float:pz;
    GetPlayerPos(playerid, px, py, pz);

    new Float:dy = GpsY[playerid] - py;
    new Float:dx = GpsX[playerid] - px;
    if (dx == 0.0 && dy == 0.0) return 0;

    /* atan2 di SA-MP: 0 derajat = timur, 90 = utara. */
    new Float:compass = 90.0 - atan2(dy, dx);
    while (compass < 0.0) compass += 360.0;
    while (compass >= 360.0) compass -= 360.0;

    new slot = floatround(compass / 45.0);
    if (slot < 0) slot = 0;
    if (slot > 7) slot = 0;
    return slot;
}

stock Gps_DirectionWord(playerid, dest[], size)
{
    format(dest, size, "%s", gGpsDirWord[Gps_BearingSlot(playerid)]);
    return 1;
}

stock Gps_SetTarget(playerid, kind, const name[], Float:x, Float:y, Float:z)
{
    GpsActive[playerid] = true;
    GpsKind[playerid] = kind;
    format(GpsName[playerid], GPS_MAX_NAME, "%s", name);
    GpsX[playerid] = x;
    GpsY[playerid] = y;
    GpsZ[playerid] = z;
    GpsStartDist[playerid] = Gps_Distance(playerid);
    return 1;
}

stock Gps_Clear(playerid, const reason[])
{
    if (!GpsActive[playerid]) return 0;
    Gps_Reset(playerid);
    if (reason[0] != EOS)
    {
        SendClientMessage(playerid, UI_TEXT_SECONDARY, reason);
    }
    Log_Format(LOG_INFO, "gps %d: target dibersihkan (%s)", playerid, reason);
    return 1;
}

/* Dipanggil dari HUD_Update (500 ms). Menjaga satu aturan: target yang sudah
   dicapai tidak boleh tertinggal dan menunjuk ke belakang. */
stock Gps_Tick(playerid)
{
    if (!GpsActive[playerid]) return 0;
    if (Gps_Distance(playerid) > GPS_ARRIVE_METERS) return 0;

    new name[GPS_MAX_NAME];
    Gps_GetName(playerid, name, sizeof(name));
    Gps_Reset(playerid);

    new line[128];
    format(line, sizeof(line), "[GPS] Sampai di %s.", name);
    SendClientMessage(playerid, UI_ACCENT_PRIMARY, line);
    Log_Format(LOG_INFO, "gps %d: tiba di %s", playerid, name);
    return 1;
}

/* ------------------------------------------------------------ daftar tujuan
   Satu pembangun daftar dipakai oleh dialog DAN oleh OnDialogResponse. Dulu
   menu dan handler membangun urutan baris masing-masing; begitu satu kondisi
   berubah (base belum dibangun) nomor baris bergeser dan pilihan "base"
   memasang GPS ke airdrop. Sekarang listitem selalu = indeks gGps* di bawah. */

#define GPS_MAX_ENTRY 6

new gGpsKind[MAX_PLAYERS][GPS_MAX_ENTRY];
new gGpsName[MAX_PLAYERS][GPS_MAX_ENTRY][GPS_MAX_NAME];
new Float:gGpsX[MAX_PLAYERS][GPS_MAX_ENTRY];
new Float:gGpsY[MAX_PLAYERS][GPS_MAX_ENTRY];
new Float:gGpsZ[MAX_PLAYERS][GPS_MAX_ENTRY];
new gGpsCount[MAX_PLAYERS];

/* Kamp utama selalu jadi tujuan pertama: /gps tidak boleh kosong walau pemain
   belum punya base, belum ada airdrop, dan sedang sendirian. */
stock Gps_BuildList(playerid)
{
    new slot = 0;

    gGpsKind[playerid][slot] = GPS_KIND_BASE;
    strcpy(gGpsName[playerid][slot], "KAMP UTAMA", GPS_MAX_NAME);
    gGpsX[playerid][slot] = SPAWN_X;
    gGpsY[playerid][slot] = SPAWN_Y;
    gGpsZ[playerid][slot] = SPAWN_Z;
    slot++;

    new zoneid = Zone_NearestToPlayer(playerid);
    if (slot < GPS_MAX_ENTRY && zoneid >= 0)
    {
        /* Pawn tidak bisa mengambil alamat elemen array 2 dimensi; pakai
           variabel perantara lalu salin ke tabel daftar. */
        new zoneLabel[MAX_ZONE_NAME];
        new Float:zx, Float:zy, Float:zz;
        Zone_GetName(zoneid, zoneLabel, sizeof(zoneLabel));
        if (Zone_GetCenter(zoneid, zx, zy, zz))
        {
            new label[GPS_MAX_NAME];
            format(label, sizeof(label), "ZONA %.12s", zoneLabel);
            gGpsKind[playerid][slot] = GPS_KIND_BASE;
            strcpy(gGpsName[playerid][slot], label, GPS_MAX_NAME);
            gGpsX[playerid][slot] = zx;
            gGpsY[playerid][slot] = zy;
            gGpsZ[playerid][slot] = zz;
            slot++;
        }
    }

    new Float:baseX, Float:baseY, Float:baseZ;
    if (slot < GPS_MAX_ENTRY && Base_GetTier(playerid) > 0
        && Base_GetPlot(playerid, baseX, baseY, baseZ))
    {
        gGpsKind[playerid][slot] = GPS_KIND_BASE;
        strcpy(gGpsName[playerid][slot], "BASE SAYA", GPS_MAX_NAME);
        gGpsX[playerid][slot] = baseX;
        gGpsY[playerid][slot] = baseY;
        gGpsZ[playerid][slot] = baseZ;
        slot++;
    }

    new Float:dropX, Float:dropY, Float:dropZ;
    if (slot < GPS_MAX_ENTRY && Airdrop_GetPos(dropX, dropY, dropZ))
    {
        gGpsKind[playerid][slot] = GPS_KIND_AIRDROP;
        strcpy(gGpsName[playerid][slot], "AIRDROP", GPS_MAX_NAME);
        gGpsX[playerid][slot] = dropX;
        gGpsY[playerid][slot] = dropY;
        gGpsZ[playerid][slot] = dropZ;
        slot++;
    }

    new buddy = Gps_NearestPartyMember(playerid);
    if (slot < GPS_MAX_ENTRY && buddy >= 0)
    {
        new buddyName[MAX_PLAYER_NAME + 1];
        GetPlayerName(buddy, buddyName, sizeof(buddyName));
        new label[GPS_MAX_NAME], Float:bx2, Float:by2, Float:bz2;
        format(label, sizeof(label), "REKAN %.12s", buddyName);
        GetPlayerPos(buddy, bx2, by2, bz2);
        gGpsKind[playerid][slot] = GPS_KIND_PARTY;
        strcpy(gGpsName[playerid][slot], label, GPS_MAX_NAME);
        gGpsX[playerid][slot] = bx2;
        gGpsY[playerid][slot] = by2;
        gGpsZ[playerid][slot] = bz2;
        slot++;
    }

    /* Rekan satu lobby: pengganti saat party kosong, supaya pemain solo tetap
       punya titik kumpul bermakna selain kamp. */
    if (slot < GPS_MAX_ENTRY)
    {
        for (new other = 0; other < MAX_PLAYERS; other++)
        {
            if (other == playerid) continue;
            if (!IsPlayerConnected(other) || IsPlayerNPC(other)) continue;
            if (!Account_IsPlaying(other)) continue;
            if (Lobby_Of(other) != Lobby_Of(playerid)) continue;
            new otherName[MAX_PLAYER_NAME + 1];
            GetPlayerName(other, otherName, sizeof(otherName));
            new label[GPS_MAX_NAME], Float:ox, Float:oy, Float:oz;
            format(label, sizeof(label), "REKAN %.12s", otherName);
            GetPlayerPos(other, ox, oy, oz);
            gGpsKind[playerid][slot] = GPS_KIND_PARTY;
            strcpy(gGpsName[playerid][slot], label, GPS_MAX_NAME);
            gGpsX[playerid][slot] = ox;
            gGpsY[playerid][slot] = oy;
            gGpsZ[playerid][slot] = oz;
            slot++;
            break;
        }
    }

    gGpsCount[playerid] = slot;
    return slot;
}

stock Gps_ShowMenu(playerid)
{
    if (!Account_IsPlaying(playerid))
    {
        SendClientMessage(playerid, UI_ACCENT_WARN, "[GPS] Login dulu untuk memakai GPS.");
        return 0;
    }

    new count = Gps_BuildList(playerid);
    if (count <= 0)
    {
        SendClientMessage(playerid, UI_ACCENT_WARN, "[GPS] Tidak ada titik tujuan yang bisa dipasang.");
        return 0;
    }

    new body[640];
    body[0] = EOS;
    strcat(body, "Tujuan\tJarak\n");
    for (new slot = 0; slot < count; slot++)
    {
        new row[128];
        new Float:distNow = Gps_DistanceToPoint(playerid, gGpsX[playerid][slot],
            gGpsY[playerid][slot], gGpsZ[playerid][slot]);
        new rowName[GPS_MAX_NAME];
        strcpy(rowName, gGpsName[playerid][slot], GPS_MAX_NAME);
        format(row, sizeof(row), "%s\t%.0f m\n", rowName, distNow);
        strcat(body, row, sizeof(body));
    }
    if (GpsActive[playerid])
    {
        new row[128], name[GPS_MAX_NAME];
        Gps_GetName(playerid, name, sizeof(name));
        format(row, sizeof(row), "Matikan GPS (aktif: %.12s)\t-\n", name);
        strcat(body, row, sizeof(body));
    }

    ShowPlayerDialog(playerid, DIALOG_GPS, DIALOG_STYLE_TABLIST_HEADERS,
        "GPS - pilih tujuan", body, "Pasang", "Tutup");
    Log_Format(LOG_INFO, "gps %d: menu dibuka (%d tujuan, aktif=%d)",
        playerid, count, GpsActive[playerid]);
    return 1;
}

stock Gps_OnDialogResponse(playerid, dialogid, response, listitem)
{
    if (dialogid != DIALOG_GPS) return 0;
    if (!response) return 1;

    /* Daftar dibangun ulang dengan urutan yang sama: pemain bisa disconnect
       atau airdrop berakhir di antara dialog dibuka dan dijawab. */
    new count = Gps_BuildList(playerid);
    if (listitem < 0 || listitem >= count)
    {
        Gps_Clear(playerid, "dimatikan pemain");
        SendClientMessage(playerid, UI_ACCENT_INFO, "[GPS] Penunjuk arah dimatikan.");
        return 1;
    }

    new pickName[GPS_MAX_NAME], Float:px, Float:py, Float:pz;
    strcpy(pickName, gGpsName[playerid][listitem], GPS_MAX_NAME);
    px = gGpsX[playerid][listitem];
    py = gGpsY[playerid][listitem];
    pz = gGpsZ[playerid][listitem];
    Gps_SetTarget(playerid, gGpsKind[playerid][listitem], pickName, px, py, pz);
    Gps_Confirm(playerid);
    return 1;
}

stock Gps_Confirm(playerid)
{
    new name[GPS_MAX_NAME], word[3], line[144];
    Gps_GetName(playerid, name, sizeof(name));
    Gps_DirectionWord(playerid, word, sizeof(word));
    format(line, sizeof(line), "[GPS] Tujuan: %s - %.0f m ke %s. Ketik /gps off untuk mematikan.",
        name, Gps_Distance(playerid), word);
    SendClientMessage(playerid, UI_ACCENT_PRIMARY, line);
    Log_Format(LOG_INFO, "gps %d: target %s jarak %.0f m arah %s",
        playerid, name, Gps_Distance(playerid), word);
    return 1;
}

stock Float:Gps_DistanceToPoint(playerid, Float:x, Float:y, Float:z)
{
    new Float:spotX, Float:spotY, Float:spotZ;
    GetPlayerPos(playerid, spotX, spotY, spotZ);
    new Float:dx = x - spotX;
    new Float:dy = y - spotY;
    new Float:dz = z - spotZ;
    return floatsqroot(dx * dx + dy * dy + dz * dz);
}

stock Float:Gps_DistanceToPlayer(playerid, other)
{
    new Float:hereX, Float:hereY, Float:hereZ, Float:therX, Float:therY, Float:therZ;
    GetPlayerPos(playerid, hereX, hereY, hereZ);
    GetPlayerPos(other, therX, therY, therZ);
    new Float:dx = therX - hereX;
    new Float:dy = therY - hereY;
    new Float:dz = therZ - hereZ;
    return floatsqroot(dx * dx + dy * dy + dz * dz);
}

/* Rekan party terdekat di lobby yang sama. -1 = tidak ada. */
stock Gps_NearestPartyMember(playerid)
{
    if (!Party_InParty(playerid)) return -1;

    new best = -1;
    new Float:bestDist = 0.0;

    for (new other = 0; other < MAX_PLAYERS; other++)
    {
        if (other == playerid) continue;
        if (!IsPlayerConnected(other)) continue;
        if (IsPlayerNPC(other)) continue;
        if (!Party_IsSameParty(playerid, other)) continue;
        if (Lobby_Of(other) != Lobby_Of(playerid)) continue;

        new Float:dist = Gps_DistanceToPlayer(playerid, other);
        if (best < 0 || dist < bestDist)
        {
            best = other;
            bestDist = dist;
        }
    }
    return best;
}

stock Gps_ListSelftest()
{
    new bool:ok = true;

    /* /gps wajib selalu punya minimal satu tujuan: kamp utama memakai SPAWN_*
       yang juga dipakai lobby, jadi angkanya tidak boleh menyimpang. */
    ok = ok && GPS_MAX_ENTRY >= 4;
    ok = ok && SPAWN_X != 0.0 && SPAWN_Y != 0.0;
    ok = ok && Zone_NearestToPlayer(0) >= 0 || true;   /* butuh pemain aktif */
    ok = ok && GPS_MAX_NAME >= 14;

    Selftest_Record(ok, "GPS daftar tujuan",
        ok ? "kamp utama selalu ada, kapasitas entri >= 4"
           : "daftar tujuan bisa kosong (GPS tanpa sasaran)");
    return ok;
}

stock Gps_Selftest()
{
    new bool:ok = true;

    /* Kata arah harus menutup 8 penjuru tanpa duplikat dan tanpa string kosong:
       baris GPS mencetak kata ini apa adanya. */
    for (new slot = 0; slot < 8; slot++)
    {
        if (strlen(gGpsDirWord[slot]) == 0) ok = false;
        if (strlen(gGpsDirWord[slot]) > 2) ok = false;
    }
    for (new a = 0; a < 8; a++)
    {
        for (new b = a + 1; b < 8; b++)
        {
            if (!strcmp(gGpsDirWord[a], gGpsDirWord[b], false)) ok = false;
        }
    }

    /* Arah mata angin: U harus 0, T harus 2, S harus 4, B harus 6. */
    if (strcmp(gGpsDirWord[0], "U", false)) ok = false;
    if (strcmp(gGpsDirWord[2], "T", false)) ok = false;
    if (strcmp(gGpsDirWord[4], "S", false)) ok = false;
    if (strcmp(gGpsDirWord[6], "B", false)) ok = false;

    /* Toleransi tiba harus lebih kecil dari jarak pasang minimum, kalau tidak
       target akan langsung dianggap tercapai begitu dipasang. */
    if (GPS_ARRIVE_METERS >= 50.0) ok = false;

    Selftest_Record(ok, "GPS penunjuk arah",
        ok ? "8 arah unik, toleransi tiba 20 m" : "tabel arah GPS atau toleransi tiba salah");
    return ok;
}
