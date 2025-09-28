#ifndef SDCARD_H
#define SDCARD_H

#include <stdio.h>
#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "esp_log.h"

// Configuración de pines SPI para SD
#define PIN_NUM_MISO  19
#define PIN_NUM_MOSI  23
#define PIN_NUM_CLK   18
#define PIN_NUM_CS    5

#define MOUNT_POINT "/sdcard"
#define EXAMPLE_MAX_CHAR_SIZE 64

// Estructura para información de la tarjeta SD
typedef struct {
    sdmmc_card_t *card;
    bool is_mounted;
    const char *mount_point;
} sdcard_info_t;

// Funciones de inicialización
esp_err_t sdcard_init(void);
esp_err_t sdcard_deinit(void);
esp_err_t sdcard_unmount(void);

// Funciones de archivos
esp_err_t sdcard_write_file(const char *path, const char *data);
esp_err_t sdcard_append_file(const char *path, const char *data);
esp_err_t sdcard_append_fileV(const char *pathg, const char *datag);
bool sdcard_file_exists(const char *path);

// Funciones específicas para datos de peso
esp_err_t sdcard_log_peso(float peso, struct tm *timeinfo);
esp_err_t sdcard_log_error(const char *error_msg, struct tm *timeinfo);

// Funciones de utilidad
void sdcard_print_info(void);
esp_err_t sdcard_format_if_needed(void);

// Variables globales
extern sdcard_info_t sdcard_info;

#endif // SDCARD_H 