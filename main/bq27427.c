#include "../include/battery.h"

static esp_err_t read_control_word(i2c_dev_t *dev, uint16_t function, uint16_t *data);

static const char *TAG = "bq27427";

#define I2C_FREQ_HZ 50000 

#define CHECK(x) do { esp_err_t __; if ((__ = x) != ESP_OK) return __; } while (0)
#define CHECK_ARG(VAL) do { if (!(VAL)) return ESP_ERR_INVALID_ARG; } while (0)

esp_err_t bq27427_init_desc(i2c_dev_t *dev, i2c_port_t port, gpio_num_t sda_gpio, gpio_num_t scl_gpio)
{
    CHECK_ARG(dev);

    dev->port = port;
    dev->addr = BQ27427_I2C_ADDRESS;
    dev->cfg.sda_io_num = sda_gpio;
    dev->cfg.scl_io_num = scl_gpio;
#if HELPER_TARGET_IS_ESP32
    dev->cfg.master.clk_speed = I2C_FREQ_HZ;
#endif
    return i2c_dev_create_mutex(dev);
}

esp_err_t bq27427_get_device_type(i2c_dev_t *dev, uint16_t *dev_type)
{
    CHECK_ARG(dev);
    read_control_word(dev, BQ27427_CONTROL_DEVICE_TYPE, dev_type);
    return ESP_OK;
}

esp_err_t bq27427_get_fw_version(i2c_dev_t *dev, uint16_t *dev_type)
{
    CHECK_ARG(dev);
    read_control_word(dev, BQ27427_CONTROL_FW_VERSION, dev_type);
    return ESP_OK;
}

esp_err_t bq27427_get_voltage(i2c_dev_t *dev, uint16_t *voltage)
{
    CHECK_ARG(dev);
    uint8_t buf[2] = {0};

    // Read 2 bytes starting at register 0x04 (Voltage)
    // At 0x04 is LSB, at 0x05 is MSB → combine as (MSB<<8) | LSB
    I2C_DEV_TAKE_MUTEX(dev);
    I2C_DEV_CHECK(dev, i2c_dev_read_reg(dev, BQ27427_COMMAND_VOLTAGE, buf, sizeof(buf)));
    I2C_DEV_GIVE_MUTEX(dev);

    *voltage = ((uint16_t)buf[1] << 8) | buf[0];
    return ESP_OK;
}

/**
 * PRIVATE FUNCTIONS
*/
esp_err_t read_control_word(i2c_dev_t *dev, uint16_t function, uint16_t *data)
{
    uint8_t subCommandMSB = (function >> 8);
	uint8_t subCommandLSB = (function & 0x00FF);
	uint8_t command[3] = {BQ27427_COMMAND_CONTROL ,subCommandLSB, subCommandMSB};
    uint8_t read_data[2];

    I2C_DEV_TAKE_MUTEX(dev);
    I2C_DEV_CHECK(dev, i2c_dev_write(dev, NULL, 0, command, sizeof(command))); 
    I2C_DEV_CHECK(dev, i2c_dev_read_reg(dev, command[0], read_data, 2));
    I2C_DEV_GIVE_MUTEX(dev);
    *data = (uint16_t)read_data[1] << 8 | read_data[0];
	
	return ESP_OK;
}
