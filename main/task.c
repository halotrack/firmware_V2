#include "../include/task.h"
#include "../include/button_actions.h"
extern const char *TAG;

// Variables globales del botón
static QueueHandle_t gpio_evt_queue = NULL;
static TaskHandle_t user_button_task_handle = NULL;
static button_handler_t button_handler = {0};

void IRAM_ATTR user_button_isr_handler(void* arg) {
    uint32_t gpio_num = (uint32_t) arg;
    // Solo enviar evento si el botón está en estado válido
    if (button_handler.state == BUTTON_STATE_IDLE || 
        button_handler.state == BUTTON_STATE_RELEASE_WAIT) {
        xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
    }
}

static void process_button_state(uint32_t current_time) {
    bool button_pressed = (gpio_get_level(USER_BUTTON) == 1); // Pull-down, 1 = presionado
    
    
    switch (button_handler.state) {
        case BUTTON_STATE_IDLE:
            if (button_pressed) {
                ESP_LOGI(TAG, "🔘 Botón presionado detectado");
                gpio_set_level(LED_USER, 1); // Enciende el LED 
                
                button_handler.state = BUTTON_STATE_DEBOUNCE;
                button_handler.press_start_time = current_time;
                button_handler.long_press_detected = false;
                button_handler.double_click_detected = false;
            }
            break;
        case BUTTON_STATE_DEBOUNCE:
            if (!button_pressed) {
                button_handler.state = BUTTON_STATE_IDLE;
            } else if (current_time - button_handler.press_start_time >= BUTTON_DEBOUNCE_MS) {
                button_handler.state = BUTTON_STATE_PRESSED;
                button_handler.press_start_time = current_time;
            }
            break;
        case BUTTON_STATE_PRESSED:
            if (!button_pressed) {
                button_handler.state = BUTTON_STATE_RELEASE_WAIT;
                button_handler.release_time = current_time;
                uint32_t press_duration = current_time - button_handler.press_start_time;
                if (press_duration < BUTTON_LONG_PRESS_MS) {
                    button_handler.click_count++;
                    if (button_handler.click_count == 1) {
                        button_handler.state = BUTTON_STATE_DOUBLE_CLICK_WAIT;
                    } else if (button_handler.click_count == 2) {
                        button_handler.double_click_detected = true;
                        button_handler.click_count = 0;
                    }
                }
            } else if (current_time - button_handler.press_start_time >= BUTTON_LONG_PRESS_MS) {
                button_handler.state = BUTTON_STATE_LONG_PRESS;
                button_handler.long_press_detected = true;
                button_handler.click_count = 0;

///////////////////////borrar credenciales tras pulsacion larga, 
                //smartconfig_clear_credentials();
                ESP_LOGI(TAG, "🔥 PULSACIÓN LARGA DETECTADA - Activando modo SmartConfig");
                // Publicar mensaje solo si hay conexión MQTT
                if (mqtt_is_connected()) {
                    esp_mqtt_client_publish(mqtt_client, "esp32/halo/status", "🔥 PULSACIÓN LARGA DETECTADA - ACTIVANDO SMARTCONFIG", 0, 1, 0);
                } else {
                    ESP_LOGI(TAG, "📱 SmartConfig: Pulsación larga detectada - Sin conexión MQTT actual");
                }
            }
            break;
        case BUTTON_STATE_LONG_PRESS:
            if (!button_pressed) {
                button_handler.state = BUTTON_STATE_RELEASE_WAIT;
                button_handler.release_time = current_time;
            }
            break;
        case BUTTON_STATE_RELEASE_WAIT:
            if (current_time - button_handler.release_time >= 100) {
                button_handler.state = BUTTON_STATE_IDLE;
            }
            break;
        case BUTTON_STATE_DOUBLE_CLICK_WAIT:
            if (button_pressed) {
                button_handler.click_count++;
                if (button_handler.click_count == 2) {
                    button_handler.double_click_detected = true;
                    button_handler.click_count = 0;
                }
                button_handler.state = BUTTON_STATE_DEBOUNCE;
                button_handler.press_start_time = current_time;
            } else if (current_time - button_handler.release_time >= BUTTON_DOUBLE_CLICK_MS) {
                button_handler.state = BUTTON_STATE_IDLE;
            }
            break;
        default:
            button_handler.state = BUTTON_STATE_IDLE;
            break;
    }
}

void user_button_task(void* arg) {
    uint32_t io_num;
    TickType_t last_state_check = 0;
    memset(&button_handler, 0, sizeof(button_handler));
    button_handler.state = BUTTON_STATE_IDLE;
    for (;;) {
        TickType_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
        if (current_time - last_state_check >= 50) {
            process_button_state(current_time);
            last_state_check = current_time;
        }
        if (xQueueReceive(gpio_evt_queue, &io_num, pdMS_TO_TICKS(100))) {
            continue;
        }
        // Manejar todos los tipos de eventos del botón
        if (button_handler.long_press_detected) {
            button_handler.long_press_detected = false;
            manejar_pulsacion_larga();
        } else if (button_handler.double_click_detected) {
            button_handler.double_click_detected = false;
            manejar_click_doble();
        } else if (button_handler.click_count == 1 && 
                   button_handler.state == BUTTON_STATE_IDLE) {
            button_handler.click_count = 0;
            manejar_click_simple();
        }
    }
}

void user_button_init(void) {
    ESP_LOGI(TAG, "🔧 Inicializando botón de usuario en pin %d", USER_BUTTON);
    
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_POSEDGE,  // Cambiado a POSEDGE para pull-down
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << USER_BUTTON),
        .pull_up_en = 0,
        .pull_down_en = 0,
    };
    gpio_config(&io_conf);
    
    // Verificar estado inicial del botón
    int button_state = gpio_get_level(USER_BUTTON);
    ESP_LOGI(TAG, "🔍 Estado inicial del botón (pin %d): %s", USER_BUTTON, 
             button_state ? "PRESIONADO (HIGH)" : "LIBERADO (LOW)");
    gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));
    if (gpio_evt_queue == NULL) {
        ESP_LOGE(TAG, "❌ Error: No se pudo crear cola de eventos del botón");
        return;
    }
    esp_err_t ret = gpio_install_isr_service(0);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "❌ Error al instalar servicio ISR: %s", esp_err_to_name(ret));
        return;
    }
    ret = gpio_isr_handler_add(USER_BUTTON, user_button_isr_handler, (void*) USER_BUTTON);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ Error al agregar manejador ISR: %s", esp_err_to_name(ret));
        return;
    }
    BaseType_t result = xTaskCreatePinnedToCore(
        user_button_task,
        "User_Button",
        TAREA_BUTTON_STACK_SIZE,
        NULL,
        TAREA_BUTTON_PRIORIDAD,
        &user_button_task_handle,
        1
    );
    if (result != pdPASS) {
        ESP_LOGE(TAG, "❌ Error: No se pudo crear tarea de botón de usuario");
        return;
    }
    
    ESP_LOGI(TAG, "✅ Botón de usuario inicializado correctamente en pin %d", USER_BUTTON);
    ESP_LOGI(TAG, "📋 Configuración: Pull-down habilitado, Interrupción en flanco positivo");
    ESP_LOGI(TAG, "⏱️ Tiempo de pulsación larga: %d ms", BUTTON_LONG_PRESS_MS);
}

void create_task_HX711(void){
    xTaskCreatePinnedToCore(
        task_HX711,                     // Función de la tarea
        "HX711_Sensor",                 // Nombre descriptivo
        TAREA_HX711_STACK_SIZE,         // Stack optimizado: 3072 bytes
        NULL,                           // Parámetros
        TAREA_HX711_PRIORIDAD,          // Prioridad ALTA: 6
        NULL,                           // Handle (no necesario)
        NUCLEO_PROTOCOLO                // NÚCLEO 0: dedicado a protocolo
    );
}


 void create_task_MQTT(void){
    xTaskCreatePinnedToCore(
        task_MQTT,                      // Función de la tarea
        "MQTT_Network",                 // Nombre descriptivo  
        TAREA_MQTT_STACK_SIZE,          // Stack aumentado: 6144 bytes (SSL)
        NULL,                           // Parámetros
        TAREA_MQTT_PRIORIDAD,           // Prioridad MEDIA: 4
        NULL,                           // Handle (no necesario)
        NUCLEO_APLICACION               // NÚCLEO 1: aplicación y red
    );
}

// Definición de los estados principales de la tarea HX711
typedef enum {
    HX711_ESPERA_INICIALIZACION,
    HX711_ESPERA_FECHA_HORA,
    HX711_ESPERA_COMANDO,
    HX711_CALIBRACION,
    HX711_ESPERA_PESO,
    HX711_ESPERA_CONFIG_HORARIO,
    HX711_MEDICION
} hx711_task_state_t;

// Definición de los estados principales de la tarea MQTT
typedef enum {
    MQTT_ESPERA_INICIALIZACION,
    MQTT_ESPERA_HORARIO_ENVIO,
    MQTT_CONECTANDO_WIFI,
    MQTT_CONECTANDO_BROKER,
    MQTT_ENVIANDO_DATOS,
    MQTT_FINALIZANDO_ENVIO,
    MQTT_ESPERANDO_SIGUIENTE_CICLO
} mqtt_task_state_t;

// Función auxiliar para determinar el próximo estado basado en las banderas del sistema
static hx711_task_state_t hx711_get_next_state(bool calibracion_ejecutada) {
    // Verificación en orden de prioridad
    if (!sistema.estado.sistema_inicializado) return HX711_ESPERA_INICIALIZACION;
    
    // Si no hay WiFi conectado, saltar estados que requieren servidor y ir directo a medición
    bool wifi_available = wifi_is_connected();
    
    if (!wifi_available) {
        // Log solo cada 60 segundos para evitar spam
                        // Reducir logs offline - solo cada 5 minutos
        static uint32_t last_offline_log = 0;
        uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
                        if (current_time - last_offline_log >= 300000) {
                            ESP_LOGI(TAG, "Modo offline - mediciones en SD");
            last_offline_log = current_time;
        }
        return HX711_MEDICION;
    }
    
    // Estados que requieren conexión al servidor
    if (sistema.estado.esperando_fecha_hora) return HX711_ESPERA_FECHA_HORA;
    if (sistema.estado.esperando_comando) return HX711_ESPERA_COMANDO;
    if (sistema.estado.sistema_calibrado && !calibracion_ejecutada) return HX711_CALIBRACION;
    if (sistema.estado.esperando_comando_peso) return HX711_ESPERA_PESO;
    if (sistema.estado.esperando_config_horario) return HX711_ESPERA_CONFIG_HORARIO;
    
    return HX711_MEDICION;
}

void task_HX711(void *pvParameters) {
    hx711_task_state_t estado = HX711_ESPERA_INICIALIZACION;
    static bool calibracion_ejecutada = false;
    static uint32_t last_log_time = 0;
    const uint32_t LOG_INTERVAL_MS = 10000; // Log cada 10 segundos para estados de espera
    
    while (1) {
        uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
        
        switch (estado) {
            case HX711_ESPERA_INICIALIZACION:
                vTaskDelay(1000 / portTICK_PERIOD_MS);
                estado = hx711_get_next_state(calibracion_ejecutada);
                break;
                
            case HX711_ESPERA_FECHA_HORA:
                // Log reducido
                if (current_time - last_log_time >= LOG_INTERVAL_MS) {
                    ESP_LOGI(TAG, "Esperando fecha/hora del servidor...");
                    last_log_time = current_time;
                }
                vTaskDelay(2000 / portTICK_PERIOD_MS);
                estado = hx711_get_next_state(calibracion_ejecutada);
                break;
                
            case HX711_ESPERA_COMANDO:
                // Log reducido
                if (current_time - last_log_time >= LOG_INTERVAL_MS) {
                    ESP_LOGI(TAG, "Esperando comando de calibración...");
                    last_log_time = current_time;
                }
                vTaskDelay(2000 / portTICK_PERIOD_MS);
                estado = hx711_get_next_state(calibracion_ejecutada);
                break;
                
            case HX711_CALIBRACION:
                ESP_LOGI(TAG, "=== EJECUTANDO CALIBRACIÓN SOLICITADA ===");
                hx711_calibrar_inicial();
                sistema.estado.calibracion_completada = true;
                sistema.estado.esperando_comando_peso = true;
                calibracion_ejecutada = true;
                estado = HX711_ESPERA_PESO;
                break;
                
            case HX711_ESPERA_PESO:
                vTaskDelay(2000 / portTICK_PERIOD_MS);
                estado = hx711_get_next_state(calibracion_ejecutada);
                break;
                
            case HX711_ESPERA_CONFIG_HORARIO:
                // Log periódico para evitar spam
                if (current_time - last_log_time >= LOG_INTERVAL_MS) {
                    ESP_LOGI(TAG, "Esperando configuración de horario de envío...");
                    last_log_time = current_time;
                }
                vTaskDelay(2000 / portTICK_PERIOD_MS);
                estado = hx711_get_next_state(calibracion_ejecutada);
                break;
                
            case HX711_MEDICION: {
                struct tm timeinfo;
                bool tiempo_valido = rtc_get_time(&timeinfo);
                
                // Si no hay tiempo del RTC, usar tiempo del sistema (desde boot)
                if (!tiempo_valido) {
                    ESP_LOGW(TAG, "⚠️ RTC no disponible - usando tiempo del sistema");
                    time_t now = time(NULL);
                    if (now > 0) {
                        localtime_r(&now, &timeinfo);
                        tiempo_valido = true;
                    } else {
                        // Como último recurso, usar timestamp desde boot
                        uint32_t uptime_seconds = xTaskGetTickCount() * portTICK_PERIOD_MS / 1000;
                        timeinfo.tm_year = 70;  // 1970
                        timeinfo.tm_mon = 0;    // Enero
                        timeinfo.tm_mday = 1 + (uptime_seconds / 86400);  // Días desde epoch
                        timeinfo.tm_hour = (uptime_seconds % 86400) / 3600;
                        timeinfo.tm_min = (uptime_seconds % 3600) / 60;
                        timeinfo.tm_sec = uptime_seconds % 60;
                        tiempo_valido = true;
                        ESP_LOGW(TAG, "⚠️ Usando timestamp desde boot: %d días desde inicio", timeinfo.tm_mday - 1);
                    }
                }
                
                if (tiempo_valido) {
                    float peso = hx711_leer_peso();
                    if (peso > HX711_ERROR_THRESHOLD) {
                        if (xSemaphoreTake(sistema.mutex_sd, pdMS_TO_TICKS(1000)) == pdTRUE) {
                            sdcard_log_peso(peso, &timeinfo);
                            sdcard_log_voltaje(&timeinfo);
                            xSemaphoreGive(sistema.mutex_sd);
                        } else {
                            ESP_LOGW(TAG, "⚠️ No se pudo obtener mutex de SD - saltando medición");
                        }
                    } else {
                        ESP_LOGE(TAG, "❌ Error al obtener peso del sensor");
                    }
                } else {
                    ESP_LOGE(TAG, "❌ No se pudo obtener timestamp válido");
                }
                
                vTaskDelay(sistema.envio.muestreo_ms / portTICK_PERIOD_MS);
                // Solo verificar cambio de estado después del delay de medición
                estado = hx711_get_next_state(calibracion_ejecutada);
                break;
            }
            
            default:
                ESP_LOGW(TAG, "Estado HX711 inválido: %d", estado);
                estado = HX711_ESPERA_INICIALIZACION;
                break;
        }
    }
}


// Función auxiliar para determinar si es hora de envío MQTT
static bool mqtt_es_hora_envio(const struct tm *timeinfo) {
    return (timeinfo->tm_mday != sistema.envio.ultimo_dia_envio &&
            (timeinfo->tm_hour > sistema.envio.hora_envio ||
             (timeinfo->tm_hour == sistema.envio.hora_envio && timeinfo->tm_min >= sistema.envio.minuto_envio)));
}

// Función auxiliar para conectar WiFi con timeout
static bool mqtt_conectar_wifi(void) {
    return conectar_wifi_con_reintentos();
}

// Función auxiliar para conectar broker MQTT
static bool mqtt_conectar_broker(void) {
    ESP_LOGI(TAG, "🔄 Conectando al broker MQTT...");
    
    // Asegurar que el cliente MQTT esté inicializado
    if (mqtt_client == NULL) {
        ESP_LOGI(TAG, "Inicializando cliente MQTT (inicio diferido)...");
        mqtt_init();
        vTaskDelay(500 / portTICK_PERIOD_MS);
    }
    
    for (int intento = 1; intento <= 3; intento++) {
        esp_mqtt_client_start(mqtt_client);
        
        int mqtt_timeout = 0;
        while (!mqtt_is_connected() && mqtt_timeout < MQTT_TIMEOUT_SECONDS) {
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            mqtt_timeout++;
        }
        
        if (mqtt_is_connected()) {
            ESP_LOGI(TAG, "✅ Broker MQTT conectado exitosamente");
            return true;
        } else {
            ESP_LOGW(TAG, "⚠️ Intento %d fallido, esperando antes del siguiente...", intento);
            esp_mqtt_client_stop(mqtt_client);
            if (intento < 3) {
                vTaskDelay(5000 / portTICK_PERIOD_MS);
            }
        }
    }
    
    ESP_LOGE(TAG, "❌ No se pudo conectar al broker MQTT después de 3 intentos");
    return false;
}

// Función auxiliar para contar y enviar datos desde SD
static int mqtt_enviar_datos_sd(const struct tm *timeinfo) {
    FILE *f = NULL;
    int mensajes_enviados = 0;
    int total_pendientes = 0;
    
    // Verificar si hay datos pendientes ANTES de conectar
    if (xSemaphoreTake(sistema.mutex_sd, pdMS_TO_TICKS(1000)) == pdTRUE) {
        f = fopen("/sdcard/pesos.csv", "r");
    if (f) {
        char line[128];
            int line_number = 0;
            
            // Saltar líneas ya enviadas
            while (line_number < sistema.envio.ultima_muestra_enviada && fgets(line, sizeof(line), f)) {
                if (strstr(line, "Fecha")) continue;
                line_number++;
            }
            
            // Contar líneas pendientes
        struct tm limite_envio = *timeinfo;
        limite_envio.tm_hour = sistema.envio.hora_envio;
        limite_envio.tm_min = sistema.envio.minuto_envio;
        limite_envio.tm_sec = 0;
        time_t timestamp_limite = mktime(&limite_envio);
        
            while (fgets(line, sizeof(line), f)) {
                if (strstr(line, "Fecha")) continue;
                
                int y, m, d, h, min, s;
                float p;
                if (sscanf(line, "%d-%d-%d,%d:%d:%d,%f", &y, &m, &d, &h, &min, &s, &p) == 7) {
                    struct tm muestra_time = {0};
                    muestra_time.tm_year = y - 1900;
                    muestra_time.tm_mon = m - 1;
                    muestra_time.tm_mday = d;
                    muestra_time.tm_hour = h;
                    muestra_time.tm_min = min;
                    muestra_time.tm_sec = s;
                    time_t timestamp_muestra = mktime(&muestra_time);
                    
                    if (timestamp_muestra <= timestamp_limite) {
                        total_pendientes++;
                    } else {
                        break;
                    }
                }
            }
            
            // Si hay datos pendientes, reposicionar y enviar
            if (total_pendientes > 0) {
                ESP_LOGI(TAG, "📤 Hay %d mensajes pendientes - iniciando envío", total_pendientes);
                rewind(f);
                line_number = 0;
                
                // Saltar líneas ya enviadas nuevamente
        while (line_number < sistema.envio.ultima_muestra_enviada && fgets(line, sizeof(line), f)) {
            if (strstr(line, "Fecha")) continue;
            line_number++;
        }
        
                // Enviar datos
                while (fgets(line, sizeof(line), f) && mensajes_enviados < total_pendientes) {
            if (strstr(line, "Fecha")) continue;
            
            if (!mqtt_is_connected()) {
                        ESP_LOGE(TAG, "Conexión MQTT perdida");
                break;
            }
            
            int y, m, d, h, min, s;
            float p;
            if (sscanf(line, "%d-%d-%d,%d:%d:%d,%f", &y, &m, &d, &h, &min, &s, &p) == 7) {
                struct tm muestra_time = {0};
                muestra_time.tm_year = y - 1900;
                muestra_time.tm_mon = m - 1;
                muestra_time.tm_mday = d;
                muestra_time.tm_hour = h;
                muestra_time.tm_min = min;
                muestra_time.tm_sec = s;
                
                    esp_err_t result = mqtt_enviar_datos(p, &muestra_time, "Datos históricos");
                    if (result == ESP_OK) {
                        mensajes_enviados++;
                        sistema.envio.ultima_muestra_enviada++;
                            vTaskDelay(100 / portTICK_PERIOD_MS); // Reducido delay
                } else {
                    break;
                }
            }
                }
            } else {
                ESP_LOGI(TAG, "📭 No hay datos pendientes de envío");
        }
        
        fclose(f);
    }
        xSemaphoreGive(sistema.mutex_sd);
    }
    
    return mensajes_enviados;
}

// Función auxiliar para finalizar envío y desconectar
static void mqtt_finalizar_envio(int mensajes_enviados) {
    ESP_LOGI(TAG, "✅ ENVIO COMPLETO. Total mensajes enviados: %d", mensajes_enviados);
    esp_mqtt_client_publish(mqtt_client, "esp32/halo/status", "✅ Datos enviados correctamente", 0, 1, 0);
    
    vTaskDelay(2000 / portTICK_PERIOD_MS);
    
    // Actualizar estado del sistema
    guardar_ultima_muestra_enviada();
    
    struct tm timeinfo;
    if (rtc_get_time(&timeinfo)) {
        sistema.envio.ultimo_dia_envio = timeinfo.tm_mday;
    }
    
    // Desconectar MQTT pero mantener WiFi para próximas conexiones
    ESP_LOGI(TAG, "🔌 Desconectando MQTT tras envío exitoso...");
    esp_mqtt_client_publish(mqtt_client, "esp32/halo/conection", "OFF", 0, 1, 0);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    esp_mqtt_client_publish(mqtt_client, "esp32/halo/status", "📱 No hay datos pendientes - desconectando para ahorrar energía", 0, 1, 0);
    gpio_set_level(LED_USER, 0);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    
    // Solo desconectar MQTT, mantener WiFi activo
    desconectar_mqtt_seguro();
    desconectar_wifi_seguro();
}

typedef struct {
    mqtt_task_state_t estado;
    TickType_t ultima_verificacion_mqtt;
    TickType_t last_log_tick;
    int mensajes_enviados;
} mqtt_context_t;

// Constantes
static const TickType_t INTERVALO_VERIFICACION = pdMS_TO_TICKS(1000);
static const TickType_t TIMEOUT_RECONEXION = pdMS_TO_TICKS(600000);
static const TickType_t LOG_INTERVAL = pdMS_TO_TICKS(30000);

// ----------------------
// Funciones auxiliares
// ----------------------
static void log_esperando_envio(const struct tm *timeinfo) {
    ESP_LOGI(TAG, "⏰ Esperando horario de envío: %02d:%02d (actual: %02d:%02d)",
             sistema.envio.hora_envio, sistema.envio.minuto_envio,
             timeinfo->tm_hour, timeinfo->tm_min);
}

static BaseType_t verificar_horario_envio(struct tm *timeinfo) {
    if (!rtc_get_time(timeinfo)) return pdFALSE;
    return mqtt_es_hora_envio(timeinfo) ? pdTRUE : pdFALSE;
}

// ----------------------
// Tarea principal MQTT
// ----------------------
void task_MQTT(void *pvParameters) {
    mqtt_context_t ctx = {
        .estado = MQTT_ESPERA_INICIALIZACION,
        .ultima_verificacion_mqtt = 0,
        .last_log_tick = 0,
        .mensajes_enviados = 0
    };

    struct tm timeinfo;

    while (1) {
        TickType_t tick_actual = xTaskGetTickCount();

        switch (ctx.estado) {

            case MQTT_ESPERA_INICIALIZACION:
                if (sistema.estado.sistema_inicializado) {
                    ctx.estado = MQTT_ESPERA_HORARIO_ENVIO;
                } else {
                    vTaskDelay(INTERVALO_VERIFICACION);
                }
                break;

            case MQTT_ESPERA_HORARIO_ENVIO:
                if (verificar_horario_envio(&timeinfo)) {
                    // Verificar si realmente hay datos pendientes antes de conectar
                    FILE *check_file = fopen("/sdcard/pesos.csv", "r");
                    bool hay_datos_pendientes = false;
                    
                    if (check_file) {
                        char line[128];
                        int count = 0;
                        while (fgets(line, sizeof(line), check_file) && count < sistema.envio.ultima_muestra_enviada + 5) {
                            if (!strstr(line, "Fecha")) count++;
                        }
                        hay_datos_pendientes = (count > sistema.envio.ultima_muestra_enviada);
                        fclose(check_file);
                    }
                    
                    if (hay_datos_pendientes) {
                        ////ESP_LOGI(TAG, "📤 Hay datos pendientes - manteniendo conexión para envío automático");
                    ctx.estado = MQTT_CONECTANDO_WIFI;
                    } else {
                        ESP_LOGI(TAG, "📭 No hay datos pendientes - saltando conexión MQTT");
                        struct tm current_time;
                        if (rtc_get_time(&current_time)) {
                            sistema.envio.ultimo_dia_envio = current_time.tm_mday;
                        }
                        // Esperar hasta el próximo día
                        vTaskDelay(pdMS_TO_TICKS(3600000)); // 1 hora
                    }
                } else {
                    if ((tick_actual - ctx.last_log_tick) >= LOG_INTERVAL) {
                        log_esperando_envio(&timeinfo);
                        ctx.last_log_tick = tick_actual;
                    }
                    vTaskDelay(INTERVALO_VERIFICACION);
                }
                break;

            case MQTT_CONECTANDO_WIFI:
                if (mqtt_conectar_wifi()) {
                    ctx.estado = MQTT_CONECTANDO_BROKER;
                } else {
                    ctx.ultima_verificacion_mqtt = tick_actual;
                    ctx.estado = MQTT_ESPERANDO_SIGUIENTE_CICLO;
                }
                break;

            case MQTT_CONECTANDO_BROKER:
                if (mqtt_conectar_broker()) {
                    vTaskDelay(pdMS_TO_TICKS(2000)); // estabilizar conexión
                    ctx.estado = MQTT_ENVIANDO_DATOS;
                } else {
                    // No desconectar WiFi - mantenerlo para próximas conexiones
                    ctx.ultima_verificacion_mqtt = tick_actual;
                    ctx.estado = MQTT_ESPERANDO_SIGUIENTE_CICLO;
                }
                break;

            case MQTT_ENVIANDO_DATOS:
                ctx.mensajes_enviados = mqtt_enviar_datos_sd(&timeinfo);
                ctx.estado = MQTT_FINALIZANDO_ENVIO;
                break;

            case MQTT_FINALIZANDO_ENVIO:
                mqtt_finalizar_envio(ctx.mensajes_enviados);
                ctx.estado = MQTT_ESPERA_HORARIO_ENVIO;
                break;

            case MQTT_ESPERANDO_SIGUIENTE_CICLO:
                if ((tick_actual - ctx.ultima_verificacion_mqtt) >= TIMEOUT_RECONEXION) {
                    ctx.ultima_verificacion_mqtt = 0;
                    ctx.estado = MQTT_ESPERA_HORARIO_ENVIO;
                } else {
                    vTaskDelay(INTERVALO_VERIFICACION);
                }
                break;

            default:
                ESP_LOGW(TAG, "Estado MQTT inválido: %d", ctx.estado);
                ctx.estado = MQTT_ESPERA_INICIALIZACION;
                break;
        }
    }
}