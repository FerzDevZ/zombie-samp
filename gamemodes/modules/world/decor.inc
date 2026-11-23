#if defined _ls_decor_included
    #endinput
#endif
#define _ls_decor_included

/* Dekorasi tambahan kamp (ROADMAP 223).
   Semua model yang dipakai di sini SUDAH terbukti tampil di server ini
   (19447 panel dinding 3 m, 1427 barikade jalan, 19477 papan tanda) — bukan
   model tebakan. Koordinat dikumpulkan di satu tabel supaya tuning hasil
   playtest tidak menyentuh logika.

   Verifikasi visual tetap di client: `/decor` mencetak daftar + `/decor tp <n>`
   memindahkan admin ke titik dekorasi supaya bisa dilihat langsung. */

#define DECOR_SIGN_TEXT_MAX 40

enum E_DECOR_PROP
{
    dcModel,
    Float:dcX,
    Float:dcY,
    Float:dcZ,
    Float:dcRz
};

enum E_DECOR_SIGN
{
    Float:dsX,
    Float:dsY,
    Float:dsZ,
    Float:dsRz,
    dsText[DECOR_SIGN_TEXT_MAX]
};

/* Barikade tambahan di mulut kamp + dinding penahan angin di sisi utara. */
new const DecorProp[][E_DECOR_PROP] = {
    {1427, 1675.0, -2336.0, 12.9, 0.0},
    {1427, 1679.0, -2336.0, 12.9, 0.0},
    {1427, 1655.0, -2332.0, 12.9, 90.0},
    {19447, 1654.0, -2312.0, 13.9, 0.0},
    {19447, 1657.0, -2312.0, 13.9, 0.0},
    {19447, 1660.0, -2312.0, 13.9, 0.0}
};

/* Papan peringatan: menghadap jalan masuk (rz 270 = menghadap timur). */
new const DecorSign[][E_DECOR_SIGN] = {
    {1690.0, -2333.0, 14.2, 270.0, "KARANTINA - JANGAN MASUK"},
    {1652.0, -2320.0, 14.2, 90.0, "SHELTER ->"},
    {1670.0, -2340.0, 13.6, 0.0, "ZONA AMAN - SENJATA SIAP"}
};

stock Decor_Count()
{
    return sizeof(DecorProp) + sizeof(DecorSign);
}

stock bool:Decor_IsValidIndex(index)
{
    return (index >= 0 && index < Decor_Count());
}

stock Decor_Init()
{
    for (new index = 0; index < sizeof(DecorProp); index++)
    {
        CreateDynamicObject(DecorProp[index][dcModel],
            DecorProp[index][dcX], DecorProp[index][dcY], DecorProp[index][dcZ],
            0.0, 0.0, DecorProp[index][dcRz]);
    }

    for (new index = 0; index < sizeof(DecorSign); index++)
    {
        new STREAMER_TAG_OBJECT:sign = CreateDynamicObject(19477,
            DecorSign[index][dsX], DecorSign[index][dsY], DecorSign[index][dsZ],
            0.0, 0.0, DecorSign[index][dsRz]);
        Map_ApplySignage(sign, DecorSign[index][dsText]);
    }

    Log_Format(LOG_INFO, "[DECOR] %d prop + %d papan tanda dipasang.", sizeof(DecorProp), sizeof(DecorSign));
    return 1;
}

stock Decor_ShowList(playerid)
{
    if (!IsServerAdmin(playerid)) return 0;

    new line[160], count = Decor_Count();
    format(line, sizeof(line), "[DECOR] %d dekorasi terpasang. /decor tp <0-%d> untuk melihat di client.",
        count, count - 1);
    SendClientMessage(playerid, UI_ACCENT_PRIMARY, line);

    for (new index = 0; index < count; index++)
    {
        if (index < sizeof(DecorProp))
        {
            format(line, sizeof(line), "  #%d prop model=%d di %.1f, %.1f, %.1f",
                index, DecorProp[index][dcModel],
                DecorProp[index][dcX], DecorProp[index][dcY], DecorProp[index][dcZ]);
        }
        else
        {
            new signIndex = index - sizeof(DecorProp);
            format(line, sizeof(line), "  #%d papan \"%s\" di %.1f, %.1f, %.1f",
                index, DecorSign[signIndex][dsText],
                DecorSign[signIndex][dsX], DecorSign[signIndex][dsY], DecorSign[signIndex][dsZ]);
        }
        SendClientMessage(playerid, UI_TEXT_SECONDARY, line);
    }
    return 1;
}

stock Decor_TeleportTo(playerid, index)
{
    if (!IsServerAdmin(playerid)) return 0;
    if (!Decor_IsValidIndex(index))
    {
        SendClientMessage(playerid, UI_ACCENT_WARN, "[DECOR] Indeks tidak ada. Lihat /decor.");
        return 0;
    }

    new Float:x, Float:y, Float:z;
    if (index < sizeof(DecorProp))
    {
        x = DecorProp[index][dcX];
        y = DecorProp[index][dcY];
        z = DecorProp[index][dcZ];
    }
    else
    {
        new signIndex = index - sizeof(DecorProp);
        x = DecorSign[signIndex][dsX];
        y = DecorSign[signIndex][dsY];
        z = DecorSign[signIndex][dsZ];
    }

    /* Berdiri 6 m dari objek menghadapnya: cukup jauh untuk melihat prop utuh. */
    SetPlayerPos(playerid, x - 6.0, y, z + 1.0);
    AC_ClearSample(playerid);
    SetCameraBehindPlayer(playerid);

    new line[128];
    format(line, sizeof(line), "[DECOR] Kamu di dekorasi #%d. Kalau tidak terlihat, lapor model-nya.", index);
    SendClientMessage(playerid, UI_ACCENT_PRIMARY, line);
    return 1;
}
