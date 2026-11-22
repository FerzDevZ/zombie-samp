# ARCHITECTURE — LAST STAND

> **Status runtime (26-11-19):** implementasi aktual berjalan di **SA-MP 0.3.7**
> (`<a_samp>` + samp03svr), bukan open.mp — bagian open.mp di bawah adalah target
> migrasi, bukan keadaan sekarang. Struktur modul aktual berbeda sedikit dari
> rencana §2 (mis. crafting ada di ui/craft_ui + config/recipes, vehicle di
> world/vehicles+garage, party di player/party). Sumber kebenaran struktur:
> `gamemodes/main.pwn` (urutan include) dan `docs/CODEBASE_STUDY.md`.
> Lapisan transaksi atomik: `core/transaction.inc` (koneksi khusus terurut).

## 1. Keputusan Stack

| Lapisan | Pilihan | Alasan | Alternatif yang ditolak |
|---|---|---|---|
| Runtime server | **open.mp 1.x** | Komponen native, `config.json`, crash handler, tetap kompatibel client 0.3.7 & SA-MP Mobile | SA-MP 0.3.7 (tanpa komponen, maintenance mati), 0.3.DL (mobile tidak dukung custom model) |
| Bahasa | **PAWN** via `sampctl` | Satu-satunya jalur stabil untuk gamemode SA-MP | C++ component untuk seluruh gamemode (biaya maintenance terlalu tinggi untuk 1 orang) |
| Framework | **YSI 5** (`y_hooks`, `y_iterate`, `y_timers`, `y_commands`, `y_inline`) | Hook chaining bersih, iterator menghindari loop MAX_PLAYERS | Hook manual ALS (rawan urutan) |
| Database | **MySQL 8 / MariaDB 10.11** + **BlueG R41-4** | Threaded query, `%e` escaping, transaction | SQLite (tidak cocok untuk multi-instance), flatfile |
| Streamer | **Incognito Streamer** | Pickup/area/label/objek dinamis, player-specific pickup untuk anti-ESP | Streamer buatan sendiri (buang waktu) |
| Zombie | **FCNPC** + AI custom | Kontrol penuh movement, damage, animation; hanya FCNPC yang sanggup 100+ NPC | RNPC (dipanggil script, tidak cocok AI), actor streamer (tidak bisa diserang) |
| Fisika/LOS | **ColAndreas** | Raycast server-side untuk validasi tembakan & Z ground | MapAndreas (hanya tinggi, tanpa objek) |
| Auth | **bcrypt** (`samp-bcrypt`) | Standar; bukan MD5/Whirlpool | hash buatan sendiri |
| Opsional fase 7 | **Pawn.RakNet**, **sampvoice** | Anti-cheat paket & proximity voice | – |

**Yang sengaja TIDAK dipakai:** CEF (tidak ada di Android), custom model 0.3.DL, plugin zombie pihak ketiga, ORM.

---

## 2. Struktur Repo

```
zombie-samp/
├── pawn.json                     # dependency sampctl (pin versi!)
├── config.json                   # open.mp server config
├── gamemodes/
│   ├── main.pwn                  # bootstrap murni: include + main()
│   └── modules/
│       ├── config/               # constants.inc, zombies.inc, loot_tables.inc, items.inc
│       ├── core/                 # database, logger, spatial_hash, tick_scheduler, pool
│       ├── ui/                   # hud, toast, progress, prompt, inventory_ui, death_screen
│       ├── player/               # auth, characters, needs, infection, downed, skill_tree
│       ├── interact/             # sphere dispatcher [Y], keybind + command fallback
│       ├── zombie/               # zombie_core, zombie_ai, zombie_types, spawn_director
│       ├── combat/               # damage_pipeline, weapons, durability, noise
│       ├── inventory/            # inventory_core, items_registry, drop, stash, transfer_tx
│       ├── crafting/             # blueprint, bench, field_craft
│       ├── loot/                 # node_registry, respawn_cycle, airdrop
│       ├── world/                # zones, daynight, weather, extraction_points
│       ├── vehicle/              # vehicle_core, fuel_wear, garage
│       ├── party/                # party_core, revive, shared_xp
│       ├── base/                 # safehouse, barricade, horde_defense
│       ├── economy/              # cash, scrap, market_hub, medical_bill
│       └── security/             # anticheat_core, shot_validation, discord_audit
├── database/migrations/          # 001_init.sql, 002_..., dijalankan manual/CI
├── scriptfiles/                  # log, ban, cache node graph
├── .github/workflows/build.yml   # sampctl package ensure + build
└── tests/                        # checklist manual per fase (lihat ROADMAP)
```

Aturan mutlak:
- `main.pwn` **hanya** berisi include + `main()`. Tidak ada logika.
- Satu modul = satu domain. Modul tidak boleh `#include` modul domain lain; komunikasi lewat public function atau `CallLocalFunction`.
- Tidak ada `for (new i = 0; i < MAX_PLAYERS; i++)` — selalu `foreach`.
- Tidak ada query MySQL non-threaded.

### Urutan include di `main.pwn`

```pawn
#include <open.mp>
#include <a_mysql>
#include <streamer>
#include <sscanf2>
#include <YSI_Coding\y_hooks>
#include <YSI_Coding\y_commands>
#include <YSI_Data\y_iterate>
#include <YSI_Core\y_timers>
#include <fcnpc>
#include <colandreas>
#include <bcrypt>

#include "modules/config/constants.inc"
#include "modules/config/items.inc"
#include "modules/config/zombies.inc"
#include "modules/core/database.inc"
#include "modules/core/logger.inc"
#include "modules/core/spatial_hash.inc"
#include "modules/core/tick_scheduler.inc"
// ... ui, player, zombie, combat, inventory, ...
#include "modules/world/zones.inc"      // harus sebelum loot & zombie (butuh zona)
#include "modules/security/anticheat_core.inc"
```

---

## 3. Tick Budget (anggaran per frame 20 ms)

`OnPlayerUpdate` **dilarang** untuk logika berat. Semua kerja ada di timer dengan fase berbeda supaya tidak menumpuk di frame yang sama.

| Timer | Interval | Kerja | Budget maks |
|---|---|---|---|
| `Zombie_Tick` | 200 ms, dibagi 5 slice zona | AI, pursuit, attack check, LOD promote/demote | 4 ms |
| `Needs_Tick` | 1000 ms | Hunger/thirst/stamina, infection decay di hub | 0.5 ms |
| `HUD_Tick` | 250 ms | Dirty-flag HUD update per pemain | 1 ms |
| `HordeDirector_Tick` | 5000 ms | Threat decay, event trigger, populasi target | 1 ms |
| `LootRespawn_Tick` | 30000 ms | Node respawn cycle | 0.5 ms |
| `Autosave_Tick` | 60000 ms | Flush dirty player/vehicle row | 2 ms (threaded, non-blocking) |
| `Airdrop_Tick` | 1000 ms | Marker & timer crate | 0.1 ms |

Aturan: **tidak ada satu tick pun yang boleh melebihi 5 ms**. Kalau lewat, slice jadi lebih kecil atau kerja dipindahkan ke thread MySQL.

---

## 4. Zombie Engine

### 4.1 Level of Detail (LOD)

Ini bagian paling menentukan performa. Zombie tidak semuanya berpikir setiap tick.

| LOD | Jarak dari pemain terdekat | Perilaku | Biaya |
|---|---|---|---|
| **FULL** | < 60 m | Pathfinding node graph + LOS raycast + attack + animasi | Penuh |
| **CHASE** | 60–120 m | Bergerak lurus ke target terakhir, tanpa pathfinding | Ringan |
| **IDLE** | > 120 m | Berdiri, animasi ambient, tidak diproses | Hampir nol |
| **POOL** | streamed out | Diambil dari pool, data disimpan, tidak ada NPC | Nol |

- Konversi FULL→CHASE→IDLE hanya dievaluasi tiap 500 ms per zombie (bukan tiap tick), disebar dengan offset acak per index.
- **Budget NPC aktif: 40 ambient + 150 horde = 190 FCNPC.** Di atas itu, director menolak spawn dan menulis log, bukan memaksa server lag.
- Zombie di luar 120 m tidak pernah memanggil `FCNPC_*` movement.

### 4.2 Spatial Hash

```pawn
#define HASH_CELL_SIZE   100.0
#define HASH_CELLS_X     60
#define HASH_CELLS_Y     60

enum E_CELL { Iterator:cellZombies<MAX_ZOMBIES>, cellCount }
new CellData[HASH_CELLS_X][HASH_CELLS_Y][E_CELL];
```

- Setiap zombie masuk/keluar sel saat posisinya berubah > 5 m dari pusat sel terakhir.
- Query radius = iterasi sel yang beririsan (maks 9 sel), bukan loop 300 zombie.
- Dipakai untuk: zombie target acquisition, AoE Bloater, noise propagation, party proximity.

### 4.3 Pathfinding

- **Node graph** per zona, di-generate sekali dari sampel grid 8 m lalu di-prune dengan ColAndreas (node yang jatuh di dalam objek dibuang). Disimpan ke `scriptfiles/nodes/<zone>.json` agar tidak digenerate tiap start.
- Pathfinding: A* pada node graph, maksimum 400 node ekspansi, hasil disimpan sebagai `zPath[16]` waypoint.
- Repath hanya jika: target bergerak > 12 m dari target terakhir, atau zombie stuck > 2 detik (deteksi via jarak tempuh < 1 m padahal movement aktif).
- Kalau pathfinding gagal 2× berturut → fallback langsung ke CHASE (jangan pernah membuat zombie berdiri diam di depan pemain, itu bug yang paling sering dilaporkan pemain).

### 4.4 Serangan & Sync

- Attack hanya dieksekusi jika `IsPlayerInRangeOfPoint(zombie, 2.2, ...)` **dan** LOS ColAndreas bersih. Ini mencegah zombie memukul menembus dinding (bug klasik SA-MP).
- Damage diterapkan di server, bukan animasi. Animasi hanya kosmetik.
- Zombie diserang pemain: `OnPlayerGiveDamageActor`/hook damage → validasi di `damage_pipeline`, bukan langsung kurangi HP.

---

## 5. Horde Director (FSM)

```
IDLE ──(night>=3 / noise>threshold / boss timer)──▶ WARN (20s, tanpa spawn)
WARN ──▶ SPAWNING (maks 8 NPC / 200ms sampai kuota)
SPAWNING ──▶ ACTIVE (horde bergerak, 90–180s)
ACTIVE ──(kuota mati / timer habis)──▶ COOLDOWN (180s global)
COOLDOWN ──▶ IDLE
```

- Director memilih **shelter/area dengan barricade terbanyak** sebagai target Horde Night, dan menyebar spawn dari 3 arah terluar zona.
- Global cooldown 180 s mencegah event bertumpuk.
- Pemain Field Level < 5 tidak menyumbang noise dan tidak pernah jadi target horde pertama.

---

## 6. Damage Pipeline (satu pintu)

Semua damage — zombie ke pemain, pemain ke zombie, pemain ke pemain — lewat satu fungsi:

```pawn
stock ApplyVerifiedDamage(victimType, victimId, attackerId, Float:amount, weaponId, bodyPart)
```

Urutan validasi:

1. Zona: jika PvP off dan keduanya pemain → **reject**.
2. Jarak: senjata punya `maxRange`; jika `GetPlayerDistanceFromPlayer`/zombie > maxRange + 3 → reject + flag.
3. LOS: `CA_RaycastLine` dari mata penyerang ke korban; jika kena objek → reject kecuali senjata melee/explosive.
4. Senjata: pemain harus benar-benar memegang `weaponId` (`GetPlayerWeapon`) dan ammo > 0.
5. Rate limit: damage event per pemain > 12/detik → flag `DPS_SPIKE`, drop event.
6. Headshot: maksimum 2 headshot kill per 1.5 detik per pemain (anti aimbot), sisanya dihitung bodyshot.
7. Multiplier: zona PvP 0.75, armor menyerap, lalu apply.

Semua reject dengan amount > 50 menulis baris `anticheat_flags` dan Discord webhook staff.

---

## 7. Inventory & Transaksi

- 20 slot, bobot 30 kg, dua wadah: `carrier` (hilang saat mati) dan `stash` (aman).
- Semua transfer (pickup, drop, stash, trade, trunk) memakai satu fungsi `Inventory_MoveTx` dengan pola:

```text
BEGIN
  SELECT amount, durability FROM inventory WHERE char_id=? AND slot=? FOR UPDATE
  -- validasi bobot & slot di memori SETELAH row lock
  UPDATE / INSERT / DELETE sumber & tujuan
COMMIT
```

- Tidak ada `INSERT` tanpa `SELECT ... FOR UPDATE` pendahulu untuk item yang bisa dipindah dua arah. Ini satu-satunya cara mencegah dupe.
- Setiap perubahan menulis `item_ledger` (append-only) untuk audit rollback bila ada laporan dupe.
- Drop di dunia memakai pickup player-specific (`CreateDynamicPickup` dengan `playerid`) → anti-ESP.

---

## 8. Skema Database (ringkas)

Detail ada di `database/migrations/001_init.sql`.

| Tabel | Isi | Index penting |
|---|---|---|
| `accounts` | login, hash, ban | `UNIQUE(username)` |
| `characters` | nama, level, xp, cash, scrap, skin, posisi terakhir | `idx(account_id)` |
| `inventory` | carrier slot | `PK(char_id, slot)` |
| `stash` | stash slot | `PK(char_id, slot)` |
| `weapons` | senjata tersimpan + durability | `idx(char_id)` |
| `vehicles` | posisi, fuel, health, plate, mods | `idx(owner_char_id)` |
| `bases` / `barricades` | safehouse, HP barricade, posisi | `idx(base_id)` |
| `loot_nodes` | node world + `respawn_at` | `idx(zone_id, respawn_at)` |
| `kill_log` | audit PvP/PvE | `idx(ts)`, `idx(killer_id)` |
| `item_ledger` | append-only audit item | `idx(char_id, ts)` |
| `anticheat_flags` | flag + payload | `idx(char_id, ts)` |
| `bans` | ban aktif | `idx(account_id)` |

Konvensi: InnoDB, `utf8mb4_unicode_ci`, semua timestamp `BIGINT` epoch ms (bukan `DATETIME`) agar konsisten dengan `GetTickCount` server dan mudah dibandingkan.

---

## 9. TextDraw Budget

| Subsystem | Budget | Lifecycle |
|---|---|---|
| HUD persisten | ≤ 24 | Dibuat saat spawn, dihancurkan saat disconnect |
| Speedometer | ≤ 16 | Dibuat saat masuk driver seat, dihancurkan saat keluar |
| Toast (3 slot) | ≤ 12 | On demand, destroy saat expire |
| Prompt `[Y]` + progress | ≤ 8 | On demand |
| Modal aktif (inventory/craft/stash/market) | ≤ 120 | **Mutually exclusive** — `CloseActiveUI()` wajib sebelum buka modal lain |
| Headroom | ≥ 60 | Wajib tersisa |

Aturan implementasi:
- Background statis (bingkai inventory, modal frame) → **Global TextDraw** (hemat slot per pemain).
- Semua `PlayerText` disimpan di struct per subsystem dan dihancurkan lewat `SafeDestroyPlayerTD`.
- Tidak ada `TextDrawShow` per frame. Hanya saat nilai berubah (dirty flag).
- Tombol: sprite `LD_SPAC:white` sebagai hitbox selectable, teks overlay `selectable = false`.

---

## 10. Anti-Cheat Matrix

| Cheat | Deteksi | Aksi |
|---|---|---|
| Health hack | HP klien vs HP server delta, `OnPlayerTakeDamage` gap | Resync + flag |
| Teleport / speed | Delta posisi per 200 ms > `maxSpeed*1.4` + ColAndreas ground check | Rubber-band + flag |
| Airbreak | Z berubah tanpa ground raycast & tanpa kendaraan | Pull down + flag |
| Silent aim | Sudut tembak vs vektor korban > threshold; hit rate > 70% dengan jarak > 60 m | Reject shot + flag |
| Wallbang | LOS ColAndreas gagal | Reject damage |
| Weapon spawn | `GetPlayerWeapon` mismatch dengan DB/ammo | Remove + flag |
| Dupe item | `item_ledger` anomali + transaction audit | Rollback + ban manual |
| Loot ESP | Pickup player-specific (tidak pernah global) | Struktural |
| Fast revive / skip | FSM stage + `GetTickCount` minimum duration | Reject payout |
| Job/event payout spam | Idempotency key per contract di memori + DB | Reject |

Semua flag: `anticheat_flags` + Discord webhook `#staff-audit`. Auto-ban hanya untuk pola berulang (3 flag sejenis < 10 menit), selain itu review manual.

---

## 11. Target Performa

| Metrik | Target | Cara ukur |
|---|---|---|
| Player | 100 bersamaan, 190 zombie | Load test fase 8 |
| Tick | < 20 ms (p95) | Counter internal + log tiap 5 s |
| Query | 0 query di main thread | Code review + grep `mysql_query` non-thread |
| Memory | < 3 MB per pemain | Profiling |
| Compile | 0 error, 0 warning | `sampctl package build` |
| Startup | < 8 s | Log timestamp |

Kalau tick p95 > 20 ms dengan 100 pemain: matikan ambient zombie dulu (faktor terbesar), bukan memotong fitur pemain.

---

## 12. Observability

- `logger.inc`: level (INFO/WARN/ERROR), file rotation harian, mirror ke Discord webhook untuk ERROR.
- `crashdetect` aktif di dev & staging; decode runtime error 4/19/20/26 dengan skill `samp-crashdetect-debugger`.
- Setiap horde event menulis satu baris ringkasan (zona, jumlah zombie, durasi, tick max) → memudahkan tuning tanpa profiling.
- `/admin diag` menampilkan: tick p95 5 detik terakhir, jumlah FCNPC per LOD, jumlah TextDraw per pemain, query antrean.
