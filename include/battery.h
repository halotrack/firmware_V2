#ifndef BATTERY_H
#define BATTERY_H

#include "bq27427.h"
#include <time.h>

extern i2c_dev_t fuel_gauge_dev;

esp_err_t init_battery(void);
esp_err_t battery_get_voltage(uint16_t *voltage);
esp_err_t battery_send_voltage(void);
void sdcard_log_voltaje(struct tm *timeinfo);

#endif // BATTERY_H