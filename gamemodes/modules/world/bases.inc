#if defined _ls_bases_included
    #endinput
#endif
#define _ls_bases_included

/* Barricade sekarang objek dunia, bukan efek sesi:
   - tersimpan di tabel barricades (FK ke bases milik karakter),
   - dimuat ulang saat boot sehingga tidak hilang setelah restart,
   - punya HP, dicakar zombie di sekitarnya, dan bisa diperbaiki dengan scrap. */

enum E_BARRICADE
{
    bool:bcActive,
    bcDbId,
    bcHealth,
    Float:bcX,
    Float:bcY,
    Float:bcZ,
    Float:bcRotZ,
    STREAMER_TAG_OBJECT:bcObject,
    bcCaIndex,
    bcLastDamageTick,
    bcLastHpSave
};

new Barricade[MAX_BARRICADES][E_BARRICADE];
new BaseId[MAX_PLAYERS];
new bool:BaseLoaded[MAX_PLAYERS];

forward Base_OnRowLoaded(playerid);
forward Base_OnAllBarricadesLoaded();
forward Base_OnBaseCreated(playerid);
forward Base_OnBarricadeInserted(slot);
forward Base_Tick();
forward Base_TickBody();
forward BaseBuild_GeneratorTick();
forward BaseBuild_ZombieTick(const Float:zombieX[], const Float:zombieY[], const Float:zombieZ[], const zombieLobby[], zombieCount);
forward BaseBuild_TurretTick(const zombieIds[], const Float:zombieX[], const Float:zombieY[], const Float:zombieZ[], const zombieLobby[], zombieCount);

new BaseTier[MAX_PLAYERS];
new BaseLobby[MAX_PLAYERS];
new BarricadeLobby[MAX_BARRICADES];
new Float:BasePlotX[MAX_PLAYERS];
new Float:BasePlotY[MAX_PLAYERS];
new Float:BasePlotZ[MAX_PLAYERS];

stock Base_GetLobby(playerid)
{
    if (!Lobby_IsValid(BaseLobby[playerid])) return 0;
    return BaseLobby[playerid];
}

stock Base_GetTier(playerid)
{
    if (BaseTier[playerid] < 1) return 1;
    return BaseTier[playerid];
}

stock Base_GetPlot(playerid, &Float:x, &Float:y, &Float:z)
{
    x = BasePlotX[playerid];
    y = BasePlotY[playerid];
    z = BasePlotZ[playerid];
    return 1;
}

stock Base_SetPlot(playerid, Float:x, Float:y, Float:z)
{
    BasePlotX[playerid] = x;
    BasePlotY[playerid] = y;
    BasePlotZ[playerid] = z;
    return 1;
}

stock Base_ResetPlayer(playerid)
{
    BaseId[playerid] = 0;
    BaseLoaded[playerid] = false;
    return 1;
}

stock Base_Id(playerid)
{
    return BaseId[playerid];
}

/* Base dibuat sekali per karakter; shelter_id unik per base supaya satu shelter satu pemilik. */
stock Base_Load(playerid)
{
    if (!SQL_IsReady() || ActiveCharId[playerid] == 0) return 0;

    new query[160];
    mysql_format(gSQL, query, sizeof(query),
        "SELECT id, tier, lobby_id, pos_x, pos_y, pos_z FROM bases WHERE char_id = %d LIMIT 1", ActiveCharId[playerid]);
    mysql_tquery(gSQL, query, "Base_OnRowLoaded", "i", playerid);

    /* Keanggotaan party dimuat terpisah: pemilik tidak punya baris di
       base_members, jadi dua query ini saling melengkapi. */
    BaseMembers_Load(playerid);
    return 1;
}

public Base_OnRowLoaded(playerid)
{
    if (!IsPlayerConnected(playerid)) return 0;

    if (cache_num_rows() > 0)
    {
        cache_get_value_name_int(0, "id", BaseId[playerid]);
        cache_get_value_name_int(0, "tier", BaseTier[playerid]);
        cache_get_value_name_int(0, "lobby_id", BaseLobby[playerid]);
        if (!Lobby_IsValid(BaseLobby[playerid])) BaseLobby[playerid] = 0;
        cache_get_value_name_float(0, "pos_x", BasePlotX[playerid]);
        cache_get_value_name_float(0, "pos_y", BasePlotY[playerid]);
        cache_get_value_name_float(0, "pos_z", BasePlotZ[playerid]);
        BaseLoaded[playerid] = true;
        Horde_ResetBaseHeat(playerid);
        return 1;
    }

    new query[192];
    mysql_format(gSQL, query, sizeof(query),
        "INSERT INTO bases (char_id, shelter_id, name, lobby_id) VALUES (%d, %d, 'Kamp Karantina', %d)",
        ActiveCharId[playerid], (ActiveCharId[playerid] % 250) + 1, Lobby_Of(playerid));
    mysql_tquery(gSQL, query, "Base_OnBaseCreated", "i", playerid);
    return 1;
}

public Base_OnBaseCreated(playerid)
{
    if (!IsPlayerConnected(playerid)) return 0;

    if (mysql_errno(gSQL) != 0)
    {
        Log_Format(LOG_WARN, "[BASE] gagal membuat base player=%d errno=%d", playerid, mysql_errno(gSQL));
        return 0;
    }

    BaseId[playerid] = cache_insert_id();
    BaseTier[playerid] = 1;
    BaseLobby[playerid] = Lobby_Of(playerid);
    BaseLoaded[playerid] = true;
    Horde_ResetBaseHeat(playerid);
    return 1;
}

stock Base_CreateBarricadeObjects(slot)
{
    if (slot < 0 || slot >= MAX_BARRICADES) return 0;

    Barricade[slot][bcObject] = CreateDynamicObject(BARRICADE_MODEL,
        Barricade[slot][bcX], Barricade[slot][bcY], Barricade[slot][bcZ],
        0.0, 0.0, Barricade[slot][bcRotZ], .worldid = Lobby_WorldById(BarricadeLobby[slot]));

    Barricade[slot][bcCaIndex] = CA_CreateObject(BARRICADE_MODEL,
        Barricade[slot][bcX], Barricade[slot][bcY], Barricade[slot][bcZ],
        0.0, 0.0, Barricade[slot][bcRotZ], true);
    return 1;
}

stock Base_DestroyBarricade(slot, bool:deleteFromDb)
{
    if (slot < 0 || slot >= MAX_BARRICADES) return 0;
    if (!Barricade[slot][bcActive]) return 0;

    if (Barricade[slot][bcObject] != STREAMER_TAG_OBJECT:0) DestroyDynamicObject(Barricade[slot][bcObject]);
    if (Barricade[slot][bcCaIndex] != -1) CA_DestroyObject(Barricade[slot][bcCaIndex]);

    if (deleteFromDb && SQL_IsReady() && Barricade[slot][bcDbId] > 0)
    {
        new query[96];
        mysql_format(gSQL, query, sizeof(query), "DELETE FROM barricades WHERE id = %d", Barricade[slot][bcDbId]);
        mysql_tquery(gSQL, query, "", "");
    }

    Barricade[slot][bcActive] = false;
    Barricade[slot][bcDbId] = 0;
    Barricade[slot][bcHealth] = 0;
    Barricade[slot][bcObject] = STREAMER_TAG_OBJECT:0;
    Barricade[slot][bcCaIndex] = -1;
    return 1;
}

stock Base_FreeSlot()
{
    for (new slot = 0; slot < MAX_BARRICADES; slot++)
    {
        if (!Barricade[slot][bcActive]) return slot;
    }
    return -1;
}

/* Semua barricade dimuat saat boot: objek dunia milik server, bukan milik pemain.
   Harus public karena dipanggil SetTimer. */
forward Base_BootLoad();

public Base_BootLoad()
{
    if (!SQL_IsReady()) return 0;

    for (new slot = 0; slot < MAX_BARRICADES; slot++)
    {
        Barricade[slot][bcActive] = false;
        Barricade[slot][bcDbId] = 0;
        Barricade[slot][bcObject] = STREAMER_TAG_OBJECT:0;
        Barricade[slot][bcCaIndex] = -1;
    }

    mysql_tquery(gSQL,
        "SELECT id, lobby_id, pos_x, pos_y, pos_z, rot_z, health FROM barricades ORDER BY id ASC LIMIT 40",
        "Base_OnAllBarricadesLoaded", "");
    return 1;
}

public Base_OnAllBarricadesLoaded()
{
    new rows = cache_num_rows();
    if (rows >= MAX_BARRICADES)
    {
        Log_Format(LOG_WARN, "[BASE] barricade DB >= %d: boot LIMIT memotong sisa, naikkan MAX_BARRICADES bila perlu.", MAX_BARRICADES);
    }

    for (new row = 0; row < rows; row++)
    {
        new slot = Base_FreeSlot();
        if (slot == -1) break;

        new Float:posX, Float:posY, Float:posZ, Float:rotZ;
        new Float:health;
        cache_get_value_name_int(row, "id", Barricade[slot][bcDbId]);
        cache_get_value_name_int(row, "lobby_id", BarricadeLobby[slot]);
        if (!Lobby_IsValid(BarricadeLobby[slot])) BarricadeLobby[slot] = 0;
        cache_get_value_name_float(row, "pos_x", posX);
        cache_get_value_name_float(row, "pos_y", posY);
        cache_get_value_name_float(row, "pos_z", posZ);
        cache_get_value_name_float(row, "rot_z", rotZ);
        cache_get_value_name_float(row, "health", health);

        Barricade[slot][bcActive] = true;
        Barricade[slot][bcHealth] = floatround(health);
        Barricade[slot][bcX] = posX;
        Barricade[slot][bcY] = posY;
        Barricade[slot][bcZ] = posZ;
        Barricade[slot][bcRotZ] = rotZ;
        Barricade[slot][bcLastDamageTick] = 0;
        Base_CreateBarricadeObjects(slot);
    }

    Log_Format(LOG_INFO, "[BASE] %d barricade dimuat dari database.", rows);
    return 1;
}

stock bool:Base_PlaceBarricade(playerid)
{
    if (!Account_IsPlaying(playerid)) return false;
    if (!BaseLoaded[playerid] || BaseId[playerid] == 0)
    {
        Base_Load(playerid);
        SendClientMessage(playerid, UI_ACCENT_WARN, "[BASE] Memuat data base, tekan lagi sebentar.");
        return false;
    }

    new slot = Base_FreeSlot();
    if (slot == -1)
    {
        SendClientMessage(playerid, UI_ACCENT_WARN, "[BASE] Batas barricade server tercapai.");
        return false;
    }

    if (!Inventory_Consume(playerid, ITEM_BARRICADE_KIT, 1, "barricade"))
    {
        SendClientMessage(playerid, UI_ACCENT_WARN, "[BASE] Butuh Barricade Kit di tas.");
        return false;
    }

    new Float:x, Float:y, Float:z, Float:angle;
    GetPlayerPos(playerid, x, y, z);
    GetPlayerFacingAngle(playerid, angle);

    new Float:placeX = x + floatsin(-angle, degrees) * 2.0;
    new Float:placeY = y + floatcos(-angle, degrees) * 2.0;

    Barricade[slot][bcActive] = true;
    BarricadeLobby[slot] = Lobby_Of(playerid);
    Barricade[slot][bcHealth] = BARRICADE_MAX_HEALTH;
    Barricade[slot][bcX] = placeX;
    Barricade[slot][bcY] = placeY;
    Barricade[slot][bcZ] = z - 0.7;
    Barricade[slot][bcRotZ] = angle;
    Barricade[slot][bcLastDamageTick] = GetTickCount();
    Base_CreateBarricadeObjects(slot);

    new query[256];
    mysql_format(gSQL, query, sizeof(query),
        "INSERT INTO barricades (base_id, model, lobby_id, pos_x, pos_y, pos_z, rot_x, rot_y, rot_z, health) VALUES (%d, %d, %d, %.4f, %.4f, %.4f, 0.0, 0.0, %.4f, %d)",
        BaseId[playerid], BARRICADE_MODEL, Lobby_Of(playerid), placeX, placeY, z - 0.7, angle, BARRICADE_MAX_HEALTH);

    mysql_tquery(gSQL, query, "Base_OnBarricadeInserted", "i", slot);

    Toast_Show(playerid, "Barricade", "Dipasang. Zombie mencakarnya, perbaiki dengan /fix.", UI_ACCENT_PRIMARY);
    return true;
}

/* Zombie yang menempel mencakar barricade terdekat. Posisi semua zombie diambil
   SEKALI per tick (FCNPC_GetPosition itu mahal), lalu dibandingkan dengan tiap barricade. */
public Base_Tick()
{
    new perfStart = GetTickCount();
    Base_TickBody();
    Perf_Update(PERF_BASE, perfStart);
    return 1;
}

stock Base_TickBody()
{
    BaseBuild_GateTick();

    BaseBuild_GeneratorTick();

    new zombieCount = 0;
    new Float:zombieX[MAX_ZOMBIES];
    new Float:zombieY[MAX_ZOMBIES];
    new Float:zombieZ[MAX_ZOMBIES];
    new zombieIds[MAX_ZOMBIES];
    new zombieLobby[MAX_ZOMBIES];

    foreach (new zombieid : ZombiePool)
    {
        if (zombieCount >= MAX_ZOMBIES) break;
        FCNPC_GetPosition(zombieid, zombieX[zombieCount], zombieY[zombieCount], zombieZ[zombieCount]);
        zombieIds[zombieCount] = zombieid;
        zombieLobby[zombieCount] = Zombie_GetLobby(zombieid);
        zombieCount++;
    }

    if (zombieCount == 0) return 1;

    for (new slot = 0; slot < MAX_BARRICADES; slot++)
    {
        if (!Barricade[slot][bcActive]) continue;

        new attackers = 0;
        for (new index = 0; index < zombieCount; index++)
        {
            if (zombieLobby[index] != BarricadeLobby[slot]) continue;

            new Float:dx = zombieX[index] - Barricade[slot][bcX];
            new Float:dy = zombieY[index] - Barricade[slot][bcY];
            if (floatsqroot(dx * dx + dy * dy) > 2.2) continue;

            attackers++;
            if (attackers >= 4) break;
        }

        if (attackers == 0) continue;

        Barricade[slot][bcHealth] -= BARRICADE_ZOMBIE_DPS * attackers * 2;
        if (Barricade[slot][bcHealth] > 0)
        {
            /* HP rusak wajib ikut persist (throttle 10 dtk): tanpa ini restart
               mengembalikan barricade ke HP penuh dan damage zombie hilang. */
            new nowHp = GetTickCount();
            if (nowHp - Barricade[slot][bcLastHpSave] > 10000
                && SQL_IsReady() && Barricade[slot][bcDbId] > 0)
            {
                Barricade[slot][bcLastHpSave] = nowHp;
                new hpQuery[128];
                mysql_format(gSQL, hpQuery, sizeof(hpQuery),
                    "UPDATE barricades SET health = %d WHERE id = %d",
                    Barricade[slot][bcHealth], Barricade[slot][bcDbId]);
                mysql_tquery(gSQL, hpQuery, "", "");
            }
            continue;
        }

        Base_DestroyBarricade(slot, true);
        Toast_NotifyAll("BARRICADE JEBOL", "Satu penahan runtuh.", UI_ACCENT_DANGER);
    }

    /* Posisi zombie sudah diambil sekali di atas: struktur memakai array yang sama
       supaya tidak ada 190 FCNPC_GetPosition tambahan per tick. */
    BaseBuild_ZombieTick(zombieX, zombieY, zombieZ, zombieLobby, zombieCount);
    BaseBuild_TurretTick(zombieIds, zombieX, zombieY, zombieZ, zombieLobby, zombieCount);
    return 1;
}

stock Base_NearestBarricade(playerid, Float:range)
{
    new Float:px, Float:py, Float:pz;
    GetPlayerPos(playerid, px, py, pz);

    new best = -1;
    new Float:bestDistance = range;

    for (new slot = 0; slot < MAX_BARRICADES; slot++)
    {
        if (!Barricade[slot][bcActive]) continue;

        new Float:dx = Barricade[slot][bcX] - px;
        new Float:dy = Barricade[slot][bcY] - py;
        new Float:dz = Barricade[slot][bcZ] - pz;
        new Float:distance = floatsqroot(dx * dx + dy * dy + dz * dz);

        if (distance > bestDistance) continue;
        bestDistance = distance;
        best = slot;
    }
    return best;
}

stock bool:Base_RepairBarricade(playerid)
{
    new slot = Base_NearestBarricade(playerid, 3.5);
    if (slot == -1)
    {
        SendClientMessage(playerid, UI_TEXT_MUTED, "[BASE] Tidak ada barricade dalam 3,5 meter.");
        return false;
    }

    if (Barricade[slot][bcHealth] >= BARRICADE_MAX_HEALTH)
    {
        SendClientMessage(playerid, UI_TEXT_MUTED, "[BASE] Barricade itu masih utuh.");
        return false;
    }

    if (!Inventory_Consume(playerid, ITEM_SCRAP, BARRICADE_REPAIR_SCRAP, "barricade_repair"))
    {
        SendClientMessage(playerid, UI_ACCENT_WARN, "[BASE] Butuh Scrap 2 untuk memperbaiki.");
        return false;
    }

    Barricade[slot][bcHealth] += BARRICADE_REPAIR_HEALTH;
    if (Barricade[slot][bcHealth] > BARRICADE_MAX_HEALTH) Barricade[slot][bcHealth] = BARRICADE_MAX_HEALTH;

    if (SQL_IsReady() && Barricade[slot][bcDbId] > 0)
    {
        new query[128];
        mysql_format(gSQL, query, sizeof(query), "UPDATE barricades SET health = %d WHERE id = %d",
            Barricade[slot][bcHealth], Barricade[slot][bcDbId]);
        mysql_tquery(gSQL, query, "", "");
    }

    new message[112];
    format(message, sizeof(message), "[BASE] Barricade diperbaiki ke %d/%d HP.",
        Barricade[slot][bcHealth], BARRICADE_MAX_HEALTH);
    SendClientMessage(playerid, UI_ACCENT_PRIMARY, message);
    return true;
}

stock Base_ActiveCount()
{
    new active = 0;
    for (new slot = 0; slot < MAX_BARRICADES; slot++)
    {
        if (Barricade[slot][bcActive]) active++;
    }
    return active;
}

/* INSERT selesai: simpan id supaya repair/destroy bisa menyentuh baris yang benar. */
public Base_OnBarricadeInserted(slot)
{
    if (slot < 0 || slot >= MAX_BARRICADES) return 0;
    if (!Barricade[slot][bcActive]) return 0;

    if (mysql_errno(gSQL) != 0)
    {
        Log_Format(LOG_WARN, "[BASE] barricade gagal disimpan errno=%d", mysql_errno(gSQL));
        return 0;
    }

    Barricade[slot][bcDbId] = cache_insert_id();
    return 1;
}
