# Soak 12 menit — instans s1

- Bot diminta: **100**, spawn: **0**, durasi sesi: 12 menit
- Sampel: 12 (interval 30 dtk)
- RSS awal/akhir/puncak: 569.0 / 0.0 / 569.0 MB
- Pertumbuhan RSS: -569.0 MB
- Zombie aktif maks: 100
- Assertion RakNet: 0; baris errors.log baru: 0
- tick zombie: p95 0 ms, maks 1 ms
- tick director: p95 1 ms, maks 1 ms
- tick horde: p95 1 ms, maks 1 ms
- tick player: p95 1 ms, maks 1 ms
- tick hud: p95 2 ms, maks 3 ms

## Gate

| Gate | Hasil |
|---|---|
| server tetap hidup | PASS |
| 0 assertion RakNet / SIGABRT | PASS |
| spawn bot >= 90% (0/100) | FAIL |
| tick p95 semua sistem < 20 ms | PASS |
| pertumbuhan RSS < 15% RSS awal | PASS |

**Verdict: BELUM LULUS**
