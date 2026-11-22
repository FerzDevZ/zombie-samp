# Soak 4 menit — instans s1

- Bot diminta: **100**, spawn: **81**, durasi sesi: 4 menit
- Sampel: 9 (interval 30 dtk)
- RSS awal/akhir/puncak: 498.0 / 1260.0 / 1260.0 MB
- Pertumbuhan RSS: 762.0 MB
- Zombie aktif maks: 0
- Assertion RakNet: 0; baris errors.log baru: 0
- tick zombie: p95 0 ms, maks 0 ms
- tick director: p95 1 ms, maks 1 ms
- tick horde: p95 1 ms, maks 1 ms
- tick player: p95 1 ms, maks 1 ms
- tick hud: p95 1 ms, maks 1 ms

## Gate

| Gate | Hasil |
|---|---|
| server tetap hidup | PASS |
| 0 assertion RakNet / SIGABRT | PASS |
| spawn bot >= 90% (81/100) | FAIL |
| tick p95 semua sistem < 20 ms | PASS |
| pertumbuhan RSS < 15% RSS awal | FAIL |

**Verdict: BELUM LULUS**
