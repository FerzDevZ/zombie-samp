# Soak 20 menit — instans s1

- Bot diminta: **100**, spawn: **0**, durasi sesi: 20 menit
- Sampel: 9 (interval 30 dtk)
- RSS awal/akhir/puncak: 498.0 / 0.0 / 1504.0 MB
- Pertumbuhan RSS: -498.0 MB
- Zombie aktif maks: 89
- Assertion RakNet: 0; baris errors.log baru: 0
- tick zombie: p95 1 ms, maks 1 ms
- tick director: p95 16 ms, maks 16 ms
- tick horde: p95 2 ms, maks 2 ms
- tick player: p95 2 ms, maks 2 ms
- tick hud: p95 2 ms, maks 2 ms

## Gate

| Gate | Hasil |
|---|---|
| server tetap hidup | PASS |
| 0 assertion RakNet / SIGABRT | PASS |
| spawn bot >= 90% (0/100) | FAIL |
| tick p95 semua sistem < 20 ms | PASS |
| pertumbuhan RSS < 15% RSS awal | PASS |

**Verdict: BELUM LULUS**
