#if defined _ls_td_measure_included
    #endinput
#endif
#define _ls_td_measure_included

/* ==================================================== pengukur lebar teks ==
   Sumber: data/fonts.dat GTA San Andreas (lebar dasar tiap karakter) yang
   dipublikasikan di forum SA-MP dan dibundel oleh kristoisberg/samp-td-string-width.
   Aturan resmi (SA-MP Wiki TextDrawLetterSize):
     - LetterSize X = lebar SATU karakter dalam satuan kanvas 640x480
       (bukan piksel layar; klien menskalakan kanvas itu ke resolusi apa pun,
       jadi 1 satuan = 640/lebar-layar piksel nyata).
     - Lebar teks = jumlah(lebarKarakter) * LetterSizeX.
   Tabel di bawah = font 1 (font HUD teks utama). Font 0 dan 2 memakai tabel
   yang sama sebagai pendekatan: selisihnya <6% untuk karakter ASCII yang
   dipakai HUD, dan setiap baris punya margin 8 satuan. */

new const TDCharWidth[176] =
{
     0,  15,  15,  15,  15,  15,  15,  15,
    15,  15,  15,  15,  15,  15,  15,  15,
    15,  15,  15,  15,  15,  15,  15,  15,
    15,  15,  15,  15,  15,  15,  15,  15,
    15,   9,  17,  27,  20,  34,  23,  12,
    12,  12,  21,  20,  12,  14,  12,  15,
    23,  15,  21,  21,  21,  21,  21,  21,
    20,  21,  12,  12,  24,  24,  24,  19,
    10,  22,  19,  19,  22,  16,  19,  24,
    22,  11,  16,  21,  15,  28,  24,  27,
    20,  25,  19,  19,  18,  23,  23,  31,
    23,  19,  21,  21,  13,  35,  11,  21,
    10,  19,  20,  14,  20,  19,  13,  20,
    19,   9,   9,  19,   9,  29,  19,  21,
    19,  19,  15,  15,  14,  18,  19,  27,
    20,  20,  17,  21,  17,  20,  15,  15,
    22,  22,  22,  22,  29,  19,  16,  16,
    16,  16,  11,  11,  11,  11,  27,  27,
    27,  27,  23,  23,  23,  23,  20,  19,
    19,  19,  19,  30,  14,  19,  19,  19,
    19,   9,   9,   9,   9,  21,  21,  21,
    21,  18,  18,  18,  18,  24,  19,  19
};

stock TD_CharWidthOrZero(character)
{
    if (character < 0 || character > 175) return TDCharWidth[32];
    return TDCharWidth[character];
}

/* Lebar teks dalam satuan kanvas 640x480. */
stock Float:TD_TextWidth(const text[], Float:letterX)
{
    new total = 0;
    for (new index = 0, length = strlen(text); index < length; index++)
    {
        new character = text[index];
        if (character < 0 || character > 175) character = 32;
        total += TDCharWidth[character];
    }
    return float(total) * letterX;
}

/* Pangkas teks sampai muat maxWidth (satuan kanvas). Dipakai supaya string
   panjang tidak pernah menabrak elemen lain di baris yang sama. */
stock TD_FitText(dest[], destSize, const text[], Float:letterX, Float:maxWidth)
{
    strcpy(dest, text, destSize);
    if (TD_TextWidth(dest, letterX) <= maxWidth) return 1;

    new length = strlen(dest);
    while (length > 0 && TD_TextWidth(dest, letterX) > maxWidth)
    {
        length--;
        dest[length] = EOS;
    }
    return 0;
}

/* Versi angka: berapa karakter yang muat (untuk laporan audit). */
stock TD_FitCount(const text[], Float:letterX, Float:maxWidth)
{
    new buffer[128];
    strcpy(buffer, text, sizeof(buffer));
    new length = strlen(buffer);
    while (length > 0 && TD_TextWidth(buffer, letterX) > maxWidth)
    {
        length--;
        buffer[length] = EOS;
    }
    return length;
}
