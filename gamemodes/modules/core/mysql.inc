#if defined _ls_mysql_included
    #endinput
#endif
#define _ls_mysql_included

/* Satu handle MySQL untuk seluruh gamemode. Semua query wajib lewat mysql_tquery/
   mysql_pquery: query sinkron di main thread membekukan tick permainan. */

new MySQL:gSQL = MYSQL_INVALID_HANDLE;
new bool:SQLReady = false;
new SQLPingLatency = 0;

forward SQL_OnPing();
forward SQL_OnPingFailed();

stock bool:SQL_IsReady()
{
    return SQLReady;
}

/* Epoch MILIDETIK sebagai string: cell PAWN 32-bit, jadi gettime()*1000
   overflow menjadi negatif dan merusak semua kolom created_at BIGINT.
   Concat "%d000" menghasilkan angka ms benar tanpa arithmetic 64-bit.
   Pakai sebagai argumen %s di mysql_format (isinya hanya digit, aman). */
stock EpochMs(dest[], destSize)
{
    format(dest, destSize, "%d000", gettime());
    return 1;
}

stock bool:SQL_Boot()
{
    /* mysql_connect_file() membaca mysql.ini (gitignored). Opsi pool/reconnect
       dibiarkan default plugin; kredensial tidak pernah masuk kode. */
    gSQL = mysql_connect_file("mysql.ini");

    if (gSQL == MYSQL_INVALID_HANDLE || mysql_errno(gSQL) != 0)
    {
        Log_Format(LOG_ERROR, "[MYSQL] gagal tersambung (errno %d). Cek mysql.ini dan container MariaDB.", mysql_errno(gSQL));
        SQLReady = false;
        return false;
    }

    SQLReady = true;
    SQLPingLatency = GetTickCount();
    mysql_tquery(gSQL, "SELECT VERSION(), DATABASE()", "SQL_OnPing", "");
    return true;
}

stock SQL_Shutdown()
{
    if (gSQL == MYSQL_INVALID_HANDLE) return 0;

    mysql_close(gSQL);
    gSQL = MYSQL_INVALID_HANDLE;
    SQLReady = false;
    return 1;
}

public SQL_OnPing()
{
    if (cache_num_rows() == 0)
    {
        SQL_OnPingFailed();
        return 1;
    }

    new serverVersion[48], databaseName[48];
    cache_get_value_index(0, 0, serverVersion, sizeof(serverVersion));
    cache_get_value_index(0, 1, databaseName, sizeof(databaseName));

    Log_Format(LOG_INFO, "[MYSQL] tersambung ke %s (%s) dalam %d ms.",
        databaseName, serverVersion, GetTickCount() - SQLPingLatency);
    return 1;
}

public SQL_OnPingFailed()
{
    Log_Format(LOG_ERROR, "[MYSQL] ping gagal: query uji tidak mengembalikan baris.");
    SQLReady = false;
    return 1;
}
