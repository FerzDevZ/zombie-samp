# Aktivasi Skill & Agent SAMP

Dihasilkan oleh `python3 tools/samp_skills_audit.py` — jangan diedit manual.

- Skill SAMP: **76** (45 terverifikasi probe, 7 referensi bergate, 24 N/A absen-terbukti)
- Agent SAMP: **27** (frontmatter + prompt sinkron OK)
- Probe terdefinisi: **42** (42 PASS, 0 FAIL)
- Skill tanpa bukti: **0**

## Probe

| Probe | Domain | Hasil | Bukti | Skill |
|---|---|---|---|---|
| `map-streamer` | mapping | PASS | Objektif dinamis + preload Streamer_UpdateEx (anti jatuh tembus) — Streamer_UpdateEx dipakai, 29 CreateDynamicObject | `samp-streamer-plugin`, `samp-mapping-dynamic-objects-pro`, `samp-map-pipeline-2026` |
| `map-colandreas` | mapping | PASS | Raycast ColAndreas untuk verifikasi lantai/line-of-sight — 13 pemanggilan CA_RayCastLine: gamemodes/modules/core/selftest.inc:47:    new model = CA_RayCastLine(x, y, z + 100.0, x, y, z - 100.0, hitX, hitY, hitZ); | `samp-colandreas-physics` |
| `map-retexture` | mapping | PASS | Retexture objek dinamis (Textures Studio pipeline) — gamemodes/modules/world/mapping.inc:61:        SetDynamicObjectMaterial(obj, materialIndex, 19447, "wall_textures", "concrete_01", 0xFF141923); | `samp-mapping-dynamic-objects-pro`, `samp-streamer-plugin` |
| `td-zona-android` | textdraw | PASS | Posisi HUD di pita aman 320..446 untuk 3 gaya — GATE ANDROID: lulus — 39 Y + 39 X diperiksa di 3 gaya (pita 320..446) | `samp-android-safe-hud-layout`, `samp-textdraw-ui-craft`, `samp-mobile-android-cef-ui` |
| `td-lebar-640` | textdraw | PASS | Gate lebar teks 640px + larangan TextSize pada TD teks — GATE LULUS: semua baris muat dan tabel gate sinkron | `samp-textdraw-ui-craft`, `samp-textdraw-layout-debugger` |
| `td-galeri` | textdraw | PASS | Galeri TDUI mencerminkan literal kode HUD (13 halaman) — 13 halaman, 0 gagal, keluaran di docs/tdui | `samp-textdraw-studio-converter`, `samp-textdraw-fivem-modern-design` |
| `td-pool` | textdraw | PASS | Budget pool PlayerTextDraw <= 200 per pemain — 38 PlayerTextDraw dibuat (batas pool 256, target <= 200) | `samp-textdraw-ui-craft` |
| `td-cache` | textdraw | PASS | Cache teks HUD direset setiap builder berjalan — 3 builder, 2 HUD_ResetCaches | `samp-textdraw-ui-craft`, `samp-mobile-android-cef-ui` |
| `logic-dispatcher` | logic | PASS | Satu dispatcher perintah (tanpa tangga string) — dispatcher tunggal, 1 OnPlayerCommandText | `samp-command-processor` |
| `logic-dialog-lint` | logic | PASS | Lint dialog-unguarded + injeksi lolos self-test — [LINT] self-test 4/4 lulus (2 positif, 2 negatif) | `samp-dialog-system`, `samp-security-lint`, `samp-server-security-hardening` |
| `logic-timer` | logic | PASS | Tidak ada timer per-pemain; tick 500ms global — timer global TimerHandles, 0 timer per-pemain | `samp-timer-scheduling`, `samp-high-performance-tuning` |
| `logic-onplayerupdate` | logic | PASS | OnPlayerUpdate tidak memuat logika gameplay — 0 baris non-komentar di OnPlayerUpdate | `samp-high-performance-tuning`, `samp-zombie-survival-horde` |
| `logic-horde-raid-fsm` | horde | PASS | Raid base sebagai state machine: flag/timestamp/pemilik + satu penggerak — state=3/3 | driver_tick=True pemanggil=1 | status_nol_dulu=True | clamp_wrap=True | timeout=True | `samp-zombie-survival-horde` |
| `logic-horde-budget` | horde | PASS | Budget zombie: cap di pembuat+director, spawn batch, despawn deterministik — MAX_ZOMBIES=190 (<1024: True) | cap_buat=True | cap_director=True | batch_break=2 | pool=True | despawn=True | `samp-zombie-survival-horde`, `samp-high-performance-tuning` |
| `logic-horde-payout` | horde | PASS | Reward horde/raid dibayar server-side setelah objektif terverifikasi, sekali — sekali=True | tersambung=True | char_sama=True | lobby_sama=True | dekat_40m=True | cash_server=True | tanpa_GivePlayerMoney=True | pemanggil=1 | `samp-zombie-survival-horde` |
| `logic-persist` | logic | PASS | Skema persisten berversi lewat migrasi — 36 migrasi SQL | `samp-persistence-schema`, `samp-database-engineer` |
| `logic-mysql` | logic | PASS | Query DB threaded, tanpa query blocking di jalur publik — 99 mysql_tquery, 0 mysql_query blocking | `samp-mysql-database` |
| `logic-ysi` | logic | PASS | Iterasi YSI foreach + struktur data lokal — gamemodes/modules/player/party.inc:112:    foreach (new other : PlayerPool) | `samp-ysi-framework`, `samp-data-structures-pawn` |
| `logic-include-guard` | logic | PASS | Setiap include punya header guard — semua .inc punya include guard | `samp-include-library-design` |
| `logic-selftest` | logic | PASS | Selftest runtime lulus di instance berjalan — [SELFTEST] 74/74 PASS, 0 FAIL | `samp-testing-harness`, `samp-qa-engineer` |
| `logic-compile` | logic | PASS | Compile 0 warning 0 error + dua gate — [BUILD] OK — main.amx 10415818 byte, 0 warning, 0 error | `samp-pawn-compile-gate`, `samp-pawn-compile-verifier` |
| `logic-anticheat` | logic | PASS | Pemeriksaan anti-cheat sisi server + pencatatan flag — 3 pemanggilan sampler AC, flag tercatat ke anticheat_flags | `samp-anticheat-system` |
| `logic-arch` | logic | PASS | Arsitektur modular satu gamemode (urutan include terkunci) — 98 modul .inc, 1 gamemode tunggal | `samp-gamemode-architecture`, `samp-pawn-scripting` |
| `logic-servercfg` | logic | PASS | server.cfg + dua instance systemd berjalan — server.cfg lengkap; systemd: active active | `samp-server-configuration`, `samp-release-engineer` |
| `feat-moderation` | gameplay | PASS | Sanksi admin berjenjang + jejak audit — ban/mute/warn + AdminAudit_Log tersambung | `samp-admin-moderation` |
| `feat-logging` | gameplay | PASS | Log terstruktur + audit trail admin ke DB — 180 baris Log_*(), tabel admin_actions dipakai | `samp-logging-audit` |
| `feat-anticheat-deep` | gameplay | PASS | Deteksi berlapis (DPS, sampler, handler flag) — DPS tracker + AC_StoreFlag/AC_HandleFlag | `samp-anticheat-deep` |
| `feat-crafting` | gameplay | PASS | Resep + bahan + UI crafting — 22 baris resep + CraftUI_Show | `samp-crafting-system` |
| `feat-death` | gameplay | PASS | Kematian dicatat (jenis, posisi, pembunuh) — Deaths_Record + INSERT INTO deaths + Deaths_ShowLast | `samp-death-respawn` |
| `feat-medical` | gameplay | PASS | Luka berdarah + kebutuhan (haus/lapar/infeksi) — Wounds_Add/Wounds_Tick + modul needs | `samp-medical-ems` |
| `feat-economy` | gameplay | PASS | Aliran uang lewat vendor/trade/kontrak — vendor + trade + kontrak (uang tertutup) | `samp-economy-simulation` |
| `feat-inventory` | gameplay | PASS | Registry item + inventori + hotbar — 102 referensi item, Inventory_/Hotbar_/Stash_ ada | `samp-inventory-item-system` |
| `feat-vehicle` | gameplay | PASS | Siklus kendaraan persisten (tangki, garasi, mod) — register + bahan bakar + garasi + modifikasi | `samp-vehicle-system` |
| `feat-daynight` | gameplay | PASS | Siklus cuaca + jam server tersinkron — World_SetWeather/SetHour; 12 SetWeather, 4 SetWorldTime | `samp-weather-time` |
| `feat-npc` | gameplay | PASS | NPC zombie: budget pool + nametag selalu disembunyikan — 4 FCNPC_Create dilindungi Npc_HideNameTag | `samp-npc-ai-behavior` |
| `feat-sound` | gameplay | PASS | Umpan balik suara untuk aksi pemain — 2 PlayerPlaySound (umpan balik audio) | `samp-sound-audio` |
| `feat-teleport` | gameplay | PASS | Perpindahan lewat modul shelter/lobby, bukan teleport liar — 6 SetPlayerPos via Shelter_Enter/Exit (bukan perintah bebas); guard=tidak ada | `samp-teleport-system` |
| `feat-property` | gameplay | PASS | Base pemain: tier, plot, anggota (properti klan) — Base_GetTier/GetPlot + BaseMembers_ + shelter | `samp-property-system` |
| `feat-spawn` | gameplay | PASS | Pemilihan lobby/karakter sebelum spawn — SetSpawnInfo+SpawnPlayer lewat auth, lobby menentukan dunia | `samp-spawn-system` |
| `feat-editor` | gameplay | PASS | Editor in-game (/bangun) + galeri TDUI di repo — 13 halaman, 0 gagal, keluaran di docs/tdui + panel edit in-game | `samp-editor-ecosystem` |
| `meta-routing` | meta | PASS | Orkestrator memilih agent per domain dari AGENTS.md — AGENTS.md memuat 80 baris routing domain→agent | `samp-master-2026` |
| `logic-compile-omp-baseline` | logic | PASS | Ukur target open.mp terhadap baseline; regresi = gagal — target open.mp: 6 error, 156 warning (baseline terkunci 6/156 — BELUM memenuhi gate dual-target) | `samp-pawn-compile-gate` |

## Skill

| Skill | Status | Bukti |
|---|---|---|
| `samp-admin-moderation` | TERVERIFIKASI | feat-moderation |
| `samp-agent-evals` | REFERENSI | eval keputusan agent — ditegakkan gate `meta-routing` |
| `samp-android-safe-hud-layout` | TERVERIFIKASI | td-zona-android |
| `samp-anticheat-deep` | TERVERIFIKASI | feat-anticheat-deep |
| `samp-anticheat-system` | TERVERIFIKASI | logic-anticheat |
| `samp-banking-system` | N/A absen-terbukti | belum ada bank/ATM pemain — 0 kemunculan 'Bank_' di gamemodes |
| `samp-camera-cinematic` | N/A absen-terbukti | tanpa kamera sinematik — 0 kemunculan 'Camera_' di gamemodes |
| `samp-chat-system` | N/A absen-terbukti | chat pakai SendClientMessage bawaan — 0 kemunculan 'Chat_' di gamemodes |
| `samp-clothing-accessories` | N/A absen-terbukti | skin hanya dipilih saat buat karakter — 0 kemunculan 'Cloth_' di gamemodes |
| `samp-colandreas-physics` | TERVERIFIKASI | map-colandreas |
| `samp-command-processor` | TERVERIFIKASI | logic-dispatcher |
| `samp-crafting-system` | TERVERIFIKASI | feat-crafting |
| `samp-crashdetect-debugger` | N/A absen-terbukti | dipakai ad-hoc saat crash — 0 kemunculan 'CrashDetect_' di gamemodes |
| `samp-custom-models-cdn` | N/A absen-terbukti | tanpa custom model / FastDL — 0 kemunculan 'AddCharModel' di gamemodes |
| `samp-data-structures-pawn` | TERVERIFIKASI | logic-ysi |
| `samp-death-respawn` | TERVERIFIKASI | feat-death |
| `samp-dialog-system` | TERVERIFIKASI | logic-dialog-lint |
| `samp-discord-connector` | N/A absen-terbukti | belum ada connector Discord — 0 kemunculan 'DCC_' di gamemodes |
| `samp-economy-simulation` | TERVERIFIKASI | feat-economy |
| `samp-editor-ecosystem` | TERVERIFIKASI | feat-editor |
| `samp-entity-lifecycle` | N/A absen-terbukti | library EL terpasang tapi belum ada pemanggil — 0 kemunculan 'EL_Track(' di gamemodes (di luar samp_elife.inc) |
| `samp-filterscript-engineering` | N/A absen-terbukti | belum ada filterscript terpisah — 0 kemunculan 'CallRemoteFunction' di gamemodes |
| `samp-fivem-character-cinema-anim` | N/A absen-terbukti | belum ada studio karakter — 0 kemunculan 'Cinema_' di gamemodes |
| `samp-fivem-heist-crime-gang` | N/A absen-terbukti | belum ada heist — 0 kemunculan 'Heist_' di gamemodes |
| `samp-fivem-housing-business-crafting` | N/A absen-terbukti | belum ada rumah/bisnis pemain — 0 kemunculan 'House_' di gamemodes |
| `samp-fivem-inventory-metadata` | N/A absen-terbukti | inventori ada, metadata per-slot belum — 0 kemunculan 'InvMeta_' di gamemodes |
| `samp-fivem-oxlib-target-logic` | N/A absen-terbukti | interaksi ada, radial/skillcheck belum — 0 kemunculan 'Radial_' di gamemodes |
| `samp-fivem-smartphone-mdt` | N/A absen-terbukti | belum ada MDT — 0 kemunculan 'MDT_' di gamemodes |
| `samp-fivem-vehicle-tuning-garage` | N/A absen-terbukti | tuning belum, garasi sudah — 0 kemunculan 'Tuning_' di gamemodes |
| `samp-gamemode-architecture` | TERVERIFIKASI | logic-arch |
| `samp-gang-faction` | N/A absen-terbukti | party = regu co-op, bukan faksi berperingkat — 0 kemunculan 'Gang_' di gamemodes |
| `samp-greenfield-gamemode` | REFERENSI | fase greenfield sudah dilalui — ditegakkan gate `logic-arch` |
| `samp-high-performance-tuning` | TERVERIFIKASI | logic-timer; logic-onplayerupdate; logic-horde-budget |
| `samp-http-json-integration` | N/A absen-terbukti | tanpa integrasi HTTP eksternal — 0 kemunculan 'HTTP(' di gamemodes |
| `samp-include-library-design` | TERVERIFIKASI | logic-include-guard |
| `samp-inventory-item-system` | TERVERIFIKASI | feat-inventory |
| `samp-legacy-openmp-migrator` | REFERENSI | migrasi 0.3.7→open.mp — ditegakkan gate `logic-compile` |
| `samp-logging-audit` | TERVERIFIKASI | feat-logging |
| `samp-map-pipeline-2026` | TERVERIFIKASI | map-streamer |
| `samp-mapping-dynamic-objects-pro` | TERVERIFIKASI | map-streamer; map-retexture |
| `samp-master-2026` | TERVERIFIKASI | meta-routing |
| `samp-medical-ems` | TERVERIFIKASI | feat-medical |
| `samp-mobile-android-cef-ui` | TERVERIFIKASI | td-zona-android; td-cache |
| `samp-modern-creator-2026` | REFERENSI | playbook pembuatan server — ditegakkan gate `feat-editor` |
| `samp-modern-jobs-2026-logic` | N/A absen-terbukti | pekerjaan belum dibangun — 0 kemunculan 'Job_' di gamemodes |
| `samp-mysql-database` | TERVERIFIKASI | logic-mysql |
| `samp-npc-ai-behavior` | TERVERIFIKASI | feat-npc |
| `samp-openmp-modern-guide` | REFERENSI | panduan platform — ditegakkan gate `logic-compile` |
| `samp-pawn-compile-gate` | TERVERIFIKASI | logic-compile; logic-compile-omp-baseline |
| `samp-pawn-scripting` | TERVERIFIKASI | logic-arch |
| `samp-persistence-schema` | TERVERIFIKASI | logic-persist |
| `samp-phone-system` | N/A absen-terbukti | belum ada smartphone — 0 kemunculan 'Phone_' di gamemodes |
| `samp-police-system` | N/A absen-terbukti | tanpa kepolisian/MDT — 0 kemunculan 'Police_' di gamemodes |
| `samp-property-system` | TERVERIFIKASI | feat-property |
| `samp-raknet-packet-sync` | N/A absen-terbukti | Pawn.RakNet belum dipakai — 0 kemunculan 'Pawn.RakNet' di gamemodes |
| `samp-roleplay-systems-bible` | REFERENSI | blueprint mode (di sini zombie) — ditegakkan gate `logic-arch` |
| `samp-security-lint` | TERVERIFIKASI | logic-dialog-lint |
| `samp-server-configuration` | TERVERIFIKASI | logic-servercfg |
| `samp-server-security-hardening` | TERVERIFIKASI | logic-dialog-lint |
| `samp-sound-audio` | TERVERIFIKASI | feat-sound |
| `samp-spawn-system` | TERVERIFIKASI | feat-spawn |
| `samp-streamer-plugin` | TERVERIFIKASI | map-streamer; map-retexture |
| `samp-teleport-system` | TERVERIFIKASI | feat-teleport |
| `samp-testing-harness` | TERVERIFIKASI | logic-selftest |
| `samp-textdraw-fivem-modern-design` | TERVERIFIKASI | td-galeri |
| `samp-textdraw-layout-debugger` | TERVERIFIKASI | td-lebar-640 |
| `samp-textdraw-studio-converter` | TERVERIFIKASI | td-galeri |
| `samp-textdraw-ui-craft` | TERVERIFIKASI | td-zona-android; td-lebar-640; td-pool; td-cache |
| `samp-timer-scheduling` | TERVERIFIKASI | logic-timer |
| `samp-ucp-web-panel-bridge` | N/A absen-terbukti | UCP web backlog — 0 modul *ucp* |
| `samp-vehicle-system` | TERVERIFIKASI | feat-vehicle |
| `samp-voice-spatial-audio` | N/A absen-terbukti | voicechat belum dipasang — 0 kemunculan 'SvCreate' di gamemodes |
| `samp-vs-openmp-encyclopedia` | REFERENSI | perbandingan platform — ditegakkan gate `logic-servercfg` |
| `samp-weather-time` | TERVERIFIKASI | feat-daynight |
| `samp-ysi-framework` | TERVERIFIKASI | logic-ysi |
| `samp-zombie-survival-horde` | TERVERIFIKASI | logic-onplayerupdate; logic-horde-raid-fsm; logic-horde-budget; logic-horde-payout |

## Agent

`samp-android-layout-guardian`, `samp-architecture-sage`, `samp-creator-2026`, `samp-crime-faction-architect`, `samp-database-engineer`, `samp-developer`, `samp-filterscript-master`, `samp-fivem-systems-engineer`, `samp-gamemode-architect`, `samp-legacy-migrator`, `samp-map-pipeline-engineer`, `samp-mapping-mlo-architect`, `samp-modern-jobs-architect`, `samp-pawn-compile-verifier`, `samp-pawn-wizard`, `samp-qa-engineer`, `samp-release-engineer`, `samp-security-anticheat`, `samp-security-hardener`, `samp-server-admin`, `samp-syntax-triager`, `samp-textdraw-fivem-master`, `samp-textdraw-layout-auditor`, `samp-textdraw-ui-designer`, `samp-ucp-fullstack-engineer`, `samp-voice-audio-engineer`, `samp-zombie-systems-engineer`
