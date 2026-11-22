# Kontribusi

Terima kasih sudah mau ikut. Repo ini server SA-MP yang benar-benar jalan, jadi
standarnya lebih ketat dari repo contoh: setiap perubahan harus bisa dibuktikan.

## Sebelum mulai

1. Baca [`MEMORY.md`](MEMORY.md). Isinya invariant hasil post-mortem — banyak
   aturan di sana lahir dari crash produksi, bukan preferensi gaya.
2. Baca [`ARCHITECTURE.md`](ARCHITECTURE.md) untuk peta modul.
3. `docs/INSTALL.md` untuk menyiapkan environment.

## Alur kerja

```bash
git clone https://github.com/FerzDevZ/zombie-samp.git
cd zombie-samp
docker compose up -d
cp mysql.ini.example mysql.ini && cp mysql.ini scriptfiles/mysql.ini
sampctl build                  # wajib 0 error
./scripts/instance.sh init dev && ./scripts/instance.sh start dev
```

## Gate yang wajib lulus sebelum PR

| Gate | Perintah | Syarat |
|---|---|---|
| Build | `sampctl build` | 0 error (1 warning `colandreas.inc:603` diterima) |
| Selftest | log boot / `/selftest` | `36/36 PASS, 0 FAIL` |
| Soak | `STAGGER=0.9 ./scripts/soak.sh 5 30 s1` | semua gate PASS |
| Memori | `tools/loadtest/mem_probe.sh 30 120` | RSS tidak naik > 15 % |

## Aturan kode yang tidak bisa dinegosiasi

* Tidak ada gameplay di `OnPlayerUpdate`; logika berat masuk `y_timers`.
* Semua log memakai `printf("%s", buffer)` — `print(buffer)` yang mengandung `%`
  menyebabkan SIGSEGV.
* `FCNPC_Create` hanya setelah `Npc_SlotAvailable()`; zombie diparkir
  (`Zombie_Park`), tidak di-`FCNPC_Destroy` di dalam callback.
* Penghapusan entitas lewat antrean (`Zombie_QueueDestroy`), bukan langsung.
* Teleport selalu diikuti `AC_ClearSample()`.
* Query MySQL selalu threaded; tidak ada query sinkron di callback pemain.
* Nominal item tidak pernah ditukar; item id permanen.
* Nama simbol Pawn ≤ 31 karakter.
* Tidak ada `for (i = 0; i < MAX_PLAYERS; ...)` — pakai iterator.

## Format commit

Conventional Commits: `feat:`, `fix:`, `perf:`, `docs:`, `test:`, `chore:`.
Sertakan angka hasil ukur di badan commit bila perubahan menyangkut performa
(mis. `RSS 498 → 552 MB datar, 100 bot 10 menit`).

## Melaporkan bug

Sertakan: commit, langkah reproduksi, potongan `runtime/<instans>/server_log.txt`,
dan `fcnpc-crash_*.txt` bila ada crash. Tanpa log, laporan sulit ditindak.
