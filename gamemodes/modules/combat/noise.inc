#if defined _ls_noise_included
    #endinput
#endif
#define _ls_noise_included

forward Horde_AddHeat(Float:x, Float:y, Float:z, Float:radius);
forward Float:Event_NoiseMultiplier();

/* Noise is the survival game's core tension: gunfire buys speed at the cost of attention. */
stock Suppressor_FindSlot(playerid)
{
    for (new slot = 0; slot < MAX_INVENTORY_SLOTS; slot++)
    {
        if (PlayerInventory[playerid][slot][invItemId] == ITEM_SUPPRESSOR
            && PlayerInventory[playerid][slot][invDurability] > 0) return slot;
    }
    return -1;
}

stock Noise_EmitFromPlayer(playerid, weaponid)
{
    if (!IsPlayerConnected(playerid)) return 0;

    new Float:radius = Weapon_GetNoiseRadius(weaponid);
    if (radius <= 0.0) return 0;

    /* Suppressor dibawa di tas: tembakan pistol (22-24) jadi senyap dan
       menghabiskan 1 tembakan durability. Ini membuat blueprint suppressor
       bernilai (sebelumnya item mati — audit D3). */
    if (weaponid >= 22 && weaponid <= 24)
    {
        new suppSlot = Suppressor_FindSlot(playerid);
        if (suppSlot != -1)
        {
            radius = NOISE_SUPPRESSED;
            PlayerInventory[playerid][suppSlot][invDurability]--;
            if (PlayerInventory[playerid][suppSlot][invDurability] <= 0)
            {
                PlayerInventory[playerid][suppSlot][invItemId] = ITEM_NONE_ID;
                PlayerInventory[playerid][suppSlot][invAmount] = 0;
                SendClientMessage(playerid, UI_ACCENT_WARN, "[SUPPRESSOR] Habis terpakai. Craft ulang di meja crafting.");
            }
            PlayerInventoryDirty[playerid] = true;
            Bag_OnInventoryChanged(playerid);
        }
    }

    new Float:x, Float:y, Float:z;
    GetPlayerPos(playerid, x, y, z);

    PlayerSurvival[playerid][psLastNoiseTick] = GetTickCount();

    radius *= Skills_NoiseMultiplier(playerid);
    radius *= Event_NoiseMultiplier();

    /* Proteksi newbie (audit D5/DESIGN §14): noise pemain level <5 tetap
       menarik zombie terdekat (hash trigger) tapi TIDAK menaikkan heat zona —
       tembakan pertama tidak boleh memicu horde/raid ke pemain baru. */
    if (PlayerSurvival[playerid][psLevel] >= 5) Horde_AddHeat(x, y, z, radius);
    return Hash_TriggerNoise(x, y, z, radius, Lobby_Of(playerid));
}

stock Noise_EmitAtPosition(Float:x, Float:y, Float:z, Float:radius, lobbyId = 0)
{
    radius *= Event_NoiseMultiplier();
    Horde_AddHeat(x, y, z, radius);
    return Hash_TriggerNoise(x, y, z, radius, lobbyId);
}

/* Line of sight against real map collision, not just distance. */
stock bool:LOS_IsClear(Float:startX, Float:startY, Float:startZ, Float:endX, Float:endY, Float:endZ)
{
    new Float:hitX, Float:hitY, Float:hitZ;
    new hitModel = CA_RayCastLine(startX, startY, startZ, endX, endY, endZ, hitX, hitY, hitZ);

    if (hitModel == 0)
    {
        return true;
    }

    new Float:targetDist = VectorSize(startX - endX, startY - endY, startZ - endZ);
    new Float:hitDist = VectorSize(startX - hitX, startY - hitY, startZ - hitZ);

    return (hitDist >= targetDist - 0.6);
}
