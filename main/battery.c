#include "../include/battery.h"
#include "../include/HALO.h"
#include "../include/mqtt_lib.h"
#include <esp_log.h>
#include "../include/sdcard.h"
#include "../include/rtc_lib.h"
#include <time.h>
#include <stdio.h>

#define BQ27427_INIT_OK             0
#define BQ27427_INIT_FAIL           1

i2c_dev_t fuel_gauge_dev;
static uint8_t fuel_init = BQ27427_INIT_FAIL;
esp_err_t init_battery(void)
{
    esp_err_t retval = ESP_ERR_NOT_ALLOWED;
    //Proper I2C initialization should be verified before this.
    if (fuel_init == BQ27427_INIT_FAIL){
        esp_err_t fuel_init_err = bq27427_init_desc(&fuel_gauge_dev, I2C_NUM_0, I2C_SDA, I2C_SCL);
        if (fuel_init_err == ESP_OK)
        {
            fuel_init = BQ27427_INIT_OK;
            retval =  ESP_OK;
        }
            
        else{
            retval = ESP_FAIL;
        }
    }

    return retval;
}

esp_err_t battery_get_voltage(uint16_t *voltage)
{
    return bq27427_get_voltage(&fuel_gauge_dev, voltage);
}


void sdcard_log_voltaje(struct tm *timeinfo)
{

    uint16_t voltage;
    esp_err_t ret = battery_get_voltage(&voltage);

    char log_entry[128]; 
    int written = snprintf(log_entry, sizeof(log_entry), 
                          "%04d-%02d-%02d,%02d:%02d:%02d,%.2f\n",
                          timeinfo->tm_year + 1900, timeinfo->tm_mon + 1, timeinfo->tm_mday,
                          timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec, voltage/1000.0f);
    
    if (written > 0 && written < sizeof(log_entry)) {
        sdcard_append_file("/voltajes.csv", log_entry);
    } 
}





