#if defined _ls_base_build_included
    #endinput
#endif
#define _ls_base_build_included

/* Base Building: petak pribadi dengan struktur yang punya HP, disimpan di DB, dan
   dihancurkan zombie. Semua biaya lewat Inventory_Consume (ledger "build"), semua
   pengembalian lewat Inventory_Grant ("build_refund"/"build_dismantle"). */

#define DIALOG_BUILD_MAIN 2701

forward BaseBuild_OnRowsLoaded(playerid);
forward BaseBuild_OnInserted(playerid, slot);

enum E_STRUCT_SLOT
{
    ssActive,
    ssDbId,
    ssBaseId,
    ssLobby,
    ssType,
    STREAMER_TAG_OBJECT:ssObject,
    ssCollision,
    STREAMER_TAG_PICKUP:ssPickup,
    STREAMER_TAG_3D_TEXT_LABEL:ssLabel,
    Float:ssX,
    Float:ssY,
    Float:ssZ,
    Float:ssRotZ,
    ssHealth,
    ssHealthMax,
    Float:ssFuel,
    ssLastFuelSave,
    ssLastSaveTick
};

new BaseStructure[BASE_MAX_STRUCTURES][E_STRUCT_SLOT];
new BaseBuildLoadedBase[MAX_PLAYERS];
new BaseBuildLastBuildTick[MAX_PLAYERS];
/* Pending per pemain: slot global tunggal membuat dua INSERT bersamaan
   (dua pemain build di tick sama) saling menimpa assignment dbId. */
new BaseBuildPendingSlot[MAX_PLAYERS];

new const gTierUpgradeScrap[BASE_MAX_TIER + 1] = {0, 0, 20, 40};
new const gTierUpgradeWood[BASE_MAX_TIER + 1] = {0, 0, 15, 25};
new const gTierUpgradeChemical[BASE_MAX_TIER + 1] = {0, 0, 5, 0};
new const gTierUpgradeElectronics[BASE_MAX_TIER + 1] = {0, 0, 0, 10};

stock BaseBuild_ResetPlayer(playerid)
{
    BaseBuildLoadedBase[playerid] = 0;
    BaseBuildLastBuildTick[playerid] = 0;
    BaseBuildPendingSlot[playerid] = -1;
    return 1;
}

stock BaseBuild_TotalActive()
{
    new total = 0;
    for (new slot = 0; slot < BASE_MAX_STRUCTURES; slot++)
    {
        if (BaseStructure[slot][ssActive]) total++;
    }
    return total;
}

stock BaseBuild_CountType(baseId, type)
{
    new count = 0;
    for (new slot = 0; slot < BASE_MAX_STRUCTURES; slot++)
    {
        if (!BaseStructure[slot][ssActive]) continue;
        if (BaseStructure[slot][ssBaseId] != baseId) continue;
        if (BaseStructure[slot][ssType] != type) continue;
        count++;
    }
    return count;
}

stock BaseBuild_CountBase(baseId)
{
    new count = 0;
    for (new slot = 0; slot < BASE_MAX_STRUCTURES; slot++)
    {
        if (BaseStructure[slot][ssActive] && BaseStructure[slot][ssBaseId] == baseId) count++;
    }
    return count;
}

stock BaseBuild_GetIntegrity(playerid, &percent, &health, &healthMax)
{
    health = 0;
    healthMax = 0;
    percent = 0;

    new baseId = Base_Id(playerid);
    if (baseId == 0) return 0;

    for (new slot = 0; slot < BASE_MAX_STRUCTURES; slot++)
    {
        if (!BaseStructure[slot][ssActive] || BaseStructure[slot][ssBaseId] != baseId) continue;
        health += BaseStructure[slot][ssHealth];
        healthMax += BaseStructure[slot][ssHealthMax];
    }

    if (healthMax > 0) percent = (health * 100) / healthMax;
    return 1;
}

stock BaseBuild_CrateCount(playerid)
{
    if (Base_Id(playerid) == 0) return 0;
    return BaseBuild_CountType(Base_Id(playerid), STRUCT_CRATE);
}

stock BaseBuild_StashCapacity(playerid)
{
    new capacity = STASH_BASE_SLOTS + (BaseBuild_CrateCount(playerid) * BASE_CRATE_SLOTS);
    if (capacity > BASE_STASH_MAX_SLOTS) capacity = BASE_STASH_MAX_SLOTS;
    return capacity;
}

stock BaseBuild_FreeSlot()
{
    for (new slot = 0; slot < BASE_MAX_STRUCTURES; slot++)
    {
        if (!BaseStructure[slot][ssActive] && BaseStructure[slot][ssDbId] == 0) return slot;
    }
    return -1;
}

stock BaseBuild_CreateVisuals(slot)
{
    if (slot < 0 || slot >= BASE_MAX_STRUCTURES) return 0;

    new type = BaseStructure[slot][ssType];
    new structureLabel[64];

    if (type == STRUCT_GENERATOR)
    {
        format(structureLabel, sizeof(structureLabel), "%s\n{P}%d HP | BBM %.1f L",
            gStructures[type][stLabel], BaseStructure[slot][ssHealth], BaseStructure[slot][ssFuel]);
    }
    else
    {
        format(structureLabel, sizeof(structureLabel), "%s\n{P}%d HP",
            gStructures[type][stLabel], BaseStructure[slot][ssHealth]);
    }

    BaseStructure[slot][ssLabel] = CreateDynamic3DTextLabel(structureLabel, UI_ACCENT_PRIMARY,
        BaseStructure[slot][ssX], BaseStructure[slot][ssY], BaseStructure[slot][ssZ] + 1.0,
        25.0, INVALID_PLAYER_ID, INVALID_VEHICLE_ID, 0, Lobby_WorldById(BaseStructure[slot][ssLobby]), -1, -1, 25.0);

    if (gStructures[type][stSolid])
    {
        BaseStructure[slot][ssObject] = CreateDynamicObject(BASE_WALL_OBJECT_MODEL,
            BaseStructure[slot][ssX], BaseStructure[slot][ssY], BaseStructure[slot][ssZ] - 0.7,
            0.0, 0.0, BaseStructure[slot][ssRotZ], .worldid = Lobby_WorldById(BaseStructure[slot][ssLobby]));

        BaseStructure[slot][ssCollision] = CA_CreateObject(BASE_WALL_OBJECT_MODEL,
            BaseStructure[slot][ssX], BaseStructure[slot][ssY], BaseStructure[slot][ssZ] - 0.7,
            0.0, 0.0, BaseStructure[slot][ssRotZ], true);
        return 1;
    }

    BaseStructure[slot][ssPickup] = CreateDynamicPickup(gStructures[type][stPickupModel], 1,
        BaseStructure[slot][ssX], BaseStructure[slot][ssY], BaseStructure[slot][ssZ],
        .worldid = Lobby_WorldById(BaseStructure[slot][ssLobby]), .streamdistance = 25.0);
    return 1;
}

stock BaseBuild_DestroySlot(slot, bool:deleteFromDb)
{
    if (slot < 0 || slot >= BASE_MAX_STRUCTURES) return 0;
    if (!BaseStructure[slot][ssActive]) return 0;

    if (BaseStructure[slot][ssObject] != STREAMER_TAG_OBJECT:0) DestroyDynamicObject(BaseStructure[slot][ssObject]);
    if (BaseStructure[slot][ssCollision] != -1) CA_DestroyObject(BaseStructure[slot][ssCollision]);
    if (BaseStructure[slot][ssPickup] != STREAMER_TAG_PICKUP:0) DestroyDynamicPickup(BaseStructure[slot][ssPickup]);
    if (BaseStructure[slot][ssLabel] != STREAMER_TAG_3D_TEXT_LABEL:0) DestroyDynamic3DTextLabel(BaseStructure[slot][ssLabel]);

    if (deleteFromDb && SQL_IsReady() && BaseStructure[slot][ssDbId] > 0)
    {
        new query[96];
        mysql_format(gSQL, query, sizeof(query),
            "DELETE FROM base_structures WHERE id = %d", BaseStructure[slot][ssDbId]);
        mysql_tquery(gSQL, query, "", "");
    }

    BaseStructure[slot][ssActive] = false;
    BaseStructure[slot][ssDbId] = 0;
    BaseStructure[slot][ssBaseId] = 0;
    BaseStructure[slot][ssObject] = STREAMER_TAG_OBJECT:0;
    BaseStructure[slot][ssCollision] = -1;
    BaseStructure[slot][ssPickup] = STREAMER_TAG_PICKUP:0;
    BaseStructure[slot][ssLabel] = STREAMER_TAG_3D_TEXT_LABEL:0;
    BaseStructure[slot][ssHealth] = 0;
    BaseStructure[slot][ssFuel] = 0.0;
    return 1;
}

/* Struktur dimuat hanya saat pemain menyentuh sistemnya: 40 object per base x 200
   pemain tidak boleh dibuat saat boot. */
stock BaseBuild_EnsureLoaded(playerid)
{
    if (Base_Id(playerid) == 0)
    {
        Base_Load(playerid);
        SendClientMessage(playerid, UI_TEXT_MUTED, "[BUILD] Menyiapkan petak base, coba lagi.");
        return 0;
    }

    new baseId = Base_Id(playerid);
    if (BaseBuildLoadedBase[playerid] == baseId) return 1;

    BaseBuildLoadedBase[playerid] = baseId;

    SendClientMessage(playerid, UI_TEXT_MUTED, "[BUILD] Memuat struktur base, coba lagi dalam 1 detik.");
    new query[160];
    mysql_format(gSQL, query, sizeof(query),
        "SELECT id, struct_id, pos_x, pos_y, pos_z, rot_z, health, fuel FROM base_structures WHERE base_id = %d", baseId);
    mysql_tquery(gSQL, query, "BaseBuild_OnRowsLoaded", "i", playerid);
    return 0;
}

public BaseBuild_OnRowsLoaded(playerid)
{
    if (!IsPlayerConnected(playerid)) return 0;

    new baseId = Base_Id(playerid);
    if (baseId == 0) return 0;

    new rows = cache_num_rows();

    for (new row = 0; row < rows; row++)
    {
        new slot = BaseBuild_FreeSlot();
        if (slot < 0) break;

        new structId = 0, health = 0;
        new Float:fuel = 0.0;
        cache_get_value_name_int(row, "struct_id", structId);
        if (structId < 0 || structId >= STRUCTURE_COUNT) continue;

        cache_get_value_name_int(row, "id", BaseStructure[slot][ssDbId]);
        cache_get_value_name_int(row, "health", health);
        cache_get_value_name_float(row, "fuel", fuel);
        cache_get_value_name_float(row, "pos_x", BaseStructure[slot][ssX]);
        cache_get_value_name_float(row, "pos_y", BaseStructure[slot][ssY]);
        cache_get_value_name_float(row, "pos_z", BaseStructure[slot][ssZ]);
        cache_get_value_name_float(row, "rot_z", BaseStructure[slot][ssRotZ]);

        BaseStructure[slot][ssActive] = true;
        BaseStructure[slot][ssBaseId] = baseId;
        BaseStructure[slot][ssLobby] = Base_GetLobby(playerid);
        BaseStructure[slot][ssType] = structId;
        BaseStructure[slot][ssHealth] = health;
        BaseStructure[slot][ssHealthMax] = BaseBuild_Health(structId, Base_GetTier(playerid));
        if (fuel < 0.0) fuel = 0.0;
        if (fuel > BASE_GENERATOR_FUEL_MAX) fuel = BASE_GENERATOR_FUEL_MAX;
        BaseStructure[slot][ssFuel] = fuel;
        BaseStructure[slot][ssLastFuelSave] = GetTickCount();
        BaseStructure[slot][ssCollision] = -1;

        BaseBuild_CreateVisuals(slot);
    }

    new line[128];
    format(line, sizeof(line), "[BUILD] %d struktur dimuat untuk base #%d.", rows, baseId);
    SendClientMessage(playerid, UI_TEXT_SECONDARY, line);
    return 1;
}

/* Validasi titik build. Menolak dengan alasan spesifik, tidak pernah diam. */
stock bool:BaseBuild_IsSpotValid(playerid, Float:x, Float:y, Float:z)
{
    if (Zone_IsHub(Zone_GetIdAt(x, y, z)))
    {
        SendClientMessage(playerid, UI_ACCENT_WARN, "[BUILD] Tidak bisa membangun di dalam kamp.");
        return false;
    }

    new Float:plotX, Float:plotY, Float:plotZ;
    Base_GetPlot(playerid, plotX, plotY, plotZ);

    if (plotX != 0.0 || plotY != 0.0)
    {
        new Float:plotDistance = VectorSize(x - plotX, y - plotY, 0.0);
        if (plotDistance > BASE_PLOT_RADIUS)
        {
            new message[128];
            format(message, sizeof(message), "[BUILD] Di luar petak base (%.0f m > %.0f m).", plotDistance, BASE_PLOT_RADIUS);
            SendClientMessage(playerid, UI_ACCENT_WARN, message);
            return false;
        }
    }

    for (new slot = 0; slot < BASE_MAX_STRUCTURES; slot++)
    {
        if (!BaseStructure[slot][ssActive]) continue;

        new Float:gap = VectorSize(x - BaseStructure[slot][ssX], y - BaseStructure[slot][ssY], 0.0);
        if (gap < BASE_STRUCT_MIN_GAP)
        {
            new message[128];
            format(message, sizeof(message), "[BUILD] Terlalu dekat struktur lain (%.1f m).", gap);
            SendClientMessage(playerid, UI_ACCENT_WARN, message);
            return false;
        }
    }

    new Float:hitX, Float:hitY, Float:hitZ;
    new hit = CA_RayCastLine(x, y, z + 120.0, x, y, z - 80.0, hitX, hitY, hitZ);
    if (hit == 0)
    {
        SendClientMessage(playerid, UI_ACCENT_WARN, "[BUILD] Tidak ada tanah di titik itu.");
        return false;
    }
    return true;
}

stock BaseBuild_MaterialsOk(playerid, type)
{
    if (Inventory_CountItem(playerid, gStructures[type][stMatA]) < gStructures[type][stAmtA]) return 0;
    if (Inventory_CountItem(playerid, gStructures[type][stMatB]) < gStructures[type][stAmtB]) return 0;
    return 1;
}

stock BaseBuild_Place(playerid, type)
{
    if (!Account_IsPlaying(playerid)) return 0;

    /* listitem dialog datang dari client: tanpa guard ini, index liar bisa membaca
       gStructures di luar batas dan menaruh struktur yang tidak ada. */
    if (type < 0 || type >= STRUCTURE_COUNT) return 0;

    if (GetTickCount() - BaseBuildLastBuildTick[playerid] < BASE_BUILD_COOLDOWN_MS)
    {
        SendClientMessage(playerid, UI_TEXT_MUTED, "[BUILD] Terlalu cepat, tunggu sebentar.");
        return 0;
    }

    if (!BaseBuild_EnsureLoaded(playerid))
    {
        SendClientMessage(playerid, UI_TEXT_MUTED, "[BUILD] Memuat struktur base, coba lagi.");
        return 0;
    }

    new baseId = Base_Id(playerid);
    new tier = Base_GetTier(playerid);
    if (tier < gStructures[type][stTier])
    {
        new message[112];
        format(message, sizeof(message), "[BUILD] %s butuh base tier %d.", gStructures[type][stLabel], gStructures[type][stTier]);
        SendClientMessage(playerid, UI_ACCENT_WARN, message);
        return 0;
    }

    if (BaseBuild_CountType(baseId, type) >= BaseBuild_Limit(type, tier))
    {
        new message[112];
        format(message, sizeof(message), "[BUILD] Batas %s sudah tercapai.", gStructures[type][stLabel]);
        SendClientMessage(playerid, UI_ACCENT_WARN, message);
        return 0;
    }

    if (BaseBuild_TotalActive() >= BASE_MAX_STRUCTURES)
    {
        SendClientMessage(playerid, UI_ACCENT_WARN, "[BUILD] Server penuh struktur. Bongkar yang lama dulu.");
        return 0;
    }

    if (!BaseBuild_MaterialsOk(playerid, type))
    {
        new message[144];
        format(message, sizeof(message), "[BUILD] Butuh %d material A dan %d material B.",
            gStructures[type][stAmtA], gStructures[type][stAmtB]);
        SendClientMessage(playerid, UI_ACCENT_WARN, message);
        return 0;
    }

    new Float:x, Float:y, Float:z, Float:angle;
    GetPlayerPos(playerid, x, y, z);
    GetPlayerFacingAngle(playerid, angle);

    new Float:buildX = x + floatsin(-angle, degrees) * 2.5;
    new Float:buildY = y + floatcos(-angle, degrees) * 2.5;

    if (!BaseBuild_IsSpotValid(playerid, buildX, buildY, z)) return 0;

    new Float:plotX, Float:plotY, Float:plotZ;
    Base_GetPlot(playerid, plotX, plotY, plotZ);
    if (plotX == 0.0 && plotY == 0.0)
    {
        // Petak lahir di titik build pertama: base tidak punya koordinat sampai dipakai.
        Base_SetPlot(playerid, buildX, buildY, z);

        new query[160];
        mysql_format(gSQL, query, sizeof(query),
            "UPDATE bases SET pos_x = %f, pos_y = %f, pos_z = %f WHERE id = %d",
            buildX, buildY, z, baseId);
        mysql_tquery(gSQL, query, "", "");
    }

    if (!Inventory_Consume(playerid, gStructures[type][stMatA], gStructures[type][stAmtA], "build"))
    {
        SendClientMessage(playerid, UI_ACCENT_WARN, "[BUILD] Material pertama tidak cukup.");
        return 0;
    }

    if (!Inventory_Consume(playerid, gStructures[type][stMatB], gStructures[type][stAmtB], "build"))
    {
        // Tidak boleh ada material yang menguap: kembalikan yang sudah diambil.
        Inventory_Grant(playerid, gStructures[type][stMatA], gStructures[type][stAmtA], "build_refund");
        SendClientMessage(playerid, UI_ACCENT_WARN, "[BUILD] Material kedua tidak cukup, bahan dikembalikan.");
        return 0;
    }

    new slot = BaseBuild_FreeSlot();
    if (slot < 0)
    {
        Inventory_Grant(playerid, gStructures[type][stMatA], gStructures[type][stAmtA], "build_refund");
        Inventory_Grant(playerid, gStructures[type][stMatB], gStructures[type][stAmtB], "build_refund");
        SendClientMessage(playerid, UI_ACCENT_WARN, "[BUILD] Slot struktur penuh, bahan dikembalikan.");
        return 0;
    }

    BaseStructure[slot][ssActive] = true;
    BaseStructure[slot][ssBaseId] = baseId;
    BaseStructure[slot][ssLobby] = Base_GetLobby(playerid);
    BaseStructure[slot][ssType] = type;
    BaseStructure[slot][ssX] = buildX;
    BaseStructure[slot][ssY] = buildY;
    BaseStructure[slot][ssZ] = z;
    BaseStructure[slot][ssRotZ] = angle;
    BaseStructure[slot][ssHealthMax] = BaseBuild_Health(type, tier);
    BaseStructure[slot][ssHealth] = BaseStructure[slot][ssHealthMax];
    BaseStructure[slot][ssCollision] = -1;

    BaseBuild_CreateVisuals(slot);

    new query[288], msBuf[16];
    EpochMs(msBuf, sizeof(msBuf));
    mysql_format(gSQL, query, sizeof(query),
        "INSERT INTO base_structures (base_id, struct_id, pos_x, pos_y, pos_z, rot_z, health, created_at) VALUES (%d, %d, %f, %f, %f, %f, %d, %s)",
        baseId, _:type, buildX, buildY, z, angle, BaseStructure[slot][ssHealth], msBuf);

    BaseBuildPendingSlot[playerid] = slot;
    /* Slot ikut sebagai parameter: dua INSERT overlap tidak boleh saling
       menulis insert_id ke struktur yang salah (audit #16). */
    mysql_tquery(gSQL, query, "BaseBuild_OnInserted", "ii", playerid, slot);

    BaseBuildLastBuildTick[playerid] = GetTickCount();

    new message[144];
    format(message, sizeof(message), "[BUILD] %s berdiri (%d HP). Zombie bisa merusaknya.",
        gStructures[type][stLabel], BaseStructure[slot][ssHealth]);
    SendClientMessage(playerid, UI_ACCENT_PRIMARY, message);
    Toast_Show(playerid, "BASE", message, UI_ACCENT_PRIMARY);
    return 1;
}

public BaseBuild_OnInserted(playerid, slot)
{
    if (mysql_errno(gSQL) != 0)
    {
        Log_Format(LOG_WARN, "[BUILD] insert struktur gagal errno=%d", mysql_errno(gSQL));
        return 0;
    }

    if (!IsPlayerConnected(playerid)) return 0;
    if (slot < 0 || slot >= BASE_MAX_STRUCTURES) return 0;
    /* Struktur harus masih aktif dan milik base pemain ini: slot bisa sudah
       dibongkar/diisi ulang saat callback tiba. */
    if (!BaseStructure[slot][ssActive] || BaseStructure[slot][ssBaseId] != Base_Id(playerid)) return 0;

    BaseStructure[slot][ssDbId] = cache_insert_id();
    if (BaseBuildPendingSlot[playerid] == slot) BaseBuildPendingSlot[playerid] = -1;
    return 1;
}

stock BaseBuild_DismantleNearest(playerid)
{
    if (!BaseBuild_EnsureLoaded(playerid)) return 0;

    new baseId = Base_Id(playerid);
    new bestSlot = -1;
    new Float:bestDistance = 4.0;

    new Float:x, Float:y, Float:z;
    GetPlayerPos(playerid, x, y, z);

    for (new slot = 0; slot < BASE_MAX_STRUCTURES; slot++)
    {
        if (!BaseStructure[slot][ssActive] || BaseStructure[slot][ssBaseId] != baseId) continue;

        new Float:distance = VectorSize(x - BaseStructure[slot][ssX], y - BaseStructure[slot][ssY], z - BaseStructure[slot][ssZ]);
        if (distance >= bestDistance) continue;

        bestDistance = distance;
        bestSlot = slot;
    }

    if (bestSlot < 0)
    {
        SendClientMessage(playerid, UI_ACCENT_WARN, "[BUILD] Tidak ada struktur base dalam 4 meter.");
        return 0;
    }

    new type = BaseStructure[bestSlot][ssType];
    new refundA = (gStructures[type][stAmtA] * BASE_STRUCT_DISMANTLE_PCT) / 100;
    new refundB = (gStructures[type][stAmtB] * BASE_STRUCT_DISMANTLE_PCT) / 100;

    if (refundA > 0) Inventory_Grant(playerid, gStructures[type][stMatA], refundA, "build_dismantle");
    if (refundB > 0) Inventory_Grant(playerid, gStructures[type][stMatB], refundB, "build_dismantle");

    BaseBuild_DestroySlot(bestSlot, true);

    new message[128];
    format(message, sizeof(message), "[BUILD] %s dibongkar. Refund %d + %d material.", gStructures[type][stLabel], refundA, refundB);
    SendClientMessage(playerid, UI_ACCENT_PRIMARY, message);
    return 1;
}

stock BaseBuild_RepairNearest(playerid)
{
    if (!BaseBuild_EnsureLoaded(playerid)) return 0;

    new baseId = Base_Id(playerid);
    new bestSlot = -1;
    new Float:bestDistance = 4.0;

    new Float:x, Float:y, Float:z;
    GetPlayerPos(playerid, x, y, z);

    for (new slot = 0; slot < BASE_MAX_STRUCTURES; slot++)
    {
        if (!BaseStructure[slot][ssActive] || BaseStructure[slot][ssBaseId] != baseId) continue;
        if (BaseStructure[slot][ssHealth] >= BaseStructure[slot][ssHealthMax]) continue;

        new Float:distance = VectorSize(x - BaseStructure[slot][ssX], y - BaseStructure[slot][ssY], z - BaseStructure[slot][ssZ]);
        if (distance >= bestDistance) continue;

        bestDistance = distance;
        bestSlot = slot;
    }

    if (bestSlot < 0) return 0;

    if (Inventory_CountItem(playerid, ITEM_SCRAP) < BASE_STRUCT_REPAIR_SCRAP)
    {
        SendClientMessage(playerid, UI_ACCENT_WARN, "[BUILD] Perlu Scrap 2 untuk perbaikan.");
        return 0;
    }

    if (!Inventory_Consume(playerid, ITEM_SCRAP, BASE_STRUCT_REPAIR_SCRAP, "repair")) return 0;

    BaseStructure[bestSlot][ssHealth] += BASE_STRUCT_REPAIR_HEALTH;
    if (BaseStructure[bestSlot][ssHealth] > BaseStructure[bestSlot][ssHealthMax])
    {
        BaseStructure[bestSlot][ssHealth] = BaseStructure[bestSlot][ssHealthMax];
    }

    new labelText[64];
    format(labelText, sizeof(labelText), "%s\n{P}%d HP",
        gStructures[BaseStructure[bestSlot][ssType]][stLabel], BaseStructure[bestSlot][ssHealth]);
    UpdateDynamic3DTextLabelText(BaseStructure[bestSlot][ssLabel], UI_ACCENT_PRIMARY, labelText);

    new query[128];
    mysql_format(gSQL, query, sizeof(query),
        "UPDATE base_structures SET health = %d WHERE id = %d",
        BaseStructure[bestSlot][ssHealth], BaseStructure[bestSlot][ssDbId]);
    mysql_tquery(gSQL, query, "", "");

    SendClientMessage(playerid, UI_ACCENT_PRIMARY, "[BUILD] Struktur diperbaiki +400 HP.");
    return 1;
}

stock BaseBuild_Upgrade(playerid)
{
    if (!BaseBuild_EnsureLoaded(playerid)) return 0;

    new tier = Base_GetTier(playerid);
    if (tier >= BASE_MAX_TIER)
    {
        SendClientMessage(playerid, UI_ACCENT_WARN, "[BASE] Tier sudah maksimal (3).");
        return 0;
    }

    new nextTier = tier + 1;
    /* Precheck ALL-OR-NOTHING keempat material (audit F4): Chemical dan
       Electronics sebelumnya di-Consume tanpa cek ketersediaan — tier bisa
       naik tanpa membayar material langka. */
    if (Inventory_CountItem(playerid, ITEM_SCRAP) < gTierUpgradeScrap[nextTier] ||
        Inventory_CountItem(playerid, ITEM_WOOD) < gTierUpgradeWood[nextTier] ||
        Inventory_CountItem(playerid, ITEM_CHEMICAL) < gTierUpgradeChemical[nextTier] ||
        Inventory_CountItem(playerid, ITEM_ELECTRONICS) < gTierUpgradeElectronics[nextTier])
    {
        new message[160];
        format(message, sizeof(message), "[BASE] Butuh Scrap %d + Wood %d + Chemical %d + Electronics %d.",
            gTierUpgradeScrap[nextTier], gTierUpgradeWood[nextTier],
            gTierUpgradeChemical[nextTier], gTierUpgradeElectronics[nextTier]);
        SendClientMessage(playerid, UI_ACCENT_WARN, message);
        return 0;
    }

    /* Semua sudah diprecheck; bila consume tetap gagal (perubahan di antara
       statement), kembalikan yang sudah terconsume dan batalkan upgrade. */
    if (!Inventory_Consume(playerid, ITEM_SCRAP, gTierUpgradeScrap[nextTier], "base_upgrade")
        || !Inventory_Consume(playerid, ITEM_WOOD, gTierUpgradeWood[nextTier], "base_upgrade"))
    {
        Inventory_Grant(playerid, ITEM_SCRAP, gTierUpgradeScrap[nextTier], "base_upgrade_refund");
        Inventory_Grant(playerid, ITEM_WOOD, gTierUpgradeWood[nextTier], "base_upgrade_refund");
        SendClientMessage(playerid, UI_ACCENT_DANGER, "[BASE] Material berubah, upgrade dibatalkan.");
        return 0;
    }

    if (gTierUpgradeChemical[nextTier] > 0)
    {
        if (!Inventory_Consume(playerid, ITEM_CHEMICAL, gTierUpgradeChemical[nextTier], "base_upgrade"))
        {
            Inventory_Grant(playerid, ITEM_SCRAP, gTierUpgradeScrap[nextTier], "base_upgrade_refund");
            Inventory_Grant(playerid, ITEM_WOOD, gTierUpgradeWood[nextTier], "base_upgrade_refund");
            SendClientMessage(playerid, UI_ACCENT_DANGER, "[BASE] Chemical kurang, upgrade dibatalkan.");
            return 0;
        }
    }
    if (gTierUpgradeElectronics[nextTier] > 0)
    {
        if (!Inventory_Consume(playerid, ITEM_ELECTRONICS, gTierUpgradeElectronics[nextTier], "base_upgrade"))
        {
            Inventory_Grant(playerid, ITEM_SCRAP, gTierUpgradeScrap[nextTier], "base_upgrade_refund");
            Inventory_Grant(playerid, ITEM_WOOD, gTierUpgradeWood[nextTier], "base_upgrade_refund");
            Inventory_Grant(playerid, ITEM_CHEMICAL, gTierUpgradeChemical[nextTier], "base_upgrade_refund");
            SendClientMessage(playerid, UI_ACCENT_DANGER, "[BASE] Electronics kurang, upgrade dibatalkan.");
            return 0;
        }
    }

    BaseTier[playerid] = nextTier;

    new query[128];
    mysql_format(gSQL, query, sizeof(query),
        "UPDATE bases SET tier = %d WHERE id = %d", nextTier, Base_Id(playerid));
    mysql_tquery(gSQL, query, "", "");

    new message[128];
    format(message, sizeof(message), "[BASE] Naik ke tier %d. Wall limit +4, HP struktur +25%% di tier 3.", nextTier);
    SendClientMessage(playerid, UI_ACCENT_PRIMARY, message);
    Toast_Show(playerid, "BASE UPGRADE", message, UI_ACCENT_PRIMARY);
    return 1;
}

stock bool:BaseBuild_HasNearby(playerid, type, Float:range)
{
    new baseId = Base_Id(playerid);
    if (baseId == 0) return false;

    new Float:x, Float:y, Float:z;
    GetPlayerPos(playerid, x, y, z);

    for (new slot = 0; slot < BASE_MAX_STRUCTURES; slot++)
    {
        if (!BaseStructure[slot][ssActive] || BaseStructure[slot][ssBaseId] != baseId) continue;
        if (BaseStructure[slot][ssType] != type) continue;

        if (VectorSize(x - BaseStructure[slot][ssX], y - BaseStructure[slot][ssY], z - BaseStructure[slot][ssZ]) <= range) return true;
    }
    return false;
}

/* Titik respawn base hanya aktif kalau ada bed dan minimal 2 wall: tanpa itu,
   respawn di luar kamp sama dengan bunuh diri. */
stock bool:BaseBuild_BedPosition(playerid, &Float:x, &Float:y, &Float:z)
{
    new baseId = Base_Id(playerid);
    if (baseId == 0) return false;
    if (BaseBuild_CountType(baseId, STRUCT_WALL) < 2) return false;

    for (new slot = 0; slot < BASE_MAX_STRUCTURES; slot++)
    {
        if (!BaseStructure[slot][ssActive] || BaseStructure[slot][ssBaseId] != baseId) continue;
        if (BaseStructure[slot][ssType] != STRUCT_BED) continue;

        x = BaseStructure[slot][ssX];
        y = BaseStructure[slot][ssY];
        z = BaseStructure[slot][ssZ] + 0.5;
        return true;
    }
    return false;
}

stock BaseBuild_ZombieTick(const Float:zombieX[], const Float:zombieY[], const Float:zombieZ[], const zombieLobby[], zombieCount)
{
    #pragma unused zombieZ

    if (zombieCount == 0) return 0;

    for (new slot = 0; slot < BASE_MAX_STRUCTURES; slot++)
    {
        if (!BaseStructure[slot][ssActive]) continue;

        new attackers = 0;
        for (new index = 0; index < zombieCount; index++)
        {
            if (zombieLobby[index] != BaseStructure[slot][ssLobby]) continue;

            new Float:deltaX = zombieX[index] - BaseStructure[slot][ssX];
            new Float:deltaY = zombieY[index] - BaseStructure[slot][ssY];
            if (floatsqroot(deltaX * deltaX + deltaY * deltaY) > 2.2) continue;

            attackers++;
            if (attackers >= BASE_STRUCT_MAX_ATTACKERS) break;
        }

        if (attackers == 0) continue;

        BaseStructure[slot][ssHealth] -= BASE_STRUCT_ZOMBIE_DPS * attackers * 2;
        if (BaseStructure[slot][ssHealth] > 0)
        {
            /* Simpan HP maksimal tiap 10 detik per struktur: kalau server restart,
               base tidak kembali penuh tanpa alasan. */
            if (GetTickCount() - BaseStructure[slot][ssLastSaveTick] > 10000 &&
                BaseStructure[slot][ssDbId] > 0 && SQL_IsReady())
            {
                new saveQuery[128];
                mysql_format(gSQL, saveQuery, sizeof(saveQuery),
                    "UPDATE base_structures SET health = %d WHERE id = %d",
                    BaseStructure[slot][ssHealth], BaseStructure[slot][ssDbId]);
                mysql_tquery(gSQL, saveQuery, "", "");
                BaseStructure[slot][ssLastSaveTick] = GetTickCount();
            }

            new labelText[64];
            format(labelText, sizeof(labelText), "%s\n{P}%d HP",
                gStructures[BaseStructure[slot][ssType]][stLabel], BaseStructure[slot][ssHealth]);
            UpdateDynamic3DTextLabelText(BaseStructure[slot][ssLabel], UI_ACCENT_PRIMARY, labelText);
            continue;
        }

        new destroyedType = BaseStructure[slot][ssType];
        BaseBuild_DestroySlot(slot, true);

        new message[128];
        format(message, sizeof(message), "Zombie menghancurkan %s di sebuah base.", gStructures[destroyedType][stLabel]);
        Toast_NotifyAll("BASE RUSAK", message, UI_ACCENT_DANGER);
    }
    return 1;
}

stock BaseBuild_GeneratorSlot(baseId)
{
    if (baseId <= 0) return -1;

    for (new slot = 0; slot < BASE_MAX_STRUCTURES; slot++)
    {
        if (!BaseStructure[slot][ssActive]) continue;
        if (BaseStructure[slot][ssBaseId] != baseId) continue;
        if (BaseStructure[slot][ssType] != STRUCT_GENERATOR) continue;
        return slot;
    }
    return -1;
}

stock bool:BaseBuild_HasPower(baseId)
{
    new slot = BaseBuild_GeneratorSlot(baseId);
    if (slot < 0) return false;
    return (BaseStructure[slot][ssFuel] > 0.0) ? true : false;
}

stock BaseBuild_RefreshGeneratorLabel(slot)
{
    if (slot < 0 || slot >= BASE_MAX_STRUCTURES) return 0;
    if (!BaseStructure[slot][ssActive]) return 0;
    if (BaseStructure[slot][ssType] != STRUCT_GENERATOR) return 0;
    if (BaseStructure[slot][ssLabel] == STREAMER_TAG_3D_TEXT_LABEL:0) return 0;

    new labelText[64];
    format(labelText, sizeof(labelText), "%s\n{P}%d HP | BBM %.1f L",
        gStructures[STRUCT_GENERATOR][stLabel], BaseStructure[slot][ssHealth], BaseStructure[slot][ssFuel]);
    UpdateDynamic3DTextLabelText(BaseStructure[slot][ssLabel], UI_ACCENT_PRIMARY, labelText);
    return 1;
}

stock BaseBuild_SaveGeneratorFuel(slot)
{
    if (slot < 0 || slot >= BASE_MAX_STRUCTURES) return 0;
    if (!SQL_IsReady()) return 0;
    if (BaseStructure[slot][ssDbId] <= 0) return 0;

    new query[128];
    mysql_format(gSQL, query, sizeof(query),
        "UPDATE base_structures SET fuel = %f WHERE id = %d",
        BaseStructure[slot][ssFuel], BaseStructure[slot][ssDbId]);
    mysql_tquery(gSQL, query, "", "");
    BaseStructure[slot][ssLastFuelSave] = GetTickCount();
    return 1;
}

/* Generator hanya menyala saat malam dan hanya menghabiskan bahan bakar selama
   listrik dibutuhkan. Turret tanpa daya berhenti menembak, jadi bahan bakar
   adalah keputusan ekonomi malam, bukan dekorasi. */
stock BaseBuild_GeneratorTick()
{
    if (!World_IsNight()) return 0;

    for (new slot = 0; slot < BASE_MAX_STRUCTURES; slot++)
    {
        if (!BaseStructure[slot][ssActive]) continue;
        if (BaseStructure[slot][ssType] != STRUCT_GENERATOR) continue;
        if (BaseStructure[slot][ssFuel] <= 0.0) continue;

        BaseStructure[slot][ssFuel] -= BASE_GENERATOR_BURN_TICK;
        if (BaseStructure[slot][ssFuel] <= 0.0)
        {
            BaseStructure[slot][ssFuel] = 0.0;
            BaseBuild_SaveGeneratorFuel(slot);
            BaseBuild_RefreshGeneratorLabel(slot);

            foreach (new playerid : PlayerPool)
            {
                if (!IsPlayerConnected(playerid)) continue;
                if (Base_Id(playerid) != BaseStructure[slot][ssBaseId]) continue;
                Toast_Show(playerid, "GENSET MATI", "Bahan bakar habis. Turret berhenti. Isi dengan /generator.",
                    UI_ACCENT_DANGER);
            }
            continue;
        }

        BaseBuild_RefreshGeneratorLabel(slot);
        if (GetTickCount() - BaseStructure[slot][ssLastFuelSave] >= BASE_GENERATOR_FUEL_SAVE_MS)
        {
            BaseBuild_SaveGeneratorFuel(slot);
        }
    }
    return 1;
}

stock BaseBuild_RefillGenerator(playerid)
{
    if (!IsPlayerConnected(playerid)) return 0;
    if (!BaseBuild_EnsureLoaded(playerid)) return 0;

    new slot = BaseBuild_GeneratorSlot(Base_Id(playerid));
    if (slot < 0)
    {
        SendClientMessage(playerid, UI_TEXT_MUTED, "[GENSET] Base ini belum punya generator. Bangun lewat /base.");
        return 0;
    }

    new Float:x, Float:y, Float:z;
    GetPlayerPos(playerid, x, y, z);
    if (VectorSize(x - BaseStructure[slot][ssX], y - BaseStructure[slot][ssY], z - BaseStructure[slot][ssZ]) > BASE_GENERATOR_REFILL_RANGE)
    {
        SendClientMessage(playerid, UI_TEXT_MUTED, "[GENSET] Dekati generator dulu (6 meter).");
        return 0;
    }

    new Float:room = BASE_GENERATOR_FUEL_MAX - BaseStructure[slot][ssFuel];
    if (room < 0.5)
    {
        SendClientMessage(playerid, UI_TEXT_MUTED, "[GENSET] Tangki generator penuh.");
        return 0;
    }

    new jerrySlot = -1;
    for (new invSlot = 0; invSlot < MAX_INVENTORY_SLOTS; invSlot++)
    {
        if (PlayerInventory[playerid][invSlot][invItemId] != ITEM_JERRYCAN) continue;
        if (PlayerInventory[playerid][invSlot][invDurability] <= 0) continue;
        jerrySlot = invSlot;
        break;
    }

    if (jerrySlot < 0)
    {
        SendClientMessage(playerid, UI_TEXT_MUTED, "[GENSET] Butuh jerrycan berisi. Isi di pompa dengan /isi kan.");
        return 0;
    }

    new durability = PlayerInventory[playerid][jerrySlot][invDurability];
    new Float:available = BASE_GENERATOR_JERRY_LITERS * (float(durability) / 100.0);
    new Float:transfer = (available < room) ? available : room;

    BaseStructure[slot][ssFuel] += transfer;
    if (BaseStructure[slot][ssFuel] > BASE_GENERATOR_FUEL_MAX) BaseStructure[slot][ssFuel] = BASE_GENERATOR_FUEL_MAX;

    new spentPercent = floatround(transfer / BASE_GENERATOR_JERRY_LITERS * 100.0);
    new remaining = durability - spentPercent;
    if (remaining < 0) remaining = 0;
    PlayerInventory[playerid][jerrySlot][invDurability] = remaining;
    Inventory_Save(playerid);

    BaseBuild_RefreshGeneratorLabel(slot);
    BaseBuild_SaveGeneratorFuel(slot);

    new line[128];
    format(line, sizeof(line), "[GENSET] +%.1f L. BBM %.1f/%.0f L. Sisa jerrycan %d%%.",
        transfer, BaseStructure[slot][ssFuel], BASE_GENERATOR_FUEL_MAX, remaining);
    SendClientMessage(playerid, UI_ACCENT_PRIMARY, line);
    Log_Format(LOG_INFO, "[GENSET] char=%d isi %.1f L, sisa %.1f L",
        ActiveCharId[playerid], transfer, BaseStructure[slot][ssFuel]);
    return 1;
}

stock BaseBuild_TurretTick(const zombieIds[], const Float:zombieX[], const Float:zombieY[], const Float:zombieZ[], const zombieLobby[], zombieCount)
{
    if (zombieCount == 0) return 0;

    for (new slot = 0; slot < BASE_MAX_STRUCTURES; slot++)
    {
        if (!BaseStructure[slot][ssActive]) continue;
        if (BaseStructure[slot][ssType] != STRUCT_TURRET) continue;

        /* Malam tanpa daya = turret mati. Ini yang membuat generator dan BBM
           punya konsekuensi nyata, bukan sekadar properti dekoratif. */
        if (World_IsNight() && !BaseBuild_HasPower(BaseStructure[slot][ssBaseId])) continue;

        new bestZombie = -1;
        new Float:bestDistance = BASE_TURRET_RANGE;

        for (new index = 0; index < zombieCount; index++)
        {
            if (zombieLobby[index] != BaseStructure[slot][ssLobby]) continue;

            new Float:distance = VectorSize(zombieX[index] - BaseStructure[slot][ssX],
                zombieY[index] - BaseStructure[slot][ssY],
                zombieZ[index] - BaseStructure[slot][ssZ]);

            if (distance >= bestDistance) continue;
            bestDistance = distance;
            bestZombie = index;
        }

        if (bestZombie < 0) continue;

        /* Id zombie ikut dari tick yang sama: tidak ada pencarian ulang per turret. */
        new zombieid = zombieIds[bestZombie];
        if (zombieid == INVALID_ZOMBIE) continue;
        if (!Zombie_IsActive(zombieid)) continue;

        Zombie_ApplyDamage(zombieid, INVALID_PLAYER_ID, BASE_TURRET_DAMAGE, 0);
        Noise_EmitAtPosition(BaseStructure[slot][ssX], BaseStructure[slot][ssY], BaseStructure[slot][ssZ],
            BASE_TURRET_NOISE_RADIUS);
    }
    return 1;
}

stock BaseBuild_ShowMenu(playerid)
{
    if (!Account_IsPlaying(playerid)) return 0;
    if (!BaseBuild_EnsureLoaded(playerid)) return 0;

    new baseId = Base_Id(playerid);
    new tier = Base_GetTier(playerid);
    new body[1280], line[144], materialA[24], materialB[24];

    body[0] = EOS;
    for (new structure = 0; structure < STRUCTURE_COUNT; structure++)
    {
        new type = structure;
        new used = BaseBuild_CountType(baseId, type);
        new limit = BaseBuild_Limit(type, tier);

        Item_GetName(gStructures[type][stMatA], materialA, sizeof(materialA));
        Item_GetName(gStructures[type][stMatB], materialB, sizeof(materialB));

        new status[24] = "siap";
        if (tier < gStructures[type][stTier]) format(status, sizeof(status), "butuh tier %d", gStructures[type][stTier]);
        else if (used >= limit) format(status, sizeof(status), "penuh");
        else if (!BaseBuild_MaterialsOk(playerid, type)) format(status, sizeof(status), "bahan kurang");

        format(line, sizeof(line), "%s (%d/%d) - %s %d + %s %d [%s]\n",
            gStructures[type][stLabel], used, limit,
            materialA, gStructures[type][stAmtA],
            materialB, gStructures[type][stAmtB],
            status);
        strcat(body, line, sizeof(body));
    }

    strcat(body, "\nDibongkar: /bongkar | Perbaiki: /fix | Naik tier: /upgrade", sizeof(body));

    new caption[96];
    format(caption, sizeof(caption), "Bangun base - tier %d | %d/%d struktur",
        tier, BaseBuild_CountBase(baseId), BASE_MAX_STRUCTURES);

    ShowPlayerDialog(playerid, DIALOG_BUILD_MAIN, DIALOG_STYLE_LIST, caption, body, "Bangun", "Tutup");
    return 1;
}

stock BaseBuild_OnDialogResponse(playerid, dialogid, response, listitem, const inputtext[])
{
    #pragma unused inputtext

    if (dialogid != DIALOG_BUILD_MAIN) return 0;
    if (!response) return 1;
    if (listitem < 0 || listitem >= STRUCTURE_COUNT) return 1;

    BaseBuild_Place(playerid, listitem);
    BaseBuild_ShowMenu(playerid);
    return 1;
}
