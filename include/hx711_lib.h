#ifndef HX711_LIB_H
#define HX711_LIB_H

#include <esp_err.h>
#include <stdbool.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "mqtt_client.h"
#include <time.h>

typedef enum
{
    HX711_MODE_CHANNEL_A_GAIN_128 = 0x01,
    HX711_MODE_CHANNEL_B_GAIN_32  = 0x02,
    HX711_MODE_CHANNEL_A_GAIN_64  = 0x03,
} hx711_mode_t;

typedef struct hx711_handle_s
{
    uint8_t (*bus_init)(void);
    uint8_t (*bus_deinit)(void);
    uint8_t (*bus_read)(uint8_t *value);
    uint8_t (*clock_init)(void);
    uint8_t (*clock_deinit)(void);
    uint8_t (*clock_write)(uint8_t value);
    void (*delay_us)(uint32_t us);
    void (*enable_irq)(void);
    void (*disable_irq)(void);
    void (*debug_print)(const char *const fmt, ...);
    uint8_t inited;
    uint8_t mode;
} hx711_handle_t;

typedef struct hx711_info_s
{
    char chip_name[32];
    char manufacturer_name[32];
    char interface[8];
    float supply_voltage_min_v;
    float supply_voltage_max_v;
    float max_current_ma;
    float temperature_min;
    float temperature_max;
    uint32_t driver_version;
} hx711_info_t;

#define DRIVER_HX711_LINK_INIT(HANDLE, STRUCTURE)   memset(HANDLE, 0, sizeof(STRUCTURE))
#define DRIVER_HX711_LINK_BUS_INIT(HANDLE, FUC)    (HANDLE)->bus_init = FUC
#define DRIVER_HX711_LINK_BUS_DEINIT(HANDLE, FUC)  (HANDLE)->bus_deinit = FUC
#define DRIVER_HX711_LINK_BUS_READ(HANDLE, FUC)    (HANDLE)->bus_read = FUC
#define DRIVER_HX711_LINK_CLOCK_INIT(HANDLE, FUC)  (HANDLE)->clock_init = FUC
#define DRIVER_HX711_LINK_CLOCK_DEINIT(HANDLE, FUC)(HANDLE)->clock_deinit = FUC
#define DRIVER_HX711_LINK_CLOCK_WRITE(HANDLE, FUC) (HANDLE)->clock_write = FUC
#define DRIVER_HX711_LINK_DELAY_US(HANDLE, FUC)    (HANDLE)->delay_us = FUC
#define DRIVER_HX711_LINK_ENABLE_IRQ(HANDLE, FUC)  (HANDLE)->enable_irq = FUC
#define DRIVER_HX711_LINK_DISABLE_IRQ(HANDLE, FUC) (HANDLE)->disable_irq = FUC
#define DRIVER_HX711_LINK_DEBUG_PRINT(HANDLE, FUC) (HANDLE)->debug_print = FUC

// Variables globales externas
extern hx711_handle_t hx711;
extern int32_t offset;
extern float scale;
// extern esp_mqtt_client_handle_t mqtt_client;

// Funciones de la librería HX711
void init_HX711(void);
float hx711_leer_peso(void);
void hx711_calibrar_inicial(void);
void hx711_continuar_calibracion_peso(void);
////void write_weight_to_sd(float peso, struct tm *timeinfo);

// Funciones de persistencia de calibración
esp_err_t hx711_guardar_calibracion(void);
esp_err_t hx711_cargar_calibracion(void);
int hx711_tiene_calibracion_guardada(void);

// Función para verificar conexión MQTT
bool mqtt_is_connected(void);

#endif // HX711_LIB_H