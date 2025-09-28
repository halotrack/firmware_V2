#include "../include/sdcard.h"
#include "driver/gpio.h"
#include "driver/spi_common.h"
#include "esp_vfs_fat.h"
#include "mqtt_lib.h"
#include "../include/HALO.h"
static const char *TAG = "SDCARD";
//esp_mqtt_client_handle_t mqtt_client = NULL;

// Variables globales
sdcard_info_t sdcard_info = {
    .card = NULL,
    .is_mounted = false,
    .mount_point = MOUNT_POINT
};

esp_err_t sdcard_init(void) {
    esp_err_t ret;
    
    // Configuración de montaje
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = 
        #ifdef CONFIG_EXAMPLE_FORMAT_IF_MOUNT_FAILED
            true,
        #else
            false,
        #endif
        .max_files = 5,
        .allocation_unit_size = 16 * 1024
    };

    // Configuración del host SPI
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.max_freq_khz = 1000; // 1MHz para estabilidad

    // Configuración del bus SPI
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = PIN_NUM_MOSI,
        .miso_io_num = PIN_NUM_MISO,
        .sclk_io_num = PIN_NUM_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4000,
    };

    // Configurar pull-up para los pines SPI
    gpio_set_pull_mode(PIN_NUM_MOSI, GPIO_PULLUP_ONLY);
    gpio_set_pull_mode(PIN_NUM_MISO, GPIO_PULLUP_ONLY);
    gpio_set_pull_mode(PIN_NUM_CLK, GPIO_PULLUP_ONLY);
    gpio_set_pull_mode(PIN_NUM_CS, GPIO_PULLUP_ONLY);

    // Inicializar el bus SPI
    spi_bus_initialize(host.slot, &bus_cfg, SDSPI_DEFAULT_DMA);

    // Configuración del dispositivo SD
    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = PIN_NUM_CS;
    slot_config.host_id = host.slot;

    // Montar el sistema de archivos
    ESP_LOGI(TAG, "Montando sistema de archivos...");
    esp_vfs_fat_sdspi_mount(sdcard_info.mount_point, &host, &slot_config,&mount_config, &sdcard_info.card);

    sdcard_info.is_mounted = true;
    ESP_LOGI(TAG, "Tarjeta SD inicializada correctamente");
    
    // Crear archivos CSV si no existen
    const char* csv_files[] = {"/pesos.csv"};
    const char* headers[] = {"Fecha,Hora,Peso_kg\n"};
    
    for (int i = 0; i < sizeof(csv_files)/sizeof(csv_files[0]); i++) {
        if (!sdcard_file_exists(csv_files[i])) {
            sdcard_write_file(csv_files[i], headers[i]);
            ESP_LOGI(TAG, "Archivo %s creado", csv_files[i]);
        }
    }

    const char* csv_filesV[] = {"/voltajes.csv"};
    const char* headersV[] = {"Fecha,Hora,Voltaje\n"};
    
    for (int i = 0; i < sizeof(csv_filesV)/sizeof(csv_filesV[0]); i++) {
        if (!sdcard_file_exists(csv_filesV[i])) {
            sdcard_write_file(csv_filesV[i], headersV[i]);
            ESP_LOGI(TAG, "Archivo %s creado", csv_filesV[i]);
        }
    }

    // Mostrar información de la tarjeta
    sdcard_print_info();
    
    return ESP_OK;
}



esp_err_t sdcard_deinit(void) {
    if (sdcard_info.is_mounted) {
        sdcard_unmount();
    }
    
    // Liberar el bus SPI
    spi_bus_free(SPI2_HOST);
    
    ESP_LOGI(TAG, "Tarjeta SD desinicializada");
    return ESP_OK;
}


esp_err_t sdcard_unmount(void) {
    if (!sdcard_info.is_mounted) {
        ESP_LOGW(TAG, "La tarjeta SD no está montada");
        return ESP_OK;
    }
    
    esp_vfs_fat_sdcard_unmount(sdcard_info.mount_point, sdcard_info.card);
    sdcard_info.is_mounted = false;
    sdcard_info.card = NULL;
    
    ESP_LOGI(TAG, "Tarjeta SD desmontada");
    esp_mqtt_client_publish(mqtt_client, "esp32/halo/status", "⚠️⚠️⚠️TARJETA SD DESMONTADA⚠️⚠️⚠️", 0, 1, 0);
    return ESP_OK;
}

esp_err_t sdcard_write_file(const char *path, const char *data) {
    if (!sdcard_info.is_mounted) {
        ESP_LOGE(TAG, "Tarjeta SD no montada");
        return ESP_FAIL;
    }
    
    char full_path[128];
    snprintf(full_path, sizeof(full_path), "%s%s", sdcard_info.mount_point, path);
    
    ESP_LOGI(TAG, "Escribiendo archivo: %s", full_path);
    
    FILE *f = fopen(full_path, "w");
    if (f == NULL) {
        ESP_LOGE(TAG, "Error al abrir archivo para escritura: %s", full_path);
        return ESP_FAIL;
    }
    
    fprintf(f, "%s", data);
    fclose(f);
    
    ESP_LOGI(TAG, "Archivo escrito correctamente");
    return ESP_OK;
}


esp_err_t sdcard_append_file(const char *path, const char *data) {
    const int MAX_REINTENTOS = 10;
    const int DELAY_ENTRE_INTENTOS_MS = 3000;
    
    for (int intento = 1; intento <= MAX_REINTENTOS; intento++) {
        if (!sdcard_info.is_mounted) {
            ESP_LOGW(TAG, "SD no montada (intento %d/%d). Reinicializando completamente...", intento, MAX_REINTENTOS);
            
            // Desmonta completamente y libera recursos
            if (sdcard_info.card != NULL) {
                sdcard_unmount();
            }
            sdcard_deinit();
            
            // Espera antes de reintentar
            vTaskDelay(DELAY_ENTRE_INTENTOS_MS / portTICK_PERIOD_MS);
            
            // Reinicializa desde cero
            if (sdcard_init() != ESP_OK) {
                ESP_LOGE(TAG, "Fallo en reinicialización SD (intento %d/%d)", intento, MAX_REINTENTOS);
                continue;
            }
            
            ESP_LOGI(TAG, "SD reinicializada exitosamente en intento %d", intento);
            if(mqtt_is_connected()){
                esp_mqtt_client_publish(mqtt_client, "esp32/halo/status", "✅✅✅SISTEMA CONECTADO ✅✅✅", 0, 1, 0);
                gpio_set_level(LED_USER, 0);
                esp_mqtt_client_publish(mqtt_client, "esp32/halo/conection", "ON", 0, 1, 0);
            }
        }
        
        char full_path[128];
        snprintf(full_path, sizeof(full_path), "%s%s", sdcard_info.mount_point, path);
        
        FILE *f = fopen(full_path, "a");
        if (f == NULL) {
            ESP_LOGE(TAG, "Error al abrir archivo %s (intento %d/%d)", full_path, intento, MAX_REINTENTOS);
            // Fuerza desmontaje para próximo intento
            sdcard_unmount();
            if (intento < MAX_REINTENTOS) {
                vTaskDelay(DELAY_ENTRE_INTENTOS_MS / portTICK_PERIOD_MS);
            }
            continue;
        }
        
        if (fprintf(f, "%s", data) < 0) {
            ESP_LOGE(TAG, "Error al escribir en archivo (intento %d/%d)", intento, MAX_REINTENTOS);
            fclose(f);
            sdcard_unmount();
            if (intento < MAX_REINTENTOS) {
                vTaskDelay(DELAY_ENTRE_INTENTOS_MS / portTICK_PERIOD_MS);
            }
            continue;
        }
        fclose(f);
        //sdcard_log_voltaje();

        ESP_LOGI(TAG, "Datos agregados");
        
        return ESP_OK;
    }
    
    ESP_LOGE(TAG, "FALLO CRÍTICO: No se pudo escribir en SD tras %d intentos", MAX_REINTENTOS);
    return ESP_FAIL;
}



bool sdcard_file_exists(const char *path) {
    if (!sdcard_info.is_mounted) {
        return false;
    }
    
    char full_path[128];
    snprintf(full_path, sizeof(full_path), "%s%s", sdcard_info.mount_point, path);
    
    struct stat st;
    return (stat(full_path, &st) == 0);
}

esp_err_t sdcard_log_peso(float peso, struct tm *timeinfo) {
    if (!sdcard_info.is_mounted) {
        ESP_LOGE(TAG, "Tarjeta SD no montada");
        return ESP_FAIL;
    }
    
    char log_entry[128]; 
    int written = snprintf(log_entry, sizeof(log_entry), 
                          "%04d-%02d-%02d,%02d:%02d:%02d,%.2f\n",
                          timeinfo->tm_year + 1900, timeinfo->tm_mon + 1, timeinfo->tm_mday,
                          timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec, peso);
    
    return sdcard_append_file("/pesos.csv", log_entry);
}





void sdcard_print_info(void) {
    if (sdcard_info.card == NULL) {
        ESP_LOGI(TAG, "No hay información de tarjeta disponible");
        return;
    }
    ESP_LOGI(TAG, "Información de la tarjeta SD:");

    uint64_t total_bytes, free_bytes;
    esp_err_t info_err = esp_vfs_fat_info("/sdcard", &total_bytes, &free_bytes);
    if (info_err == ESP_OK) {
        ESP_LOGI(TAG, "Capacidad total: %.2f MB", total_bytes / (1024.0 * 1024.0));
        ESP_LOGI(TAG, "Espacio disponible: %.2f MB", free_bytes / (1024.0 * 1024.0));
    } else {
        ESP_LOGW(TAG, "No se pudo obtener información de espacio de la SD: %s", esp_err_to_name(info_err));
    }
}
