#include "../include/rtc_lib.h"


// Helpers y defines mínimos:
static uint8_t bcd2dec(uint8_t val) { return (val >> 4) * 10 + (val & 0x0f); }
static uint8_t dec2bcd(uint8_t val) { return ((val / 10) << 4) + (val % 10); }
#define DS3231_ADDR 0x68
#define DS3231_ADDR_TIME    0x00
#define DS3231_ADDR_STATUS  0x0f
#define DS3231_STAT_OSCILLATOR 0x80
// Las constantes ESP_OK y ESP_ERR_* se incluyen desde esp_err.h

esp_err_t ds3231_init_desc(i2c_dev_t *dev, i2c_port_t port, gpio_num_t sda_gpio, gpio_num_t scl_gpio) {
    if (!dev) return ESP_ERR_INVALID_ARG;
    dev->port = port;
    dev->addr = DS3231_ADDR;
    dev->cfg.sda_io_num = sda_gpio;
    dev->cfg.scl_io_num = scl_gpio;
    dev->cfg.master.clk_speed = 400000;
    return ESP_OK;
}

esp_err_t ds3231_set_time(i2c_dev_t *dev, struct tm *time) {
    if (!dev || !time) return ESP_ERR_INVALID_ARG;
    
    uint8_t data[7];
    data[0] = dec2bcd(time->tm_sec);
    data[1] = dec2bcd(time->tm_min);
    data[2] = dec2bcd(time->tm_hour);
    data[3] = dec2bcd(time->tm_wday + 1);
    data[4] = dec2bcd(time->tm_mday);
    data[5] = dec2bcd(time->tm_mon + 1);
    data[6] = dec2bcd(time->tm_year - 100);
    
    ESP_LOGI("RTC_LIB", "🕒 Escribiendo al RTC: %04d-%02d-%02d %02d:%02d:%02d", 
             time->tm_year + 1900, time->tm_mon + 1, time->tm_mday,
             time->tm_hour, time->tm_min, time->tm_sec);
    
    // Escribir los datos al registro de tiempo del DS3231
    esp_err_t ret = i2c_dev_write_reg(dev, DS3231_ADDR_TIME, data, 7);
    
    if (ret != ESP_OK) {
        ESP_LOGE("RTC_LIB", "❌ Error al escribir al RTC DS3231: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI("RTC_LIB", "✅ Tiempo escrito correctamente al RTC DS3231");
    return ESP_OK;
}

esp_err_t ds3231_get_time(i2c_dev_t *dev, struct tm *time) {
    if (!dev || !time) return ESP_ERR_INVALID_ARG;
    uint8_t data[7] = {0};
    esp_err_t res = i2c_dev_read_reg(dev, DS3231_ADDR_TIME, data, 7);
    if (res != ESP_OK) return res;
    time->tm_sec = bcd2dec(data[0]);
    time->tm_min = bcd2dec(data[1]);
    time->tm_hour = bcd2dec(data[2]);
    time->tm_wday = bcd2dec(data[3]) - 1;
    time->tm_mday = bcd2dec(data[4]);
    time->tm_mon  = bcd2dec(data[5]) - 1;
    time->tm_year = bcd2dec(data[6]) + 100;
    time->tm_isdst = 0;
    return ESP_OK;
}

esp_err_t ds3231_get_flag(i2c_dev_t *dev, uint8_t addr, uint8_t mask, uint8_t *flag) {
    if (!dev || !flag) return -1;
    *flag = 0; // Simulación
    return ESP_OK;
}
esp_err_t ds3231_get_oscillator_stop_flag(i2c_dev_t *dev, bool *flag) {
    if (!dev || !flag) return -1;
    uint8_t f = 0;
    ds3231_get_flag(dev, DS3231_ADDR_STATUS, DS3231_STAT_OSCILLATOR, &f);
    *flag = (f ? true : false);
    return ESP_OK;
}
esp_err_t ds3231_clear_oscillator_stop_flag(i2c_dev_t *dev) {
    // Simulación: no hace nada
    return ESP_OK;
}


static const char *RTC_TAG = "RTC_LIB";

i2c_dev_t rtc_dev;

void init_RTC(void) {
    // --- Configuración de la interfaz I2C en modo maestro ---
    i2c_config_t i2c_conf = {
        .mode = I2C_MODE_MASTER,        // Modo maestro (ESP32 controla el bus I2C)
        .sda_io_num = I2C_SDA,          // Pin asignado para la línea SDA
        .scl_io_num = I2C_SCL,          // Pin asignado para la línea SCL
        .sda_pullup_en = GPIO_PULLUP_ENABLE, // Activa resistencia pull-up interna en SDA
        .scl_pullup_en = GPIO_PULLUP_ENABLE, // Activa resistencia pull-up interna en SCL
        .master.clk_speed = 50000      // Frecuencia del bus I2C: 400 kHz (modo rápido)
    };

    // Cargar configuración del I2C en el puerto I2C_NUM_0
    ESP_ERROR_CHECK(i2c_param_config(I2C_NUM_0, &i2c_conf));

    // Instalar el driver de I2C en el puerto I2C_NUM_0 con la configuración de maestro
    // No se usan buffers RX/TX (por eso los parámetros 0,0,0)
    ESP_ERROR_CHECK(i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0));
    
    // --- Inicialización de librerías auxiliares para dispositivos I2C ---
    ESP_ERROR_CHECK(i2cdev_init());  

    // Inicialización de la descripción del dispositivo DS3231 (RTC)
    // Se le pasa el puerto I2C y los pines configurados
    ESP_ERROR_CHECK(ds3231_init_desc(&rtc_dev, I2C_NUM_0, I2C_SDA, I2C_SCL));
    
    // Mensaje de log informando que el RTC quedó inicializado correctamente
    ESP_LOGI(RTC_TAG, "RTC inicializado correctamente");
}



bool rtc_get_time(struct tm *timeinfo) {
    if (ds3231_get_time(&rtc_dev, timeinfo) == ESP_OK) return true;
    return false;
}
    

bool rtc_set_time(struct tm *timeinfo) {
    if (!timeinfo) {
        ESP_LOGE(RTC_TAG, "❌ rtc_set_time: puntero timeinfo es NULL");
        return false;
    }
    
    ESP_LOGI(RTC_TAG, "🔄 Intentando configurar RTC con: %04d-%02d-%02d %02d:%02d:%02d", 
             timeinfo->tm_year + 1900, timeinfo->tm_mon + 1, timeinfo->tm_mday,
             timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
    
    esp_err_t ret = ds3231_set_time(&rtc_dev, timeinfo);
    if (ret == ESP_OK) {
        ESP_LOGI(RTC_TAG, "✅ RTC configurado exitosamente");
        return true;
    } else {
        ESP_LOGE(RTC_TAG, "❌ Falló la configuración del RTC: %s", esp_err_to_name(ret));
        return false;
    }
}

void rtc_configurar_zona_horaria(void) {
    // Usar la zona horaria fija para Argentina (sin DST)
    setenv("TZ", "ART-3", 1);
    tzset();
    ESP_LOGI(RTC_TAG, "Zona horaria configurada para Argentina (UTC-3, sin DST)");
}

void rtc_set_system_time_from_rtc(void) {
    struct tm timeinfo;
    if (ds3231_get_time(&rtc_dev, &timeinfo) == ESP_OK) {
        time_t t = mktime(&timeinfo);
        struct timeval tv = { .tv_sec = t, .tv_usec = 0 };
        settimeofday(&tv, NULL);
        ESP_LOGI(RTC_TAG, "Sistema configurado con hora local del RTC: %04d-%02d-%02d %02d:%02d:%02d",
                 timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                 timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    } else {
        ESP_LOGE(RTC_TAG, "Error al obtener hora del RTC");
    }
}
/*
void rtc_configurar_hora_argentina(void) {
    time_t now;
    struct tm timeinfo;
    
    // Obtener la hora actual del sistema
    time(&now);
    localtime_r(&now, &timeinfo);
    
    // Configurar el RTC con la hora actual
    if (ds3231_set_time(&rtc_dev, &timeinfo) == ESP_OK) {
        ESP_LOGI(RTC_TAG, "Hora configurada para Argentina: %02d:%02d:%02d", 
                 timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    } else {
        ESP_LOGE(RTC_TAG, "Error al configurar hora en el RTC");
    }
} */