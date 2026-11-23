#if defined _ls_lore_included
    #endinput
#endif
#define _ls_lore_included

/* Dokumen lore (ROADMAP 332).
   Teks ditulis di sini, bukan di database: dokumen dibaca pemain kapan saja dan
   tidak boleh hilang kalau DB putus. Indeks dipilih deterministik dari char id +
   jam berjalan supaya dua pemain tidak selalu mendapat cerita yang sama, tapi
   dokumen yang sama tidak berubah isinya setiap kali dibaca. */

#define LORE_DOCUMENT_COUNT 12
#define LORE_TEXT_MAX       160

new const LoreDocument[LORE_DOCUMENT_COUNT][LORE_TEXT_MAX] = {
    "CATATAN POS 14: Karantina dimulai 03.12. Gerbang utara jebol lebih dulu. Yang masih hidup, jangan pakai jalan utama.",
    "SURAT TULIS TANGAN: Kalau kau membaca ini, dapur umum di gudang D sudah kosong sejak Selasa. Cari kaleng, bukan roti.",
    "LAPORAN MEDIS: Infeksi stadium dua tidak menunjukkan gejala selama sembilan jam. Jangan percaya pada yang tidak tergigit.",
    "DAFTAR NAMA: Sepuluh nama, sembilan dicoret. Di bawahnya: 'Yang terakhir jangan menunggu aku. Bawa kuncinya.'",
    "PENGUMUMAN ROBEK: Malam ini listrik padam pukul sepuluh. Generator besar hanya untuk ruang isolasi. Dilarang menyalakan lampu.",
    "BUKU HARIAN PENJAGA: Mereka tidak menyerang cahaya. Mereka menyerang suara. Hentikan langkah berat di koridor logam.",
    "PESAN RADIO TERAKHIR: Kalau kalian dengar ini, jalur ekstraksi pindah ke dermaga selatan. Kami tidak bisa menunggu lagi.",
    "CATATAN MEKANIK: Pintu besi butuh empat engkol. Bawa satu selalu. Yang tertinggal di dalam tidak pernah keluar lagi.",
    "SURAT ANAK: Ayah bilang jangan buka pintu untuk siapa pun yang tidak mengetuk tiga kali. Aku sudah tiga hari tidak mengetuk.",
    "LAPORAN GUDANG: Persediaan air sembilan belas galon. Setelah itu, penyaring di atap harus diperbaiki atau kita minum dari talang.",
    "CATATAN SATPAM: Ada yang membuka pagar belakang setiap malam dan menutupnya lagi. Aku berhenti memeriksa setelah menemukan jejak kaki.",
    "LEMBAR TERAKHIR: Kalau kota ini jatuh, bawa dokumen ini ke utara. Mereka perlu tahu apa yang sebenarnya keluar dari laboratorium."
};

stock Lore_IndexForPlayer(playerid)
{
    if (playerid < 0 || playerid >= MAX_PLAYERS) return 0;
    new seed = ActiveCharId[playerid] + (gettime() / 3600);
    if (seed < 0) seed = -seed;
    return seed % LORE_DOCUMENT_COUNT;
}

stock Lore_ShowDocument(playerid)
{
    if (!IsPlayerConnected(playerid)) return 0;

    new index = Lore_IndexForPlayer(playerid);
    if (index < 0 || index >= LORE_DOCUMENT_COUNT) index = 0;

    new line[192];
    format(line, sizeof(line), "[DOKUMEN] %s", LoreDocument[index]);
    SendClientMessage(playerid, UI_ACCENT_PRIMARY, line);
    SendClientMessage(playerid, UI_TEXT_MUTED,
        "[DOKUMEN] Simpan atau jual $180 di vendor. Baca ulang kapan saja dengan /dokumen.");
    return 1;
}
