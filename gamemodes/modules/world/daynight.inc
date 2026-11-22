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
