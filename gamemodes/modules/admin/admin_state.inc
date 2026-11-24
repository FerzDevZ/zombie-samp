/* State superadmin yang harus terlihat jalur damage.

   File ini sengaja dipisah dan di-include SEBELUM `combat/damage_pipeline.inc`,
   karena Pawn satu pass: variabel harus dideklarasikan sebelum dipakai. */

#if defined _ls_admin_state_included
    #endinput
#endif
#define _ls_admin_state_included

new AdminGod[MAX_PLAYERS];
/* Dipindah ke sini dari superadmin.inc: achecks.inc (include lebih awal)
   memakai AdminFrozen untuk AC_Unfreeze (audit F-16). */
new AdminFrozen[MAX_PLAYERS];

stock bool:Admin_IsGod(playerid)
{
    if (playerid < 0 || playerid >= MAX_PLAYERS) return false;
    return AdminGod[playerid] != 0;
}

stock Admin_ResetGod(playerid)
{
    if (playerid < 0 || playerid >= MAX_PLAYERS) return 0;
    AdminGod[playerid] = 0;
    return 1;
}