#include "../include/button_actions.h"
#include "../include/battery.h"




extern const char *TAG;
extern sistema_config_t sistema;
extern esp_mqtt_client_handle_t mqtt_client;

// Variables para comunicación entre tareas
static TaskHandle_t connection_task_handle = NULL;
static bool connection_task_running = false;



void publicar_estado(const char *topic, const char *mensaje) {  
    // Verificar si hay conexión MQTT antes de intentar publicar (sin log redundante)
    if (mqtt_is_connected()) {
        esp_mqtt_client_publish(mqtt_client, topic, mensaje, 0, 1, 0);
    }
}

// Tarea para manejar pulsación larga - Modo SmartConfig
void long_press_task(void* arg) {
    ESP_LOGI(TAG, "📱 PULSACIÓN LARGA - Iniciando SmartConfig...");
    publicar_estado("esp32/halo/status", "📱 MODO SMARTCONFIG INICIADO - Usa la app ESPTouch");

    // Desconectar servicios activos
    if (mqtt_is_connected()) {
        publicar_estado("esp32/halo/status", "🔄 DESCONECTANDO PARA SMARTCONFIG...");
        esp_mqtt_client_stop(mqtt_client);
    }
    if (wifi_is_connected()) {
        esp_wifi_disconnect();
        esp_wifi_stop();
    }
    vTaskDelay(pdMS_TO_TICKS(1500));

    // Iniciar SmartConfig
    if (smartconfig_init() != ESP_OK || esp_wifi_set_mode(WIFI_MODE_STA) != ESP_OK || esp_wifi_start() != ESP_OK) {
        ESP_LOGE(TAG, "❌ Error iniciando SmartConfig");
        vTaskDelete(NULL);
    }

    if (smartconfig_start() != ESP_OK) {
        ESP_LOGE(TAG, "❌ Error al arrancar SmartConfig");
        smartconfig_stop();
        vTaskDelete(NULL);
    }

    ESP_LOGI(TAG, "✅ SmartConfig iniciado. Usa ESPTouch para enviar credenciales");
    TickType_t start = xTaskGetTickCount();
    const TickType_t timeout = pdMS_TO_TICKS(12000);

    // Monitoreo SmartConfig
    while (smartconfig_is_active() && (xTaskGetTickCount() - start) < timeout) {
        vTaskDelay(pdMS_TO_TICKS(500));
    }

    // Timeout o finalización
    if (smartconfig_is_active()) {
        ESP_LOGW(TAG, "⏰ SmartConfig: Timeout - Deteniendo");
        smartconfig_stop();
        esp_wifi_stop();
        vTaskDelay(pdMS_TO_TICKS(500));

        if (smartconfig_has_saved_credentials()) {
            ESP_LOGI(TAG, "🔄 Reintentando conexión con credenciales guardadas");
            esp_wifi_set_mode(WIFI_MODE_STA);
            esp_wifi_start();
        } else {
            ESP_LOGI(TAG, "ℹ️ No hay credenciales guardadas, permaneciendo offline");
        }
    } else {
        ESP_LOGI(TAG, "✅ SmartConfig completado correctamente");
    }

    vTaskDelete(NULL);
}

void manejar_pulsacion_larga(void) {
    
    ESP_LOGI(TAG, "🔄 Creando tarea para pulsación larga...");
    
    // Crear tarea para manejar pulsación larga (no bloqueante)
    BaseType_t result = xTaskCreatePinnedToCore(
        long_press_task,
        "Long_Press_Task",
        4096,
        NULL,
        5,            // Prioridad media
        NULL,
        0             // Núcleo 0
    );
    
    if (result != pdPASS) {
        ESP_LOGE(TAG, "❌ Error al crear tarea de pulsación larga");
        // Fallback: ejecutar directamente si falla la creación de tarea
        long_press_task(NULL);
    }
    
}

// Función eliminada - Solo usamos pulsación larga para SmartConfig

void manejar_click_doble(void) {
    ESP_LOGI(TAG, "📱 DOBLE CLICK - Iniciando modo SmartConfig...");
    // Usar la misma lógica que pulsación larga para consistencia
    manejar_pulsacion_larga();
}


void manejar_click_simple(void) {
    // Verificar el estado actual real de las conexiones
    bool wifi_connected = wifi_is_connected();
    bool mqtt_connected = mqtt_is_connected();
    
    // Sincronizar estado del botón con estado real de conexiones
    bool should_be_connected = wifi_connected || mqtt_connected;

    
    // Verificar si ya hay una tarea de conexión ejecutándose
    if (connection_task_running) {
        ESP_LOGW(TAG, "⚠️ Ya hay una operación de conexión en progreso - Ignorando clic");
        return;
    }
    
    // Decisión basada en estado real de conexiones, no solo en flag del botón
    bool currently_connected = wifi_connected || mqtt_connected;

    if (!currently_connected) {
        
        // Crear tarea para conectar (no bloqueante)
        connection_task_running = true;
        BaseType_t result = xTaskCreatePinnedToCore(
            connection_task,
            "Connection_Task",
            4096,
            (void*)true,  // true = conectar
            5,            // Prioridad media
            &connection_task_handle,
            0             // Núcleo 0
        );
        
        if (result != pdPASS) {
            ESP_LOGE(TAG, "❌ Error al crear tarea de conexión");
            connection_task_running = false;
        }

    } else {
        // Crear tarea para desconectar (no bloqueante)
        connection_task_running = true;
        BaseType_t result = xTaskCreatePinnedToCore(
            connection_task,
            "Connection_Task",
            4096,
            (void*)false, // false = desconectar
            5,            // Prioridad media
            &connection_task_handle,
            0             // Núcleo 0
        );
        
        if (result != pdPASS) {
            ESP_LOGE(TAG, "❌ Error al crear tarea de desconexión");
            connection_task_running = false;
        }
    }
}
// Tarea para manejar conexiones sin bloquear el botón
void connection_task(void* arg) {
    bool should_connect = (bool)arg;

    ESP_LOGI(TAG, "🔧 Tarea de conexión iniciada - Modo: %s", should_connect ? "CONECTAR" : "DESCONECTAR");
    gpio_set_level(0, 1);
    
    if (should_connect) {
        // Verificar que hay credenciales antes de intentar conectar
        if (!smartconfig_has_saved_credentials()) {
            ESP_LOGW(TAG, "⚠️ No hay credenciales WiFi configuradas");
            ESP_LOGW(TAG, "💡 Mantén presionado el botón para configurar WiFi con SmartConfig");
            connection_task_running = false;
            vTaskDelete(NULL);
            return;
        }

        // Intentar conectar WiFi
        if (!conectar_wifi_con_reintentos()) {
            ESP_LOGW(TAG, "❌ Fallo en conexión WiFi - Continuando en modo offline");
            connection_task_running = false;
            vTaskDelete(NULL);
            return;
        }

        // Intentar conectar MQTT
        if (!conectar_mqtt_con_reintentos()) {
            ESP_LOGW(TAG, "❌ Fallo en conexión MQTT - WiFi conectado pero sin MQTT");
            // Mantener WiFi conectado aunque MQTT falle
            sistema.estado.conexion_boton_activa = true;
            ESP_LOGI(TAG, "✅ WiFi conectado (MQTT falló) - Usa el botón para desconectar");
            connection_task_running = false;
            vTaskDelete(NULL);
            return;
        }

        publicar_estado("esp32/halo/status", "✅✅✅SISTEMA CONECTADO ✅✅✅");

        gpio_set_level(0, 0);

        publicar_estado("esp32/halo/conection", "ON");
        sistema.estado.conexion_boton_activa = true;
        ESP_LOGI(TAG, "✅ Sistema conectado: WiFi + MQTT activos");
    
        uint16_t voltage;
        char msg[64];
        battery_get_voltage(&voltage);
        ESP_LOGW("BATT", "Battery voltage %u mV", voltage);

        snprintf(msg, sizeof(msg), "{\"battery_voltage\":%u}", voltage);
        ESP_LOGI("BATTERY", "📤 Enviando voltaje: %s", msg);
     
        esp_mqtt_client_publish(mqtt_client, "esp32/halo/battery", msg, 0, 1, 0);



    } else {
        publicar_estado("esp32/halo/status", "DESCONECTANDO...");
        desconectar_mqtt_seguro();
        desconectar_wifi_seguro();

        sistema.estado.conexion_boton_activa = false;
        ESP_LOGI(TAG, "✅ Sistema desconectado");
        gpio_set_level(0, 0);
    }
    
    connection_task_running = false;
    vTaskDelete(NULL);
}