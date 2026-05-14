#ifndef BLINKY_TEM_HUM_H
#define BLINKY_TEM_HUM_H

// ================================
// Temp / humidity functions
// ================================

void tem_hum_init(void);
void tem_hum_update(void);

double tem_hum_get_temp(void);
double tem_hum_get_hum(void);

#endif