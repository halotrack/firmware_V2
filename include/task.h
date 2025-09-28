#ifndef TASK_H
#define TASK_H


////#include "HALO.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_err.h"
#include "esp_timer.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_http_client.h"
#include <limits.h>
#include "freertos/portmacro.h"

void create_task_HX711(void);
void create_task_MQTT(void);
void task_MQTT(void *pvParameters); 
void task_HX711(void *pvParameters);                      
void user_button_init(void);
void user_button_task(void* arg);
void IRAM_ATTR user_button_isr_handler(void* arg);


#endif // TASK_H 