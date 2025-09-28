#ifndef OTA_LIB_H
#define OTA_LIB_H

// =====================================================
// OTA UPDATE LIBRARY - HTTP Client Implementation
// =====================================================

#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "esp_ota_ops.h"
#include <esp_log.h>

// =====================================================
// CONSTANTES Y CONFIGURACIONES OTA
// =====================================================

#define OTA_TAG                         "OTA_LIB"
#define OTA_URL_MAX_LENGTH              100
#define OTA_BUFFER_SIZE                 4096
#define OTA_TIMEOUT_MS                  30000  // 30 segundos
#define OTA_PROGRESS_LOG_INTERVAL       1000   // Log cada 1KB

// Estados de la actualización OTA
typedef enum {
    OTA_STATE_IDLE = 0,
    OTA_STATE_STARTING,
    OTA_STATE_CONNECTING,
    OTA_STATE_DOWNLOADING,
    OTA_STATE_WRITING,
    OTA_STATE_FINISHED,
    OTA_STATE_ERROR
} ota_state_t;

// Estructura de configuración OTA
typedef struct {
    char url[OTA_URL_MAX_LENGTH];           // URL del binario a descargar
    ota_state_t state;                      // Estado actual de la actualización
    size_t total_size;                      // Tamaño total del archivo
    size_t downloaded_size;                 // Bytes descargados
    uint32_t last_progress_log;             // Timestamp del último log de progreso
    bool validation_enabled;                // Validar certificado SSL
} ota_config_t;

// =====================================================
// FUNCIONES PÚBLICAS OTA
// =====================================================

/**
 * @brief Inicializa la librería OTA
 * @return ESP_OK si se inicializó correctamente, error en caso contrario
 */
esp_err_t ota_init(void);

/**
 * @brief Valida si una URL es válida para OTA
 * @param url URL a validar
 * @return true si es válida, false en caso contrario
 */
bool ota_validate_url(const char* url);

/**
 * @brief Inicia actualización OTA desde URL HTTP/HTTPS
 * @param url URL del binario a descargar
 * @return ESP_OK si inició correctamente, error en caso contrario
 */
esp_err_t ota_start_update(const char* url);

/**
 * @brief Obtiene el estado actual de la actualización OTA
 * @return Estado actual de la OTA
 */
ota_state_t ota_get_state(void);

/**
 * @brief Obtiene el progreso de descarga (0-100%)
 * @return Progreso en porcentaje
 */
int ota_get_progress(void);

/**
 * @brief Obtiene información detallada de la configuración OTA
 * @return Puntero a la configuración OTA actual
 */
ota_config_t* ota_get_config(void);

/**
 * @brief Cancela la actualización OTA en progreso
 * @return ESP_OK si se canceló correctamente
 */
esp_err_t ota_cancel_update(void);

/**
 * @brief Procesa un comando OTA recibido por MQTT
 * @param url URL del binario OTA
 * @return ESP_OK si se procesó correctamente
 */
esp_err_t ota_process_command(const char* url);

/**
 * @brief Verifica si hay una actualización OTA en progreso
 * @return true si hay OTA en progreso, false en caso contrario
 */
bool ota_is_update_in_progress(void);

#endif // OTA_LIB_H
