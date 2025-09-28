#include "../include/wifi_lib.h"
static const char *WIFI_TAG = "WIFI_LIB";

// Variables globales del WiFi
EventGroupHandle_t s_wifi_event_group;
int s_retry_num = 0;
static bool wifi_auto_reconnect = true;
static uint32_t last_connection_attempt = 0;
static const uint32_t RECONNECT_DELAY_MS = 5000;
static TaskHandle_t led_task_handle = NULL;

static void led_blink_task(void *pvParameters) {
    while (1) {
        gpio_set_level(LED_USER, 1);
        vTaskDelay(pdMS_TO_TICKS(250));
        gpio_set_level(LED_USER, 0);
        vTaskDelay(pdMS_TO_TICKS(250));
    }
}

void wifi_event_handler(void* arg, esp_event_base_t event_base,
                          int32_t event_id, void* event_data) {
    (void)arg;
    
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI(WIFI_TAG, "WiFi iniciado - conectando...");
        xTaskCreate(led_blink_task, "led_blink", 1024, NULL, 1, &led_task_handle);
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        
        // Solo reconectar automáticamente si está habilitado y no hemos excedido intentos
        if (wifi_auto_reconnect && s_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY) {
            vTaskDelay(pdMS_TO_TICKS(2000)); // Esperar antes de reconectar
            esp_wifi_connect();
            s_retry_num++;
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
            if (s_retry_num >= EXAMPLE_ESP_MAXIMUM_RETRY) {
                ESP_LOGE(WIFI_TAG, "Máximo de intentos alcanzado - deteniendo reconexión automática");
            }
        }
    } 
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;

        s_retry_num = 0;
        last_connection_attempt = 0;
        if (led_task_handle) {
            vTaskDelete(led_task_handle);
            led_task_handle = NULL;
        }
        gpio_set_level(LED_USER, 0);
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

void wifi_init_sta(void) {
    s_wifi_event_group = xEventGroupCreate();

    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler, NULL, &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler, NULL, &instance_got_ip));

    // Preparar configuración WiFi
    wifi_config_t wifi_config = {
        .sta = {
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            .pmf_cfg = {
                .capable = true,
                .required = false
            },
        },
    };
    
    // Intentar con hasta 3 credenciales guardadas (incluye soporte legado)
    size_t creds_count = smartconfig_get_saved_credentials_count();
    if (creds_count > 0) {
        bool connected = false;
        char ssid[33] = {0};
        char password[65] = {0};

        // Cargar primera credencial antes de iniciar WiFi
        if (smartconfig_load_credentials_index(0, ssid, password, sizeof(ssid)) == ESP_OK) {
            strncpy((char*)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid) - 1);
            strncpy((char*)wifi_config.sta.password, password, sizeof(wifi_config.sta.password) - 1);
        }

        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
        ESP_ERROR_CHECK(esp_wifi_start());

        for (int i = 0; i < (int)creds_count && !connected; i++) {
            if (i > 0) {
                memset(&wifi_config, 0, sizeof(wifi_config));
                wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
                wifi_config.sta.pmf_cfg.capable = true;
                wifi_config.sta.pmf_cfg.required = false;
                if (smartconfig_load_credentials_index(i, ssid, password, sizeof(ssid)) != ESP_OK) {
                    continue;
                }
                strncpy((char*)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid) - 1);
                strncpy((char*)wifi_config.sta.password, password, sizeof(wifi_config.sta.password) - 1);
                esp_wifi_disconnect();
                ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
            }

            ESP_LOGI(WIFI_TAG, "📡 Probando credencial %d/%d: SSID='%s'", i + 1, (int)creds_count, ssid);
            xEventGroupClearBits(s_wifi_event_group, WIFI_FAIL_BIT);
            s_retry_num = 0;
            esp_wifi_connect();

            EventBits_t bits = xEventGroupWaitBits(
                s_wifi_event_group,
                WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                pdFALSE,
                pdFALSE,
                pdMS_TO_TICKS(15000)
            );

            if (bits & WIFI_CONNECTED_BIT) {
                ESP_LOGI(WIFI_TAG, "✅ Conectado al AP SSID:%s", ssid);
                connected = true;
            } else {
                ESP_LOGW(WIFI_TAG, "❌ Falló con SSID:%s, intentando siguiente si existe", ssid);
                xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT);
            }
        }

        if (!connected) {
            ESP_LOGW(WIFI_TAG, "⚠️ No se pudo conectar con ninguna credencial guardada");
            if (led_task_handle) {
                vTaskDelete(led_task_handle);
                led_task_handle = NULL;
            }
        }
    } else {
        ESP_LOGI(WIFI_TAG, "⚠️ No hay credenciales WiFi - iniciando sin conexión");
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
        ESP_ERROR_CHECK(esp_wifi_start());
        ESP_LOGI(WIFI_TAG, "WiFi inicializado en modo STA sin credenciales");
    }
}

bool wifi_is_connected(void) {
    if (s_wifi_event_group == NULL) {
        return false;
    }
    
    EventBits_t bits = xEventGroupGetBits(s_wifi_event_group);
    return (bits & WIFI_CONNECTED_BIT) != 0;
}

void wifi_attempt_reconnect(void) {
    if (!wifi_auto_reconnect) {
        return;
    }
    
    uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
    if (current_time - last_connection_attempt < RECONNECT_DELAY_MS) {
        return; // Esperar antes del siguiente intento
    }
    
    if (!wifi_is_connected()) {
        ESP_LOGI(WIFI_TAG, "🔄 Intentando reconexión WiFi manual...");
        s_retry_num = 0;
        last_connection_attempt = current_time;
        xEventGroupClearBits(s_wifi_event_group, WIFI_FAIL_BIT);
        esp_wifi_connect();
    }
}

void wifi_set_auto_reconnect(bool enable) {
    wifi_auto_reconnect = enable;
}

esp_err_t wifi_get_rssi(int8_t *rssi) {
    if (!wifi_is_connected()) {
        return ESP_FAIL;
    }
    
    wifi_ap_record_t ap_info;
    if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
        *rssi = ap_info.rssi;
        return ESP_OK;
    }
    
    return ESP_FAIL;
} 