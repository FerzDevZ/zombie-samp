# Soak 3 menit — instans s1

- Bot diminta: **40**, spawn: **40**, durasi sesi: 3 menit
- Sampel: 7 (interval 30 dtk)
- RSS awal/akhir/puncak: 503.0 / 520.0 / 520.0 MB
- Pertumbuhan RSS: 17.0 MB
- Zombie aktif maks: 76
- Assertion RakNet: 0; baris errors.log baru: 0
- tick zombie: p95 1 ms, maks 1 ms
- tick director: p95 2 ms, maks 2 ms
- tick horde: p95 1 ms, maks 1 ms
- tick player: p95 0 ms, maks 0 ms
- tick hud: p95 1 ms, maks 1 ms

## Gate

| Gate | Hasil |
|---|---|
| server tetap hidup | PASS |
| 0 assertion RakNet / SIGABRT | PASS |
| spawn bot >= 90% (40/40) | PASS |
| tick p95 semua sistem < 20 ms | PASS |
| pertumbuhan RSS < 15% RSS awal | PASS |

**Verdict: LULUS**
