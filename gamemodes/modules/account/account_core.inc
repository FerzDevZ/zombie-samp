#if defined _ls_account_core_included
    #endinput
#endif
#define _ls_account_core_included

/* Autentikasi: setiap pemain harus melewati register/login sebelum menyentuh dunia.
   Semua I/O MySQL asinkron (mysql_tquery) dan hashing berjalan di thread bcrypt. */

#define DIALOG_AUTH_REGISTER   2001
#define DIALOG_AUTH_LOGIN      2002
#define DIALOG_AUTH_CHARSEL    2003
#define DIALOG_AUTH_CHARNEW    2004
#define DIALOG_AUTH_INFO       2005

#define MAX_LOGIN_ATTEMPTS     5
#define MIN_PASSWORD_LENGTH    6
#define MAX_PASSWORD_LENGTH    40
#define MAX_CHARACTER_SLOTS    2
#define MIN_NAME_LENGTH        3
#define MAX_NAME_LENGTH        20

enum E_AUTH_STAGE
{
    AUTH_LOADING = 0,
    AUTH_REGISTER,
    AUTH_LOGIN,
    AUTH_CHARSELECT,
    AUTH_PLAYING
};

new AccountId[MAX_PLAYERS];
new ActiveCharId[MAX_PLAYERS];
new E_AUTH_STAGE:AuthStage[MAX_PLAYERS];
new StoredPasswordHash[MAX_PLAYERS][BCRYPT_HASH_LENGTH];

/* Sebagian klien (SA-MP Mobile) tidak mengirim teks dialog sama sekali: server
   menerima inputtext kosong. Setelah itu terjadi sekali, semua langkah input
   dialihkan ke chat supaya pemain tidak terjebak dialog yang tidak bisa diketik. */
new ChatAuthOnly[MAX_PLAYERS];

stock Account_MarkChatAuth(playerid)
{
    ChatAuthOnly[playerid] = 1;
    return 1;
}

stock Account_UsesChatAuth(playerid)
{
    return ChatAuthOnly[playerid];
}

stock Account_ResetChatAuth(playerid)
{
    ChatAuthOnly[playerid] = 0;
    return 1;
}
new LoginAttempts[MAX_PLAYERS];
new PendingCharacterSlot[MAX_PLAYERS];
new CharacterCount[MAX_PLAYERS];
new CharacterDBSlot[MAX_PLAYERS][MAX_CHARACTER_SLOTS];
new CharacterDBId[MAX_PLAYERS][MAX_CHARACTER_SLOTS];

forward Account_OnLookup(playerid);
forward Account_OnPasswordHashed(playerid);
forward Account_OnAccountInserted(playerid);
forward Account_OnPasswordVerified(playerid, bool:success);
forward Account_OnCharactersLoaded(playerid);
forward Account_OnPassChangeVerified(playerid, bool:success);
forward Account_OnPasswordChangeHashed(playerid);
forward Account_OnPasswordChanged(playerid);
forward Account_OnRecoveryHashed(playerid);
forward CharCreate_OnCreated(playerid);
forward Account_OnCharacterCreated(playerid);
forward Account_OnCharacterLoaded(playerid);
forward Account_OnSaved(playerid);
forward Account_AutosaveTick();

/* Stagger autosave: satu tick tidak boleh menyentuh semua pemain sekaligus. */
new AccountLastSave[MAX_PLAYERS];

stock bool:Account_IsPlaying(playerid)
{
    return AuthStage[playerid] == AUTH_PLAYING;
}

/* Terautentikasi = password diverifikasi (login) atau akun baru dibuat
   (registrasi). Lookup nick saja TIDAK cukup: nick SA-MP tidak terautentikasi,
   siapa pun bisa connect memakai nick orang lain saat pemilik offline. */
new bool:AccountAuthed[MAX_PLAYERS];

/* Generasi koneksi: naik tiap OnPlayerConnect. Callback asyncregistrasi
   menangkap generasi saat request; bila slot sudah dihuni orang lain saat
   callback tiba, INSERT dibatalkan (anti pre-hijack akun — audit F-09). */
new ConnectGen[MAX_PLAYERS];
new RegNick[MAX_PLAYERS][MAX_PLAYER_NAME + 1];
new RegGen[MAX_PLAYERS];

stock bool:Account_IsAuthed(playerid)
{
    return AccountAuthed[playerid];
}

stock Account_ResetPlayer(playerid)
{
    AccountId[playerid] = 0;
    ActiveCharId[playerid] = 0;
    AuthStage[playerid] = AUTH_LOADING;
    AccountAuthed[playerid] = false;
    StoredPasswordHash[playerid][0] = EOS;
    LoginAttempts[playerid] = 0;
    PendingCharacterSlot[playerid] = -1;
    CharacterCount[playerid] = 0;
    AccountLastSave[playerid] = 0;
    ChatAuthOnly[playerid] = 0;

    for (new slot = 0; slot < MAX_CHARACTER_SLOTS; slot++)
    {
        CharacterDBSlot[playerid][slot] = 0;
        CharacterDBId[playerid][slot] = 0;
    }
    return 1;
}

stock Account_LockPlayer(playerid)
{
    TogglePlayerControllable(playerid, false);
    SetPlayerColor(playerid, 0x777777AA);
    return 1;
}

stock Account_UnlockPlayer(playerid)
{
    TogglePlayerControllable(playerid, true);
    return 1;
}

stock bool:Account_RequirePlaying(playerid)
{
    if (Account_IsPlaying(playerid)) return true;

    SendClientMessage(playerid, UI_ACCENT_WARN, "[AKUN] Selesaikan login dulu sebelum memakai perintah ini.");
    return false;
}

/* Hanya huruf, angka, dan garis bawah: nama karakter dipakai di chat dan database. */
stock bool:Account_IsValidCharacterName(const input[])
{
    new length = strlen(input);
    if (length < MIN_NAME_LENGTH || length > MAX_NAME_LENGTH) return false;

    for (new index = 0; index < length; index++)
    {
        new character = input[index];
        if ((character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z')) continue;
        if (character >= '0' && character <= '9') continue;
        if (character == '_') continue;
        return false;
    }
    return true;
}

stock bool:Account_IsValidPassword(const input[])
{
    new length = strlen(input);
    return (length >= MIN_PASSWORD_LENGTH && length <= MAX_PASSWORD_LENGTH);
}
