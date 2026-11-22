#if defined _ls_damage_contract_included
    #endinput
#endif
#define _ls_damage_contract_included

/* Shared contract so the zombie module and the damage pipeline can reference each
   other without include cycles. */
enum E_DAMAGE_KIND
{
    DAMAGE_KIND_PLAYER = 0,
    DAMAGE_KIND_ZOMBIE
};

forward bool:Damage_Apply(E_DAMAGE_KIND:kind, victimId, attackerId, Float:amount, weaponId, bodyPart);
