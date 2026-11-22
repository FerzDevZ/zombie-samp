#if defined _ls_damage_pipeline_included
    #endinput
#endif
#define _ls_damage_pipeline_included

/* Single entry point for every damage event. No module is allowed to touch
   player or zombie health directly: skipping validation here is how silent-aim,
   wallbang and PvP-in-safe-zone exploit all start. */

#define DAMAGE_EVENTS_PER_SECOND 12
new DamageEventCount[MAX_PLAYERS];
new DamageEventWindow[MAX_PLAYERS];

stock bool:Damage_RateLimitOk(attackerId)
{
    if (attackerId == INVALID_PLAYER_ID) return true;

    new now = GetTickCount();
    if (now - DamageEventWindow[attackerId] > 1000)
    {
        DamageEventWindow[attackerId] = now;
        DamageEventCount[attackerId] = 0;
    }

    DamageEventCount[attackerId]++;
    if (DamageEventCount[attackerId] <= DAMAGE_EVENTS_PER_SECOND) return true;

    new payload[64];
    format(payload, sizeof(payload), "dps=%d/s", DamageEventCount[attackerId]);
    Anticheat_Flag(attackerId, "DPS_SPIKE", payload);
    return false;
}

stock bool:Damage_ValidatePlayerToPlayer(attackerId, victimId, Float:amount, weaponId)
{
    if (attackerId == victimId) return false;

    // Friendly fire is only disabled inside the safe hub.
    if (Zone_IsHub(Player_GetZone(attackerId))) return false;
    if (Zone_IsHub(Player_GetZone(victimId))) return false;

    /* Zona off-PvP ditegakkan (sebelumnya Zone_IsPvP tak pernah dipanggil —
       audit D4): PvP hanya sah bila KEDUA pemain berada di zona PvP.
       Wasteland (INVALID_ZONE) dianggap PvP oleh Zone_IsPvP. */
    if (!Zone_IsPvP(Player_GetZone(attackerId))) return false;
    if (!Zone_IsPvP(Player_GetZone(victimId))) return false;

    if (Player_HasGrace(victimId)) return false;
    if (Player_IsDowned(victimId)) return false;

    /* Sumber non-senjata (kendaraan/ledakan/jatuh, id >= 41) bukan bidikan:
       jarak/LOS tidak relevan (granat memantul, tabrakan terjadi kontak).
       Cek party/hub/grace/downed di atas tetap berlaku. */
    if (weaponId >= 41) return true;

    new Float:ax, Float:ay, Float:az, Float:vx, Float:vy, Float:vz;
    GetPlayerPos(attackerId, ax, ay, az);
    GetPlayerPos(victimId, vx, vy, vz);

    new Float:distance = VectorSize(vx - ax, vy - ay, vz - az);
    if (distance > Weapon_GetMaxRange(weaponId))
    {
        Anticheat_Flag(attackerId, "RANGE_FAIL", "shot beyond weapon range");
        return false;
    }

    if (!Weapon_IsMelee(weaponId))
    {
        if (!LOS_IsClear(ax, ay, az + 0.7, vx, vy, vz + 0.6))
        {
            Anticheat_Flag(attackerId, "LOS_FAIL", "wallbang attempt");
            return false;
        }
    }

    #pragma unused amount

    return true;
}

bool:Damage_Apply(E_DAMAGE_KIND:kind, victimId, attackerId, Float:amount, weaponId, bodyPart)
{
    #pragma unused bodyPart

    if (amount <= 0.0) return false;
    if (!Damage_RateLimitOk(attackerId)) return false;

    switch (kind)
    {
        case DAMAGE_KIND_PLAYER:
        {
            if (!IsPlayerConnected(victimId)) return false;
            if (!Player_IsAlive(victimId)) return false;

            /* Kebal superadmin: diblokir di choke point yang sama dengan damage lain,
               jadi kelaparan/haus/zombie/api juga tidak mengurangi HP. */
            if (Admin_IsGod(victimId)) return false;

            if (attackerId != INVALID_PLAYER_ID)
            {
                /* Party: satu-satunya jalur damage pemain-ke-pemain. Ditaruh di sini
                   supaya ledakan/kendaraan juga tidak melukai anggota party. */
                if (Party_IsSameParty(attackerId, victimId)) return false;

                /* Clamp damage PvP ke base senjata x2 (margin headshot/skill).
                   Sumber non-senjata (kendaraan/ledakan/jatuh, id >= 41) dikecualikan
                   karena damage sah mereka bisa di atas base senjata apa pun. */
                if (weaponId >= 0 && weaponId <= 40)
                {
                    new Float:weaponCap = Weapon_GetBaseDamage(weaponId) * 2.0;
                    if (amount > weaponCap)
                    {
                        new capPayload[64];
                        format(capPayload, sizeof(capPayload), "weapon=%d amount=%.1f cap=%.1f", weaponId, amount, weaponCap);
                        Anticheat_Flag(attackerId, "DAMAGE_CAP", capPayload);
                        amount = weaponCap;
                    }
                }

                if (!Damage_ValidatePlayerToPlayer(attackerId, victimId, amount, weaponId)) return false;
            }

            new Float:incoming = amount;
            if (attackerId == INVALID_PLAYER_ID) incoming *= Skills_DamageTakenMultiplier(victimId);

            Player_ApplyDamage(victimId, incoming, attackerId, false);

            if (Player_GetHealth(victimId) <= 0.0)
            {
                Player_EnterDownedState(victimId);
            }
            return true;
        }

        case DAMAGE_KIND_ZOMBIE:
        {
            if (!Zombie_IsActive(victimId)) return false;

            new Float:outgoing = amount;
            if (attackerId != INVALID_PLAYER_ID && IsPlayerConnected(attackerId))
            {
                if (!Dps_TrackPlayerDamage(attackerId, amount, weaponId)) return false;
                outgoing *= Skills_DamageDealtMultiplier(attackerId);
            }
            return Zombie_ApplyDamage(victimId, attackerId, outgoing, weaponId);
        }
    }
    return false;
}
