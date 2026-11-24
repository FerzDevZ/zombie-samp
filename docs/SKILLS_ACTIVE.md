# Aktivasi Skill & Agent SAMP

Dihasilkan oleh `python3 tools/samp_skills_audit.py` — jangan diedit manual.

- Skill SAMP: **52** (29 terverifikasi, 23 N/A)
- Agent SAMP: **27**
- Probe rantai mapping→textdraw→logic: **22/22 PASS**

## Probe

| Probe | Domain | Hasil | Bukti | Skill |
|---|---|---|---|---|
| `map-streamer` | mapping | PASS | Objektif dinamis + preload Streamer_UpdateEx (anti jatuh tembus) — Streamer_UpdateEx dipakai, 29 CreateDynamicObject | `samp-streamer-plugin`, `samp-mapping-dynamic-objects-pro`, `samp-map-pipeline-2026` |
| `map-colandreas` | mapping | PASS | Raycast ColAndreas untuk verifikasi lantai/line-of-sight — 13 pemanggilan CA_RayCastLine: gamemodes/modules/core/selftest.inc:47:    new model = CA_RayCastLine(x, y, z + 100.0, x, y, z - 100.0, hitX, hitY, hitZ); | `samp-colandreas-physics` |
| `map-retexture` | mapping | PASS | Retexture objek dinamis (Textures Studio pipeline) — gamemodes/modules/world/mapping.inc:61:        SetDynamicObjectMaterial(obj, materialIndex, 19447, "wall_textures", "concrete_01", 0xFF141923); | `samp-mapping-dynamic-objects-pro`, `samp-streamer-plugin` |
| `td-zona-android` | textdraw | PASS | Posisi HUD di pita aman 320..446 untuk 3 gaya — GATE ANDROID: lulus — 38 Y + 38 X diperiksa di 3 gaya (pita 320..446) | `samp-android-safe-hud-layout`, `samp-textdraw-ui-craft`, `samp-mobile-android-cef-ui` |
| `td-lebar-640` | textdraw | PASS | Gate lebar teks 640px + larangan TextSize pada TD teks — GATE LULUS: semua baris muat dan tabel gate sinkron | `samp-textdraw-ui-craft`, `samp-textdraw-layout-debugger` |
| `td-galeri` | textdraw | PASS | Galeri TDUI mencerminkan literal kode HUD (13 halaman) — 13 halaman, 0 gagal, keluaran di docs/tdui | `samp-textdraw-studio-converter`, `samp-textdraw-fivem-modern-design` |
| `td-pool` | textdraw | PASS | Budget pool PlayerTextDraw <= 200 per pemain — 37 PlayerTextDraw dibuat (batas pool 256, target <= 200) | `samp-textdraw-ui-craft` |
| `td-cache` | textdraw | PASS | Cache teks HUD direset setiap builder berjalan — 3 builder, 2 HUD_ResetCaches | `samp-textdraw-ui-craft`, `samp-mobile-android-cef-ui` |
| `logic-dispatcher` | logic | PASS | Satu dispatcher perintah (tanpa tangga string) — dispatcher tunggal, 1 OnPlayerCommandText | `samp-command-processor` |
| `logic-dialog-lint` | logic | PASS | Lint dialog-unguarded + injeksi lolos self-test — [LINT] self-test 4/4 lulus (2 positif, 2 negatif) | `samp-dialog-system`, `samp-security-lint`, `samp-server-security-hardening` |
| `logic-timer` | logic | PASS | Tidak ada timer per-pemain; tick 500ms global — timer global TimerHandles, 0 timer per-pemain | `samp-timer-scheduling`, `samp-high-performance-tuning` |
| `logic-onplayerupdate` | logic | PASS | OnPlayerUpdate tidak memuat logika gameplay — 0 baris non-komentar di OnPlayerUpdate | `samp-high-performance-tuning`, `samp-zombie-survival-horde` |
| `logic-entity` | logic | PASS | Entitas dicatat & dibersihkan lewat EL_Track — 8 pemanggilan EL_Track (entity lifecycle) | `samp-entity-lifecycle`, `samp-entity-lifecycle` |
| `logic-persist` | logic | PASS | Skema persisten berversi lewat migrasi — 36 migrasi SQL | `samp-persistence-schema`, `samp-database-engineer` |
| `logic-mysql` | logic | PASS | Query DB threaded, tanpa query blocking di jalur publik — 99 mysql_tquery, 0 mysql_query blocking | `samp-mysql-database` |
| `logic-ysi` | logic | PASS | Iterasi YSI foreach + struktur data lokal — gamemodes/modules/player/party.inc:112:    foreach (new other : PlayerPool) | `samp-ysi-framework`, `samp-data-structures-pawn` |
| `logic-include-guard` | logic | PASS | Setiap include punya header guard — semua .inc punya include guard | `samp-include-library-design` |
| `logic-selftest` | logic | PASS | Selftest runtime lulus di instance berjalan — [SELFTEST] 74/74 PASS, 0 FAIL | `samp-testing-harness`, `samp-qa-engineer` |
| `logic-compile` | logic | PASS | Compile 0 warning 0 error + dua gate — [BUILD] OK — main.amx 10414673 byte, 0 warning, 0 error | `samp-pawn-compile-gate`, `samp-pawn-compile-verifier` |
| `logic-anticheat` | logic | PASS | Pemeriksaan anti-cheat sisi server + pencatatan flag — 3 pemanggilan sampler AC, flag tercatat ke anticheat_flags | `samp-anticheat-system` |
| `logic-arch` | logic | PASS | Arsitektur modular satu gamemode (urutan include terkunci) — 98 modul .inc, 1 gamemode tunggal | `samp-gamemode-architecture`, `samp-pawn-scripting` |
| `logic-servercfg` | logic | PASS | server.cfg + dua instance systemd berjalan — server.cfg lengkap; systemd: active active | `samp-server-configuration`, `samp-release-engineer` |

## Skill

| Skill | Status | Bukti |
|---|---|---|
| `samp-agent-evals` | N/A | eval agent belum disiapkan |
| `samp-android-safe-hud-layout` | TERVERIFIKASI | td-zona-android |
| `samp-anticheat-system` | TERVERIFIKASI | logic-anticheat |
| `samp-colandreas-physics` | TERVERIFIKASI | map-colandreas |
| `samp-command-processor` | TERVERIFIKASI | logic-dispatcher |
| `samp-crashdetect-debugger` | N/A | dipakai saat crash (ad-hoc) |
| `samp-custom-models-cdn` | N/A | belum ada custom model / FastDL |
| `samp-data-structures-pawn` | TERVERIFIKASI | logic-ysi |
| `samp-dialog-system` | TERVERIFIKASI | logic-dialog-lint |
| `samp-discord-connector` | N/A | belum ada connector Discord di repo |
| `samp-entity-lifecycle` | TERVERIFIKASI | logic-entity |
| `samp-filterscript-engineering` | N/A | belum ada filterscript terpisah |
| `samp-fivem-character-cinema-anim` | N/A | belum ada studio karakter |
| `samp-fivem-heist-crime-gang` | N/A | belum ada heist |
| `samp-fivem-housing-business-crafting` | N/A | belum ada housing/bisnis |
| `samp-fivem-inventory-metadata` | N/A | inventori ada tapi belum metadata penuh |
| `samp-fivem-oxlib-target-logic` | N/A | interaksi ada (interact/dispatcher.inc), radial belum |
| `samp-fivem-smartphone-mdt` | N/A | belum ada smartphone/MDT |
| `samp-fivem-vehicle-tuning-garage` | N/A | garasi ada (world/garage.inc), tuning belum |
| `samp-gamemode-architecture` | TERVERIFIKASI | logic-arch |
| `samp-greenfield-gamemode` | N/A | sudah lewat fase greenfield |
| `samp-high-performance-tuning` | TERVERIFIKASI | logic-timer; logic-onplayerupdate |
| `samp-http-json-integration` | N/A | belum ada integrasi HTTP eksternal |
| `samp-include-library-design` | TERVERIFIKASI | logic-include-guard |
| `samp-inventory-item-system` | N/A | inventori ada (inventory/), sistem item lengkap belum |
| `samp-legacy-openmp-migrator` | N/A | masih 0.3.7 (migrasi belum dijadwalkan) |
| `samp-map-pipeline-2026` | TERVERIFIKASI | map-streamer |
| `samp-mapping-dynamic-objects-pro` | TERVERIFIKASI | map-streamer; map-retexture |
| `samp-mobile-android-cef-ui` | TERVERIFIKASI | td-zona-android; td-cache |
| `samp-modern-creator-2026` | N/A | playbook, bukan artefak |
| `samp-modern-jobs-2026-logic` | N/A | pekerjaan belum dibangun |
| `samp-mysql-database` | TERVERIFIKASI | logic-mysql |
| `samp-pawn-compile-gate` | TERVERIFIKASI | logic-compile |
| `samp-pawn-scripting` | TERVERIFIKASI | logic-arch |
| `samp-persistence-schema` | TERVERIFIKASI | logic-persist |
| `samp-raknet-packet-sync` | N/A | Pawn.RakNet belum dipakai |
| `samp-roleplay-systems-bible` | N/A | mode zombie, bukan roleplay |
| `samp-security-lint` | TERVERIFIKASI | logic-dialog-lint |
| `samp-server-configuration` | TERVERIFIKASI | logic-servercfg |
| `samp-server-security-hardening` | TERVERIFIKASI | logic-dialog-lint |
| `samp-streamer-plugin` | TERVERIFIKASI | map-streamer; map-retexture |
| `samp-testing-harness` | TERVERIFIKASI | logic-selftest |
| `samp-textdraw-fivem-modern-design` | TERVERIFIKASI | td-galeri |
| `samp-textdraw-layout-debugger` | TERVERIFIKASI | td-lebar-640 |
| `samp-textdraw-studio-converter` | TERVERIFIKASI | td-galeri |
| `samp-textdraw-ui-craft` | TERVERIFIKASI | td-zona-android; td-lebar-640; td-pool; td-cache |
| `samp-timer-scheduling` | TERVERIFIKASI | logic-timer |
| `samp-ucp-web-panel-bridge` | N/A | UCP web belum dibangun (backlog) |
| `samp-voice-spatial-audio` | N/A | voicechat belum dipasang |
| `samp-vs-openmp-encyclopedia` | N/A | referensi, bukan artefak |
| `samp-ysi-framework` | TERVERIFIKASI | logic-ysi |
| `samp-zombie-survival-horde` | TERVERIFIKASI | logic-onplayerupdate |

## Agent

`samp-android-layout-guardian`, `samp-architecture-sage`, `samp-creator-2026`, `samp-crime-faction-architect`, `samp-database-engineer`, `samp-developer`, `samp-filterscript-master`, `samp-fivem-systems-engineer`, `samp-gamemode-architect`, `samp-legacy-migrator`, `samp-map-pipeline-engineer`, `samp-mapping-mlo-architect`, `samp-modern-jobs-architect`, `samp-pawn-compile-verifier`, `samp-pawn-wizard`, `samp-qa-engineer`, `samp-release-engineer`, `samp-security-anticheat`, `samp-security-hardener`, `samp-server-admin`, `samp-syntax-triager`, `samp-textdraw-fivem-master`, `samp-textdraw-layout-auditor`, `samp-textdraw-ui-designer`, `samp-ucp-fullstack-engineer`, `samp-voice-audio-engineer`, `samp-zombie-systems-engineer`
