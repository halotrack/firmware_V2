#ifndef HALO_H
#define HALO_H

// === SISTEMA ===
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <inttypes.h>
#include <time.h>
#include <sys/time.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include "esp_log.h"
// === ESP-IDF ===
#include "esp_system.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_vfs_fat.h"
#include "nvs_flash.h"
#include "esp_smartconfig.h"
#include "esp_app_desc.h"
// === DRIVERS ===
#include "driver/gpio.h"
#include "driver/uart.h"
#include "i2cdev.h"
// #include "ds3231.h"
#include "sdmmc_cmd.h"
// ===  FreeRTOS ===
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
// ===  MQTT ===
#include "mqtt_client.h"
// === PROPIAS ===
#include "rtc_lib.h"
#include "hx711_lib.h"
#include "mqtt_lib.h"
#include "wifi_lib.h"
#include "sdcard.h"
#include "button_actions.h"
#include "conexion.h"
#include "smartconfig.h"
#include "battery.h"
#include "bq27427.h"
#include "task.h"
#include "version.h"
#include "ota_lib.h"

// === HARDWARE ===
#define USER_BUTTON      25     
#define LED_USER         0   
#define HX711_DOUT       17      
#define HX711_SCK        16      
#define I2C_SDA          21     
#define I2C_SCL          22              

// === ALMACENAMIENTO NVS ===
#define NVS_NAMESPACE            "halo"              // Namespace principal del sistema
#define NVS_KEY_ULTIMA           "ultima_muestra"    // Última muestra enviada
#define NVS_KEY_HORA_ENVIO       "hora_envio"        // Hora de envío programada
#define NVS_KEY_MINUTO_ENVIO     "minuto_envio"      // Minuto de envío programado
#define NVS_KEY_WIFI_SSID        "wifi_ssid"         // (LEGADO) SSID de WiFi guardado
#define NVS_KEY_WIFI_PASS        "wifi_pass"         // (LEGADO) Contraseña de WiFi guardada
#define NVS_KEY_WIFI_SSID_0      "wifi_ssid_0"       // SSID slot 0
#define NVS_KEY_WIFI_PASS_0      "wifi_pass_0"       // PASS slot 0
#define NVS_KEY_WIFI_SSID_1      "wifi_ssid_1"       // SSID slot 1
#define NVS_KEY_WIFI_PASS_1      "wifi_pass_1"       // PASS slot 1
#define NVS_KEY_WIFI_SSID_2      "wifi_ssid_2"       // SSID slot 2
#define NVS_KEY_WIFI_PASS_2      "wifi_pass_2"       // PASS slot 2
#define NVS_MAX_WIFI_CREDENTIALS 3
#define NVS_KEY_MUESTREO_MS       "muestreo_ms"      // Intervalo de muestreo en ms

// === RED ===
#define EXAMPLE_ESP_MAXIMUM_RETRY    5                   // Máximo número de intentos de conexión
#define WIFI_CONNECTED_BIT           BIT0                // Bit para indicar conexión WiFi exitosa
#define WIFI_FAIL_BIT                BIT1                // Bit para indicar fallo de conexión WiFi

// === MQTT ===
#define CONFIG_BROKER_URL            "mqtts://mqtthalotrack.ddns.net"
#define CONFIG_BROKER_PORT           8883                // Puerto
#define CONFIG_BROKER_USERNAME       "halomqttuser"         // Usuario 
#define CONFIG_BROKER_PASSWORD       "GJZ9K%tD73yF"        // Contraseña 

// === TIMEOUT Y LÍMITES ===
#define WIFI_TIMEOUT_SECONDS        30                  // Timeout para conexión WiFi
#define MQTT_TIMEOUT_SECONDS        30                  // Timeout para conexión MQTT
#define HX711_ERROR_THRESHOLD       -900.0f             // Umbral de error para el sensor HX711
#define SMARTCONFIG_TIMEOUT_SECONDS 120                 // Timeout para SmartConfig (2 minutos)
#define SMARTCONFIG_BUTTON_TIMEOUT  1500                // Tiempo para detectar botón presionado (1.5 segundos)
#define SISTEMA_TIMEOUT_MUTEX       1000            // Timeout para mutex (ms)

// === CONFIGURACIÓN DE LOGGING ===
#define SISTEMA_LOG_LEVEL_ERROR     0                   // Solo errores críticos
#define SISTEMA_LOG_LEVEL_WARN      1                   // Errores y advertencias
#define SISTEMA_LOG_LEVEL_INFO      2                   // Información general
#define SISTEMA_LOG_LEVEL_DEBUG     3                   // Información de debug
#define SISTEMA_LOG_LEVEL_VERBOSE   4                   // Información muy detallada

// === CONFIGURACIONES OPTIMIZADAS DE TAREAS FreeRTOS ===
#define TAREA_HX711_STACK_SIZE   3072  // solo lectura de sensor
#define TAREA_MQTT_STACK_SIZE    6144  // SSL/TLS requiere más memoria
#define TAREA_BUTTON_STACK_SIZE  2048  //  lógica mínima

// === PRIORIDADES DE TAREAS ===
#define TAREA_HX711_PRIORIDAD    6     // ALTA: sensor crítico del sistema
#define TAREA_MQTT_PRIORIDAD     4     // MEDIA: red no crítica  
#define TAREA_BUTTON_PRIORIDAD   8     // MUY ALTA: responsividad del usuario

// === DISTRIBUCIÓN POR NÚCLEOS ===
#define NUCLEO_PROTOCOLO          0     // Núcleo dedicado a protocolos (HX711)
#define NUCLEO_APLICACION         1     // Núcleo para aplicación y red (MQTT, Botón)

// === CONFIGURACIÓN DEL SISTEMA ===
bool hay_datos_pendientes_envio(void); 
esp_err_t sistema_init_config(void); 

// === FUNCIONES DE SMARTCONFIG ===
bool smartconfig_is_active(void);
bool smartconfig_has_saved_credentials(void);
esp_err_t smartconfig_init(void);
esp_err_t smartconfig_stop(void);
esp_err_t smartconfig_start(void);
esp_err_t smartconfig_clear_credentials(void);
esp_err_t smartconfig_save_credentials(const char *ssid, const char *password);
esp_err_t smartconfig_load_credentials(char *ssid, char *password, size_t max_len);
// Manejo de múltiples credenciales
size_t smartconfig_get_saved_credentials_count(void);
esp_err_t smartconfig_load_credentials_index(int index, char *ssid, char *password, size_t max_len);


void inicializar_sistema(void);  
void diagnostico_sistema(void); 
                   

void enviar_mac(void);  
void user_button_init(void);                            
void guardar_horario_envio(void);                                        
void restaurar_horario_envio(void);   
void guardar_ultima_muestra_enviada(void);  
void restaurar_ultima_muestra_enviada(void); 
void guardar_muestreo_ms(void);
void restaurar_muestreo_ms(void);

// === ESTRUCTURAS DE CONFIGURACIÓN DEL SISTEMA ===
typedef struct {
    // Configuración de envío MQTT y muestreo
    struct {
        int ultima_muestra_enviada;     // Índice de última línea enviada del CSV
        int ultimo_dia_envio;           // Día del último envío (-1 = forzar envío)
        int hora_envio;                 // Hora programada para envío diario
        int minuto_envio;               // Minuto programado para envío diario
        int muestreo_ms;                // Intervalo entre lecturas del sensor (ms)
    } envio;
    
    // Estado del sistema y banderas de control
    struct {
        bool sistema_inicializado;      // Sistema completamente inicializado
        bool esperando_comando;         // Esperando comando MQTT genérico
        bool esperando_comando_muestreo;         // Esperando comando MQTT genérico
        bool sistema_calibrado;         // Comando de calibración recibido
        bool calibracion_completada;    // Offset de calibración completado
        bool esperando_comando_peso;    // Esperando peso de calibración
        bool esperando_fecha_hora;      // Esperando fecha/hora por MQTT
        bool esperando_config_horario;  // Esperando configuración de horario
        bool conexion_boton_activa;     // Estado de conexión manual por botón
    } estado;
    
    // Sincronización y protección de datos
    SemaphoreHandle_t mutex_config;     // Protege acceso concurrente a esta estructura
    SemaphoreHandle_t mutex_sd;         // Protege acceso a la tarjeta SD
    
} sistema_config_t;

extern sistema_config_t sistema;


typedef struct {
    float offset;                       // Offset de calibración
    float scale;                        // Factor de escala
    float peso_conocido;                // Peso conocido usado en calibración
    uint32_t timestamp_calibracion;     // Timestamp de la calibración
    bool calibracion_valida;            // Indica si la calibración es válida
} sistema_calibracion_t;


// === MACROS DE MUTEX ===
#define SISTEMA_TAKE_MUTEX(mutex) do { \
    if (xSemaphoreTake((mutex), pdMS_TO_TICKS(SISTEMA_TIMEOUT_MUTEX)) != pdTRUE) { \
        ESP_LOGE(TAG, "Timeout al tomar mutex en %s:%d", __FILE__, __LINE__); \
        return ESP_ERR_TIMEOUT; \
    } \
} while(0)

#define SISTEMA_GIVE_MUTEX(mutex) do { \
    xSemaphoreGive((mutex)); \
} while(0)

// Macro para esperar condición con timeout
#define WAIT_UNTIL(cond, timeout_var, timeout_limit) \
    for ((timeout_var) = 0; !(cond) && (timeout_var) < (timeout_limit); (timeout_var)++) \
        vTaskDelay(1000 / portTICK_PERIOD_MS);

#endif // HALO_H    