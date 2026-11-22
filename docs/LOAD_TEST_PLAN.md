# Rencana Load Test — ROADMAP Fase 8

Target gate: 100 pemain bersamaan + 190 zombie, tick p95 < 20 ms, 0 crash selama 60 menit.
Status: BELUM dijalankan (butuh client nyata/bot di mesin lain). Dokumen ini membuat
gate bisa dieksekusi tanpa riset ulang.

## 1. Yang sudah tersedia di repo (tidak perlu dibuat lagi)
- Profiler internal: modules/core/perf.inc — [PERF] log tiap 60 dtk (akhir/puncak per sistem),
  /diag menampilkan ms tick tiap sistem. Ini sumber angka tick p95.
- Selftest 34 cek + /selftest ulang setelah load.
- Budget NPC dinamis (18+34/pemain, cap 150/190) + sync adaptif 80/100 ms.
- Autosave stagger 5 karakter/10 dtk; koneksi transaksi khusus (core/transaction.inc).
- scripts/instance.sh: instance s1/s2 terpisah (DB laststand_s2) untuk uji multi-instance.
- logs/laststand.log + logs/errors.log + server_log.txt untuk bukti.

## 2. Pilihan metode (urut realisme)

### A. Client nyata (paling valid, butuh manusia)
1. 10–20 pemain nyata (komunitas/tester) PC + Android, sesi 60 menit.
2. Skenario terarah: 5 party farming zona tier 2, 3 base building + raid, 2 trade paralel,
   1 boss malam, ekstraksi bergantian 2 lobby.
3. Catat /diag tiap 5 menit; kumpulkan logs setelah sesi.
4. Kelebihan: menguji jalur input/dialog/mobile sungguhan. Kekurangan: tidak mencapai 100.

### B. Bot protokol SA-MP (mencapai 100, butuh implementasi)
1. Implementasi client RakNet 0.3.7 (Python/Go) — koneksi + join + gerak acak + chat.
   Catatan: auth gamemode menahan pemain belum login di kamp (Account_LockPlayer),
   jadi bot tanpa login tetap membebani koneksi/streamer/kamp tetapi tidak memicu
   gameplay DB. Untuk beban gameplay penuh, bot harus menyelesaikan /daftar + /masuk
   via chat (protocol chat packet) — bisa di-script.
2. Jalankan 100 bot dari mesin terpisah (jangan satu mesin dengan server: CPU/network bias).
3. Ukur dari sisi server: [PERF], jumlah query/detik (SHOW PROCESSLIST + performance_schema),
   bandwidth per pemain (netstat/ss), FCNPC sync rate.
4. Risiko: implementasi protokol butuh waktu; mulai dari 20 bot, naik bertahap 50→100.

### C. samp-npc flood (parsial, paling murah, ada di repo)
1. samp-npc + npcmodes/recording: NPC connect sebagai player (IsPlayerNPC).
   Gamemode meng-guard NPC dari auth/DB, jadi ini menguji: koneksi, streamer,
   sync posisi, tick PlayerPool — BUKAN beban DB/gameplay.
2. Batas: maxnpc 200 dibagi dengan FCNPC zombie; uji saat zombie budget rendah.
3. Jalankan: ./samp-npc -h 127.0.0.1 -p 7777 -n BotName -m npcmodes/recording.rec
   (ulangi N proses; recording apa pun yang menggerakkan NPC).
4. Nilai: bukti cepat server tidak crash dengan 100+ koneksi paralel.

### D. Query-protocol monitor (pendamping, bukan beban)
- Samp query UDP (port 7777) untuk status pemain/koneksi dari luar; skrip monitor
  tiap 5 dtk mencatat jumlah pemain + timestamp ke CSV selama uji A/B/C.

## 3. Prosedur eksekusi (semua metode)
1. Backup dulu: ./scripts/backup.sh.
2. Restart server bersih; tunggu selftest 35/35 PASS.
3. Jalankan beban bertahap: 20 → 50 → 100 (atau maksimum metode), tahan 60 menit.
4. Selama uji: /diag tiap 5 menit (atau baca [PERF] log), pantau errors.log,
   SHOW PROCESSLIST untuk antrean query, ss -u untuk bandwidth UDP.
5. Setelah uji: /selftest ulang, cek item_ledger anomali (dupe), cek crash log
   (crashdetect bila aktif), bandingkan tick p95 vs target 20 ms.
6. Tulis hasil ke tests/LOAD_TEST_<tanggal>.md: metode, jumlah, durasi, tick p95,
   crash (ada/tidak), anomali DB, kesimpulan LULUS/GAGAL + tindak lanjut.

## 4. Kriteria lulus (gate ROADMAP fase 8)
- [ ] 100 koneksi bersamaan stabil 60 menit (metode B/C) ATAU sesi nyata terjadwal (A).
- [ ] Tick p95 < 20 ms pada beban puncak (bukti [PERF]/diag, bukan perasaan).
- [ ] 0 crash, 0 runtime error baru di errors.log.
- [ ] 0 dupe item (audit item_ledger: tidak ada delta tanpa pasangan).
- [ ] Antrean MySQL tidak tumbuh monoton (koneksi transaksi tidak jadi bottleneck).
- [ ] Mobile tester: tidak ada disconnect massal saat horde (sync 100 ms cukup).

## 5. Bila GAGAL
- Tick > 20 ms: matikan ambient zombie dulu (faktor terbesar), bukan fitur pemain;
  lihat sistem mana yang puncak di /diag; slice timer lebih kecil.
- Antrean TX tumbuh: naikkan batch autosave lebih jarang, atau gabungkan ledger
  multi-baris per transaksi (satu INSERT multi-VALUES untuk beberapa mutasi).
- Bandwidth mobile putus: turunkan FCNPC sync ke 120 ms saat >= 50 pemain
  (threshold baru di Zombie_UpdateSyncRate), kurangi streamdistance objek.
