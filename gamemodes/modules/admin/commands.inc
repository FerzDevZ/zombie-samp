#if defined _ls_commands_included
    #endinput
#endif
#define _ls_commands_included

/* Plain dispatcher on purpose: YSI's y_commands pulls y_hooks, which rewrites the
   FCNPC callback macros and breaks server-authoritative zombie damage hooks. */

stock Command_Split(const cmdtext[], cmd[], cmdSize, params[], paramsSize)
{
    new index = 0;
    while (cmdtext[index] == ' ') index++;
    if (cmdtext[index] != '/') return 0;
    index++;

    new outIndex = 0;
    while (cmdtext[index] != '\0' && cmdtext[index] != ' ' && outIndex < cmdSize - 1)
    {
        cmd[outIndex++] = cmdtext[index++];
    }
    cmd[outIndex] = '\0';

    while (cmdtext[index] == ' ') index++;

    outIndex = 0;
    while (cmdtext[index] != '\0' && outIndex < paramsSize - 1)
    {
        params[outIndex++] = cmdtext[index++];
    }
    params[outIndex] = '\0';

    return (outIndex >= 0);
}


/* Ambil argumen ke-N dari params tanpa sscanf. Dipakai jalur auth karena
   sscanf di server ini memakai include 2.8.2 dengan plugin 2.13.8 dan
   memuntahkan peringatan mismatch setiap kali dipanggil. */
stock Command_Arg(const params[], argIndex, dest[], destSize)
{
    dest[0] = EOS;

    new index = 0, currentArg = 0, outIndex = 0;
    while (params[index] == ' ') index++;

    while (params[index] != EOS && currentArg <= argIndex)
    {
        if (params[index] == ' ')
        {
            currentArg++;
            while (params[index] == ' ') index++;
            continue;
        }

        if (currentArg == argIndex && outIndex < destSize - 1) dest[outIndex++] = params[index];
        index++;
    }

    dest[outIndex] = EOS;
    return outIndex > 0;
}

stock bool:IsNumericToken(const token[])
{
    if (token[0] == EOS) return false;
    for (new index = 0; token[index] != EOS; index++)
    {
        if (token[index] < '0' || token[index] > '9') return false;
    }
    return true;
}

/* Deklarasi maju: /setbbm memanggil gerbang DEV sebelum definisinya di bawah
   (tanpa ini compiler memaksa reparse ⇒ warning 208). */
forward bool:Command_DevAllowed(playerid);

stock Command_Process(playerid, const cmdtext[])
{
    new cmd[32], params[192];
    Command_Split(cmdtext, cmd, sizeof(cmd), params, sizeof(params));

    if (!strcmp(cmd, "help", true))
    {
        SendClientMessage(playerid, UI_ACCENT_INFO, "Utama: /menu /interact /take /geledah /kontrak /inv /skill /craft /stash /shop /shelter /zone");
        SendClientMessage(playerid, UI_TEXT_SECONDARY, "Sosial: /party (co-op) /invite /accept /leave /kick /p <pesan> /rekrut /teman /trade (barter) /radio <pesan> /revivekan (channel 6 dtk). Ekstraksi: /ekstrak (butuh 3 sampel). Klinik: /klinik di kamp (cure infeksi berbayar).");
        SendClientMessage(playerid, UI_TEXT_SECONDARY, "Kendaraan: /isi [liter|kan] /lockpick /mesin /kunci /perbaiki /sedot.");
        SendClientMessage(playerid, UI_TEXT_SECONDARY, "Base: /base /fix /generator. Generator hanya hidup malam dan menghabiskan BBM.");
        SendClientMessage(playerid, UI_TEXT_SECONDARY, "Misi: /misi (onboarding), /kontrak, /escort di luar kamp.");
        SendClientMessage(playerid, UI_TEXT_SECONDARY, "Admin: /veh /revive /zspawn /horde /event /diag /ac /selftest");
        SendClientMessage(playerid, UI_TEXT_SECONDARY, "Akun: /login /register. Hotspot pakai tombol Y; di mobile gunakan /menu.");
        SendClientMessage(playerid, UI_TEXT_SECONDARY,
            "Hotspot memakai tombol Y; di mobile gunakan /interact.");
        return 1;
    }

    if (!strcmp(cmd, "nama", true) || !strcmp(cmd, "charname", true))
    {
        new typedName[MAX_NAME_LENGTH + 1];
        if (!Command_Arg(params, 0, typedName, sizeof(typedName)))
        {
            SendClientMessage(playerid, UI_ACCENT_INFO, "[KARAKTER] Pakai: /nama Namamu  contoh: /nama Arif_Pratama");
            return 1;
        }

        CharCreate_SetNameFromCommand(playerid, typedName);
        return 1;
    }

    if (!strcmp(cmd, "pulihkan", true) || !strcmp(cmd, "recover", true))
    {
        new recoveryPassword[MAX_PASSWORD_LENGTH + 1], recoveryKey[33];
        if (!Command_Arg(params, 0, recoveryPassword, sizeof(recoveryPassword)))
        {
            SendClientMessage(playerid, UI_ACCENT_INFO, "[AKUN] Pakai: /pulihkan passwordbaru [kunci pemilik]");
            return 1;
        }

        Command_Arg(params, 1, recoveryKey, sizeof(recoveryKey));
        Account_RecoverPassword(playerid, recoveryPassword, recoveryKey);
        return 1;
    }

    if (!strcmp(cmd, "gantipass", true) || !strcmp(cmd, "passwd", true) || !strcmp(cmd, "gantipassword", true))
    {
        if (!Account_IsPlaying(playerid))
        {
            SendClientMessage(playerid, UI_ACCENT_WARN, "[AKUN] Login dulu sebelum mengganti password.");
            return 1;
        }

        new oldPassword[MAX_PASSWORD_LENGTH + 1], newPassword[MAX_PASSWORD_LENGTH + 1];
        if (!Command_Arg(params, 0, oldPassword, sizeof(oldPassword)) || !Command_Arg(params, 1, newPassword, sizeof(newPassword)))
        {
            SendClientMessage(playerid, UI_ACCENT_INFO, "[AKUN] Pakai: /gantipass passwordlama passwordbaru  (contoh: /gantipass passwordlama passwordbaru)");
            return 1;
        }

        Account_RequestPasswordChange(playerid, oldPassword, newPassword);
        return 1;
    }

    /* Fallback mobile: dialog bisa gagal tampil di klien tertentu, jadi daftar
       dan login juga bisa lewat chat. */
    if (!strcmp(cmd, "daftar", true) || !strcmp(cmd, "register", true))
    {
        if (Queue_BlockAuth(playerid)) return 1;

        if (AuthStage[playerid] != AUTH_REGISTER)
        {
            SendClientMessage(playerid, UI_ACCENT_WARN, "[AKUN] Kamu tidak sedang mendaftar. Kalau akun sudah ada, pakai /masuk password.");
            return 1;
        }

        new newPassword[MAX_PASSWORD_LENGTH + 1];
        /* Parser manual, bukan sscanf: include 2.8.2 vs plugin 2.13.8 mismatch
           membuat sscanf jalur auth tidak bisa dipercaya (invariant MEMORY). */
        if (!Command_Arg(params, 0, newPassword, sizeof(newPassword)) || !Account_IsValidPassword(newPassword))
        {
            SendClientMessage(playerid, UI_ACCENT_INFO, "[AKUN] Pakai: /daftar password  (6-40 karakter). Contoh: /daftar tahan2026");
            return 1;
        }

        SendClientMessage(playerid, UI_TEXT_SECONDARY, "[AKUN] Password diterima, akun sedang dibuat...");
        Account_MarkChatAuth(playerid);
        Account_RegisterPassword(playerid, newPassword);
        return 1;
    }

    if (!strcmp(cmd, "masuk", true) || !strcmp(cmd, "login", true))
    {
        if (Queue_BlockAuth(playerid)) return 1;

        if (AuthStage[playerid] != AUTH_LOGIN)
        {
            SendClientMessage(playerid, UI_ACCENT_INFO, "[AKUN] Tidak perlu login (akun belum terdaftar atau kamu sudah masuk).");
            return 1;
        }

        new knownPassword[MAX_PASSWORD_LENGTH + 1];
        if (!Command_Arg(params, 0, knownPassword, sizeof(knownPassword)))
        {
            SendClientMessage(playerid, UI_ACCENT_INFO, "[AKUN] Pakai: /masuk password. Contoh: /masuk tahan2026");
            return 1;
        }

        Account_MarkChatAuth(playerid);
        Account_SubmitLogin(playerid, knownPassword);
        return 1;
    }

    if (!strcmp(cmd, "akun", true) || !strcmp(cmd, "auth", true))
    {
        switch (AuthStage[playerid])
        {
            case AUTH_REGISTER: Account_ShowRegister(playerid);
            case AUTH_LOGIN: Account_ShowLogin(playerid);
            case AUTH_CHARSELECT: Account_LoadCharacters(playerid);
            default: SendClientMessage(playerid, UI_TEXT_SECONDARY, "[AKUN] Kamu sudah masuk. Pakai /profil untuk identitas karakter.");
        }
        return 1;
    }

    if (!strcmp(cmd, "interact", true))
    {
        if (!Interact_HandlePlayer(playerid))
        {
            SendClientMessage(playerid, UI_TEXT_MUTED, "[LAST STAND] Tidak ada hotspot dalam jangkauan.");
        }
        return 1;
    }

    if (!strcmp(cmd, "geledah", true))
    {
        if (Corpse_TryOpenNearest(playerid)) return 1;
        if (Grave_TryOpenNearest(playerid)) return 1;

        SendClientMessage(playerid, UI_TEXT_MUTED, "[MAYAT] Tidak ada mayat atau makam dalam 3 meter.");
        return 1;
    }

    if (!strcmp(cmd, "kontrak", true) || !strcmp(cmd, "contract", true))
    {
        Contracts_Show(playerid);
        return 1;
    }

    if (!strcmp(cmd, "kencing", true) || !strcmp(cmd, "pipis", true) || !strcmp(cmd, "pee", true))
    {
        Needs_Urinate(playerid);
        return 1;
    }

    if (!strcmp(cmd, "misi", true) || !strcmp(cmd, "tutorial", true))
    {
        Tutorial_ShowStatus(playerid);
        return 1;
    }

    if (!strcmp(cmd, "radio", true))
    {
        if (params[0] == EOS)
        {
            SendClientMessage(playerid, UI_ACCENT_WARN, "Pemakaian: /radio <pesan>. Butuh Radio di tas.");
            return 1;
        }
        if (Inventory_CountItem(playerid, ITEM_RADIO) < 1)
        {
            SendClientMessage(playerid, UI_ACCENT_WARN, "[RADIO] Kamu butuh Radio di tas. Bisa dicraft atau ditambang dari Screamer.");
            return 1;
        }
        if (strlen(params) > 96) params[96] = EOS;

        new senderName[MAX_PLAYER_NAME + 1], line[160];
        GetPlayerName(playerid, senderName, sizeof(senderName));
        format(line, sizeof(line), "[RADIO] %s: %s", senderName, params);

        new lobbyId = Lobby_Of(playerid);
        foreach (new target : PlayerPool)
        {
            if (!Account_IsPlaying(target)) continue;
            if (Lobby_Of(target) != lobbyId) continue;
            SendClientMessage(target, 0xF1C40FFF, line);
        }
        return 1;
    }

    if (!strcmp(cmd, "party", true))
    {
        Party_Show(playerid);
        return 1;
    }

    if (!strcmp(cmd, "rekrut", true) || !strcmp(cmd, "recruit", true))
    {
        Companion_Recruit(playerid);
        return 1;
    }

    if (!strcmp(cmd, "teman", true) || !strcmp(cmd, "kompanyon", true) || !strcmp(cmd, "rekrutan", true))
    {
        new arg[16];
        sscanf(params, "s[16]", arg);

        if (arg[0] == EOS) Companion_Show(playerid);
        else if (!strcmp(arg, "ikut", true) || !strcmp(arg, "follow", true))
        {
            CompMode[playerid] = COMPANION_MODE_FOLLOW;
            Companion_Spawn(playerid);
            SendClientMessage(playerid, UI_ACCENT_PRIMARY, "[REKRUTAN] Dia mengikuti kamu lagi.");
        }
        else if (!strcmp(arg, "tunggu", true) || !strcmp(arg, "jaga", true) || !strcmp(arg, "hold", true))
        {
            CompMode[playerid] = COMPANION_MODE_HOLD;
            Companion_Save(playerid);
            SendClientMessage(playerid, UI_ACCENT_PRIMARY, "[REKRUTAN] Dia berjaga di titik ini.");
        }
        else if (!strcmp(arg, "revive", true) || !strcmp(arg, "tolong", true))
        {
            Companion_Revive(playerid);
        }
        else if (!strcmp(arg, "upgrade", true) || !strcmp(arg, "tingkat", true))
        {
            Companion_Upgrade(playerid);
        }
        else if (!strcmp(arg, "buang", true) || !strcmp(arg, "pulang", true))
        {
            Companion_Dismiss(playerid);
        }
        else Companion_Show(playerid);
        return 1;
    }

    if (!strcmp(cmd, "invite", true) || !strcmp(cmd, "undang", true))
    {
        new arg[8];
        if (sscanf(params, "s[8]", arg) != 0 || arg[0] == EOS)
        {
            Party_InviteNearest(playerid);
            return 1;
        }

        new target = strval(arg);
        if (!IsPlayerConnected(target))
        {
            SendClientMessage(playerid, UI_ACCENT_WARN, "[PARTY] Playerid tidak tersambung.");
            return 1;
        }

        Party_Invite(playerid, target);
        return 1;
    }

    if (!strcmp(cmd, "accept", true) || !strcmp(cmd, "terima", true))
    {
        Party_Accept(playerid);
        return 1;
    }

    if (!strcmp(cmd, "leave", true) || !strcmp(cmd, "keluar", true))
    {
        if (!Party_InParty(playerid))
        {
            SendClientMessage(playerid, UI_ACCENT_WARN, "[PARTY] Kamu tidak sedang di party.");
            return 1;
        }

        Party_Leave(playerid, "keluar sendiri");
        return 1;
    }

    if (!strcmp(cmd, "kick", true) || !strcmp(cmd, "tendang", true))
    {
        new arg[8];
        if (sscanf(params, "s[8]", arg) != 0 || arg[0] == EOS)
        {
            SendClientMessage(playerid, UI_ACCENT_WARN, "Pemakaian: /kick [playerid]");
            return 1;
        }

        new target = strval(arg);
        if (!IsPlayerConnected(target))
        {
            SendClientMessage(playerid, UI_ACCENT_WARN, "[PARTY] Playerid tidak tersambung.");
            return 1;
        }

        Party_Kick(playerid, target);
        return 1;
    }

    if (!strcmp(cmd, "p", true) || !strcmp(cmd, "party-chat", true))
    {
        if (!Party_InParty(playerid))
        {
            SendClientMessage(playerid, UI_ACCENT_WARN, "[PARTY] Kamu tidak sedang di party. Ketik /party.");
            return 1;
        }

        if (params[0] == EOS)
        {
            SendClientMessage(playerid, UI_ACCENT_WARN, "Pemakaian: /p [pesan]");
            return 1;
        }

        if (strlen(params) > 96) params[96] = EOS;

        new senderName[MAX_PLAYER_NAME + 1], line[160];
        GetPlayerName(playerid, senderName, sizeof(senderName));
        format(line, sizeof(line), "[PARTY] %s: %s", senderName, params);

        Party_Notify(PartyLeader[playerid], line, 0x7FD1FFFF);
        return 1;
    }

    if (!strcmp(cmd, "ban", true) || !strcmp(cmd, "blokir", true))
    {
        if (!IsServerAdmin(playerid))
        {
            SendClientMessage(playerid, UI_ACCENT_DANGER, "[BAN] Khusus admin.");
            return 1;
        }

        new targetArg[8], minuteArg[8], answer[100];
        if (sscanf(params, "s[8]s[8]s[100]", targetArg, minuteArg, answer) < 2)
        {
            SendClientMessage(playerid, UI_ACCENT_WARN, "Pemakaian: /ban [playerid] [menit|0=permanen] [alasan]");
            return 1;
        }

        new target = strval(targetArg);
        Moderation_Ban(playerid, target, strval(minuteArg), answer);
        return 1;
    }

    if (!strcmp(cmd, "unban", true) || !strcmp(cmd, "buka", true))
    {
        if (!IsServerSuperAdmin(playerid))
        {
            SendClientMessage(playerid, UI_ACCENT_DANGER, "[UNBAN] Khusus superadmin.");
            return 1;
        }

        new username[32];
        if (sscanf(params, "s[32]", username) != 1 || username[0] == EOS)
        {
            SendClientMessage(playerid, UI_ACCENT_WARN, "Pemakaian: /unban [nama akun]");
            return 1;
        }

        Moderation_Unban(playerid, username);
        return 1;
    }

    if (!strcmp(cmd, "mute", true) || !strcmp(cmd, "bisukan", true))
    {
        if (!IsServerAdmin(playerid))
        {
            SendClientMessage(playerid, UI_ACCENT_WARN, "[MUTE] Hanya admin.");
            return 1;
        }

        new targetId, minutes, reason[80];
        if (sscanf(params, "uD(10)s[80]", targetId, minutes, reason) < 1)
        {
            SendClientMessage(playerid, UI_ACCENT_WARN, "Pemakaian: /mute [playerid] [menit] [alasan]");
            return 1;
        }
        if (!IsPlayerConnected(targetId))
        {
            SendClientMessage(playerid, UI_ACCENT_WARN, "[MUTE] Pemain tidak ada.");
            return 1;
        }

        Moderation_Mute(playerid, targetId, minutes, reason);
        return 1;
    }

    if (!strcmp(cmd, "unmute", true))
    {
        if (!IsServerAdmin(playerid))
        {
            SendClientMessage(playerid, UI_ACCENT_WARN, "[MUTE] Hanya admin.");
            return 1;
        }

        new targetId;
        if (sscanf(params, "u", targetId) != 1)
        {
            SendClientMessage(playerid, UI_ACCENT_WARN, "Pemakaian: /unmute [playerid]");
            return 1;
        }
        if (!Moderation_Unmute(playerid, targetId))
        {
            SendClientMessage(playerid, UI_ACCENT_WARN, "[MUTE] Pemain itu tidak sedang dibisukan.");
        }
        return 1;
    }

    if (!strcmp(cmd, "warn", true) || !strcmp(cmd, "peringatkan", true))
    {
        if (!IsServerAdmin(playerid))
        {
            SendClientMessage(playerid, UI_ACCENT_WARN, "[WARN] Hanya admin.");
            return 1;
        }

        new targetId, reason[96];
        if (sscanf(params, "us[96]", targetId, reason) < 1)
        {
            SendClientMessage(playerid, UI_ACCENT_WARN, "Pemakaian: /warn [playerid] [alasan]");
            return 1;
        }
        if (!IsPlayerConnected(targetId))
        {
            SendClientMessage(playerid, UI_ACCENT_WARN, "[WARN] Pemain tidak ada.");
            return 1;
        }

        Moderation_Warn(playerid, targetId, reason);
        return 1;
    }

    if (!strcmp(cmd, "adminlog", true) || !strcmp(cmd, "audit", true))
    {
        new limit = 15;
        if (sscanf(params, "d", limit) < 1) limit = 15;
        AdminAudit_ShowRecent(playerid, limit);
        return 1;
    }

    if (!strcmp(cmd, "bans", true))
    {
        Moderation_ShowBans(playerid);
        return 1;
    }

    if (!strcmp(cmd, "peti", true) || !strcmp(cmd, "partystash", true))
    {
        new arg[16];
        sscanf(params, "s[16]", arg);

        if (arg[0] == EOS || !strcmp(arg, "buka", true) || !strcmp(arg, "open", true))
        {
            PartyStash_Open(playerid);
        }
        else if (!strcmp(arg, "taruh", true) || !strcmp(arg, "simpan", true) || !strcmp(arg, "put", true))
        {
            PartyStash_ShowPut(playerid);
        }
        else
        {
            SendClientMessage(playerid, UI_TEXT_SECONDARY, "Peti party: /peti (buka) atau /peti taruh (simpan item).");
        }
        return 1;
    }

    if (!strcmp(cmd, "superadmin", true) || !strcmp(cmd, "sadmin", true))
    {
        new key[33];
        if (!Command_Arg(params, 0, key, sizeof(key)))
        {
            Admin_PanelMain(playerid);
            return 1;
        }

        Admin_TryKeyLogin(playerid, key);
        return 1;
    }

    if (!strcmp(cmd, "sa", true) || !strcmp(cmd, "adminpanel", true))
    {
        Admin_PanelMain(playerid);
        return 1;
    }

    if (!strcmp(cmd, "god", true) || !strcmp(cmd, "kebal", true))
    {
        new arg[8];
        sscanf(params, "s[8]", arg);
        Admin_GodToggle(playerid, Admin_TargetFromArg(playerid, arg));
        return 1;
    }

    if (!strcmp(cmd, "spawnz", true) || !strcmp(cmd, "zspawnz", true))
    {
        new typeArg[16], countArg[8];
        if (sscanf(params, "s[16]s[8]", typeArg, countArg) < 1)
        {
            SendClientMessage(playerid, UI_ACCENT_WARN,
                "Pemakaian: /spawnz [walker|runner|crawler|brute|bloater|screamer|stalker|boss] [jumlah]");
            return 1;
        }

        new E_ZOMBIE_TYPE:zombieType;
        if (!Admin_ZombieTypeFromArg(typeArg, zombieType))
        {
            SendClientMessage(playerid, UI_ACCENT_WARN, "[SA] Tipe tidak dikenal. Pilihan: walker runner crawler brute bloater screamer stalker boss.");
            return 1;
        }

        new count = (countArg[0] == EOS) ? 3 : strval(countArg);
        Admin_SpawnZombies(playerid, zombieType, count);
        return 1;
    }

    if (!strcmp(cmd, "zclear", true) || !strcmp(cmd, "zombieclear", true))
    {
        Admin_ClearZombies(playerid);
        return 1;
    }

    if (!strcmp(cmd, "itemlist", true))
    {
        new arg[8];
        sscanf(params, "s[8]", arg);
        Admin_ListItems(playerid, (arg[0] == EOS) ? 0 : strval(arg));
        return 1;
    }

    if (!strcmp(cmd, "giveitem", true) || !strcmp(cmd, "item", true))
    {
        new itemArg[24], amountArg[8];
        if (sscanf(params, "s[24]s[8]", itemArg, amountArg) < 1)
        {
            SendClientMessage(playerid, UI_ACCENT_WARN, "Pemakaian: /giveitem [item id|nama] [jumlah] — lihat /itemlist");
            return 1;
        }

        new itemid = Admin_ItemIdFromArg(itemArg);
        if (itemid == -1)
        {
            SendClientMessage(playerid, UI_ACCENT_WARN, "[SA] Item tidak ditemukan. Cek /itemlist.");
            return 1;
        }

        Admin_GiveItem(playerid, playerid, itemid, (amountArg[0] == EOS) ? 1 : strval(amountArg));
        return 1;
    }

    if (!strcmp(cmd, "give", true))
    {
        new targetArg[8], itemArg[24], amountArg[8];
        if (sscanf(params, "s[8]s[24]s[8]", targetArg, itemArg, amountArg) < 2)
        {
            SendClientMessage(playerid, UI_ACCENT_WARN, "Pemakaian: /give [playerid] [item id|nama] [jumlah]");
            return 1;
        }

        new target = Admin_TargetFromArg(playerid, targetArg);
        new itemid = Admin_ItemIdFromArg(itemArg);
        if (itemid == -1)
        {
            SendClientMessage(playerid, UI_ACCENT_WARN, "[SA] Item tidak ditemukan. Cek /itemlist.");
            return 1;
        }

        Admin_GiveItem(playerid, target, itemid, (amountArg[0] == EOS) ? 1 : strval(amountArg));
        return 1;
    }

    if (!strcmp(cmd, "setcash", true))
    {
        new targetArg[8], valueArg[12];
        if (sscanf(params, "s[8]s[12]", targetArg, valueArg) != 2)
        {
            SendClientMessage(playerid, UI_ACCENT_WARN, "Pemakaian: /setcash [playerid] [jumlah]");
            return 1;
        }
        Admin_SetCash(playerid, Admin_TargetFromArg(playerid, targetArg), strval(valueArg));
        return 1;
    }

    if (!strcmp(cmd, "setxp", true) || !strcmp(cmd, "addxp", true))
    {
        new targetArg[8], valueArg[12];
        if (sscanf(params, "s[8]s[12]", targetArg, valueArg) != 2)
        {
            SendClientMessage(playerid, UI_ACCENT_WARN, "Pemakaian: /setxp [playerid] [jumlah]");
            return 1;
        }
        Admin_AddXp(playerid, Admin_TargetFromArg(playerid, targetArg), strval(valueArg));
        return 1;
    }

    if (!strcmp(cmd, "setskill", true) || !strcmp(cmd, "setsp", true))
    {
        new targetArg[8], valueArg[8];
        if (sscanf(params, "s[8]s[8]", targetArg, valueArg) != 2)
        {
            SendClientMessage(playerid, UI_ACCENT_WARN, "Pemakaian: /setskill [playerid] [poin]");
            return 1;
        }
        Admin_SetSkillPoints(playerid, Admin_TargetFromArg(playerid, targetArg), strval(valueArg));
        return 1;
    }

    if (!strcmp(cmd, "max", true) || !strcmp(cmd, "fullheal", true) || !strcmp(cmd, "restore", true))
    {
        new arg[8];
        sscanf(params, "s[8]", arg);
        Admin_FullRestore(playerid, Admin_TargetFromArg(playerid, arg));
        return 1;
    }

    if (!strcmp(cmd, "tppos", true))
    {
        new Float:targetX, Float:targetY, Float:targetZ;
        if (sscanf(params, "fff", targetX, targetY, targetZ) != 3)
        {
            SendClientMessage(playerid, UI_ACCENT_WARN, "Pemakaian: /tppos [x] [y] [z]");
            return 1;
        }
        Admin_Teleport(playerid, playerid, targetX, targetY, targetZ);
        return 1;
    }

    if (!strcmp(cmd, "goto", true))
    {
        new arg[8];
        if (sscanf(params, "s[8]", arg) != 1)
        {
            SendClientMessage(playerid, UI_ACCENT_WARN, "Pemakaian: /goto [playerid]");
            return 1;
        }
        Admin_TeleportToPlayer(playerid, Admin_TargetFromArg(playerid, arg));
        return 1;
    }

    if (!strcmp(cmd, "gethere", true) || !strcmp(cmd, "tarik", true))
    {
        new arg[8];
        if (sscanf(params, "s[8]", arg) != 1)
        {
            SendClientMessage(playerid, UI_ACCENT_WARN, "Pemakaian: /gethere [playerid]");
            return 1;
        }
        Admin_BringPlayer(playerid, Admin_TargetFromArg(playerid, arg));
        return 1;
    }

    if (!strcmp(cmd, "freeze", true) || !strcmp(cmd, "bekukan", true))
    {
        new arg[8];
        if (sscanf(params, "s[8]", arg) != 1)
        {
            SendClientMessage(playerid, UI_ACCENT_WARN, "Pemakaian: /freeze [playerid]");
            return 1;
        }
        Admin_FreezeToggle(playerid, Admin_TargetFromArg(playerid, arg));
        return 1;
    }

    if (!strcmp(cmd, "sethour", true) || !strcmp(cmd, "jam", true))
    {
        new arg[8];
        if (sscanf(params, "s[8]", arg) != 1)
        {
            SendClientMessage(playerid, UI_ACCENT_WARN, "Pemakaian: /sethour [0-23]");
            return 1;
        }
        Admin_SetHour(playerid, strval(arg));
        return 1;
    }

    if (!strcmp(cmd, "weather", true) || !strcmp(cmd, "cuaca", true))
    {
        if (!IsServerAdmin(playerid))
        {
            SendClientMessage(playerid, UI_ACCENT_WARN, "[CUACA] Hanya admin.");
            return 1;
        }

        new arg[8];
        if (sscanf(params, "s[8]", arg) != 1)
        {
            SendClientMessage(playerid, UI_ACCENT_WARN, "Pemakaian: /weather [0-20] atau /weather auto");
            return 1;
        }

        if (!strcmp(arg, "auto", true))
        {
            World_SetWeather(-1);
            SendClientMessage(playerid, UI_ACCENT_PRIMARY, "[CUACA] Kembali ke siklus otomatis (siang cerah, malam badai).");
            Log_Format(LOG_INFO, "[CUACA] auto oleh account=%d", AccountId[playerid]);
            return 1;
        }

        new weatherId = strval(arg);
        if (!World_SetWeather(weatherId))
        {
            SendClientMessage(playerid, UI_ACCENT_WARN, "[CUACA] Id tidak valid. Rentang 0-20.");
            return 1;
        }

        new line[96];
        format(line, sizeof(line), "[CUACA] Cuaca di-set ke id %d (tetap sampai /weather auto).", weatherId);
        foreach (new target : PlayerPool) SendClientMessage(target, UI_TEXT_MUTED, line);
        Log_Format(LOG_INFO, "[CUACA] id=%d oleh account=%d", weatherId, AccountId[playerid]);
        return 1;
    }

    if (!strcmp(cmd, "setfuel", true))
    {
        if (!IsServerAdmin(playerid))
        {
            SendClientMessage(playerid, UI_ACCENT_WARN, "[BBM] Hanya admin.");
            return 1;
        }
        if (GetPlayerState(playerid) != PLAYER_STATE_DRIVER)
        {
            SendClientMessage(playerid, UI_ACCENT_WARN, "[BBM] Kamu harus jadi pengemudi.");
            return 1;
        }

        new liters;
        if (sscanf(params, "d", liters) != 1 || liters < 0)
        {
            SendClientMessage(playerid, UI_ACCENT_WARN, "Pemakaian: /setfuel [liter]");
            return 1;
        }

        new vehicleid = GetPlayerVehicleID(playerid);
        if (vehicleid == 0)
        {
            SendClientMessage(playerid, UI_ACCENT_WARN, "[BBM] Kendaraan tidak terbaca.");
            return 1;
        }

        Vehicle_SetFuel(vehicleid, float(liters));

        new line[112];
        format(line, sizeof(line), "[BBM] Tangki kendaraan %d di-set ke %d liter.", vehicleid, liters);
        SendClientMessage(playerid, UI_ACCENT_PRIMARY, line);
        Log_Format(LOG_INFO, "[BBM] setfuel veh=%d liter=%d oleh account=%d",
            vehicleid, liters, AccountId[playerid]);
        return 1;
    }

    if (!strcmp(cmd, "top", true) || !strcmp(cmd, "peringkat", true) || !strcmp(cmd, "leaderboard", true))
    {
        Leaderboard_Show(playerid);
        return 1;
    }

    if (!strcmp(cmd, "admin", true) || !strcmp(cmd, "adminhelp", true))
    {
        new key[33];
        if (Command_Arg(params, 0, key, sizeof(key)))
        {
            Admin_TryKeyLogin(playerid, key);
            return 1;
        }

        Admin_Show(playerid);
        return 1;
    }

    if (!strcmp(cmd, "setadmin", true))
    {
        if (!IsServerSuperAdmin(playerid))
        {
            SendClientMessage(playerid, UI_ACCENT_DANGER, "[ADMIN] Khusus superadmin.");
            return 1;
        }

        new targetArg[8], levelArg[8];
        if (sscanf(params, "s[8]s[8]", targetArg, levelArg) != 2)
        {
            SendClientMessage(playerid, UI_ACCENT_WARN, "Pemakaian: /setadmin [playerid] [0=pemain 1=admin 2=superadmin]");
            return 1;
        }

        new target = strval(targetArg);
        if (!IsPlayerConnected(target) || !Account_IsPlaying(target))
        {
            SendClientMessage(playerid, UI_ACCENT_WARN, "[ADMIN] Pemain tidak tersambung atau belum login.");
            return 1;
        }

        Admin_SetLevel(playerid, target, strval(levelArg));
        return 1;
    }

    if (!strcmp(cmd, "trade", true))
    {
        if (Trade_InSession(playerid))
        {
            Trade_OpenPanel(playerid);
            return 1;
        }

        new target = INVALID_PLAYER_ID;
        new arg[8];
        if (sscanf(params, "s[8]", arg) == 0 && arg[0] != EOS)
        {
            target = strval(arg);
            if (!IsPlayerConnected(target))
            {
                SendClientMessage(playerid, UI_ACCENT_WARN, "[TRADE] Playerid tidak tersambung.");
                return 1;
            }
        }
        else
        {
            new Float:px, Float:py, Float:pz, Float:bestDistance = TRADE_RANGE;
            GetPlayerPos(playerid, px, py, pz);

            foreach (new candidate : PlayerPool)
            {
                if (candidate == playerid) continue;
                if (!Account_IsPlaying(candidate)) continue;
                if (Lobby_Of(candidate) != Lobby_Of(playerid)) continue;

                new Float:cx, Float:cy, Float:cz;
                GetPlayerPos(candidate, cx, cy, cz);
                new Float:distance = VectorSize(cx - px, cy - py, cz - pz);
                if (distance > bestDistance) continue;

                bestDistance = distance;
                target = candidate;
            }

            if (target == INVALID_PLAYER_ID)
            {
                SendClientMessage(playerid, UI_ACCENT_WARN, "Pemakaian: /trade [playerid] atau dekati pemain lalu /trade.");
                return 1;
            }
        }

        Trade_Start(playerid, target);
        return 1;
    }

    if (!strcmp(cmd, "revivekan", true) || !strcmp(cmd, "bangkitkan", true))
    {
        new Float:px, Float:py, Float:pz;
        GetPlayerPos(playerid, px, py, pz);

        new bestTarget = INVALID_PLAYER_ID;
        new Float:bestDistance = 3.5;

        foreach (new target : PlayerPool)
        {
            if (target == playerid) continue;
            if (!Account_IsPlaying(target)) continue;
            if (!Player_IsDowned(target)) continue;
            if (Lobby_Of(target) != Lobby_Of(playerid)) continue;

            new Float:tx, Float:ty, Float:tz;
            GetPlayerPos(target, tx, ty, tz);
            new Float:distance = VectorSize(tx - px, ty - py, tz - pz);
            if (distance > bestDistance) continue;

            bestDistance = distance;
            bestTarget = target;
        }

        if (bestTarget == INVALID_PLAYER_ID)
        {
            SendClientMessage(playerid, UI_TEXT_MUTED, "[REVIVE] Tidak ada pemain downed dalam 3,5 meter.");
            return 1;
        }
        if (ReviveTarget[playerid] != INVALID_PLAYER_ID)
        {
            SendClientMessage(playerid, UI_TEXT_MUTED, "[REVIVE] Kamu sedang menolong orang lain. Tunggu selesai.");
            return 1;
        }
        if (Inventory_CountItem(playerid, ITEM_BANDAGE) < 1)
        {
            SendClientMessage(playerid, UI_ACCENT_WARN, "[REVIVE] Butuh 1 Bandage untuk menolong temanmu.");
            return 1;
        }

        /* Channel 6 detik (bukan instan — audit D2): berdiri dekat target,
           jangan bergerak/terkena damage. Bandage dikonsumsi di awal dan
           dikembalikan bila channel batal. */
        Inventory_Consume(playerid, ITEM_BANDAGE, 1, "revive_player");
        ReviveTarget[playerid] = bestTarget;
        ReviveStartTick[playerid] = GetTickCount();
        ReviveStartHealth[playerid] = PlayerSurvival[playerid][psHealth];

        new helperName[MAX_PLAYER_NAME + 1], line[128];
        GetPlayerName(playerid, helperName, sizeof(helperName));
        format(line, sizeof(line), "[REVIVE] %s sedang menolongmu (6 detik). Jangan bergerak!", helperName);
        SendClientMessage(bestTarget, UI_ACCENT_INFO, line);
        SendClientMessage(playerid, UI_ACCENT_INFO, "[REVIVE] Menolong... tetap dekat selama 6 detik. Damage/menjauh = batal.");
        return 1;
    }

    /* Klinik kamp (audit D1/E2): cure infeksi berbayar = sink uang berulang
       yang sebelumnya tidak ada; jalur cure jelas selain antibiotics. */
    if (!strcmp(cmd, "klinik", true))
    {
        if (!Account_IsPlaying(playerid))
        {
            SendClientMessage(playerid, UI_ACCENT_WARN, "[KLINIK] Masuk dulu ke karakter.");
            return 1;
        }
        if (!Zone_IsHub(Player_GetZone(playerid)))
        {
            SendClientMessage(playerid, UI_ACCENT_WARN, "[KLINIK] Klinik hanya ada di kamp. Kembali ke zona aman.");
            return 1;
        }

        new cost = 50 + floatround(PlayerSurvival[playerid][psInfection] * 2.0);
        if (cost > 300) cost = 300;
        if (PlayerSurvival[playerid][psCash] < cost)
        {
            new line[112];
            format(line, sizeof(line), "[KLINIK] Biaya cure $%d. Uangmu kurang.", cost);
            SendClientMessage(playerid, UI_ACCENT_WARN, line);
            return 1;
        }

        PlayerSurvival[playerid][psCash] -= cost;
        PlayerSurvival[playerid][psInfection] = 0.0;
        PlayerSurvival[playerid][psInfectionStage] = 0;
        Player_Heal(playerid, 20.0);
        Account_SaveCharacter(playerid);

        new line[128];
        format(line, sizeof(line), "[KLINIK] Infeksi bersih total. -$%d, +20 HP.", cost);
        SendClientMessage(playerid, UI_ACCENT_PRIMARY, line);
        Toast_Show(playerid, "KLINIK", "Infeksi bersih. Tetap hati-hati di luar.", UI_ACCENT_PRIMARY);
        Log_Format(LOG_INFO, "[KLINIK] char=%d cure biaya=%d", ActiveCharId[playerid], cost);
        return 1;
    }

    if (!strcmp(cmd, "garasi", true) || !strcmp(cmd, "garage", true))
    {
        Garage_Show(playerid);
        return 1;
    }

    if (!strcmp(cmd, "simpan", true) || !strcmp(cmd, "store", true))
    {
        Garage_StoreNearest(playerid);
        return 1;
    }

    if (!strcmp(cmd, "isi", true) || !strcmp(cmd, "refuel", true) || !strcmp(cmd, "bensin", true))
    {
        /* strcmp bawaan SA-MP mengembalikan 0 untuk string pertama KOSONG saat
           ignorecase=true — jadi /isi tanpa argumen dianggap "/isi kan".
           isnull() wajib jadi penjaga sebelum membandingkan apa pun. */
        if (!isnull(params) && (!strcmp(params, "kan", true) || !strcmp(params, "jerrycan", true)))
        {
            Garage_RefillJerrycan(playerid);
            return 1;
        }

        /* Tanpa angka: di pom buka menu, di luar pom tampilkan status BBM.
           Pemain tidak perlu menghitung liter sendiri, dan tetap ada jalur
           angka untuk yang mau isi cepat (/isi 20). */
        if (params[0] == EOS)
        {
            if (Garage_IsAtPump(playerid))
            {
                Garage_PumpMenu(playerid);
            }
            else
            {
                Garage_FuelReport(playerid);
            }
            return 1;
        }

        if (!Garage_IsAtPump(playerid))
        {
            SendClientMessage(playerid, UI_TEXT_MUTED, "[BENSIN] Isi hanya bisa di pom bensin kamp. Jerrycan bisa dipakai di mana saja: /isi jerrycan.");
            return 1;
        }

        new vehicleid = Vehicle_NearestToPlayer(playerid, VEHICLE_REFUEL_RANGE);
        if (vehicleid == INVALID_VEHICLE_ID)
        {
            SendClientMessage(playerid, UI_TEXT_MUTED, "[BENSIN] Tidak ada kendaraan dalam 4 meter.");
            return 1;
        }

        new requested = -1;
        if (sscanf(params, "d", requested))
        {
            SendClientMessage(playerid, UI_ACCENT_WARN, "Pemakaian: /isi [liter] (kosong = menu pom). Harga $5/liter.");
            return 1;
        }
        if (requested < 1) requested = -1;

        Garage_RefuelVehicle(playerid, vehicleid, requested);
        return 1;
    }

    if (!strcmp(cmd, "lockpick", true) || !strcmp(cmd, "bobol", true))
    {
        new vehicleid = Vehicle_NearestToPlayer(playerid, VEHICLE_LOCKPICK_RANGE);
        if (vehicleid == INVALID_VEHICLE_ID)
        {
            SendClientMessage(playerid, UI_TEXT_MUTED, "[LOCKPICK] Tidak ada kendaraan dalam 4 meter.");
            return 1;
        }
        if (!Vehicle_IsLocked(vehicleid))
        {
            SendClientMessage(playerid, UI_TEXT_MUTED, "[LOCKPICK] Kendaraan ini tidak terkunci.");
            return 1;
        }
        if (Inventory_CountItem(playerid, ITEM_LOCKPICK) < 1)
        {
            SendClientMessage(playerid, UI_ACCENT_WARN, "[LOCKPICK] Tidak punya lockpick. Beli di pedagang atau cari loot.");
            return 1;
        }

        Inventory_Consume(playerid, ITEM_LOCKPICK, 1, "lockpick");

        if (random(100) < 65)
        {
            Vehicle_SetLocked(vehicleid, false);
            SendClientMessage(playerid, UI_ACCENT_PRIMARY, "[LOCKPICK] Berhasil. Mesin bisa dinyalakan dengan /mesin.");
            Log_Format(LOG_INFO, "[LOCKPICK] char=%d kendaraan=%d berhasil", ActiveCharId[playerid], vehicleid);
        }
        else
        {
            SendClientMessage(playerid, UI_ACCENT_DANGER, "[LOCKPICK] Gagal, lockpick patah. Coba lagi dengan yang baru.");
            Log_Format(LOG_INFO, "[LOCKPICK] char=%d kendaraan=%d gagal", ActiveCharId[playerid], vehicleid);
        }
        return 1;
    }

    if (!strcmp(cmd, "mesin", true) || !strcmp(cmd, "engine", true))
    {
        if (GetPlayerState(playerid) != PLAYER_STATE_DRIVER)
        {
            SendClientMessage(playerid, UI_TEXT_MUTED, "[VEH] Anda harus di kursi pengemudi.");
            return 1;
        }

        new vehicleid = GetPlayerVehicleID(playerid);
        if (Vehicle_IsLocked(vehicleid))
        {
            SendClientMessage(playerid, UI_ACCENT_WARN, "[VEH] Terkunci. Gunakan /lockpick.");
            return 1;
        }
        if (Vehicle_GetFuel(vehicleid) <= 0.0)
        {
            SendClientMessage(playerid, UI_ACCENT_DANGER, "[VEH] Tangki kosong. Isi dengan /isi atau jerrycan.");
            return 1;
        }

        new engine, lights, alarm, doors, bonnet, boot, objective;
        GetVehicleParamsEx(vehicleid, engine, lights, alarm, doors, bonnet, boot, objective);
        Vehicle_SetEngine(vehicleid, engine == 0);
        SendClientMessage(playerid, UI_ACCENT_PRIMARY, (engine == 0) ? "[VEH] Mesin dinyalakan." : "[VEH] Mesin dimatikan.");
        return 1;
    }

    if (!strcmp(cmd, "kunci", true) || !strcmp(cmd, "lock", true))
    {
        if (GetPlayerState(playerid) != PLAYER_STATE_DRIVER)
        {
            SendClientMessage(playerid, UI_TEXT_MUTED, "[VEH] Anda harus di kursi pengemudi.");
            return 1;
        }

        new vehicleid = GetPlayerVehicleID(playerid);
        new ownerChar = Vehicle_GetOwnerChar(vehicleid);
        new charId = ActiveCharId[playerid];

        if (ownerChar != 0 && ownerChar != charId)
        {
            SendClientMessage(playerid, UI_ACCENT_WARN, "[VEH] Ini kendaraan orang lain. Kunci hanya bisa dibuka dengan /lockpick.");
            return 1;
        }

        if (Vehicle_IsLocked(vehicleid))
        {
            if (ownerChar == charId)
            {
                Vehicle_SetLocked(vehicleid, false);
                SendClientMessage(playerid, UI_ACCENT_PRIMARY, "[VEH] Kunci dibuka dengan kunci Anda.");
            }
            else
            {
                SendClientMessage(playerid, UI_ACCENT_WARN, "[VEH] Sudah terkunci. Unlock dari luar memakai /lockpick.");
            }
            return 1;
        }

        Vehicle_SetLocked(vehicleid, true);
        SendClientMessage(playerid, UI_ACCENT_PRIMARY, "[VEH] Kendaraan dikunci.");
        return 1;
    }

    if (!strcmp(cmd, "ekstrak", true) || !strcmp(cmd, "extract", true))
    {
        Extraction_Try(playerid);
        return 1;
    }

    if (!strcmp(cmd, "perbaiki", true) || !strcmp(cmd, "repair", true))
    {
        Garage_RepairNearest(playerid);
        return 1;
    }

    if (!strcmp(cmd, "sedot", true) || !strcmp(cmd, "siphon", true))
    {
        Garage_SiphonNearest(playerid);
        return 1;
    }

    if (!strcmp(cmd, "generator", true) || !strcmp(cmd, "genset", true))
    {
        BaseBuild_RefillGenerator(playerid);
        return 1;
    }

    if (!strcmp(cmd, "escort", true) || !strcmp(cmd, "trader", true))
    {
        Escort_Start(playerid);
        return 1;
    }

    if (!strcmp(cmd, "zone", true))
    {
        new zone = Player_GetZone(playerid);
        new zoneLabel[48];
        Zone_GetName(zone, zoneLabel, sizeof(zoneLabel));

        new line[160];
        format(line, sizeof(line), "[ZONE] %s | tier %d | zombie %d | %s",
            zoneLabel, Zone_GetTier(zone), ZoneZombieCount[Lobby_Of(playerid)][zone],
            World_IsNight() ? "malam" : "siang");
        SendClientMessage(playerid, UI_ACCENT_INFO, line);

        if (Zone_IsHub(zone))
        {
            SendClientMessage(playerid, UI_TEXT_SECONDARY, "[ZONE] Ini zona aman: zombie tidak pernah spawn di kamp.");
        }

        SendClientMessage(playerid, UI_TEXT_SECONDARY, "[ZONE] Sebaran semua zona + arah: /menu > Zona & sebaran zombie.");
        return 1;
    }

    if (!strcmp(cmd, "zspawn", true))
    {
        return Command_SpawnZombie(playerid, params);
    }

    if (!strcmp(cmd, "horde", true))
    {
        return Command_TriggerHorde(playerid, params);
    }

    if (!strcmp(cmd, "ac", true))
    {
        return Command_AnticheatReport(playerid);
    }

    if (!strcmp(cmd, "revive", true))
    {
        return Command_RevivePlayer(playerid, params);
    }

    /* Blok /login & /register duplikat dihapus (tak terjangkau: handler auth di
       atas sudah menangkap keduanya). Buka ulang dialog auth lewat /akun. */

    // Perintah gameplay butuh sesi login; /help tetap terbuka.
    if (strcmp(cmd, "help", true) != 0 && !Account_RequirePlaying(playerid)) return 1;

    if (!strcmp(cmd, "menu", true) || !strcmp(cmd, "bantuan", true))
    {
        Menu_Show(playerid);
        return 1;
    }

    if (!strcmp(cmd, "skill", true) || !strcmp(cmd, "skills", true))
    {
        Skills_Show(playerid);
        return 1;
    }

    if (!strcmp(cmd, "shelter", true) || !strcmp(cmd, "rumah", true))
    {
        Shelter_Toggle(playerid);
        return 1;
    }

    if (!strcmp(cmd, "event", true))
    {
        if (!IsServerAdmin(playerid))
        {
            SendClientMessage(playerid, UI_ACCENT_DANGER, "[EVENT] Khusus admin.");
            return 1;
        }
        Event_Force(playerid);
        return 1;
    }

    if (!strcmp(cmd, "shop", true) || !strcmp(cmd, "toko", true))
    {
        Vendor_ShowMenu(playerid);
        return 1;
    }

    if (!strcmp(cmd, "kematian", true) || !strcmp(cmd, "deaths", true))
    {
        if (!Account_IsPlaying(playerid))
        {
            SendClientMessage(playerid, UI_ACCENT_WARN, "[KEMATIAN] Login dulu untuk melihat riwayat.");
            return 1;
        }
        Deaths_ShowLast(playerid);
        return 1;
    }

    if (!strcmp(cmd, "decor", true))
    {
        /* Verifikasi visual dekorasi di client: /decor = daftar, /decor tp N = lihat. */
        if (!IsServerAdmin(playerid)) return 1;

        new decorArg[8];
        if (Command_Arg(params, 0, decorArg, sizeof(decorArg)) && !strcmp(decorArg, "tp", true))
        {
            new indexText[8];
            if (!Command_Arg(params, 1, indexText, sizeof(indexText)))
            {
                SendClientMessage(playerid, UI_ACCENT_INFO, "Pemakaian: /decor tp <indeks>. Lihat daftar dengan /decor.");
                return 1;
            }
            Decor_TeleportTo(playerid, strval(indexText));
            return 1;
        }
        Decor_ShowList(playerid);
        return 1;
    }

    if (!strcmp(cmd, "dokumen", true) || !strcmp(cmd, "lore", true))
    {
        if (Inventory_CountItem(playerid, ITEM_DOCUMENT) <= 0 && Inventory_CountItem(playerid, ITEM_NOTEBOOK) <= 0)
        {
            SendClientMessage(playerid, UI_TEXT_MUTED, "[DOKUMEN] Kamu belum memegang dokumen apa pun.");
            return 1;
        }
        Lore_ShowDocument(playerid);
        return 1;
    }

    if (!strcmp(cmd, "gudang", true))
    {
        /* Tanpa argumen = riwayat peti. Dengan argumen = tunjuk/cabut petugas
           gudang (hanya pemimpin party, dicek di PartyStash_SetOfficer). */
        new warehouseTarget = INVALID_PLAYER_ID;
        if (sscanf(params, "u", warehouseTarget))
        {
            PartyStash_ShowHistory(playerid);
            return 1;
        }
        PartyStash_SetOfficer(playerid, warehouseTarget);
        return 1;
    }

    if (!strcmp(cmd, "base", true))
    {
        /* Sub-perintah party base (ROADMAP 222). Ditaruh paling depan supaya
           /base add|remove|members tidak jatuh ke tampilan status. */
        new sub[12];
        new subTarget;
        if (sscanf(params, "s[12]u", sub, subTarget) >= 1)
        {
            if (!strcmp(sub, "add", true) || !strcmp(sub, "tambah", true))
            {
                if (sscanf(params, "s[12]u", sub, subTarget) != 2)
                {
                    SendClientMessage(playerid, UI_ACCENT_WARN, "Pemakaian: /base add [playerid]");
                    return 1;
                }
                BaseMembers_Add(playerid, subTarget, BASE_ROLE_MEMBER);
                return 1;
            }
            if (!strcmp(sub, "officer", true))
            {
                if (sscanf(params, "s[12]u", sub, subTarget) != 2)
                {
                    SendClientMessage(playerid, UI_ACCENT_WARN, "Pemakaian: /base officer [playerid]");
                    return 1;
                }
                BaseMembers_Add(playerid, subTarget, BASE_ROLE_OFFICER);
                return 1;
            }
            if (!strcmp(sub, "remove", true) || !strcmp(sub, "kick", true))
            {
                if (sscanf(params, "s[12]u", sub, subTarget) != 2)
                {
                    SendClientMessage(playerid, UI_ACCENT_WARN, "Pemakaian: /base remove [playerid]");
                    return 1;
                }
                if (!BaseMembers_Remove(playerid, subTarget))
                {
                    SendClientMessage(playerid, UI_ACCENT_WARN, "[BASE] Tidak bisa mengeluarkan pemain itu.");
                }
                return 1;
            }
            if (!strcmp(sub, "members", true) || !strcmp(sub, "anggota", true))
            {
                BaseMembers_Show(playerid);
                return 1;
            }
        }

        if (Base_Id(playerid) == 0)
        {
            Base_Load(playerid);
            SendClientMessage(playerid, UI_TEXT_MUTED, "[BASE] Memuat data base, coba lagi.");
            return 1;
        }

        if (!BaseBuild_EnsureLoaded(playerid))
        {
            SendClientMessage(playerid, UI_TEXT_MUTED, "[BASE] Memuat struktur base, coba lagi.");
            return 1;
        }

        new line[144], integrity = 0, health = 0, healthMax = 0;
        BaseBuild_GetIntegrity(playerid, integrity, health, healthMax);

        format(line, sizeof(line), "[BASE] #%d tier %d | %d struktur | integritas %d%% (%d/%d HP)",
            Base_Id(playerid), Base_GetTier(playerid), BaseBuild_CountBase(Base_Id(playerid)),
            integrity, health, healthMax);
        SendClientMessage(playerid, UI_ACCENT_PRIMARY, line);

        format(line, sizeof(line), "[BASE] Barricade server=%d | stash %d slot (crate %d) | /build /upgrade /bongkar /fix",
            Base_ActiveCount(), BaseBuild_StashCapacity(playerid), BaseBuild_CrateCount(playerid));
        SendClientMessage(playerid, UI_TEXT_SECONDARY, line);
        return 1;
    }

    /* /perbaiki adalah milik kendaraan (di atas); base hanya lewat /fix supaya
       tidak ada alias yang membayangi command lain. */
    if (!strcmp(cmd, "fix", true))
    {
        /* Struktur base diprioritaskan: kalau tidak ada, baru coba barricade. */
        if (!BaseBuild_RepairNearest(playerid)) Base_RepairBarricade(playerid);
        return 1;
    }

    if (!strcmp(cmd, "raidbase", true))
    {
        if (!IsServerAdmin(playerid))
        {
            SendClientMessage(playerid, UI_ACCENT_WARN, "[HORDE] Hanya admin.");
            return 1;
        }
        Horde_ForceBaseRaid(playerid);
        return 1;
    }

    if (!strcmp(cmd, "lobby", true) || !strcmp(cmd, "dunia", true))
    {
        Lobby_ShowSelect(playerid);
        return 1;
    }

    if (!strcmp(cmd, "nightfall", true))
    {
        if (!IsServerAdmin(playerid))
        {
            SendClientMessage(playerid, UI_ACCENT_WARN, "[LOBBY] Hanya admin.");
            return 1;
        }

        /* Toggle manual untuk pengujian: normalnya populasi yang membuka lobby. */
        if (Lobby_IsValid(NIGHTFALL_LOBBY_ID))
        {
            if (!Lobby_Lock(NIGHTFALL_LOBBY_ID, "perintah admin"))
            {
                SendClientMessage(playerid, UI_ACCENT_WARN, "[LOBBY] Tidak bisa ditutup: masih ada pemain di dalamnya.");
            }
        }
        else if (!Lobby_Unlock(NIGHTFALL_LOBBY_ID, "perintah admin"))
        {
            SendClientMessage(playerid, UI_ACCENT_WARN, "[LOBBY] Nightfall tidak bisa dibuka sekarang.");
        }
        return 1;
    }

    /* /peti adalah peti party (di atas); spawn peti admin hanya /lootnode. */
    if (!strcmp(cmd, "lootnode", true))
    {
        if (!IsServerAdmin(playerid))
        {
            SendClientMessage(playerid, UI_ACCENT_WARN, "[LOOT] Hanya admin.");
            return 1;
        }
        LootNode_AdminSpawn(playerid);
        return 1;
    }

    if (!strcmp(cmd, "stats", true) || !strcmp(cmd, "statistik", true))
    {
        Stats_Show(playerid);
        return 1;
    }

    if (!strcmp(cmd, "respec", true))
    {
        Skills_Respec(playerid);
        return 1;
    }

    if (!strcmp(cmd, "selftest", true))
    {
        if (!IsServerAdmin(playerid))
        {
            SendClientMessage(playerid, UI_ACCENT_WARN, "[SELFTEST] Hanya admin.");
            return 1;
        }
        Selftest_Run(playerid);
        return 1;
    }

    if (!strcmp(cmd, "build", true) || !strcmp(cmd, "bangun", true))
    {
        BaseBuild_ShowMenu(playerid);
        return 1;
    }

    if (!strcmp(cmd, "bongkar", true) || !strcmp(cmd, "dismantle", true))
    {
        BaseBuild_DismantleNearest(playerid);
        return 1;
    }

    if (!strcmp(cmd, "upgrade", true))
    {
        BaseBuild_Upgrade(playerid);
        return 1;
    }

    if (!strcmp(cmd, "diag", true))
    {
        if (!IsServerAdmin(playerid))
        {
            SendClientMessage(playerid, UI_ACCENT_DANGER, "[DIAG] Khusus admin.");
            return 1;
        }

        new line[192];
        format(line, sizeof(line), "[DIAG] zombie=%d/%d | barricade=%d | loot=%d | players=%d | sql=%d",
            Zombie_GetActiveCount(), MAX_ZOMBIES, Base_ActiveCount(), LootDropCount, Iter_Count(PlayerPool), SQL_IsReady());
        SendClientMessage(playerid, UI_ACCENT_PRIMARY, line);

        format(line, sizeof(line), "[DIAG] flag AC=%d dalam jendela %d ms | skor heat zona ini=%.1f",
            AC_GetFlagWindowCount(playerid), AC_FLAG_WINDOW_MS, Horde_GetZoneHeat(Player_GetZone(playerid)));
        SendClientMessage(playerid, UI_TEXT_SECONDARY, line);

        format(line, sizeof(line), "[DIAG] tick ms: zombie=%d/%d dir=%d/%d base=%d/%d hud=%d/%d player=%d/%d",
            Perf_GetLast(PERF_ZOMBIE), Perf_GetPeak(PERF_ZOMBIE),
            Perf_GetLast(PERF_DIRECTOR), Perf_GetPeak(PERF_DIRECTOR),
            Perf_GetLast(PERF_BASE), Perf_GetPeak(PERF_BASE),
            Perf_GetLast(PERF_HUD), Perf_GetPeak(PERF_HUD),
            Perf_GetLast(PERF_PLAYER), Perf_GetPeak(PERF_PLAYER));
        SendClientMessage(playerid, UI_TEXT_SECONDARY, line);

        /* Slot profiler tambahan (audit F16): sebelumnya needs/event/horde/
           autosave/loot/veh tidak terlihat di /diag. */
        format(line, sizeof(line), "[DIAG] tick ms: loot=%d/%d veh=%d/%d needs=%d/%d event=%d/%d horde=%d/%d save=%d/%d",
            Perf_GetLast(PERF_LOOT), Perf_GetPeak(PERF_LOOT),
            Perf_GetLast(PERF_VEHICLE), Perf_GetPeak(PERF_VEHICLE),
            Perf_GetLast(PERF_NEEDS), Perf_GetPeak(PERF_NEEDS),
            Perf_GetLast(PERF_EVENT), Perf_GetPeak(PERF_EVENT),
            Perf_GetLast(PERF_HORDE), Perf_GetPeak(PERF_HORDE),
            Perf_GetLast(PERF_AUTOSAVE), Perf_GetPeak(PERF_AUTOSAVE));
        SendClientMessage(playerid, UI_TEXT_SECONDARY, line);
        return 1;
    }

    if (!strcmp(cmd, "craft", true) || !strcmp(cmd, "meja", true))
    {
        /* Meja kamp atau Bench di base sendiri: dua tempat, satu perintah. */
        if (!Player_IsSafe(playerid) && !BaseBuild_HasNearby(playerid, STRUCT_BENCH, 6.0))
        {
            SendClientMessage(playerid, UI_ACCENT_WARN, "[CRAFT] Butuh meja kamp atau Bench di base (6 m).");
            return 1;
        }
        CraftUI_Show(playerid);
        return 1;
    }

    if (!strcmp(cmd, "stash", true))
    {
        Stash_Open(playerid);
        return 1;
    }

    if (!strcmp(cmd, "take", true) || !strcmp(cmd, "ambil", true))
    {
        if (!Loot_TakeNearest(playerid, 3.0))
        {
            SendClientMessage(playerid, UI_TEXT_MUTED, "[LOOT] Tidak ada loot dalam 3 meter.");
        }
        return 1;
    }

    if (!strcmp(cmd, "inv", true))
    {
        InventoryUI_Show(playerid);
        return 1;
    }

    if (!strcmp(cmd, "tas", true) || !strcmp(cmd, "bag", true) || !strcmp(cmd, "kapasitas", true))
    {
        Bag_ShowInfo(playerid);
        return 1;
    }

    if (!strcmp(cmd, "pakai", true) || !strcmp(cmd, "use", true))
    {
        if (!Account_IsPlaying(playerid)) return 1;

        new target[32];
        if (!Command_Arg(params, 0, target, sizeof(target)))
        {
            SendClientMessage(playerid, UI_TEXT_MUTED, "[TAS] /pakai <nama item>   contoh: /pakai medkit");
            InventoryUI_Show(playerid);
            return 1;
        }

        new slot = -1;
        if (IsNumericToken(target)) slot = InventoryUI_SlotByNumber(playerid, strval(target));
        if (slot == -1) slot = InventoryUI_FindSlotByName(playerid, target);

        if (slot == -1)
        {
            SendClientMessage(playerid, UI_ACCENT_WARN, "[TAS] Item tidak ada di tas. Lihat daftar: /inv");
            return 1;
        }

        if (!InventoryUI_Use(playerid, slot))
        {
            SendClientMessage(playerid, UI_TEXT_MUTED, "[TAS] Item ini tidak bisa dipakai sekarang.");
        }
        return 1;
    }

    if (!strcmp(cmd, "hotbar", true) || !strcmp(cmd, "hs", true) || !strcmp(cmd, "quick", true))
    {
        if (!Account_IsPlaying(playerid)) return 1;

        new action[16];
        if (!Command_Arg(params, 0, action, sizeof(action)))
        {
            Hotbar_Show(playerid);
            return 1;
        }

        if (IsNumericToken(action))
        {
            Hotbar_Use(playerid, strval(action) - 1);
            return 1;
        }

        new slotToken[8];
        if (!Command_Arg(params, 1, slotToken, sizeof(slotToken)))
        {
            SendClientMessage(playerid, UI_TEXT_MUTED, "[HOTBAR] /hotbar set <1-5> <nama item>   /hotbar clear <1-5>   /hs <1-5>");
            Hotbar_Show(playerid);
            return 1;
        }

        new index = strval(slotToken) - 1;
        if (index < 0 || index >= HOTBAR_SLOTS)
        {
            SendClientMessage(playerid, UI_ACCENT_WARN, "[HOTBAR] Nomor slot harus 1 sampai 5.");
            return 1;
        }

        if (!strcmp(action, "clear", true) || !strcmp(action, "hapus", true) || !strcmp(action, "kosong", true))
        {
            Hotbar_Clear(playerid, index);
            SendClientMessage(playerid, UI_ACCENT_PRIMARY, "[HOTBAR] Slot dikosongkan.");
            Hotbar_Show(playerid);
            return 1;
        }

        if (strcmp(action, "set", true) && strcmp(action, "isi", true))
        {
            SendClientMessage(playerid, UI_TEXT_MUTED, "[HOTBAR] /hotbar set <1-5> <nama item>   /hotbar clear <1-5>   /hs <1-5>");
            return 1;
        }

        new itemToken[32], extraToken[32], wantedName[32];
        if (!Command_Arg(params, 2, itemToken, sizeof(itemToken)))
        {
            SendClientMessage(playerid, UI_TEXT_MUTED, "[HOTBAR] Contoh: /hotbar set 1 bandage");
            return 1;
        }

        /* Nama item boleh dua kata (Canned Food, Tas Harian): gabungkan argumen 3 kalau ada. */
        format(wantedName, sizeof(wantedName), "%s", itemToken);
        if (Command_Arg(params, 3, extraToken, sizeof(extraToken)))
        {
            strcat(wantedName, " ", sizeof(wantedName));
            strcat(wantedName, extraToken, sizeof(wantedName));
        }

        new invSlot = InventoryUI_FindSlotByName(playerid, wantedName);
        if (invSlot == -1)
        {
            SendClientMessage(playerid, UI_ACCENT_WARN, "[HOTBAR] Item itu tidak ada di tas. Lihat /inv.");
            return 1;
        }

        new itemid = PlayerInventory[playerid][invSlot][invItemId];
        Hotbar_Set(playerid, index, itemid);
        Account_SaveCharacter(playerid);

        new itemLabel[24], line[112];
        Item_GetName(itemid, itemLabel, sizeof(itemLabel));
        format(line, sizeof(line), "[HOTBAR] Slot %d = %s. Pakai dengan /hs %d.", index + 1, itemLabel, index + 1);
        SendClientMessage(playerid, UI_ACCENT_PRIMARY, line);
        Hotbar_Show(playerid);
        return 1;
    }

    if (!strcmp(cmd, "hud", true))
    {
        if (!Account_IsPlaying(playerid)) return 1;

        if (PlayerHudVisible[playerid])
        {
            HUD_Hide(playerid);
            SendClientMessage(playerid, UI_TEXT_MUTED, "[HUD] disembunyikan. Ketik /hud untuk menampilkan lagi.");
        }
        else
        {
            /* force=true: ini perintah eksplisit, jadi harus menampilkan walau
               PlayerHudVisible masih false dari /hud sebelumnya. */
            HUD_Reshow(playerid, true);
            SendClientMessage(playerid, UI_ACCENT_PRIMARY, "[HUD] ditampilkan ulang (HP, kebutuhan, zona, BBM, progres).");
        }
        return 1;
    }

    if (!strcmp(cmd, "mobil", true) || !strcmp(cmd, "car", true))
    {
        if (!Account_IsPlaying(playerid)) return 1;

        if (GetPlayerState(playerid) == PLAYER_STATE_DRIVER)
        {
            SendClientMessage(playerid, UI_TEXT_MUTED, "[MOBIL] Kamu sedang menyetir. Keluar dulu kalau mau mobil lain.");
            return 1;
        }

        if (WildVeh_EnsureOneNear(playerid))
        {
            SendClientMessage(playerid, UI_ACCENT_PRIMARY, "[MOBIL] Satu mobil diletakkan 7 meter di depanmu. Masuk lewat pintu pengemudi.");
        }
        else
        {
            SendClientMessage(playerid, UI_TEXT_MUTED, "[MOBIL] Sudah ada mobil dalam 40 meter. Ketik /uicheck untuk melihat jaraknya.");
        }
        return 1;
    }

    if (!strcmp(cmd, "rapikan", true) || !strcmp(cmd, "compact", true))
    {
        if (!Account_IsPlaying(playerid)) return 1;
        Inventory_Compact(playerid);
        return 1;
    }

    if (!strcmp(cmd, "buang", true) || !strcmp(cmd, "drop", true))
    {
        if (!Account_IsPlaying(playerid)) return 1;

        new target[32], count[8];
        if (!Command_Arg(params, 0, target, sizeof(target)))
        {
            SendClientMessage(playerid, UI_TEXT_MUTED, "[TAS] /buang <nama item> [jumlah]   contoh: /buang scrap 5");
            return 1;
        }

        new slot = -1;
        if (IsNumericToken(target)) slot = InventoryUI_SlotByNumber(playerid, strval(target));
        if (slot == -1) slot = InventoryUI_FindSlotByName(playerid, target);

        if (slot == -1)
        {
            SendClientMessage(playerid, UI_ACCENT_WARN, "[TAS] Item tidak ada di tas. Lihat daftar: /inv");
            return 1;
        }

        new amount = 0;
        if (Command_Arg(params, 1, count, sizeof(count))) amount = strval(count);
        InventoryUI_Drop(playerid, slot, amount);
        return 1;
    }

    if (!strcmp(cmd, "profil", true) || !strcmp(cmd, "profile", true) || !strcmp(cmd, "bio", true))
    {
        CharCreate_Describe(playerid);
        return 1;
    }

    if (!strcmp(cmd, "veh", true))
    {
        return Command_SpawnVehicle(playerid, params);
    }

    if (!strcmp(cmd, "test", true))
    {
        return Command_DevTest(playerid);
    }

    if (!strcmp(cmd, "ai", true))
    {
        return Command_DevAiInfo(playerid);
    }

    if (!strcmp(cmd, "noise", true))
    {
        return Command_DevNoise(playerid);
    }

    if (!strcmp(cmd, "hurt", true))
    {
        return Command_DevHurt(playerid);
    }

    if (!strcmp(cmd, "infect", true))
    {
        return Command_DevInfect(playerid);
    }

    if (!strcmp(cmd, "setbbm", true))
    {
        if (!Command_DevAllowed(playerid)) return Command_DevDenied(playerid);

        new Float:liters;
        if (isnull(params) || sscanf(params, "f", liters))
        {
            SendClientMessage(playerid, UI_ACCENT_WARN, "Pemakaian: /setbbm [liter]");
            return 1;
        }

        new vehicleid = (GetPlayerState(playerid) == PLAYER_STATE_DRIVER)
            ? GetPlayerVehicleID(playerid)
            : Vehicle_NearestToPlayer(playerid, 12.0);
        if (vehicleid == INVALID_VEHICLE_ID)
        {
            SendClientMessage(playerid, UI_TEXT_MUTED, "[DEV] Tidak ada kendaraan dalam 12 meter.");
            return 1;
        }
        if (!Vehicle_SetFuel(vehicleid, liters))
        {
            SendClientMessage(playerid, UI_TEXT_MUTED, "[DEV] Kendaraan ini tidak memakai bahan bakar.");
            return 1;
        }

        new line[128];
        format(line, sizeof(line), "[DEV] BBM kendaraan %d = %.1f/%.0f L (~%.0f km).",
            vehicleid, Vehicle_GetFuel(vehicleid), Vehicle_GetTank(vehicleid), Vehicle_RangeKm(vehicleid));
        SendClientMessage(playerid, UI_ACCENT_PRIMARY, line);
        return 1;
    }

    if (!strcmp(cmd, "heal", true))
    {
        return Command_DevHeal(playerid);
    }

    if (!strcmp(cmd, "uicheck", true))
    {
        return Command_DevUiCheck(playerid);
    }

    return 0;
}

/* Diagnostik UI: pemain melaporkan "HUD tidak muncul / tidak ada mobil".
   Perintah ini mencetak apa yang SEHARUSNYA tampil di client (status HUD,
   lobi, dunia virtual, jumlah mobil liar di lobi, jarak mobil terdekat) dan
   menyalinnya ke log supaya bisa dibaca tanpa screenshot. */
stock Command_DevUiCheck(playerid)
{
    /* Sengaja tidak digerbang DEV: pemain yang melaporkan "HUD tidak muncul"
       harus bisa membaca kondisi render di client-nya sendiri. */
    if (!Account_IsPlaying(playerid))
    {
        SendClientMessage(playerid, UI_TEXT_MUTED, "[UIC] Masuk dulu dengan /masuk.");
        return 1;
    }

    new lobbyId = Lobby_Of(playerid);
    new world = GetPlayerVirtualWorld(playerid);
    new interior = GetPlayerInterior(playerid);
    new nearest = Vehicle_NearestToPlayer(playerid, 400.0);
    new Float:px, Float:py, Float:pz;
    GetPlayerPos(playerid, px, py, pz);

    new Float:vehDist = -1.0;
    new vehModel = 0;
    if (nearest != INVALID_VEHICLE_ID)
    {
        new Float:vx, Float:vy, Float:vz;
        GetVehiclePos(nearest, vx, vy, vz);
        vehDist = VectorSize(px - vx, py - vy, pz - vz);
        vehModel = GetVehicleModel(nearest);
    }

    new line[144];
    format(line, sizeof(line), "[UIC] HUD tampil=%d lobi=%d vw=%d interior=%d mobil_lobi=%d terdekat=%d jarak=%.1f m model=%d",
        PlayerHudVisible[playerid] ? 1 : 0, lobbyId, world, interior,
        WildVeh_CountForLobby(lobbyId), nearest, vehDist, vehModel);
    SendClientMessage(playerid, UI_ACCENT_PRIMARY, line);
    Log_Info(line);

    format(line, sizeof(line), "[UIC] pos=%.1f,%.1f,%.1f hidup=%d akun=%d peti_dalam_50m=%d",
        px, py, pz, Player_IsAlive(playerid) ? 1 : 0, Account_IsPlaying(playerid) ? 1 : 0,
        LootDrop_CountNear(px, py, 50.0));
    SendClientMessage(playerid, UI_TEXT_SECONDARY, line);
    Log_Info(line);

    SendClientMessage(playerid, UI_TEXT_MUTED, "[UIC] Coba juga /veh 411 (mobil langsung di samping) dan /isi (pom).");
    return 1;
}

/* Harness uji manual: dipakai untuk memvalidasi chase, attack, dan infeksi
   dengan client asli sebelum sistem akun dibangun di atasnya. */
stock bool:Command_DevAllowed(playerid)
{
    #if DEV_MODE
        #pragma unused playerid
        return true;
    #else
        return IsServerAdmin(playerid) ? true : false;
    #endif
}

stock Command_DevDenied(playerid)
{
    SendClientMessage(playerid, UI_ACCENT_DANGER, "[DEV] Perintah ini butuh DEV_MODE=1 atau admin.");
    return 1;
}

stock Command_SpawnVehicle(playerid, const params[])
{
    if (!Command_DevAllowed(playerid)) return Command_DevDenied(playerid);

    new modelid = TEST_VEHICLE_DEFAULT;
    if (params[0] != '\0' && sscanf(params, "d", modelid))
    {
        SendClientMessage(playerid, UI_ACCENT_WARN, "Pemakaian: /veh [model 400-611]");
        return 1;
    }

    if (!Vehicle_ModelAllowed(modelid))
    {
        SendClientMessage(playerid, UI_ACCENT_WARN, "[VEH] Model tidak valid. Pakai 400-611 (tanpa kereta/tram).");
        return 1;
    }

    new vehicleid = Vehicle_CreateForPlayer(playerid, modelid);
    if (vehicleid == INVALID_VEHICLE_ID)
    {
        SendClientMessage(playerid, UI_ACCENT_DANGER, "[VEH] Gagal membuat kendaraan.");
        return 1;
    }

    new line[128];
    format(line, sizeof(line), "[VEH] Model %d siap. Slot uji maksimal %d, yang paling lama dihapus.", modelid, MAX_TEST_VEHICLES);
    SendClientMessage(playerid, UI_ACCENT_PRIMARY, line);
    return 1;
}

stock Command_DevTest(playerid)
{
    if (!Command_DevAllowed(playerid)) return Command_DevDenied(playerid);

    if (!Player_IsAlive(playerid))
    {
        SendClientMessage(playerid, UI_ACCENT_WARN, "[DEV] Hidupkan diri dulu (/heal).");
        return 1;
    }

    new zone = Player_GetZone(playerid);
    new zoneLabel[48];
    Zone_GetName(zone, zoneLabel, sizeof(zoneLabel));

    if (Zone_IsHub(zone))
    {
        SendClientMessage(playerid, UI_ACCENT_WARN,
            "[DEV] Anda di zona HUB (aman): zombie tidak boleh menyerang di sini.");
        SendClientMessage(playerid, UI_TEXT_SECONDARY,
            "[DEV] Keluar dulu dari kamp (lewat /zone lihat nama zona), lalu /test lagi.");
        return 1;
    }

    new Float:x, Float:y, Float:z, Float:angle;
    GetPlayerPos(playerid, x, y, z);
    GetPlayerFacingAngle(playerid, angle);

    new Float:dirX = floatsin(-angle, degrees);
    new Float:dirY = floatcos(-angle, degrees);
    new spawned = 0;

    for (new i = 0; i < 6; i++)
    {
        new Float:offset = float(i % 3) * 1.5 - 1.5;
        new Float:spawnX = x + dirX * (14.0 + float(i / 3) * 2.0) + dirY * offset;
        new Float:spawnY = y + dirY * (14.0 + float(i / 3) * 2.0) - dirX * offset;

        if (Zombie_Create(ZT_WALKER, spawnX, spawnY, z, Lobby_Of(playerid)) != INVALID_ZOMBIE) spawned++;
    }

    if (Zombie_Create(ZT_BLOATER, x + dirX * 18.0, y + dirY * 18.0, z, Lobby_Of(playerid)) != INVALID_ZOMBIE) spawned++;

    new line[144];
    format(line, sizeof(line), "[DEV] zona %s: %d zombie di depan. Cek chase + pukulan.", zoneLabel, spawned);
    SendClientMessage(playerid, UI_ACCENT_PRIMARY, line);
    SendClientMessage(playerid, UI_TEXT_SECONDARY, "[DEV] /ai diagnosa | /noise suara | /hurt -30 HP | /infect +50 | /heal");
    return 1;
}

/* Diagnosa satu zombie terdekat: kalau angka di sini benar tapi tetap tidak memukul,
   masalahnya ada di FCNPC (navigasi), bukan di logika AI. */
stock Command_DevAiInfo(playerid)
{
    if (!Command_DevAllowed(playerid)) return Command_DevDenied(playerid);

    new Float:px, Float:py, Float:pz;
    GetPlayerPos(playerid, px, py, pz);

    new nearest = INVALID_ZOMBIE;
    new Float:nearestDist = 9999.0;

    foreach (new zombieid : ZombiePool)
    {
        if (!Zombie_IsActive(zombieid)) continue;

        new Float:zx, Float:zy, Float:zz;
        FCNPC_GetPosition(zombieid, zx, zy, zz);
        new Float:distance = VectorSize(px - zx, py - zy, pz - zz);
        if (distance < nearestDist)
        {
            nearestDist = distance;
            nearest = zombieid;
        }
    }

    if (nearest == INVALID_ZOMBIE)
    {
        SendClientMessage(playerid, UI_ACCENT_WARN, "[AI] Tidak ada zombie aktif dalam 1 km.");
        return 1;
    }

    new zone = Player_GetZone(playerid);
    new zoneLabel[48];
    Zone_GetName(zone, zoneLabel, sizeof(zoneLabel));

    new line[192];
    format(line, sizeof(line), "[AI] zona=%s hub=%d | jarak=%.1f m (serang < %.1f)",
        zoneLabel, Zone_IsHub(zone), nearestDist, ZOMBIE_ATTACK_RANGE);
    SendClientMessage(playerid, UI_ACCENT_INFO, line);

    format(line, sizeof(line), "[AI] npc=%d state=%d target=%d noise=%d moving=%d",
        nearest, _:ZombieAI[nearest][zaiBehaviour], ZombieAI[nearest][zaiTarget],
        ZombieAI[nearest][zaiHasNoisePoint], FCNPC_IsMoving(nearest));
    SendClientMessage(playerid, UI_ACCENT_INFO, line);
    return 1;
}

stock Command_DevNoise(playerid)
{
    if (!Command_DevAllowed(playerid)) return Command_DevDenied(playerid);

    new Float:x, Float:y, Float:z;
    GetPlayerPos(playerid, x, y, z);
    Hash_TriggerNoise(x, y, z, 60.0);
    SendClientMessage(playerid, UI_ACCENT_INFO, "[DEV] Noise radius 60 m dipancarkan dari posisi Anda.");
    return 1;
}

stock Command_DevHurt(playerid)
{
    if (!Command_DevAllowed(playerid)) return Command_DevDenied(playerid);

    Player_ApplyDamage(playerid, 30.0);
    SendClientMessage(playerid, UI_ACCENT_WARN, "[DEV] -30 HP.");
    return 1;
}

stock Command_DevInfect(playerid)
{
    if (!Command_DevAllowed(playerid)) return Command_DevDenied(playerid);

    Player_AddInfection(playerid, 50.0);
    SendClientMessage(playerid, UI_ACCENT_WARN, "[DEV] +50 infeksi. Tahap 2 mulai merusak HP, tahap 3 mengubah Anda.");
    return 1;
}

stock Command_DevHeal(playerid)
{
    if (!Command_DevAllowed(playerid)) return Command_DevDenied(playerid);

    Player_Heal(playerid, 100.0);
    Player_CureInfection(playerid, 100.0);
    SendClientMessage(playerid, UI_ACCENT_PRIMARY, "[DEV] HP penuh, infeksi nol.");
    return 1;
}

stock Command_SpawnZombie(playerid, const params[])
{
    if (!IsServerAdmin(playerid))
    {
        SendClientMessage(playerid, UI_ACCENT_DANGER, "[ADMIN] Perintah khusus admin.");
        return 1;
    }

    /* sscanf 2.13.8 tidak mendukung "dD(1)": parameter opsional diurai manual
       supaya tidak memicu "Include / plugin mismatch" di log. */
    new type, amount = 1;
    if (sscanf(params, "dd", type, amount) && sscanf(params, "d", type))
    {
        SendClientMessage(playerid, UI_ACCENT_WARN, "Pemakaian: /zspawn <type 0-6> <jumlah>");
        return 1;
    }

    /* Boss (type 7) TIDAK boleh di-spawn manual: /zspawn 7 lolos dari cek
       lama (>= _:E_ZOMBIE_TYPE = 8) dan bisa dipanen berulang (audit F8). */
    if (type < 0 || type > 6)
    {
        SendClientMessage(playerid, UI_ACCENT_WARN, "Tipe zombie 0-6.");
        return 1;
    }

    if (amount < 1 || amount > 30)
    {
        SendClientMessage(playerid, UI_ACCENT_WARN, "Jumlah 1-30.");
        return 1;
    }

    new Float:x, Float:y, Float:z, Float:angle;
    GetPlayerPos(playerid, x, y, z);
    GetPlayerFacingAngle(playerid, angle);

    new Float:spawnX = x + floatsin(-angle, degrees) * 6.0;
    new Float:spawnY = y + floatcos(-angle, degrees) * 6.0;

    new spawned = 0;
    for (new i = 0; i < amount; i++)
    {
        if (Zombie_Create(E_ZOMBIE_TYPE:type, spawnX + float(i % 5), spawnY + float(i / 5), z, Lobby_Of(playerid)) != INVALID_ZOMBIE)
        {
            spawned++;
        }
    }

    new line[96];
    format(line, sizeof(line), "[ADMIN] %d zombie tipe %d di-spawn.", spawned, type);
    SendClientMessage(playerid, UI_ACCENT_PRIMARY, line);
    return 1;
}

stock Command_TriggerHorde(playerid, const params[])
{
    if (!IsServerAdmin(playerid))
    {
        SendClientMessage(playerid, UI_ACCENT_DANGER, "[ADMIN] Perintah khusus admin.");
        return 1;
    }

    new zone, amount = 30;
    if (sscanf(params, "dd", zone, amount) && sscanf(params, "d", zone))
    {
        SendClientMessage(playerid, UI_ACCENT_WARN, "Pemakaian: /horde <zone 0-9> <jumlah>");
        return 1;
    }

    if (zone < 0 || zone >= MAX_ZONE_COUNT)
    {
        SendClientMessage(playerid, UI_ACCENT_WARN, "Zone tidak valid.");
        return 1;
    }

    if (amount < 1 || amount > 120)
    {
        SendClientMessage(playerid, UI_ACCENT_WARN, "Jumlah 1-120.");
        return 1;
    }

    new spawned = Director_TriggerHorde(zone, amount);

    new line[96];
    format(line, sizeof(line), "[ADMIN] Horde %d zombie diluncurkan.", spawned);
    SendClientMessage(playerid, UI_ACCENT_DANGER, line);
    return 1;
}

stock Command_AnticheatReport(playerid)
{
    if (!IsServerAdmin(playerid))
    {
        SendClientMessage(playerid, UI_ACCENT_DANGER, "[ADMIN] Perintah khusus admin.");
        return 1;
    }

    new line[192];
    format(line, sizeof(line), "[AC] zombie=%d/%d drop=%d cursor=%d",
        Zombie_GetActiveCount(), MAX_ZOMBIES, LootDropCount, ZombieSliceCursor);
    SendClientMessage(playerid, UI_ACCENT_INFO, line);

    foreach (new otherPlayer : PlayerPool)
    {
        if (Anticheat_GetFlagCount(otherPlayer) == 0) continue;

        format(line, sizeof(line), "[AC] player %d flag=%d", otherPlayer, Anticheat_GetFlagCount(otherPlayer));
        SendClientMessage(playerid, UI_ACCENT_WARN, line);
    }
    return 1;
}

stock Command_RevivePlayer(playerid, const params[])
{
    if (!IsServerAdmin(playerid))
    {
        SendClientMessage(playerid, UI_ACCENT_DANGER, "[ADMIN] Perintah khusus admin.");
        return 1;
    }

    new target;
    if (sscanf(params, "d", target))
    {
        SendClientMessage(playerid, UI_ACCENT_WARN, "Pemakaian: /revive <playerid>");
        return 1;
    }

    if (!IsPlayerConnected(target))
    {
        SendClientMessage(playerid, UI_ACCENT_WARN, "Pemain tidak tersambung.");
        return 1;
    }

    Player_Revive(target);
    SendClientMessage(target, UI_ACCENT_PRIMARY, "[ADMIN] Anda di-revive oleh staff.");
    return 1;
}
