#if defined _ls_daynight_included
    #endinput
#endif
#define _ls_daynight_included

/* 18 real minutes per in-game day keeps a full night inside a play session. */
#define WORLD_HOUR_TICK_MS 45000

forward World_Tick();

new WorldHour = 8;
new WorldDay = 1;

stock World_IsNight()
{
    return (WorldHour >= 20 || WorldHour < 6);
}

stock World_GetHour()
{
    return WorldHour;
}

new WorldWeatherOverride = -1;

/* Cuaca manual menimpa siklus otomatis sampai di-reset dengan /weather auto.
   Tanpa override ini, World_Tick akan menimpanya kembali satu jam kemudian. */
stock World_SetWeather(weatherId)
{
    if (weatherId < -1 || weatherId > 20) return 0;
    WorldWeatherOverride = weatherId;
    if (weatherId >= 0) SetWeather(weatherId);
    return 1;
}

stock World_GetWeatherOverride()
{
    return WorldWeatherOverride;
}

/* Setter admin: memaksa jam dunia lalu menerapkan cuaca yang sesuai. */
stock World_SetHour(hour)
{
    if (hour < 0) hour = 0;
    if (hour > 23) hour = 23;

    WorldHour = hour;
    SetWorldTime(WorldHour);
    SetWeather(World_IsNight() ? 8 : 1);
    return WorldHour;
}

public World_Tick()
{
    WorldHour++;
    if (WorldHour >= 24)
    {
        WorldHour = 0;
        WorldDay++;
    }

    SetWorldTime(WorldHour);

    /* Jam per pemain harus diperbarui: Nightfall memakai SetPlayerTime sendiri
       dan SetWorldTime global tidak menyentuhnya. Iterasinya ada di modul lobby
       karena PlayerPool baru dideklarasikan setelah file ini. */
    Lobby_ReapplyClocks();

    if (WorldWeatherOverride >= 0)
    {
        SetWeather(WorldWeatherOverride);
        return 1;
    }

    if (World_IsNight())
    {
        SetWeather(8);
    }
    else
    {
        SetWeather(1);
    }
    return 1;
}
