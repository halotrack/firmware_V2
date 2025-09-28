#ifndef RTC_LIB_H
#define RTC_LIB_H

#include <time.h>
#include <esp_err.h>
#include <i2cdev.h>
#include "HALO.h"
#include <esp_log.h>
#include <sys/time.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/i2c.h>
////#include <esp_idf_lib_helpers.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

// Variables globales externas
extern i2c_dev_t rtc_dev;

// Funciones de la librería RTC
void init_RTC(void);
bool rtc_get_time(struct tm *timeinfo);
bool rtc_set_time(struct tm *timeinfo);
void rtc_configurar_zona_horaria(void);
void rtc_set_system_time_from_rtc(void);

#endif // RTC_LIB_H 