#if defined _ls_interact_included
    #endinput
#endif
#define _ls_interact_included

#define MAX_INTERACTS 32

enum E_INTERACT_KIND
{
    INTERACT_EXTRACT = 0,
    INTERACT_STASH,
    INTERACT_CRAFT,
    INTERACT_SHOP,
    INTERACT_GARAGE,
    INTERACT_FUEL
};

enum E_INTERACT
{
    bool:iaActive,
    E_INTERACT_KIND:iaKind,
    Float:iaX,
    Float:iaY,
    Float:iaZ,
    Float:iaRadius,
    STREAMER_TAG_AREA:iaArea,
    STREAMER_TAG_3D_TEXT_LABEL:iaLabel,
    iaLabelText[64]
};

new InteractPoint[MAX_INTERACTS][E_INTERACT];
new InteractCount = 0;

/* Single dispatcher: every world hotspot goes through here so mobile clients always
   have a /interact fallback when KEY_YES is unavailable. */
stock Interact_Create(E_INTERACT_KIND:kind, Float:x, Float:y, Float:z, Float:radius, const labelText[])
{
    if (InteractCount >= MAX_INTERACTS) return -1;

    new slot = -1;
    for (new i = 0; i < MAX_INTERACTS; i++)
    {
        if (InteractPoint[i][iaActive]) continue;
        slot = i;
        break;
    }
    if (slot == -1) return -1;

    InteractPoint[slot][iaActive] = true;
    InteractPoint[slot][iaKind] = kind;
    InteractPoint[slot][iaX] = x;
    InteractPoint[slot][iaY] = y;
    InteractPoint[slot][iaZ] = z;
    InteractPoint[slot][iaRadius] = radius;
    format(InteractPoint[slot][iaLabelText], 64, "%s", labelText);

    InteractPoint[slot][iaArea] = CreateDynamicCylinder(x, y, z - 3.0, z + 4.0, radius, .priority = 100);
    InteractPoint[slot][iaLabel] = CreateDynamic3DTextLabel(labelText, UI_ACCENT_PRIMARY,
        x, y, z + 1.2, 15.0, .testlos = 1);

    InteractCount++;
    return slot;
}

stock Interact_NearestForPlayer(playerid, &Float:outDistance)
{
    new bestSlot = -1;
    outDistance = 99999.0;

    new Float:px, Float:py, Float:pz;
    GetPlayerPos(playerid, px, py, pz);

    for (new slot = 0; slot < MAX_INTERACTS; slot++)
    {
        if (!InteractPoint[slot][iaActive]) continue;

        new Float:distance = VectorSize(px - InteractPoint[slot][iaX],
                                        py - InteractPoint[slot][iaY],
                                        pz - InteractPoint[slot][iaZ]);
        distance -= InteractPoint[slot][iaRadius];
        if (distance < 0.0) distance = 0.0;

        if (distance < outDistance)
        {
            outDistance = distance;
            bestSlot = slot;
        }
    }
    return bestSlot;
}

stock Interact_Execute(playerid, slot)
{
    if (slot < 0 || slot >= MAX_INTERACTS) return 0;
    if (!InteractPoint[slot][iaActive]) return 0;

    switch (InteractPoint[slot][iaKind])
    {
        case INTERACT_EXTRACT:
        {
            Extraction_Try(playerid);
            return 1;
        }

        case INTERACT_STASH:
        {
            if (!Zone_IsHub(Player_GetZone(playerid)))
            {
                Toast_Show(playerid, "Stash", "Stash hanya bisa diakses di kamp karantina.", UI_ACCENT_WARN);
                return 0;
            }

            Player_Heal(playerid, FULL_HP);
            Player_CureInfection(playerid, 10.0);
            Stash_Open(playerid);
            return 1;
        }

        case INTERACT_CRAFT:
        {
            CraftUI_Show(playerid);
            return 1;
        }
        case INTERACT_GARAGE:
        {
            Garage_Show(playerid);
            return 1;
        }
        case INTERACT_FUEL:
        {
            Garage_PumpInteract(playerid);
            return 1;
        }
        case INTERACT_SHOP:
        {
            Vendor_ShowMenu(playerid);
            return 1;
        }
    }
    return 0;
}

forward CraftUI_Show(playerid);

stock Interact_HandlePlayer(playerid)
{
    // Mayat elite didahulukan: dialog, bukan item yang harus dicari di tanah.
    if (Corpse_TryOpenNearest(playerid)) return 1;
    if (Grave_TryOpenNearest(playerid)) return 1;

    // Loot lebih dekat dan lebih sering dipakai daripada hotspot tetap.
    if (Loot_TakeNearest(playerid, 2.5)) return 1;

    new Float:distance;
    new slot = Interact_NearestForPlayer(playerid, distance);
    if (slot == -1 || distance > 2.0) return 0;
    return Interact_Execute(playerid, slot);
}

public OnPlayerKeyStateChange(playerid, newkeys, oldkeys)
{
    #pragma unused oldkeys

    if ((newkeys & KEY_YES) && !(oldkeys & KEY_YES))
    {
        Interact_HandlePlayer(playerid);
    }
    return 1;
}

public OnPlayerEnterDynamicArea(playerid, STREAMER_TAG_AREA:areaid)
{
    for (new slot = 0; slot < MAX_INTERACTS; slot++)
    {
        if (!InteractPoint[slot][iaActive]) continue;
        if (InteractPoint[slot][iaArea] != areaid) continue;

        GameTextForPlayer(playerid, "~g~[Y]~w~ Gunakan", 2000, 3);
        return 1;
    }
    return 1;
}
