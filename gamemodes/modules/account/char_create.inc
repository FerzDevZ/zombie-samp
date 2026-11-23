/* Pembuatan karakter berlapis: nama -> gender -> umur -> ras -> asal ->
   pekerjaan -> trait -> penampilan -> ringkasan.

   Semua langkah memakai dialog daftar (mobile-safe: tidak ada input angka),
   hanya nama karakter yang diketik.

   Efek pilihan harus NYATA, jadi tiap langkah hanya memberi hal yang memang
   dipakai sistem: uang awal, item awal (lewat Inventory_Grant), XP, poin skill,
   dan HP awal. Ras sengaja tidak memberi bonus statistik — dia identitas.

   Item awal tidak bisa diberikan saat INSERT karena `Inventory_Load` baru jalan
   setelah spawn dan akan menimpa memori. Karena itu pekerjaan disimpan di
   `PendingKitJob` dan diberikan setelah `Inventory_OnLoaded` selesai. */

#define DIALOG_CC_NAME     2010
#define DIALOG_CC_GENDER   2011
#define DIALOG_CC_AGE      2012
#define DIALOG_CC_RACE     2013
#define DIALOG_CC_ORIGIN   2014
#define DIALOG_CC_JOB      2015
#define DIALOG_CC_TRAIT    2016
#define DIALOG_CC_LOOK     2017
#define DIALOG_CC_CONFIRM  2018

#define CC_GENDER_COUNT    2
#define CC_AGE_COUNT       5
#define CC_RACE_COUNT      7
#define CC_ORIGIN_COUNT    7
#define CC_JOB_COUNT       8
#define CC_TRAIT_COUNT     8
#define CC_LOOK_COUNT      8
#define CC_KIT_SLOTS       3
#define CC_CASH_BASE       500
#define CC_CASH_MIN        100

new const CCAgeNames[CC_AGE_COUNT][24] = {
    "18-24 (muda)", "25-34 (prima)", "35-49 (veteran)", "50-64 (senior)", "65+ (lanjut)"
};

new const CCAgeMin[CC_AGE_COUNT] = {18, 25, 35, 50, 65};

new const CCRaceNames[CC_RACE_COUNT][24] = {
    "Kaukasia", "Afrika", "Asia Timur", "Asia Selatan", "Hispanik", "Timur Tengah", "Pribumi/Pasifik"
};

new const CCOriginNames[CC_ORIGIN_COUNT][28] = {
    "Kota besar", "Pinggiran kota", "Pedesaan", "Kamp militer", "Kapal pengungsi", "Korporat", "Tunawisma"
};

new const CCOriginCash[CC_ORIGIN_COUNT] = {150, 50, -100, 250, -200, 400, -250};

new const CCOriginDesc[CC_ORIGIN_COUNT][56] = {
    "Terbiasa hiruk pikuk, uang lebih banyak",
    "Tumbuh di gang sempit, cukup uang",
    "Kebun dan ternak, uang pas-pasan",
    "Disiplin dan tahan tekanan, uang lebih",
    "Hanya bawa diri, uang sangat sedikit",
    "Koneksi dan tabungan, uang paling banyak",
    "Jalanan mengajarkan bertahan, tanpa uang"
};

new const CCJobNames[CC_JOB_COUNT][20] = {
    "Dokter", "Mekanik", "Polisi", "Petani", "Nelayan", "Kurir", "Insinyur", "Pengangguran"
};

new const CCJobDesc[CC_JOB_COUNT][64] = {
    "Medkit, vaksin, perban", "Toolbox, repair kit, scrap",
    "Amunisi pistol, perban, energi", "Makanan, herbal, air",
    "Makanan, air, kain", "Energi, air, scrap",
    "Elektronik, blueprint defib, kimia", "Makanan, air, scrap"
};

new const CCJobCash[CC_JOB_COUNT] = {100, 50, 75, -50, -50, 0, 100, -150};
new const CCJobXp[CC_JOB_COUNT] = {50, 25, 50, 0, 0, 25, 75, 0};
new const CCJobKitItem[CC_JOB_COUNT][CC_KIT_SLOTS] = {
    {ITEM_MEDKIT,     ITEM_VACCINE,       ITEM_BANDAGE},
    {ITEM_TOOLBOX,    ITEM_REPAIR_KIT,    ITEM_SCRAP},
    {ITEM_PISTOL_AMMO, ITEM_BANDAGE,      ITEM_ENERGY_DRINK},
    {ITEM_FOOD_CAN,   ITEM_HERB,          ITEM_WATER_BOTTLE},
    {ITEM_FOOD_CAN,   ITEM_WATER_BOTTLE,  ITEM_CLOTH},
    {ITEM_ENERGY_DRINK, ITEM_WATER_BOTTLE, ITEM_SCRAP},
    {ITEM_ELECTRONICS, ITEM_BLUEPRINT_DEFIB, ITEM_CHEMICAL},
    {ITEM_FOOD_CAN,   ITEM_WATER_BOTTLE,  ITEM_SCRAP}
};
new const CCJobKitAmount[CC_JOB_COUNT][CC_KIT_SLOTS] = {
    {2, 1, 3},
    {1, 1, 5},
    {30, 2, 1},
    {3, 3, 2},
    {4, 3, 2},
    {2, 2, 2},
    {3, 1, 2},
    {1, 1, 3}
};

new const CCTraitNames[CC_TRAIT_COUNT][20] = {
    "Kuat", "Cepat", "Tahan banting", "Cerdas", "Sosial", "Pendiam", "Beruntung", "Waspada"
};

new const CCTraitDesc[CC_TRAIT_COUNT][56] = {
    "HP awal +10", "Poin skill +1, HP -5",
    "HP awal +15", "Poin skill +2",
    "XP awal +100", "XP awal +50, HP +5",
    "Uang awal +200", "Poin skill +1, XP +50"
};

new const CCLookMale[CC_LOOK_COUNT] = {59, 60, 250, 166, 228, 230, 207, 187};
new const CCLookFemale[CC_LOOK_COUNT] = {192, 193, 12, 169, 190, 199, 226, 141};

/* Data pembuatan yang sedang berjalan. */
new CCGender[MAX_PLAYERS];
new CCAge[MAX_PLAYERS];
new CCRace[MAX_PLAYERS];
new CCOrigin[MAX_PLAYERS];
new CCJob[MAX_PLAYERS];
new CCTrait[MAX_PLAYERS];
new CCLook[MAX_PLAYERS];
new CCName[MAX_PLAYERS][MAX_NAME_LENGTH + 1];
new PendingKitJob[MAX_PLAYERS];

/* Profil karakter yang sudah jadi, dipakai /profil. */
new CharGender[MAX_PLAYERS];
new CharAge[MAX_PLAYERS];
new CharRace[MAX_PLAYERS];
new CharOrigin[MAX_PLAYERS];
new CharJob[MAX_PLAYERS];
new CharTrait[MAX_PLAYERS];
new CharLook[MAX_PLAYERS];

stock CharCreate_Reset(playerid)
{
    CCGender[playerid] = 0;
    CCAge[playerid] = 0;
    CCRace[playerid] = 0;
    CCOrigin[playerid] = 0;
    CCJob[playerid] = 0;
    CCTrait[playerid] = 0;
    CCLook[playerid] = 0;
    CCName[playerid][0] = EOS;
    PendingKitJob[playerid] = 0;
    CharGender[playerid] = 0;
    CharAge[playerid] = 0;
    CharRace[playerid] = 0;
    CharOrigin[playerid] = 0;
    CharJob[playerid] = 0;
    CharTrait[playerid] = 0;
    CharLook[playerid] = 0;
    return 1;
}

stock CharCreate_SkinFor(gender, look)
{
    if (look < 0 || look >= CC_LOOK_COUNT) look = 0;
    return (gender == 1) ? CCLookFemale[look] : CCLookMale[look];
}

stock CharCreate_StartKitItem(job, slot)
{
    if (job < 0 || job >= CC_JOB_COUNT || slot < 0 || slot >= CC_KIT_SLOTS) return ITEM_NONE_ID;
    return CCJobKitItem[job][slot];
}

stock CharCreate_StartKitAmount(job, slot)
{
    if (job < 0 || job >= CC_JOB_COUNT || slot < 0 || slot >= CC_KIT_SLOTS) return 0;
    return CCJobKitAmount[job][slot];
}

stock CharCreate_StartCash(job, origin)
{
    if (job < 0 || job >= CC_JOB_COUNT) job = 0;
    if (origin < 0 || origin >= CC_ORIGIN_COUNT) origin = 0;

    new cash = CC_CASH_BASE + CCJobCash[job] + CCOriginCash[origin];
    if (cash < CC_CASH_MIN) cash = CC_CASH_MIN;
    return cash;
}

stock CharCreate_StartHealth(gender, age, trait)
{
    new Float:health = 100.0;
    #pragma unused gender

    switch (trait)
    {
        case 0: health += 10.0;          /* Kuat */
        case 1: health -= 5.0;           /* Cepat */
        case 2: health += 15.0;          /* Tahan banting */
        case 5: health += 5.0;           /* Pendiam */
    }

    switch (age)
    {
        case 1: health += 5.0;
        case 3: health -= 5.0;
        case 4: health -= 15.0;
    }

    if (health < 50.0) health = 50.0;
    if (health > 125.0) health = 125.0;
    return floatround(health);
}

stock CharCreate_StartXp(job, trait)
{
    if (job < 0 || job >= CC_JOB_COUNT) job = 0;

    new xp = CCJobXp[job];
    switch (trait)
    {
        case 4: xp += 100;
        case 5: xp += 50;
        case 7: xp += 50;
    }
    return xp;
}

stock CharCreate_StartSkillPoints(job, age, trait)
{
    #pragma unused job

    new points = 0;
    switch (age)
    {
        case 0: points += 1;
        case 3: points += 2;
    }

    switch (trait)
    {
        case 1: points += 1;
        case 3: points += 2;
        case 7: points += 1;
    }
    return points;
}

stock CharCreate_StartCashTrait(job, origin, trait)
{
    new cash = CharCreate_StartCash(job, origin);
    if (trait == 6) cash += 200;
    return cash;
}

/* ---------- tampilan langkah ---------- */

stock CharCreate_Start(playerid)
{
    CharCreate_Reset(playerid);
    SendClientMessage(playerid, UI_ACCENT_PRIMARY, "[KARAKTER] Contoh nama yang benar: Arif_Pratama | Siti_Rahma | Budi_88 | Vira_Sanjaya");
    SendClientMessage(playerid, UI_TEXT_SECONDARY, "[KARAKTER] Contoh satu karakter utuh: Arif_Pratama / Pria / 25-34 / Asia Timur / Kota besar / Mekanik / Tahan banting / Model 2");
    SendClientMessage(playerid, UI_TEXT_SECONDARY, "[KARAKTER] Dialog tidak muncul di HP? Ketik /akun untuk membuka ulang dari langkah awal.");
    CharCreate_ShowName(playerid);
    return 1;
}

stock CharCreate_ShowName(playerid)
{
#if !AUTH_INPUT_DIALOGS
    SendClientMessage(playerid, UI_ACCENT_INFO, "[KARAKTER] Langkah 1/8 (nama): ketik di chat /nama Namamu   contoh: /nama Arif_Pratama");
    return 1;
#else
    if (ChatAuthOnly[playerid])
    {
        SendClientMessage(playerid, UI_ACCENT_INFO, "[KARAKTER] Langkah 1/8 (nama): ketik di chat /nama Namamu   contoh: /nama Arif_Pratama");
        return 1;
    }

    ShowPlayerDialog(playerid, DIALOG_CC_NAME, DIALOG_STYLE_INPUT,
        "Karakter Baru 1/8 - Nama",
        "Masukkan nama karakter.\n\n\
         Contoh nama yang BENAR:\n\
          Arif_Pratama\n\
          Siti_Rahma\n\
          Budi_88\n\
          Vira_Sanjaya\n\n\
         Contoh yang SALAH:\n\
          arif pratama   (pakai spasi)\n\
          A   (terlalu pendek)\n\
          Arif-Pratama   (pakai strip)\n\n\
         Aturan: 3-20 karakter, hanya huruf, angka, garis bawah.\n\
         Nama ini dilihat pemain lain dan tidak bisa diganti sendiri.",
        "Lanjut", "Batal");
    return 1;
#endif
}

stock CharCreate_ShowGender(playerid)
{
    new body[256];
    format(body, sizeof(body),
        "Gender\tKeterangan\n\
         Pria\tModel tubuh pria (HP awal sama, tanpa bonus gender)\n\
         Wanita\tModel tubuh wanita (HP awal sama, tanpa bonus gender)\n");

    ShowPlayerDialog(playerid, DIALOG_CC_GENDER, DIALOG_STYLE_TABLIST_HEADERS,
        "Karakter Baru 2/8 - Gender (contoh: pilih Wanita untuk karakter perempuan)", body, "Pilih", "Sebelumnya");
    return 1;
}

stock CharCreate_ShowAge(playerid)
{
    new body[512];
    format(body, sizeof(body),
        "Umur\tBonus\n\
         %s\tPoin skill +1\n\
         %s\tHP awal +5\n\
         %s\tXP awal +75\n\
         %s\tPoin skill +2, HP -5\n\
         %s\tUang +300, HP -15\n",
        CCAgeNames[0], CCAgeNames[1], CCAgeNames[2], CCAgeNames[3], CCAgeNames[4]);

    ShowPlayerDialog(playerid, DIALOG_CC_AGE, DIALOG_STYLE_TABLIST_HEADERS,
        "Karakter Baru 3/8 - Umur (contoh: 25-34 = HP awal +5)", body, "Pilih", "Sebelumnya");
    return 1;
}

stock CharCreate_ShowRace(playerid)
{
    new body[512], line[96];
    body[0] = EOS;
    strcat(body, "Ras\tKeterangan\n", sizeof(body));

    for (new index = 0; index < CC_RACE_COUNT; index++)
    {
        format(line, sizeof(line), "%s\tIdentitas saja, tanpa bonus status\n", CCRaceNames[index]);
        strcat(body, line, sizeof(body));
    }

    ShowPlayerDialog(playerid, DIALOG_CC_RACE, DIALOG_STYLE_TABLIST_HEADERS,
        "Karakter Baru 4/8 - Ras (contoh: Asia Timur untuk wajah oriental; hanya identitas)", body, "Pilih", "Sebelumnya");
    return 1;
}

stock CharCreate_ShowOrigin(playerid)
{
    new body[640], line[112];
    body[0] = EOS;
    strcat(body, "Asal\tKeterangan\n", sizeof(body));

    for (new index = 0; index < CC_ORIGIN_COUNT; index++)
    {
        format(line, sizeof(line), "%s\t%s (%+d uang)\n", CCOriginNames[index], CCOriginDesc[index], CCOriginCash[index]);
        strcat(body, line, sizeof(body));
    }

    ShowPlayerDialog(playerid, DIALOG_CC_ORIGIN, DIALOG_STYLE_TABLIST_HEADERS,
        "Karakter Baru 5/8 - Asal (contoh: Korporat = uang awal +400)", body, "Pilih", "Sebelumnya");
    return 1;
}

stock CharCreate_ShowJob(playerid)
{
    new body[640], line[112];
    body[0] = EOS;
    strcat(body, "Pekerjaan\tKeterangan\n", sizeof(body));

    for (new index = 0; index < CC_JOB_COUNT; index++)
    {
        format(line, sizeof(line), "%s\t%s\n", CCJobNames[index], CCJobDesc[index]);
        strcat(body, line, sizeof(body));
    }

    ShowPlayerDialog(playerid, DIALOG_CC_JOB, DIALOG_STYLE_TABLIST_HEADERS,
        "Karakter Baru 6/8 - Pekerjaan (contoh: Mekanik = Toolbox + Repair Kit + Scrap)", body, "Pilih", "Sebelumnya");
    return 1;
}

stock CharCreate_ShowTrait(playerid)
{
    new body[640], line[112];
    body[0] = EOS;
    strcat(body, "Keahlian\tKeterangan\n", sizeof(body));

    for (new index = 0; index < CC_TRAIT_COUNT; index++)
    {
        format(line, sizeof(line), "%s\t%s\n", CCTraitNames[index], CCTraitDesc[index]);
        strcat(body, line, sizeof(body));
    }

    ShowPlayerDialog(playerid, DIALOG_CC_TRAIT, DIALOG_STYLE_TABLIST_HEADERS,
        "Karakter Baru 7/8 - Keahlian (contoh: Tahan banting = HP awal +15)", body, "Pilih", "Sebelumnya");
    return 1;
}

stock CharCreate_ShowLook(playerid)
{
    new body[384];
    format(body, sizeof(body),
        "Model\tKeterangan\n\
         Model 1\tPenampilan sipil\n\
         Model 2\tPenampilan pekerja\n\
         Model 3\tPenampilan jalanan\n\
         Model 4\tPenampilan lusuh\n\
         Model 5\tPenampilan gelap\n\
         Model 6\tPenampilan santai\n\
         Model 7\tPenampilan formal\n\
         Model 8\tPenampilan kasar\n");

    ShowPlayerDialog(playerid, DIALOG_CC_LOOK, DIALOG_STYLE_TABLIST_HEADERS,
        "Karakter Baru 8/8 - Penampilan (contoh: Model 2 untuk buruh/mekanik)", body, "Pilih", "Sebelumnya");
    return 1;
}

stock CharCreate_ShowConfirm(playerid)
{
    new genLabel[16], lookLabel[16];
    format(genLabel, sizeof(genLabel), "%s", (CCGender[playerid] == 1) ? "Wanita" : "Pria");
    format(lookLabel, sizeof(lookLabel), "Model %d", CCLook[playerid] + 1);

    new body[768];
    format(body, sizeof(body),
        "Data\tNilai\n\
         Nama\t%s\n\
         Gender\t%s\n\
         Umur\t%s (%d+)\n\
         Ras\t%s\n\
         Asal\t%s\n\
         Pekerjaan\t%s\n\
         Keahlian\t%s\n\
         Penampilan\t%s\n\
         Uang awal\t$%d\n\
         HP awal\t%d\n\
         XP awal\t%d\n\
         Poin skill\t%d\n",
        CCName[playerid], genLabel, CCAgeNames[CCAge[playerid]], CCAgeMin[CCAge[playerid]],
        CCRaceNames[CCRace[playerid]], CCOriginNames[CCOrigin[playerid]],
        CCJobNames[CCJob[playerid]], CCTraitNames[CCTrait[playerid]], lookLabel,
        CharCreate_StartCashTrait(CCJob[playerid], CCOrigin[playerid], CCTrait[playerid]),
        CharCreate_StartHealth(CCGender[playerid], CCAge[playerid], CCTrait[playerid]),
        CharCreate_StartXp(CCJob[playerid], CCTrait[playerid]),
        CharCreate_StartSkillPoints(CCJob[playerid], CCAge[playerid], CCTrait[playerid]));

    ShowPlayerDialog(playerid, DIALOG_CC_CONFIRM, DIALOG_STYLE_TABLIST_HEADERS,
        "Ringkasan Karakter", body, "Buat Karakter", "Ulangi");
    return 1;
}
/* ---------- simpan & terapkan ---------- */

stock CharCreate_Create(playerid)
{
    if (!Account_IsValidCharacterName(CCName[playerid]))
    {
        SendClientMessage(playerid, UI_ACCENT_WARN, "[KARAKTER] Nama tidak valid. Contoh benar: Arif_Pratama (3-20 huruf/angka/garis bawah, tanpa spasi).");
        CharCreate_ShowName(playerid);
        return 0;
    }

    new slot = PendingCharacterSlot[playerid];
    if (slot < 0 || slot >= MAX_CHARACTER_SLOTS) slot = CharacterCount[playerid];

    new skin = CharCreate_SkinFor(CCGender[playerid], CCLook[playerid]);
    new cash = CharCreate_StartCashTrait(CCJob[playerid], CCOrigin[playerid], CCTrait[playerid]);
    new health = CharCreate_StartHealth(CCGender[playerid], CCAge[playerid], CCTrait[playerid]);
    new xp = CharCreate_StartXp(CCJob[playerid], CCTrait[playerid]);
    new points = CharCreate_StartSkillPoints(CCJob[playerid], CCAge[playerid], CCTrait[playerid]);

    new query[512];
    mysql_format(gSQL, query, sizeof(query),
        "INSERT INTO characters \
         (account_id, server_id, slot, name, skin, gender, age, race, origin, occupation, trait, appearance, cash, health, xp, skill_points) \
         VALUES (%d, %d, %d, '%e', %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d)",
        AccountId[playerid], Server_InstanceId(), slot, CCName[playerid], skin,
        CCGender[playerid], CCAgeMin[CCAge[playerid]], CCRace[playerid], CCOrigin[playerid],
        CCJob[playerid], CCTrait[playerid], CCLook[playerid],
        cash, health, xp, points);
    mysql_tquery(gSQL, query, "CharCreate_OnCreated", "i", playerid);
    return 1;
}

public CharCreate_OnCreated(playerid)
{
    if (!IsPlayerConnected(playerid)) return 0;

    new createdId = cache_insert_id();
    if (createdId == 0)
    {
        SendClientMessage(playerid, UI_ACCENT_DANGER, "[KARAKTER] Nama sudah dipakai. Coba nama lain.");
        CharCreate_ShowName(playerid);
        return 1;
    }

    ActiveCharId[playerid] = createdId;
    /* Kit pekerjaan diberikan setelah Inventory_Load selesai (lihat ApplyPending). */
    PendingKitJob[playerid] = CCJob[playerid] + 1;

    Log_Format(LOG_INFO,
        "[KARAKTER] id %d dibuat akun %d: gender=%d umur=%d ras=%d asal=%d kerja=%d trait=%d tampilan=%d",
        createdId, AccountId[playerid], CCGender[playerid], CCAgeMin[CCAge[playerid]],
        CCRace[playerid], CCOrigin[playerid], CCJob[playerid], CCTrait[playerid], CCLook[playerid]);

    Account_LoadCharacterData(playerid);
    return 1;
}

/* Dipanggil dari Inventory_OnLoaded: inventory baru benar-benar ada di memori. */
stock CharCreate_ApplyPending(playerid)
{
    if (PendingKitJob[playerid] == 0) return 0;

    new job = PendingKitJob[playerid] - 1;
    PendingKitJob[playerid] = 0;

    if (job < 0 || job >= CC_JOB_COUNT) return 0;

    new granted = 0;
    for (new slot = 0; slot < CC_KIT_SLOTS; slot++)
    {
        new itemid = CharCreate_StartKitItem(job, slot);
        new amount = CharCreate_StartKitAmount(job, slot);
        if (!Item_IsValid(itemid) || amount <= 0) continue;

        /* Tas penuh bukan alasan membatalkan karakter: item yang tidak muat
           dilewati, dan yang berhasil tetap tercatat di ledger. */
        if (Inventory_Grant(playerid, itemid, amount, "char_start")) granted++;
    }

    new line[112];
    format(line, sizeof(line), "[KARAKTER] Kit %s: %d/%d jenis item masuk tas. Cek /inv.",
        CCJobNames[job], granted, CC_KIT_SLOTS);
    SendClientMessage(playerid, UI_ACCENT_PRIMARY, line);
    SendClientMessage(playerid, UI_TEXT_SECONDARY, "[KARAKTER] Detail identitas: /profil");
    return 1;
}

stock CharCreate_LoadProfile(playerid, gender, age, race, origin, job, trait, look)
{
    CharGender[playerid] = gender;
    CharAge[playerid] = age;
    CharRace[playerid] = race;
    CharOrigin[playerid] = origin;
    CharJob[playerid] = job;
    CharTrait[playerid] = trait;
    CharLook[playerid] = look;
    return 1;
}

stock CharCreate_Label(index, const names[][], count, dest[], size)
{
    if (index < 0 || index >= count)
    {
        format(dest, size, "-");
        return 0;
    }
    format(dest, size, "%s", names[index]);
    return 1;
}

stock CharCreate_Describe(playerid)
{
    if (!Account_IsPlaying(playerid)) return 0;

    new genderLabel[16], raceLabel[28], originLabel[28], jobLabel[20], traitLabel[20], line[144];
    format(genderLabel, sizeof(genderLabel), "%s", (CharGender[playerid] == 1) ? "Wanita" : "Pria");
    CharCreate_Label(CharRace[playerid], CCRaceNames, CC_RACE_COUNT, raceLabel, sizeof(raceLabel));
    CharCreate_Label(CharOrigin[playerid], CCOriginNames, CC_ORIGIN_COUNT, originLabel, sizeof(originLabel));
    CharCreate_Label(CharJob[playerid], CCJobNames, CC_JOB_COUNT, jobLabel, sizeof(jobLabel));
    CharCreate_Label(CharTrait[playerid], CCTraitNames, CC_TRAIT_COUNT, traitLabel, sizeof(traitLabel));

    SendClientMessage(playerid, UI_ACCENT_PRIMARY, "--- Profil karakter ---");
    format(line, sizeof(line), "Gender: %s | Umur: %d | Ras: %s", genderLabel, CharAge[playerid], raceLabel);
    SendClientMessage(playerid, UI_TEXT_SECONDARY, line);
    format(line, sizeof(line), "Asal: %s | Pekerjaan: %s | Keahlian: %s", originLabel, jobLabel, traitLabel);
    SendClientMessage(playerid, UI_TEXT_SECONDARY, line);
    return 1;
}

stock CharCreate_SetNameFromCommand(playerid, const typedName[])
{
    if (!Account_IsAuthed(playerid))
    {
        SendClientMessage(playerid, UI_ACCENT_WARN, "[KARAKTER] Selesaikan /daftar atau /masuk dulu.");
        return 0;
    }
    if (Account_IsPlaying(playerid))
    {
        SendClientMessage(playerid, UI_ACCENT_WARN, "[KARAKTER] Kamu sudah punya karakter aktif. Pakai /akun untuk menambah karakter.");
        return 0;
    }

    if (!Account_IsValidCharacterName(typedName))
    {
        SendClientMessage(playerid, UI_ACCENT_WARN, "[KARAKTER] Nama tidak valid. Contoh benar: Arif_Pratama (3-20 huruf/angka/garis bawah, tanpa spasi).");
        return 0;
    }

    format(CCName[playerid], sizeof(CCName[]), "%s", typedName);
    SendClientMessage(playerid, UI_ACCENT_PRIMARY, "[KARAKTER] Nama disimpan. Lanjut pilih gender di dialog.");
    CharCreate_ShowGender(playerid);
    return 1;
}

/* ---------- alur dialog ---------- */

stock bool:CharCreate_OnDialogResponse(playerid, dialogid, response, listitem, const inputtext[])
{
    /* Wizard hanya untuk sesi terautentikasi: tanpa ini, spoof dialog
       2010-2018 bisa membuat & memainkan karakter di akun orang lain
       (AccountId terisi saat lookup nick, sebelum password). */
    if (!Account_IsAuthed(playerid) || Account_IsPlaying(playerid)) return false;

    switch (dialogid)
    {
        case DIALOG_CC_NAME:
        {
            if (!response)
            {
                if (CharacterCount[playerid] > 0) Account_LoadCharacters(playerid);
                else
                {
                    Account_MarkChatAuth(playerid);
                    SendClientMessage(playerid, UI_TEXT_SECONDARY, "[KARAKTER] Ketik di chat: /nama Namamu   contoh: /nama Arif_Pratama");
                }
                return true;
            }

            new typedName[MAX_NAME_LENGTH + 1];
            if (inputtext[0] == EOS || sscanf(inputtext, "s[21]", typedName) != 1 || !Account_IsValidCharacterName(typedName))
            {
                Account_MarkChatAuth(playerid);
                SendClientMessage(playerid, UI_ACCENT_WARN, "[KARAKTER] Input dialog kosong/tidak valid (klien mobile).");
                SendClientMessage(playerid, UI_ACCENT_INFO, "[KARAKTER] Ketik di chat: /nama Namamu   contoh: /nama Arif_Pratama");
                return true;
            }

            format(CCName[playerid], sizeof(CCName[]), "%s", typedName);
            CharCreate_ShowGender(playerid);
            return true;
        }

        case DIALOG_CC_GENDER:
        {
            if (!response)
            {
                CharCreate_ShowName(playerid);
                return true;
            }
            if (listitem < 0 || listitem >= CC_GENDER_COUNT) return true;

            CCGender[playerid] = listitem;
            CharCreate_ShowAge(playerid);
            return true;
        }

        case DIALOG_CC_AGE:
        {
            if (!response)
            {
                CharCreate_ShowGender(playerid);
                return true;
            }
            if (listitem < 0 || listitem >= CC_AGE_COUNT) return true;

            CCAge[playerid] = listitem;
            CharCreate_ShowRace(playerid);
            return true;
        }

        case DIALOG_CC_RACE:
        {
            if (!response)
            {
                CharCreate_ShowAge(playerid);
                return true;
            }
            if (listitem < 0 || listitem >= CC_RACE_COUNT) return true;

            CCRace[playerid] = listitem;
            CharCreate_ShowOrigin(playerid);
            return true;
        }

        case DIALOG_CC_ORIGIN:
        {
            if (!response)
            {
                CharCreate_ShowRace(playerid);
                return true;
            }
            if (listitem < 0 || listitem >= CC_ORIGIN_COUNT) return true;

            CCOrigin[playerid] = listitem;
            CharCreate_ShowJob(playerid);
            return true;
        }

        case DIALOG_CC_JOB:
        {
            if (!response)
            {
                CharCreate_ShowOrigin(playerid);
                return true;
            }
            if (listitem < 0 || listitem >= CC_JOB_COUNT) return true;

            CCJob[playerid] = listitem;
            CharCreate_ShowTrait(playerid);
            return true;
        }

        case DIALOG_CC_TRAIT:
        {
            if (!response)
            {
                CharCreate_ShowJob(playerid);
                return true;
            }
            if (listitem < 0 || listitem >= CC_TRAIT_COUNT) return true;

            CCTrait[playerid] = listitem;
            CharCreate_ShowLook(playerid);
            return true;
        }

        case DIALOG_CC_LOOK:
        {
            if (!response)
            {
                CharCreate_ShowTrait(playerid);
                return true;
            }
            if (listitem < 0 || listitem >= CC_LOOK_COUNT) return true;

            CCLook[playerid] = listitem;
            CharCreate_ShowConfirm(playerid);
            return true;
        }

        case DIALOG_CC_CONFIRM:
        {
            if (!response)
            {
                CharCreate_ShowLook(playerid);
                return true;
            }

            CharCreate_Create(playerid);
            return true;
        }
    }
    return false;
}
