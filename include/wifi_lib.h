#ifndef WIFI_LIB_H
#define WIFI_LIB_H

#include <esp_err.h>
#include <esp_event.h>
#include <esp_wifi.h>
#include <stdbool.h>
#include "HALO.h"
#include "smartconfig.h"
#include <esp_log.h>
#include <esp_netif.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <string.h>

// Variables globales externas
extern EventGroupHandle_t s_wifi_event_group;
extern int s_retry_num;

// Funciones de la librería WiFi
void wifi_init_sta(void);
void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
bool wifi_is_connected(void);
void wifi_attempt_reconnect(void);
void wifi_set_auto_reconnect(bool enable);
esp_err_t wifi_get_rssi(int8_t *rssi);

#endif // WIFI_LIB_H 