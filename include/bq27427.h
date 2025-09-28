/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2025 Tomoyuki Sakurai
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#pragma once

#include <stdint.h> // system headers first
#include <esp_err.h> // then, esp-idf headers
#include <i2cdev.h>
// #include "local.h" // local header files at the end.

#ifdef __cplusplus
extern "C" {
#endif

extern i2c_dev_t bq27427_dev;

#define BQ27427_I2C_ADDRESS 0x55 // Default I2C address of the BQ27427-G1A

///////////////////////
// General Constants //
///////////////////////
#define BQ27427_UNSEAL_KEY	0x8000 // Secret code to unseal the BQ27427-G1A
#define BQ27427_DEVICE_ID	0x0421 // Default device ID

///////////////////////
// Standard Commands //
///////////////////////
// The fuel gauge uses a series of 2-byte standard commands to enable system 
// reading and writing of battery information. Each command has an associated
// sequential command-code pair.
#define BQ27427_COMMAND_CONTROL			0x00 // Control()
#define BQ27427_COMMAND_TEMP			0x02 // Temperature()
#define BQ27427_COMMAND_VOLTAGE			0x04 // Voltage()
#define BQ27427_COMMAND_FLAGS			0x06 // Flags()
#define BQ27427_COMMAND_NOM_CAPACITY	0x08 // NominalAvailableCapacity()
#define BQ27427_COMMAND_AVAIL_CAPACITY	0x0A // FullAvailableCapacity()
#define BQ27427_COMMAND_REM_CAPACITY	0x0C // RemainingCapacity()
#define BQ27427_COMMAND_FULL_CAPACITY	0x0E // FullChargeCapacity()
#define BQ27427_COMMAND_AVG_CURRENT		0x10 // AverageCurrent()
#define BQ27427_COMMAND_STDBY_CURRENT	0x12 // StandbyCurrent()
#define BQ27427_COMMAND_MAX_CURRENT		0x14 // MaxLoadCurrent()
#define BQ27427_COMMAND_AVG_POWER		0x18 // AveragePower()
#define BQ27427_COMMAND_SOC				0x1C // StateOfCharge()
#define BQ27427_COMMAND_INT_TEMP		0x1E // InternalTemperature()
#define BQ27427_COMMAND_SOH				0x20 // StateOfHealth()
#define BQ27427_COMMAND_REM_CAP_UNFL	0x28 // RemainingCapacityUnfiltered()
#define BQ27427_COMMAND_REM_CAP_FIL		0x2A // RemainingCapacityFiltered()
#define BQ27427_COMMAND_FULL_CAP_UNFL	0x2C // FullChargeCapacityUnfiltered()
#define BQ27427_COMMAND_FULL_CAP_FIL	0x2E // FullChargeCapacityFiltered()
#define BQ27427_COMMAND_SOC_UNFL		0x30 // StateOfChargeUnfiltered()

//////////////////////////
// Control Sub-commands //
//////////////////////////
// Issuing a Control() command requires a subsequent 2-byte subcommand. These
// additional bytes specify the particular control function desired. The 
// Control() command allows the system to control specific features of the fuel
// gauge during normal operation and additional features when the device is in 
// different access modes.
#define BQ27427_CONTROL_STATUS			0x00
#define BQ27427_CONTROL_DEVICE_TYPE		0x01
#define BQ27427_CONTROL_FW_VERSION		0x02
#define BQ27427_CONTROL_DM_CODE			0x04
#define BQ27427_CONTROL_PREV_MACWRITE	0x07
#define BQ27427_CONTROL_CHEM_ID			0x08
#define BQ27427_CONTROL_BAT_INSERT		0x0C
#define BQ27427_CONTROL_BAT_REMOVE		0x0D
#define BQ27427_CONTROL_SET_CFGUPDATE	0x13
#define BQ27427_CONTROL_SHUTDOWN_ENABLE	0x1B
#define BQ27427_CONTROL_SHUTDOWN		0x1C
#define BQ27427_CONTROL_SEALED			0x20
#define BQ27427_CONTROL_PULSE_SOC_INT	0x23
#define BQ27427_CONTROL_CHEM_A          0x30
#define BQ27427_CONTROL_CHEM_B          0x31
#define BQ27427_CONTROL_CHEM_C          0x32
#define BQ27427_CONTROL_RESET			0x41
#define BQ27427_CONTROL_SOFT_RESET		0x42

///////////////////////////////////////////
// Control Status Word - Bit Definitions //
///////////////////////////////////////////
// Bit positions for the 16-bit data of CONTROL_STATUS.
// CONTROL_STATUS instructs the fuel gauge to return status information to 
// Control() addresses 0x00 and 0x01. The read-only status word contains status
// bits that are set or cleared either automatically as conditions warrant or
// through using specified subcommands.
#define BQ27427_STATUS_SHUTDOWNEN	(1<<15)
#define BQ27427_STATUS_WDRESET		(1<<14)
#define BQ27427_STATUS_SS			(1<<13)
#define BQ27427_STATUS_CALMODE		(1<<12)
#define BQ27427_STATUS_CCA			(1<<11)
#define BQ27427_STATUS_BCA			(1<<10)
#define BQ27427_STATUS_QMAX_UP		(1<<9)
#define BQ27427_STATUS_RES_UP		(1<<8)
#define BQ27427_STATUS_INITCOMP		(1<<7)
#define BQ27427_STATUS_SLEEP		(1<<4)
#define BQ27427_STATUS_LDMD			(1<<3)
#define BQ27427_STATUS_RUP_DIS		(1<<2)
#define BQ27427_STATUS_VOK			(1<<1)

////////////////////////////////////
// Flag Command - Bit Definitions //
////////////////////////////////////
// Bit positions for the 16-bit data of Flags()
// This read-word function returns the contents of the fuel gauging status
// register, depicting the current operating status.
#define BQ27427_FLAG_OT			(1<<15)
#define BQ27427_FLAG_UT			(1<<14)
#define BQ27427_FLAG_FC			(1<<9)
#define BQ27427_FLAG_CHG		(1<<8)
#define BQ27427_FLAG_OCVTAKEN	(1<<7)
#define BQ27427_FLAG_DOD_CRRCT	(1<<6)
#define BQ27427_FLAG_ITPOR		(1<<5)
#define BQ27427_FLAG_CFGUPMODE	(1<<4)
#define BQ27427_FLAG_BAT_DET	(1<<3)
#define BQ27427_FLAG_SOC1		(1<<2)
#define BQ27427_FLAG_SOCF		(1<<1)
#define BQ27427_FLAG_DSG		(1<<0)

////////////////////////////
// Extended Data Commands //
////////////////////////////
// Extended data commands offer additional functionality beyond the standard
// set of commands. They are used in the same manner; however, unlike standard
// commands, extended commands are not limited to 2-byte words.
#define BQ27427_EXTENDED_DATACLASS	0x3E // DataClass()
#define BQ27427_EXTENDED_DATABLOCK	0x3F // DataBlock()
#define BQ27427_EXTENDED_BLOCKDATA	0x40 // BlockData()
#define BQ27427_EXTENDED_CHECKSUM	0x60 // BlockDataCheckSum()
#define BQ27427_EXTENDED_CONTROL	0x61 // BlockDataControl()

////////////////////////////////////////
// Configuration Class, Subclass ID's //
////////////////////////////////////////
// To access a subclass of the extended data, set the DataClass() function
// with one of these values.
// Configuration Classes
#define BQ27427_ID_SAFETY			2   // Safety
#define BQ27427_ID_CHG_TERMINATION	36  // Charge Termination
// #define BQ27427_ID_CONFIG_DATA		48  // Data
#define BQ27427_ID_DISCHARGE		49  // Discharge
#define BQ27427_ID_REGISTERS		64  // Registers
// #define BQ27427_ID_POWER			68  // Power
// Gas Gauging Classes
#define BQ27427_ID_IT_CFG			80  // IT Cfg
#define BQ27427_ID_CURRENT_THRESH	81  // Current Thresholds
#define BQ27427_ID_STATE			82  // State
// Ra Tables Classes
#define BQ27427_ID_R_A_RAM			89  // R_a RAM
// Chemistry Info Classes
#define BQ27427_ID_CHEM_DATA        109  // Chem Data
// Calibration Classes
#define BQ27427_ID_CALIB_DATA		104 // Data
#define BQ27427_ID_CC_CAL			105 // CC Cal
#define BQ27427_ID_CURRENT			107 // Current
// Security Classes
#define BQ27427_ID_CODES			112 // Codes

/////////////////////////////////////////
// OpConfig Register - Bit Definitions //
/////////////////////////////////////////
// Bit positions of the OpConfig Register
#define BQ27427_OPCONFIG_BIE        (1<<13)
#define BQ27427_OPCONFIG_GPIOPOL    (1<<11)
#define BQ27427_OPCONFIG_RS_FCT_STP (1<<6)
#define BQ27427_OPCONFIG_SLEEP      (1<<5)
#define BQ27427_OPCONFIG_RMFCC      (1<<4)
#define BQ27427_OPCONFIG_FASTCNV_EN (1<<3)
#define BQ27427_OPCONFIG_BATLOWEN   (1<<2)
#define BQ27427_OPCONFIG_TEMPS      (1<<0)



#define BQ72441_I2C_TIMEOUT 2000


/** 
* @brief Chemistry profiles
*/ 
typedef enum {
	CHEM_A = BQ27427_CONTROL_CHEM_A,  // 4.35V
	CHEM_B = BQ27427_CONTROL_CHEM_B, // 4.2V
	CHEM_C = BQ27427_CONTROL_CHEM_C   // 4.4V
} chemistry_profiles;

/**
 * @brief Parameters for the current() function, to specify which current to read
 */
typedef enum {
	AVG,  // Average Current (DEFAULT)
	STBY, // Standby Current
	MAX   // Max Current
} current_measure;

/**
 * @brief Parameters for the capacity() function, to specify which capacity to read
 */
typedef enum {
	REMAIN,     // Remaining Capacity (DEFAULT)
	FULL,       // Full Capacity
	AVAIL,      // Available Capacity
	AVAIL_FULL, // Full Available Capacity
	REMAIN_F,   // Remaining Capacity Filtered
	REMAIN_UF,  // Remaining Capacity Unfiltered
	FULL_F,     // Full Capacity Filtered
	FULL_UF,    // Full Capacity Unfiltered
	DESIGN      // Design Capacity
} capacity_measure;

// Parameters for the soc() function
typedef enum {
	FILTERED,  // State of Charge Filtered (DEFAULT)
	UNFILTERED // State of Charge Unfiltered
} soc_measure;

// Parameters for the soh() function
typedef enum {
	PERCENT,  // State of Health Percentage (DEFAULT)
	SOH_STAT  // State of Health Status Bits
} soh_measure;

// Parameters for the temperature() function
typedef enum {
	BATTERY,      // Battery Temperature (DEFAULT)
	INTERNAL_TEMP // Internal IC Temperature
} temp_measure;

// Parameters for the setGPOUTFunction() funciton
typedef enum {
	SOC_INT, // Set GPOUT to SOC_INT functionality
	BAT_LOW  // Set GPOUT to BAT_LOW functionality
} gpout_function;

/**
    Initializes I2C and verifies communication with the BQ27427.
    Must be called before using any other functions.
    
    @param sda pin number for I2C data line
    @param scl pin number for I2C clock line
    @return true if communication was successful.
*/
esp_err_t bq27427_init_desc(i2c_dev_t *dev, i2c_port_t port, gpio_num_t sda_gpio, gpio_num_t scl_gpio);

/**
    Configures the design capacity of the connected battery.
    
    @param capacity of battery (unsigned 16-bit value)
    @return true if capacity successfully set.
*/
esp_err_t bq27427_set_capacity(i2c_dev_t *dev, uint16_t *capacity);

/**
    Reads and returns the design energy of the connected battery
    
    @return design energy in milliWattHours (mWh)
*/
esp_err_t bq27427_get_design_energy(i2c_dev_t *dev, uint16_t *energy);

/**
    Configures the design energy of the connected battery.
    
    @param energy of battery (unsigned 16-bit value)
    @return true if energy successfully set.
*/
esp_err_t bq27427_set_design_energy(i2c_dev_t *dev, uint16_t *energy);

/**
    Reads and returns the terminate voltage of the connected battery
    
    @return terminate voltage in millivolts (mV)
*/
esp_err_t bq27427_get_terminate_voltage(i2c_dev_t *dev, uint16_t *voltage);

/**
    Configures terminate voltage (lowest operational voltage of battery powered circuit)
    
    @param voltage of battery (unsigned 16-bit value)
    @return true if voltage successfully set.
*/
esp_err_t bq27427_set_terminate_voltage(i2c_dev_t *dev, uint16_t *voltage);

/**
    Reads and returns the discharge current threshold
    
    @return discharge current threshold in 0.1h units
*/
esp_err_t bq27427_get_discharge_current_threshold(i2c_dev_t *dev, uint16_t *current);

/**
    Configures discharge current threshold
    
    @param value in 0.1h units (unsigned 16-bit value)
    @return true if threshold successfully set.
*/
esp_err_t bq27427_set_discharge_current_threshold(i2c_dev_t *dev, uint16_t *current);

/**
    Reads and returns the taper voltage of the connected battery
    
    @return taper voltage in millivolts (mV)
*/
esp_err_t bq27427_get_taper_voltage(i2c_dev_t *dev, uint16_t *voltage);

/**
    Configures taper voltage
    
    @param voltage of battery (unsigned 16-bit value)
    @return true if voltage successfully set.
*/
esp_err_t bq27427_set_taper_voltage(i2c_dev_t *dev, uint16_t *voltage);

/**
    Reads and returns the taper rate of the connected battery
    
    @return taper rate in 0.1 h units
*/
esp_err_t bq27427_get_taper_rate(i2c_dev_t *dev, uint16_t *rate);

/**
    Configures taper rate of connected battery
    
    @param rate in 0.1 h units (unsigned 16-bit value)
    @return true if taper rate successfully set.
*/
esp_err_t bq27427_set_taper_rate(i2c_dev_t *dev, uint16_t *rate);

/**
    Configures taper rate of connected battery
    
    @return true if current polarity successfully changed.
*/
esp_err_t bq27427_change_current_polarity(i2c_dev_t *dev);

/////////////////////////////
// Battery Characteristics //
/////////////////////////////
/**
    Reads and returns the battery voltage
    
    @return battery voltage in mV
*/
esp_err_t bq27427_get_voltage(i2c_dev_t *dev, uint16_t *voltage);

/**
    Reads and returns the specified current measurement
    
    @param current_measure enum specifying current value to be read
    @return specified current measurement in mA. >0 indicates charging.
*/
esp_err_t bq27427_get_current(i2c_dev_t *dev, current_measure type, int16_t *current);

/**
    Reads and returns the specified capacity measurement
    
    @param capacity_measure enum specifying capacity value to be read
    @return specified capacity measurement in mAh.
*/
esp_err_t bq27427_get_capacity(i2c_dev_t *dev, capacity_measure type, uint16_t *capacity);

/**
    Reads and returns measured average power
    
    @return average power in mAh. >0 indicates charging.
*/
esp_err_t bq27427_get_power(i2c_dev_t *dev, int16_t *power);

/**
    Reads and returns specified state of charge measurement
    
    @param soc_measure enum specifying filtered or unfiltered measurement
    @return specified state of charge measurement in %
*/
esp_err_t bq27427_get_soc(i2c_dev_t *dev, soc_measure type, uint16_t *soc);

/**
    Reads and returns specified state of health measurement
    
    @param soh_measure enum specifying filtered or unfiltered measurement
    @return specified state of health measurement in %, or status bits
*/
esp_err_t bq27427_get_soh(i2c_dev_t *dev, soh_measure type, uint8_t *soh);

/**
    Reads and returns specified temperature measurement
    
    @param temp_measure enum specifying internal or battery measurement
    @return specified temperature measurement in degrees C
*/
esp_err_t bq27427_get_temperature(i2c_dev_t *dev, temp_measure type, uint16_t *temperature);

////////////////////////////	
// GPOUT Control Commands //
////////////////////////////
/**
    Get GPOUT polarity setting (active-high or active-low)
    
    @return true if active-high, false if active-low
*/
esp_err_t bq27427_get_gpout_polarity(i2c_dev_t *dev, uint8_t *polarity);

/**
    Set GPOUT polarity to active-high or active-low
    
    @param activeHigh is true if active-high, false if active-low
    @return true on success
*/
esp_err_t bq27427_set_gpout_polarity(i2c_dev_t *dev, uint8_t activeHigh);

/**
    Get GPOUT function (BAT_LOW or SOC_INT)
    
    @return true if BAT_LOW or false if SOC_INT
*/
esp_err_t bq27427_get_gpout_function(i2c_dev_t *dev, gpout_function *function);

/**
    Set GPOUT function to BAT_LOW or SOC_INT
    
    @param function should be either BAT_LOW or SOC_INT
    @return true on success
*/
esp_err_t bq27427_set_gpout_function(i2c_dev_t *dev, gpout_function *function);

/**
    Get SOC1_Set Threshold - threshold to set the alert flag
    
    @return state of charge value between 0 and 100%
*/
esp_err_t bq27427_soc1_set_threshold(i2c_dev_t *dev);

/**
    Get SOC1_Clear Threshold - threshold to clear the alert flag
    
    @return state of charge value between 0 and 100%
*/
esp_err_t bq27427_soc1_clear_threshold(i2c_dev_t *dev);

/**
    Set the SOC1 set and clear thresholds to a percentage
    
    @param set and clear percentages between 0 and 100. clear > set.
    @return true on success
*/
esp_err_t bq27427_set_soc1_thresholds(i2c_dev_t *dev, uint8_t *set, uint8_t *clear);

/**
    Get SOCF_Set Threshold - threshold to set the alert flag
    
    @return state of charge value between 0 and 100%
*/
esp_err_t bq27427_socf_set_threshold(i2c_dev_t *dev);

/**
    Get SOCF_Clear Threshold - threshold to clear the alert flag
    
    @return state of charge value between 0 and 100%
*/
esp_err_t bq27427_socf_clear_threshold(i2c_dev_t *dev);

/**
    Set the SOCF set and clear thresholds to a percentage
    
    @param set and clear percentages between 0 and 100. clear > set.
    @return true on success
*/
esp_err_t bq27427_set_socf_thresholds(i2c_dev_t *dev, uint8_t *set, uint8_t *clear);

/**
    Check if the SOC1 flag is set in flags()
    
    @return true if flag is set
*/
esp_err_t bq27427_get_soc_flag(i2c_dev_t *dev, bool *out);

/**
    Check if the SOCF flag is set in flags()
    
    @return true if flag is set
*/
esp_err_t bq27427_get_socf_flag(i2c_dev_t *dev, bool *out);

/**
    Check if the ITPOR flag is set in flags()
    
    @return true if flag is set
*/
esp_err_t bq27427_get_itpor_flag(i2c_dev_t *dev, bool *out);

/**
    Check if the FC flag is set in flags()
    
    @return true if flag is set
*/
esp_err_t bq27427_get_fc_flag(i2c_dev_t *dev, bool *out);

/**
    Check if the CHG flag is set in flags()
    
    @return true if flag is set
*/
esp_err_t bq27427_get_chg_flag(i2c_dev_t *dev, bool *out);

/**
    Check if the DSG flag is set in flags()
    
    @return true if flag is set
*/
esp_err_t bq27427_get_dsg_flag(i2c_dev_t *dev, bool *out);


/**
    Get the SOC_INT interval delta
    
    @return interval percentage value between 1 and 100
*/
esp_err_t bq27427_get_soci_delta(i2c_dev_t *dev, uint8_t *delta);

/**
    Set the SOC_INT interval delta to a value between 1 and 100
    
    @param interval percentage value between 1 and 100
    @return true on success
*/
esp_err_t bq27427_set_soci_delta(i2c_dev_t *dev, uint8_t *delta);

/**
    Pulse the GPOUT pin - must be in SOC_INT mode
    
    @return true on success
*/
esp_err_t bq27427_pulse_gpout(i2c_dev_t *dev);

//////////////////////////
// Control Sub-commands //
//////////////////////////

/**
    Read the device type - should be 0x0421
    
    @return 16-bit value read from DEVICE_TYPE subcommand
*/
esp_err_t bq27427_get_device_type(i2c_dev_t *dev, uint16_t *dev_type);

esp_err_t bq27427_get_fw_version(i2c_dev_t *dev, uint16_t *dev_type);

/**
    Configures the chemistry profile of the connected battery.
    
    @param chem_id enum specifying chemistry profile value to be set
    @return true if chemistry profile successfully set.
*/
esp_err_t bq27427_set_chem_id(i2c_dev_t *dev, chemistry_profiles *chem_id);

/**
    Reads and returns the battery chemistry profile.
    
    @return chemistry profile enum value
*/
esp_err_t bq27427_get_chem_id(i2c_dev_t *dev, uint16_t *out);

/**
    Enter configuration mode - set userControl if calling from an Arduino
    sketch and you want control over when to exitConfig.
    
    @param userControl is true if the Arduino sketch is handling entering 
    and exiting config mode (should be false in library calls).
    @return true on success
*/
esp_err_t bq27427_enter_config(i2c_dev_t *dev, uint8_t userControl);

/**
    Exit configuration mode
    
    @return true on success
*/
esp_err_t bq27427_exit_config(i2c_dev_t *dev, uint8_t userControl);

/**
    Read the flags() command
    
    @return 16-bit representation of flags() command register
*/
esp_err_t bq27427_get_flags(i2c_dev_t *dev, uint16_t *out);

/**
    Read the CONTROL_STATUS subcommand of control()
    
    @return 16-bit representation of CONTROL_STATUS subcommand
*/
esp_err_t bq27427_get_status(i2c_dev_t *dev, uint16_t *out);

/**
    Issue a factory reset to the BQ27427
    
    @return true on success
*/	
esp_err_t bq27427_reset(i2c_dev_t *dev);

#ifdef __cplusplus
}
#endif
