#include "HALO.h"

const char *TAG = "HALO";
sistema_config_t sistema = {0};


void app_main(void) {

    ESP_LOGI(TAG, "FW %s (IDF %s) tag=%s sha=%s at %s",
         esp_app_get_description()->version, esp_get_idf_version(),
         FW_GIT_TAG, FW_GIT_SHA, FW_BUILD_DATE);
         


         
    inicializar_sistema();
    create_task_HX711();
    create_task_MQTT();
    printf("========================================\n");
    printf("         HALO - Version 2.0\n");
    printf("========================================\n");

    ESP_LOGI(TAG, "----------VERSION 2.0.0----------------");
    ESP_LOGE(TAG, "----------VERSION 2.0.0----------------");
    ESP_LOGW(TAG, "----------VERSION 2.0.0----------------");


}



