// === button_actions.h ===
#ifndef BUTTON_ACTIONS_H
#define BUTTON_ACTIONS_H

#include <stdint.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "conexion.h"
#include "mqtt_client.h"
#include "esp_log.h"
#include "HALO.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <string.h>
// Constantes para detección del botón
#define BUTTON_DEBOUNCE_MS          100    // Tiempo de debounce
#define BUTTON_LONG_PRESS_MS        5000   // Tiempo para pulsación larga (5 segundos) - SmartConfig
#define BUTTON_DOUBLE_CLICK_MS      1000   // Tiempo máximo entre doble clic
#define BUTTON_STATE_TIMEOUT_MS     10000  // Timeout para cambio de estado
#define SMARTCONFIG_USER_TIMEOUT_MS 10000  // Timeout de 10 segundos para SmartConfig

// Estados del botón
typedef enum {
    BUTTON_STATE_IDLE = 0,           // Estado inicial
    BUTTON_STATE_DEBOUNCE,           // En proceso de debounce
    BUTTON_STATE_PRESSED,            // Botón presionado
    BUTTON_STATE_LONG_PRESS,         // Pulsación larga detectada
    BUTTON_STATE_RELEASE_WAIT,       // Esperando liberación
    BUTTON_STATE_DOUBLE_CLICK_WAIT   // Esperando posible doble clic
} button_state_t;

// Estructura para manejo del botón
typedef struct {
    button_state_t state;            // Estado actual del botón
    uint32_t press_start_time;       // Tiempo de inicio de pulsación
    uint32_t last_press_time;        // Tiempo de última pulsación
    uint32_t release_time;           // Tiempo de liberación
    bool long_press_detected;        // Flag de pulsación larga
    bool double_click_detected;      // Flag de doble clic
    uint8_t click_count;             // Contador de clics
} button_handler_t;

// Funciones públicas
void manejar_pulsacion_larga(void);
void long_press_task(void* arg);
void manejar_click_doble(void);
void manejar_click_simple(void);
void connection_task(void* arg);
void publicar_estado(const char *topic, const char *mensaje);
#endif // BUTTON_ACTIONS_H
