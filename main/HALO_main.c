#include "HALO.h"

const char *TAG = "HALO";
sistema_config_t sistema = {0};


void app_main(void) {

    ESP_LOGI(TAG, "FW %s (IDF %s) tag=%s sha=%s at %s",
         esp_app_get_description()->version, esp_get_idf_version(),
         FW_GIT_TAG, FW_GIT_SHA, FW_BUILD_DATE);
         

    // Hacer titilar cada 500ms el LED en GPIO0
    gpio_pad_select_gpio(GPIO_NUM_0);
    gpio_set_direction(GPIO_NUM_0, GPIO_MODE_OUTPUT);

    for (int i = 0; i < 10; i++) { // Titilar 10 veces antes de continuar
        gpio_set_level(GPIO_NUM_0, 1);
        vTaskDelay(500 / portTICK_PERIOD_MS);
        gpio_set_level(GPIO_NUM_0, 0);
        vTaskDelay(500 / portTICK_PERIOD_MS);
    }

         
    //inicializar_sistema();
    //create_task_HX711();
    //create_task_MQTT();

}



