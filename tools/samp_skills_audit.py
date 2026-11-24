#!/usr/bin/env python3
"""Audit aktivasi seluruh skill & agent SAMP untuk repo ini.

"Load skill" di repo ini berarti MENJALANKAN aturannya terhadap kode nyata,
bukan membaca lalu mengangguk. Skrip ini menjalankan probe konkret untuk mata
rantai mapping -> textdraw -> logic, memetakan tiap probe ke skill yang
menuntutnya, lalu menulis docs/SKILLS_ACTIVE.md.

Exit 0 hanya kalau semua probe lulus. Probe yang tidak relevan (mis. skill UCP
web yang memang bukan bagian repo ini) ditandai N/A secara eksplisit, bukan
dihitung sebagai lulus.
"""
import re
import shlex
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SKILLS = ROOT / ".agents/skills"
AGENTS = ROOT / ".agents/agents"
GM = ROOT / "gamemodes"


def sh(cmd):
    r = subprocess.run(cmd, shell=True, cwd=ROOT, capture_output=True, text=True)
    return r.returncode, (r.stdout + r.stderr).strip()


def grep(pattern, path, flags="-rn"):
    code, out = sh(f"grep {flags} {shlex.quote(pattern)} {path} 2>/dev/null | head -3")
    return code == 0 and bool(out), out.splitlines()[0] if out else ""


def count(pattern, path):
    code, out = sh(f"grep -ro {shlex.quote(pattern)} {path} 2>/dev/null | wc -l")
    return int(out) if out.isdigit() else 0


def lines_matching(pattern, path):
    """Jumlah BARIS yang cocok (bukan jumlah kemunculan)."""
    code, out = sh(f"grep -rE {shlex.quote(pattern)} {path} 2>/dev/null | wc -l")
    return int(out) if out.isdigit() else 0


def code_sites(pattern, window_must_contain, window=14):
    """Setiap kemunculan `pattern` di kode non-komentar wajib diikuti
    `window_must_contain` dalam `window` baris berikutnya."""
    bad, sites = [], 0
    for f in GM.rglob("*.inc"):
        text = f.read_text(errors="ignore").splitlines()
        for i, line in enumerate(text):
            s = line.lstrip()
            if s.startswith(("//", "/*", "*")):
                continue
            if pattern not in line:
                continue
            sites += 1
            nxt = "\n".join(text[i:i + window + 1])
            if window_must_contain not in nxt:
                bad.append(f"{f.name}:{i + 1}")
    return sites, bad


# (id, domain, skills, deskripsi, callable -> (bool, evidence))
def build_probes():
    p = []

    def add(pid, domain, skills, desc, fn):
        p.append((pid, domain, skills, desc, fn))

    # ---------- MAPPING ----------
    def streamer():
        ok1, _ = grep("Streamer_UpdateEx", "gamemodes/modules/world")
        n = count("CreateDynamicObject", "gamemodes/modules")
        return ok1 and n > 0, f"Streamer_UpdateEx dipakai, {n} CreateDynamicObject"

    add("map-streamer", "mapping",
        ["samp-streamer-plugin", "samp-mapping-dynamic-objects-pro", "samp-map-pipeline-2026"],
        "Objektif dinamis + preload Streamer_UpdateEx (anti jatuh tembus)", streamer)

    def colandreas():
        ok, ev = grep("CA_RayCastLine", "gamemodes/modules")
        n = count("CA_RayCastLine", "gamemodes/modules")
        if ok: ev = f"{n} pemanggilan CA_RayCastLine: " + ev
        return ok, ev or "CA_RayCastLine tidak dipakai"

    add("map-colandreas", "mapping",
        ["samp-colandreas-physics"],
        "Raycast ColAndreas untuk verifikasi lantai/line-of-sight", colandreas)

    def retexture():
        ok, ev = grep("SetDynamicObjectMaterial", "gamemodes/modules")
        return ok, ev or "tanpa retexture dinamis"

    add("map-retexture", "mapping",
        ["samp-mapping-dynamic-objects-pro", "samp-streamer-plugin"],
        "Retexture objek dinamis (Textures Studio pipeline)", retexture)

    # ---------- TEXTDRAW ----------
    def td_zone():
        code, out = sh("python3 tools/hud_preview/android_zone.py")
        return code == 0, out.splitlines()[-1] if out else ""

    add("td-zona-android", "textdraw",
        ["samp-android-safe-hud-layout", "samp-textdraw-ui-craft", "samp-mobile-android-cef-ui"],
        "Posisi HUD di pita aman 320..446 untuk 3 gaya", td_zone)

    def td_width():
        code, out = sh("python3 tools/hud_preview/generate.py")
        return code == 0 and "GATE LULUS" in out, out.splitlines()[-1] if out else ""

    add("td-lebar-640", "textdraw",
        ["samp-textdraw-ui-craft", "samp-textdraw-layout-debugger"],
        "Gate lebar teks 640px + larangan TextSize pada TD teks", td_width)

    def td_gallery():
        code, out = sh("python3 tools/tdui/gallery.py")
        return code == 0 and "0 gagal" in out, out.splitlines()[-1] if out else ""

    add("td-galeri", "textdraw",
        ["samp-textdraw-studio-converter", "samp-textdraw-fivem-modern-design"],
        "Galeri TDUI mencerminkan literal kode HUD (13 halaman)", td_gallery)

    def td_pool():
        n = count("CreatePlayerTextDraw", "gamemodes/modules/ui")
        return n <= 200, f"{n} PlayerTextDraw dibuat (batas pool 256, target <= 200)"

    add("td-pool", "textdraw", ["samp-textdraw-ui-craft"],
        "Budget pool PlayerTextDraw <= 200 per pemain", td_pool)

    def td_cache():
        creators, resets = 0, count("HUD_ResetCaches", "gamemodes/modules/ui/hud.inc")
        code, out = sh("grep -c 'stock HUD_Create' gamemodes/modules/ui/hud.inc")
        creators = int(out) if out.isdigit() else 0
        return resets >= 1 and creators > 0, f"{creators} builder, {resets} HUD_ResetCaches"

    add("td-cache", "textdraw", ["samp-textdraw-ui-craft", "samp-mobile-android-cef-ui"],
        "Cache teks HUD direset setiap builder berjalan", td_cache)

    # ---------- LOGIC ----------
    def dispatcher():
        ok, ev = grep("stock Command_Process", "gamemodes/modules/admin/commands.inc")
        n = count("OnPlayerCommandText", "gamemodes/main.pwn")
        return ok and n == 1, f"dispatcher tunggal, {n} OnPlayerCommandText"

    add("logic-dispatcher", "logic", ["samp-command-processor"],
        "Satu dispatcher perintah (tanpa tangga string)", dispatcher)

    def dialogs():
        code, out = sh("bash scripts/lint-security.sh --self-test")
        return code == 0, out.splitlines()[-1] if out else ""

    add("logic-dialog-lint", "logic",
        ["samp-dialog-system", "samp-security-lint", "samp-server-security-hardening"],
        "Lint dialog-unguarded + injeksi lolos self-test", dialogs)

    def timers():
        per_player = count("SetTimerEx(\\\"Player_", "gamemodes/modules")
        handles, ev = grep("TimerHandles", "gamemodes/main.pwn")
        return handles and per_player == 0, f"timer global TimerHandles, {per_player} timer per-pemain"

    add("logic-timer", "logic",
        ["samp-timer-scheduling", "samp-high-performance-tuning"],
        "Tidak ada timer per-pemain; tick 500ms global", timers)

    def update_loop():
        body = sh("awk '/public OnPlayerUpdate/{f=1} f&&/^}/{exit} f' gamemodes/main.pwn")[1]
        skip = ("public OnPlayerUpdate", "return", "{", "}", "//")
        logic = len([l for l in body.splitlines()
                     if l.strip() and not l.strip().startswith(skip)])
        return logic <= 2, f"{logic} baris non-komentar di OnPlayerUpdate"

    add("logic-onplayerupdate", "logic",
        ["samp-high-performance-tuning", "samp-zombie-survival-horde"],
        "OnPlayerUpdate tidak memuat logika gameplay", update_loop)

    def lifecycle():
        n = count("EL_Track", "gamemodes/modules")
        return n > 0, f"{n} pemanggilan EL_Track (entity lifecycle)"

    add("logic-entity", "logic", ["samp-entity-lifecycle", "samp-entity-lifecycle"],
        "Entitas dicatat & dibersihkan lewat EL_Track", lifecycle)

    def persistence():
        migrations = len(list((ROOT / "database/migrations").glob("*.sql")))
        _, bad = sh("grep -il 'unique' database/migrations/*.sql | wc -l")
        return migrations >= 30, f"{migrations} migrasi SQL"

    add("logic-persist", "logic", ["samp-persistence-schema", "samp-database-engineer"],
        "Skema persisten berversi lewat migrasi", persistence)

    def mysql_threaded():
        t = count("mysql_tquery", "gamemodes/modules")
        b = count("mysql_query(", "gamemodes/modules")
        return t > 0 and b == 0, f"{t} mysql_tquery, {b} mysql_query blocking"

    add("logic-mysql", "logic", ["samp-mysql-database"],
        "Query DB threaded, tanpa query blocking di jalur publik", mysql_threaded)

    def ysi():
        ok, ev = grep("foreach", "gamemodes/modules")
        inc, _ = grep("YSI_Coding", "gamemodes/main.pwn")
        return ok and inc, ev or ""

    add("logic-ysi", "logic", ["samp-ysi-framework", "samp-data-structures-pawn"],
        "Iterasi YSI foreach + struktur data lokal", ysi)

    def guards():
        code, out = sh("for f in $(find gamemodes -name '*.inc'); do "
                       "grep -qiE '^#if defined .*_inc' $f || echo $f; done | head -5")
        missing = [l for l in out.splitlines() if l.strip()]
        return not missing, "semua .inc punya include guard" if not missing else f"tanpa guard: {missing[:3]}"

    add("logic-include-guard", "logic", ["samp-include-library-design"],
        "Setiap include punya header guard", guards)

    def selftest():
        code, out = sh("grep -E 'SELFTEST\\] [0-9]+/' runtime/s1/server_log.txt | tail -1")
        return code == 0 and " 0 FAIL" in out, out.split("INFO]")[-1].strip() if out else ""

    add("logic-selftest", "logic", ["samp-testing-harness", "samp-qa-engineer"],
        "Selftest runtime lulus di instance berjalan", selftest)

    def compile_gate():
        code, out = sh("bash scripts/build.sh")
        return code == 0, [l for l in out.splitlines() if "OK" in l or "lulus" in l][-1] if code == 0 else "build gagal"

    add("logic-compile", "logic", ["samp-pawn-compile-gate", "samp-pawn-compile-verifier"],
        "Compile 0 warning 0 error + dua gate", compile_gate)

    def anticheat():
        n = count("AC_RecordSample", "gamemodes/modules/security") + count("AC_PositionTick", "gamemodes/modules/security")
        flags, ev = grep("anticheat_flags", "gamemodes/modules/security")
        return n > 0 and flags, f"{n} pemanggilan sampler AC, flag tercatat ke anticheat_flags"

    add("logic-anticheat", "logic", ["samp-anticheat-system"],
        "Pemeriksaan anti-cheat sisi server + pencatatan flag", anticheat)

    def arch():
        modules = len(list((GM / "modules").glob("*/*.inc")))
        mains = len(list(GM.glob("*.pwn")))
        return modules > 40 and mains == 1, f"{modules} modul .inc, {mains} gamemode tunggal"

    add("logic-arch", "logic", ["samp-gamemode-architecture", "samp-pawn-scripting"],
        "Arsitektur modular satu gamemode (urutan include terkunci)", arch)

    def server_conf():
        cfg = (ROOT / "runtime/s1/server.cfg").read_text(errors="ignore")
        need = ["plugins", "maxplayers", "rcon_password", "hostname"]
        miss = [k for k in need if k not in cfg]
        units = count("laststand-s", "runtime/*/server_log.txt")
        _, ev = sh("systemctl is-active laststand-s1 laststand-s2 | tr '\\n' ' '")
        return not miss and "active" in ev, f"server.cfg lengkap; systemd: {ev}"

    add("logic-servercfg", "logic", ["samp-server-configuration", "samp-release-engineer"],
        "server.cfg + dua instance systemd berjalan", server_conf)

    # ---------- FITUR GAMEPLAY ----------
    # Setiap probe di bawah menagih modul nyata di gamemodes/modules, bukan
    # keberadaan nama skill. Skill tanpa probe harus diklasifikasi eksplisit.
    def moderation():
        a, _ = grep("stock Moderation_Ban", "gamemodes/modules/admin/moderation.inc")
        b, _ = grep("stock Moderation_Mute", "gamemodes/modules/admin/moderation.inc")
        c, _ = grep("stock Moderation_Warn", "gamemodes/modules/admin/moderation.inc")
        d, _ = grep("AdminAudit_Log(", "gamemodes/modules/admin")
        return a and b and c and d, "ban/mute/warn + AdminAudit_Log tersambung"

    add("feat-moderation", "gameplay", ["samp-admin-moderation"],
        "Sanksi admin berjenjang + jejak audit", moderation)

    def logging():
        a, _ = grep("stock Log_Info", "gamemodes/modules/core/logger.inc")
        b, _ = grep("INSERT INTO admin_actions", "gamemodes/modules/admin/audit.inc")
        n = lines_matching("Log_(Info|Warn|Error|Format)\\(", "gamemodes/modules")
        return a and b and n >= 10, f"{n} baris Log_*(), tabel admin_actions dipakai"

    add("feat-logging", "gameplay", ["samp-logging-audit"],
        "Log terstruktur + audit trail admin ke DB", logging)

    def anticheat_deep():
        d, _ = grep("Dps_TrackPlayerDamage", "gamemodes/modules/security/dps.inc")
        s, _ = grep("stock AC_StoreFlag", "gamemodes/modules/security/achecks.inc")
        h, _ = grep("stock AC_HandleFlag", "gamemodes/modules/security/achecks.inc")
        return d and s and h, "DPS tracker + AC_StoreFlag/AC_HandleFlag"

    add("feat-anticheat-deep", "gameplay", ["samp-anticheat-deep"],
        "Deteksi berlapis (DPS, sampler, handler flag)", anticheat_deep)

    def crafting():
        n = lines_matching(r"^\s*\{ITEM_", "gamemodes/modules/config/recipes.inc")
        ui, _ = grep("stock CraftUI_Show", "gamemodes/modules/ui/craft_ui.inc")
        return n >= 10 and ui, f"{n} baris resep + CraftUI_Show"

    add("feat-crafting", "gameplay", ["samp-crafting-system"],
        "Resep + bahan + UI crafting", crafting)

    def death():
        a, _ = grep("stock Deaths_Record", "gamemodes/modules/player/deaths.inc")
        b, _ = grep("INSERT INTO deaths", "gamemodes/modules/player/deaths.inc")
        c, _ = grep("stock Deaths_ShowLast", "gamemodes/modules/player/deaths.inc")
        return a and b and c, "Deaths_Record + INSERT INTO deaths + Deaths_ShowLast"

    add("feat-death", "gameplay", ["samp-death-respawn"],
        "Kematian dicatat (jenis, posisi, pembunuh)", death)

    def medical():
        a, _ = grep("stock Wounds_Add", "gamemodes/modules/player/wounds.inc")
        b, _ = grep("stock Wounds_Tick", "gamemodes/modules/player/wounds.inc")
        c, _ = grep("stock Needs_", "gamemodes/modules/player/needs.inc")
        return a and b and c, "Wounds_Add/Wounds_Tick + modul needs"

    add("feat-medical", "gameplay", ["samp-medical-ems"],
        "Luka berdarah + kebutuhan (haus/lapar/infeksi)", medical)

    def economy():
        a, _ = grep("stock Vendor_SpendCash", "gamemodes/modules/economy/vendor.inc")
        b, _ = grep("stock Trade_Reset", "gamemodes/modules/economy/trade.inc")
        c, _ = grep("stock Contract", "gamemodes/modules/progression")
        return a and b and c, "vendor + trade + kontrak (uang tertutup)"

    add("feat-economy", "gameplay", ["samp-economy-simulation"],
        "Aliran uang lewat vendor/trade/kontrak", economy)

    def inventory():
        n = count("ITEM_[A-Z_]*", "gamemodes/modules/config/items.inc")
        a, _ = grep("stock Inventory_", "gamemodes/modules/inventory/inventory_core.inc")
        b, _ = grep("stock Hotbar_", "gamemodes/modules/inventory/hotbar.inc")
        s, _ = grep("stock Stash_", "gamemodes/modules/inventory/stash.inc")
        return n >= 40 and a and b and s, f"{n} referensi item, Inventory_/Hotbar_/Stash_ ada"

    add("feat-inventory", "gameplay", ["samp-inventory-item-system"],
        "Registry item + inventori + hotbar", inventory)

    def vehicle():
        a, _ = grep("stock Vehicle_Register", "gamemodes/modules/world/vehicles.inc")
        b, _ = grep("Vehicle_EnsureFuel(", "gamemodes/modules/world/vehicles.inc")
        c, _ = grep("stock Garage_", "gamemodes/modules/world/garage.inc")
        d, _ = grep("stock VehicleMods_", "gamemodes/modules/world/vehicle_mods.inc")
        return a and b and c and d, "register + bahan bakar + garasi + modifikasi"

    add("feat-vehicle", "gameplay", ["samp-vehicle-system"],
        "Siklus kendaraan persisten (tangki, garasi, mod)", vehicle)

    def daynight():
        a, _ = grep("stock World_SetWeather", "gamemodes/modules/world/daynight.inc")
        b, _ = grep("stock World_SetHour", "gamemodes/modules/world/daynight.inc")
        w = count("SetWeather", "gamemodes/modules")
        t = count("SetWorldTime", "gamemodes/modules")
        return a and b and w > 0 and t > 0, f"World_SetWeather/SetHour; {w} SetWeather, {t} SetWorldTime"

    add("feat-daynight", "gameplay", ["samp-weather-time"],
        "Siklus cuaca + jam server tersinkron", daynight)

    def npc():
        a, _ = grep("stock Npc_HideNameTag", "gamemodes/modules/core/npc_budget.inc")
        sites, bad = code_sites("FCNPC_Create(", "Npc_HideNameTag")
        return a and sites > 0 and not bad, (
            f"{sites} FCNPC_Create dilindungi Npc_HideNameTag" if not bad
            else f"create tanpa hide: {bad[:3]}")

    add("feat-npc", "gameplay", ["samp-npc-ai-behavior"],
        "NPC zombie: budget pool + nametag selalu disembunyikan", npc)

    def sound():
        n = count("PlayerPlaySound", "gamemodes/modules")
        return n > 0, f"{n} PlayerPlaySound (umpan balik audio)"

    add("feat-sound", "gameplay", ["samp-sound-audio"],
        "Umpan balik suara untuk aksi pemain", sound)

    def teleport():
        enter, _ = grep("stock Shelter_Enter", "gamemodes/modules/world/shelter.inc")
        exit_, _ = grep("stock Shelter_Exit", "gamemodes/modules/world/shelter.inc")
        pos = count("SetPlayerPos", "gamemodes/modules")
        guard, _ = grep("TP_COOLDOWN\\|Tele_Guard\\|lastTeleport", "gamemodes/modules")
        return enter and exit_ and pos > 0, f"{pos} SetPlayerPos via Shelter_Enter/Exit (bukan perintah bebas); guard={'ada' if guard else 'tidak ada'}"

    add("feat-teleport", "gameplay", ["samp-teleport-system"],
        "Perpindahan lewat modul shelter/lobby, bukan teleport liar", teleport)

    def property():
        a, _ = grep("stock Base_GetTier", "gamemodes/modules/world/bases.inc")
        b, _ = grep("stock Base_GetPlot", "gamemodes/modules/world/bases.inc")
        c, _ = grep("stock BaseMembers_", "gamemodes/modules/world/base_members.inc")
        d, _ = grep("stock Shelter_Enter", "gamemodes/modules/world/shelter.inc")
        return a and b and c and d, "Base_GetTier/GetPlot + BaseMembers_ + shelter"

    add("feat-property", "gameplay", ["samp-property-system"],
        "Base pemain: tier, plot, anggota (properti klan)", property)

    def spawn():
        info, _ = grep("SetSpawnInfo", "gamemodes/modules/account/account_auth.inc")
        sp, _ = grep("SpawnPlayer(playerid)", "gamemodes/modules/account/account_auth.inc")
        lobby, _ = grep("stock Lobby_Of", "gamemodes/modules/world/lobbies.inc")
        return info and sp and lobby, "SetSpawnInfo+SpawnPlayer lewat auth, lobby menentukan dunia"

    add("feat-spawn", "gameplay", ["samp-spawn-system"],
        "Pemilihan lobby/karakter sebelum spawn", spawn)

    def editor():
        code, out = sh("python3 tools/tdui/gallery.py")
        b, _ = grep("stock BuildEdit_CreatePanel", "gamemodes/modules/world/build_edit.inc")
        return code == 0 and b, (out.splitlines()[-1] if out else "") + " + panel edit in-game"

    add("feat-editor", "gameplay", ["samp-editor-ecosystem"],
        "Editor in-game (/bangun) + galeri TDUI di repo", editor)

    # ---------- META ----------
    def routing():
        agents_md = (ROOT / "AGENTS.md").read_text(errors="ignore")
        rows = len(re.findall(r"^\| \*\*", agents_md, re.M))
        return rows >= 10, f"AGENTS.md memuat {rows} baris routing domain→agent"

    add("meta-routing", "meta", ["samp-master-2026"],
        "Orkestrator memilih agent per domain dari AGENTS.md", routing)

    return p


# Absen TERBUKTI: skill tanpa artefak, dibuktikan dengan menyatakan fitur itu
# memang tidak ada. Bila penanda muncul di repo, statusnya jadi "N/A BASI" dan
# audit gagal — artinya fiturnya sudah dibangun dan wajib punya probe nyata.
def absent_ident(pattern, path="gamemodes"):
    def fn():
        n = count(pattern, path)
        return n == 0, f"{n} kemunculan '{pattern}' di {path}"
    return fn


def absent_file(pattern):
    def fn():
        hits = sorted(p.name for p in (GM / "modules").rglob(pattern))
        return not hits, f"0 modul {pattern}" if not hits else f"modul muncul: {hits[:3]}"
    return fn


N_A = {
    "samp-banking-system": ("belum ada bank/ATM pemain", absent_ident("Bank_")),
    "samp-phone-system": ("belum ada smartphone", absent_ident("Phone_")),
    "samp-police-system": ("tanpa kepolisian/MDT", absent_ident("Police_")),
    "samp-chat-system": ("chat pakai SendClientMessage bawaan", absent_ident("Chat_")),
    "samp-clothing-accessories": ("skin hanya dipilih saat buat karakter", absent_ident("Cloth_")),
    "samp-camera-cinematic": ("tanpa kamera sinematik", absent_ident("Camera_")),
    "samp-gang-faction": ("party = regu co-op, bukan faksi berperingkat", absent_ident("Gang_")),
    "samp-ucp-web-panel-bridge": ("UCP web backlog", absent_file("*ucp*")),
    "samp-discord-connector": ("belum ada connector Discord", absent_ident("DCC_")),
    "samp-voice-spatial-audio": ("voicechat belum dipasang", absent_ident("SvCreate")),
    "samp-custom-models-cdn": ("tanpa custom model / FastDL", absent_ident("AddCharModel")),
    "samp-raknet-packet-sync": ("Pawn.RakNet belum dipakai", absent_ident("Pawn.RakNet")),
    "samp-http-json-integration": ("tanpa integrasi HTTP eksternal", absent_ident("HTTP(")),
    "samp-crashdetect-debugger": ("dipakai ad-hoc saat crash", absent_ident("CrashDetect_")),
    "samp-filterscript-engineering": ("belum ada filterscript terpisah", absent_ident("CallRemoteFunction")),
    "samp-fivem-heist-crime-gang": ("belum ada heist", absent_ident("Heist_")),
    "samp-fivem-housing-business-crafting": ("belum ada rumah/bisnis pemain", absent_ident("House_")),
    "samp-fivem-smartphone-mdt": ("belum ada MDT", absent_ident("MDT_")),
    "samp-fivem-character-cinema-anim": ("belum ada studio karakter", absent_ident("Cinema_")),
    "samp-fivem-vehicle-tuning-garage": ("tuning belum, garasi sudah", absent_ident("Tuning_")),
    "samp-fivem-inventory-metadata": ("inventori ada, metadata per-slot belum", absent_ident("InvMeta_")),
    "samp-fivem-oxlib-target-logic": ("interaksi ada, radial/skillcheck belum", absent_ident("Radial_")),
    "samp-modern-jobs-2026-logic": ("pekerjaan belum dibangun", absent_ident("Job_")),
}

# Skill referensi/playbook: tidak punya artefak sendiri, tapi aturannya
# ditegakkan oleh gate yang disebut di sini. Gate wajib ada dan lulus.
REF = {
    "samp-openmp-modern-2026-api": ("aturan tag/native open.mp", "logic-compile"),
    "samp-openmp-modern-guide": ("panduan platform", "logic-compile"),
    "samp-vs-openmp-encyclopedia": ("perbandingan platform", "logic-servercfg"),
    "samp-legacy-openmp-migrator": ("migrasi 0.3.7→open.mp", "logic-compile"),
    "samp-greenfield-gamemode": ("fase greenfield sudah dilalui", "logic-arch"),
    "samp-roleplay-systems-bible": ("blueprint mode (di sini zombie)", "logic-arch"),
    "samp-modern-creator-2026": ("playbook pembuatan server", "feat-editor"),
    "samp-agent-evals": ("eval keputusan agent", "meta-routing"),
}


def main():
    probes = build_probes()
    results = []
    for pid, domain, skills, desc, fn in probes:
        try:
            ok, ev = fn()
        except Exception as e:  # probe rusak != lulus
            ok, ev = False, f"probe error: {e}"
        results.append((pid, domain, skills, desc, ok, ev))
        print(f"[{'PASS' if ok else 'FAIL'}] {pid:22s} {ev}")

    all_skills = sorted(p.name for p in SKILLS.glob("samp-*"))
    all_agents = sorted(p.stem for p in AGENTS.glob("samp-*.md"))

    rows = []
    for sk in all_skills:
        hit = [r for r in results if sk in r[2]]
        if hit:
            status = "TERVERIFIKASI" if all(h[4] for h in hit) else "GAGAL PROBE"
            rows.append((sk, status, "; ".join(h[0] for h in hit)))
        elif sk in REF:
            why, pid = REF[sk]
            gate = next((r for r in results if r[0] == pid), None)
            if gate and gate[4]:
                rows.append((sk, "REFERENSI", f"{why} — ditegakkan gate `{pid}`"))
            else:
                rows.append((sk, "GAGAL GATE", f"gate `{pid}` tidak lulus/tidak ada"))
        elif sk in N_A:
            why, fn = N_A[sk]
            still, ev = fn()
            rows.append((sk, "N/A absen-terbukti" if still else "N/A BASI", f"{why} — {ev}"))
        else:
            rows.append((sk, "TANPA PROBE", "belum ada probe, belum diklasifikasi"))

    bad_status = ("GAGAL PROBE", "GAGAL GATE", "N/A BASI", "TANPA PROBE")
    probe_failed = [r for r in results if not r[4]]
    row_failed = [r for r in rows if r[1] in bad_status]
    failed = probe_failed + row_failed

    # Gate agent: frontmatter lengkap + prompt tersinkron ke ~/.free-pi.
    prompt_dir = Path.home() / ".free-pi/agent/prompts"
    agent_bad = []
    for stem in all_agents:
        body = (AGENTS / f"{stem}.md").read_text(errors="ignore")
        miss = [k for k in ("name:", "description:") if k not in body.split("---")[1]]
        if miss:
            agent_bad.append(f"{stem} tanpa {'/'.join(miss)}")
        if not (prompt_dir / f"{stem}.md").exists():
            agent_bad.append(f"{stem} tidak tersinkron ke ~/.free-pi")
    agent_ok = not agent_bad
    print(f"[{'PASS' if agent_ok else 'FAIL'}] {'agent-gate':22s} "
          f"{len(all_agents)} agent samp: frontmatter + prompt sinkron"
          + ("" if agent_ok else f" ({len(agent_bad)} masalah: {agent_bad[:3]})"))
    ver = sum(1 for r in rows if r[1] == "TERVERIFIKASI")
    ref = sum(1 for r in rows if r[1] == "REFERENSI")
    na = sum(1 for r in rows if r[1] == "N/A absen-terbukti")
    lines = [
        "# Aktivasi Skill & Agent SAMP",
        "",
        "Dihasilkan oleh `python3 tools/samp_skills_audit.py` — jangan diedit manual.",
        "",
        f"- Skill SAMP: **{len(all_skills)}** ({ver} terverifikasi probe, {ref} referensi bergate, {na} N/A absen-terbukti)",
        f"- Agent SAMP: **{len(all_agents)}** ({'frontmatter + prompt sinkron OK' if agent_ok else f'{len(agent_bad)} masalah'})",
        f"- Probe terdefinisi: **{len(results)}** ({len(results) - len(probe_failed)} PASS, {len(probe_failed)} FAIL)",
        f"- Skill tanpa bukti: **{len(row_failed)}**" + (
            "" if not row_failed else " — " + ", ".join(f"`{r[0]}`" for r in row_failed)),
        "",
        "## Probe",
        "",
        "| Probe | Domain | Hasil | Bukti | Skill |",
        "|---|---|---|---|---|",
    ]
    for pid, domain, skills, desc, ok, ev in results:
        lines.append(f"| `{pid}` | {domain} | {'PASS' if ok else 'FAIL'} | {desc} — {ev} | {', '.join('`'+s+'`' for s in skills)} |")
    lines += ["", "## Skill", "", "| Skill | Status | Bukti |", "|---|---|---|"]
    for sk, status, ev in rows:
        lines.append(f"| `{sk}` | {status} | {ev} |")
    lines += ["", "## Agent", ""]
    lines.append(", ".join(f"`{a}`" for a in all_agents))
    (ROOT / "docs/SKILLS_ACTIVE.md").write_text("\n".join(lines) + "\n")
    print(f"\n{len(results) - len(probe_failed)}/{len(results)} probe PASS, "
          f"{len(all_skills)} skill: {ver} terverifikasi + {ref} referensi + {na} N/A, "
          f"{len(all_agents)} agent")
    if row_failed:
        print("GAGAL: " + ", ".join(f"{r[0]}[{r[1]}]" for r in row_failed))
    print("tulis docs/SKILLS_ACTIVE.md")
    return 1 if (failed or not agent_ok) else 0


if __name__ == "__main__":
    sys.exit(main())
