#if defined _ls_slotqueue_included
    #endinput
#endif
#define _ls_slotqueue_included

/* Antrean masuk saat server penuh (ROADMAP 316).
   SA-MP tidak memberi cara menolak koneksi sebelum slot dipakai, jadi antrean
   ini bekerja pada tingkat gamemode: pemain yang masuk melewati batas main
   ditahan di "ruang tunggu" (tidak boleh login) sampai ada slot bebas.

   Angka default berasal dari konfigurasi nyata server ini: `maxplayers 300`
   dan `maxnpc 200` (FCNPC zombie). Artinya slot manusia maksimum 100; cap 90
   menyisakan 10 slot supaya pemain yang reconnect tidak tertolak saat zombie
   pool sedang penuh. `player_cap` di server.cfg harus <= 300 - maxnpc.

   Batas diatur server.cfg:
       player_cap 180
   Tanpa baris itu dipakai SERVER_PLAYER_CAP_DEFAULT. */

#define SERVER_PLAYER_CAP_DEFAULT 90
#define SERVER_PLAYER_CAP_MIN     2
#define QUEUE_TICK_MS             5000
#define QUEUE_TIMEOUT_MS          600000     /* 10 menit menunggu */
#define QUEUE_SLOT_CHECK_MAX      64

new bool:QueueWaiting[MAX_PLAYERS];
new QueueJoinTick[MAX_PLAYERS];

stock Server_PlayerCap()
{
    new cap = GetConsoleVarAsInt("player_cap");
    if (cap < SERVER_PLAYER_CAP_MIN) cap = SERVER_PLAYER_CAP_DEFAULT;
    if (cap > MAX_PLAYERS) cap = MAX_PLAYERS;
    return cap;
}

stock Server_PlayingCount()
{
    new total = 0;
    for (new playerid = 0; playerid < MAX_PLAYERS; playerid++)
    {
        if (!IsPlayerConnected(playerid)) continue;
        if (IsPlayerNPC(playerid)) continue;
        if (!Account_IsPlaying(playerid)) continue;
        total++;
    }
    return total;
}

/* Murni: dipakai selftest dan Queue_OnPlayerConnect. */
stock bool:Queue_HoldsFor(activeCount)
{
    return (activeCount >= Server_PlayerCap());
}

stock bool:Queue_IsWaiting(playerid)
{
    if (playerid < 0 || playerid >= MAX_PLAYERS) return false;
    return QueueWaiting[playerid];
}

stock Queue_Position(playerid)
{
    new position = 1;
    for (new other = 0; other < MAX_PLAYERS; other++)
    {
        if (!QueueWaiting[other]) continue;
        if (other == playerid) continue;
        if (QueueJoinTick[other] < QueueJoinTick[playerid]) position++;
        else if (QueueJoinTick[other] == QueueJoinTick[playerid] && other < playerid) position++;
    }
    return position;
}

stock Queue_Message(playerid, const message[])
{
    SendClientMessage(playerid, UI_ACCENT_WARN, message);
    return 1;
}

stock Queue_OnPlayerConnect(playerid)
{
    if (!IsPlayerConnected(playerid)) return 0;
    if (IsPlayerNPC(playerid)) return 0;

    QueueWaiting[playerid] = false;
    QueueJoinTick[playerid] = GetTickCount();

    if (!Queue_HoldsFor(Server_PlayingCount())) return 0;

    QueueWaiting[playerid] = true;
    QueueJoinTick[playerid] = GetTickCount();

    new line[176];
    format(line, sizeof(line),
        "[ANTREAN] Server penuh (%d/%d bermain). Kamu di posisi %d, tunggu maksimum 10 menit.",
        Server_PlayingCount(), Server_PlayerCap(), Queue_Position(playerid));
    Queue_Message(playerid, line);
    Queue_Message(playerid, "[ANTREAN] Login dibuka otomatis begitu ada slot. Jangan tutup client.");
    return 1;
}

/* Dipanggil dari timer 5 detik: satu pemain paling depan diizinkan masuk
   begitu ada slot. FIFO, bukan acak, supaya tidak ada yang kelaparan. */
forward Queue_Tick();

public Queue_Tick()
{
    new now = GetTickCount();
    new admitted = 0;

    for (new playerid = 0; playerid < MAX_PLAYERS; playerid++)
    {
        if (!QueueWaiting[playerid]) continue;

        if (!IsPlayerConnected(playerid))
        {
            QueueWaiting[playerid] = false;
            continue;
        }

        /* Habis waktu: keluar dengan pesan jelas, bukan diam-diam ditendang. */
        if (now - QueueJoinTick[playerid] >= QUEUE_TIMEOUT_MS)
        {
            QueueWaiting[playerid] = false;
            KickQueuedPlayer(playerid);
            continue;
        }

        if (admitted > 0) continue;
        if (Queue_HoldsFor(Server_PlayingCount())) continue;

        QueueWaiting[playerid] = false;
        admitted++;

        SendClientMessage(playerid, UI_ACCENT_PRIMARY,
            "[ANTREAN] Slot tersedia. Login sekarang: /masuk <password> atau /daftar <password>.");
        Toast_Show(playerid, "Antrean selesai", "Slot untukmu sudah terbuka.", UI_ACCENT_PRIMARY);
        Log_Format(LOG_INFO, "[ANTREAN] admit player=%d aktif=%d cap=%d",
            playerid, Server_PlayingCount(), Server_PlayerCap());
    }
    return 1;
}

stock KickQueuedPlayer(playerid)
{
    if (!IsPlayerConnected(playerid)) return 0;

    SendClientMessage(playerid, UI_ACCENT_DANGER,
        "[ANTREAN] Waktu tunggu habis (10 menit). Coba masuk lagi nanti.");
    Log_Format(LOG_INFO, "[ANTREAN] timeout player=%d", playerid);
    Kick(playerid);
    return 1;
}

/* Pesan penolakan untuk handler login/daftar: satu tempat supaya tidak ada
   jalur auth yang lolos saat antrean masih menahan. */
stock bool:Queue_BlockAuth(playerid)
{
    if (!Queue_IsWaiting(playerid)) return false;

    new line[176];
    format(line, sizeof(line),
        "[ANTREAN] Kamu masih menunggu slot (posisi %d, %d/%d bermain). Login terbuka otomatis.",
        Queue_Position(playerid), Server_PlayingCount(), Server_PlayerCap());
    Queue_Message(playerid, line);
    return true;
}
