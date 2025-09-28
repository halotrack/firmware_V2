#include "../include/hx711_lib.h"
#include "../include/HALO.h"
#include <esp_log.h>
#include <driver/gpio.h>
#include <rom/ets_sys.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <stdio.h>
#include <time.h>
#include <inttypes.h>
#include "nvs_flash.h"
#include "nvs.h"
#include "../include/mqtt_lib.h"
#include <math.h>
#include <string.h>

static const char *HX711_TAG = "HX711_LIB";

// Variables globales del HX711
hx711_handle_t hx711;
int32_t offset = 0;
float scale = 1000.0f;

// Claves NVS para calibración
#define NVS_NAMESPACE "hx711_cal"
#define NVS_KEY_OFFSET "offset"
#define NVS_KEY_SCALE "scale"
#define NVS_KEY_CALIBRATED "calibrated"

// ------------ HX711 funciones de bajo nivel -------------
static uint8_t hx711_bus_init(void) { 
    gpio_set_direction(HX711_DOUT, GPIO_MODE_INPUT); 
    return 0; 
}

static uint8_t hx711_bus_deinit(void) { 
    return 0; 
}

static uint8_t hx711_bus_read(uint8_t *value) { 
    *value = gpio_get_level(HX711_DOUT); 
    return 0; 
}

static uint8_t hx711_clock_init(void) { 
    gpio_set_direction(HX711_SCK, GPIO_MODE_OUTPUT); 
    gpio_set_level(HX711_SCK, 0); 
    return 0; 
}

static uint8_t hx711_clock_deinit(void) { 
    return 0; 
}

static uint8_t hx711_clock_write(uint8_t val) { 
    gpio_set_level(HX711_SCK, val); 
    return 0; 
}

static void hx711_delay_us(uint32_t us) { 
    ets_delay_us(us); 
}

static void hx711_enable_irq(void) { 
    taskENABLE_INTERRUPTS(); 
}

static void hx711_disable_irq(void) { 
    taskDISABLE_INTERRUPTS(); 
}

static void hx711_debug_print(const char *const fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
}

/**
 * @brief Guarda los valores de calibración en NVS
 * 
 * @return ESP_OK si se guardó correctamente, ESP_FAIL en caso contrario
 */
esp_err_t hx711_guardar_calibracion(void) {
    nvs_handle_t nvs_handle;
    esp_err_t err;
    
    // Abrir namespace NVS
    err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(HX711_TAG, "Error al abrir NVS: %s", esp_err_to_name(err));
        return ESP_FAIL;
    }
    
    // Guardar offset
    err = nvs_set_i32(nvs_handle, NVS_KEY_OFFSET, offset);
    if (err != ESP_OK) {
        ESP_LOGE(HX711_TAG, "Error al guardar offset: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return ESP_FAIL;
    }
    
    // Guardar scale
    err = nvs_set_blob(nvs_handle, NVS_KEY_SCALE, &scale, sizeof(float));
    if (err != ESP_OK) {
        ESP_LOGE(HX711_TAG, "Error al guardar scale: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return ESP_FAIL;
    }
    
    // Marcar como calibrado
    err = nvs_set_u8(nvs_handle, NVS_KEY_CALIBRATED, 1);
    if (err != ESP_OK) {
        ESP_LOGE(HX711_TAG, "Error al marcar como calibrado: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return ESP_FAIL;
    }
    
    // Commit los cambios
    err = nvs_commit(nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(HX711_TAG, "Error al hacer commit: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return ESP_FAIL;
    }
    
    nvs_close(nvs_handle);
    ESP_LOGI(HX711_TAG, "Calibración guardada en NVS - Offset: %d, Scale: %.2f", (int)offset, scale);
    return ESP_OK;
}

/**
 * @brief Carga los valores de calibración desde NVS
 * 
 * @return ESP_OK si se cargó correctamente, ESP_FAIL si no hay calibración guardada
 */
esp_err_t hx711_cargar_calibracion(void) {
    nvs_handle_t nvs_handle;
    esp_err_t err;
    
    // Abrir namespace NVS
    err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(HX711_TAG, "Error al abrir NVS: %s", esp_err_to_name(err));
        return ESP_FAIL;
    }
    
    // Verificar si está calibrado
    uint8_t calibrated = 0;
    err = nvs_get_u8(nvs_handle, NVS_KEY_CALIBRATED, &calibrated);
    if (err != ESP_OK || calibrated == 0) {
        ESP_LOGW(HX711_TAG, "No hay calibración guardada en NVS");
        nvs_close(nvs_handle);
        return ESP_FAIL;
    }
    
    // Cargar offset
    err = nvs_get_i32(nvs_handle, NVS_KEY_OFFSET, &offset);
    if (err != ESP_OK) {
        ESP_LOGE(HX711_TAG, "Error al cargar offset: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return ESP_FAIL;
    }
    
    // Cargar scale
    size_t scale_size = sizeof(float);
    err = nvs_get_blob(nvs_handle, NVS_KEY_SCALE, &scale, &scale_size);
    if (err != ESP_OK) {
        ESP_LOGE(HX711_TAG, "Error al cargar scale: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return ESP_FAIL;
    }
    
    nvs_close(nvs_handle);
    ESP_LOGI(HX711_TAG, "Calibración cargada desde NVS - Offset: %d, Scale: %.2f", (int)offset, scale);
    return ESP_OK;
}

/**
 * @return 1 si hay calibración guardada, 0 en caso contrario
 */
int hx711_tiene_calibracion_guardada(void) {
    nvs_handle_t nvs_handle;
    esp_err_t err;
    
    err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        return false;
    }
    
    uint8_t calibrated = 0;
    err = nvs_get_u8(nvs_handle, NVS_KEY_CALIBRATED, &calibrated);
    nvs_close(nvs_handle);
    
    return (err == ESP_OK && calibrated == 1);
}

// --- INICIO: Funciones fusionadas de driver_hx711.c ---
#define CHIP_NAME                 "Aviaic HX711"
#define MANUFACTURER_NAME         "Aviaic"
#define SUPPLY_VOLTAGE_MIN        2.6f
#define SUPPLY_VOLTAGE_MAX        5.5f
#define MAX_CURRENT               1.5f
#define TEMPERATURE_MIN           -40.0f
#define TEMPERATURE_MAX           85.0f
#define DRIVER_VERSION            2000

static uint8_t a_hx711_read_ad(hx711_handle_t *handle, uint8_t len, int32_t *value)
{
    uint32_t val = 0;
    uint32_t cnt = 0;
    uint8_t i;
    uint8_t v;
    if (handle->clock_write(0) != 0) {
        handle->debug_print("hx711: clock write 0 failed.\n");
        return 1;
    }
    while (1) {
        handle->delay_us(100);
        if (handle->bus_read((uint8_t *)&v) != 0) {
            handle->debug_print("hx711: bus read failed.\n");
            return 1;
        }
        cnt++;
        if (v == 1) {
            if (cnt >= 50000) {
                handle->debug_print("hx711: bus no response.\n");
                return 1;
            }
        } else {
            break;
        }
    }
    handle->disable_irq();
    handle->delay_us(1);
    for (i = 0; i < 24; i++) {
        if (handle->clock_write(1) != 0) {
            handle->enable_irq();
            handle->debug_print("hx711: clock write 1 failed.\n");
            return 1;
        }
        val = val << 1;
        handle->delay_us(1);
        if (handle->clock_write(0) != 0) {
            handle->enable_irq();
            handle->debug_print("hx711: clock write 0 failed.\n");
            return 1;
        }
        if (handle->bus_read((uint8_t *)&v) != 0) {
            handle->enable_irq();
            handle->debug_print("hx711: bus read failed.\n");
            return 1;
        }
        if (v != 0) {
            val++;
        }
        handle->delay_us(1);
    }
    while (len != 0) {
        if (handle->clock_write(1) != 0) {
            handle->enable_irq();
            handle->debug_print("hx711: clock write 1 failed.\n");
            return 1;
        }
        handle->delay_us(1);
        if (handle->clock_write(0) != 0) {
            handle->enable_irq();
            handle->debug_print("hx711: clock write 0 failed.\n");
            return 1;
        }
        handle->delay_us(1);
        len--;
    }
    handle->enable_irq();
    if ((val & 0x800000) != 0) {
        union {
            int32_t i_f;
            uint32_t u_f;
        } u;
        val = 0xFF000000U | val;
        u.u_f = val;
        *value = (int32_t)u.i_f;
    } else {
        *value = (int32_t)val;
    }
    return 0;
}

uint8_t hx711_init(hx711_handle_t *handle)
{
    if (handle == NULL) return 2;
    if (handle->debug_print == NULL) return 3;
    if (handle->bus_init == NULL) { handle->debug_print("hx711: bus_init is null.\n"); return 3; }
    if (handle->bus_deinit == NULL) { handle->debug_print("hx711: bus_deinit is null.\n"); return 3; }
    if (handle->bus_read == NULL) { handle->debug_print("hx711: bus_read is null.\n"); return 3; }
    if (handle->clock_init == NULL) { handle->debug_print("hx711: clock_init is null.\n"); return 3; }
    if (handle->clock_deinit == NULL) { handle->debug_print("hx711: clock_deinit is null.\n"); return 3; }
    if (handle->clock_write == NULL) { handle->debug_print("hx711: clock_write is null.\n"); return 3; }
    if (handle->delay_us == NULL) { handle->debug_print("hx711: delay_us is null.\n"); return 3; }
    if (handle->enable_irq == NULL) { handle->debug_print("hx711: enable_irq is null.\n"); return 3; }
    if (handle->disable_irq == NULL) { handle->debug_print("hx711: disable_irq is null.\n"); return 3; }
    if (handle->clock_init() != 0) { handle->debug_print("hx711: clock init failed.\n"); return 1; }
    if (handle->bus_init() != 0) { handle->debug_print("hx711: bus init failed.\n"); (void)handle->clock_deinit(); return 1; }
    handle->inited = 1;
    handle->mode = 1;
    return 0;
}

uint8_t hx711_deinit(hx711_handle_t *handle)
{
    if (handle == NULL) return 2;
    if (handle->inited != 1) return 3;
    if (handle->bus_deinit() != 0) { handle->debug_print("hx711: bus deinit failed.\n"); return 1; }
    if (handle->clock_deinit() != 0) { handle->debug_print("hx711: clock deinit failed.\n"); return 1; }
    handle->inited = 0;
    return 0;
}

uint8_t hx711_set_mode(hx711_handle_t *handle, hx711_mode_t mode)
{
    int32_t value;
    if (handle == NULL) return 2;
    if (handle->inited != 1) return 3;
    handle->mode = (uint8_t)mode;
    if (a_hx711_read_ad(handle, handle->mode, (int32_t *)&value) != 0) {
        handle->debug_print("hx711: read ad failed.\n");
        return 1;
    }
    return 0;
}

uint8_t hx711_get_mode(hx711_handle_t *handle, hx711_mode_t *mode)
{
    if (handle == NULL) return 2;
    if (handle->inited != 1) return 3;
    *mode = (hx711_mode_t)(handle->mode);
    return 0;
}

uint8_t hx711_read(hx711_handle_t *handle, int32_t *raw, double *voltage_v)
{
    if (handle == NULL) return 2;
    if (handle->inited != 1) return 3;
    if (a_hx711_read_ad(handle, handle->mode, (int32_t *)raw) != 0) {
        handle->debug_print("hx711: read voltage failed.\n");
        return 1;
    }
    if (handle->mode == (uint8_t)HX711_MODE_CHANNEL_A_GAIN_128) {
        *voltage_v = (double)(*raw) * (20.0 / (pow(2.0, 24.0))) / 1000.0;
        return 0;
    } else if (handle->mode == (uint8_t)HX711_MODE_CHANNEL_B_GAIN_32) {
        *voltage_v = (double)(*raw) * (80.0 / (pow(2.0, 24.0))) / 1000.0;
        return 0;
    } else if (handle->mode == (uint8_t)HX711_MODE_CHANNEL_A_GAIN_64) {
        *voltage_v = (double)(*raw) * (40.0 / (pow(2.0, 24.0))) / 1000.0;
        return 0;
    } else {
        handle->debug_print("hx711: mode error.\n");
        return 4;
    }
}

uint8_t hx711_info(hx711_info_t *info)
{
    if (info == NULL) return 2;
    memset(info, 0, sizeof(hx711_info_t));
    strncpy(info->chip_name, CHIP_NAME, 32);
    strncpy(info->manufacturer_name, MANUFACTURER_NAME, 32);
    strncpy(info->interface, "GPIO", 8);
    info->supply_voltage_min_v = SUPPLY_VOLTAGE_MIN;
    info->supply_voltage_max_v = SUPPLY_VOLTAGE_MAX;
    info->max_current_ma = MAX_CURRENT;
    info->temperature_max = TEMPERATURE_MAX;
    info->temperature_min = TEMPERATURE_MIN;
    info->driver_version = DRIVER_VERSION;
    return 0;
}
// --- FIN: Funciones fusionadas de driver_hx711.c ---

void init_HX711(void) {
    // Inicializar la estructura del handle
    DRIVER_HX711_LINK_INIT(&hx711, hx711_handle_t);
    
    // Configurar las funciones de bajo nivel
    DRIVER_HX711_LINK_BUS_INIT(&hx711, hx711_bus_init);
    DRIVER_HX711_LINK_BUS_DEINIT(&hx711, hx711_bus_deinit);
    DRIVER_HX711_LINK_BUS_READ(&hx711, hx711_bus_read);
    DRIVER_HX711_LINK_CLOCK_INIT(&hx711, hx711_clock_init);
    DRIVER_HX711_LINK_CLOCK_DEINIT(&hx711, hx711_clock_deinit);
    DRIVER_HX711_LINK_CLOCK_WRITE(&hx711, hx711_clock_write);
    DRIVER_HX711_LINK_DELAY_US(&hx711, hx711_delay_us);
    DRIVER_HX711_LINK_ENABLE_IRQ(&hx711, hx711_enable_irq);
    DRIVER_HX711_LINK_DISABLE_IRQ(&hx711, hx711_disable_irq);
    DRIVER_HX711_LINK_DEBUG_PRINT(&hx711, hx711_debug_print);
    
    // Inicializar el chip
    if (hx711_init(&hx711) == 0) {
        // Configurar modo
        hx711_set_mode(&hx711, HX711_MODE_CHANNEL_A_GAIN_128);
        ESP_LOGI(HX711_TAG, "HX711 inicializado correctamente");
        
        // Intentar cargar calibración guardada
        if (hx711_cargar_calibracion() == ESP_OK) {
            ESP_LOGI(HX711_TAG, "Calibración cargada exitosamente desde NVS");
        } else {
            ESP_LOGW(HX711_TAG, "No se encontró calibración guardada, usando valores por defecto");
            ESP_LOGI(HX711_TAG, "Valores por defecto - Offset: %d, Scale: %.2f", (int)offset, scale);
        }
    } else {
        ESP_LOGE(HX711_TAG, "Error al inicializar HX711");
    }
}

float hx711_leer_peso(void) {
    int32_t raw_value;
    double voltage_v;
    float peso_kg;
    
    if (hx711_read(&hx711, &raw_value, &voltage_v) == 0) {
        peso_kg = (raw_value - offset) / scale;
        return peso_kg;
    } else {
        ESP_LOGE(HX711_TAG, "Error al leer datos del HX711");
        return -999.0f; // Valor de error
    }
}

void hx711_calibrar_inicial(void) {
    ESP_LOGI(HX711_TAG, "Iniciando calibración HX711...");
    

    // Tomar múltiples lecturas para el offset
    int32_t sum = 0;
    const int num_readings = 10;
    
    ESP_LOGI(HX711_TAG, "Tomando %d lecturas para calcular offset...", num_readings);
    

    
    for (int i = 0; i < num_readings; i++) {
        int32_t raw_value;
        double voltage_v;
        if (hx711_read(&hx711, &raw_value, &voltage_v) == 0) {
            sum += raw_value;
            ESP_LOGI(HX711_TAG, "Lectura %d: %d", i + 1, (int)raw_value);
        }
        vTaskDelay(500 / portTICK_PERIOD_MS);
    }
    
    offset = sum / num_readings;
    ESP_LOGI(HX711_TAG, "Offset calculado: %d", (int)offset);
    esp_mqtt_client_publish(mqtt_client, "esp32/halo/conection", "ON", 0, 1, 0);
    
    // Enviar confirmación de offset calculado
    if (mqtt_is_connected()) {
        char mensaje[256];
        snprintf(mensaje, sizeof(mensaje), "Offset calculado exitosamente: %d", (int)offset);
        esp_mqtt_client_publish(mqtt_client, "esp32/halo/status", mensaje, 0, 1, 0);
    }
    

    // Enviar instrucción para esperar comando del servidor
    if (mqtt_is_connected()) {
        esp_mqtt_client_publish(mqtt_client, "esp32/halo/conection", "ON1", 0, 1, 0);
    }
    
    // La función termina aquí y espera el comando "8"
    // El resto de la calibración se ejecutará cuando se reciba el comando
}

void hx711_continuar_calibracion_peso(void) {
    ESP_LOGI(HX711_TAG, "PASO 2: Continuando calibración con peso ");
    

    // Calcular escala con peso conocido
    int32_t sum = 0;
    const int num_readings = 10;
    ESP_LOGI(HX711_TAG, "Tomando lecturas ...");
    
    
    for (int i = 0; i < num_readings; i++) {
        int32_t raw_value;
        double voltage_v;
        if (hx711_read(&hx711, &raw_value, &voltage_v) == 0) {
            sum += raw_value;
            ESP_LOGI(HX711_TAG, "Lectura con peso %d: %d", i + 1, (int)raw_value);
        }
        vTaskDelay(500 / portTICK_PERIOD_MS);
    }
    
    int32_t raw_with_weight = sum / num_readings;
    scale = (float)(raw_with_weight - offset) / 1.0f; // 1 kg conocido
    
    

    
    // Guardar calibración en NVS
    if (hx711_guardar_calibracion() == ESP_OK) {
        // Enviar confirmación de guardado exitoso
        if (mqtt_is_connected()) {
            char mensaje[256];
            snprintf(mensaje, sizeof(mensaje), "Calibración guardada exitosamente en memoria");
            esp_mqtt_client_publish(mqtt_client, "esp32/halo/status", mensaje, 0, 1, 0);
        }
    } else {
        ESP_LOGE(HX711_TAG, "Error al guardar calibración en NVS");
    }
    
    
    if (mqtt_is_connected()) {
        esp_mqtt_client_publish(mqtt_client, "esp32/halo/conection", "ON2", 0, 1, 0);
    }
    

}

