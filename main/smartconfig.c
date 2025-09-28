// === smartconfig.c ===
#include "../include/HALO.h"
#include "esp_smartconfig.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include <string.h>
#include "freertos/timers.h"

extern const char *TAG;

// Variables globales para SmartConfig
static bool smartconfig_active = false;
static EventGroupHandle_t smartconfig_event_group;
static const int SMARTCONFIG_CONNECTED_BIT = BIT0;
static const int SMARTCONFIG_DONE_BIT = BIT1;
static const int SMARTCONFIG_FAIL_BIT = BIT2;
static const int SMARTCONFIG_TIMEOUT_BIT = BIT3;
static TimerHandle_t smartconfig_timeout_timer = NULL;

// Callback del timer de timeout para SmartConfig
static void smartconfig_timeout_callback(TimerHandle_t xTimer) {
    ESP_LOGW(TAG, "⏰ SmartConfig: Timeout de 10 segundos alcanzado - Cancelando configuración");
    xEventGroupSetBits(smartconfig_event_group, SMARTCONFIG_TIMEOUT_BIT);
}

// Manejador de eventos de SmartConfig
static void smartconfig_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_base == SC_EVENT) {
        switch (event_id) {
            case SC_EVENT_SCAN_DONE:
                ESP_LOGI(TAG, "🔍 SmartConfig: Escaneo de redes completado");
                break;
                
            case SC_EVENT_FOUND_CHANNEL:
                ESP_LOGI(TAG, "📡 SmartConfig: Canal encontrado");
                break;
                
            case SC_EVENT_GOT_SSID_PSWD:
                ESP_LOGI(TAG, "📱 SmartConfig: Credenciales recibidas desde el teléfono");
                
                // Detener timer de timeout ya que recibimos credenciales
                if (smartconfig_timeout_timer != NULL) {
                    xTimerStop(smartconfig_timeout_timer, 0);
                    ESP_LOGI(TAG, "⏰ SmartConfig: Timer de timeout cancelado - Credenciales recibidas a tiempo");
                }
                
                {
                    // Las credenciales vienen directamente en event_data
                    smartconfig_event_got_ssid_pswd_t *evt = (smartconfig_event_got_ssid_pswd_t *)event_data;
                    
                    // Extraer SSID y password de la estructura del evento
                    char ssid_str[33] = {0};  // 32 chars + null terminator
                    char pass_str[65] = {0};  // 64 chars + null terminator
                    
                    // Copiar con seguridad los datos del evento
                    memcpy(ssid_str, evt->ssid, sizeof(evt->ssid));
                    memcpy(pass_str, evt->password, sizeof(evt->password));
                    
                    // Asegurar terminación null
                    ssid_str[32] = '\0';
                    pass_str[64] = '\0';
                    
                    ESP_LOGI(TAG, "🔍 SmartConfig: SSID recibido='%s'", ssid_str);
                    ESP_LOGI(TAG, "🔍 SmartConfig: Password length=%d", strlen(pass_str));
                    
                    // Guardar credenciales en NVS
                    if (smartconfig_save_credentials(ssid_str, pass_str) == ESP_OK) {
                        ESP_LOGI(TAG, "✅ SmartConfig: Credenciales guardadas en NVS");
                        
                        // Aplicar inmediatamente las nuevas credenciales a WiFi
                        wifi_config_t wifi_config = {0};
                        strncpy((char*)wifi_config.sta.ssid, ssid_str, sizeof(wifi_config.sta.ssid) - 1);
                        strncpy((char*)wifi_config.sta.password, pass_str, sizeof(wifi_config.sta.password) - 1);
                        wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
                        wifi_config.sta.pmf_cfg.capable = true;
                        wifi_config.sta.pmf_cfg.required = false;
                        
                        esp_err_t ret = esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
                        if (ret == ESP_OK) {
                            ESP_LOGI(TAG, "✅ SmartConfig: Configuración WiFi actualizada");
                            ESP_LOGI(TAG, "🔄 SmartConfig: Reiniciando sistema en 3 segundos...");
                            vTaskDelay(pdMS_TO_TICKS(3000));
                            esp_restart();
                        } else {
                            ESP_LOGE(TAG, "❌ SmartConfig: Error al actualizar configuración WiFi: %s", esp_err_to_name(ret));
                        }
                        
                        xEventGroupSetBits(smartconfig_event_group, SMARTCONFIG_DONE_BIT);
                    } else {
                        ESP_LOGE(TAG, "❌ SmartConfig: Error al guardar credenciales");
                        xEventGroupSetBits(smartconfig_event_group, SMARTCONFIG_FAIL_BIT);
                    }
                }
                break;
                
            case SC_EVENT_SEND_ACK_DONE:
                ESP_LOGI(TAG, "✅ SmartConfig: ACK enviado al teléfono");
                break;
                
            default:
                ESP_LOGI(TAG, "📡 SmartConfig: Evento: %d", (int)event_id);
                break;
        }
    }
}

// Manejador de eventos WiFi para SmartConfig
static void smartconfig_wifi_handler(void* arg, esp_event_base_t event_base,
                              int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_CONNECTED) {
        ESP_LOGI(TAG, "✅ SmartConfig: WiFi conectado");
        xEventGroupSetBits(smartconfig_event_group, SMARTCONFIG_CONNECTED_BIT);
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGI(TAG, "❌ SmartConfig: WiFi desconectado");
        xEventGroupClearBits(smartconfig_event_group, SMARTCONFIG_CONNECTED_BIT);
    }
}

esp_err_t smartconfig_init(void) {
    ESP_LOGI(TAG, "🔧 SmartConfig: Inicializando...");
    
    // Crear grupo de eventos
    smartconfig_event_group = xEventGroupCreate();
    if (smartconfig_event_group == NULL) {
        ESP_LOGE(TAG, "❌ SmartConfig: Error al crear grupo de eventos");
        return ESP_ERR_NO_MEM;
    }
    
    // Registrar manejador de eventos WiFi
    esp_err_t ret = esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, 
                                              &smartconfig_wifi_handler, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ SmartConfig: Error al registrar manejador WiFi: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "✅ SmartConfig: Inicializado correctamente");
    return ESP_OK;
}

esp_err_t smartconfig_start(void) {
    if (smartconfig_active) {
        ESP_LOGW(TAG, "⚠️ SmartConfig: Ya está activo");
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "📱 SmartConfig: Abre la app ESPTouch en tu teléfono");
    
    // Configurar SmartConfig
    smartconfig_start_config_t cfg = SMARTCONFIG_START_CONFIG_DEFAULT();
    
    esp_err_t ret = esp_smartconfig_start(&cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ SmartConfig: Error al iniciar: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Registrar manejador de eventos de SmartConfig
    ret = esp_event_handler_register(SC_EVENT, ESP_EVENT_ANY_ID, 
                                    &smartconfig_event_handler, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ SmartConfig: Error al registrar manejador: %s", esp_err_to_name(ret));
        esp_smartconfig_stop();
        return ret;
    }
    
    // Crear y iniciar timer de timeout de 10 segundos
    smartconfig_timeout_timer = xTimerCreate(
        "SmartConfig_Timeout",              // Nombre del timer
        pdMS_TO_TICKS(10000),              // Período: 10 segundos
        pdFALSE,                           // No repetir (one-shot)
        NULL,                              // ID del timer (no usado)
        smartconfig_timeout_callback       // Función callback
    );
    
    if (smartconfig_timeout_timer == NULL) {
        ESP_LOGE(TAG, "❌ SmartConfig: Error al crear timer de timeout");
        esp_smartconfig_stop();
        esp_event_handler_unregister(SC_EVENT, ESP_EVENT_ANY_ID, &smartconfig_event_handler);
        return ESP_ERR_NO_MEM;
    }
    
    if (xTimerStart(smartconfig_timeout_timer, 0) != pdPASS) {
        ESP_LOGE(TAG, "❌ SmartConfig: Error al iniciar timer de timeout");
        xTimerDelete(smartconfig_timeout_timer, 0);
        smartconfig_timeout_timer = NULL;
        esp_smartconfig_stop();
        esp_event_handler_unregister(SC_EVENT, ESP_EVENT_ANY_ID, &smartconfig_event_handler);
        return ESP_ERR_TIMEOUT;
    }
    
    smartconfig_active = true;
    ESP_LOGI(TAG, "✅ SmartConfig: Iniciado correctamente");
    return ESP_OK;
}

esp_err_t smartconfig_stop(void) {
    if (!smartconfig_active) {
        ESP_LOGW(TAG, "⚠️ SmartConfig: No está activo");
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "🛑 SmartConfig: Deteniendo...");
    
    // Detener y eliminar timer de timeout si existe
    if (smartconfig_timeout_timer != NULL) {
        xTimerStop(smartconfig_timeout_timer, 0);
        xTimerDelete(smartconfig_timeout_timer, 0);
        smartconfig_timeout_timer = NULL;
        ESP_LOGI(TAG, "⏰ SmartConfig: Timer de timeout eliminado");
    }
    
    // Desregistrar manejadores de eventos
    esp_event_handler_unregister(SC_EVENT, ESP_EVENT_ANY_ID, &smartconfig_event_handler);
    
    // Detener SmartConfig
    esp_err_t ret = esp_smartconfig_stop();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ SmartConfig: Error al detener: %s", esp_err_to_name(ret));
        return ret;
    }
    
    smartconfig_active = false;
    ESP_LOGI(TAG, "✅ SmartConfig: Detenido correctamente");
    return ESP_OK;
}

bool smartconfig_is_active(void) {
    return smartconfig_active;
}

esp_err_t smartconfig_save_credentials(const char *ssid, const char *password) {
    if (!ssid || !password) {
        ESP_LOGE(TAG, "❌ SmartConfig: Parámetros inválidos");
        return ESP_ERR_INVALID_ARG;
    }
    
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "❌ SmartConfig: Error al abrir NVS: %s", esp_err_to_name(err));
        return err;
    }

    // Intentar actualizar si el SSID ya existe en algún slot
    const char *ssid_keys[NVS_MAX_WIFI_CREDENTIALS] = { NVS_KEY_WIFI_SSID_0, NVS_KEY_WIFI_SSID_1, NVS_KEY_WIFI_SSID_2 };
    const char *pass_keys[NVS_MAX_WIFI_CREDENTIALS] = { NVS_KEY_WIFI_PASS_0, NVS_KEY_WIFI_PASS_1, NVS_KEY_WIFI_PASS_2 };

    for (int i = 0; i < NVS_MAX_WIFI_CREDENTIALS; i++) {
        size_t tmp_len = 0;
        esp_err_t get_err = nvs_get_str(nvs_handle, ssid_keys[i], NULL, &tmp_len);
        if (get_err == ESP_OK && tmp_len > 0) {
            char existing_ssid[33] = {0};
            if (nvs_get_str(nvs_handle, ssid_keys[i], existing_ssid, &tmp_len) == ESP_OK) {
                if (strcmp(existing_ssid, ssid) == 0) {
                    // Actualizar password para este SSID
                    err = nvs_set_str(nvs_handle, pass_keys[i], password);
                    if (err == ESP_OK) {
                        err = nvs_commit(nvs_handle);
                    }
                    nvs_close(nvs_handle);
                    if (err == ESP_OK) {
                        ESP_LOGI(TAG, "✅ SmartConfig: Contraseña actualizada para SSID existente en slot %d", i);
                    } else {
                        ESP_LOGE(TAG, "❌ SmartConfig: Error al actualizar contraseña: %s", esp_err_to_name(err));
                    }
                    return err;
                }
            }
        }
    }

    // Buscar primer slot vacío
    int empty_slot = -1;
    for (int i = 0; i < NVS_MAX_WIFI_CREDENTIALS; i++) {
        size_t len = 0;
        esp_err_t get_err = nvs_get_str(nvs_handle, ssid_keys[i], NULL, &len);
        if (get_err == ESP_ERR_NVS_NOT_FOUND || len == 0) {
            empty_slot = i;
            break;
        }
    }

    if (empty_slot == -1) {
        // Si no hay espacio, sobreescribir el slot 0
        empty_slot = 0;
        ESP_LOGW(TAG, "⚠️ SmartConfig: Slots llenos, sobrescribiendo slot 0");
    }

    // Guardar en el slot seleccionado
    err = nvs_set_str(nvs_handle, ssid_keys[empty_slot], ssid);
    if (err == ESP_OK) {
        err = nvs_set_str(nvs_handle, pass_keys[empty_slot], password);
    }
    if (err == ESP_OK) {
        err = nvs_commit(nvs_handle);
    }
    nvs_close(nvs_handle);

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "✅ SmartConfig: Credenciales guardadas en slot %d: SSID='%s'", empty_slot, ssid);
    } else {
        ESP_LOGE(TAG, "❌ SmartConfig: Error al guardar credenciales: %s", esp_err_to_name(err));
    }
    return err;
}

esp_err_t smartconfig_load_credentials(char *ssid, char *password, size_t max_len) {
    if (!ssid || !password || max_len == 0) {
        ESP_LOGE(TAG, "❌ SmartConfig: Parámetros inválidos");
        return ESP_ERR_INVALID_ARG;
    }
    
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "❌ SmartConfig: Error al abrir NVS: %s", esp_err_to_name(err));
        return err;
    }

    const char *ssid_keys[NVS_MAX_WIFI_CREDENTIALS] = { NVS_KEY_WIFI_SSID_0, NVS_KEY_WIFI_SSID_1, NVS_KEY_WIFI_SSID_2 };
    const char *pass_keys[NVS_MAX_WIFI_CREDENTIALS] = { NVS_KEY_WIFI_PASS_0, NVS_KEY_WIFI_PASS_1, NVS_KEY_WIFI_PASS_2 };

    // Preferir slots múltiples en orden 0..N-1
    for (int i = 0; i < NVS_MAX_WIFI_CREDENTIALS; i++) {
        size_t ssid_len = max_len;
        size_t pass_len = max_len;
        esp_err_t get_err = nvs_get_str(nvs_handle, ssid_keys[i], ssid, &ssid_len);
        if (get_err == ESP_OK && ssid_len > 0) {
            get_err = nvs_get_str(nvs_handle, pass_keys[i], password, &pass_len);
            if (get_err == ESP_OK) {
                nvs_close(nvs_handle);
                ESP_LOGI(TAG, "✅ SmartConfig: Credenciales cargadas (slot %d): SSID='%s'", i, ssid);
                return ESP_OK;
            }
        }
    }

    // Fallback a claves legado
    size_t ssid_len = max_len;
    size_t pass_len = max_len;
    err = nvs_get_str(nvs_handle, NVS_KEY_WIFI_SSID, ssid, &ssid_len);
    if (err == ESP_OK) {
        err = nvs_get_str(nvs_handle, NVS_KEY_WIFI_PASS, password, &pass_len);
        if (err == ESP_OK) {
            nvs_close(nvs_handle);
            ESP_LOGI(TAG, "✅ SmartConfig: Credenciales cargadas (legado): SSID='%s'", ssid);
            return ESP_OK;
        }
    }

    nvs_close(nvs_handle);
    ESP_LOGE(TAG, "❌ SmartConfig: No hay credenciales disponibles para cargar");
    return ESP_ERR_NOT_FOUND;
}

bool smartconfig_has_saved_credentials(void) {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        return false;
    }

    const char *ssid_keys[NVS_MAX_WIFI_CREDENTIALS] = { NVS_KEY_WIFI_SSID_0, NVS_KEY_WIFI_SSID_1, NVS_KEY_WIFI_SSID_2 };
    for (int i = 0; i < NVS_MAX_WIFI_CREDENTIALS; i++) {
        size_t len = 0;
        if (nvs_get_str(nvs_handle, ssid_keys[i], NULL, &len) == ESP_OK && len > 0) {
            nvs_close(nvs_handle);
            return true;
        }
    }

    size_t legacy_len = 0;
    err = nvs_get_str(nvs_handle, NVS_KEY_WIFI_SSID, NULL, &legacy_len);
    nvs_close(nvs_handle);
    return (err == ESP_OK && legacy_len > 0);
}

esp_err_t smartconfig_clear_credentials(void) {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "❌ SmartConfig: Error al abrir NVS: %s", esp_err_to_name(err));
        return err;
    }

    const char *ssid_keys[NVS_MAX_WIFI_CREDENTIALS] = { NVS_KEY_WIFI_SSID_0, NVS_KEY_WIFI_SSID_1, NVS_KEY_WIFI_SSID_2 };
    const char *pass_keys[NVS_MAX_WIFI_CREDENTIALS] = { NVS_KEY_WIFI_PASS_0, NVS_KEY_WIFI_PASS_1, NVS_KEY_WIFI_PASS_2 };

    for (int i = 0; i < NVS_MAX_WIFI_CREDENTIALS; i++) {
        esp_err_t e1 = nvs_erase_key(nvs_handle, ssid_keys[i]);
        esp_err_t e2 = nvs_erase_key(nvs_handle, pass_keys[i]);
        if ((e1 != ESP_OK && e1 != ESP_ERR_NVS_NOT_FOUND) || (e2 != ESP_OK && e2 != ESP_ERR_NVS_NOT_FOUND)) {
            ESP_LOGE(TAG, "❌ SmartConfig: Error al eliminar slot %d", i);
            nvs_close(nvs_handle);
            return ESP_FAIL;
        }
    }

    // Borrar claves legado también
    (void)nvs_erase_key(nvs_handle, NVS_KEY_WIFI_SSID);
    (void)nvs_erase_key(nvs_handle, NVS_KEY_WIFI_PASS);

    err = nvs_commit(nvs_handle);
    nvs_close(nvs_handle);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "✅ SmartConfig: Todas las credenciales eliminadas");
    } else {
        ESP_LOGE(TAG, "❌ SmartConfig: Error al confirmar eliminación: %s", esp_err_to_name(err));
    }
    return err;
}

size_t smartconfig_get_saved_credentials_count(void) {
    nvs_handle_t nvs_handle;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle) != ESP_OK) {
        return 0;
    }
    size_t count = 0;
    const char *ssid_keys[NVS_MAX_WIFI_CREDENTIALS] = { NVS_KEY_WIFI_SSID_0, NVS_KEY_WIFI_SSID_1, NVS_KEY_WIFI_SSID_2 };
    for (int i = 0; i < NVS_MAX_WIFI_CREDENTIALS; i++) {
        size_t len = 0;
        if (nvs_get_str(nvs_handle, ssid_keys[i], NULL, &len) == ESP_OK && len > 0) {
            count++;
        }
    }
    if (count == 0) {
        size_t legacy_len = 0;
        if (nvs_get_str(nvs_handle, NVS_KEY_WIFI_SSID, NULL, &legacy_len) == ESP_OK && legacy_len > 0) {
            count = 1;
        }
    }
    nvs_close(nvs_handle);
    return count;
}

esp_err_t smartconfig_load_credentials_index(int index, char *ssid, char *password, size_t max_len) {
    if (!ssid || !password || max_len == 0 || index < 0) {
        return ESP_ERR_INVALID_ARG;
    }
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        return err;
    }
    const char *ssid_keys[NVS_MAX_WIFI_CREDENTIALS] = { NVS_KEY_WIFI_SSID_0, NVS_KEY_WIFI_SSID_1, NVS_KEY_WIFI_SSID_2 };
    const char *pass_keys[NVS_MAX_WIFI_CREDENTIALS] = { NVS_KEY_WIFI_PASS_0, NVS_KEY_WIFI_PASS_1, NVS_KEY_WIFI_PASS_2 };

    if (index < NVS_MAX_WIFI_CREDENTIALS) {
        size_t ssid_len = max_len;
        size_t pass_len = max_len;
        if (nvs_get_str(nvs_handle, ssid_keys[index], ssid, &ssid_len) == ESP_OK && ssid_len > 0) {
            if (nvs_get_str(nvs_handle, pass_keys[index], password, &pass_len) == ESP_OK) {
                nvs_close(nvs_handle);
                return ESP_OK;
            }
        }
        nvs_close(nvs_handle);
        return ESP_ERR_NOT_FOUND;
    }

    // Mapear index 0 a legacy si no hay múltiples
    if (index == 0) {
        size_t ssid_len = max_len;
        size_t pass_len = max_len;
        if (nvs_get_str(nvs_handle, NVS_KEY_WIFI_SSID, ssid, &ssid_len) == ESP_OK && ssid_len > 0) {
            if (nvs_get_str(nvs_handle, NVS_KEY_WIFI_PASS, password, &pass_len) == ESP_OK) {
                nvs_close(nvs_handle);
                return ESP_OK;
            }
        }
    }
    nvs_close(nvs_handle);
    return ESP_ERR_NOT_FOUND;
}

// Función simplificada - SmartConfig solo se activa con pulsación larga del botón
