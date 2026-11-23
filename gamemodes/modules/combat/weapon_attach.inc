#if defined _ls_weapon_attach_included
    #endinput
#endif
#define _ls_weapon_attach_included

/* Modifikasi Senjata / Attachment System (fitur #16).

   Tiga attachment, semuanya punya wujud nyata menempel di tangan pemain
   (SetPlayerAttachedObject) DAN efek server-side yang terasa:

     - Scope          -> senapan/sniper (30,31,33,34): damage tembakan x1.25.
     - Silencer       -> pistol (22-24): menempelkan item Suppressor yang sudah
                         ada ke senjata; tembakan senyap tanpa menimbang tas.
                         Punya sisa tembakan sendiri, habis = lepas.
     - Magasin Ekstra -> pistol & senapan: setiap tembakan ke-4 tidak mengikis
                         kondisi senjata (klip lebih besar, wear lebih lambat).

   State per pemain per kategori senjata, tersimpan di kolom characters.watt_*
   (migrasi 032). Visual hanya mengikuti senjata di tangan via tick 500 ms —
   bukan OnPlayerUpdate (invariant). */

#define WATT_SLOT_MAIN      8       /* scope / silencer (slot 9 dipakai tas)   */
#define WATT_SLOT_MAG       7       /* magasin ekstra                          */
#define WATT_BONE_HAND      8       /* bone 8 = tangan kanan                   */
#define WATT_MODEL_SCOPE    2690    /* tabung kecil -> scope                   */
#define WATT_MODEL_SILENCER 2690    /* tabung sama, skala lebih kecil          */
#define WATT_MODEL_MAG      2038    /* kotak amunisi -> magasin ekstra         */
#define WATT_SCOPE_MULT     1.25
#define WATT_MAG_EVERY      4       /* tembakan ke-N bebas wear                */
#define WATT_SILENCER_SHOTS 120     /* sisa tembakan silencer saat dipasang    */
#define WATT_SCALE_SCOPE    0.18
#define WATT_SCALE_SILENCER 0.10
#define WATT_SCALE_MAG      0.22

#define DIALOG_WATT_INSTALL 2962
#define DIALOG_WATT_REMOVE  2963

enum E_WATT
{
    waPistolSil,        /* silencer terpasang di pistol            */
    waPistolMag,        /* magasin ekstra di pistol                */
    waRifleScope,       /* scope di senapan/sniper                 */
    waRifleMag          /* magasin ekstra di senapan/sniper        */
}

new gWAtt[MAX_PLAYERS][E_WATT];
new gWAttSilShots[MAX_PLAYERS];     /* sisa tembakan silencer terpasang */
new gWAttShotCounter[MAX_PLAYERS];  /* penghitung wear untuk magasin ekstra */

forward Float:WeaponAttach_ScopeMultiplier(playerid, weaponid);

/* ---------------------------------------------------------- kategori */

stock bool:WeaponAttach_IsPistol(weaponid)
{
    return (weaponid >= 22 && weaponid <= 24) ? true : false;
}

stock bool:WeaponAttach_IsRifle(weaponid)
{
    return ((weaponid >= 30 && weaponid <= 31) || (weaponid >= 33 && weaponid <= 34)) ? true : false;
}

stock WeaponAttach_HasScope(playerid)        return gWAtt[playerid][waRifleScope];
stock WeaponAttach_HasSilencer(playerid)     return gWAtt[playerid][waPistolSil];
stock WeaponAttach_HasMag(playerid, weaponid)
{
    if (WeaponAttach_IsPistol(weaponid)) return gWAtt[playerid][waPistolMag];
    if (WeaponAttach_IsRifle(weaponid)) return gWAtt[playerid][waRifleMag];
    return 0;
}

/* Efek scope: dipanggil damage_pipeline untuk kedua jalur (zombie & PvP). */
stock Float:WeaponAttach_ScopeMultiplier(playerid, weaponid)
{
    if (gWAtt[playerid][waRifleScope] == 0) return 1.0;
    if (!WeaponAttach_IsRifle(weaponid)) return 1.0;
    return WATT_SCOPE_MULT;
}

/* Efek magasin ekstra: tembakan ke-N tidak mengikis kondisi senjata.
   Return true = lewati Inventory_DamageWeapon. */
stock bool:WeaponAttach_ShouldSkipWear(playerid, weaponid)
{
    if (!WeaponAttach_HasMag(playerid, weaponid)) return false;
    gWAttShotCounter[playerid]++;
    if (gWAttShotCounter[playerid] % WATT_MAG_EVERY == 0) return true;
    return false;
}

/* Efek silencer terpasang: dipanggil noise.inc sebelum cek suppressor di tas.
   Return true kalau tembakan jadi senyap (dan durabilitas silencer berkurang). */
stock bool:WeaponAttach_EatSilencerShot(playerid)
{
    if (gWAtt[playerid][waPistolSil] == 0) return false;

    gWAttSilShots[playerid]--;
    if (gWAttSilShots[playerid] <= 0)
    {
        gWAtt[playerid][waPistolSil] = 0;
        gWAttSilShots[playerid] = 0;
        WeaponAttach_Save(playerid);
        SendClientMessage(playerid, UI_ACCENT_WARN, "[SILENCER] Silencer di pistolmu lepas terpakai habis.");
    }
    return true;
}

/* ------------------------------------------------------------- visual */

stock WeaponAttach_ClearVisual(playerid)
{
    RemovePlayerAttachedObject(playerid, WATT_SLOT_MAIN);
    RemovePlayerAttachedObject(playerid, WATT_SLOT_MAG);
    return 1;
}

stock WeaponAttach_Attach(playerid, slot, modelid, Float:oy, Float:oz, Float:scale)
{
    SetPlayerAttachedObject(playerid, slot, modelid, WATT_BONE_HAND,
        0.0, oy, oz, 0.0, 0.0, 0.0, scale, scale, scale);
    return 1;
}

/* Dipanggil dari Player_Tick (500 ms): pasang tanggal sesuai senjata di tangan. */
stock WeaponAttach_VisualTick(playerid)
{
    if (!IsPlayerConnected(playerid)) return 0;

    new weaponid = GetPlayerWeapon(playerid);

    new wantMain = 0, wantMag = 0;
    new Float:mainOy = 0.0, Float:mainScale = 1.0;

    if (WeaponAttach_IsPistol(weaponid))
    {
        if (gWAtt[playerid][waPistolSil])
        {
            wantMain = WATT_MODEL_SILENCER;
            mainOy = 0.34;
            mainScale = WATT_SCALE_SILENCER;
        }
        if (gWAtt[playerid][waPistolMag]) wantMag = WATT_MODEL_MAG;
    }
    else if (WeaponAttach_IsRifle(weaponid))
    {
        if (gWAtt[playerid][waRifleScope])
        {
            wantMain = WATT_MODEL_SCOPE;
            mainOy = 0.22;
            mainScale = WATT_SCALE_SCOPE;
        }
        if (gWAtt[playerid][waRifleMag]) wantMag = WATT_MODEL_MAG;
    }

    /* Scope duduk di atas laras; silencer di ujung laras; magasin di bawah. */
    if (wantMain != 0)
    {
        new Float:mainOz = (wantMain == WATT_MODEL_SCOPE) ? 0.12 : 0.02;
        WeaponAttach_Attach(playerid, WATT_SLOT_MAIN, wantMain, mainOy, mainOz, mainScale);
    }
    if (wantMag != 0) WeaponAttach_Attach(playerid, WATT_SLOT_MAG, wantMag, 0.10, -0.06, WATT_SCALE_MAG);

    return 1;
}

/* ------------------------------------------------------------- /pasang */

stock WeaponAttach_ShowInstall(playerid)
{
    if (!Account_IsPlaying(playerid)) return 0;

    new body[512], line[96];
    body[0] = EOS;
    strcat(body, "Modifikasi\tKategori\tStatus\n", sizeof(body));

    new hasScope = Inventory_CountItem(playerid, ITEM_SCOPE) > 0;
    new hasMag = Inventory_CountItem(playerid, ITEM_EXTMAG) > 0;
    new hasSil = Inventory_CountItem(playerid, ITEM_SUPPRESSOR) > 0;

    format(line, sizeof(line), "Scope\tSenapan/Sniper\t%s\n",
        gWAtt[playerid][waRifleScope] ? "Terpasang" : (hasScope ? "Siap dipasang" : "Butuh Scope"));
    strcat(body, line, sizeof(body));
    format(line, sizeof(line), "Magasin Ekstra\tPistol\t%s\n",
        gWAtt[playerid][waPistolMag] ? "Terpasang" : (hasMag ? "Siap dipasang" : "Butuh Magasin"));
    strcat(body, line, sizeof(body));
    format(line, sizeof(line), "Magasin Ekstra\tSenapan/Sniper\t%s\n",
        gWAtt[playerid][waRifleMag] ? "Terpasang" : (hasMag ? "Siap dipasang" : "Butuh Magasin"));
    strcat(body, line, sizeof(body));
    format(line, sizeof(line), "Silencer\tPistol\t%s\n",
        gWAtt[playerid][waPistolSil] ? "Terpasang" : (hasSil ? "Siap dipasang" : "Butuh Suppressor"));
    strcat(body, line, sizeof(body));

    ShowPlayerDialog(playerid, DIALOG_WATT_INSTALL, DIALOG_STYLE_TABLIST_HEADERS,
        "Pasang Modifikasi Senjata", body, "Pasang", "Tutup");
    return 1;
}

stock WeaponAttach_InstallFromRow(playerid, row)
{
    switch (row)
    {
        case 0: /* Scope -> senapan */
        {
            if (gWAtt[playerid][waRifleScope]) return WeaponAttach_Already(playerid);
            if (Inventory_CountItem(playerid, ITEM_SCOPE) < 1)
            {
                SendClientMessage(playerid, UI_ACCENT_WARN, "[PASANG] Butuh item Scope. Cari di peti kota.");
                return 0;
            }
            Inventory_Consume(playerid, ITEM_SCOPE, 1, "pasang scope");
            gWAtt[playerid][waRifleScope] = 1;
            SendClientMessage(playerid, UI_ACCENT_PRIMARY, "[PASANG] Scope terpasang. Tembakan senapan/sniper 25% lebih mematikan.");
        }
        case 1: /* Magasin -> pistol */
        {
            if (gWAtt[playerid][waPistolMag]) return WeaponAttach_Already(playerid);
            if (Inventory_CountItem(playerid, ITEM_EXTMAG) < 1)
            {
                SendClientMessage(playerid, UI_ACCENT_WARN, "[PASANG] Butuh item Magasin Tambahan.");
                return 0;
            }
            Inventory_Consume(playerid, ITEM_EXTMAG, 1, "pasang magasin pistol");
            gWAtt[playerid][waPistolMag] = 1;
            SendClientMessage(playerid, UI_ACCENT_PRIMARY, "[PASANG] Magasin ekstra terpasang. Kondisi pistol terkikis lebih lambat.");
        }
        case 2: /* Magasin -> senapan */
        {
            if (gWAtt[playerid][waRifleMag]) return WeaponAttach_Already(playerid);
            if (Inventory_CountItem(playerid, ITEM_EXTMAG) < 1)
            {
                SendClientMessage(playerid, UI_ACCENT_WARN, "[PASANG] Butuh item Magasin Tambahan.");
                return 0;
            }
            Inventory_Consume(playerid, ITEM_EXTMAG, 1, "pasang magasin senapan");
            gWAtt[playerid][waRifleMag] = 1;
            SendClientMessage(playerid, UI_ACCENT_PRIMARY, "[PASANG] Magasin ekstra terpasang. Kondisi senapan terkikis lebih lambat.");
        }
        case 3: /* Silencer -> pistol */
        {
            if (gWAtt[playerid][waPistolSil]) return WeaponAttach_Already(playerid);
            if (Inventory_CountItem(playerid, ITEM_SUPPRESSOR) < 1)
            {
                SendClientMessage(playerid, UI_ACCENT_WARN, "[PASANG] Butuh item Suppressor (craft dari blueprint).");
                return 0;
            }

            /* Ambil durability sisa suppressor di tas sebagai sisa tembakan. */
            new suppSlot = Suppressor_FindSlot(playerid);
            if (suppSlot != -1)
            {
                gWAttSilShots[playerid] = PlayerInventory[playerid][suppSlot][invDurability];
                Inventory_RemoveItem(playerid, ITEM_SUPPRESSOR, 1);
            }
            else
            {
                gWAttSilShots[playerid] = WATT_SILENCER_SHOTS;
                Inventory_Consume(playerid, ITEM_SUPPRESSOR, 1, "pasang silencer");
            }
            gWAtt[playerid][waPistolSil] = 1;
            SendClientMessage(playerid, UI_ACCENT_PRIMARY, "[PASANG] Silencer menempel di pistol. Tembakan senyap, tidak menimbang tas.");
        }
        default: return 0;
    }

    PlayerInventoryDirty[playerid] = true;
    WeaponAttach_Save(playerid);
    WeaponAttach_VisualTick(playerid);
    Log_Format(LOG_INFO, "[WATT] char=%d pasang modifikasi row=%d", ActiveCharId[playerid], row);
    return 1;
}

stock WeaponAttach_Already(playerid)
{
    Toast_Show(playerid, "PASANG", "Modifikasi itu sudah terpasang. /lepas dulu kalau mau ganti.", UI_ACCENT_WARN);
    return 0;
}

/* ------------------------------------------------------------- /lepas */

stock WeaponAttach_ShowRemove(playerid)
{
    if (!Account_IsPlaying(playerid)) return 0;

    new body[384], line[96], rows = 0;
    body[0] = EOS;
    strcat(body, "Modifikasi\tKategori\tKondisi\n", sizeof(body));

    if (gWAtt[playerid][waRifleScope])
    {
        strcat(body, "Scope\tSenapan/Sniper\tBaik\n", sizeof(body));
        rows++;
    }
    if (gWAtt[playerid][waPistolMag])
    {
        strcat(body, "Magasin Ekstra\tPistol\tBaik\n", sizeof(body));
        rows++;
    }
    if (gWAtt[playerid][waRifleMag])
    {
        strcat(body, "Magasin Ekstra\tSenapan/Sniper\tBaik\n", sizeof(body));
        rows++;
    }
    if (gWAtt[playerid][waPistolSil])
    {
        format(line, sizeof(line), "Silencer\tPistol\tSisa %d tembakan\n", gWAttSilShots[playerid]);
        strcat(body, line, sizeof(body));
        rows++;
    }

    if (rows == 0)
    {
        SendClientMessage(playerid, UI_TEXT_MUTED, "[LEPAS] Tidak ada modifikasi terpasang. /pasang dulu.");
        return 0;
    }

    ShowPlayerDialog(playerid, DIALOG_WATT_REMOVE, DIALOG_STYLE_TABLIST_HEADERS,
        "Lepas Modifikasi Senjata", body, "Lepas", "Tutup");
    return 1;
}

stock WeaponAttach_RemoveFromRow(playerid, row)
{
    switch (row)
    {
        case 0:
        {
            if (!gWAtt[playerid][waRifleScope]) return 0;
            gWAtt[playerid][waRifleScope] = 0;
            Inventory_Grant(playerid, ITEM_SCOPE, 1, "lepas scope");
        }
        case 1:
        {
            if (!gWAtt[playerid][waPistolMag]) return 0;
            gWAtt[playerid][waPistolMag] = 0;
            Inventory_Grant(playerid, ITEM_EXTMAG, 1, "lepas magasin pistol");
        }
        case 2:
        {
            if (!gWAtt[playerid][waRifleMag]) return 0;
            gWAtt[playerid][waRifleMag] = 0;
            Inventory_Grant(playerid, ITEM_EXTMAG, 1, "lepas magasin senapan");
        }
        case 3:
        {
            if (!gWAtt[playerid][waPistolSil]) return 0;
            gWAtt[playerid][waPistolSil] = 0;
            /* Silencer kembali ke tas dengan sisa tembakannya sebagai durability. */
            new shots = (gWAttSilShots[playerid] > 0) ? gWAttSilShots[playerid] : 1;
            Inventory_Grant(playerid, ITEM_SUPPRESSOR, 1, "lepas silencer", shots);
            gWAttSilShots[playerid] = 0;
        }
        default: return 0;
    }

    PlayerInventoryDirty[playerid] = true;
    WeaponAttach_Save(playerid);
    WeaponAttach_ClearVisual(playerid);
    WeaponAttach_VisualTick(playerid);
    SendClientMessage(playerid, UI_ACCENT_PRIMARY, "[LEPAS] Modifikasi dilepas dan kembali ke tas.");
    return 1;
}

/* ------------------------------------------------------------- dialog */

forward WeaponAttach_OnDialogResponse(playerid, dialogid, response, listitem, inputtext[]);

public WeaponAttach_OnDialogResponse(playerid, dialogid, response, listitem, inputtext[])
{
    #pragma unused inputtext

    switch (dialogid)
    {
        case DIALOG_WATT_INSTALL:
        {
            if (response) WeaponAttach_InstallFromRow(playerid, listitem);
            return 1;
        }
        case DIALOG_WATT_REMOVE:
        {
            if (response) WeaponAttach_RemoveFromRow(playerid, listitem);
            return 1;
        }
    }
    return 0;
}

/* -------------------------------------------------------- persistensi */

stock WeaponAttach_Load(playerid)
{
    gWAtt[playerid][waPistolSil] = 0;
    gWAtt[playerid][waPistolMag] = 0;
    gWAtt[playerid][waRifleScope] = 0;
    gWAtt[playerid][waRifleMag] = 0;
    gWAttSilShots[playerid] = 0;
    gWAttShotCounter[playerid] = 0;
    if (ActiveCharId[playerid] == 0 || !SQL_IsReady()) return 0;

    new query[160];
    mysql_format(gSQL, query, sizeof(query),
        "SELECT watt_pistol_sil, watt_pistol_mag, watt_rifle_scope, watt_rifle_mag, watt_sil_shots \
         FROM characters WHERE id = %d AND server_id = %d LIMIT 1",
        ActiveCharId[playerid], Server_InstanceId());
    mysql_tquery(gSQL, query, "WeaponAttach_OnLoaded", "i", playerid);
    return 1;
}

forward WeaponAttach_OnLoaded(playerid);

public WeaponAttach_OnLoaded(playerid)
{
    if (!IsPlayerConnected(playerid)) return 0;
    if (cache_num_rows() == 0) return 0;

    cache_get_value_name_int(0, "watt_pistol_sil", gWAtt[playerid][waPistolSil]);
    cache_get_value_name_int(0, "watt_pistol_mag", gWAtt[playerid][waPistolMag]);
    cache_get_value_name_int(0, "watt_rifle_scope", gWAtt[playerid][waRifleScope]);
    cache_get_value_name_int(0, "watt_rifle_mag", gWAtt[playerid][waRifleMag]);
    cache_get_value_name_int(0, "watt_sil_shots", gWAttSilShots[playerid]);

    /* Baris lama tanpa data: jangan bangunkan silencer tanpa durabilitas. */
    if (gWAtt[playerid][waPistolSil] && gWAttSilShots[playerid] <= 0)
    {
        gWAtt[playerid][waPistolSil] = 0;
    }
    return 1;
}

stock WeaponAttach_Save(playerid)
{
    if (ActiveCharId[playerid] == 0 || !SQL_IsReady()) return 0;

    new query[256];
    mysql_format(gSQL, query, sizeof(query),
        "UPDATE characters SET watt_pistol_sil = %d, watt_pistol_mag = %d, \
         watt_rifle_scope = %d, watt_rifle_mag = %d, watt_sil_shots = %d WHERE id = %d",
        gWAtt[playerid][waPistolSil], gWAtt[playerid][waPistolMag],
        gWAtt[playerid][waRifleScope], gWAtt[playerid][waRifleMag],
        gWAttSilShots[playerid], ActiveCharId[playerid]);
    mysql_tquery(gSQL, query, "", "");
    return 1;
}

stock WeaponAttach_Reset(playerid)
{
    gWAtt[playerid][waPistolSil] = 0;
    gWAtt[playerid][waPistolMag] = 0;
    gWAtt[playerid][waRifleScope] = 0;
    gWAtt[playerid][waRifleMag] = 0;
    gWAttSilShots[playerid] = 0;
    gWAttShotCounter[playerid] = 0;
    WeaponAttach_ClearVisual(playerid);
    return 1;
}

/* ------------------------------------------------------------ selftest */

stock WeaponAttach_Selftest()
{
    new bool:ok = true;

    ok = ok && Item_IsValid(ITEM_SCOPE) && Item_IsValid(ITEM_EXTMAG);

    /* Kategori senjata tidak boleh tumpang tindih. */
    ok = ok && WeaponAttach_IsPistol(22) && WeaponAttach_IsPistol(24) && !WeaponAttach_IsPistol(25);
    ok = ok && WeaponAttach_IsRifle(30) && WeaponAttach_IsRifle(34) && !WeaponAttach_IsRifle(29) && !WeaponAttach_IsRifle(35);

    /* Multiplier scope hanya untuk senapan. */
    ok = ok && WeaponAttach_ScopeMultiplier(0, 22) == 1.0;   /* pistol: tidak */
    gWAtt[0][waRifleScope] = 1;
    ok = ok && WeaponAttach_ScopeMultiplier(0, 31) == WATT_SCOPE_MULT;
    ok = ok && WeaponAttach_ScopeMultiplier(0, 22) == 1.0;
    gWAtt[0][waRifleScope] = 0;

    /* Magasin: tembakan ke-4 bebas wear, lalu pola berulang. */
    gWAtt[0][waPistolMag] = 1;
    new skipped = 0;
    for (new shot = 0; shot < 8; shot++)
    {
        if (WeaponAttach_ShouldSkipWear(0, 22)) skipped++;
    }
    ok = ok && skipped == 2;   /* tembakan ke-4 dan ke-8 */
    gWAtt[0][waPistolMag] = 0;
    gWAttShotCounter[0] = 0;

    Selftest_Record(ok, "modifikasi senjata",
        ok ? "scope/magasin/silencer konsisten" : "attachment system gagal");
    return 1;
}
