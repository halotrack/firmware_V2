#include "../include/ota_lib.h"
#include "../include/HALO.h"
#include <string.h>
#include "esp_tls.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"

// =====================================================
// VARIABLES GLOBALES OTA
// =====================================================

static ota_config_t ota_config = {
    .url = {0},
    .state = OTA_STATE_IDLE,
    .total_size = 0,
    .downloaded_size = 0,
    .last_progress_log = 0,
    .validation_enabled = true
};

// =====================================================
// FUNCIONES PRIVADAS AUXILIARES
// =====================================================

/**
 * @brief Valida formato de URL HTTP/HTTPS
 * @param url URL a validar
 * @return true si es válida, false en caso contrario
 */
static bool ota_validate_url_format(const char* url) {
    if (url == NULL || strlen(url) == 0 || strlen(url) >= OTA_URL_MAX_LENGTH) {
        ESP_LOGE(OTA_TAG, "❌ URL inválida: NULL, vacía o demasiado larga");
        return false;
    }

    // Verificar que empiece con http:// o https://
    if (strncmp(url, "http://", 7) != 0 && strncmp(url, "https://", 8) != 0) {
        ESP_LOGE(OTA_TAG, "❌ URL debe comenzar con http:// o https://");
        return false;
    }

    // Verificar que termine con .bin
    if (!strstr(url, ".bin")) {
        ESP_LOGE(OTA_TAG, "❌ URL debe apuntar a un archivo .bin");
        return false;
    }

    ESP_LOGI(OTA_TAG, "✅ URL válida: %s", url);
    return true;
}

/**
 * @brief Manejador de eventos HTTP para OTA
 * @param evt Evento HTTP
 * @return ESP_OK
 */
static esp_err_t ota_http_event_handler(esp_http_client_event_t *evt) {
    switch (evt->event_id) {
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGI(OTA_TAG, "🌐 Conectado al servidor HTTP");
            ota_config.state = OTA_STATE_CONNECTING;
            break;

        case HTTP_EVENT_HEADER_SENT:
            ESP_LOGI(OTA_TAG, "📤 Headers HTTP enviados");
            break;

        case HTTP_EVENT_ON_HEADER:
            // Verificar tamaño del archivo si está disponible
            if (strcmp(evt->header_key, "Content-Length") == 0) {
                ota_config.total_size = atoi(evt->header_value);
                ESP_LOGI(OTA_TAG, "📊 Tamaño del archivo: %d bytes", ota_config.total_size);
            }
            break;

        case HTTP_EVENT_ON_DATA:
            if (evt->data_len > 0) {
                ESP_LOGI(OTA_TAG, "📥 Recibiendo datos: %d bytes", evt->data_len);
                ota_config.downloaded_size += evt->data_len;

                // Log de progreso cada OTA_PROGRESS_LOG_INTERVAL bytes
                uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
                if (current_time - ota_config.last_progress_log >= OTA_PROGRESS_LOG_INTERVAL) {
                    int progress = 0;
                    if (ota_config.total_size > 0) {
                        progress = (ota_config.downloaded_size * 100) / ota_config.total_size;
                    }

                    ESP_LOGI(OTA_TAG, "📥 Progreso: %d%% (%d/%d bytes)",
                            progress, ota_config.downloaded_size, ota_config.total_size);
                    ota_config.last_progress_log = current_time;
                }
            }
            break;

        case HTTP_EVENT_ON_FINISH:
            ESP_LOGI(OTA_TAG, "✅ Descarga HTTP completada");
            break;

        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGI(OTA_TAG, "🔌 Desconectado del servidor HTTP");
            break;

        case HTTP_EVENT_ERROR:
            ESP_LOGE(OTA_TAG, "❌ Error HTTP: %d", evt->event_id);
            ota_config.state = OTA_STATE_ERROR;
            break;

        default:
            break;
    }
    return ESP_OK;
}

/**
 * @brief Configura y realiza la actualización OTA
 * @param url URL del binario
 * @return ESP_OK si fue exitosa, error en caso contrario
 */
static esp_err_t ota_perform_update(const char* url) {
    esp_err_t err = ESP_OK;

    esp_http_client_config_t config = {
        .url = url,
        .event_handler = ota_http_event_handler,
        .timeout_ms = OTA_TIMEOUT_MS,
        .buffer_size = OTA_BUFFER_SIZE,
        .buffer_size_tx = 2048,
    };

    // Configurar validación SSL si es HTTPS
    if (strncmp(url, "https://", 8) == 0) {
        // Usar certificate bundle de ESP-IDF para verificación SSL
        config.crt_bundle_attach = esp_crt_bundle_attach;
        config.skip_cert_common_name_check = true;
        ESP_LOGI(OTA_TAG, "🔒 Configuración SSL con certificate bundle habilitada para HTTPS (GitHub)");
    } else {
        ESP_LOGI(OTA_TAG, "🌐 Sin SSL para HTTP");
    }

    esp_https_ota_config_t ota_config_https = {
        .http_config = &config,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        ESP_LOGE(OTA_TAG, "❌ Error al inicializar cliente HTTP");
        return ESP_FAIL;
    }

    ESP_LOGI(OTA_TAG, "🚀 Iniciando actualización OTA desde: %s", url);
    ota_config.state = OTA_STATE_STARTING;

    err = esp_https_ota(&ota_config_https);
    if (err != ESP_OK) {
        ESP_LOGE(OTA_TAG, "❌ Error durante actualización OTA: %s", esp_err_to_name(err));
        ota_config.state = OTA_STATE_ERROR;
        esp_http_client_cleanup(client);
        return err;
    }

    ESP_LOGI(OTA_TAG, "✅ Actualización OTA completada exitosamente");
    ota_config.state = OTA_STATE_FINISHED;

    esp_http_client_cleanup(client);
    return ESP_OK;
}

// =====================================================
// FUNCIONES PÚBLICAS OTA
// =====================================================

esp_err_t ota_init(void) {
    ESP_LOGI(OTA_TAG, "🔧 Inicializando librería OTA");

    // Verificar que las particiones OTA estén disponibles
    const esp_partition_t *running_partition = esp_ota_get_running_partition();
    if (running_partition == NULL) {
        ESP_LOGE(OTA_TAG, "❌ No se pudo obtener partición actual");
        return ESP_FAIL;
    }

    ESP_LOGI(OTA_TAG, "📍 Partición actual: %s", running_partition->label);

    // Obtener siguiente partición OTA disponible
    esp_ota_handle_t ota_handle;
    const esp_partition_t *update_partition = esp_ota_get_next_update_partition(NULL);

    if (update_partition == NULL) {
        ESP_LOGE(OTA_TAG, "❌ No hay partición OTA disponible");
        return ESP_FAIL;
    }

    ESP_LOGI(OTA_TAG, "📍 Partición OTA destino: %s", update_partition->label);
    ESP_LOGI(OTA_TAG, "✅ Librería OTA inicializada correctamente");

    return ESP_OK;
}

bool ota_validate_url(const char* url) {
    return ota_validate_url_format(url);
}

esp_err_t ota_start_update(const char* url) {
    if (!ota_validate_url(url)) {
        ESP_LOGE(OTA_TAG, "❌ URL inválida para OTA");
        return ESP_ERR_INVALID_ARG;
    }

    // Verificar que no hay otra OTA en progreso
    if (ota_config.state != OTA_STATE_IDLE && ota_config.state != OTA_STATE_FINISHED && ota_config.state != OTA_STATE_ERROR) {
        ESP_LOGE(OTA_TAG, "❌ Ya hay una actualización OTA en progreso");
        return ESP_ERR_INVALID_STATE;
    }

    // Copiar URL y resetear estado
    strncpy(ota_config.url, url, sizeof(ota_config.url) - 1);
    ota_config.url[sizeof(ota_config.url) - 1] = '\0';
    ota_config.state = OTA_STATE_STARTING;
    ota_config.total_size = 0;
    ota_config.downloaded_size = 0;
    ota_config.last_progress_log = 0;

    ESP_LOGI(OTA_TAG, "🎯 Iniciando actualización OTA desde: %s", url);

    // Realizar actualización en una tarea separada para no bloquear
    // Nota: En ESP32, las funciones OTA son bloqueantes pero necesarias
    return ota_perform_update(url);
}

ota_state_t ota_get_state(void) {
    return ota_config.state;
}

int ota_get_progress(void) {
    if (ota_config.total_size == 0) {
        return 0;
    }
    return (ota_config.downloaded_size * 100) / ota_config.total_size;
}

ota_config_t* ota_get_config(void) {
    return &ota_config;
}

esp_err_t ota_cancel_update(void) {
    ESP_LOGI(OTA_TAG, "⛔ Cancelando actualización OTA");

    if (ota_config.state == OTA_STATE_IDLE) {
        ESP_LOGW(OTA_TAG, "⚠️ No hay actualización OTA en progreso");
        return ESP_OK;
    }

    ota_config.state = OTA_STATE_IDLE;
    ota_config.total_size = 0;
    ota_config.downloaded_size = 0;
    ota_config.last_progress_log = 0;
    memset(ota_config.url, 0, sizeof(ota_config.url));

    ESP_LOGI(OTA_TAG, "✅ Actualización OTA cancelada");
    return ESP_OK;
}

esp_err_t ota_process_command(const char* url) {
    ESP_LOGI(OTA_TAG, "📥 Procesando comando OTA con URL: %s", url);

    // Validar URL
    if (!ota_validate_url(url)) {
        ESP_LOGE(OTA_TAG, "❌ URL inválida en comando OTA");
        return ESP_ERR_INVALID_ARG;
    }

    // Publicar estado antes de iniciar
    esp_mqtt_client_publish(mqtt_client, "esp32/halo/status", "🚀 Iniciando actualización OTA...", 0, 1, 0);

    // Iniciar actualización
    esp_err_t result = ota_start_update(url);

    if (result == ESP_OK) {
        ESP_LOGI(OTA_TAG, "✅ Actualización OTA iniciada exitosamente");
        esp_mqtt_client_publish(mqtt_client, "esp32/halo/status", "✅ Actualización OTA completada - reiniciando...", 0, 1, 0);

        // Pequeño delay antes del reinicio para que llegue el mensaje
        vTaskDelay(2000 / portTICK_PERIOD_MS);

        // Reiniciar para aplicar la actualización
        ESP_LOGI(OTA_TAG, "🔄 Reiniciando sistema para aplicar actualización OTA...");
        esp_restart();
    } else {
        ESP_LOGE(OTA_TAG, "❌ Error al iniciar actualización OTA");
        char error_msg[128];
        snprintf(error_msg, sizeof(error_msg), "❌ Error OTA: %s", esp_err_to_name(result));
        esp_mqtt_client_publish(mqtt_client, "esp32/halo/status", error_msg, 0, 1, 0);
    }

    return result;
}

bool ota_is_update_in_progress(void) {
    return (ota_config.state != OTA_STATE_IDLE &&
            ota_config.state != OTA_STATE_FINISHED &&
            ota_config.state != OTA_STATE_ERROR);
}

/**
 * @brief Realiza rollback a la partición OTA anterior
 * @return ESP_OK si el rollback fue exitoso, error en caso contrario
 */
esp_err_t ota_rollback_to_previous(void) {
    ESP_LOGI(OTA_TAG, "🔄 Iniciando rollback a partición OTA anterior");

    // Obtener información sobre las particiones OTA
    const esp_partition_t *running_partition = esp_ota_get_running_partition();
    if (running_partition == NULL) {
        ESP_LOGE(OTA_TAG, "❌ No se pudo obtener la partición actual");
        return ESP_FAIL;
    }

    ESP_LOGI(OTA_TAG, "📍 Partición actual: %s (subtype: %d)", running_partition->label, running_partition->subtype);

    // Obtener la partición OTA anterior (la que no está corriendo actualmente)
    esp_ota_handle_t ota_handle;
    const esp_partition_t *previous_partition = NULL;

    // Buscar la partición OTA que no es la actual
    esp_partition_iterator_t it = esp_partition_find(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_0, NULL);
    if (it != NULL) {
        const esp_partition_t *ota_0 = esp_partition_get(it);
        esp_partition_iterator_release(it);

        it = esp_partition_find(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_1, NULL);
        if (it != NULL) {
            const esp_partition_t *ota_1 = esp_partition_get(it);
            esp_partition_iterator_release(it);

            // Determinar cuál es la partición anterior
            if (running_partition->subtype == ESP_PARTITION_SUBTYPE_APP_OTA_0) {
                previous_partition = ota_1; // Si está corriendo ota_0, la anterior es ota_1
            } else if (running_partition->subtype == ESP_PARTITION_SUBTYPE_APP_OTA_1) {
                previous_partition = ota_0; // Si está corriendo ota_1, la anterior es ota_0
            }
        }
    }

    if (previous_partition == NULL) {
        ESP_LOGE(OTA_TAG, "❌ No se pudo encontrar la partición OTA anterior");
        return ESP_FAIL;
    }

    ESP_LOGI(OTA_TAG, "📍 Partición anterior encontrada: %s", previous_partition->label);

    // Verificar que la partición anterior sea válida y tenga datos
    if (previous_partition->size == 0) {
        ESP_LOGE(OTA_TAG, "❌ La partición anterior está vacía");
        return ESP_FAIL;
    }

    // Configurar el rollback
    esp_err_t err = esp_ota_set_boot_partition(previous_partition);
    if (err != ESP_OK) {
        ESP_LOGE(OTA_TAG, "❌ Error al configurar partición de boot: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(OTA_TAG, "✅ Rollback configurado exitosamente");
    ESP_LOGI(OTA_TAG, "🔄 Reiniciando sistema para aplicar rollback...");

    // Pequeño delay para que llegue el mensaje
    vTaskDelay(2000 / portTICK_PERIOD_MS);

    // Reiniciar para aplicar el rollback
    esp_restart();

    return ESP_OK; // Nunca se alcanza, pero por completitud
}
