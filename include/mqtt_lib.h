#ifndef MQTT_LIB_H
#define MQTT_LIB_H

#include <esp_err.h>
#include "mqtt_client.h"
#include <time.h>

// Variables globales externas
extern esp_mqtt_client_handle_t mqtt_client;

// Funciones de la librería MQTT
void mqtt_init(void);
esp_err_t mqtt_enviar_datos(float peso, struct tm *timeinfo, const char* mensaje);
void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data);
bool mqtt_is_connected(void);
esp_err_t mqtt_test_connection(void);

// Funciones de procesamiento de comandos
void menu_mqtt(const char* comando);

// Función de rollback OTA (declaración directa)
esp_err_t ota_rollback_to_previous(void);

// Constantes de tamaño para MQTT
#define MAX_MQTT_DATA_LENGTH 512

#endif // MQTT_LIB_H 