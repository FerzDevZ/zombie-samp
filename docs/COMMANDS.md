# Daftar Perintah — LAST STAND: San Andreas Quarantine

Semua perintah bisa diketik di chat (PC) **dan** dipakai di klien
**SA-MP Mobile Android**. Tidak ada interaksi yang hanya tersedia lewat tombol
keyboard: setiap aksi punya perintah teks, dan sebaliknya setiap perintah punya
padanan tombol (`Y` untuk interaksi konteks di hotspot hub).

Format: `/nama [argumen]` — argumen dalam `<>` wajib, `[]` opsional.

---

## 1. Akun & karakter

| Perintah | Fungsi |
|---|---|
| `/daftar <password>` | Buat akun untuk nickname yang sedang dipakai |
| `/masuk <password>` | Login (alias `/login`) |
| `/akun` | Info akun: level admin, jumlah karakter, status ban |
| `/gantipass <lama> <baru>` | Ganti password akun |
| `/pulihkan <password> [kunci]` | Set ulang password; tanpa kunci hanya untuk akun sendiri |
| `/nama <nama_karakter>` | Buat karakter baru (memicu wizard 8 langkah) |
| `/char` | Pilih karakter (dialog TABLIST) |
| `/profil` | Profil karakter: umur, ras, asal, pekerjaan, keahlian |
| `/tutorial` | Ulangi onboarding |
| `/help` / `/bantuan` | Ringkasan perintah untuk pemain |

> Semua input akun lewat **chat**, bukan dialog input: klien mobile sering
> mengirim string kosong sehingga dialog input tidak bisa diandalkan.

---

## 2. Inventory, tas, hotbar

| Perintah | Fungsi |
|---|---|
| `/inv` (alias `/tas`, `/inventory`) | Buka inventory 20 slot + berat |
| `/pakai <slot\|nama item>` | Pakai/equip item |
| `/drop <slot> [jumlah]` | Jatuhkan item ke tanah (milik sendiri 60 detik) |
| `/ambil` | Ambil drop/karung terdekat |
| `/rapikan` | Gabungkan tumpukan sama-durability di inventory |
| `/bag` | Info tas: tier, kapasitas kg, model di punggung |
| `/hs <1-5>` | Pakai item hotbar (menyimpan **item id**, bukan nomor slot) |
| `/hotbar <1-5> <slot>` | Isi slot hotbar dari inventory |
| `/kapasitas` | Sisa kapasitas berat |
| `/itemlist` | Daftar item yang kamu bawa |
| `/stash` (alias `/peti`) | Buka stash pribadi (60 slot → 120 slot setelah upgrade) |
| `/partystash` | Peti party (40 slot, 240 kg) — hanya anggota party |

---

## 3. Crafting & ekonomi

| Perintah | Fungsi |
|---|---|
| `/craft` | Buka 17 resep; refund otomatis bila input gagal |
| `/bongkar <item>` | Dismantle item jadi bahan |
| `/perbaiki <item>` | Perbaiki alat/senjata dengan Toolbox/Repair Kit |
| `/toko` (alias `/shop`, `/store`) | Toko hub |
| `/jual <slot> [jumlah]` | Jual ke vendor terdekat |
| `/trade [id]` | Barter dua pihak: item + uang, konfirmasi ganda |
| `/bayar <id> <jumlah>` | Transfer uang |
| `/top` (alias `/leaderboard`, `/peringkat`) | Papan peringkat |
| `/kontrak` (alias `/contract`, `/misi`) | Kontrak progresif + reward |
| `/statistik` (alias `/stats`) | Statistik karakter |

---

## 4. Party & sosial

| Perintah | Fungsi |
|---|---|
| `/party` | Status party (maks 4 orang) |
| `/invite <id>` (alias `/undang`) | Undang pemain |
| `/accept` (alias `/terima`) | Terima undangan |
| `/leave` (alias `/keluar`) | Keluar party |
| `/kick <id>` (alias `/tendang`) | Keluarkan anggota (leader) |
| `/p <pesan>` | Chat party |
| `/radio <pesan>` | Siaran satu lobby (butuh item Radio) |
| `/revivekan` | Bangunkan teman downed di ≤ 3,5 m (butuh 1 Bandage) |
| `/teman revive` | Rekrutan NPC menolong kamu saat downed |
| `/bio [teks]` | Bio karakter |

---

## 5. Survival

| Perintah | Fungsi |
|---|---|
| `/makan` / `/minum` | Konsumsi dari inventory |
| `/kencing` (alias `/pee`, `/pipis`) | Buang air — mengurangi needs, meninggalkan noise |
| `/revive` / `/bangkitkan` | Revive diri saat downed (butuh Adrenaline) |
| `/ekstrak` (alias `/extract`) | Ekstraksi di titik ekstraksi dengan sampel |
| `/lootnode` | Ambil peti dunia terdekat |
| `/klinik` | Rawat luka di klinik hub |
| `/shelter` (alias `/rumah`) | Masuk shelter pribadi |
| `/base` / `/bangun` / `/dismantle` | Base building: lihat, bangun, bongkar |
| `/generator` (alias `/genset`) | Nyalakan/matikan generator base |
| `/raidbase` | Serang base pemain lain (raid window) |

---

## 6. Kendaraan

| Perintah | Fungsi |
|---|---|
| `/veh` (alias `/garasi`, `/garage`) | Buka garasi |
| `/kunci` | Kunci/buka kendaraan |
| `/lockpick` | Bobol kendaraan (butuh Lockpick) |
| `/isi` (alias `/bensin`, `/refuel`) | Di pom: buka menu isi (penuh / 10 / 25 / 50 L / jerrycan). Di luar pom: laporan BBM (liter, persen, sisa km, jarak pom) |
| `/isi [liter]` | Isi cepat sejumlah liter di pom ($3/L) |
| `/siphon` (alias `/sedot`) | Sedot BBM kendaraan lain |
| `/engine` | Hidupkan/matikan mesin |
| `/fix` (alias `/repair`) | Perbaiki kendaraan dengan kit |
| `/tarik` / `/angkut` | Derek / angkut kendaraan |

---

## 7. Navigasi & info

| Perintah | Fungsi |
|---|---|
| `/interact` (alias `/aksi`) | Interaksi konteks di hotspot hub (padanan tombol `Y`) |
| `/zone` | Zona sekarang + tier bahaya |
| `/menu` | Menu utama |
| `/compact` | Ringkas tampilan HUD |
| `/jam` | Waktu server (malam menaikkan spawn zombie) |
| `/diag` | Diagnostik klien: versi, mapping, build |
| `/selftest` | Jalankan selftest internal (36 cek) |

---

## 8. Admin (`admin_level ≥ 1`)

| Perintah | Fungsi |
|---|---|
| `/admin <kunci>` | Naik ke level admin dengan kunci runtime |
| `/superadmin <kunci>` (alias `/sadmin`, `/sa`) | Level superadmin |
| `/adminpanel` (alias `/adminhelp`) | Panel admin (dialog) |
| `/admin` | Daftar perintah admin |
| `/goto <id>` / `/gethere <id>` | Teleport ke pemain / tarik pemain |
| `/tppos <x> <y> <z>` | Teleport ke koordinat |
| `/freeze` / `/bekukan <id>` | Bekukan pemain |
| `/kick <id>` / `/ban <id>` / `/unban <nama>` | Moderasi |
| `/blokir <id>` | Blokir chat pemain |
| `/heal <id>` / `/fullheal <id>` | Pulihkan HP |
| `/god <id>` / `/kebal <id>` | Mode kebal |
| `/setcash <id> <jumlah>` | Set uang |
| `/giveitem <id> <item> [jumlah]` | Beri item |
| `/setadmin <id> <level>` | Set level admin (superadmin saja) |
| `/setsp <id> <jumlah>` / `/addxp <id> <jumlah>` | Set poin skill / tambah XP |
| `/setskill <id> <skill> <level>` | Set level skill |
| `/setlevel` / `/sethour <jam>` | Set level pemain / waktu server |
| `/spawnz <jumlah>` (alias `/zspawn`, `/zspawnz`) | Spawn zombie manual |
| `/zclear` (alias `/zombieclear`) | Hapus semua zombie |
| `/horde [zona]` | Paksa horde di zona |
| `/event` | Kendali event global |
| `/escort` | Kendali misi escort |
| `/trader` | Kendali trader keliling |
| `/inspect` (alias `/geledah`) | Lihat inventory pemain |
| `/restore` / `/recover` | Pulihkan karakter (superadmin) |
| `/bans` | Daftar ban |
| `/test` | Perintah uji internal |

> Level admin **hanya** dibaca dari `accounts.admin_level` di database. Kunci
> runtime hanya memberi level sesi; tidak ada jalur lain untuk mendapat akses.

---

## 9. Contoh alur pemain baru

```
/daftar rahasia123
/masuk rahasia123
# pilih karakter di dialog TABLIST
/interact            # ambil kit awal di hotspot hub
/craft               # buat bandage
/hs 1                # pakai dari hotbar
/zone                # cek zona sekarang
# keluar hub, bunuh zombie, ambil sampel
/ekstrak             # keluar hidup-hidup, loot masuk stash
/stash               # rapikan
/top                 # lihat peringkat
```

## 10. Aturan pemakaian di mobile

* Dialog selalu `TABLIST_HEADERS`: header di baris pertama, tombol navigasi
  (`<< Kembali`, `Tutup`) di baris terakhir.
* Tidak ada dialog input teks; semua isian lewat perintah chat.
* Setiap aksi penting punya perintah fallback (`/interact` untuk `Y`).
* Menutup dialog **tidak pernah** membuat pemain di-kick.
