# DESIGN — LAST STAND: San Andreas Quarantine

> **STATUS IMPLEMENTASI (26-11-19, pasca audit swarm):** angka di dokumen ini
> adalah TARGET DESAIN; sumber kebenaran runtime adalah `gamemodes/modules/config/`
> dan `docs/CODEBASE_STUDY.md` §11. Delta yang sudah diimplementasi:
> infeksi gigitan 60% (Bloater 100%) × 12-25 poin, infeksi RESET saat mati;
> klinik kamp `/klinik` (cure total $50+$2×infeksi, cap $300); biaya medis mati
> $100+$25/level cap $600; respawn pistol 12 peluru (60 hanya spawn pertama sesi)
> + senjata tersimpan kembali; grace newbie 10 menit (PvP + needs decay);
> noise pemain level <5 tidak menaikkan heat; revive channel 6 detik (batal:
> menjauh/damage), solo bleed-out 25 dtk vs party 45 dtk; suppressor NYATA
> (craft Scrap 8+Electronics 2+blueprint, 40 tembakan pistol senyap 12 m);
> cap level 50 + diminishing XP tier-1 (>10 kill/5mnt -20%, >20 -40%);
> registrasi maks 3 akun/IP/24 jam; raid base cooldown 10 menit + reward
> menuntut pemilik ≤40 m; ekstraksi cooldown per-pemain + per-lobby;
> specimen pity 5 kill; kontrak dokumen konsumsi fisik; scrap = ITEM_SCRAP.
> Belum diimplementasi (roadmap): turned playable 90 dtk, headshot deagle,
> skill Engineer/Runner, pajak kamp, airdrop 25-35 menit + blackout.

## 1. Posisi & Target Pemain

**Satu kalimat:** Tarkov-lite di San Andreas, tanpa login rumit — masuk, ambil senjata, masuk dead zone, keluar hidup-hidup.

- **Audiens primer:** pemain SA-MP Mobile Android 13–25 tahun, sesi 20–45 menit, suka progres tapi benci grinding 4 jam.
- **Audiens sekunder:** pemain PC yang ingin co-op party 4 orang dengan voice.
- **Bukan untuk:** pemain heavy RP (tidak ada faction government, tidak ada pajak RP) dan bukan deathmatch murni.

**Janji retensi:** 5 menit pertama pemain sudah membunuh zombie, mendapat loot, dan mengekstrak. Tidak ada tutorial 20 menit.

---

## 2. Empat Pilar Desain

1. **Noise adalah mata uang tersembunyi.** Setiap aksi punya biaya noise. Menembak = cepat tapi memanggil masalah. Melee = lambat tapi senyap. Ini yang membuat stealth jadi pilihan nyata, bukan roleplay kosong.
2. **Kematian harus berarti, tapi tidak menghukum.** Carrier loot hilang, stash di hub aman. Tidak ada wipe karakter. Gear bagus mempermudah, bukan syarat.
3. **Dunia bergerak tanpa pemain.** Siklus siang-malam, horde director, airdrop, blackout. Pemain masuk ke dunia yang sudah berjalan, bukan menunggu event admin.
4. **Setiap 20 menit ada keputusan sulit.** Loot lebih banyak di zona merah, tapi ekstraksi harus dilalui. Bawa obat atau bawa amunisi? Bawa scrap atau bawa shotgun?

---

## 3. Core Loop

```
[ HUB / SAFE ZONE ]
  stash · crafting bench · klinik · job board · pasar pemain
        │
        │ pilih zona + loadout (bobot 30 kg)
        ▼
[ DEAD ZONE ]  ── noise naik ──▶  threat naik  ──▶  horde mendekat
  loot node · zombie · party member down · blackout
        │
        ├── ekstraksi (checkpoint / helipad / kendaraan) ──▶ loot masuk stash, XP
        └── mati ──▶ carrier loot hilang · respawn di hub · biaya medis · XP tetap 70%
```

Durasi per run: **12–25 menit**. Tidak ada timer paksa; threat level yang memaksa keluar.

---

## 4. Peta & Zona

Memakai map GTA SA vanilla (nol custom map di MVP — mempercepat produksi dan tidak membebani client mobile).

| Zona | Area vanilla (verifikasi in-game) | Tier | Zombie dominan | Loot khas | PvP |
|---|---|---|---|---|---|
| **Quarantine Camp** | LS Airport ± (1685, -2333, 13) | Hub | – | – | Non-PvP |
| **Idlewood Residential** | ± (1900, -1200, 20) | 1 | Walker, Crawler | Food, water, scrap, pistol ammo | Off |
| **Ganton Projects** | ± (2200, -2000, 13) | 1 | Walker, Runner | Melee, bandage, scrap | Off |
| **Downtown LS** | ± (1300, -1700, 20) | 2 | Runner, Screamer | Rifle ammo, electronics, medicine | **On** |
| **LS Sewers** | ± (1350, -1600, -20) | 2 | Crawler, Bloater | Rare blueprint, chemical | Off |
| **Angel Pine Forest** | ± (-2100, -2400, 30) | 1 (resource) | Walker | Wood, herb, peluru hunting | Off |
| **Abandoned Airfield** | ± (400, 2450, 16) | 3 | Brute, Runner | Military crate, armor, rifle | **On** |
| **Bone County Quarry** | ± (-500, 1400, 20) | 3 (contested) | Semua + boss | Airdrop crate, rare mod | **On (full loot)** |

Aturan zona:
- Zona **Off-PvP**: damage antar pemain dimatikan total (bukan hanya "dilarang"). Party tetap bisa revive.
- Zona **On-PvP**: semua damage masuk, tapi ada **damage multiplier 0.75** di Downtown supaya bukan deathmatch liar.
- Zona **contested**: full damage + loot drop semua saat mati + ada bounty marker untuk pembunuh (skull 60 detik di map).
- Ekstraksi hanya dari titik tetap yang ditandai (3 helipad + 2 checkpoint kendaraan). Tidak bisa `/exit` di mana saja.

**Batasan teknis:** semua dead zone memakai dunia/interior default. Tidak ada mapping custom di fase 1–6. Custom object hanya untuk barricade base (maks 400 objek aktif).

---

## 5. Roster Zombie

Angka di bawah adalah **baseline tuning**, bukan final. Semua nilai ada di `config/zombies.inc` agar bisa di-tune tanpa compile ulang logika.

| Tipe | HP | Speed (m/s) | Damage | Perilaku | Loot drop | Peran taktis |
|---|---|---|---|---|---|---|
| **Walker** | 60 | 1.2 | 8 | Pursuit lambat, reaksi 800 ms | Scrap 40%, food 25% | Massal, sumber ekonomi |
| **Runner** | 45 | 4.6 | 6 | Sprint, hanya siang-malam campuran | Bandage 30% | Memaksa pemain tidak stagnan |
| **Crawler** | 30 | 0.8 | 12 | Diam sampai pemain < 6 m, muncul dari bawah mobil | Rare blueprint 5% | Jump scare terkontrol, rasa tidak aman |
| **Brute** | 350 | 1.6 | 35 | Stagger tahan, menghancurkan barricade | Armor plate 60% | Boss mini, butuh party |
| **Bloater** | 120 | 1.0 | 0 (explode 40 AoE) | Meledak, meninggalkan cloud acid 8 detik | Chemical 70% | Zonasi area, hukuman melee |
| **Screamer** | 80 | 2.0 | 4 | Berteriak → memanggil 6–10 Walker radius 80 m | Electronics 50% | Target prioritas, reward bagus |
| **Stalker** | 70 | 5.4 | 18 | Hanya malam, mendekat dari belakang, menghindari cahaya | Night-vision mod 20% | Membuat malam benar-benar berbahaya |

**Aturan spawn:** zombie tidak spawn di depan mata pemain (minimal 35 m di luar frustum) dan tidak spawn di dalam safe zone. Populasi per zona diatur director, bukan timer acak.

---

## 6. Senjata & Ekonomi Noise

**Noise radius** = radius dalam meter di mana zombie mendengar dan mulai investigasi.

| Kelas | Contoh | DPS kasar | Noise | Ammo | Catatan |
|---|---|---|---|---|---|
| Melee ringan | Crowbar, Machete | Rendah | 0 m | ∞ (durabilitas) | Senyap total, durability turun per hit |
| Melee berat | Fire Axe, Sledgehammer | Sedang | 2 m | ∞ (durabilitas) | Bisa merusak barricade musuh |
| Pistol | 9mm, Deagle | Sedang | 45 m | Langka | Deagle headshot = 1 hit Walker |
| SMG | MP5 | Sedang-tinggi | 60 m | Langka | Recoil, spread besar di mobile |
| Shotgun | Pump, Sawn-off | Tinggi dekat | 75 m | Langka | Cepat menarik Brute |
| Rifle | M4, AK | Tinggi | 90 m | Sangat langka | Hanya zona tier 3 |
| Suppressed | Pistol + suppressor (craft) | Sama | 12 m | Sama | Durabilitas suppressor 40 tembakan |

**Amunisi adalah sumber tekanan utama.** Pemain tidak bisa menembak tanpa berpikir. Setiap peluru punya nilai ekonomi nyata (dijual antar pemain).

**Durabilitas:** senjata melee punya durability 60–200 hit, firearm punya `jam` (tidak bisa rusak total, tapi akurasi turun di bawah 30% kondisi). Perbaikan di crafting bench memakai scrap.

---

## 7. Infeksi, Downed, dan Revive

Ini sistem pembeda utama. Bukan health bar kedua yang membosankan, tapi timer keputusan.

```
Gigitan zombie ──▶ Infection meter +12..25%
                     │
       ┌─────────────┼──────────────────┐
       ▼             ▼                  ▼
   < 40%       40–79%               80–99%
   Aman        Hallucination:        Kehilangan kontrol 3 detik,
               suara + blur          penglihatan merah, batuk
                                              │
                                            100% ▼
                                   TURNED: pemain jadi zombie 90 detik
                                   (kontrol terbatas, bisa menyerang pemain)
                                   lalu mati & respawn di hub
```

- **Antibiotics** menurunkan 25%. **Medkit** menurunkan 10% + heal. **Blood pack** menurunkan 40% tapi butuh waktu 8 detik dan noise 5 m.
- Infeksi **tidak turun sendiri** di dead zone. Di hub, klinik menyembuhkan penuh dengan biaya cash.
- **Downed state:** HP 0 di zona party = downed 45 detik (bukan langsung mati). Bisa di-revive dengan defibrillator (6 detik channel) atau bandage (12 detik). Kalau timer habis → mati normal.
- **Turned state** harus dramatized: skin zombie, HUD diganti merah, kamera goyang, dan pemain lain dengar suara. Ini momen cerita yang bikin clip.

---

## 8. Inventory, Loot, dan Bobot

Model **hybrid grid + weight**: 20 slot, maksimum 30 kg.

| Tier loot | Container | Contoh | Aturan |
|---|---|---|---|
| Common | Rumah, mobil rusak | Food, water, scrap, bandage | Respawn 6 menit |
| Uncommon | Toko, kantor polisi | Ammo, electronics, medkit | Respawn 15 menit |
| Rare | Militer, safe, lab | Rifle, armor, blueprint | Respawn 45 menit + RNG 60% |
| Airdrop | Contested zone | Military crate, rare mod | 3 menit setelah drop, lalu hancur |

- Loot node adalah **pickup per-player** (streamer player-specific), jadi tidak ada cheat ESP yang melihat semua loot di map.
- **Extraction inventory** terpisah dari stash. Mati = isi carrier hilang. Berhasil ekstrak = masuk stash (kapasitas 60 slot, bisa di-upgrade).
- Dupe prevention: semua transfer item berjalan dalam satu MySQL transaction dengan `SELECT ... FOR UPDATE` pada row inventory.

---

## 9. Crafting & Base Defense

**Crafting bench** (hub) memakai blueprint yang ditemukan di dead zone:

| Blueprint | Bahan | Efek |
|---|---|---|
| Suppressor | Scrap 8 + Electronics 2 | Noise pistol 45 → 12 m |
| Bandage | Cloth 2 | Heal 15 + stop bleeding |
| Antibiotics | Chemical 2 + Herb 3 | −25% infection |
| Barricade Kit | Wood 6 + Scrap 4 | Deploy barricade 1500 HP |
| Defibrillator | Electronics 4 + Battery 2 | Item revive party |
| Ammo reload | Scrap 3 + Powder 1 | +12 peluru pistol |

**Base defense (lite, bukan free-build).**
- Pemain memiliki 1 safehouse per karakter di hub, plus boleh memasang barricade di **dead zone shelter** yang ditentukan (5 lokasi).
- Maksimum **400 objek barricade aktif** di seluruh server (streamer limit & performa). Barricade punya HP, bisa diperbaiki dengan scrap, dan dihancurkan Brute dalam ~20 detik.
- **Horde Night** menyerang shelter yang barricade-nya paling banyak → insentif untuk mempertahankan, bukan menumpuk tanpa risiko.

---

## 10. Progresi

- **Field Level 1–50** dari XP: bunuh zombie, ekstrak loot, revive kawan, selesaikan kontrak.
- XP dari membunuh di safe zone = 0 (anti farming).
- **Diminishing return:** zombie yang sama di zona tier 1 memberi XP −20% per 10 kill dalam 5 menit.
- **5 cabang skill** (maks 3 poin per cabang per 10 level):

| Cabang | Fokus | Contoh node |
|---|---|---|
| **Scavenger** | Loot & bobot | +5 kg kapasitas, +10% rare loot, buka rare container lebih cepat |
| **Medic** | Support | Revive 6 → 4 detik, antibiotics +40% efektif, bisa buat bandage di lapangan |
| **Engineer** | Base & kendaraan | Barricade +50% HP, repair kendaraan tanpa kit, turret sederhana |
| **Marksman** | Presisi | Recoil −20%, headshot damage +15%, ammo craft +50% |
| **Runner** | Mobilitas | Stamina +30%, noise jalan −40%, panjat cepat |

**Quarantine Rank** (prestise, reset per season): Bronze → Silver → Gold → Obsidian, dari total ekstraksi sukses.

---

## 11. Ekonomi: Keran dan Bak

**Dua mata uang, sengaja dipisah supaya inflasi tidak langsung mematikan progression:**

- **Cash (LS$)** — untuk layanan hub (klinik, sewa kendaraan, upgrade stash, ammo dari pedagang resmi).
- **Scrap** — material crafting; hanya dari salvage dan loot.

| Keran (masuk) | Bak (keluar) |
|---|---|
| Ekstraksi loot | Biaya medis saat mati (naik sesuai level) |
| Kontrak harian | Ammo & repair |
| Penjualan antar pemain (pasar hub) | Sewa/pajak kendaraan & safehouse |
| Reward event horde | Upgrade stash/skill respec |
| | Kehilangan carrier loot saat mati |

Target: pemain rata-rata tidak pernah menumpuk > 150k cash dalam 7 hari tanpa membeli upgrade. Kalau terjadi, tuning biaya medis dan harga ammo, bukan menambah keran.

---

## 12. Event Otomatis (Horde Director)

Director berjalan tiap 5 detik, tidak butuh admin.

| Event | Trigger | Efek |
|---|---|---|
| **Horde Night** | Setiap malam ke-3 (siklus 18 menit) | Populasi zombie ×2.5, Screamer +30%, reward ×1.5 |
| **Airdrop** | Tiap 25–35 menit | Crate di contested zone, marker global, 3 menit waktu ambil |
| **Blackout** | 20% chance saat malam | Lampu hub mati, Stalker spawn, noise radius +25% |
| **Threat Surge** | Noise akumulatif zona > threshold | 1 gelombang 8–14 zombie dari arah terjauh pemain |
| **Named Boss** | Malam-only per lobby, cooldown 15 menit, peluang 40%/menit | "Abomination" (2400 HP, fase-2 adds); anchor wajib level ≥5 ATAU party ≥2 ATAU zona tier ≥2; reward $250+60XP+3 scrap ke killing blow pemain |
| **Supply Run** | Kontrak harian | Escort truk NPC dari hub ke outpost, gagal = loot hilang |

**Aturan penting:** director punya **cooldown global 3 menit** supaya server tidak pernah menumpuk tiga event dan membuat pemain baru mati terus di menit pertama. Pemain level < 5 punya proteksi: threat tidak naik dari noise mereka.

---

## 13. Arah UI/UX

Gaya: **Dark Slate Glass** (bukan neon, bukan gradient ungu). Semua teks `SetOutline(0)` + `SetProportional(1)`.

| Palet | Hex | Pakai |
|---|---|---|
| Surface | `0x0B0F19E6` | Kartu modal, HUD pill |
| Elevated | `0x1E293BF2` | Header, slot aktif |
| Text primary | `0xF8FAFCFF` | Angka & label utama |
| Text secondary | `0x94A3B8FF` | Unit, hint |
| Emerald | `0x10B981FF` | Health, sukses |
| Amber | `0xF59E0BFF` | Stamina, hunger |
| Coral | `0xEF4444FF` | HP rendah, infeksi tinggi |
| Bio (infeksi) | `0x84CC16FF` | Infection meter — sengaja beda dari HP |

**HUD (≤ 24 PlayerTextDraw):**
- Kiri bawah: Health, Armor, **Infection**, Stamina, Hunger/Thirst (pill tipis).
- Kanan atas: Threat level (3 bar) + clock + horde countdown.
- Kanan bawah: speedometer hanya saat mengemudi.
- Tengah bawah: prompt interaksi `[Y] Ambil Loot` + progress bar.

**Interaksi:** satu dispatcher `[Y]` berbasis `CreateDynamicSphere` (O(1), bukan loop). Semua aksi punya fallback command untuk mobile: `/loot`, `/revive`, `/extract`.

**HUD tidak pernah dirender di `OnPlayerUpdate`.** Dirty-flag + timer 250 ms.

**Layar mati:** bukan dialog "You died". Tampilkan ringkasan run: loot hilang, XP didapat, jarak tempuh, dan tombol `[Y] Respawn di Hub`.

---

## 14. Aturan Anti-Frustrasi (Non-negotiable)

1. Tidak ada zombie spawn < 35 m dari pemain yang sedang tidak bergerak.
2. Pemain baru dapat 10 menit "grace": threat tidak naik, zombie tidak menyerang lebih dari 3 sekaligus.
3. Downed 45 detik, bukan instant death, selama ada party.
4. Semua kehilangan loot saat mati dibatasi: **item quest & blueprint tidak hilang**.
5. Tidak ada mekanik yang memaksa login harian untuk mempertahankan progres (tidak ada decay base).
6. Ekstraksi selalu punya minimal 2 rute; kalau satu dikepung horde, ada jalur kedua.
