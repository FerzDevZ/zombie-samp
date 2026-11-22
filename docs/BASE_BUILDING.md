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

Visual: Wall/Gate memakai objek `1225` (model yang sudah terbukti punya collision di
cadb ini) + `CA_CreateObject`; Crate/Bench/Bed/Turret memakai dynamic pickup model
standar GTA (1210/1239/1240/1242) + label 3D. **Tidak ada model yang belum terverifikasi**
di build ini — alasan: aset yang salah model tidak bisa dideteksi server, jadi harus
diuji di client dulu sebelum dipakai.

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
| `/upgrade` | naik tier base |
| `/bongkar` | bongkar struktur terdekat (refund 50% material) |
| `/fix` | perbaiki barricade **atau** struktur terdekat |
| `/menu` | baris "Base: bangun / info" |

Semua dialog memakai satu modal aktif dan diperiksa ulang saat response
(material bisa habis di antara membuka dialog dan menekan tombol).

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
