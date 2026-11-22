#if defined _ls_transaction_included
    #endinput
#endif
#define _ls_transaction_included

/* Lapisan transaksi.
   Fakta plugin R41-4 (diverifikasi dari sumber C++ + log live, audit T1):
   - mysql_tquery SELALU terurut pada SATU threaded connection per handle
     (antrean FIFO); pool_size hanya dipakai mysql_pquery.
   - mysql_connect dengan kredensial sama MENGEMBALIKAN handle lama kecuali
     global option DUPLICATE_CONNECTIONS dinyalakan. Tanpa itu, "koneksi
     khusus" di sini fiktif (gTxSQL == gSQL) dan Tx_Shutdown menutup handle
     utama dua kali.
   Maka: DUPLICATE_CONNECTIONS dinyalakan, koneksi ini dibuat dengan options
   sendiri (POOL_SIZE 1) dan diassert berbeda dari gSQL. Isolasi nyata:
   write transaksi tidak mengantre di belakang telemetry/autosave pool utama.

   Semua penulisan tabel item (inventory, weapons, stash, item_ledger,
   grave_items) dirutekan lewat sini supaya satu penulis per tabel.
   Kredensial dibaca dari mysql.ini saat boot, tidak pernah masuk kode/log.

   TxDepth: Begin/Commit bersarang (Grant/Consume di dalam transaksi caller)
   hanya mengirim START/COMMIT di kedalaman terluar. Tx_Rollback membatalkan
   seluruh transaksi. OnQueryError menandai kegagalan sehingga COMMIT tidak
   pernah mempersist mutasi parsial tanpa jejak. */

new MySQL:gTxSQL = MYSQL_INVALID_HANDLE;
new bool:TxReady = false;
new bool:TxOpen = false;
new TxDepth = 0;
new bool:TxFailed = false;

stock bool:Tx_IsReady()
{
    return TxReady;
}

stock bool:Tx_InTransaction()
{
    return TxOpen;
}

/* Ambil nilai `key = value` dari satu baris ini (key harus di awal baris). */
stock Tx_IniValue(const line[], const key[], dest[], destSize)
{
    if (strfind(line, key, false) != 0) return 0;
    new eq = strfind(line, "=", false);
    if (eq == -1) return 0;

    new start = eq + 1;
    while (line[start] == ' ' || line[start] == '\t') start++;
    new end = strlen(line);
    while (end > start && (line[end - 1] == ' ' || line[end - 1] == '\t'
        || line[end - 1] == '\r' || line[end - 1] == '\n')) end--;

    strmid(dest, line, start, end, destSize);
    return 1;
}

stock bool:Tx_Boot()
{
    /* fopen PAWN relatif terhadap scriptfiles/: "mysql.ini" di sini adalah
       salinan scriptfiles/mysql.ini yang dibuat run-server.sh/instance.sh.
       Path "scriptfiles/mysql.ini" justru membuka scriptfiles/scriptfiles/...
       (tidak ada) — urutan lama menyesatkan (audit #13). */
    new File:ini = fopen("mysql.ini", io_read);
    if (!ini)
    {
        Log_Format(LOG_WARN, "[TX] mysql.ini tidak terbaca: transaksi nonaktif, write kembali ke pool tanpa jaminan atomik.");
        return false;
    }

    new host[64], user[64], pass[64], db[64], line[160];
    while (fread(ini, line))
    {
        Tx_IniValue(line, "hostname", host, sizeof(host));
        Tx_IniValue(line, "username", user, sizeof(user));
        Tx_IniValue(line, "password", pass, sizeof(pass));
        Tx_IniValue(line, "database", db, sizeof(db));
    }
    fclose(ini);

    if (host[0] == EOS || user[0] == EOS || db[0] == EOS)
    {
        Log_Format(LOG_WARN, "[TX] mysql.ini tidak lengkap: transaksi nonaktif.");
        return false;
    }

    /* Tanpa DUPLICATE_CONNECTIONS, mysql_connect mengembalikan handle gSQL
       (reuse) dan koneksi khusus tidak pernah ada (audit T1). */
    mysql_global_options(DUPLICATE_CONNECTIONS, 1);
    new MySQLOpt:txOptions = mysql_init_options();
    mysql_set_option(txOptions, POOL_SIZE, 1);
    gTxSQL = mysql_connect(host, user, pass, db, txOptions);
    if (gTxSQL == MYSQL_INVALID_HANDLE || mysql_errno(gTxSQL) != 0)
    {
        Log_Format(LOG_ERROR, "[TX] koneksi transaksi gagal (errno %d): write kembali ke pool.", mysql_errno(gTxSQL));
        TxReady = false;
        return false;
    }
    if (gTxSQL == gSQL)
    {
        Log_Format(LOG_ERROR, "[TX] handle transaksi == handle utama: isolasi gagal, transaksi dinonaktifkan.");
        mysql_close(gTxSQL);
        gTxSQL = MYSQL_INVALID_HANDLE;
        TxReady = false;
        return false;
    }

    TxReady = true;
    Log_Format(LOG_INFO, "[TX] koneksi transaksi khusus aktif (handle terpisah, atomic write item).");
    return true;
}

stock Tx_Shutdown()
{
    if (gTxSQL == MYSQL_INVALID_HANDLE) return 0;
    /* Transaksi terbuka saat exit = urutan write BELUM lengkap: ROLLBACK,
       bukan COMMIT (COMMIT mempersist mutasi parsial — audit T5). */
    if (TxOpen) mysql_tquery(gTxSQL, "ROLLBACK", "", "");
    /* Jangan close handle yang sama dengan gSQL (double-close, audit T1). */
    if (gTxSQL != gSQL) mysql_close(gTxSQL);
    gTxSQL = MYSQL_INVALID_HANDLE;
    TxReady = false;
    TxOpen = false;
    TxDepth = 0;
    TxFailed = false;
    return 1;
}

/* Query tunggal: lewat koneksi transaksi bila aktif, fallback pool.
   Di luar transaksi = autocommit per statement (tetap terurut per handle). */
stock Tx_Query(const query[])
{
    if (TxReady) mysql_tquery(gTxSQL, query, "", "");
    else mysql_tquery(gSQL, query, "", "");
    return 1;
}

stock Tx_Begin()
{
    if (!TxReady) return 0;
    if (TxOpen)
    {
        /* Bersarang: ikut transaksi luar, jangan START baru (START TRANSACTION
           meng-commit implisit transaksi yang masih terbuka). */
        TxDepth++;
        return 1;
    }
    mysql_tquery(gTxSQL, "START TRANSACTION", "", "");
    TxOpen = true;
    TxDepth = 1;
    TxFailed = false;
    return 1;
}

stock Tx_Commit()
{
    if (!TxReady || !TxOpen) return 0;
    TxDepth--;
    if (TxDepth > 0) return 1;   /* masih di dalam transaksi luar */

    if (TxFailed)
    {
        /* Ada query gagal di tengah transaksi: jangan commit parsial. */
        mysql_tquery(gTxSQL, "ROLLBACK", "", "");
        Log_Format(LOG_ERROR, "[TX] query gagal di tengah transaksi: ROLLBACK (mutasi dibatalkan).");
    }
    else
    {
        mysql_tquery(gTxSQL, "COMMIT", "", "");
    }
    TxOpen = false;
    TxDepth = 0;
    TxFailed = false;
    return 1;
}

stock Tx_Rollback()
{
    if (!TxReady || !TxOpen) return 0;
    mysql_tquery(gTxSQL, "ROLLBACK", "", "");
    TxOpen = false;
    TxDepth = 0;
    TxFailed = false;
    return 1;
}

/* Kegagalan query tidak boleh diam-diam: log terstruktur + tandai transaksi
   aktif supaya Tx_Commit berubah menjadi ROLLBACK (audit T4). */
public OnQueryError(errorid, const error[], const callback[], const query[], MySQL:handle)
{
    Log_Format(LOG_ERROR, "[SQL] error %d (%s) callback=%s query=%.120s", errorid, error, callback, query);
    if (handle == gTxSQL && TxOpen) TxFailed = true;
    return 1;
}
