# Soak 8 menit — instans s1

- Bot diminta: **100**, spawn: **98**, durasi sesi: 8 menit
- Sampel: 17 (interval 30 dtk)
- RSS awal/akhir/puncak: 498.0 / 665.0 / 665.0 MB
- Pertumbuhan RSS: 167.0 MB
- Zombie aktif maks: 98
- Assertion RakNet: 0; baris errors.log baru: 0
- tick zombie: p95 1 ms, maks 1 ms
- tick director: p95 2 ms, maks 2 ms
- tick horde: p95 6 ms, maks 6 ms
- tick player: p95 2 ms, maks 2 ms
- tick hud: p95 2 ms, maks 2 ms

## Gate

| Gate | Hasil |
|---|---|
| server tetap hidup | PASS |
| 0 assertion RakNet / SIGABRT | PASS |
| spawn bot >= 90% (98/100) | PASS |
| tick p95 semua sistem < 20 ms | PASS |
| pertumbuhan RSS < 15% RSS awal | FAIL |

**Verdict: BELUM LULUS**
