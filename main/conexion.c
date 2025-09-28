#include "../include/conexion.h"
#include "esp_log.h"
#include "mqtt_client.h"
#include "../include/wifi_lib.h" 
#include "../include/HALO.h"
#include "../include/button_actions.h"

extern const char *TAG;

#define WIFI_TIMEOUT_SECONDS 15
#define MQTT_TIMEOUT_SECONDS 15
extern esp_mqtt_client_handle_t mqtt_client;





bool esperar_estado(bool (*check_func)(void), int timeout_s, const char *msg) {
    // Espera activa con pasos de 500 ms, límite en segundos
    int elapsed_steps = 0;                           // cada step = 500 ms
    const int steps_limit = timeout_s * 2;           // 2 steps por segundo

    while (!check_func() && elapsed_steps < steps_limit) {
        vTaskDelay(pdMS_TO_TICKS(500));
        elapsed_steps++;

        if ((elapsed_steps % 6) == 0) {              // log cada 3 s (6 steps)
            int elapsed_seconds = elapsed_steps / 2;
            ESP_LOGI(TAG, "⏳ Esperando %s... ", msg);
        }
    }
    return check_func();
}

bool conectar_wifi_con_reintentos(void) {
    // Verificar si ya está conectado
    if (wifi_is_connected()) {
        return true;
    }

    if (!smartconfig_has_saved_credentials()) {
        ESP_LOGE(TAG, "❌ No hay credenciales WiFi guardadas");
        return false;
    }

    size_t creds = smartconfig_get_saved_credentials_count();
    if (creds == 0) {
        ESP_LOGE(TAG, "❌ No hay credenciales WiFi válidas");
        return false;
    }

    ESP_LOGI(TAG, "🔄 Iniciando WiFi");
    
    // Habilitar reconexión automática
    wifi_set_auto_reconnect(true);
    
    // Iniciar WiFi si no está ya iniciado
    esp_wifi_start();
    
    // Esperar conexión usando el sistema de eventos
    if (esperar_estado(wifi_is_connected, WIFI_TIMEOUT_SECONDS, "WiFi")) {
        //ESP_LOGI(TAG, "✅ WiFi conectado exitosamente");
        return true;
    }

    ESP_LOGE(TAG, "❌ No se pudo conectar WiFi");
    return false;
}

bool conectar_mqtt_con_reintentos(void) {
    // Asegurar inicialización del cliente MQTT en primer uso
    if (mqtt_client == NULL) {
        ESP_LOGI(TAG, "Inicializando cliente MQTT...");
        mqtt_init();
        vTaskDelay(pdMS_TO_TICKS(100)); // Reducido delay
    }
    
    // Intento único con timeout optimizado
    esp_mqtt_client_start(mqtt_client);

    if (esperar_estado(mqtt_is_connected, MQTT_TIMEOUT_SECONDS, "MQTT")) {
         //   ESP_LOGI(TAG, "✅ Broker MQTT conectado exitosamente");
        return true;
    }

    ESP_LOGW(TAG, "❌ Fallo en conexión MQTT");
    esp_mqtt_client_stop(mqtt_client);
    return false;
}

void desconectar_wifi_seguro(void) {
    // Deshabilitar reconexión automática antes de desconectar
    wifi_set_auto_reconnect(false);
    
    esp_wifi_stop();
    int timeout = 0;
    while (wifi_is_connected() && timeout++ < 10) {
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

void desconectar_mqtt_seguro(void) {
        publicar_estado("esp32/halo/conection", "OFF");
        publicar_estado("esp32/halo/status", "❌❌❌SISTEMA DESCONECTADO❌❌❌");
        gpio_set_level(LED_USER, 0);
    esp_mqtt_client_stop(mqtt_client);
    int timeout = 0;
    while (mqtt_is_connected() && timeout++ < 10) {
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}