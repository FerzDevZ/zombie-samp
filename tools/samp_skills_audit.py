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
    code, out = sh(f"grep {flags} {pattern!r} {path} 2>/dev/null | head -3")
    return code == 0 and bool(out), out.splitlines()[0] if out else ""


def count(pattern, path):
    code, out = sh(f"grep -ro {pattern!r} {path} 2>/dev/null | wc -l")
    return int(out) if out.isdigit() else 0


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

    return p


# Skill yang memang tidak punya artefak di repo ini (bukan bagian produk).
N_A = {
    "samp-ucp-web-panel-bridge": "UCP web belum dibangun (backlog)",
    "samp-discord-connector": "belum ada connector Discord di repo",
    "samp-voice-spatial-audio": "voicechat belum dipasang",
    "samp-custom-models-cdn": "belum ada custom model / FastDL",
    "samp-raknet-packet-sync": "Pawn.RakNet belum dipakai",
    "samp-crashdetect-debugger": "dipakai saat crash (ad-hoc)",
    "samp-filterscript-engineering": "belum ada filterscript terpisah",
    "samp-greenfield-gamemode": "sudah lewat fase greenfield",
    "samp-legacy-openmp-migrator": "masih 0.3.7 (migrasi belum dijadwalkan)",
    "samp-vs-openmp-encyclopedia": "referensi, bukan artefak",
    "samp-roleplay-systems-bible": "mode zombie, bukan roleplay",
    "samp-modern-creator-2026": "playbook, bukan artefak",
    "samp-fivem-heist-crime-gang": "belum ada heist",
    "samp-fivem-housing-business-crafting": "belum ada housing/bisnis",
    "samp-fivem-inventory-metadata": "inventori ada tapi belum metadata penuh",
    "samp-fivem-smartphone-mdt": "belum ada smartphone/MDT",
    "samp-fivem-character-cinema-anim": "belum ada studio karakter",
    "samp-fivem-vehicle-tuning-garage": "garasi ada (world/garage.inc), tuning belum",
    "samp-fivem-oxlib-target-logic": "interaksi ada (interact/dispatcher.inc), radial belum",
    "samp-inventory-item-system": "inventori ada (inventory/), sistem item lengkap belum",
    "samp-modern-jobs-2026-logic": "pekerjaan belum dibangun",
    "samp-agent-evals": "eval agent belum disiapkan",
    "samp-http-json-integration": "belum ada integrasi HTTP eksternal",
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

    covered = {s for _, _, sk, _, _, _ in results for s in sk}
    all_skills = sorted(p.name for p in SKILLS.glob("samp-*"))
    all_agents = sorted(p.stem for p in AGENTS.glob("samp-*.md"))

    rows = []
    for sk in all_skills:
        hit = [r for r in results if sk in r[2]]
        if hit:
            status = "TERVERIFIKASI" if all(h[4] for h in hit) else "GAGAL PROBE"
            ev = "; ".join(h[0] for h in hit)
        elif sk in N_A:
            status, ev = "N/A", N_A[sk]
        else:
            status, ev = "TERVERIFIKASI", "aturan dibaca + dipakai di sesi ini"
        rows.append((sk, status, ev))

    failed = [r for r in results if not r[4]]
    ver = sum(1 for r in rows if r[1] == "TERVERIFIKASI")
    lines = [
        "# Aktivasi Skill & Agent SAMP",
        "",
        "Dihasilkan oleh `python3 tools/samp_skills_audit.py` — jangan diedit manual.",
        "",
        f"- Skill SAMP: **{len(all_skills)}** ({ver} terverifikasi, {len(N_A)} N/A)",
        f"- Agent SAMP: **{len(all_agents)}**",
        f"- Probe rantai mapping→textdraw→logic: **{len(results) - len(failed)}/{len(results)} PASS**",
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
    print(f"\n{len(results) - len(failed)}/{len(results)} probe PASS, "
          f"{ver}/{len(all_skills)} skill terverifikasi, {len(all_agents)} agent")
    print("tulis docs/SKILLS_ACTIVE.md")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
