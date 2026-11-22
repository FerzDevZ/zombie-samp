# CODEBASE STUDY — LAST STAND: San Andreas Quarantine

Tanggal: 2026-11-19 · Metode: swarm 4 subagent paralel (satu per domain) + verifikasi manual klaim kritis ke源码 + compile gate nyata.
Cakupan: 80 file PAWN, 22.856 baris (gamemodes/main.pwn + modules/*), 19 migrasi SQL, 7 docs, 5 skrip ops.
Compile gate: `sampctl build` exit 0 — 3 warning (208 invpers.inc:155, 234 deprecated colandreas.inc:603, 225 unreachable char_create.inc:264).
Persona aktif: @samp-creator-2026 + skill samp-modern-creator-2026, samp-gamemode-architecture, samp-pawn-scripting (repo .agents/).

---

## 1. Identitas & Stack

Gamemode zombie survival co-op berbasis ekstraksi (bukan deathmatch/RP): loop 20 menit, solo atau party 4 orang. Noise economy (tembakan menarik zombie), horde director berbasis heat, mati = kehilangan carrier loot (makam), stash aman di hub.

Stack aktual (pawn.json + server.cfg): SA-MP 0.3.7 (samp03svr), YSI y_iterate/y_va, Incognito Streamer 2.9.6, BlueG MySQL R41 (100% threaded, pool 4), sscanf (include 2.8.2 vs plugin 2.13.8 — MISMATCH, auth sengaja pakai parser manual), samp-bcrypt 0.4.1, FCNPC 2.0.11, ColAndreas 1.5.0. MariaDB 10.11 via docker-compose. Dua instance (s1/s2) via scripts/instance.sh; sub-lobby dalam satu proses via virtual world 100/200/300.

Catatan: README/ARCHITECTURE menyebut open.mp sebagai target, tetapi runtime yang terpasang dan terverifikasi adalah SA-MP 0.3.7 (main.pwn include <a_samp>, bukan <open.mp>). Dokumen belum sepenuhnya sinkron dengan realitas runtime.

Struktur: main.pwn = bootstrap + callback global (544 baris, sedikit logika di Player_Tick/spawn — masih sesuai semangat "entry tipis"). 16 domain modul. Urutan include adalah kontrak keras (Pawn satu pass): state → inventory → combat → zombie → world → ui → commands; admin_state.inc wajib sebelum damage_pipeline.inc; roles.inc sesudah account_core.inc sebelum account_auth.inc.

Database: 16 tabel diverifikasi selftest — accounts, characters, bases, base_structures, inventory, stash, item_ledger, kill_log, anticheat_flags, barricades, graves, contracts, owned_vehicles, companions, bans, party_stash (+ grave_items, character_skills). Konvensi: timestamp epoch, escaping %e, semua query mysql_tquery/pquery.

---

## 2. Boot & Tick (main.pwn)

OnGameModeInit: init Escort/Companion/Roles/Party → AddPlayerClass di kamp (anti spawn 0,0,0) → SQL_Boot → CA_Init + raycast probe bukti data .cadb → FCNPC move mode ColAndreas + pathfinding raycast + sync rate adaptif → Map_Init (cap objek klien 920) → 6 hotspot Interact_Create → 20 timer berulang + selftest 8 dtk + boot-load barricade (3 dtk) & makam (4 dtk).

Timer utama: Zombie_Tick 200 ms (5 bucket, tiap zombie berpikir 1×/detik), Director_Tick 5 s, Horde_Tick 5 s, Needs_Tick 5 s (satu jalur DoT tubuh+luka), HUD_Tick 500 ms, Player_Tick 500 ms (hash posisi, zona, sync rate, Lock_Tick), AC_PositionTick 1 s, Base_Tick 2 s, VehicleFuel_Tick 2 s, Escort/Companion_Tick 2 s, autosave 10 s (batch 5, interval 120 s), Loot_Expire 15 s, LootNode 10 s, Corpse/Grave 30 s, WildVeh 20 s, Event 5 s, World_Tick 45 s/jam game.

Damage engine selalu return 0 (OnPlayerGiveDamage, FCNPC_OnTakeDamage) → server otoritatif penuh lewat Damage_Apply. FCNPC_OnDeath global: Escort → Companion → Zombie (urutan wajib, invariant MEMORY). FCNPC hook wajib di main.pwn karena macro include hanya menulis ulang nama callback di sumber utama.

---

## 3. Domain account/ + admin/ + player/

### account_core.inc
State: AuthStage[] (LOADING→REGISTER→LOGIN→CHARSELECT→PLAYING), AccountId, ActiveCharId, StoredPasswordHash, ChatAuthOnly, LoginAttempts, CharacterDBId/DBSlot[2], AccountLastSave. Gate semua command gameplay: Account_IsPlaying/RequirePlaying. Lock/UnlockPlayer (freeze). Validasi nama 3–20 alfanumerik+_, password 6–40. Dialog 2001–2005.
Risiko: RecoveryPending/PasswordChangePending tidak di-reset → bocor saat slot dipakai ulang.

### account_auth.inc (825 baris)
Alur: BeginAuth (guard NPC + SQL ready) → SELECT accounts → OnLookup: tanpa baris → arahkan /daftar; is_banned → Moderation_CheckBan; else simpan hash + Admin_ApplyLoadedLevel → /masuk. Registrasi bcrypt_hash → INSERT → cache_insert_id → bootstrap owner. Login bcrypt_verify (5 gagal = Kick) → LoadCharacters (0 karakter → wizard; else CHARSEL). OnCharacterLoaded membaca 28 kolom → isi PlayerSurvival/lobby/profil/hotbar CSV/tutorial/wounds → SetSpawnInfo+SpawnPlayer+SetPlayerName → AUTH_PLAYING → load async Inventory/Weapons/Skills/Contracts/Companion/Garage.
Autosave stagger: tick 10 s, maks 5 karakter, interval 120 s per pemain (AccountLastSave) → UPDATE characters + Inventory/Weapons/Skills_Save.
Keamanan: %e escaping, bcrypt threaded cost 12, IsPlayerConnected di tiap callback.
Risiko: callback async keyed playerid saja → race slot-reuse; last_ip hardcode '0.0.0.0'; autosave ganda (SaveCharacter sudah memanggil Inventory/Weapons_Save); OnGameModeExit tidak menyimpan karakter (hilang ≤2 menit); pesan login membocorkan kunci pemilik "ferzdevz" (baris 73).

### char_create.inc (732 baris)
Wizard 8 langkah (dialog 2010–2018 TABLIST_HEADERS berheader): nama→gender→umur→ras→asal→kerja→trait→look→konfirmasi. Efek nyata: cash 500+job+origin, HP 50–125, XP, poin skill; ras murni identitas (invariant balance). INSERT characters 15 kolom → PendingKitJob → kit HANYA via CharCreate_ApplyPending dari Inventory_OnLoaded (anti ditimpa load async). /nama untuk mobile. Reset tanpa syarat saat disconnect.
Risiko: insert_id==0 selalu dianggap "nama dipakai" padahal bisa error DB lain; warning 225 unreachable code baris 264.

### admin_state.inc / roles.inc
AdminGod[] sengaja di-include sebelum damage_pipeline (dibaca choke point). Level 0/1/2 dari accounts.admin_level; gate tunggal IsServerAdmin/IsServerSuperAdmin (bukan RCON IsPlayerAdmin). Bootstrap pemilik 3 alias case-insensitive + persist. Kunci in-game /admin /superadmin: 5 salah → blokir 60 dtk. Admin_SetLevel menolak ubah level sendiri (anti-lockout).
Risiko: kunci hardcode "ferzdevz"/"adminkubu" + dibocorkan pesan auth; promosi berbasis nickname = takeover saat pemilik offline; blokir per-slot direset saat disconnect → reconnect melewati rate-limit; /pulihkan tanpa counter percobaan.

### moderation.inc
/ban → INSERT bans → callback set is_banned=1 → kick delay 1,2 dtk. Admin tak boleh ban admin (kecuali super). Login: SELECT bans ORDER BY expires_at DESC LIMIT 1 → kedaluwarsa → auto-unban. /unban (super) menutup ban aktif, riwayat tetap. /bans 8 terakhir.
Risiko: ORDER BY expires_at DESC menaruh ban permanen (expires_at=0) terakhir → ban sementara kedaluwarsa membayangi ban permanen → auto-unban bypass (TERVERIFIKASI, lihat §7). Moderation_OnBanSaved membaca AccountId[targetId] saat callback → race slot bisa salah akun dibanned.

### superadmin.inc / panel.inc
Perkakas ber-guard: SpawnZombies ≤30 cap MAX_ZOMBIES-1, GiveItem selalu Inventory_Grant "admin_grant" ≤500, SetCash cap 10 juta, AddXp ≤100 ribu, Teleport wajib AC_ClearSample, FreezeToggle, SetHour. Semua log account pelaku.
/sa: dialog 3501–3514 dengan SNAPSHOT SaPlayerIds[32]/SaItemIds[12]/SaBanAccount[6]; navigasi di akhir; aksi hanya memanggil helper ber-guard yang sama dengan command. Repair admin: grant Toolbox sementara "admin_repair_temp" → saldo bersih nol, tetap berledger.
Risiko: daftar pemain mentok 32 tanpa paging (maxplayers 200); kick panel tanpa cek superadmin; menu teleport gate admin tetapi helper butuh super → no-op senyap.

### commands.inc (1.751 baris, ±90 command)
Dispatcher manual (tanpa y_commands demi hook ALS FCNPC). Command_Split/Command_Arg parser tanpa sscanf untuk auth. Gate Account_RequirePlaying di tengah file; dev via Command_DevAllowed (DEV_MODE=0 → admin).
Risiko: handler duplikat tak terjangkau (/login /register blok kedua); /peti party-stash membayangi /peti lootnode; /perbaiki garage membayangi versi base; /daftar masih memakai sscanf (baris 149, melanggar invariant auth); Party_Create tanpa cek IsPlaying.

### player/state.inc
PlayerSurvival ±30 field + Iterator:PlayerPool. HP otoritatif: Player_ApplyDamage (armour serap 60%, infeksi 12–24/gigitan), stage infeksi 40/80/100, EnterDownedState (45 dtk, HP engine 1.0), Revive (35 HP), Player_IsSafe = hub||shelter.
Risiko: revive tidak menyembuhkan infeksi → stage 3 turned lagi di Needs_Tick berikutnya + spawn runner baru (potensi farm zombie lewat loop revive).

### player/needs.inc
Needs_Tick 5 s: Wounds_Tick; DoT infeksi stage 2 (4 HP); stage 3 → downed + spawn ZT_RUNNER; decay lapar 0,10/haus 0,14/bladder +0,12 (×1,35 overload tas); damage via Damage_Apply; warning bitmask + throttle 45 dtk. Makan +30 lapar, air +42 haus +12 bladder, /kencing min 12.
BUG KRITIS TERVERIFIKASI: Needs_Clamp mengembalikan 0/100/1 dan pemanggil mengabaikan return → clamp tidak pernah terjadi (lihat §7).

### player/wounds.inc
psWounds 0..3, DoT 0,9 HP/stack tiap 5 dtk HANYA dari Needs_Tick (satu jalur). Bandage 1 stack, medkit semua. Persist characters.wounds → logout bukan cara menghilangkan luka.

### player/party.inc
Maks 4, key pemimpin = playerid runtime (tidak persist, bubar saat relog). Invite TTL 30 dtk ≤8 m lobby sama; snapshot PartyPanelRows/Member (anti index bergeser). FF mati di Damage_Apply + OnPlayerGiveDamage. Party_ShareXp 20% per anggota hidup ≤60 m, min 1, gate amount≥5, guard PartyXpGuard anti-rekursi.
Risiko: /invite id eksplisit tanpa cek lobby/jarak (party lintas lobby); Party_Show cetak Float psHealth dengan %d.

### player/party_stash.inc
Tabel party_stash key char_id pemimpin: 40 slot, 240 kg. Anggota ≤8 m. Lock_Acquire → pembuka lain read-only. Ambil = act-then-verify: Grant → DELETE/UPDATE → cache_affected_rows; 0 → Consume "party_stash_rollback". Simpan: cek berat → Consume → INSERT ON DUPLICATE. Specimen dilarang.
Risiko: Put tanpa verifikasi affected_rows (DB gagal → item lenyap); DELETE ambil-semua tanpa predikat amount; rollback bisa gagal bila tas berubah di antara callback → potensi dupe.

### player/extraction.inc
/ekstrak atau Y: titik 5 m, cooldown per lobby 90 dtk, min 3 maks 8 sampel → Consume "extract" → $100+$120/sampel, heal 20+6/sampel, cure 10+4/sampel, heat+14, teleport pulang + AC_ClearSample. Sampel dilarang masuk stash/peti → tak bisa ditimbun.
Risiko: psExtractions/psDeaths tidak dipersistensi.

### player/tutorial.inc
Onboarding 5 langkah persisten (tutorial_step/tutorial_count): kill 3→loot 1→craft 1→sampel 3→ekstrak 1. Satu jalur Tutorial_Bump; reward sekali via Inventory_Grant "tutorial" (final $300+5 scrap); hook hanya dari jalur resmi.
Risiko: Grant gagal (tas penuh) langkah tetap maju → reward hilang.

---

## 4. Domain zombie/ + combat/ + security/ + core/

### zombie_core.inc
Pool ZombieEntity[512] (active/dead/type/health/max/lastAttacker/spawn/deathTick/lobby), iterator ZombiePool, cap MAX_ZOMBIES=190. Create menolak saat budget penuh; FCNPC_Create+Spawn pakai profil (skin/HP/speed×lobby), VW per lobby, Hash_ZombieAdd. ApplyDamage → mati → anim KO, AwardKill+Contracts+Loot, cleanup timer 4000 ms. AwardKill: hub nol-farming, XP=10+type×5, boss sekali ($250/60XP/3scrap), streak 5/10 per 60 s, telemetri kill_log. HookTakeDamage memblokir issuer NPC (cone melee saling pukul) → Damage_Apply, return 0. Sync adaptif 80 ms (<3 pemain) / 100 ms (≥3), throttle 5 s dari Player_Tick.
Risiko: cleanup 4 dtk vs reuse slot FCNPC bisa menghapus zombie baru; kematian engine (kendaraan) tanpa loot/kredit.

### zombie_ai.inc (629 baris)
E_ZOMBIE_AI: target, behaviour IDLE/INVESTIGATE/CHASE/ATTACK, noise point+memory 20 s, anchor, repath/stuck tick. Zombie_Tick 200 ms 5 bucket (id%5). LOD: >90 m parkir (anchor→return, noise→investigate, else idle); 45–90 m CHASE lurus repath 1800 ms bila target geser ≥1,5 m; <45 m FULL acquire/attack/ring. MoveToRing bearing (id×47)%360; jalur langsung bila ≤30 m & LOS bersih, else RAYCAST; WALK bila speed≤0,35 (anti meluncur). TryAttack: range 2,6 + lobby + LOS → stop+hadap+melee (tanpa reissue GoTo = anti jitter), infeksi 18/40%, wound per tipe, Screamer panggil 15 s. Scream menarik zombie 3×3 sel. Bloater mati: gas 40 damage r4,5 + noise 25. Anchor/leash 38 m, hold 3,5 m. CheckStuck 2 s gerak <1 m → strike, 3 strike → daur ulang.
Risiko: LOD jarak 2D sedangkan serang 3D; recycle bisa membuang zombie raid tanpa kredit.

### spawn_director.inc (526 baris)
ZoneZombieCount[lobby][zone]; Director_Tick 5 s. Budget dinamis = 18+34×pemain, cap 150 & MAX_ZOMBIES; TrimToBudget buang terjauh bila >180 s sejak horde (O(n²)). Pembawa sampel menambah target (cap 4). Ambient zona ×malam 1,6, proporsional pemain, cap 8, peluang 55%. PickType roll per tier (tier2 malam→Stalker). SpawnForZone 6 percobaan ring 0,35–0,95 radius, hindari pemain <35 m, FindGround CA raycast +120/−80. TriggerHorde default 45. Boss per lobby: malam saja, respawn 900 s, chance 40%, butuh anchor hidup non-hub, headroom 2; PhaseTwo <50% sekali → 5 Runner r12; BossStatus untuk HUD. NightWatch 60 s: boss tick + auto-horde zona tersibuk (30) bila >900 s & aktif≥60.
Risiko: trim tidak mengecualikan zombie raid ber-anchor & LastHordeTick tidak di-set raid base; FindGround selalu true.

### horde.inc (413 baris)
ZoneHeat[lobby][zone], BaseHeat[lobby][player]+guard charId. Horde_Tick 5 s: decay base ×0,94, zona ×0,90; trigger ≥45 & cooldown 45 s & ada pemain → cluster 6–14, reset heat, toast [HORDE]. AddHeat: skip hub; pemain hidup dalam radius+2 menambah r/10; noise ≤26 m dari petak base sendiri → BaseHeat+=r/6. Raid base: cooldown 90 s, BaseHeat tertinggi ≥30, spawn 8–14 ber-anchor r45; aktif >12 s & 0 zombie raid → reward $420/35XP; timeout 180 s surut. ForceBaseRaid uji 12. ScreamerPulse 20 s: screamer dengan pemain <50 m → noise 60 + spawn 3.
Risiko: reward hanya cek connected+lobby bukan charId → klaim via slot reuse; heat berlipat per pemain sekitar.

### combat/damage_contract.inc + damage_pipeline.inc
Contract enum E_DAMAGE_KIND + forward Damage_Apply memecah siklus include zombie↔pipeline. Satu pintu Damage_Apply: amount>0 → rate-limit 12 event/detik per attacker (DPS_SPIKE). Cabang PLAYER: connected+alive → Admin_IsGod blokir → attacker pemain: party block + ValidatePlayerToPlayer (bukan diri, kedua sisi non-hub, victim bukan grace/downed, jarak≤Weapon_GetMaxRange else RANGE_FAIL, non-melee wajib LOS else LOS_FAIL) → ×Skills_DamageTakenMultiplier → Player_ApplyDamage → downed bila ≤0. Cabang ZOMBIE: active → Dps_Track ≤320/detik else batal → ×Skills_DamageDealtMultiplier → Zombie_ApplyDamage.
Risiko: amount PvP tidak di-clamp ke base damage senjata; cek DPS hanya cabang zombie; tanpa cek amunisi; bodyPart unused.

### combat/weapons.inc + noise.inc
Tabel murni: maxRange (melee 3–3,2, pistol 65, shotgun 55, SMG 95, rifle 160, sniper 320, launcher 60), noise radius (melee 0, pistol 45, shotgun 75, SMG 60, rifle 90, launcher 120), IsMelee 0–15, base damage. Noise_EmitFromPlayer: radius×skill×event → psLastNoiseTick + Horde_AddHeat + Hash_TriggerNoise. LOS_IsClear: CA_RayCastLine, bersih bila tanpa hit atau hitDist≥targetDist−0,6. Melee radius 0 = senyap (desain).

### security/anticheat_core.inc + achecks.inc + dps.inc
Flag → log WARN; achecks: sampel posisi 1 s (skip kendaraan/grace/frozen), teleport >40 m atau speed >20 m/s → HandleFlag: INSERT anticheat_flags (char_id/flag_type/payload/created_at, %e, tquery) + jendela 120 s; ≥6 flag → kick; ≥3 → freeze 2500 ms. AC_ClearSample saat teleport server. AC_OnWeaponShot: interval minimum per senjata (pistol 110, deagle 320, shotgun 650, sawnoff/combat 900, uzi/mp5/tec9 90, ak/m4 800, rifle 900, sniper 1200, toleransi 20 ms) → RAPID_FIRE. dps.inc: jendela 1000 ms cap 320, lebih → flag + batal (hanya jalur zombie).
Risiko: return rapid-fire diabaikan pemanggil (hanya flag, tidak memblokir); speed check melewatkan driver.

### core/ (logger, perf, mysql, spatial_hash, stats, telemetry, selftest)
logger: timestamp+level ke print + append logs/laststand.log (fopen per panggilan). perf: PerfLastMs/PeakMs[7 sistem], jendela 60 s, log [PERF] akhir/puncak, /diag. mysql: satu handle gSQL, connect_file(mysql.ini), ping SELECT VERSION/DATABASE log latensi; semua query threaded. spatial_hash: grid zombie sel 100 m (60×60), pemain 200 m (30×30), origin −3000; Hash_NearestPlayer filter connected/non-NPC/alive/not-safe/lobby/±25 m vertikal (2D); Hash_TriggerNoise → Zombie_OnNoiseHeard. stats: /stats satu UNION ALL kill_log GROUP BY zone/weapon/type, top-3, dialog. telemetry: INSERT kill_log (dist clamp 0–300). selftest: boot 8 s + /selftest, 15 cek registry statis + 8 ground raycast + ground per zona + 2 cek DB (16 tabel + 6 kolom), buffer query 2048, probe bcrypt hash+verify.
Risiko: nearest 2D vs noise 3D; cek companion selftest tautologis; logger fopen per panggilan (biaya IO).

Alur tembakan→raid: OnPlayerWeaponShot → Noise_Emit (radius×skill×event) → (a) Hash_TriggerNoise membangunkan zombie (≤20 m investigate, ≤60 m menoleh) dan (b) Horde_AddHeat (zona r/10, base r/6 bila ≤26 m). Horde_Tick decay; zona ≥45 → cluster; base ≥30 → raid ber-anchor leash 38 m; bertahan → $420/35XP. NightWatch eskalasi + boss malam.

---

## 5. Domain config/ + inventory/ + loot/ + economy/ + progression/ + ui/ + interact/

### config/
constants.inc: semua limit (inventory 20 slot/30 kg, stash 60→120, autosave 120 s tick 10 s batch 5, palet UI, DEV_MODE 0, AUTH_INPUT_DIALOGS 0). items.inc: gItemRegistry[36], id permanen (dipersist MySQL, dilarang renumber; 23–35 item baru incl 4 tas); aksesor Item_IsValid/GetWeight/GetName/GetMaxStack/GetCategory = kontrak semua modul. loot_tables.inc: gZombieLoot[8 tipe][10] berbobot + gRareLoot[17] + gNodeLoot[14] + gTierBonusLoot (ganti 2 slot akhir tier 2/3); specimen normal 14%, elite 45%, maks 1/mayat. recipes.inc: gRecipes[21], blueprint opsional, rcResult -1/-2 = repair alat/senjata +40%; upgrade tas berantai. zombies.inc: gZombieProfiles[8], boss minTier 99 specimen 100%. structures.inc: gStructures[7] biaya/HP/tier/limit; Crate +20 slot stash. lobbies.inc: 3 sub-lobby VW 100/200/300 (Quarantine; Outbreak speed 1,15 damage 1,35; Nightfall malam permanen; aktif 2).
Risiko: model 19994/19995 dipakai ganda.

### inventory/
inventory_core.inc: PlayerInventory[20] (itemId/amount/duration) + dirty flag. AddItem cek berat vs Bag_Capacity (tas baru via Bag_CapacityForTier agar upgrade tak ditolak), stack lalu slot kosong; RemoveItem cek cukup dulu; Compact gabung+rapatkan. Risiko: stacking abaikan itemMaxStack; slot baru tak reset durability.
bag.inc: tas TOOL pasif tier terbaik; +10/+20/+32/+45 kg; visual attached slot 9 bone 1 model 1550 skala per tier; anim rummage 2,2 s; overload ≥90% → drain ×1,35.
hotbar.inc: 5 slot simpan ITEM ID bukan slot; FindSlotForItem resolusi runtime; persist CSV characters.hotbar; /hs 1–5 mobile.
container_lock.inc: ContainerLock[24] per (kind,entity) PARTY_STASH/GRAVE/STASH; Acquire tolak bila dipegang lain, tahan 60 s; Lock_Tick dari Player_Tick; pelengkap bukan pengganti act-then-verify.
invpers.inc: Save satu REPLACE 20 slot (kosong = item_id 0); jalur mutasi tunggal Inventory_Grant = AddItem + Ledger (INSERT item_ledger delta/reason/ts) + Save; Consume simetris. Senjata WeaponCondition[47] aus 1/tembakan; Weapons_Save DELETE+INSERT. Risiko: ledger & save tanpa transaksi.
stash.inc: StashContents[120], kapasitas 60+20/crate, kg 240+40/crate; dialog 2201–2204 TABLIST_HEADERS navigasi di akhir; specimen dilarang; hanya di kamp. Risiko: DepositStack tak cek kg; Withdraw tak cek berat tas (bisa overload).

### loot/
drops.inc: LootDrop[300] expire 3 menit purge tertua; Spawn object+label+pickup tipe 2; pickup PLAYER-SPECIFIC untuk drop milik → anti-ESP; roll server-side bobot kumulatif; DropFromZombie per tipe (Walker 10% kosong, Brute 3–4), tier ganti 2 slot akhir, specimen 14%, rare 8%+tier+4 malam cap 25; Brute/Stalker → Corpse_Create; TakeNearest filter lobby + owner 60 s + hook Tutorial/kontrak dokumen. Risiko: callback pickup tak cek jendela owner; pickup owner tak pernah jadi publik setelah 60 s (hanya /take).
nodes.inc: peti per lobby min 3 maks 8 (hanya kind CRATE); tick 10 s throttle 20 s; posisi CA raycast bukan hub ≥12 m; 2 peti jaminan 45–120 m saat spawn; ikon peta 250 m.
corpse.inc: Corpse[24] maks 3 jenis expire 5 menit; roll saat dibuat + specimen 45%; dialog LIST 2951; Take re-cek jarak 3+1 m. Risiko: tanpa Lock_* — indeks bergeser bisa salah ambil.
grave.inc: Grave[16] 20 slot expire 10 menit owner eksklusif 2 menit; CreateFromPlayer SEBELUM Inventory_Clear; persist graves+grave_items (id manual GraveNextId); BootLoad 4 s retry 2 s LEFT JOIN; Show wajib Lock_Acquire; Take cek lock+jarak+lobby+world; PersistItems DELETE+INSERT seluruh isi. Risiko: id manual bisa tabrakan sebelum boot-load; persist non-atomik; pasca-restart eksklusivitas hilang.

### economy/
vendor.inc: beli 14 entri (tas 420/950/2100), jual 9 material (Scrap 8…Dokumen 180) + Skills_SellBonus 5%/level; Buy SpendCash→Grant gagal→refund; Sell Consume dulu baru EarnCash; hanya kamp. Bug: body dialog escape backslash-n literal → newline tampil sebagai teks.
trade.inc: sesi /trade 5 m satu lobby tidak downed; bitmask slot + cash maks 100 k clamp eksekusi; konfirmasi ganda, perubahan penawaran reset kedua pihak (anti swap-scam); eksekusi: validasi ulang 6 m → Trade_Fits pra-cek → fase 1 Consume semua → fase 2 Grant → fase 3 cash + SaveCharacter. BUG KRITIS TERVERIFIKASI: fase 2 membaca slot yang sudah dikosongkan fase 1 → item hangus (lihat §7). Trade_Fits 30 kg hardcoded abaikan tas.

### progression/
skills.inc: 5 cabang ×3 level — Marksman +5% dealt, Toughness -5% taken, Scavenger +1 material & +5% jual, Medic +25% heal, Stealth -15% noise. Poin dari level up (kurva 120×level, guard 9 level/kill, share party). Persist skill_points + REPLACE character_skills. Respec Scrap 3.
leaderboard.inc: satu UNION ALL 4 kategori (KILL/UANG/JAM/RANK) top 5, cek mysql_errno, tanpa dialog (mobile aman).
contracts.inc: 4 slot harian deterministik seed (dayId×2654435761 + charId×40503 + slot×7919); INSERT IGNORE UNIQUE(char_id,day_id,slot) + SELECT + fallback memori; progres via OnZombieKill (bukan hub) & OnDocumentFound (dua jalur loot); klaim cash+Grant+15XP; save throttled ContractDirty. Risiko: claimed tetap diset walau Grant gagal; rollover tengah malam tak di-reload.

### ui/ + interact/
hud.inc: 9 PlayerTextDraw (vitals, needs warna, infect, zona, $+scrap, LVL/SP/SAMPEL/LUKA, fuel, bar boss); HUD_Tick 500 ms refresh penuh TANPA dirty-flag render (dirty-flag hanya untuk persistensi). toast.inc: MVP chat satu baris. menu.inc: /menu LIST 13 baris urutan case = baris; kompas 8 arah (utara=-Y); onboarding timer 12/26 s. craft_ui.inc: daftar resep berstatus blueprint/bahan; Consume A → Consume B (gagal refund A) → Grant hasil (gagal refund A+B "craft_refund"); repair alat durability terendah/senjata di tangan +40 cap 100. inventory_ui.inc: TABLIST_HEADERS pemetaan ulang baris→slot tiap respons; /pakai & /buang nama/ordinal; Use: adrenaline satu-satunya saat downed (self-revive HP 45), heal/cure tolak HP penuh/infeksi 0, makanan→Needs, decoy→heat jauh, notebook→kontrak+5XP, kunci gudang→peti rare luar kamp (refund gagal), barricade→Base, jerrycan/kit/toolbox→Garage; TOOL aus 20/pakai; drop = Consume + Loot_Spawn owner sendiri.
dispatcher.inc: InteractPoint[32] (EXTRACT/STASH/CRAFT/SHOP/GARAGE/FUEL) + dynamic cylinder & label; KEY_YES edge-trigger; fallback /interact & /menu mobile; prioritas: mayat elite → makam → loot 2,5 m → hotspot ≤2 m; STASH hotspot juga heal penuh + cure 10 (hanya hub); enter area → GameText '[Y] Gunakan'.

Alur ekonomi tertutup: bunuh zombie → roll tabel per tipe (+tier/rare/specimen) → drop/peti/mayat → Y//take → Grant+ledger → tas (30 kg+bonus) → stash kamp (specimen dilarang) atau craft 21 resep → jual vendor (+5%/Scavenger) → cash untuk medis/ammo/tas/bensin/trade → kontrak harian → XP → level → poin skill → multiplier → ekstraksi ≥3 specimen = $120/sampel+$100 + heal/cure, cooldown 90 s/lobby, teleport pulang + heat. Uang masuk hanya dari jual/kontrak/ekstraksi/boss/raid; semua mutasi berledger.

---

## 6. Domain world/

### zones.inc
gZones[10]: Kamp Quarantine hub tier 0 (ambient 0); Idlewood/Ganton/Angel Pine/Camp Outskirts tier 1; Downtown LS + Sewers tier 2 (Downtown PvP); Airfield + Bone County tier 3 PvP. Zone_GetIdAt lingkaran 2D + |dz|≤80 first-match (overlap dimenangkan indeks kecil). Risiko: Zone_IsPvP(invalid)=1 fail-dangerous; Zone_IsNight() pakai gettime() jam nyata — dead code berbahaya, tidak konsisten dengan World_IsNight().

### daynight.inc
WorldHour=8; World_Tick 45 s/jam; 24 jam = 18 menit nyata; malam 20:00–06:00 (≈7,5 menit); cuaca 8 malam/1 siang; World_SetHour admin clamp. Waktu global satu proses — semua lobby berbagi malam (by design).

### events.inc
Event_Tick polling 5 s; event tiap 300 s tidak boleh sama berturut. Supply drop: 3 peti hasil satu roll (medkit/ammo rifle/bandage) sekitar pemain hidup non-safe spread ±7 m per lobby. Blood moon 180 s: zona pemain terbanyak → TriggerHorde 12 + noise ×1,25. Quiet hour 120 s: noise ×0,5. Event_Force admin.
Risiko: GetTickCount wrap ~24,8 hari; supply drop tanpa raycast tanah (peti melayang/tenggelam); blood moon hitung pemain lintas lobby.

### bases.inc (447 baris)
Barricade objek dunia persisten + identitas base per pemain (BaseId/BaseTier/BaseLobby/BasePlot, Barricade[40]). Base_Load SELECT bases per char_id; kosong → INSERT (shelter_id = char%250+1) → Horde_ResetBaseHeat. BootLoad 3 s LIMIT 40. PlaceBarricade consume ITEM_BARRICADE_KIT, 2 m depan pemain, objek streamer + CA_CreateObject (collision nyata). Base_Tick 2 s: snapshot posisi semua FCNPC SEKALI (hemat GetPosition) → GeneratorTick → damage barricade (≤4 penyerang r2,2, DPS×attacker×2) → BaseBuild_ZombieTick + TurretTick array sama. HP habis → destroy+DELETE+toast. /fix 2 scrap +HP nearest 3,5 m.
Risiko: boot LIMIT 40 memotong sisa diam-diam; HP barricade hanya disimpan saat repair (damage hilang saat restart); shelter_id modulo 250 bisa tabrakan; repair tidak cek kepemilikan.

### base_build.inc (968 baris)
Pool global BaseStructure[40] lazy-load per base (EnsureLoaded, bukan boot — budget objek). 7 struktur: Wall 2000 HP t1 maks 12 (+4 t2), Gate 1200 t1/2, Crate 800 t1/3 (+20 stash cap 120), Bench 900 t2/1, Bed 600 t1/1 (respawn base hanya ≥2 wall), Turret 700 t3/2, Generator 1200 t2/1. Tier 3 HP +25%. Upgrade t2 = 20 scrap+15 wood+5 chemical; t3 = 40 scrap+25 wood+10 electronics. Place: guard listitem dialog (anti index liar client), cooldown 1,5 s, tier/limit/material, bukan hub, radius petak 22 m (petak lahir di build pertama), gap ≥2 m, CA raycast tanah; consume dua tahap refund; INSERT base_structures. Bongkar refund 50%; repair 2 scrap +400 HP. Raid: ZombieTick 12 DPS×≤4 attacker×2 per tick; HP UPDATE tiap 10 s saat diserang; hancur DELETE. Generator bakar 0,06 L/tick hanya malam (≈13,5 L/malam dari tangki 20 L); HasPower = fuel>0; turret 8 dmg/2 s ke zombie ≤30 m se-lobby, attacker INVALID_PLAYER_ID (tanpa XP-farm) + noise 25 m; refill /generator ≤6 m dari jerrycan (durability%=isi 20 L); fuel simpan 60 s & saat habis.
Risiko: BaseBuildPendingSlot tunggal global → dua INSERT bersamaan salah assign dbId; base belum di-load pemiliknya tidak ada di memori → raid hanya efektif saat struktur ter-load; upgrade tier tidak hitung ulang HPmax lama; pool 40 dibagi seluruh server.

### shelter.inc
Interior 3, VW = 1000 + lobby×400 + baseId%400. Enter: simpan posisi kembali, Map_TeleportEx preload, AC_ClearSample, buat pintu; keluar: pulihkan posisi (fallback SPAWN) + Lobby_ApplyWorld. Reset saat mati/disconnect. Akses stash/craft/shop digerbang Player_IsSafe.
Risiko: baseId%400 bisa kolisi antar base di lobby sama → dua pemilik berbagi VW.

### mapping.inc
Map_Init: Streamer_SetVisibleItems(OBJECT, 920) (headroom 80) + kamp: 5 dinding retexture, papan neon material-text, barikade jalan, 3 actor invulnerable (pedagang 211, penjaga stash 16, perekrut 287) + label "tekan Y". Map_TeleportEx = kontrak teleport aman: VW/interior → Streamer_UpdateEx freeze 600 ms → SetPlayerPos z+0,15 (anti jatuh tembus). Pintu shelter 1495 per kunjungan, auto-buka ≤2,5 m / tutup ≥4,5 m; trik rotasi +0,001 z agar MoveDynamicObject interpolasi.
Risiko: objek kamp tanpa worldid eksplisit — perlu dipastikan tidak bocor ke VW shelter.

### lobbies.inc
Sub-lobby satu proses: world 100/200/300, akun/karakter/base dibagi DB, entitas dunia dipisah VW. Lobby_Set hanya di kamp (Player_IsSafe, bukan shelter) → apply world, teleport hub, UPDATE characters.lobby_id. Dialog 3001 flag LobbySelectionPending (anti dialog palsu) + bounds listitem. CountPlayers/ActiveList dipakai director & wild spawn.
Risiko: pindah lobby tidak membatalkan escort eksplisit (gagal via tick jarak); base tetap di lobby asal (sesuai docs).

### vehicles.inc (844 baris)
Satu pool TrackedVehicle[64] (wild + uji admin + garasi): fuel/tank liter, lobby, ownerChar, dbId, locked, persistent. Tangki per kategori: motor 14 L, truk 150, SUV 80, sport 65, sedan 70, default 60; konsumsi 4,5–28 L/100 km × skala game 80. WildVeh_Tick 20 s per lobby aktif: jamin ≥3 mobil dekat pemain (28–85 m, forceUsable tanpa WRECK fuel ≥25% kunci 20%), lalu 70% chance hingga 9/lobby; titik CA raycast bukan hub ≥22 m dari pemain ≥7 m antar mobil. Wild: fuel 10–45%, locked 45%, broken 35% (health 240–350, BBM nyaris 0, plat WRECK). VehicleFuel_Tick 2 s: bakar dari KECEPATAN SERVER GetVehicleVelocity (bukan odometer client — anti speed/fuel hack); terkunci/kosong → mesin mati + warn throttle 8 s; health<350 peluang 6% mogok. Driver: kunci pintar pemilik auto-unlock; orang lain /lockpick (consume 1, sukses 65%). OnDeath → asuransi garasi → unregister → DestroyVehicle LANGSUNG (ID reuse, tanpa timer). World_NearbyHint kompas ke peti/mobil terdekat.
Risiko: wild tak pernah despawn → pool 64 tersumbat wreck terbengkalai, garasi bisa gagal deploy.

### garage.inc (828 baris)
Cermin memori GarageVehicles[player][4] dari owned_vehicles. Toko 5 model $420–4.500; beli INSERT dulu uang dipotong di callback. Deploy hanya garasi kamp ≤12 m: 4 slot parkir raycast + okupansi 4 m; Vehicle_Register(persistent, locked, owner, dbId). /simpan ≤12 m: UPDATE fuel/health/locked lalu destroy. Hancur → asuransi: baris tetap health=400 fuel dipertahankan. Disconnect saat menyetir mobil milik → save+destroy. Pom $5/L; jerrycan 20 L durability=isi; /sedot dari mobil tak berpengemudi (rugi 0,5 L, 25% heat+8); Repair Kit+3 scrap → 1000; Toolbox → penuh + mesin nyala.
Risiko: callback beli tidak cek ulang cash (bisa negatif bila uang terpakai paralel); mobil milik ditinggal jalan kaki tetap di dunia saat owner disconnect (fuel tak tersimpan sampai store/hancur).

### escort.inc (333 baris)
FSM per lobby: idle(cooldown)→aktif→complete/fail. Mulai hanya zona bahaya, hidup, budget tersedia; trader FCNPC skin 28 mengikuti (GoToPlayer ColAndreas/raycast reissue 2 s). Escort_Tick 2 s: gagal bila pengantar/trader mati, jarak >45 m, durasi >8 menit; sukses bila pengantar di hub & trader ≤12 m → $750 + 8 scrap + 2 medkit + 25% blueprint suppressor + 40 XP. Zombie ≤3 m (≤3 attacker) cakar 16 dmg; HP asli FCNPC disinkron turun (pemain boleh menembak trader). Heat +20 tiap 4 s di posisi trader (umpan horde). Cooldown sukses 10 mnt/gagal 3 mnt. OnNpcDeath hook sebelum zombie.
Risiko: ESCORT_MAX_ACTIVE tak terpakai; GetPosition per zombie per tick (tak di-snapshot seperti Base_Tick).

### companion.inc (758 baris)
1 rekrutan/karakter (CompNpc/CompHealth/CompTier/CompDowned/CompDeadNpc), tabel companions UNIQUE char_id + tier. Tier 0/1/2: HP 100/160/220, damage 13/19/26. Rekrut ≤5 m perekrut kamp: $600 + 2 makanan + 1 air (refund gagal). Upgrade t1 $400+6 kain+2 elektronik; t2 $900+1 repair kit+4 elektronik+10 ammo rifle — validasi semua bahan sebelum consume, refund penuh. Tick 2 s: despawn bila owner offline/budget penuh; tumbang → destroy DITUNDA ke tick (CompDeadNpc anti re-entrancy FCNPC_OnDeath), timeout 3 menit → DELETE; melee leech 6/zombie ≤2,6 m; tembak zombie terdekat ≤26 m tiap 900 ms attacker INVALID_PLAYER_ID (anti AFK-farm XP/kontrak, loot tetap jatuh) + noise 7 m; follow ≤6 m repath 1,5 s, HOLD auto-follow >52 m. Revive 1 perban → HP 50 (refund gagal).
BUG TERVERIFIKASI: (1) Companion_Save ON DUPLICATE KEY UPDATE hanya name/health/mode — tier tidak dipersist untuk baris lama (upgrade hilang saat relog); (2) dialog /teman baris 2 "Tolong dia"/3 "Tingkatkan" tetapi case 2→Upgrade, case 3→Revive — TERTUKAR; (3) query pakai format bukan mysql_format (nama server-controlled, risiko rendah).

Alur dunia: zona (tier loot/PvP/ambient) → World_Tick 45 s/jam malam 20–06 → event 5 menit → noise mengisi heat (base per lobby+slot radius 26 m decay 0,94) → horde & raid ber-anchor → barricade/struktur dicakar (≤4 attacker DPS×2 per 2 s), turret membalas 8 dmg/2 s malam wajib HasPower → ekonomi scrap menutup loop. Shelter VW terisolasi via Map_TeleportEx.
Siklus kendaraan: wild spawn dekat pemain (jaminan 3/lobby) → lockpick 65%/kunci pintar → BBM liter dari kecepatan server → pom/jerrycan/sedot → repair kit/toolbox → garasi beli→deploy→simpan→hancur=asuransi health 400 → DestroyVehicle langsung, satu pool 64 slot.

---

## 7. Bug TERVERIFIKASI manual (bukti file:line)

Prioritas P0 = merusak pemain/ekonomi sekarang juga; P1 = penting; P2 = hardening.

1. [P0] Registrasi pemain baru TERBLOKIR total.
   AUTH_INPUT_DIALOGS=*** (constants.inc:42). AuthStage = AUTH_REGISTER hanya di-set di dalam #if AUTH_INPUT_DIALOGS (account_auth.inc:105). Cabang #else (baris 118–121) hanya mengirim pesan chat tanpa set stage. Akun baru → Account_OnLookup (baris 41–49) → Account_ShowRegister → stage tetap AUTH_LOADING. Guard /daftar (commands.inc:142) `AuthStage != AUTH_REGISTER` → selalu ditolak "Kamu tidak sedang mendaftar". Tidak ada satu pun assignment AUTH_REGISTER di luar #if (grep seluruh gamemodes). Fix: set AuthStage = AUTH_REGISTER di cabang #else / di Account_OnLookup saat tanpa baris.

2. [P0] Trade menghilangkan item kedua pihak.
   trade.inc:243–267 fase 1 Inventory_Consume seluruh stack penawaran → Inventory_RemoveItem mengosongkan slot (inventory_core.inc:119 invItemId = ITEM_NONE_ID). Fase 2 (trade.inc:269–291) membaca ULANG PlayerInventory[slot] yang sudah kosong → Item_IsValid gagal → continue → tidak ada Grant. Item terconsume + ledger trade_out, tanpa trade_in. Fix: snapshot (itemid, amount, durability) per slot penawaran SEBELUM fase 1, fase 2 pakai snapshot.

3. [P0] Needs_Clamp tidak pernah meng-clamp.
   needs.inc:22–27 mengembalikan 0/100/1 (bukan nilai ter-clamp; return 1 untuk nilai normal). Semua pemanggil (baris 77–78, 86–87, 156–158) mengabaikan return. Akibat: lapar/haus bisa negatif, bladder >100, makan/minum bisa >100. Fix: jadikan clamp in-place via by-reference `stock Needs_Clamp(&Float:value)` atau assign return.

4. [P1] Ban permanen bisa di-bypass auto-unban.
   moderation.inc:164 `ORDER BY expires_at DESC LIMIT 1` → ban permanen (expires_at=0) selalu terakhir. Bila ada ban sementara kedaluwarsa, baris itu terpilih → cabang 186–195 UPDATE is_banned=0 padahal ban permanen masih ada. Fix: prioritaskan permanen, mis. `ORDER BY (expires_at = 0) DESC, expires_at DESC LIMIT 1` atau cek EXISTS ban permanen dulu.

5. [P1] Tier rekrutan hilang saat relog.
   companion.inc:192–193 ON DUPLICATE KEY UPDATE hanya name/health/mode; tier hanya masuk saat INSERT pertama. Fix: tambah `tier = VALUES(tier)`.

6. [P1] Dialog /teman: aksi Tolong/Tingkatkan tertukar.
   companion.inc:712–714 baris 2 = "Tolong dia (butuh 1 perban)", baris 3 = "Tingkatkan perlengkapan"; switch 752–753 case 2 → Companion_Upgrade, case 3 → Companion_Revive. Fix: tukar case 2/3.

7. [P1] /daftar memakai sscanf padahal include/plugin mismatch.
   commands.inc:149 `sscanf(params, "s[41]", newPassword)` — melanggar invariant MEMORY (auth wajib Command_Arg); memicu warning "Include / plugin mismatch" dan potensi parse gagal. Fix: ganti Command_Arg.

8. [P1] Kunci pemilik bocor ke semua pemain.
   account_auth.inc:73 pesan login menyebut "/pulihkan passwordbaru ferzdevz (kunci pemilik)" — siapa pun yang membaca chat tahu kunci superadmin. Ditambah kunci hardcode di roles.inc. Fix: hapus kunci dari pesan; pindahkan kunci ke mysql.ini/env atau hash; beri rate-limit /pulihkan.

9. [P2] Warning compile: 208 (invpers.inc:155 tag result used before definition), 225 (char_create.inc:264 unreachable code), 234 (colandreas.inc deprecated Iter_SafeRemove — pihak ketiga).

## 8. Risiko lain (temuan subagent, belum diverifikasi manual satu per satu)

- Race slot-reuse: callback async auth/ban keyed playerid tanpa validasi AccountId generasi → recovery bisa menimpa password akun penghuni baru; Moderation_OnBanSaved bisa salah akun.
- OnGameModeExit tidak menyimpan karakter online (hilang ≤2 menit progres).
- Revive tidak menyembuhkan infeksi → loop turned + spawn runner (potensi farm).
- Party: /invite id eksplisit tanpa cek lobby/jarak; Party_Show %d untuk Float; bubar saat relog (by design v1).
- Party stash: Put tanpa act-then-verify; DELETE ambil-semua tanpa predikat amount; rollback bisa gagal → potensi dupe/lenyap.
- Stash: DepositStack tak cek kg; Withdraw tak cek berat tas.
- Inventory: stacking abaikan itemMaxStack; slot baru tak reset durability.
- Corpse tanpa Lock_* (indeks bergeser bisa salah ambil); grave id manual + persist non-atomik.
- Kontrak/tutorial: reward hangus bila Grant gagal (tas penuh) tapi progres tetap maju.
- Horde raid reward hanya cek connected+lobby bukan charId; trim ambient bisa membuang zombie raid ber-anchor.
- Damage PvP tidak di-clamp ke base damage senjata; DPS check hanya cabang zombie; rapid-fire hanya flag tidak memblokir.
- Base: pool 40 global + lazy-load (raid tak efektif bila struktur belum ter-load); BaseBuildPendingSlot tunggal; barricade boot LIMIT 40; HP barricade hilang saat restart; shelter VW baseId%400 kolisi.
- Kendaraan: wild tak pernah despawn → pool 64 tersumbat; callback beli garasi tak cek ulang cash.
- Event: GetTickCount wrap 24,8 hari; supply drop tanpa ground raycast.
- zones.inc Zone_IsNight() dead code pakai jam nyata; Zone_IsPvP(invalid)=1 fail-dangerous.
- HUD refresh penuh 500 ms tanpa dirty-flag render (boros TextDrawShow, belum kritis di 200 pemain).
- Logger fopen per panggilan; selftest cek companion tautologis.
- Panel /sa daftar pemain mentok 32 tanpa paging; kick panel tanpa cek superadmin.
- Dokumen README/ARCHITECTURE menyebut open.mp padahal runtime SA-MP 0.3.7 — perlu sinkronisasi dokumen.

## 9. Kekuatan arsitektur (yang sudah benar dan harus dipertahankan)

- Damage satu pintu server-authoritative (return 0 engine) + validasi range/LOS/rate/DPS.
- Semua mutasi item lewat Grant/Consume + item_ledger append-only → dupe bisa diaudit.
- Container bersama: Lock_* + act-then-verify cache_affected_rows + rollback.
- Budget NPC dinamis + trim + sync adaptif 80/100 ms + LOD 3 tingkat + spatial hash → terbukti 1 pemain ±52 zombie bukan 190.
- Autosave stagger batch 5/10 s (anti spike query); semua MySQL threaded pool 4.
- Selftest 26 cek boot + probe bcrypt + ground raycast = bukti runtime, bukan asumsi.
- Invariant MEMORY.md sangat disiplin (nama public ≤31 karakter, header TABLIST, snapshot dialog, urutan include, ID reuse kendaraan, re-entrancy FCNPC_OnDeath).
- Mobile first-class: semua interaksi punya fallback chat/command; auth via chat karena inputtext mobile kosong.
- Ekonomi tertutup: uang masuk hanya jual/kontrak/ekstraksi/boss/raid; specimen tak bisa ditimbun; hub nol-farming.

## 10. Rekomendasi urutan kerja

1. Fix P0 #1 (registrasi) — satu baris stage di cabang chat; tanpa ini pemain baru tidak bisa masuk sama sekali.
2. Fix P0 #2 (trade snapshot) + P0 #3 (Needs_Clamp by-reference).
3. Fix P1 #4–#8 (ban order, companion tier+dialog, sscanf /daftar, bocoran kunci).
4. Tambah invariant baru ke MEMORY.md untuk tiap fix (pola symptom→root cause→rule).
5. Hardening bertahap dari §8: race slot-reuse (generasi AccountId di callback), simpan saat OnGameModeExit, party stash Put verify, despawn wild vehicle, clamp damage PvP.
6. Sinkronkan README/ARCHITECTURE dengan runtime aktual (SA-MP 0.3.7) atau migrasi open.mp sungguhan.
7. Gate tiap perubahan: sampctl build exit 0 + /selftest PASS + uji in-game jalur terkait.

---

## 11. Status Perbaikan (26-11-19, sesi yang sama)

SEMUA item §7 (P0/P1) dan mayoritas §8 sudah DIPERBAIKI dan diverifikasi:

Gate verifikasi nyata:
- `sampctl build` exit 0 — tinggal 1 warning pihak ketiga (colandreas.inc:603 deprecated Iter_SafeRemove). Warning 208 (invpers) dan 225 (char_create) hilang.
- Server boot ulang: MySQL tersambung 181 ms, probe bcrypt OK, ColAndreas raycast OK.
- Selftest boot: 34/34 PASS, 0 FAIL (sebelumnya cek companion tautologis; sekarang cek maxnpc nyata).
- logs/errors.log kosong setelah boot.

Fix P0:
1. Registrasi: AuthStage = AUTH_REGISTER di-set di cabang chat (Account_OnLookup + #else ShowRegisterDialog).
2. Trade: snapshot offerItemId/offerAmount[2][20] sebelum fase consume; fase grant pakai snapshot; Trade_Fits pakai Bag_Capacity.
3. Needs_Clamp jadi by-reference (&Float:value), clamp in-place.

Fix P1:
4. Ban: ORDER BY (expires_at = 0) DESC, expires_at DESC.
5. Companion: tier = VALUES(tier) di upsert.
6. Dialog /teman: case 2 = Revive, case 3 = Upgrade (sesuai label).
7. /daftar: Command_Arg, bukan sscanf.
8. Kunci admin: dipindah ke scriptfiles/admin_keys.cfg (gitignore, backup, symlink instance), kunci DIROTASI ke nilai acak baru; pesan chat tidak lagi menyebut kunci; rate-limit per IP untuk /admin /superadmin /pulihkan.

Hardening §8 yang dikerjakan:
- Race slot-reuse: RecoveryAccount/PasswordChangeAccount ditangkap saat request; ban/kick membawa accountDbId; callback batal bila akun berubah.
- OnGameModeExit menyimpan karakter+kontrak+companion; autosave tidak lagi save ganda.
- last_ip login diisi IP asli (bukan 0.0.0.0).
- Revive menstabilkan infeksi <80; Player_CureInfection hitung ulang stage inline (fix cure saat turned).
- Party: invite id eksplisit cek lobby+jarak; Party_Create gate login; HP dialog %.0f.
- Party stash: Put act-then-verify + rollback; DELETE ambil-semua pakai predikat amount snapshot; rollback gagal log ERROR audit.
- Stash/inventory: transfer all-or-nothing (pre-check berat+slot+stack), stacking hormati maxStack, slot baru reset durability, withdraw cek berat tas.
- Corpse: LOCK_KIND_CORPSE (satu penggeledah).
- Loot: callback pickup cek jendela owner; pickup jadi publik setelah 60 dtk (recreate global).
- Grave: id dari MAX(id) DB saat boot; INSERT ditahan sampai GraveIdReady.
- Kontrak/tutorial: reward Grant sukses dulu baru progres maju.
- Zombie: generasi zeGeneration untuk cleanup timer (anti id-reuse).
- Raid: reward cek charId; trim lewati zombie ber-anchor; raid set DirectorLastHordeTick.
- Damage PvP: clamp base senjata ×2 + flag DAMAGE_CAP (weapon 0–40); rapid-fire memblokir shot (return 0).
- Base: pending insert per pemain; HP barricade persist throttle 10 dtk; boot LIMIT diberi WARN.
- Shelter: VW unik 1000+baseId via Shelter_VwOf (3 call site).
- Kendaraan: sweep despawn wild terbengkalai (10 menit, >150 m, tanpa driver); garasi beli anti-pending + cek ulang cash + DELETE rollback.
- Event: jadwal pakai gettime() epoch (anti wrap 24,8 hari); supply drop raycast tanah.
- Vendor: newline dialog diperbaiki (\\n literal → \n; sebelumnya semua baris menyatu).
- HUD: dirty-flag render (cache string/warna/visibility per TextDraw).
- Panel /sa: paging pemain 30/halaman; kick gate admin + anti-kick-admin; menu teleport gate superadmin.
- Selftest companion: cek maxnpc nyata vs MAX_ZOMBIES+1 dan maxnpc<=maxplayers; pawn.json maxnpc 190→200.
- Command: alias shadowing dibersihkan (/fix base, /perbaiki kendaraan, /peti party, /lootnode admin); blok /login /register duplikat dihapus.
- Escort: ESCORT_MAX_ACTIVE ditegakkan (global 2); Zone_IsNight dead code dihapus.
- Warning compile 208/225 hilang; Weapons_GetBaseDamage trailing return.

Sisa yang BELUM dikerjakan (sengaja, butuh keputusan desain/uji in-game):
- [SELESAI 26-11-19 lanjut] Transaksi MySQL eksplisit: core/transaction.inc (koneksi khusus terurut, START TRANSACTION/COMMIT) untuk ledger+save inventory, weapons DELETE+INSERT, stash save, grave+grave_items. Tx_Boot baca scriptfiles/mysql.ini (sandbox fopen PAWN; run-server.sh/instance.sh menyalin otomatis).
- [SELESAI] Persist grave atomik (satu transaksi dengan baris makam).
- [SELESAI] Logger handle persisten (buka ulang saat ganti hari/gagal tulis).
- [SELESAI] Blood moon per-lobby (zona terpadat di tiap lobby, horde+notifikasi per lobby).
- [SELESAI] tests/ diisi checklist regresi manual: tests/REGRESSION_26-11-19.md (12 bagian, semua jalur yang diperbaiki).
- [SELESAI] ARCHITECTURE.md diberi catatan status runtime aktual.
- [SELESAI 26-11-19 lanjut-2] Interior shelter CUSTOM: ruang 12x9 m objek dinamis (lantai warm_oak, dinding matte_slate, plafon, 3 crate, papan "SHELTER PRIBADI") di interior 0 + VW unik; pintu selatan fungsional (melangkah keluar = kembali ke dunia); batas ruang + penyelamatan jatuh di ShelterDoor_Tick; objek dihancurkan saat keluar/mati/disconnect.
- [SELESAI] Leaderboard musiman: kategori ke-5 "KILL MUSIM INI (BULAN INI)" di /top (filter SQL awal bulan, satu UNION ALL).
- [SELESAI + DATA REPAIR] Bug epoch overflow: gettime()*1000 PAWN 32-bit menghasilkan created_at NEGATIF di 6 tabel; helper EpochMs ("%d000" string) menggantikan 7 call site; data lama dipulihkan (0 negatif tersisa, query MUSIM terverifikasi mengembalikan data).
- Uji in-game jalur auth/trade/raid/shelter dengan client nyata (PC + mobile): checklist sudah tersedia; eksekusi butuh client SA-MP nyata, tidak bisa dari lingkungan ini.
- Load test 100 pemain + 190 zombie (ROADMAP fase 8): butuh bot/banyak mesin.
- Migrasi open.mp sungguhan: keputusan arsitektur terpisah (runtime sekarang 0.3.7 stabil).

### 11b. Ronde audit swarm independen (26-11-19 lanjut-3)

4 reviewer paralel (security/database/gameplay/performa) + code-reviewer. Temuan diverifikasi ke file:line sebelum fix; T9 (psZone basi) DITOLAK sebagian — main.pwn Player_Tick meng-update psZone, tapi sentinel -1 memang berisiko SQL unsigned (di-clamp 255).

KEAMANAN (F-01..F-24) — semua KRITIS/TINGGI ditutup:
- F-01 KRITIS: /pulihkan bypass nick pemilik → takeover superadmin. Fix: kunci super wajib untuk SEMUA akun.
- F-02 KRITIS: level admin diterapkan saat lookup nick + command admin sebelum gate playing → escalation tanpa password. Fix: AccountDbLevel ditangkap saat lookup, diterapkan setelah password benar; IsServerAdmin/SuperAdmin menuntut Account_IsPlaying.
- F-03 KRITIS: wizard karakter + CHARSEL tanpa gate → buat/main karakter di akun orang lain. Fix: AccountAuthed (set saat registrasi/login sukses) meng-gate wizard, /nama, CHARSEL, CHARNEW, SelectCharacter.
- F-04 TINGGI: RequestClass tanpa gate → heal/respawn paksa saat hidup/downed. Fix: hanya saat HP engine 0 atau belum playing.
- F-05 TINGGI: OnPlayerTakeDamage return 1 → ledakan/kendaraan/jatuh lolos party/hub/god/downed. Fix: return 0 + Damage_Apply; sumber id>=41 bebas jarak/LOS.
- F-06 TINGGI: panel SA_WORLD tanpa gate → supply-drop farm via spoof dialog. Fix: satu gate IsServerAdmin di atas switch panel.
- F-07 TINGGI: durability reset di drop/trade/party-stash/grave → jerrycan/tool refresh gratis. Fix: durability end-to-end (AddItem/Grant/Loot_Spawn parameter + snapshot trade + cache party_stash + grave).
- F-08 TINGGI: party-stash double-take sebelum callback. Fix: pending guard per slot + cache dikurangi segera + reload DB di callback.
- F-09 SEDANG: callback async registrasi/load keyed playerid. Fix: ConnectGen + RegNick (registrasi), LoadCharGuard (inventory/weapons/stash).
- F-10 SEDANG: brute force login lintas reconnect + spam bcrypt. Fix: rate-limit per-IP 16 slot (blokir tak bisa di-evict) + LoginPending guard.
- F-11 SEDANG: evict IP memblokir reset + fallback kunci default. Fix: entri terblokir tak di-evict; tanpa admin_keys.cfg login kunci NONAKTIF (default hardcode dihapus).
- F-22 RENDAH: trade respons hitung ulang baris. Fix: snapshot TradeRowSlot.
- F-24 RENDAH: /take tembus VW. Fix: cek ldWorld.
- D4 TINGGI (gameplay): Zone_IsPvP tak pernah dipanggil → off-PvP tidak ditegakkan. Fix: kedua pihak wajib zona PvP di Damage_ValidatePlayerToPlayer.

DATABASE (T1..T15):
- T1 KRITIS: gTxSQL==gSQL (duplicate-connection reuse; bukti mysql.log). Fix: DUPLICATE_CONNECTIONS=1 + options POOL_SIZE 1 + assert handle berbeda; komentar arsitektur dikoreksi (tquery terurut per handle; pool hanya pquery). Terverifikasi boot: "handle terpisah", tanpa warning duplicate baru.
- T2/T3 TINGGI: transfer stash/grave/party/trade/vendor multi-commit → jendela crash dupe/loss. Fix: satu Tx per transfer logis (TxDepth bersarang); SaveCharacter lewat handle TX; vendor sertakan UPDATE cash.
- T4 SEDANG: API Tx tanpa depth/rollback/error. Fix: TxDepth + Tx_Rollback + public OnQueryError (TxFailed → COMMIT jadi ROLLBACK).
- T5 SEDANG: shutdown COMMIT parsial + double-close + race drain. Fix: ROLLBACK bila terbuka; SQL_DelayedShutdown menunggu mysql_unprocessed_queries==0 (maks 5 dtk); close hanya bila handle berbeda.
- T6/T7/T15: migrasi 020_hot_query_indexes.sql (kill_log kind_time_killer/kind_killer/killer_cover + characters cash/rank/playtime) diterapkan live; EXPLAIN MUSIM kini range+Using index.
- T8 SEDANG: playtime_sec/rank_points tak pernah ditulis → /top JAM/RANK mati. Fix: mirror memori + akumulasi delta autosave + award (ekstraksi+10, kontrak+5, raid+15, boss+20) + load/save kolom.
- T9 (sebagian ditolak): psZone di-update Player_Tick; sentinel -1 di-clamp 255 di SaveCharacter + telemetry.
- T10 SEDANG: Contracts_Save UPDATE tanpa affected check → re-claim dupe. Fix: upsert ON DUPLICATE KEY UPDATE.
- T11 RENDAH: graves kedaluwarsa tak dipurge. Fix: DELETE items+graves expired saat boot.
- T14 RENDAH: buffer ledger 192→256, achecks 320→512, selftest 2048→3072, char load 320→384, save 640→704.

GAMEPLAY/EKONOMI (F1..N6):
- F1 kritis: ekstraksi dalam hub + specimen tanpa syarat. Fix: cooldown per-pemain + specimen hanya bila killer ter-tag & tidak safe.
- F2 tinggi: raid AFK turret. Fix: cooldown 10 menit, reward pemilik <=40m, heat minimum 30 (konstanta mati 24 dihapus).
- F3 tinggi: respawn 60 peluru + tanpa biaya mati. Fix: 60 hanya spawn pertama sesi, respawn 12; biaya medis $100+$25/level cap $600.
- F4 tinggi: upgrade base tanpa precheck Chemical/Electronics. Fix: all-or-nothing + refund.
- F5 sedang: kontrak dokumen farm 1 dokumen drop/pickup. Fix: klaim konsumsi dokumen sejumlah target.
- F6 sedang: NPC finishing blow = kredit penuh pemain. Fix: kredit hanya killing blow pemain.
- F7 sedang: escort start tepi hub. Fix: mulai >=150m dari kamp + grant-first medkit.
- D1/N1 tinggi: infeksi 1/8 tekanan desain + loop re-turn. Fix: gigitan 60% (Bloater 100%) x 12-25; infeksi reset saat mati.
- N2 tinggi: mati hilang senjata sampai relog. Fix: Weapons_Load saat respawn.
- N3: boss anchor newbie. Fix: level>=5 ATAU party>=2 ATAU tier>=2.
- F8 rendah: /zspawn type 7 lolos. Fix: clamp 0-6.
- E1 tinggi: psScrap mata uang mati. Fix: semua reward → ITEM_SCRAP; legacy DB dikonversi sekali; HUD baca inventory.
- D3 tinggi: blueprint suppressor item mati. Fix: dihapus dari loot/escort (ganti electronics); suppressor = roadmap.
- F9 rendah: streak tidak reset saat mati. Fix: reset di OnPlayerDeath.
- DITUNDA (butuh playtest): rebalance harga vendor/craft (E2), hierarki income (E3), harga newbie (N4), implementasi suppressor penuh, klinik hub, channel revive (D2), turned playable 90 dtk.

Gate verifikasi ronde ini: sampctl build exit 0 (1 warning pihak ketiga); boot selftest 34/34 PASS 0 FAIL; TX handle terpisah terkonfirmasi log; errors.log kosong; migrasi 020 live + EXPLAIN covering.

### 11c. Ronde code-reviewer regresif (26-11-19 lanjut-4)

Reviewer @code-reviewer + @samp-pawn-wizard mengaudit perubahan sesi (snapshot pra-S1..S4). 18 temuan diverifikasi; 4 sudah tertutup fix sebelumnya (#8 trade snapshot, #9 durability, #11 registrasi, #14 stash TX). Sisanya DIPERBAIKI:

Dialog mati total & salah index (TINGGI):
- #1 Grave_OnDialogResponse tidak dirouter di main.pwn → item makam tidak bisa diambil via UI. Fix: routing di rantai OnDialogResponse.
- #2 Corpse: body punya 3 baris non-item (nama/info/kosong), Corpse_Take memakai listitem mentah → semua klik ditolak (cpCount maks 3). Fix: offset listitem-3.
- #3 Contracts_OnDialogResponse tidak dirouter + index bergeser 2 (judul+kosong). Fix: routing + offset listitem-2.

Auth/moderasi (SEDANG):
- #4 Dua call site Moderation_KickTimer "i" padahal public butuh (playerid, accountDbId) → guard gagal, kick tidak jalan. Fix: "ii" + accountDbId.
- #5 Auto-unban buntu: cabang is_banned return sebelum AccountId/hash/stage di-set; ShowLogin saja tidak bisa login. Fix: Account_BeginAuth (lookup ulang penuh).
- #11b Login verify tanpa capture akun → callback bisa menulis ke akun penghuni baru. Fix: LoginAccount ditangkap saat SubmitLogin, dicek di callback.

Kontradiksi state (SEDANG):
- #6 Panel teleport "Shelter" = SetPlayerPos koordinat langit di VW lobby → jatuh mati. Fix: rutekan Shelter_Enter (bangun ruang + VW unik).
- #10 Admin_FullRestore tidak reset psInfectionStage/psDowned → turned tetap turned walau HP 100. Fix: reset keduanya.
- #12 Shelter_Reset hanya saat connect → mati/disconnect bocorkan 41 objek + Door_Tick melempar respawn kembali. Fix: Shelter_Reset di OnPlayerDeath & OnPlayerDisconnect.
- #18 Turned di shelter melahirkan runner di VW lobby koordinat langit → NPC jatuh. Fix: spawn di kamp bila psInShelter.

Ekonomi/kontainer (SEDANG):
- #7 Trade_Fits membaca inventory PENERIMA dengan mask PEMBERI (item salah total) + tanpa stack space; grant gagal hanya log → item hangus. Fix: signature (receiver, giver, mask), hitung stack space per itemid+durability + slot non-stackable; grant gagal → refund ke pemberi.
- #9a Peti party stacking tanpa maxStack. Fix: hormati Item_GetMaxStack (slot penuh → slot lain/free).

Race callback & bounds (RENDAH):
- #16 BaseBuild_OnInserted pending tunggal bisa tertimpa overlap. Fix: slot sebagai parameter "ii" + validasi ssActive/ssBaseId.
- #17 Garage_OnBought tanpa capture charId. Fix: "ii" + batal bila ActiveCharId berubah.
- #15 Stash body 2048 terpotong pada stash besar + CountUsed/SlotFromRow iterasi 120 padahal tampilan per kapasitas. Fix: body 4096 + iterasi Stash_Capacity.
- #13 Tx_Boot fopen("scriptfiles/mysql.ini") justru membuka scriptfiles/scriptfiles/... Fix: fopen("mysql.ini") (relatif scriptfiles) + komentar benar.
- #19 (gaya) CHARSEL "Keluar" = Kick: diputuskan SAH (pasca-login); bunyi invariant MEMORY 254 dipersempit.

Gate verifikasi ronde ini: sampctl build exit 0; boot ulang selftest 34/34 PASS 0 FAIL; errors.log kosong.

### 11d. Ronde fitur penutup audit (26-11-19 lanjut-5)

Item DITUNDA yang bisa dikerjakan tanpa playtest kini DIIMPLEMENTASI:

- D3 Suppressor SUNGGUHAN (sebelumnya item mati): ITEM_SUPPRESSOR=36 (durability = sisa tembakan), resep craft Scrap 8 + Electronics 2 + Blueprint Suppressor (MAX_RECIPES 22), craft memberi 40 tembakan (SUPPRESSOR_SHOTS). Selama dibawa di tas, tembakan pistol (22-24) memakai radius NOISE_SUPPRESSED 12 m (dari 45 m) dan menghabiskan 1 durability/tembakan; habis = item lenyap + pesan. Blueprint kembali jatuh di loot (bobot dipulihkan) + escort 25% + tier-3 bonus — sekarang bernilai karena fiturnya ada.
- D1/E2 Klinik kamp: /klinik (hanya di hub) = cure infeksi total + 20 HP, biaya $50 + $2×infeksi (cap $300) — sink uang berulang pertama yang benar-benar ada selain biaya medis.
- D2 Channel revive: /revivekan kini channel 6 detik (REVIVE_CHANNEL_MS) dicek di Player_Tick; batal bila penolong menjauh >3.5 m, terkena damage, downed/mati, atau target sudah tidak downed; bandage dikonsumsi di awal + refund saat batal. Solo bleed-out 25 detik (party tetap 45). Adrenaline self-revive tetap instan (safety net solo, terdokumentasi).
- E2/N4 Harga newbie (pass pertama, menunggu playtest): bandage $40→25, medkit $150→90, antibiotics $120→70 (sebelumnya 5-8× nilai craft = jebakan bangkrut sesi pertama).
- Selftest: cek baru "transaksi item" (Tx_IsReady) → total 35 cek; fallback pool diam-diam kini terlihat.
- Warning 208 baru akibat forward silang modul diselesaikan dengan forward paling awal di state.inc (Inventory_Grant/Consume, Account_IsPlaying, Selftest_Record).

Masih DITUNDA (butuh playtest/keputusan desain): rebalance powder & hierarki income penuh (E2/E3), pajak kamp, turned playable 90 detik (konstanta mati INFECTION_TURNED_MS), defib item terpisah, batas bulan UTC vs WIB (T15 — saat ini UTC, konsisten dengan EpochMs), konversi GetTickCount→epoch untuk jendela AC/loot (F-23, wrap 24.8 hari).

Gate verifikasi ronde ini: sampctl build exit 0 (1 warning pihak ketiga); boot selftest 35/35 PASS 0 FAIL; errors.log kosong.

### 11e. Ronde audit performa (26-11-19 lanjut-6)

Reviewer @performance-optimizer (selesai 2404 dtk; bukti: header AMX, [PERF] log, sumber plugin). Temuan & fix:

- F2 TINGGI (terverifikasi compiler): heap+stack default 16 KB (4096 cells) padahal estimasi pemakaian maks kode ~20.468 bytes (5117 cells) → collision diam-diam mungkin terjadi (array trim/snapshot trade/body dialog). Fix: `#pragma dynamic 262144` (CELLS = 1 MB). Laporan compiler kini: "Stack/heap size: 1048576 bytes; estimated max. usage=5117 cells (20468 bytes)".
- F1 SEDANG: Zombie_TrimToBudget O(Z²×P) — rescan semua zombie × semua pemain tiap satu pembuangan (jutaan operasi saat horde menyusut). Fix: satu pass jarak via Hash_NearestPlayer (spatial hash) + seleksi terjauh dari array lokal; kompleksitas O(Z×span_hash + Z²_ringan).
- F5 SEDANG: read-only berat (/top UNION ALL, /stats 3×GROUP BY) mengantre di FIFO write transaksi. Fix: mysql_pquery (pool paralel) untuk leaderboard & stats. Retensi telemetri: purge boot kill_log >90 hari + anticheat_flags >30 hari (sebelumnya tumbuh tanpa batas).
- F6 RENDAH: MAX_PLAYERS default a_samp 1000 padahal server 200 pemain + 200 NPC (id 0..399) → array global 41,4 MB & loop lambat. Fix: #define MAX_PLAYERS 400 sebelum a_samp → data global 19,5 MB (terverifikasi laporan compiler).
- Terverifikasi AMAN oleh reviewer: antrean query ~111 q/s pada 200 pemain (utilisasi ~5%, bukan bottleneck); cap objek streamer 920 cukup (worst ~391/klien, shelter unik per VW); memori global aman.
- Catatan reviewer: baseline [PERF] masih 0-2 pemain (dev) — angka beban 200 pemain tetap butuh load test nyata (docs/LOAD_TEST_PLAN.md).

Gate verifikasi ronde ini: sampctl build exit 0; laporan compiler stack/heap 1 MB + data 19,5 MB; boot selftest 35/35 PASS 0 FAIL; errors.log kosong.

### 11f. Ronde performa lanjutan F7-F16 (26-11-19 lanjut-7)

Laporan lengkap reviewer performa tiba (17 temuan); F1-F6 sudah ditutup ronde sebelumnya. Sisanya:

- F7 SEDANG: HUD_SetIfChanged & Zone_GetName pakai strcpy (bukan format("%s")) — hemat ~30-40% biaya salin string jalur panas; Vehicle_FuelStatus satu lookup slot (bukan 3 scan O(64) per pengemudi per tick).
- F8 SEDANG: WildVeh_SweepAbandoned filter lobby DULU sebelum cek driver/posisi (skip pemain lobby lain sepenuhnya).
- F10 SEDANG (sebagian): streamdistance pintu shelter 300→60 (konsisten dengan ruang). Pool/reuse objek ruang per VW = DITUNDA (churn ~1-2 ms/kunjungan masih aman; refactor besar).
- F12 SEDANG: Grave_PersistItems = DELETE + SATU multi-row INSERT (bukan 20 INSERT per ambil). Amplifikasi 21 q/ambil → 2 q/ambil; mass death 50 pemain ~1150 q → ~100 q.
- F13 SEDANG: Horde_AddHeat iterasi sel HashCellPlayers dalam radius (bukan scan 200 pemain per tembakan). 400 tembakan/dtk: 80 ribu native → hanya pemain di sel relevan.
- F14 RENDAH: logger SATU fwrite per baris (CRLF digabung ke buffer).
- F15 RENDAH: Companion_Tick foreach PlayerPool (bukan for 1000 slot).
- F16 RENDAH: profiler +4 slot (needs/event/horde/autosave) → [PERF] log & /diag kini menampilkan 11 sistem.
- F17 RENDAH: sync FCNPC 100 ms crowd DIPERTAHANKAN (saran reviewer: trim budget adalah tuas utama, sudah ada).
- F4/F5 (sisa): verifikasi runtime TX sudah (log "handle terpisah" + selftest "transaksi item" PASS); pquery leaderboard/stats sudah; retensi kill_log 90 hari + anticheat_flags 30 hari sudah. item_ledger TIDAK dipurge (append-only audit trail — keputusan sadar; agregat harian untuk leaderboard = fitur masa depan bila tabel membesar).
- DITUNDA (dampak kecil/refactor besar): F9 hash radius untuk spawn peti loot node, F11 kursor offset untuk Stash_Save/Inventory_Save (0,2-0,5 ms/save masih aman), F10 pool objek shelter.

### 11g. Ronde temuan tengah audit yang terlewat (26-11-19 lanjut-8)

Output reviewer security/database/gameplay terpotong di tengah saat tiba; file lengkap dibaca ulang dan SEMUA temuan tengah diproses:

KEAMANAN (F-12..F-21, F-23):
- F-12 kick timer "i" → sudah ditutup ronde code-reviewer.
- F-13 SEDANG: respons dialog stash/vendor/craft tanpa validasi ulang lokasi → akses kontainer dari zona bahaya. Fix: gate respons (playing + hub untuk stash/vendor; hub/Bench untuk craft); tutup dialog tetap diproses agar lock lepas.
- F-14 SEDANG: AC_PositionTick melewati DRIVER/PASSENGER → speedhack kendaraan lolos. Fix: sampel tetap dengan ambang kendaraan (75 m/s, teleport 120 m).
- F-15 SEDANG: rapid-fire hanya 22-34; noise/wear diproses sebelum cek. Fix: interval 35-38 (rocket 2500, flamethrower 120, minigun 80) + cek AC SEBELUM noise/wear.
- F-16 SEDANG: AC_Unfreeze membatalkan freeze admin. Fix: hormati AdminFrozen (deklarasi pindah ke admin_state.inc agar terlihat achecks).
- F-17 SEDANG: trade gagal fase 1 tanpa refund → grief item hangus. Fix: consumedMask + Trade_RefundMask mengembalikan semua yang sudah terconsume.
- F-18 SEDANG: alt tak terbatas = farm kit+tutorial. Fix: cap registrasi 3 akun/IP/24 jam (32 slot, fail-open berlog).
- F-19 RENDAH: God/Frozen bocor ke penghuni slot berikutnya bila disconnect pre-auth. Fix: Admin_ResetSession tanpa syarat di disconnect.
- F-20 RENDAH: lock makam keyed slot + tanpa cek jendela pemilik di Take + eksklusivitas hilang pasca-restart. Fix: lock keyed gvId + Lock_Invalidate saat clear; GraveSelectedId guard slot-reuse; owner_free_at epoch di DB (migrasi 021) bertahan restart; cek ulang di Show dan Take.
- F-21 RENDAH: specimen bisa masuk peti party via SelInv basi. Fix: cek ITEM_SPECIMEN di PartyStash_Put.
- F-23 RENDAH: GetTickCount wrap 24,8 hari di expire loot/grave/corpse/grace. Fix: perbandingan bentuk selisih (now - expire >= 0) wrap-safe; jendela AC sudah selisih.

DATABASE (T12/T13):
- T12: schema_migrations hanya mencatat 001. Fix: INSERT IGNORE di tiap file 002-021 + backfill live (21 versi tercatat).
- T13: kontrak hari lama menumpuk. Fix: purge boot day_id < today-7. Kolom/tabel mati (characters.stash_capacity, weapons.upgrades, vehicles, loot_nodes) DIDOKUMENTASIKAN, tidak di-drop (risiko data); owned_vehicles.id signed = catatan migrasi masa depan. item_ledger tetap append-only (audit trail).

GAMEPLAY/UX (D5/D6/D8/U1/U2/U3/N3):
- D5: noise pemain level <5 tidak menaikkan heat (hash trigger tetap) — proteksi newbie DESIGN §14.
- D6: diminishing XP tier-1 (>10 kill/5mnt -20%, >20 -40%) + cap level 50.
- D8: timeout downed menambah psDeaths (statistik kematian akurat).
- U1: pity specimen dijamin setelah 5 kill tanpa sampel; langkah SAMPLE tutorial menghitung specimen yang sudah di tas.
- U2: vendor "Jual SEMUA material" satu klik (satu transaksi); label loot "/take (Y di PC)" (mobile dulu).
- U3: peringatan INFEKSI KRITIS saat login ≥80 + pill THREAT [AMAN/WASPADA/BAHAYA] di HUD zona.
- N3: grace newbie 15 dtk → 10 menit (PvP + needs decay).
- D7: sinkronisasi doc (SURVIVAL specimen 14/45+pity, CORE_LOOP 4 kontrak, LOBBIES multiplier+VW shelter, DESIGN banner status implementasi + boss Abomination, BASE_BUILDING raid heat 30/cooldown 10mnt).

Gate verifikasi ronde ini: sampctl build exit 0; migrasi 021 live (owner_free_at) + 21 versi schema_migrations; boot selftest 35/35 PASS 0 FAIL; errors.log kosong.

Gate verifikasi ronde ini: sampctl build exit 0 (1 warning pihak ketiga); boot selftest 35/35 PASS 0 FAIL; errors.log kosong.

### 11h. Ronde studi ulang + bukti uji beban 100 pemain (26-11-19 lanjut-9)

Studi ulang source terbaru dijalankan dengan verifikasi nyata (bukan membaca dokumen):
`docs/CODEBASE_STUDY.md` §11a–11g sudah sesuai dengan kode saat ini — **tetapi §8 dan
README masih menyebut beberapa hal yang kini tidak benar**, dan satu bug fatal baru
ditemukan justru lewat uji beban.

Gate verifikasi ronde ini:
- `sampctl build` exit 0, 1 diagnostik pihak ketiga (colandreas.inc:603).
- Boot s1: `[MYSQL] tersambung 165 ms`, `[ADMIN] kunci admin dimuat dari admin_keys.cfg`,
  `[SELFTEST] 36/36 PASS, 0 FAIL` (36 = tambahan cek "anggaran slot NPC").

TEMUAN KRITIS (baru, terverifikasi crash):
- Uji beban 100 bot (`tools/loadtest/bot_run.py --count 100 --duration 60`) mematikan server
  dengan SIGABRT. Bukti di `runtime/s1/console.log`:
  `../raknet/DS_OrderedList.h:185: OrderedList<PlayerID,PlayerIDAndIndex>::Remove:
   Assertion 'objectExists==true' failed.` → `[log-core] fatal signal '6' (SIGABRT)`.
  Sebelum mati, log penuh `FCNPC] Error: NPC '...' not created. The maxplayers limit in
  server.cfg has been reached` + `[ERROR] [ZOMBIE] FCNPC_Create gagal`.
- **Root cause**: SA-MP memakai satu pool slot untuk pemain DAN NPC. Budget director memakai
  `MAX_ZOMBIES` (190) tanpa melihat `GetMaxPlayers()`; dengan 100 pemain nyata sisa slot
  tidak cukup, `FCNPC_Create` gagal setengah jadi, lalu RakNet abort saat slot dibersihkan.
  Ini juga penjelasan paling masuk akal untuk segfault FCNPC yang berulang sejak lama.

Perbaikan:
- Baru: `gamemodes/modules/core/npc_budget.inc` — `Npc_OccupiedSlots` (sensus slot lewat
  `GetPlayerPoolSize`, bukan loop MAX_PLAYERS), `Npc_SlotsFree`, `Npc_SlotAvailable`
  (`NPC_SLOT_RESERVE 8`), `Npc_SpawnRoom`, `Npc_BudgetWarn` (log throttle 10 dtk).
- `zombie_core.inc`: `Zombie_Create` menolak sebelum `FCNPC_Create` bila slot tidak tersedia.
- `spawn_director.inc`: `activeBudget` di-clamp `Zombie_GetActiveCount() + Npc_SpawnRoom()`.
- `companion.inc` (spawn + revive) dan `escort.inc`: guard yang sama; revive menolak SEBELUM
  perban dipakai (tidak ada refund yang dibutuhkan).
- `selftest.inc`: cek "anggaran slot NPC" (bukti di log boot).
- Kapasitas: `maxplayers 300` + `maxnpc 200` (`pawn.json` dan `scripts/instance.sh`), `MAX_PLAYERS 400`.

TEMUAN OPERASIONAL:
- `scriptfiles/admin_keys.cfg` dibuat setelah instans s1 lahir, sedangkan `link_shared` hanya
  dipanggil saat `init` → s1 boot dengan login kunci admin NONAKTIF. Fix: `link_shared`
  dipanggil ulang di `cmd_start` (idempoten), symlink s1/s2 dibuat; log kini
  `[ADMIN] kunci admin dimuat`.

BUKTI SESUDAH PERBAIKAN (angka nyata, bukan klaim):
- 100 bot, 60 dtk idle: **99/100 spawn** (1 connect_failed dari library bot), 0 kegagalan
  NPC pasca-restart, 0 assertion, 0 SIGABRT, server tetap hidup, RAM 1,1 GB.
- `[DIRECTOR] total aktif 90/190 | malam 0` dengan 100 pemain (budget menyusut otomatis sesuai
  slot bebas, sesuai desain).
- `[PERF]` tick saat 100 pemain: zombie 0–1 ms, director puncak 24 ms, horde puncak 12 ms,
  hud 1–3 ms.
- Churn `crash_probe.py 120 102 20` (20 worker connect-login-disconnect + `/event` paksa tiap
  2 dtk, 120 dtk): server hidup, 0 assertion.

Catatan jujur: p95 tick 60 menit soak dan target "100 pemain + 190 zombie" BELUM tercapai —
190 zombie tidak mungkin saat 100 pemain pada maxplayers 300 dengan cadangan; angka realistis
adalah ~90–150 zombie tergantung jumlah pemain. Target gate Fase 8 perlu dikoreksi menjadi
"100 pemain + zombie sesuai anggaran slot" atau maxplayers dinaikkan lagi (biaya RAM ~11 MB/pemain).
