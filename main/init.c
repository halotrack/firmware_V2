#include "../include/init.h"
#include "../include/battery.h"

extern const char *TAG;



void inicializar_sistema() {
    // Configuración GPIO consolidada
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << 26) | (1ULL << LED_USER),
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&io_conf);
    gpio_set_level(26, 1);
    //gpio_set_level(LED_USER, 0);

    user_button_init();
    rtc_configurar_zona_horaria();
    esp_err_t ret = nvs_flash_init();

    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    restaurar_ultima_muestra_enviada();
    restaurar_horario_envio();
    restaurar_muestreo_ms();
    
    // Inicializar hardware y red
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    init_RTC();
    init_battery();
    init_HX711();
    sdcard_init();
    wifi_init_sta();

    // Inicializar configuración centralizada SIEMPRE
    if (sistema_init_config() != ESP_OK) {
        ESP_LOGE(TAG, "❌ Error crítico: Fallo en inicialización de configuración");
        return;
    }

    // Inicializar todas las banderas de estado para modo offline
    sistema.estado.conexion_boton_activa = false;
    sistema.estado.esperando_comando = false;
    sistema.estado.esperando_fecha_hora = false;
    sistema.estado.esperando_config_horario = false;
    sistema.estado.esperando_comando_peso = false;
    sistema.estado.sistema_calibrado = false;
    sistema.estado.calibracion_completada = false;
    
    if (!smartconfig_has_saved_credentials()) {
        ESP_LOGI(TAG, "Sistema funcionará OFFLINE - sin credenciales WiFi");
        // Sistema inicializado correctamente en modo offline
        sistema.estado.sistema_inicializado = true;
        ESP_LOGI(TAG, "✅ Sistema inicializado en modo OFFLINE");
        return;
    }
    
    // Intentar conexión WiFi
    int wifi_timeout;
    WAIT_UNTIL(wifi_is_connected(), wifi_timeout, WIFI_TIMEOUT_SECONDS);
    
    if (!wifi_is_connected()) {
        ESP_LOGW(TAG, "WiFi no conectado - modo offline");
        // Sistema inicializado correctamente en modo offline
        sistema.estado.sistema_inicializado = true;
        ESP_LOGI(TAG, "✅ Sistema inicializado en modo OFFLINE (WiFi falló)");
        return;
    }
    
    // Conexión WiFi exitosa
    gpio_set_level(LED_USER, 0);
    rtc_set_system_time_from_rtc();
    mqtt_init();

    // Inicializar sistema OTA
    if (ota_init() != ESP_OK) {
        ESP_LOGW(TAG, "⚠️ No se pudo inicializar sistema OTA");
    }
    
    int mqtt_timeout;
    WAIT_UNTIL(mqtt_is_connected(), mqtt_timeout, MQTT_TIMEOUT_SECONDS);
    
    if (!mqtt_is_connected()) {
        ESP_LOGW(TAG, "MQTT no conectado - solo WiFi activo");
        esp_wifi_stop();
        // Sistema inicializado correctamente con WiFi pero sin MQTT
        sistema.estado.sistema_inicializado = true;
        ESP_LOGI(TAG, "✅ Sistema inicializado con WiFi (MQTT falló)");
        return;
    }

    // Conexión completa WiFi + MQTT
    enviar_mac();
    
    if (hay_datos_pendientes_envio()) {
        ESP_LOGI(TAG, "📤 Hay datos pendientes");
        esp_mqtt_client_publish(mqtt_client, "esp32/halo/conection", "ON", 0, 1, 0);
        sistema.estado.conexion_boton_activa = true;
    } else {
        ESP_LOGI(TAG, "No hay datos pendientes - desconectando");
        esp_mqtt_client_publish(mqtt_client, "esp32/halo/conection", "OFF", 0, 1, 0);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        esp_mqtt_client_stop(mqtt_client);
        esp_wifi_stop();
    }

    // Sistema inicializado correctamente con conectividad completa
    sistema.estado.sistema_inicializado = true;
    ESP_LOGI(TAG, "✅ Sistema inicializado con conectividad completa");

}

void enviar_mac(void) {
    uint8_t mac[6];
    esp_err_t ret = esp_wifi_get_mac(ESP_IF_WIFI_STA, mac);

    if (ret == ESP_OK) {
        char mac_json[128];
        snprintf(mac_json, sizeof(mac_json), "{\"device_id\":\"%02X:%02X:%02X:%02X:%02X:%02X\"}",
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        esp_mqtt_client_publish(mqtt_client, "esp32/halo/device_info", mac_json, 0, 1, 0);
    }
}


bool hay_datos_pendientes_envio(void) {
    struct tm timeinfo;
    if (!rtc_get_time(&timeinfo)) return false;
    
    // Verificar si es hora y día de envío
    bool es_hora = (timeinfo.tm_hour > sistema.envio.hora_envio ||
                   (timeinfo.tm_hour == sistema.envio.hora_envio && timeinfo.tm_min >= sistema.envio.minuto_envio));
    bool no_enviado_hoy = (timeinfo.tm_mday != sistema.envio.ultimo_dia_envio);
    
    if (!es_hora || !no_enviado_hoy) return false;
    
    // Verificar datos en SD
    FILE *f = fopen("/sdcard/pesos.csv", "r");
    if (!f) return false;
    
    char line[128];
    int count = 0;
    while (fgets(line, sizeof(line), f)) {
        if (!strstr(line, "Fecha")) count++;
        if (count > sistema.envio.ultima_muestra_enviada) {
            fclose(f);
            return true;
        }
    }
    fclose(f);
    return false;
}



void guardar_ultima_muestra_enviada() {
    nvs_handle_t nvs_handle;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle) == ESP_OK) {
        nvs_set_u32(nvs_handle, NVS_KEY_ULTIMA, sistema.envio.ultima_muestra_enviada);
        nvs_commit(nvs_handle);
        nvs_close(nvs_handle);
        ESP_LOGI(TAG, "Guardado ultima muestra: %u", (unsigned int)sistema.envio.ultima_muestra_enviada);
    }
}

int leer_ultimas_muestras_sd(int n, float *pesos, struct tm *tiempos) {
    FILE *f = fopen("/sdcard/pesos.csv", "r");
    if (!f) return 0;
    
    // Contar total de líneas
    char line[128];
    int total = 0;
    while (fgets(line, sizeof(line), f)) {
        if (!strstr(line, "Fecha")) total++;
    }
    
    // Leer últimas n muestras
    rewind(f);
    int skip = total > n ? total - n : 0;
    int idx = 0;
    while (fgets(line, sizeof(line), f) && idx < n) {
        if (strstr(line, "Fecha")) continue;
        if (skip-- > 0) continue;
        
        int y, m, d, h, min, s;
        float p;
        if (sscanf(line, "%d-%d-%d,%d:%d:%d,%f", &y, &m, &d, &h, &min, &s, &p) == 7) {
            pesos[idx] = p;
            tiempos[idx] = (struct tm){.tm_year=y-1900, .tm_mon=m-1, .tm_mday=d, .tm_hour=h, .tm_min=min, .tm_sec=s};
            idx++;
        }
    }
    fclose(f);
    return idx;
}

void restaurar_ultima_muestra_enviada() {
    nvs_handle_t nvs_handle;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle) == ESP_OK) {
        uint32_t valor;
        if (nvs_get_u32(nvs_handle, NVS_KEY_ULTIMA, &valor) == ESP_OK) {
            sistema.envio.ultima_muestra_enviada = valor;
            ESP_LOGI(TAG, "Recuperado ultima muestra: %u", (unsigned int)valor);
        }
        nvs_close(nvs_handle);
    }
}

void guardar_horario_envio() {
    nvs_handle_t nvs_handle;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle) == ESP_OK) {
        nvs_set_u8(nvs_handle, NVS_KEY_HORA_ENVIO, sistema.envio.hora_envio);
        nvs_set_u8(nvs_handle, NVS_KEY_MINUTO_ENVIO, sistema.envio.minuto_envio);
        nvs_commit(nvs_handle);
        nvs_close(nvs_handle);
        ESP_LOGI(TAG, "Horario guardado: %02d:%02d", sistema.envio.hora_envio, sistema.envio.minuto_envio);
    }
}

void restaurar_horario_envio() {
    nvs_handle_t nvs_handle;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle) == ESP_OK) {
        uint8_t hora, minuto;
        if (nvs_get_u8(nvs_handle, NVS_KEY_HORA_ENVIO, &hora) == ESP_OK &&
            nvs_get_u8(nvs_handle, NVS_KEY_MINUTO_ENVIO, &minuto) == ESP_OK) {
            sistema.envio.hora_envio = hora;
            sistema.envio.minuto_envio = minuto;
            ESP_LOGI(TAG, "Horario restaurado: %02d:%02d", hora, minuto);
        }
        nvs_close(nvs_handle);
    }
}

void guardar_muestreo_ms() {
    nvs_handle_t nvs_handle;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle) == ESP_OK) {
        nvs_set_u32(nvs_handle, NVS_KEY_MUESTREO_MS, sistema.envio.muestreo_ms);
        nvs_commit(nvs_handle);
        nvs_close(nvs_handle);
        ESP_LOGI(TAG, "Muestreo guardado: %u ms", (unsigned int)sistema.envio.muestreo_ms);
    }
}

void restaurar_muestreo_ms() {
    nvs_handle_t nvs_handle;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle) == ESP_OK) {
        uint32_t valor;
        if (nvs_get_u32(nvs_handle, NVS_KEY_MUESTREO_MS, &valor) == ESP_OK) {
            sistema.envio.muestreo_ms = valor;
            ESP_LOGI(TAG, "Muestreo restaurado: %u ms", (unsigned int)valor);
        } else {
            sistema.envio.muestreo_ms = 10000;
        }
        nvs_close(nvs_handle);
    } else {
        sistema.envio.muestreo_ms = 10000;
    }
}


esp_err_t sistema_init_config(void) {
    // Crear mutexes para thread-safety
    sistema.mutex_config = xSemaphoreCreateMutex();
    sistema.mutex_sd = xSemaphoreCreateMutex();
    
    if (!sistema.mutex_config || !sistema.mutex_sd) {
        ESP_LOGE(TAG, "Error creando mutexes");
        if (sistema.mutex_config) vSemaphoreDelete(sistema.mutex_config);
        if (sistema.mutex_sd) vSemaphoreDelete(sistema.mutex_sd);
        return ESP_ERR_NO_MEM;
    }
    
    return ESP_OK;
}