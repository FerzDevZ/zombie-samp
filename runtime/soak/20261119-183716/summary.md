# Soak 10 menit — instans s1

- Bot diminta: **100**, spawn: **0**, durasi sesi: 10 menit
- Sampel: 11 (interval 30 dtk)
- RSS awal/akhir/puncak: 498.0 / 0.0 / 545.0 MB
- Pertumbuhan RSS: -498.0 MB
- Zombie aktif maks: 92
- Assertion RakNet: 0; baris errors.log baru: 0
- tick zombie: p95 1 ms, maks 1 ms
- tick director: p95 2 ms, maks 2 ms
- tick horde: p95 1 ms, maks 1 ms
- tick player: p95 2 ms, maks 2 ms
- tick hud: p95 3 ms, maks 3 ms

## Gate

| Gate | Hasil |
|---|---|
| server tetap hidup | PASS |
| 0 assertion RakNet / SIGABRT | PASS |
| spawn bot >= 90% (0/100) | FAIL |
| tick p95 semua sistem < 20 ms | PASS |
| pertumbuhan RSS < 15% RSS awal | PASS |

**Verdict: BELUM LULUS**
