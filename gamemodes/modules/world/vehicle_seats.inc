#if defined _ls_vehicle_seats_included
    #endinput
#endif
#define _ls_vehicle_seats_included

/* Kapasitas TOTAL kursi per model kendaraan (400..611), diturunkan dari tabel
   YSI y_vehicledata (VIM_Seats) dengan offset +1 terhadap indeks model.
   Verifikasi: 400 Landstalker=4, 411 Infernus=2, 429 Banshee=2, 431 Bus=9,
   437 Coach=9, 522 NRG=2. Native SA-MP 0.3.7 tidak punya
   GetVehicleMaxPassengers, jadi tabel inilah sumbernya. */
new const gVehicleSeats[] =
{
    4, 2, 2, 2, 4, 4, 1, 2, 2, 4, 2, 2, 2, 4, 2, 2,
    4, 2, 4, 2, 4, 2, 2, 2, 2, 1, 4, 4, 4, 2, 1, 9,
    1, 2, 2, 1, 2, 9, 4, 2, 4, 1, 2, 2, 2, 4, 1, 2,
    1, 2, 1, 2, 1, 1, 1, 2, 2, 2, 4, 4, 2, 2, 2, 2,
    1, 1, 4, 4, 2, 2, 4, 2, 1, 1, 2, 2, 1, 2, 2, 4,
    2, 1, 2, 3, 1, 1, 1, 4, 2, 2, 4, 2, 4, 1, 2, 2,
    2, 4, 4, 2, 2, 1, 2, 2, 2, 2, 2, 4, 2, 1, 1, 2,
    1, 1, 2, 2, 4, 2, 2, 1, 1, 2, 2, 2, 2, 2, 2, 2,
    2, 4, 1, 1, 1, 2, 2, 2, 2, 2, 2, 1, 4, 2, 2, 2,
    2, 2, 4, 4, 2, 2, 4, 4, 2, 1, 2, 2, 2, 2, 2, 2,
    4, 4, 2, 2, 1, 2, 4, 4, 1, 1, 1, 1, 1, 2, 1, 2,
    2, 2, 2, 4, 4, 2, 4, 1, 1, 4, 2, 2, 2, 2, 1, 1,
    2, 2, 1, 1, 4, 4, 4, 2, 2, 2, 2, 2, 4, 2, 1, 1,
    1, 4, 1, 1
};

stock VehicleSeatsTotal(model)
{
    if (model < 400 || model > 611) return 4;
    return gVehicleSeats[model - 400];
}

/* Jumlah kursi penumpang (0 = kendaraan tanpa kursi penumpang, mis. sepeda). */
stock VehiclePassengerSeats(model)
{
    new total = VehicleSeatsTotal(model);
    if (total <= 1) return 0;
    return total - 1;
}
