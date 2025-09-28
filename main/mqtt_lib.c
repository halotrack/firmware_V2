#include "../include/mqtt_lib.h"
#include "../include/HALO.h"
#include "../include/battery.h"
#include "../include/ota_lib.h"
#include <esp_log.h>
#include <stdio.h>
#include <string.h>
#include "../include/hx711_lib.h"


// =====================================================
// CONSTANTES Y CONFIGURACIONES
// =====================================================
static const char *MQTT_TAG = "MQTT_LIB";

// === TIMEOUTS Y LÍMITES ===
#define MQTT_TOPIC_BUFFER_SIZE          64
#define MQTT_DATA_BUFFER_SIZE           512
#define MQTT_MESSAGE_BUFFER_SIZE        256
#define MQTT_STATUS_BUFFER_SIZE         128
#define MQTT_RECONNECT_TIMEOUT_MS       10000
#define MQTT_OPERATION_TIMEOUT_MS       5000
#define MQTT_MAX_RECONNECT_ATTEMPTS     3
#define MQTT_WAIT_BEFORE_RECONNECT_MS   2000

// === TOPICS MQTT ===
#define MQTT_TOPIC_COMMAND              "esp32/command"
#define MQTT_TOPIC_COMMAND_OTA          "esp32/command_ota"
#define MQTT_TOPIC_SET_SCHEDULE         "esp32/set_schedule"
#define MQTT_TOPIC_SET_TIME             "esp32/set_time"
#define MQTT_TOPIC_STATUS               "esp32/halo/status"
#define MQTT_TOPIC_CONECTION            "esp32/halo/conection"
#define MQTT_TOPIC_WEIGHT_DATA          "esp32/halo/weight_data"
#define MQTT_TOPIC_TEST                 "esp32/test"


// === MENSAJES DE ESTADO ===
#define MQTT_MSG_REINICIANDO            "Reiniciando equipo..."
#define MQTT_MSG_SISTEMA_REINICIADO     "Sistema reiniciado"
#define MQTT_MSG_CALIBRACION_INICIADA   "1.CALIBRACIÓN INICIADA"
#define MQTT_MSG_ESPERANDO_FECHA        "Esperando la fecha y hora............"
#define MQTT_MSG_REGISTRO_RESETEADO     "Registro de envío diario reseteado"
#define MQTT_MSG_ESPERANDO_HORARIO      "Envíe el horario de envío en formato HH:MM al topic esp32/set_schedule"

// =====================================================
// VARIABLES GLOBALES
// =====================================================
esp_mqtt_client_handle_t mqtt_client = NULL;              // Cliente MQTT global (usado por otras librerías)
static bool mqtt_connected = false;                        // Estado de conexión MQTT
static bool mqtt_initialization_complete = false;          // Flag de inicialización completa

// =====================================================
// DECLARACIONES DE FUNCIONES AUXILIARES
// =====================================================
static bool mqtt_validate_client(void);
static bool mqtt_validate_topic_data(const char* topic, const char* data);
static esp_err_t mqtt_safe_publish(const char* topic, const char* message, bool retain);
static bool mqtt_validate_time_format(const char* time_str, int* hour, int* minute);
static bool mqtt_validate_datetime_format(const char* datetime_str, struct tm* time_struct);
static esp_err_t mqtt_subscribe_to_topics(void);
static void mqtt_log_connection_status(const char* operation);

// =====================================================
// FUNCIONES AUXILIARES PRIVADAS
// =====================================================

/**
 * @brief Registra el estado de conexión MQTT para operaciones específicas
 * @param operation Descripción de la operación que se está realizando
 */
// Función eliminada para reducir logs redundantes

/**
 * @brief Valida que el cliente MQTT esté inicializado y conectado
 * @return true si el cliente es válido, false en caso contrario
 */
static bool mqtt_validate_client(void) {
    if (mqtt_client == NULL) {
        ESP_LOGE(MQTT_TAG, "❌ Cliente MQTT no inicializado");
        return false;
    }
    
    if (!mqtt_connected) {
        ESP_LOGW(MQTT_TAG, "⚠️ Cliente MQTT no conectado");
        return false;
    }
    
    return true;
}

/**
 * @brief Valida que el topic y data recibidos sean válidos
 * @param topic Topic MQTT recibido
 * @param data Datos MQTT recibidos
 * @return true si son válidos, false en caso contrario
 */
static bool mqtt_validate_topic_data(const char* topic, const char* data) {
    if (topic == NULL || data == NULL) {
        ESP_LOGE(MQTT_TAG, "❌ Topic o data es NULL");
        return false;
    }
    
    if (strlen(topic) == 0 || strlen(data) == 0) {
        ESP_LOGE(MQTT_TAG, "❌ Topic o data está vacío");
        return false;
    }
    
    if (strlen(topic) >= MQTT_TOPIC_BUFFER_SIZE) {
        ESP_LOGE(MQTT_TAG, "❌ Topic excede el tamaño máximo (%d bytes)", MQTT_TOPIC_BUFFER_SIZE);
        return false;
    }
    
    if (strlen(data) >= MQTT_DATA_BUFFER_SIZE) {
        ESP_LOGE(MQTT_TAG, "❌ Data excede el tamaño máximo (%d bytes)", MQTT_DATA_BUFFER_SIZE);
        return false;
    }
    
    
    return true;
}

/**
 * @brief Publica un mensaje MQTT de forma segura con validación
 * @param topic Topic de destino
 * @param message Mensaje a enviar
 * @param retain Si el mensaje debe ser retenido
 * @return ESP_OK si se envió correctamente, error en caso contrario
 */
static esp_err_t mqtt_safe_publish(const char* topic, const char* message, bool retain) {
    if (!mqtt_validate_client()) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (topic == NULL || message == NULL) {
        ESP_LOGE(MQTT_TAG, "❌ Topic o mensaje es NULL");
        return ESP_ERR_INVALID_ARG;
    }
    
    int msg_id = esp_mqtt_client_publish(mqtt_client, topic, message, 0, 1, retain ? 1 : 0);
    
    if (msg_id >= 0) {
        ESP_LOGD(MQTT_TAG, "✅ Mensaje enviado - Topic: %s, ID: %d", topic, msg_id);
        return ESP_OK;
    } else {
        ESP_LOGE(MQTT_TAG, "❌ Error al enviar mensaje - Topic: %s, Error: %d", topic, msg_id);
        return ESP_FAIL;
    }
}

/**
 * @brief Valida formato de hora HH:MM
 * @param time_str String de tiempo a validar
 * @param hour Puntero para almacenar la hora
 * @param minute Puntero para almacenar el minuto
 * @return true si el formato es válido, false en caso contrario
 */
static bool mqtt_validate_time_format(const char* time_str, int* hour, int* minute) {
    if (time_str == NULL || hour == NULL || minute == NULL) {
        return false;
    }
    
    if (sscanf(time_str, "%d:%d", hour, minute) != 2) {
        ESP_LOGE(MQTT_TAG, "❌ Formato de hora inválido: '%s'", time_str);
        return false;
    }
    
    if (*hour < 0 || *hour > 23 || *minute < 0 || *minute > 59) {
        ESP_LOGE(MQTT_TAG, "❌ Hora fuera de rango: %02d:%02d", *hour, *minute);
        return false;
    }
    
    return true;
}


/**
 * @brief Suscribe el cliente MQTT a todos los topics necesarios
 * @return ESP_OK si todas las suscripciones fueron exitosas, ESP_FAIL en caso contrario
 */
static esp_err_t mqtt_subscribe_to_topics(void) {
    if (!mqtt_validate_client()) {
        ESP_LOGE(MQTT_TAG, "❌ Cliente MQTT no válido para suscripción");
        return ESP_FAIL;
    }
    
    // Suscribirse al topic de comandos
    int result = esp_mqtt_client_subscribe(mqtt_client, MQTT_TOPIC_COMMAND, 0);
    if (result == -1) {
        ESP_LOGE(MQTT_TAG, "❌ Error al suscribirse a %s", MQTT_TOPIC_COMMAND);
        return ESP_FAIL;
    }

    
    // Suscribirse al topic de configuración de horario
    result = esp_mqtt_client_subscribe(mqtt_client, MQTT_TOPIC_SET_SCHEDULE, 0);
    if (result == -1) {
        ESP_LOGE(MQTT_TAG, "❌ Error al suscribirse a %s", MQTT_TOPIC_SET_SCHEDULE);
        return ESP_FAIL;
    }

    
    // Suscribirse al topic de configuración de fecha/hora
    result = esp_mqtt_client_subscribe(mqtt_client, MQTT_TOPIC_SET_TIME, 0);
    if (result == -1) {
        ESP_LOGE(MQTT_TAG, "❌ Error al suscribirse a %s", MQTT_TOPIC_SET_TIME);
        return ESP_FAIL;
    }

    // Suscribirse al topic de comandos OTA
    result = esp_mqtt_client_subscribe(mqtt_client, MQTT_TOPIC_COMMAND_OTA, 0);
    if (result == -1) {
        ESP_LOGE(MQTT_TAG, "❌ Error al suscribirse a %s", MQTT_TOPIC_COMMAND_OTA);
        return ESP_FAIL;
    }

    ESP_LOGI(MQTT_TAG, "✅ Suscrito a topic OTA: %s", MQTT_TOPIC_COMMAND_OTA);

    // Evitar múltiples publicaciones inmediatas - optimizar
    esp_mqtt_client_publish(mqtt_client, "esp32/halo/conection", "ON", 0, 1, 0);
    


    return ESP_OK;
}

/**
 * @brief Manejador de eventos MQTT mejorado con validación y logging robusto
 * @param event Evento MQTT recibido
 * @return ESP_OK si el evento se procesó correctamente
 */
static esp_err_t mqtt_event_handler_cb(esp_mqtt_event_handle_t event) {
    if (event == NULL) {
        ESP_LOGE(MQTT_TAG, "❌ Evento MQTT es NULL");
        return ESP_ERR_INVALID_ARG;
    }
    
    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            mqtt_connected = true;
            
            // Suscribirse a todos los topics usando función auxiliar
            esp_err_t subscribe_result = mqtt_subscribe_to_topics();
            if (subscribe_result != ESP_OK) {
                ESP_LOGW(MQTT_TAG, "⚠️ Algunas suscripciones fallaron");
            }
            mqtt_initialization_complete = true;
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGW(MQTT_TAG, "🔌 Desconectado del broker MQTT");
            mqtt_connected = false;
            mqtt_initialization_complete = false;
            break;
            
        case MQTT_EVENT_UNSUBSCRIBED:
            ESP_LOGI(MQTT_TAG, "📤 Desuscripción MQTT exitosa (msg_id=%d)", (int)event->msg_id);
            break;
            
        case MQTT_EVENT_PUBLISHED:
            ESP_LOGD(MQTT_TAG, "📨 Mensaje MQTT publicado (msg_id=%d)", (int)event->msg_id);
            break;
        case MQTT_EVENT_DATA: {
            ESP_LOGI(MQTT_TAG, "📥 Datos MQTT recibidos - Topic: %.*s, Data: %.*s",
                     event->topic_len, event->topic, event->data_len, event->data);
        
        
            // Copiar topic y data en buffers con null-termination segura
            char topic[MQTT_TOPIC_BUFFER_SIZE];
            char data[MQTT_DATA_BUFFER_SIZE];
            
            memcpy(topic, event->topic, event->topic_len);
            topic[event->topic_len] = '\0';
        
            memcpy(data, event->data, event->data_len);
            data[event->data_len] = '\0';
            
            // Validar los datos copiados
            if (!mqtt_validate_topic_data(topic, data)) {
                ESP_LOGE(MQTT_TAG, "❌ Validación de topic/data falló");
                break;
            }
        
            ESP_LOGI(MQTT_TAG, "✅ Topic procesado: '%s'", topic);
            ESP_LOGI(MQTT_TAG, "✅ Payload procesado: '%s'", data);
            
            // Procesar comandos MQTT usando constantes definidas
            if (strcmp(topic, MQTT_TOPIC_COMMAND) == 0) {
                ESP_LOGI(MQTT_TAG, "🎯 Procesando comando: %s", data);
                menu_mqtt(data);

            } else if (strcmp(topic, MQTT_TOPIC_COMMAND_OTA) == 0) {
                ESP_LOGI(MQTT_TAG, "🚀 Procesando comando OTA: %s", data);

                // Verificar si es comando de rollback
                if (strcmp(data, "ROLLBACK") == 0 || strcmp(data, "rollback") == 0) {
                    ESP_LOGI(MQTT_TAG, "🔄 Procesando comando ROLLBACK");

                    // Publicar estado antes de rollback
                    esp_mqtt_client_publish(mqtt_client, MQTT_TOPIC_STATUS,
                                          "🔄 Iniciando rollback a firmware anterior...", 0, 1, 0);

                    // Ejecutar rollback
                    esp_err_t rollback_result = ota_rollback_to_previous();

                    if (rollback_result == ESP_OK) {
                        ESP_LOGI(MQTT_TAG, "✅ Rollback iniciado exitosamente");
                        esp_mqtt_client_publish(mqtt_client, MQTT_TOPIC_STATUS,
                                              "✅ Rollback completado - reiniciando...", 0, 1, 0);
                    } else {
                        ESP_LOGE(MQTT_TAG, "❌ Error en rollback: %s", esp_err_to_name(rollback_result));
                        char error_msg[128];
                        snprintf(error_msg, sizeof(error_msg), "❌ Error rollback: %s", esp_err_to_name(rollback_result));
                        esp_mqtt_client_publish(mqtt_client, MQTT_TOPIC_STATUS, error_msg, 0, 1, 0);
                    }
                } else {
                    // Procesar comando OTA normal con URL
                    ota_process_command(data);
                }

            } else if (strcmp(topic, MQTT_TOPIC_SET_SCHEDULE) == 0) {
                // --- Configuración de HORARIO ---
                if (sistema.estado.esperando_config_horario) {
                ESP_LOGI(MQTT_TAG, "⏰ Procesando configuración de horario: %s", data);
                int nueva_hora, nuevo_minuto;
                if (mqtt_validate_time_format(data, &nueva_hora, &nuevo_minuto)) {
                    sistema.envio.hora_envio = nueva_hora;
                    sistema.envio.minuto_envio = nuevo_minuto;
                    extern void guardar_horario_envio(void);
                    guardar_horario_envio();
                    char mensaje_confirmacion[MQTT_STATUS_BUFFER_SIZE];
                    snprintf(mensaje_confirmacion, sizeof(mensaje_confirmacion), 
                             "Horario de envío actualizado a %02d:%02d", 
                             sistema.envio.hora_envio, sistema.envio.minuto_envio);
                    mqtt_safe_publish(MQTT_TOPIC_STATUS, mensaje_confirmacion, false);
                    mqtt_safe_publish(MQTT_TOPIC_STATUS, "Sistema operativo reanudado tras configuración de horario", false);
                    esp_mqtt_client_publish(mqtt_client, "esp32/halo/conection", "ON", 0, 1, 0);
                        ESP_LOGI(MQTT_TAG, "✅ Horario configurado exitosamente: %02d:%02d", sistema.envio.hora_envio, sistema.envio.minuto_envio);
                    sistema.estado.esperando_config_horario = false;
                } else {
                    ESP_LOGE(MQTT_TAG, "❌ Formato o valores de horario inválidos: %s", data);
                    mqtt_safe_publish(MQTT_TOPIC_STATUS, "Error: Horario inválido. Use formato HH:MM (24h)", false);
                }
                }
                // --- Configuración de MUESTREO ---
                else if (sistema.estado.esperando_comando_muestreo) {
                    ESP_LOGI(MQTT_TAG, "⏱️ Procesando configuración de muestreo: %s", data);
                    int nuevo_muestreo = atoi(data);
                    if (nuevo_muestreo >= 1000 && nuevo_muestreo <= 3600000) { // 1s a 1h
                        sistema.envio.muestreo_ms = nuevo_muestreo;
                        extern void guardar_muestreo_ms(void);
                        guardar_muestreo_ms();
                        char mensaje[MQTT_STATUS_BUFFER_SIZE];
                        snprintf(mensaje, sizeof(mensaje), "Intervalo de muestreo actualizado a %d ms", sistema.envio.muestreo_ms);
                        mqtt_safe_publish(MQTT_TOPIC_STATUS, mensaje, false);
                        mqtt_safe_publish(MQTT_TOPIC_STATUS, "Sistema operativo reanudado tras configuración de muestreo", false);
                        ESP_LOGI(MQTT_TAG, "✅ Intervalo de muestreo configurado exitosamente: %d ms", sistema.envio.muestreo_ms);
                        sistema.estado.esperando_comando_muestreo = false;
                    } else {
                        ESP_LOGE(MQTT_TAG, "❌ Valor de muestreo inválido: %s", data);
                        mqtt_safe_publish(MQTT_TOPIC_STATUS, "Error: Intervalo de muestreo inválido. Use un valor entre 1000 y 3600000 ms", false);
                    }
                } else {
                    ESP_LOGW(MQTT_TAG, "⚠️ No se esperaba configuración de horario ni de muestreo");
                    mqtt_safe_publish(MQTT_TOPIC_STATUS, "Error: No se esperaba configuración de horario ni de muestreo", false);
                }
                break;
            
            } else if (strcmp(topic, MQTT_TOPIC_SET_TIME) == 0) {
                ESP_LOGI(MQTT_TAG, "Configurando fecha/hora: %s", data);
                
                if (!sistema.estado.esperando_fecha_hora) {
                    ESP_LOGW(MQTT_TAG, "⚠️ Sistema no está esperando configuración de fecha/hora");
                    mqtt_safe_publish(MQTT_TOPIC_STATUS, "Error: Sistema no esperaba configuración de fecha/hora", false);
                    break;
                }
                
                struct tm tm_fecha;
                memset(&tm_fecha, 0, sizeof(struct tm));
        
                if (sscanf(data, "%d-%d-%d %d:%d:%d",
                           &tm_fecha.tm_year, &tm_fecha.tm_mon, &tm_fecha.tm_mday,
                           &tm_fecha.tm_hour, &tm_fecha.tm_min, &tm_fecha.tm_sec) == 6) {
        
                    // Validar rangos de fecha/hora
                    if (tm_fecha.tm_year < 2020 || tm_fecha.tm_year > 2099 ||
                        tm_fecha.tm_mon < 1 || tm_fecha.tm_mon > 12 ||
                        tm_fecha.tm_mday < 1 || tm_fecha.tm_mday > 31 ||
                        tm_fecha.tm_hour < 0 || tm_fecha.tm_hour > 23 ||
                        tm_fecha.tm_min < 0 || tm_fecha.tm_min > 59 ||
                        tm_fecha.tm_sec < 0 || tm_fecha.tm_sec > 59) {
                        
                        ESP_LOGE(MQTT_TAG, "❌ Fecha/hora fuera de rango válido");
                        mqtt_safe_publish(MQTT_TOPIC_STATUS, "Error: Fecha/hora fuera de rango válido", false);
                        break;
                    }
                    
                    // Convertir al formato tm
                    tm_fecha.tm_year -= 1900;
                    tm_fecha.tm_mon -= 1;
        
                    if (rtc_set_time(&tm_fecha)) {
                        ESP_LOGI(MQTT_TAG, "Fecha/hora configurada: %04d-%02d-%02d %02d:%02d:%02d",
                                 tm_fecha.tm_year + 1900, tm_fecha.tm_mon + 1, tm_fecha.tm_mday,
                                 tm_fecha.tm_hour, tm_fecha.tm_min, tm_fecha.tm_sec);
                        
                        // Sincronizar el tiempo del sistema con el RTC recién configurado
                        time_t t = mktime(&tm_fecha);
                        struct timeval tv = { .tv_sec = t, .tv_usec = 0 };
                        settimeofday(&tv, NULL);
                        ESP_LOGI(MQTT_TAG, "🕒 Tiempo del sistema sincronizado con RTC");
                        
                        mqtt_safe_publish(MQTT_TOPIC_STATUS, "Fecha y hora guardadas en RTC correctamente", false);
                        mqtt_safe_publish(MQTT_TOPIC_STATUS, "Sistema operativo reanudado tras ajuste de fecha/hora", false);
                    } else {
                        ESP_LOGE(MQTT_TAG, "❌ Error al guardar fecha/hora en RTC");
                        mqtt_safe_publish(MQTT_TOPIC_STATUS, "Error al guardar fecha/hora en RTC", false);
                    }
        
                    sistema.estado.esperando_fecha_hora = false;
        
                } else {
                    ESP_LOGE(MQTT_TAG, "❌ Formato de fecha/hora inválido: '%s'", data);
                    mqtt_safe_publish(MQTT_TOPIC_STATUS, "Formato inválido. Use YYYY-MM-DD HH:MM:SS", false);
                }
                
            } else if (strcmp(topic, MQTT_TOPIC_SET_SCHEDULE) == 0) {
                ESP_LOGI(MQTT_TAG, "⏰ Procesando configuración de horario: %s", data);
                if (sistema.estado.esperando_comando_muestreo) {
                    // Esperando intervalo de muestreo
                    int nuevo_muestreo = atoi(data);
                    if (nuevo_muestreo >= 1000 && nuevo_muestreo <= 3600000) { // 1s a 1h
                        sistema.envio.muestreo_ms = nuevo_muestreo;
                        extern void guardar_muestreo_ms(void);
                        guardar_muestreo_ms();
                        char mensaje[MQTT_STATUS_BUFFER_SIZE];
                        snprintf(mensaje, sizeof(mensaje), "Intervalo de muestreo actualizado a %d ms", sistema.envio.muestreo_ms);
                        mqtt_safe_publish(MQTT_TOPIC_STATUS, mensaje, false);
                        mqtt_safe_publish(MQTT_TOPIC_STATUS, "Sistema operativo reanudado tras configuración de muestreo", false);
                        ESP_LOGI(MQTT_TAG, "✅ Intervalo de muestreo configurado exitosamente: %d ms", sistema.envio.muestreo_ms);
                        sistema.estado.esperando_comando_muestreo = false;
                    } else {
                        ESP_LOGE(MQTT_TAG, "❌ Valor de muestreo inválido: %s", data);
                        mqtt_safe_publish(MQTT_TOPIC_STATUS, "Error: Intervalo de muestreo inválido. Use un valor entre 1000 y 3600000 ms", false);
                    }
                    break;
                }
            } else {
                ESP_LOGW(MQTT_TAG, "⚠️ Topic no reconocido: '%s'", topic);
            }
                break;
            }
            
        case MQTT_EVENT_ERROR:
            ESP_LOGE(MQTT_TAG, "❌ Error crítico en MQTT - Code: %d", event->error_handle->error_type);
            if (event->error_handle->esp_transport_sock_errno != 0) {
                ESP_LOGE(MQTT_TAG, "❌ Error de socket: %d", event->error_handle->esp_transport_sock_errno);
            }
            mqtt_connected = false;
            mqtt_initialization_complete = false;
            break;
            
        case MQTT_EVENT_BEFORE_CONNECT:
            //ESP_LOGI(MQTT_TAG, "🔄 Intentando conectar al broker MQTT...");
            break;
            
        default:
            ESP_LOGD(MQTT_TAG, "📋 Evento MQTT no manejado: %d", event->event_id);
            break;
    }
    
    return ESP_OK;
}

void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    (void)handler_args;
    (void)base;
    (void)event_id;
    mqtt_event_handler_cb(event_data);
}

/**
 * @brief Inicializa el cliente MQTT con configuración segura TLS
 * Configura el cliente MQTT con certificado CA, credenciales y timeouts optimizados
 */
void mqtt_init(void) {
    ESP_LOGI(MQTT_TAG, "Inicializando cliente MQTT para %s:%d", CONFIG_BROKER_URL, CONFIG_BROKER_PORT);
    
    /*// Verificar que las constantes están definidas
    if (CONFIG_BROKER_URL == NULL || strlen(CONFIG_BROKER_URL) == 0) {
        ESP_LOGE(MQTT_TAG, "❌ URL del broker no configurada");
        return;
    }*/
    
    // Certificado CA del broker MQTT - ACTUALIZAR SEGÚN EL NUEVO BROKER
    // Si el broker usa Let's Encrypt, este certificado debería ser válido
    // Para otros brokers, obtener el certificado CA específico
    const char *broker_ca_cert = "-----BEGIN CERTIFICATE-----\n"
        "MIIFYDCCBEigAwIBAgIQQAF3ITcU1UKYzcohqYzS8TANBgkqhkiG9w0BAQsFADA/\n"
        "MSQwIgYDVQQKExtEaWdpdGFsIFNpZ25hdHVyZSBUcnVzdCBDby4xFzAVBgNVBAMT\n"
        "DkRTVCBSb290IENBIFgzMB4XDTIxMDEyMDE5MTQwM1oXDTI0MDkzMDE4MTQwM1ow\n"
        "TzELMAkGA1UEBhMCVVMxKTAnBgNVBAoTIEludGVybmV0IFNlY3VyaXR5IFJlc2Vh\n"
        "cmNoIEdyb3VwMRUwEwYDVQQDEwxJU1JHIFJvb3QgWDEwggIiMA0GCSqGSIb3DQEB\n"
        "AQUAA4ICDwAwggIKAoICAQCt6CRz9BQ385ueK1coHIe+3LffOJCMbjzmV6B493XC\n"
        "ov71am72AE8o295ohmxEk7axY/0UEmu/H9LqMZshftEzPLpI9d1537O4/xLxIZpL\n"
        "wYqGcWlKZmZsj348cL+tKSIG8+TA5oCu4kuPt5l+lAOf00eXfJlII1PoOK5PCm+D\n"
        "LtFJV4yAdLbaL9A4jXsDcCEbdfIwPPqPrt3aY6vrFk/CjhFLfs8L6P+1dy70sntK\n"
        "4EwSJQxwjQMpoOFTJOwT2e4ZvxCzSow/iaNhUd6shweU9GNx7C7ib1uYgeGJXDR5\n"
        "bHbvO5BieebbpJovJsXQEOEO3tkQjhb7t/eo98flAgeYjzYIlefiN5YNNnWe+w5y\n"
        "sR2bvAP5SQXYgd0FtCrWQemsAXaVCg/Y39W9Eh81LygXbNKYwagJZHduRze6zqxZ\n"
        "Xmidf3LWicUGQSk+WT7dJvUkyRGnWqNMQB9GoZm1pzpRboY7nn1ypxIFeFntPlF4\n"
        "FQsDj43QLwWyPntKHEtzBRL8xurgUBN8Q5N0s8p0544fAQjQMNRbcTa0B7rBMDBc\n"
        "SLeCO5imfWCKoqMpgsy6vYMEG6KDA0Gh1gXxG8K28Kh8hjtGqEgqiNx2mna/H2ql\n"
        "PRmP6zjzZN7IKw0KKP/32+IVQtQi0Cdd4Xn+GOdwiK1O5tmLOsbdJ1Fu/7xk9TND\n"
        "TwIDAQABo4IBRjCCAUIwDwYDVR0TAQH/BAUwAwEB/zAOBgNVHQ8BAf8EBAMCAQYw\n"
        "SwYIKwYBBQUHAQEEPzA9MDsGCCsGAQUFBzAChi9odHRwOi8vYXBwcy5pZGVudHJ1\n"
        "c3QuY29tL3Jvb3RzL2RzdHJvb3RjYXgzLnA3YzAfBgNVHSMEGDAWgBTEp7Gkeyxx\n"
        "+tvhS5B1/8QVYIWJEDBUBgNVHSAETTBLMAgGBmeBDAECATA/BgsrBgEEAYLfEwEB\n"
        "ATAwMC4GCCsGAQUFBwIBFiJodHRwOi8vY3BzLnJvb3QteDEubGV0c2VuY3J5cHQu\n"
        "b3JnMDwGA1UdHwQ1MDMwMaAvoC2GK2h0dHA6Ly9jcmwuaWRlbnRydXN0LmNvbS9E\n"
        "U1RST09UQ0FYM0NSTC5jcmwwHQYDVR0OBBYEFHm0WeZ7tuXkAXOACIjIGlj26Ztu\n"
        "MA0GCSqGSIb3DQEBCwUAA4IBAQAKcwBslm7/DlLQrg2Z9aP8yxLb2IqnGcvTN9Yf\n"
        "XENRzeoSTjnQbKn0tX3UHX6dv9I8Ao0VuQHjmM1iVb3fzI/5kAV5k8fiKZ2I6FRu\n"
        "fG/GRXJnn63Mda9mXixyX2ucK5bj6ZVD9/1pdKtWSL5QgmwiQ9JWTK6G5taI6HsR\n"
        "64QzXm9TZQ1o29krLcX4C3qhs6R9aB4U7QxP9N4Ake4Zos2Zkh8QKQm9hnSEk0iq\n"
        "hYi0d4l/sO3XJ2Tsn2l9xqjZ1Zf7UtPJ8LTSbFAXKdaELw2T00uXbWtMpMjZLW5Z\n"
        "p4tKb6h6cjEHsYIkgrJjPO8Yg1ObRwdm5pDUVVSmXrtLa3Cu/aIq3sLHKeQYru6w\n"
        "X2GS+s6yJvsADa8G7UQ0MqZFEM8vYa3w7s6f2P5tVJMs3MdjXf0VyIjHAAgMBAAG\n"
        "jggEZMIIBFTAdBgNVHQ4EFgQU7UQZwNPwBovupHu+QucmVMiONnYwHwYDVR0jBBgw\n"
        "FoAU7UQZwNPwBovupHu+QucmVMiONnYwDgYDVR0PAQH/BAQDAgEGMBIGA1UdEwEB\n"
        "/wQIMAYBAf8CAQAwNAYIKwYBBQUHAQEEKDAmMCQGCCsGAQUFBzABhhhodHRwOi8v\n"
        "b2NzcC5pZGVudHJ1c3QuY29tMDsGA1UdHwQ0MDIwMKAuoCyGKmh0dHA6Ly9jcmwu\n"
        "aWRlbnRydXN0LmNvbS9SU0EyMDY4ZWNhLWZyb20uY3JsMB0GA1UdDgQWBBTJlrx7\n"
        "kNqiyxT4o+hjgBJymb+hLDAOBgNVHQ8BAf8EBAMCAQYwDQYJKoZIhvcNAQELBQAD\n"
        "ggEBABiP+5e2dMG1v2eeIW8Gk2Gix5fzixj34LO5N2/lfvRneBZNe2m5S6V5avd/\n"
        "X4pMvPxr7lR9ThDmM1HapIxH/uYt4Ubv/OPszXlPY3rWii5k6W5wpff5hL2RHM4e\n"
        "Xb50t6Gz9iDXzZtOsa3Q9r9Z9w77l/wP0zlbUwUov80cOuZyIxFXgVPTzEhl6i6p\n"
        "p0qKVNQBLMqoPrOIDSIagAv8ZulgsrIHKXJ4GkP5yplmozWpL5DYLlHMtLNUfK9n\n"
        "c0txv7sGss9k5y/UDE8LozS6q6HNnNyeHziiQ0B3Z2eW9f65xD+3N5Cs1sV1J1iW\n"
        "1hQNeWHQ+aoejZ1jljh9upv32mKOGJWX1+ZZt8ttxdS5bR4U3jGR1Exn6GCVq8ND\n"
        "U5W+8z1z+9IBK6b1WNC0G6D3Y5GqY3iPpAGLHdt5k=\n"
        "-----END CERTIFICATE-----";
    
    // === CONFIGURACIÓN MQTT ===
    // Opción A: MQTTS (TLS) con verificación permisiva
    // Opción B: MQTT sin TLS (comentar/descomentar según necesidad)
    
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker = {
            // OPCIÓN A: MQTTS con TLS permisivo
            .address.uri = CONFIG_BROKER_URL,
            .address.port = CONFIG_BROKER_PORT,
            .verification.skip_cert_common_name_check = true,
            .verification.use_global_ca_store = false,
            .verification.certificate = broker_ca_cert,

        },
        .credentials = {
            .username = CONFIG_BROKER_USERNAME,
            .authentication.password = CONFIG_BROKER_PASSWORD,
        },
        .network = {
            .timeout_ms = 10000,
            .reconnect_timeout_ms = 5000,
            .disable_auto_reconnect = false
        }
    };
    
    // Inicializar cliente MQTT con configuración
    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    if (mqtt_client == NULL) {
        ESP_LOGE(MQTT_TAG, "❌ Error crítico: No se pudo inicializar cliente MQTT");
        return;
    }
    
    // Registrar manejador de eventos
    esp_err_t register_err = esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, mqtt_client);
    if (register_err != ESP_OK) {
        ESP_LOGE(MQTT_TAG, "Error al registrar eventos: %s", esp_err_to_name(register_err));
        return;
    }
    
    // NO iniciar cliente automáticamente - init diferido para optimizar recursos
    ESP_LOGI(MQTT_TAG, "✅ Cliente MQTT inicializado (inicio diferido)");

}

/**
 * @brief Envía datos de peso por MQTT con reconexión automática si es necesario
 * @param peso Valor del peso a enviar
 * @param timeinfo Estructura con información de tiempo
 * @param mensaje Mensaje adicional (puede ser NULL)
 * @return ESP_OK si se envió correctamente, error en caso contrario
 */
esp_err_t mqtt_enviar_datos(float peso, struct tm *timeinfo, const char* mensaje) {
    // Validación de parámetros
    if (timeinfo == NULL) {
        ESP_LOGE(MQTT_TAG, "❌ Parámetro timeinfo es NULL");
        return ESP_ERR_INVALID_ARG;
    }
    
    if (peso < -1000.0f || peso > 10000.0f) {
        ESP_LOGE(MQTT_TAG, "❌ Peso fuera de rango válido: %.2f", peso);
        return ESP_ERR_INVALID_ARG;
    }
    
    // Log optimizado eliminado
    
    if (!mqtt_validate_client()) {
        ESP_LOGW(MQTT_TAG, "🔄 Cliente MQTT no disponible, intentando reconectar...");
        
        // Intentar reconexión automática
        esp_mqtt_client_stop(mqtt_client);
        vTaskDelay(MQTT_WAIT_BEFORE_RECONNECT_MS / portTICK_PERIOD_MS);
        
        esp_err_t start_err = esp_mqtt_client_start(mqtt_client);
        if (start_err != ESP_OK) {
            ESP_LOGE(MQTT_TAG, "❌ Error al reiniciar cliente MQTT: %s", esp_err_to_name(start_err));
            return start_err;
        }
        
        // Esperar conexión con timeout
        int timeout = 0;
        int max_timeout = MQTT_RECONNECT_TIMEOUT_MS / 1000;
        while (!mqtt_connected && timeout < max_timeout) {
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            timeout++;
        }
        
        if (!mqtt_connected) {
            ESP_LOGE(MQTT_TAG, "❌ No se pudo reconectar después de %d segundos", timeout);
            return ESP_ERR_TIMEOUT;
        }
        
        ESP_LOGI(MQTT_TAG, "✅ Reconexión MQTT exitosa en %d segundos", timeout);
    }

    // Construir mensaje con validación de buffer
    char mensaje_completo[MQTT_MESSAGE_BUFFER_SIZE];
    int bytes_written;
    
    if (mensaje != NULL && strlen(mensaje) > 0) {
        bytes_written = snprintf(mensaje_completo, sizeof(mensaje_completo),
        "{\"timestamp\":\"%04d-%02d-%02dT%02d:%02d:%02d\",\"weight\":%.2f}",
        timeinfo->tm_year + 1900, timeinfo->tm_mon + 1, timeinfo->tm_mday,
        timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec,peso);
    } else {
        bytes_written = snprintf(mensaje_completo, sizeof(mensaje_completo), 
                                "Hora: %02d:%02d:%02d | Peso: %.2f kg",
                                timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec, peso);
    }
    
    // Verificar que el mensaje no se truncó
    if (bytes_written >= (int)sizeof(mensaje_completo)) {
        ESP_LOGW(MQTT_TAG, "⚠️ Mensaje MQTT truncado");
    }

    ESP_LOGI(MQTT_TAG, "📤 Enviando datos: %s", mensaje_completo);
    
    // Usar función de publicación segura
    esp_err_t result = mqtt_safe_publish(MQTT_TOPIC_WEIGHT_DATA, mensaje_completo, false);
    
    if (result == ESP_OK) {
        ESP_LOGI(MQTT_TAG, "✅ Datos enviados");
    } else {
        ESP_LOGE(MQTT_TAG, "❌ Error al enviar datos");
    }
    
    return result;
}

/**
 * @brief Verifica si el cliente MQTT está conectado y operativo
 * @return true si está conectado, false en caso contrario
 */
bool mqtt_is_connected(void) {

    if (!wifi_is_connected()) {
        return false;
    }
    
    bool connected = mqtt_connected && (mqtt_client != NULL) && mqtt_initialization_complete;
    return connected;
}

/**
 * @brief Realiza una prueba completa del sistema MQTT
 * @return ESP_OK si todas las pruebas pasan, error en caso contrario
 */
esp_err_t mqtt_test_connection(void) {
        ESP_LOGI(MQTT_TAG, "Probando conexión MQTT...");
    
    if (mqtt_client == NULL) {
        ESP_LOGE(MQTT_TAG, "❌ Cliente MQTT no inicializado");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (!mqtt_connected) {
        ESP_LOGW(MQTT_TAG, "⚠️ Cliente MQTT no conectado");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (!mqtt_initialization_complete) {
        ESP_LOGW(MQTT_TAG, "⚠️ Inicialización MQTT no completa");
        return ESP_ERR_INVALID_STATE;
    }
    
    // Enviar mensaje de prueba
    esp_err_t test_result = mqtt_safe_publish(MQTT_TOPIC_STATUS, "Test de conexión MQTT", false);
    // Publicar estado de conectado al validar prueba
    if (test_result == ESP_OK) {
        mqtt_safe_publish(MQTT_TOPIC_STATUS, "✅✅✅SISTEMA CONECTADO ✅✅✅", false);
        gpio_set_level(LED_USER, 0);
        mqtt_safe_publish(MQTT_TOPIC_CONECTION, "ON", false);
    }

    
    return test_result;
}

/**
 * @brief Procesa comandos recibidos por MQTT con validación robusta
 * @param comando String del comando a procesar
 */
void menu_mqtt(const char* comando) {

    int comando_num = atoi(comando);
    ESP_LOGI(MQTT_TAG, "🎯 Procesando comando %d: '%s'", comando_num, comando);
    
    switch (comando_num) {
        case 0:
            ESP_LOGW(MQTT_TAG, "🔄 Reiniciando sistema por comando remoto...");
            esp_mqtt_client_publish(mqtt_client, "esp32/halo/conection", "OFF", 0, 1, 0);
            mqtt_safe_publish(MQTT_TOPIC_STATUS, MQTT_MSG_REINICIANDO, false);
            
            // Esperar para que el mensaje se envíe
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            esp_restart();
            // Nota: El código después de esp_restart() nunca se ejecuta pero se deja por completitud
            break;

        
        case 1:
            ESP_LOGI(MQTT_TAG, "🔧 Iniciando proceso de calibración...");
            mqtt_safe_publish(MQTT_TOPIC_STATUS, MQTT_MSG_CALIBRACION_INICIADA, false);
            
            sistema.estado.sistema_calibrado = true;
            esp_mqtt_client_publish(mqtt_client, "esp32/halo/status", "CALIBRANDO...", 0, 1, 0);
            
            break;

        case 6:
            ESP_LOGI(MQTT_TAG, "📅 Esperando configuración de fecha y hora...");
            sistema.estado.esperando_fecha_hora = true;
            mqtt_safe_publish(MQTT_TOPIC_STATUS, MQTT_MSG_ESPERANDO_FECHA, false);
            ESP_LOGI(MQTT_TAG, "✅ Sistema en modo espera de fecha/hora");
            break;
            
        case 7:
            ESP_LOGI(MQTT_TAG, "🔄 Reseteando registro de envío diario...");
            sistema.envio.ultimo_dia_envio = -1;
            mqtt_safe_publish(MQTT_TOPIC_STATUS, MQTT_MSG_REGISTRO_RESETEADO, false);
            ESP_LOGI(MQTT_TAG, "✅ Registro de envío diario reseteado");
            esp_mqtt_client_publish(mqtt_client, "esp32/halo/conection", "ON", 0, 1, 0);
            esp_mqtt_client_publish(mqtt_client, "esp32/halo/status", "✅✅✅SISTEMA CONECTADO ✅✅✅", 0, 1, 0);
            gpio_set_level(LED_USER, 0);
            break;
            
        case 8:
            hx711_continuar_calibracion_peso();
            sistema.estado.esperando_comando_peso = false;
            break;
            
        case 9:
            sistema.estado.esperando_config_horario = true;
            mqtt_safe_publish(MQTT_TOPIC_STATUS, MQTT_MSG_ESPERANDO_HORARIO, false);
            ESP_LOGI(MQTT_TAG, "✅ Sistema esperando configuración de horario");
            break;
            
        case 2:
            ESP_LOGI(MQTT_TAG, "⏱️ Esperando configuración de intervalo de muestreo...");
            sistema.estado.esperando_comando_muestreo = true;
            mqtt_safe_publish(MQTT_TOPIC_STATUS, "Envíe el intervalo de muestreo en milisegundos al topic esp32/set_schedule", false);
            ESP_LOGI(MQTT_TAG, "✅ Sistema en modo espera de intervalo de muestreo");
            break;

        case 99:
            ESP_LOGI(MQTT_TAG, "🚀 Procesando comando OTA con URL: %s", comando);
            // El comando 99 debe incluir la URL del binario OTA
            // Formato esperado: "99 https://url_del_binario.bin"
            ota_process_command(comando);
            break;

        default:
            ESP_LOGW(MQTT_TAG, "❓ Comando no reconocido: %d ('%s')", comando_num, comando);
            
            char mensaje_error[MQTT_STATUS_BUFFER_SIZE];
            snprintf(mensaje_error, sizeof(mensaje_error),
                     "Error: Comando %d no reconocido. Comandos válidos: 0,1,2,6,7,8,9,99",
                     comando_num);
            mqtt_safe_publish(MQTT_TOPIC_STATUS, mensaje_error, false);
            break;
    }
    
    ESP_LOGI(MQTT_TAG, "✅ Comando %d procesado exitosamente", comando_num);
} 