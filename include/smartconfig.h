#ifndef SMARTCONFIG_H
#define SMARTCONFIG_H

#include "esp_err.h"
#include <stdbool.h>
#include <stddef.h>
#include "esp_smartconfig.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include <string.h>
#include "nvs_flash.h"
// =====================================================
// FUNCIONES DE SMARTCONFIG
// =====================================================

/**
 * @brief Inicializa el sistema SmartConfig
 * 
 * Configura los manejadores de eventos y grupos de eventos necesarios
 * para el funcionamiento de SmartConfig.
 * 
 * @return ESP_OK si la inicialización es exitosa
 *         ESP_ERR_NO_MEM si no hay memoria para crear grupos de eventos
 */
esp_err_t smartconfig_init(void);

/**
 * @brief Inicia el proceso de SmartConfig
 * 
 * Pone el ESP32 en modo de escucha para recibir credenciales WiFi
 * desde una aplicación móvil usando ESPTouch.
 * 
 * @return ESP_OK si SmartConfig se inició correctamente
 *         ESP_ERR_INVALID_STATE si ya está activo
 */
esp_err_t smartconfig_start(void);

/**
 * @brief Detiene el proceso de SmartConfig
 * 
 * Termina el modo de escucha y libera los recursos asociados.
 * 
 * @return ESP_OK si SmartConfig se detuvo correctamente
 */
esp_err_t smartconfig_stop(void);

/**
 * @brief Verifica si SmartConfig está activo
 * 
 * @return true si SmartConfig está en ejecución
 *         false si no está activo
 */
bool smartconfig_is_active(void);

/**
 * @brief Guarda las credenciales WiFi en NVS
 * 
 * @param ssid Nombre de la red WiFi
 * @param password Contraseña de la red WiFi
 * @return ESP_OK si las credenciales se guardaron correctamente
 *         ESP_ERR_INVALID_ARG si los parámetros son inválidos
 *         ESP_ERR_NVS_* si hay errores de NVS
 */
esp_err_t smartconfig_save_credentials(const char *ssid, const char *password);

/**
 * @brief Carga las credenciales WiFi desde NVS
 * 
 * @param ssid Buffer para almacenar el SSID
 * @param password Buffer para almacenar la contraseña
 * @param max_len Tamaño máximo de los buffers
 * @return ESP_OK si las credenciales se cargaron correctamente
 *         ESP_ERR_INVALID_ARG si los parámetros son inválidos
 *         ESP_ERR_NVS_NOT_FOUND si no hay credenciales guardadas
 */
esp_err_t smartconfig_load_credentials(char *ssid, char *password, size_t max_len);
// Manejo de múltiples credenciales
size_t smartconfig_get_saved_credentials_count(void);
esp_err_t smartconfig_load_credentials_index(int index, char *ssid, char *password, size_t max_len);

/**
 * @brief Verifica si hay credenciales WiFi guardadas
 * 
 * @return true si hay credenciales guardadas en NVS
 *         false si no hay credenciales
 */
bool smartconfig_has_saved_credentials(void);

/**
 * @brief Elimina las credenciales WiFi guardadas
 * 
 * @return ESP_OK si las credenciales se eliminaron correctamente
 *         ESP_ERR_NVS_* si hay errores de NVS
 */
esp_err_t smartconfig_clear_credentials(void);

// Funciones eliminadas - SmartConfig solo se activa con pulsación larga del botón

#endif // SMARTCONFIG_H 