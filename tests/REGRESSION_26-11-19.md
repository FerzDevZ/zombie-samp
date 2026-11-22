# Checklist Regresi Manual — 26-11-19

Gate ROADMAP mewajibkan checklist manual per fase. File ini menutup tests/ yang
kosong dan menjadi skrip uji untuk SEMUA perbaikan sesi 26-11-19. Jalankan dengan
client nyata (PC dan SA-MP Mobile Android) sebelum produksi.

Cara pakai: centang [x] hanya setelah melihat hasil yang diharapkan di client.
Bukti tambahan: server_log.txt, logs/laststand.log, logs/errors.log, item_ledger.

## A. Auth & karakter (P0-1)
- [ ] Nick baru connect → chat "[AKUN] Nickname ini belum terdaftar" → ketik /daftar password123 → akun dibuat, wizard karakter muncul (8 langkah).
- [ ] Wizard selesai → kit pekerjaan masuk tas (cek /inv) dan tidak hilang setelah relog.
- [ ] /masuk password123 di nick terdaftar → spawn di kamp, posisi/uang kembali setelah relog.
- [ ] 5x password salah → kick.
- [ ] /pulihkan tanpa kunci → ditolak; 5x kunci salah → blokir 60 detik; reconnect TIDAK mereset blokir (per IP).
- [ ] Pesan login TIDAK menyebut kunci pemilik apa pun.
- [ ] Mobile: semua langkah auth bisa lewat chat tanpa dialog input.

## B. Trade (P0-2)
- [ ] Dua pemain /trade, masing-masing menawarkan 2 item berbeda + uang → konfirmasi ganda → item SILANG lengkap (cek /inv kedua pihak), uang berpindah benar.
- [ ] Cek item_ledger: ada trade_out DAN trade_in berpasangan untuk tiap item.
- [ ] Tas penerima penuh → trade ditolak sebelum konsumsi (tidak ada item hilang).
- [ ] Penerima pakai tas besar (62 kg) → trade item berat lolos (bukan ditolak 30 kg).
- [ ] Ubah penawaran setelah konfirmasi → konfirmasi kedua pihak reset.
- [ ] Salah satu disconnect saat sesi → trade batal, item tidak terconsume.

## C. Needs & tubuh (P0-3)
- [ ] Makan saat lapar 80 → mentok 100, tidak lebih.
- [ ] Minum berulang → haus mentok 100, bladder mentok 100.
- [ ] Biarkan lapar/haus habis → damage jalan, angka tidak negatif di HUD.
- [ ] /kencing mengosongkan bladder.
- [ ] Beban tas ≥90% → drain lebih cepat (peringatan muncul).

## D. Ban & moderasi (P1-4)
- [ ] /ban <id> 1 → target kick; login sebelum 1 menit → ditolak dengan sisa waktu.
- [ ] Setelah 1 menit → login sukses (auto-unban).
- [ ] Akun dengan ban permanen + ban sementara kedaluwarsa → TETAP ditolak (permanen menang).
- [ ] Admin level 1 tidak bisa ban admin lain; superadmin bisa.
- [ ] /unban (super) membuka ban; /bans menampilkan riwayat.

## E. Companion (P1-5, P1-6)
- [ ] /rekrut → rekrutan ikut & menembak zombie.
- [ ] /teman → "Tingkatkan" (baris 3) membuka upgrade, "Tolong dia" (baris 2) revive — sesuai label.
- [ ] Upgrade tier 1 → relog → tier TETAP 1 (HP 160, damage 19).
- [ ] Rekrutan tumbang → /teman revive dengan perban → hidup lagi.
- [ ] Rekrutan menembak zombie: XP/kontrak pemain TIDAK bertambah (anti AFK-farm), loot tetap jatuh.

## F. Admin & panel (P1-8, hardening)
- [ ] /superadmin <kunci baru dari admin_keys.cfg> → level 2; /admin <kunci admin> → level 1.
- [ ] Kunci salah 5x → blokir 60 dtk per IP (reconnect tidak mereset).
- [ ] /sa › Pemain: dengan >30 pemain online, halaman berikutnya muncul dan aksi tetap ke orang yang benar.
- [ ] /sa › kick pemain biasa (admin 1) → bisa; kick admin lain → ditolak.
- [ ] /sa › Teleport sebagai admin 1 → pesan "khusus superadmin", bukan no-op.
- [ ] Semua aksi panel tercatat di log dengan account pelaku.

## G. Inventory, stash, peti party, makam
- [ ] /rapikan menggabungkan tumpukan sejenis (durability sama), tidak melewati maxStack.
- [ ] Stash: simpan sampai batas kg → ditolak rapi; ambil saat tas penuh → ditolak, item tidak dobel.
- [ ] Peti party: dua anggota buka bersamaan → satu read-only; ambil cepat bergantian → tidak dupe (cek ledger + rollback log).
- [ ] Peti party: simpan item → cek DB party_stash; ambil semua → baris hilang utuh.
- [ ] Mati → makam jatuh dengan SEMUA isi tas; relog server saat makam aktif → makam + isi utuh (transaksi).
- [ ] Mayat elite: dua pemain geledah bersamaan → satu menunggu kunci.
- [ ] Drop sendiri: orang lain tidak bisa ambil 60 detik pertama; setelah 60 detik pickup terlihat publik dan bisa diambil siapa pun.

## H. Zombie, horde, raid
- [ ] Tembak M4 di zona → zombie sekitar investigate; melee senyap tidak menarik.
- [ ] Horde muncul saat heat tinggi; toast [HORDE].
- [ ] Raid base: zombie menyerang dinding (HP turun), tidak pergi mengejar pemain lain; reward $420+35XP hanya untuk pemilik base (char yang sama).
- [ ] Boss malam: bar HP muncul; <50% memanggil 5 runner sekali; hadiah sekali.
- [ ] /diag: tick p95 masuk akal; tidak ada spike saat horde.
- [ ] Rapid-fire (macro tembakan super cepat) → damage berhenti + flag RAPID_FIRE di /ac.

## I. Kendaraan & garasi
- [ ] Beli mobil → uang terpotong tepat sekali; beli saat uang dipakai trade paralel → transaksi batal, tidak negatif.
- [ ] Mobil liar ditinggal >10 menit jauh dari pemain → despawn (log [VEH]).
- [ ] BBM habis → mesin mati; /isi di pom → liter bertambah sesuai bayar.
- [ ] Mobil garasi hancur → asuransi (health 400 saat deploy ulang).
- [ ] /simpan lalu relog → fuel/health kembali sama.

## J. Dunia & event
- [ ] Supply drop: peti mendarat di tanah (tidak melayang), ikon terlihat.
- [ ] Blood moon dengan 2 lobby aktif: tiap lobby diserang di zona TERPADATNYA sendiri; lobby kosong tidak kena.
- [ ] Jam tenang: radius noise mengecil (zombie tidak datang dari jauh).
- [ ] Shelter: dua pemain beda base masuk /shelter → tidak bertemu (VW unik); keluar kembali ke posisi semula.
- [ ] Shelter custom: masuk → ruang 12x9 m (lantai kayu, dinding beton, plafon, peti, papan "SHELTER PRIBADI"), TIDAK jatuh tembus; jalan ke pintu selatan → auto-buka → melangkah keluar → kembali ke dunia; /shelter dari dalam → keluar normal; mati/disconnect di dalam → ruang dibersihkan (tidak ada objek bocor ke VW lain).
- [ ] /top menampilkan 5 kategori termasuk "KILL MUSIM INI (BULAN INI)"; skor musim = kill bulan berjalan saja.
- [ ] Generator: bakar BBM hanya malam; turret tanpa daya berhenti.

## K. Ekonomi & progres
- [ ] Kontrak selesai dengan tas penuh → klaim ditolak, kontrak TETAP bisa diklaim setelah tas kosong.
- [ ] Tutorial langkah selesai dengan tas penuh → reward diberikan pada aksi berikutnya, tidak hangus.
- [ ] Vendor: dialog tampil per baris (bukan satu baris panjang); beli/jual benar.
- [ ] Ekstraksi: 3 sampel → $460 + heal/cure; cooldown 90 detik per lobby; sampel tidak bisa masuk stash/peti.
- [ ] Revive player downed infeksi tinggi → tidak langsung turned lagi.

## L. Ketahanan
- [ ] Server restart saat pemain online → progres terakhir tersimpan (exit save).
- [ ] logs/errors.log kosong setelah 30 menit bermain normal.
- [ ] /selftest → 35/35 PASS.
- [ ] Mobile Android: semua interaksi inti (auth, inv, trade, stash, ekstraksi) bisa tanpa keyboard.

## M. Ronde audit swarm (26-11-19 lanjut-3) — uji manual wajib

Auth & admin (KRITIS yang ditutup):
- [ ] Connect memakai nick pemilik/admin yang OFFLINE → /masuk salah → level admin TIDAK aktif; /ban /god /giveitem /setadmin ditolak sebelum login+playing.
- [ ] Nick pemilik offline → /pulihkan passwordbaru TANPA kunci → DITOLAK; dengan kunci super benar → berhasil.
- [ ] Nick korban offline → /nama NamaBaru → ditolak ("Selesaikan /daftar atau /masuk dulu"); spoof dialog 2003/2010-2018 tanpa login → tidak ada efek.
- [ ] Registrasi baru → wizard jalan normal; login akun lama → pilih karakter normal; /akun saat playing → pesan info (bukan wizard).
- [ ] 5x password salah dari IP sama → blokir 60 detik; reconnect tidak mereset blokir; spam /masuk saat verify pending → pesan tunggu (bukan antrean bcrypt baru).
- [ ] Hapus sementara admin_keys.cfg → boot: login kunci NONAKTIF (pesan "tidak dikonfigurasi"), bukan kunci default. Kembalikan file setelah uji.
- [ ] Panel /sa: spoof dialogid 3509 (SA_WORLD) sebagai pemain biasa → tidak ada event terpicu.

Engine & damage:
- [ ] Lempar grenade ke anggota party → tidak ada damage; ke pemain di hub → tidak ada damage; admin god → kebal ledakan.
- [ ] Tabrak pemain dengan kendaraan di zona off-PvP (Idlewood/Ganton) → damage ditolak; di Downtown LS (PvP) → damage masuk.
- [ ] Saat hidup/downed, kirim request-class (F4) → tidak respawn; setelah mati benar → respawn normal.
- [ ] Mati → respawn: pistol 12 peluru (bukan 60), senjata tersimpan kembali, infeksi 0, cash terpotong biaya medis, streak reset.

Ekonomi & dupe:
- [ ] Jerrycan sisa 2L: /buang → /take → isi TETAP 2L (bukan 20L); toolbox aus tetap aus setelah trade/makam/peti party.
- [ ] Peti party: ambil slot yang sama 2x cepat → yang kedua ditolak ("sedang diproses"); isi peti konsisten setelah reload.
- [ ] Trade: ubah isi tas saat dialog terbuka → klik baris tetap menandai item yang sama (snapshot).
- [ ] Beli/jual vendor → cash & item konsisten; crash-kill server di tengah trade → setelah restart tidak ada item/uang ganda (uji staging).
- [ ] /top: kategori JAM & RANK menampilkan angka naik setelah bermain (autosave 2 menit); MUSIM tetap terisi.
- [ ] Scrap: kill zombie → ITEM_SCRAP masuk tas (bukan angka HUD mati); karakter lama dengan scrap DB → dikonversi sekali jadi item.
- [ ] Ekstraksi: bunuh zombie dari dalam hub → specimen TIDAK jatuh; cooldown per-pemain 90 detik ditegakkan.
- [ ] Raid base: picu raid lalu AFK di hub → reward hangus ("kamu tidak di tempat"); cooldown raid 10 menit.
- [ ] Upgrade base tier 2/3 tanpa Chemical/Electronics → ditolak dengan pesan kebutuhan lengkap.
- [ ] Kontrak dokumen: drop/pickup 1 dokumen berulang → progres naik tapi klaim DITOLAK tanpa dokumen fisik sejumlah target.
- [ ] Companion menghabisi zombie yang ditandai 1 tembakan pemain → loot jatuh, XP/kontrak TIDAK masuk pemain.
- [ ] Escort: mulai dalam 150m dari kamp → ditolak; dari luar → normal.
- [ ] Gigitan zombie: infeksi naik signifikan (60% peluang, 12-25 poin); mati → infeksi bersih; antibiotics menurunkan.
- [ ] /zspawn 7 → ditolak ("Tipe zombie 0-6").
- [ ] Blueprint suppressor tidak lagi jatuh dari loot/escort (diganti electronics).

Database:
- [ ] Boot: log "[TX] koneksi transaksi khusus aktif (handle terpisah...)"; mysql.log TANPA "duplicate connection" baru.
- [ ] Shutdown bersih (gmx): log "antrean SQL terkuras"; tidak ada "invalid connection handle"; progres terakhir tersimpan.
- [ ] EXPLAIN /top MUSIM memakai idx_killlog_kind_time_killer (range, Using index).

## N. Ronde code-reviewer regresif (26-11-19 lanjut-4) — uji manual wajib

- [ ] Mati membawa item → /geledah makam → dialog terbuka → klik item → item masuk tas (sebelumnya UI mati total).
- [ ] Bunuh Brute/Stalker → geledah mayat → klik baris item (baris ke-4 dst) → item masuk tas (sebelumnya semua klik ditolak).
- [ ] /kontrak → klik baris kontrak yang [KLAIM] → reward masuk (sebelumnya tidak pernah diproses); klik judul tidak mengklaim apa pun.
- [ ] Ban sementara 1 menit → tunggu kedaluwarsa saat online → pesan "masa blokir selesai" → /masuk password benar → BERHASIL login (sebelumnya buntu, harus reconnect).
- [ ] /sa → Teleport → "Shelter pribadi" → masuk ruang custom base (bukan jatuh dari langit); tanpa base → pesan menyiapkan base.
- [ ] /pulihkan penuh (admin) ke pemain turned → stage infeksi 0, tidak downed lagi.
- [ ] Masuk shelter → mati → respawn di bed/hub → TIDAK dilempar kembali ke posisi shelter; objek shelter hilang.
- [ ] Masuk shelter → disconnect → login lagi → objek shelter lama tidak bocor di VW.
- [ ] Trade: penerima tas hampir penuh (stack penuh item ditawarkan) → konfirmasi DITOLAK precheck; bila grant tetap gagal → item kembali ke pemberi (refund), tidak hangus.
- [ ] Peti party: tumpuk bandage melewati maxStack → otomatis pakai slot lain / ditolak bila penuh.
- [ ] Bangun 2 struktur cepat (lag DB) → keduanya punya ssDbId benar (health/fuel persist, bongkar menghapus baris DB).
- [ ] Beli garasi lalu relog cepat sebelum callback → cash tidak terpotong untuk karakter lain.
- [ ] Stash besar (70+ jenis): baris navigasi "Simpan/Rapikan" tetap terlihat & berfungsi.
- [ ] Turned di dalam shelter → runner spawn di kamp (tidak jatuh dari langit di dunia utama).

## O. Ronde fitur penutup (26-11-19 lanjut-5) — uji manual wajib

- [ ] Craft suppressor (Scrap 8 + Electronics 2 + Blueprint) → item masuk tas durability 40.
- [ ] Bawa suppressor → tembak pistol: zombie TIDAK datang dari jauh (radius 12 m); durability turun 1/tembakan; setelah 40 tembakan item habis + pesan.
- [ ] Blueprint suppressor jatuh lagi dari loot elite/rare/escort (sempat dihapus, kini bernilai).
- [ ] /klinik di kamp dengan infeksi 60 → biaya $170, infeksi 0, +20 HP; di luar kamp → ditolak; uang kurang → ditolak.
- [ ] /revivekan teman downed → channel 6 detik; bergerak menjauh → batal + bandage kembali; terkena damage → batal; diam dekat → teman bangkit +15 XP.
- [ ] Downed solo → bleed-out 25 detik (bukan 45); downed dalam party → 45 detik.
- [ ] Adrenaline → self-revive tetap instan.
- [ ] Vendor: bandage $25, medkit $90, antibiotics $70.
- [ ] /selftest → 35/35 PASS termasuk cek "transaksi item".

## P. Ronde temuan tengah audit (26-11-19 lanjut-8) — uji manual wajib

- [ ] Buka stash/vendor/craft di kamp → jalan ke zona bahaya → jawab dialog lama → aksi DITOLAK (harus balik kamp).
- [ ] Speedhack dalam mobil (>270 km/j) → flag SPEED/TELEPORT kendaraan muncul di /ac.
- [ ] Minigun/rapid-fire ilegal → diblokir SEBELUM noise (zombie tidak datang dari tembakan ditolak).
- [ ] Admin /freeze cheater → cheater memicu 3 flag AC → freeze AC berakhir → TETAP beku (freeze admin tidak batal).
- [ ] Trade: partner memakai itemnya sendiri setelah confirm → trade batal → item KAMU kembali utuh (refund).
- [ ] Daftar 4 akun dari IP sama dalam 24 jam → yang ke-4 ditolak.
- [ ] Superadmin /god ke pemain pre-auth → pemain disconnect → penghuni slot berikutnya TIDAK kebal.
- [ ] Geledah makam orang lain <2 menit → ditolak; restart server → eksklusivitas 2 menit TETAP berlaku; makam expire saat dialog terbuka → respons lama ditolak ("sudah tidak ada").
- [ ] Specimen ke peti party via slot basi → ditolak.
- [ ] Server uptime >24,8 hari (atau simulasi wrap) → loot tetap expire, grace tetap benar.
- [ ] /top & /stats tetap jalan; schema_migrations berisi 21 versi.
- [ ] Bunuh 5 zombie tanpa specimen → yang ke-6 DIJAMIN jatuh specimen (pity).
- [ ] Tutorial langkah SAMPLE dengan 3 specimen sudah di tas → langsung selesai tanpa pungut ulang.
- [ ] Vendor: "Jual SEMUA material" → semua material laku terjual satu klik, cash benar.
- [ ] Login dengan infeksi ≥80 → peringatan KRITIS + toast; HUD zona menampilkan [AMAN/WASPADA/BAHAYA].
- [ ] Pemain baru (level <5) menembak → heat zona TIDAK naik (zombie terdekat tetap datang).
- [ ] Farm 25 kill tier-1 dalam 5 menit → XP kill ke-11+ berkurang (taper).
- [ ] Level 50 tercapai → tidak naik lagi.
- [ ] Mati via timeout downed → statistik deaths bertambah.
- [ ] Grace newbie: PvP & needs decay terlindungi 10 menit pertama.

## Q. Crash SIGSEGV format-string + race login + load test bot (26-11-19 lanjut-9)

Latar: server segfault berulang (vsprintf pointer 0x45). Akar: data hasil
format diteruskan sebagai format string (nick bisa mengandung '%'), plus
TradePartner default 0 ≠ INVALID_PLAYER_ID. Fix: "%s" untuk joinLine/authLine,
Trade_Reset di connect, pending-login untuk /masuk yang mendahului lookup.

- [ ] Connect dengan nick mengandung '%' (mis. "Bot%sTest") → server TIDAK crash, log JOIN/AUTH tampil utuh.
- [ ] /masuk dikirim segera setelah connect (sebelum lookup selesai) → "Password diterima, menunggu data akun..." lalu login otomatis sukses tanpa ketik ulang.
- [ ] /masuk sebelum lookup untuk nick BELUM terdaftar → diarahkan ke /daftar, password pending tidak nyangkut.
- [ ] Pemain baru disconnect → TIDAK ada log "[TRADE] batal" palsu; state trade playerid 0 tidak ter-reset oleh disconnect orang lain.
- [ ] Dua pemain trade sungguhan → salah satu disconnect → trade batal benar, log char/partner sesuai.
- [ ] Load test: seed 120 --spread → swarm 100 (600s) + probe churn → ≥90% spawn, [PERF] semua sistem <20 ms, errors.log kosong, tidak ada SIGSEGV (lihat tools/loadtest/README.md).
- [ ] Event supply drop/blood moon dengan 100 bot di zona bahaya → peti jatuh/horde spawn tanpa crash; kematian bot menghasilkan grave/corpse normal.
- [ ] Uptime panjang: server hidup >15 menit dengan populasi zombie penuh (sebelumnya crash ~5 menit).

## Belum tercakup (butuh alat/kondisi khusus)
- Load test 100 pemain + 190 zombie (ROADMAP fase 8): butuh bot client / banyak mesin.
- Uji dupe agresif (packet replay): butuh client modifikasi — lakukan di staging.
- Crashdetect runtime error decode: hanya bila muncul crash (skill samp-crashdetect-debugger).
