#if defined _ls_instance_included
    #endinput
#endif
#define _ls_instance_included

/* Identitas instance server (ROADMAP 315).
   Satu database bisa dipakai beberapa server (s1, s2, ...). Semua karakter
   diberi `characters.server_id`; daftar karakter, pembuatan karakter, dan
   `/pilih` difilter dengan nilai ini, sementara `accounts` tetap bersama
   supaya login/password/ban berlaku lintas instance.

   Nilai diambil dari server.cfg:
       server_id 1
   Kalau tidak ada atau di luar rentang, dipakai 1 — server lama yang belum
   punya baris `server_id` tetap berjalan tanpa perubahan data. */

#define SERVER_ID_MIN 1
#define SERVER_ID_MAX 9
#define SERVER_ID_DEFAULT 1

/* SA-MP mengurung fungsi file ke folder scriptfiles/ dan path ditulis relatif
   terhadap folder itu (sama seperti mysql_connect_file("mysql.ini")), jadi
   path absolut/berawalan "scriptfiles/" selalu gagal dibuka. */
#define SERVER_ID_FILE       "instance.ini"
#define SERVER_ID_PORT_BASE  7776
#define SERVER_ID_LINE_MAX   64

new gServerInstanceId = 0;

/* Ambil angka pertama setelah awalan "server_id" pada satu baris ini.
   Menerima "server_id 2", "server_id=2", dan "server_id = 2". */
stock Server_ParseIdLine(const line[])
{
    new length = strlen(line);
    new cursor = 0;

    while (cursor < length && line[cursor] != EOS)
    {
        if (line[cursor] >= '0' && line[cursor] <= '9')
        {
            return strval(line[cursor]);
        }
        cursor++;
    }
    return 0;
}

stock Server_ReadIdFile()
{
    /* fopen langsung: fexist() bisa melaporkan "tidak ada" pada path relatif
       walau file ada di cwd server (terjadi di s2: instance.ini terbaca 0
       sampai pemeriksaan fexist dibuang). */
    new File:handle = fopen(SERVER_ID_FILE, io_read);
    if (!handle) return 0;

    new line[SERVER_ID_LINE_MAX], found = 0;
    while (fread(handle, line, sizeof(line)))
    {
        /* Pawn tidak punya strncmp: strcmp(str1, str2, bool:ignorecase, length). */
        if (strcmp(line, "server_id", true, 9) != 0) continue;

        found = Server_ParseIdLine(line);
        if (found > 0) break;
    }
    fclose(handle);
    return found;
}

/* Nilai diambil berurutan: scriptfiles/instance.ini, lalu turunan port
   listen (7777 = instance 1), lalu 1.

   Kenapa bukan GetConsoleVarAsInt("server_id"): SA-MP hanya mengekspos
   direktif yang dikenalnya, baris tak dikenal di server.cfg dibuang tanpa
   pesan — terbukti dari s2 yang tetap melaporkan server_id=1 walau
   server.cfg berisi "server_id 2". Port adalah console var yang pasti ada,
   jadi dipakai sebagai cadangan. */
stock Server_InstanceId()
{
    if (gServerInstanceId > 0) return gServerInstanceId;

    new raw = Server_ReadIdFile();

    if (raw < SERVER_ID_MIN || raw > SERVER_ID_MAX)
    {
        new listenPort = GetServerVarAsInt("port");
        raw = listenPort - SERVER_ID_PORT_BASE;
    }

    if (raw < SERVER_ID_MIN || raw > SERVER_ID_MAX) raw = SERVER_ID_DEFAULT;

    gServerInstanceId = raw;
    return gServerInstanceId;
}

/* Untuk log boot: membuktikan instance mana yang sedang memegang DB bersama. */
stock Server_InstanceLabel(dest[], size)
{
    format(dest, size, "server_id=%d", Server_InstanceId());
    return 1;
}
