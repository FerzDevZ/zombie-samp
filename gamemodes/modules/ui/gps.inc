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

stock Gps_ShowMenu(playerid)
{
    if (!Account_IsPlaying(playerid))
    {
        SendClientMessage(playerid, UI_ACCENT_WARN, "[GPS] Login dulu untuk memakai GPS.");
        return 0;
    }

    new body[512];
    body[0] = EOS;

    new Float:baseX, Float:baseY, Float:baseZ;
    if (Base_GetTier(playerid) > 0 && Base_GetPlot(playerid, baseX, baseY, baseZ))
    {
        new row[96];
        format(row, sizeof(row), "Base saya\t%.0f m\n", Gps_DistanceToPoint(playerid, baseX, baseY, baseZ));
        strcat(body, row);
    }

    new Float:dropX, Float:dropY, Float:dropZ;
    if (Airdrop_GetPos(dropX, dropY, dropZ))
    {
        new row[96];
        format(row, sizeof(row), "Airdrop aktif\t%.0f m\n", Gps_DistanceToPoint(playerid, dropX, dropY, dropZ));
        strcat(body, row);
    }

    new buddy = Gps_NearestPartyMember(playerid);
    if (buddy >= 0)
    {
        new buddyName[MAX_PLAYER_NAME + 1];
        GetPlayerName(buddy, buddyName, sizeof(buddyName));
        new row[128];
        format(row, sizeof(row), "Rekan party: %s\t%.0f m\n", buddyName,
            Gps_DistanceToPlayer(playerid, buddy));
        strcat(body, row);
    }

    if (GpsActive[playerid])
    {
        new row[96];
        new name[GPS_MAX_NAME];
        Gps_GetName(playerid, name, sizeof(name));
        format(row, sizeof(row), "Matikan GPS (%s)\t-\n", name);
        strcat(body, row);
    }

    if (body[0] == EOS)
    {
        SendClientMessage(playerid, UI_ACCENT_WARN,
            "[GPS] Belum ada tujuan yang tersedia. Bangun base, tunggu airdrop, atau ajak rekan party.");
        return 0;
    }

    ShowPlayerDialog(playerid, DIALOG_GPS, DIALOG_STYLE_TABLIST,
        "GPS - pilih tujuan", body, "Pasang", "Tutup");
    Log_Format(LOG_INFO, "gps %d: menu dibuka (target aktif=%d)", playerid, GpsActive[playerid]);
    return 1;
}

/* Urutan baris dialog HARUS sama dengan urutan penyusunan body di atas. */
stock Gps_OnDialogResponse(playerid, dialogid, response, listitem)
{
    if (dialogid != DIALOG_GPS) return 0;
    if (!response) return 1;

    new index = 0;

    new Float:baseX, Float:baseY, Float:baseZ;
    if (Base_GetTier(playerid) > 0 && Base_GetPlot(playerid, baseX, baseY, baseZ))
    {
        if (listitem == index)
        {
            Gps_SetTarget(playerid, GPS_KIND_BASE, "BASE SAYA", baseX, baseY, baseZ);
            Gps_Confirm(playerid);
            return 1;
        }
        index++;
    }

    new Float:dropX, Float:dropY, Float:dropZ;
    if (Airdrop_GetPos(dropX, dropY, dropZ))
    {
        if (listitem == index)
        {
            Gps_SetTarget(playerid, GPS_KIND_AIRDROP, "AIRDROP", dropX, dropY, dropZ);
            Gps_Confirm(playerid);
            return 1;
        }
        index++;
    }

    new buddy = Gps_NearestPartyMember(playerid);
    if (buddy >= 0)
    {
        if (listitem == index)
        {
            new buddyName[MAX_PLAYER_NAME + 1];
            GetPlayerName(buddy, buddyName, sizeof(buddyName));
            new Float:mateX, Float:mateY, Float:mateZ;
            GetPlayerPos(buddy, mateX, mateY, mateZ);
            Gps_SetTarget(playerid, GPS_KIND_PARTY, buddyName, mateX, mateY, mateZ);
            Gps_Confirm(playerid);
            return 1;
        }
        index++;
    }

    if (GpsActive[playerid] && listitem == index)
    {
        Gps_Clear(playerid, "[GPS] Penunjuk arah dimatikan.");
        return 1;
    }
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
