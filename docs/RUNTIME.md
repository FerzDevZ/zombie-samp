# Menjalankan server lokal

Gamemode dikompilasi untuk **SA-MP 0.3.7** (`pawn.json` → `runtime.version`) supaya bisa diuji
dengan server 32-bit biasa, walaupun target produksi open.mp.

## 1. Build

```bash
cd /home/boxd/zombie-samp
sampctl build
```

Keluaran: `gamemodes/main.amx`. Build harus 0 error. Satu warning `Iter_SafeRemove_1` berasal dari
`include/colandreas.inc` (pihak ketiga) dan tidak memengaruhi runtime.

## 2. Plugin (.so) — tidak disimpan di repo

`plugins/` di-ignore git. Unduh manual, versi yang sudah teruji:

| Plugin | Versi | Sumber | Nama file |
|---|---|---|---|
| Streamer | 2.9.6 | `samp-incognito/samp-streamer-plugin` releases | `streamer.so` |
| sscanf | 2.13.8 | `Y-Less/sscanf` (`sscanf-2.13.8-linux.tar.gz` → `components/sscanf.so`) | `sscanf.so` |
| FCNPC | 2.0.11 | `ziggi/FCNPC` (`FCNPC-2.0.11.zip` → `plugins/FCNPC.so`) | `FCNPC.so` |
| ColAndreas | 1.5.0 | `Pottus/ColAndreas` (`ColAndreas_static_ubuntu.zip`) | `ColAndreas.so` |
| samp_bcrypt | 0.4.x | `Sreyas-Sreelal/samp-bcrypt` | `samp_bcrypt.so` |

## 3. Konfigurasi runtime (`pawn.json`)

`sampctl run` menulis ulang `server.cfg` setiap kali, jadi ubah `runtime` di `pawn.json`:

- `maxnpc 190` dan `maxplayers 200` — FCNPC memperingatkan crash bila `maxplayers < maxnpc`.
  Total id NPC tetap harus di bawah `MAX_ZOMBIE_SLOTS` (512).
- `plugins` harus memuat `streamer.so` **pertama**.
- `rcon_password` masih placeholder `localdev-only-change-me`; ganti sebelum server menyentuh IP publik.

## 4. Data ColAndreas (sudah terpasang)

`scriptfiles/colandreas/ColAndreas.cadb` (54 MB) **sudah ada** di mesin ini, diambil dari
release ColAndreas v1.6.0 (`ColAndreas.cadb.zip`) dan dipasangkan dengan
`plugins/ColAndreas.so` dari release yang sama (static, 32-bit, tanpa dependency Bullet).

Aturan penting:

- **Nama folder harus huruf kecil**: plugin mencari `scriptfiles/colandreas/ColAndreas.cadb`.
  Di Linux `scriptfiles/ColAndreas/` (huruf besar) tidak ditemukan dan `CA_Init()` gagal diam-diam.
- File `.cadb` dan plugin harus **satu versi**. Mengganti plugin tanpa mengganti `.cadb`
  menghasilkan `CA_Init() == 0` walau file ada.
- `.cadb` tidak masuk repo (`.gitignore`), jadi mesin/deploy baru harus menyalinnya manual.

Verifikasi boot (baris ini wajib muncul):

```
[BOOT] ColAndreas aktif: raycast uji model 4830 di z 12.54.
```

Kalau yang muncul `[BOOT] ColAndreas tanpa data .cadb: ...` berarti file tidak ditemukan
atau beda versi. Kalau muncul `Raycast uji meleset`, file ada tapi tidak cocok dengan plugin.

Efek setelah aktif: spawn zombie memakai tinggi tanah asli, LOS damage dicek lewat raycast,
FCNPC mode RAYCAST memutar zombie melewati tembok, dan barricade buatan pemain
(`CA_CreateObject` dengan `add = true`) ikut jadi penghalang.

## 5. Jalankan

```bash
sampctl run
```

Log yang benar:

```
[BOOT] LAST STAND siap. Zombie budget 190, zone 10.
```

Perintah dalam game: `/help`, `/interact`, `/zone`, `/zspawn`, `/horde`, `/ac`, `/revive`
(empat terakhir butuh admin/rcon).

## MySQL (Fase 2)

`mysql.so` R41-4 punya dependensi `log-core.so`; tanpa itu plugin gagal dengan
`Failed (log-core.so: cannot open shared object file)`.

```
docker compose up -d                 # MariaDB 10.11 + migrasi 001_init.sql otomatis
cp mysql.ini.example mysql.ini       # isi kredensial lokal, tidak pernah di-commit
./scripts/run-server.sh              # sudah mengeset LD_LIBRARY_PATH
```

Migrasi dijalankan otomatis oleh image saat volume pertama kali dibuat. Untuk
menjalankannya ulang secara manual:

```
docker exec -i laststand-mysql mariadb -ulaststand -p change_me laststand < database/migrations/001_init.sql
```
