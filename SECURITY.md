# Keamanan

## Lingkup

Repo ini **sengaja** menyimpan artefak runtime lengkap, termasuk kredensial
development (`mysql.ini`, `scriptfiles/admin_keys.cfg`), API 32-bit, plugin, dan
data ColAndreas. Tujuannya agar siapa pun bisa menjalankan server persis seperti
yang diuji tanpa langkah tambahan.

Artinya: **semua kredensial di repo ini dianggap publik dan tidak aman.**

## Wajib dilakukan sebelum produksi

1. Ganti password MySQL (`laststand` dan root), lalu sesuaikan `mysql.ini`.
2. Rotasi seluruh isi `scriptfiles/admin_keys.cfg` (kunci `/admin`, `/superadmin`).
3. Set variabel lingkungan produksi, jangan pakai default `docker-compose.yml`:
   ```bash
   MYSQL_ROOT_PASSWORD='...' MYSQL_PASSWORD='...' docker compose up -d
   ```
4. Batasi firewall: hanya port game **7777/UDP** terbuka. MySQL hanya 127.0.0.1.
5. Set `rcon_password` unik di `server.cfg` (default repo hanya untuk lokal).
6. Pastikan backup (`scripts/backup.sh`) tidak ikut dipublikasikan di web root.

## Anti-cheat yang sudah ada

Validasi dilakukan di server: damage lewat `Damage_Apply` + raycast ColAndreas,
kecepatan/teleport diperiksa `anticheat`, inventaris memakai ledger
(`item_ledger`) + lock container, dan level admin hanya dibaca dari
`accounts.admin_level` di database.

## Melaporkan celah

Buka issue dengan label `security`, atau hubungi pemilik repo bila temuan bisa
dieksploitasi untuk menghapus data. Jangan sertakan data pemain asli.
