#if defined _ls_logger_included
    #endinput
#endif
#define _ls_logger_included

enum E_LOG_LEVEL
{
    LOG_INFO = 0,
    LOG_WARN,
    LOG_ERROR
};

/* Handle file dibuka sekali dan dipakai ulang: fopen/fclose per baris log
   adalah IO syscall ganda di jalur yang dipanggil ratusan kali per menit.
   Handle dibuka ulang saat ganti hari atau saat fwrite gagal. */
new File:LogFile = File:0;
new LogFileDay = 0;

stock Logger_Write(E_LOG_LEVEL:level, const message[])
{
    new const levelTag[][] = {"INFO", "WARN", "ERROR"};
    new const fileName[] = "logs/laststand.log";

    new hour, minute, second, day, month, year;
    gettime(hour, minute, second);
    getdate(year, month, day);

    new line[512];
    format(line, sizeof(line), "[%04d-%02d-%02d %02d:%02d:%02d][%s] %s\r\n",
        year, month, day, hour, minute, second, levelTag[_:level], message);

    /* print() meneruskan buffer sebagai FORMAT ke logprintf/vsprintf milik
       samp03svr: satu '%' di dalam data (nick, label item, pesan) membuat
       vsprintf membaca argumen yang tidak ada lalu SIGSEGV di libc
       (_IO_vsprintf, backtrace fcnpc-crash_2026-10-19_14.27.52.txt).
       Kirim selalu sebagai DATA dengan format literal "%s". */
    printf("%s", line);

    new today = year * 10000 + month * 100 + day;
    if (LogFile && today != LogFileDay)
    {
        fclose(LogFile);
        LogFile = File:0;
    }
    if (!LogFile)
    {
        LogFile = fopen(fileName, io_append);
        LogFileDay = today;
    }
    if (LogFile)
    {
        /* CRLF sudah di buffer: SATU fwrite per baris (audit performa F14). */
        if (!fwrite(LogFile, line))
        {
            /* Handle basi (disk penuh sesaat/file dihapus luar): buka ulang sekali. */
            fclose(LogFile);
            LogFile = fopen(fileName, io_append);
            if (LogFile) fwrite(LogFile, line);
        }
    }
    return 1;
}

stock Log_Info(const message[])
{
    return Logger_Write(LOG_INFO, message);
}

stock Log_Warn(const message[])
{
    return Logger_Write(LOG_WARN, message);
}

stock Log_Error(const message[])
{
    return Logger_Write(LOG_ERROR, message);
}

stock Log_Format(E_LOG_LEVEL:level, const format[], {Float, _}:...)
{
    new buffer[512];
    va_format(buffer, sizeof(buffer), format, ___(2));
    return Logger_Write(level, buffer);
}
