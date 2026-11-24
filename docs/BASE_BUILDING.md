# Base Building — Rancangan Lengkap (v1)

> Dokumen ini adalah spesifikasi kerja, bukan ide. Setiap aturan di sini punya
> alasan dan bisa diverifikasi dari kode atau DB.

## 1. Kenapa fitur ini ada

Loop sekarang: keluar kamp → bunuh zombie → loot → ekstraksi → ulang. Semua progres
hilang begitu pemain keluar. Base Building menambah **lapisan investasi**: material
yang dikumpulkan dipakai untuk mengubah satu petak tanah menjadi sesuatu yang
melindungi pemain saat offline-nya tidak ada.

Tiga konsekuensi desain yang harus dijaga:

1. **Base tidak boleh jadi safe zone gratis.** Zombie tetap bisa merusaknya, jadi
   pemain harus merawat (repair) dan tidak bisa tidur tenang.
2. **Base tidak boleh jadi mesin uang.** Tidak ada struktur yang menghasilkan item
   otomatis tanpa risiko.
3. **Base tidak boleh menghapus kebutuhan keluar.** Semua material tetap datang dari
   zombie, peti, dan pedagang — tidak ada generator material.

## 2. Alur pemain

```
/kamp (hub) ── beli material / craft ──► /build di petak base
   │                                        │
   │                                        ├─ WALL   : menahan zombie
   │                                        ├─ GATE   : pintu, HP lebih rendah
   │                                        ├─ CRATE  : +20 slot stash
   │                                        ├─ BENCH  : crafting di base (tier 2)
   │                                        ├─ BED    : titik respawn base
   │                                        └─ TURRET : menembak zombie (tier 3)
   │
   └─ /base : tier, jumlah struktur, integritas, perintah /build /fix /bongkar
```

## 3. Aturan penempatan (validasi server, bukan client)

| Aturan | Nilai | Alasan |
|---|---|---|
| Radius petak base | 22 m dari titik petak | Mencegah wall menutup jalan umum |
| Jarak antar struktur | ≥ 2.0 m | Menghindari tumpang tindih object/collision |
| Titik harus punya tanah | `CA_RayCastLine` dari +120 m ke −80 m, model ≠ 0 | Tanpa ini struktur bisa melayang |
| Zona | bukan hub, bukan zona PvP, bukan interior | Base di dalam kamp = exploit safe zone |
| Kepemilikan | 1 base per karakter, 1 petak | Sederhana, mudah diaudit |
| Batas global | 40 struktur aktif per server | Budget object + tick |

Semua penolakan mengembalikan pesan spesifik (`[BUILD] Terlalu dekat dengan struktur lain (1.4 m)`),
bukan diam-diam gagal.

## 4. Struktur v1

Biaya dibayar dari inventory lewat `Inventory_Consume` (ledger alasan `build`), bukan
dari variabel pemain. Harga sengaja membuat pemain memilih: menembak (butuh peluru)
atau membangun (butuh scrap).

| id | Struktur | Material | HP | Tier | Maks | Perilaku |
|---|---|---|---|---|---|---|
| 1 | Wall | Scrap 6, Wood 4 | 2000 | 1 | 12 | Objek solid + collision ColAndreas; zombie memutarnya |
| 2 | Gate | Scrap 4, Wood 2 | 1200 | 1 | 2 | Sama, HP lebih rendah, label pintu |
| 3 | Crate | Scrap 3, Wood 5 | 800 | 1 | 3 | +20 slot stash per crate (maks 120) |
| 4 | Bench | Scrap 5, Electronics 2 | 900 | 2 | 1 | `/craft` boleh dipakai dalam 6 m |
| 5 | Bed | Scrap 4, Cloth 3 | 600 | 1 | 1 | Titik respawn base (hanya bila ≥2 wall) |
| 6 | Turret | Scrap 8, Electronics 4 | 700 | 3 | 2 | 8 damage/2 dtk ke zombie ≤30 m, memancarkan noise; malam butuh daya generator |
| 7 | Generator | Scrap 10, Electronics 6 | 1200 | 2 | 1 | BBM 20 L, hanya bakar saat malam, `/generator` isi dari jerrycan |

Visual: setiap tipe punya model sendiri di `gamemodes/modules/config/structures.inc`
(`gStructureModels`), dan pratinjau `/bangun` memakai model yang SAMA dengan hasil
`/pasang` — satu sumber kebenaran lewat `BaseBuild_Model(type)`.

| Tipe | Model | Keterangan |
|---|---|---|
| Wall | 19447 | panel dinding (sumber material `matte_slate` di `mapping.inc`) + `CA_CreateObject` |
| Gate | 1495 | pintu garasi, sama dengan `SHELTER_DOOR_MODEL` |
| Crate | 2932 | peti, sama dengan peti airdrop |
| Bench | 1996 | bangku kerja |
| Bed | 1793 | kasur |
| Turret | 1997 | dudukan mesin |
| Generator | 1676 | mesin pompa, sama dengan `world/garage.inc` |

### Panel bangun di layar kecil

Panel `/bangun` menutupi layar saat mapping. Tiga jalan keluar, tanpa mengubah
posisi objek yang sudah diatur:

| Aksi | Efek |
|---|---|
| tombol `UKURAN` di panel | skala 100% -> 80% -> 62% -> 48% (lantai: tombol >= 12/16 px, font >= 0.10) |
| `/ukuran` | sama dengan tombol UKURAN |
| `/panel kecil` / `/panel besar` | satu langkah turun / naik |
| `/panel off` / `/panel on` | sembunyikan / tampilkan panel; ghost + tombol fisik tetap jalan |

Ukuran panel diingat selama sesi dan kembali 100% saat pratinjau baru dimulai.
Tata letaknya diturunkan dari isi (bukan literal yang diskalakan), sehingga tidak
ada baris yang saling menumpuk; `tools/hud_preview/panel_layout.py` memverifikasi
8 kombinasi (2 gaya HUD x 4 skala) di setiap build.

### Objek menempel pada pemain atau dibekukan

Mode penempatan punya dua perilaku, dipilih pemain:

| Perilaku | Cara | Untuk apa |
|---|---|---|
| Menempel (default) | aktif otomatis | mengarahkan objek 2.5 m di depan, mengikuti arah hadap |
| Beku | `/beku` | objek diam di koordinat terakhir, pemain bisa berjalan mengelilinginya untuk memeriksa |

Objek yang "ikut lari" saat pemain berlari adalah mode menempel: posisi dihitung
dari `posisi pemain + 2.5 m ke depan` setiap tick. Itu disengaja — kalau tidak,
meletakkan objek sambil berjalan menjadi tidak mungkin. `/beku` mematikan
perhitungan itu tanpa menghapus objek.

**Objek tidak bisa dikecilkan**: SA-MP 0.3.7 tidak punya `SetObjectScale`. Kalau
dinding terasa terlalu besar, ganti modelnya (`/model daftar` untuk kandidat,
`/model <id>` untuk mencoba). Model yang lebih kecil terasa jauh lebih enak untuk
mapping daripada mengecilkan panel.

Semua tipe sekarang membuat **objek nyata** (dulu tipe non-solid hanya pickup, jadi yang
terlihat ikon koper melayang) plus pickup sebagai jangkar interaksi — pola yang sama
dipakai `loot/drops.inc`. Bila sebuah siluet tidak sesuai harapan, `/model <id>` saat
pratinjau aktif mengganti model untuk sesi itu dan menulis pilihannya ke server log.

## 5. Tier base

| Tier | Syarat naik | Membuka |
|---|---|---|
| 1 | default | Wall, Gate, Crate, Bed |
| 2 | Scrap 20, Wood 15, Chemical 5 | Bench, limit wall +4 |
| 3 | Scrap 40, Wood 25, Electronics 10 | Turret, HP struktur +25% |

Naik tier memakai `/upgrade` di dalam petak; biaya diambil dari inventory dan
disimpan di `bases.tier`.

## 6. Zombie vs struktur

- `Base_Tick` (2 dtk) mengambil posisi semua zombie **sekali** lalu:
  - barricade lama: 15 DPS × penyerang (maks 4)
  - struktur base: 12 DPS × penyerang (maks 4), hanya struktur terdekat per zombie
- HP 0 → objek + collision + baris DB dihapus, `Toast_NotifyAll` untuk struktur besar.
- Perbaikan `/fix` (Scrap 2) menambah 400 HP, dibatasi `health_max`.
- Horde Director: bila heat zona ≥ 45 dan pemain punya base di zona itu, pusat spawn
  digeser ke arah base (fase 2 — belum aktif di v1).

## 7. Persistensi

```sql
bases(id, char_id, tier, pos_x, pos_y, pos_z, ...)
base_structures(id, base_id, struct_id, pos_x, pos_y, pos_z, rot_z, health, created_at)
```

- Titik petak disimpan saat struktur pertama dibangun (posisi pemain saat itu).
- Semua baris struktur ikut terhapus saat base/akun dihapus (FK CASCADE, sudah diuji).
- Load struktur dilakukan saat `/base`, `/build`, atau `/fix` pertama — bukan saat boot,
  supaya server dengan 200 base tidak membuat 800 object di awal.
- Saat boot, tidak ada struktur dimuat: object dibuat per pemain yang mendekat.

## 8. UI & perintah

| Perintah | Fungsi |
|---|---|
| `/build` | daftar struktur: biaya, tier, sisa limit, status material |
| `/base` | tier, jumlah struktur, integritas rata-rata, hint perintah |
| `/base add <id>` | tambah anggota party jadi *member* base (owner/officer saja) |
| `/base officer <id>` | jadikan anggota *officer* (boleh buka gate + bangun) |
| `/base remove <id>` | keluarkan anggota (owner tidak bisa dikeluarkan) |
| `/base members` | daftar anggota base + peringkatnya |
| tombol **Y** di gate | buka/tutup gate, hanya owner/officer; menutup otomatis 8 detik |
| `/upgrade` | naik tier base |
| `/edit` | geser/putar struktur **milik base sendiri** yang paling dekat (<= 8 m), tanpa biaya material |
| `/geser [m]` | majukan/mundurkan target pratinjau (default 0,5 m; negatif = mundur) |
| `/putar [deg]` | putar arah struktur (default 15°, kumulatif 0-359) |
| `/tinggi [m]` | naik/turunkan target saat memasang struktur baru (default 0,25 m) |
| `/panel` (`/kursor`) | panggil/lepas kursor panel bangun tanpa mengubah posisi |
| `/pasang` | konfirmasi: material dipotong **di sini**, bukan saat memilih menu |
| `/batal` | batalkan pratinjau; tidak ada material terpakai |
| `/bongkar` | bongkar struktur terdekat (refund 50% material) |
| `/fix` | perbaiki barricade **atau** struktur terdekat |
| `/menu` | baris "Base: bangun / info" |

Semua dialog memakai satu modal aktif dan diperiksa ulang saat response
(material bisa habis di antara membuka dialog dan menekan tombol).

### 8.0 Pratinjau & edit posisi (laporan "struktur bergeser")

`/build` **tidak** lagi menaruh struktur seketika. Memilih tipe hanya memulai
**pratinjau**: satu *ghost object* + panel TextDraw. Material baru dipotong
saat `/pasang`.

Panel (kanan atas, 294x210 px) menampilkan enam baris angka yang **identik
dengan angka yang dipakai saat menyimpan**: SAMPING, MAJU, NAIK, ARAH, JARAK,
LANGKAH. Di mode geser, tiga baris pertama berubah menjadi koordinat dunia
X/Y/Z.

| Kontrol panel | Aksi |
|---|---|
| `-` / `+` baris SAMPING (X) | geser target ke kiri/kanan (maks ±6 m) |
| `-` / `+` baris MAJU (Y) | maju/mundur (1-6 m) |
| `-` / `+` baris NAIK (Z) | naik/turun (-1 m .. +3 m) |
| `<` / `>` | putar arah sesuai langkah |
| `-` / `+` baris JARAK | ubah jarak di mode bangun / dorong struktur di mode geser |
| `LANGKAH` | siklus 0,25 m / 5° -> 0,5 m / 15° -> 1,0 m / 45° |
| `PASANG` | konfirmasi (hijau = material cukup, merah = kurang) |
| `BATAL` | keluar tanpa biaya |
| `X` | lepas kursor, pratinjau tetap hidup |

Panel bisa diklik lewat `SelectTextDraw` (`/panel` memunculkan kursor lagi,
`ESC` melepasnya). Setiap tombol = satu sprite selectable + satu label *non*
selectable, karena label yang ikut selectable membuat kursor berebut hitbox dan
salah-klik.

Tombol fisik tetap ada untuk pemain tanpa mouse:

| Tombol | Aksi |
|---|---|
| `ALT` | maju satu langkah |
| `C` | putar satu langkah |
| `Y` | pasang / simpan |
| `H` | batal |

Versi Android memakai `/geser`, `/putar`, `/tinggi`, `/pasang`, `/batal`,
`/panel` — jalur kode yang sama (`BuildEdit_*`), bukan implementasi kedua.

Tata letak panel dijaga dua gate, bukan mata:

- `Selftest_CheckBuildPanel` — panel harus berakhir di kanan <= 640 px, dasar
  <= 250 px (di atas baris `SCRAP`/`BBM` yang mulai 330 px), dan tombol tidak
  boleh melewati 20 px dari tepi kartu.
- `Selftest_CheckBuildEdit` — lebar teks baris terburuk <= lebar kartu dikurangi
  padding (sekarang 195/270 px).
- `tools/hud_preview/build_panel.py` merender geometri panel ke HTML 640x480;
  skrip ini **gagal** bila koordinat di `build_edit.inc` digeser tanpa
  memperbarui alat, jadi gambar pratinjau tidak pernah menipu.

Dua cacat nyata yang diperbaiki di sini:

1. **Penempatan buta.** Dulu struktur langsung berdiri 2,5 m di depan pemain
   mengikuti sudut hadap *saat itu*, tanpa pratinjau dan tanpa cara memutar.
   Benda yang muncul tidak sama dengan yang dibayangkan pemain — inilah yang
   dilaporkan sebagai "item yang dibangun bergerak".
2. **Struktur berganda.** Penanda "sudah dimuat" disimpan **per pemain**, jadi
   setiap anggota baru dari base yang sama menjalankan `SELECT` lagi dan
   `OnRowsLoaded` mengisi slot kosong berikutnya: dua salinan setiap struktur di
   titik yang sama (`ssDbId` berbeda). Penanda sekarang per **base**
   (`BaseBuild_IsLoaded`/`MarkLoaded`/`ForgetLoaded`) dan `OnRowsLoaded`
   membuang slot lama lebih dulu bila base itu ternyata sudah termuat.

### 8.1 Base milik party (migrasi `024_base_members.sql`)

`bases.char_id` tetap satu-satunya pemilik (role 2). Anggota disimpan di
`base_members (base_id, char_id, role, added_by, added_at)`:

| Role | Nilai | Boleh |
|---|---|---|
| member | 0 | masuk base, pakai peti/fasilitas, **tidak** buka gate |
| officer | 1 | + buka gate, bangun, bongkar, perbaiki |
| owner | 2 | + atur anggota (dari `bases.char_id`, tidak bisa dikeluarkan) |

- Keanggotaan dimuat sekali saat `Base_Load` (`BaseMembers_Load`), bukan per tick.
- Gate adalah struktur `STRUCT_GATE`; saat dibuka object naik 3.2 unit dan object
  ColAndreas dihancurkan, saat tertutup dibuat ulang — tanpa itu pemain tetap
  tertahan secara fisik meski model sudah naik.
- Auto-close 8 detik lewat `BaseBuild_GateTick()` di `Base_TickBody` (500 ms).
- Pindah lobby (`/lobby`) memindahkan base ikut: baris `base_structures` tidak
  dihapus, visual dibuang lalu dimuat ulang dengan world id lobby baru
  (`BaseBuild_MigrateLobby`, ROADMAP 306).

## 9. Anti-abuse (wajib, bukan opsional)

| Ancaman | Mitigasi |
|---|---|
| Dupe material saat build | Consume A → Consume B → INSERT; gagal di tengah = refund lewat `Inventory_Grant(reason="build_refund")` |
| Build di dalam hub / safe zone | Cek zona pemain + titik, tolak |
| Wall menutup pintu keluar kamp | Radius 22 m + zona hub dilarang |
| Spam build untuk lag | Batas 40 struktur/server, cooldown 1.5 dtk per build |
| Struktur jadi tempat sembunyi AFK | Zombie tetap menyerang struktur, tidak ada immunity |
| Base jadi ATM | Tidak ada struktur penghasil item; crate hanya menambah slot |

## 10. Telemetri & verifikasi

- `item_ledger` alasan: `build`, `build_refund`, `build_dismantle`, `base_upgrade`,
  `repair`.
- `/diag` menampilkan jumlah struktur aktif + sisa budget.
- Verifikasi rilis:
  1. `sampctl build` exit 0.
  2. Boot log `[BUILD] 0 struktur dimuat (lazy)`.
  3. Uji SQL: insert struktur → restart → baris tetap ada; hapus akun → 0 baris.
  4. Client: `/build` → Wall muncul + zombie memutar; hancurkan dengan `/zspawn` → baris DB hilang.
  5. Crate +1 → `/stash` menampilkan kapasitas 80.

## 11. Konvensi "Pawn modern" yang dipakai

- Enum bertag (`E_STRUCTURE`, `E_STRUCT_SLOT`) supaya salah tipe tidak lolos compile.
- Tabel `const` untuk data statis (nama, biaya, HP) — bukan `if` berantai.
- Guard clause di awal fungsi (`if (!ok) return 0;`), bukan nesting dalam.
- Tidak ada magic number: semua ambang jadi `#define` di `config/`.
- Deklarasi `Float:` satu per satu; tidak ada `new a, Float:b, c` campur.
- Semua fungsi yang dipanggil timer wajib `public` + `forward`.
- Nama variabel domain (`structureHealth`, `plotCenterX`), bukan `tmp`/`data`.
- Tidak memakai `y_commands`/`y_hooks` (merusak hook FCNPC), sesuai invariant repo.
- Catatan: "pawnmodern" di sini = gaya & disiplin Pawn modern pada stack 0.3.7 + YSI 5.
  Migrasi ke open.mp/omp-stdlib adalah proyek terpisah (lihat §13).

## 12. Fase berikutnya (belum di v1)

1. **Blueprint base**: struktur tier 4–5 (dinding beton, menara pengawas, lampu sorot).
2. **Raid terarah**: horde memilih base dengan heat tertinggi, bukan zona.
   Heat minimum raid 30 (BASE_RAID_MIN_HEAT), cooldown 10 menit per lobby,
   reward hanya bila pemilik hadir ≤40 m dari base saat raid surut.
3. **Anggota base (party)**: izin akses gate/crate untuk pemain lain.
4. **Dekorasi**: skin object per pemain (butuh verifikasi model di client + cadb).
5. **Pajak kamp**: biaya harian per struktur untuk menjaga ekonomi tetap berputar.

## 13. Risiko yang sudah diketahui

| Risiko | Dampak | Rencana |
|---|---|---|
| Model visual belum dites di client | Struktur terlihat aneh / tak terlihat | v1 hanya pakai model terbukti; uji visual sebelum menambah decor |
| Object + ColAndreas per struktur | RAM/tick naik | Batas 40 struktur, load lazy, satu `GetPosition` per tick |
| Pemain membangun jauh lalu tidak kembali | Object menggantung | Cleanup saat logout bila pemain jauh dari base |
| Migrasi open.mp | Hook FCNPC & dialog berubah | Ditunda; semua kode baru tetap 0.3.7-safe |
