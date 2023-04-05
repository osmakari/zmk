
/*
 * Copyright (c) 2020 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */
#define DT_DRV_COMPAT azoteq_iqs5xx

#include <drivers/gpio.h>
#include <nrfx_gpiote.h>
#include <init.h>
#include <drivers/sensor.h>
#include <logging/log.h>
#include <drivers/i2c.h>
#include <device.h>
#include <devicetree.h>
#include "iqs5xx.h"

// DT_DRV_INST(0);
LOG_MODULE_REGISTER(azoteq_iqs5xx, CONFIG_ZMK_LOG_LEVEL);

// Default config
struct iqs5xx_reg_config iqs5xx_reg_config_default () {
    struct iqs5xx_reg_config regconf;
  
    regconf.activeRefreshRate =         10;
    regconf.idleRefreshRate =           50;
    regconf.singleFingerGestureMask =   GESTURE_SINGLE_TAP | GESTURE_TAP_AND_HOLD;
    regconf.multiFingerGestureMask =    GESTURE_TWO_FINGER_TAP | GESTURE_SCROLLG;
    regconf.tapTime =                   150;
    regconf.tapDistance =               25;
    regconf.touchMultiplier =           0;
    regconf.debounce =                  0;
    regconf.i2cTimeout =                4; 
    regconf.filterSettings =            MAV_FILTER | IIR_FILTER /* | IIR_SELECT static mode */;
    regconf.filterDynBottomBeta =        22;
    regconf.filterDynLowerSpeed =        19;
    regconf.filterDynUpperSpeed =        140;

    regconf.initScrollDistance =        25;
    

    return regconf;
}

/**
 * @brief Read from the iqs550 chip via i2c
 * example: iqs5xx_seq_read(dev, GestureEvents0_adr, buffer, 44)
 *
 * @param dev Pointer to device driver MASTER
 * @param start start address for reading
 * @param  pointer to buffer to be read into
 * @param len number of bytes to read
 * @return int
 */
static int iqs5xx_seq_read(const struct device *dev, const uint16_t start, uint8_t *read_buf,
                           const uint8_t len) {
    const struct iqs5xx_data *data = dev->data;
    const struct iqs5xx_config *config = dev->config;
    uint16_t nstart = (start << 8 ) | (start >> 8);
    return i2c_write_read(data->i2c, AZOTEQ_IQS5XX_ADDR, &nstart, sizeof(nstart), read_buf, len);
}

/**
 * @brief Write to the iqs550 chip via i2c
 * example: iqs5xx_write(dev, GestureEvents0_adr, buffer, 44)
 * @param dev Pointer to device driver MASTER
 * @param start address of the i2c slave
 * @param buf Buffer to be written
 * @param len number of bytes to write
 * @return int
 */
static int iqs5xx_write(const struct device *dev, const uint16_t start_addr, uint8_t *buf,
                        uint32_t num_bytes) {

    const struct iqs5xx_data *data = dev->data;

    uint8_t addr_buffer[2];
    struct i2c_msg msg[2];

    addr_buffer[1] = start_addr & 0xFF;
    addr_buffer[0] = start_addr >> 8;
    msg[0].buf = addr_buffer;
    msg[0].len = 2U;
    msg[0].flags = I2C_MSG_WRITE;

    msg[1].buf = (uint8_t *)buf;
    msg[1].len = num_bytes;
    msg[1].flags = I2C_MSG_WRITE | I2C_MSG_STOP;

    int err = i2c_transfer(data->i2c, msg, 2, AZOTEQ_IQS5XX_ADDR);
    return err;
}

static int iqs5xx_attr_set(const struct device *dev, enum sensor_channel chan,
                           enum sensor_attribute attr, const struct sensor_value *val) {
	LOG_ERR("\nSetting attributes\n");
    const struct iqs5xx_config *config = dev->config;
    return 0;
}

/**
 * @brief Read data from IQS5XX
*/
static int iqs5xx_sample_fetch (const struct device *dev) {
    uint8_t buffer[44];
    int res = iqs5xx_seq_read(dev, GestureEvents0_adr, buffer, 44);
	iqs5xx_write(dev, END_WINDOW, 0, 1);
    if (res < 0) {
        //LOG_ERR("\ntrackpad res: %d", res);
        return res;
    }

    struct iqs5xx_data *data = dev->data;
    
    // Gestures
    data->raw_data.gestures0 =      buffer[0];
    data->raw_data.gestures1 =      buffer[1];
    // System info
    data->raw_data.system_info0 =   buffer[2];
    data->raw_data.system_info1 =   buffer[3];
    // Number of fingers
    data->raw_data.finger_count =   buffer[4];
    // Relative X position
    data->raw_data.rx =             buffer[5] << 8 | buffer[6];
    // Relative Y position
    data->raw_data.ry =             buffer[7] << 8 | buffer[8];

    // Fingers
    for(int i = 0; i < 5; i++) {
        const int p = 9 + (7 * i);
        // Absolute X
        data->raw_data.fingers[i].ax = buffer[p + 0] << 8 | buffer[p + 1];
        // Absolute Y
        data->raw_data.fingers[i].ay = buffer[p + 2] << 8 | buffer[p + 3];
        // Touch strength
        data->raw_data.fingers[i].strength = buffer[p + 4] << 8 | buffer[p + 5];
        // Area
        data->raw_data.fingers[i].area= buffer[p + 6];
    }
    return 0;
}

static void iqs5xx_thread(void *arg, void *unused2, void *unused3) {
    const struct device *dev = arg;
	ARG_UNUSED(unused2);
	ARG_UNUSED(unused3);
    struct iqs5xx_data *data = dev->data;
    struct iqs5xx_config *conf = dev->config;
    int err = 0;

    // Initialize device registers - may be overwritten later in trackpad.c
    struct iqs5xx_reg_config iqs5xx_registers = iqs5xx_reg_config_default();

    err = iqs5xx_registers_init(dev, &iqs5xx_registers);
    if(err) {
        LOG_ERR("Failed to initialize IQS5xx registers!\r\n");
    }

    int nstate = 0;
    while (1) {
        // Sleep for maximum possible time to maximize processor time for other tasks
        #ifdef CONFIG_IQS5XX_POLL
            
            k_msleep(4);

            // Poll data ready pin
            nstate = gpio_pin_get(conf->dr_port, conf->dr_pin);

            if(nstate) {
                // Fetch the sample
                iqs5xx_sample_fetch(dev);
                
                // Trigger
                if(data->data_ready_handler != NULL) {
                    data->data_ready_handler(dev, &data->raw_data);
                }
            }
        #elif CONFIG_IQS5XX_INTERRUPT
            k_sem_take(&data->gpio_sem, K_FOREVER);
            
            iqs5xx_sample_fetch(dev);
            // Trigger 
            if(data->data_ready_handler != NULL) {
                data->data_ready_handler(dev, &data->raw_data);
            }

        #endif
    }
}

/**
 * @brief Sets the trigger handler 
*/
int iqs5xx_trigger_set(const struct device *dev, iqs5xx_trigger_handler_t handler) {
    struct iqs5xx_data *data = dev->data;
    data->data_ready_handler = handler;
    return 0;
}

/**
 * @brief Called when data ready pin goes active. Releases the semaphore allowing thread to run.
 * 
 * @param dev 
 * @param cb 
 * @param pins 
 */
static void iqs5xx_callback(const struct device *dev, struct gpio_callback *cb, uint32_t pins) {
    struct iqs5xx_data *data = CONTAINER_OF(cb, struct iqs5xx_data, dr_cb);
    struct iqs5xx_config *config = data->dev->config;
    k_sem_give(&data->gpio_sem);
}

/**
 * @brief Sets registers to initial values
 * 
 * @param dev 
 * @return >0 if error
 */
int iqs5xx_registers_init (const struct device *dev, const struct iqs5xx_reg_config *config) {
    // TODO: Retry if error on write

    struct iqs5xx_config *conf = dev->config;
    // Wait for dataready?
    while(!gpio_pin_get(conf->dr_port, conf->dr_pin)) {
        k_msleep(1);
    }

    int err = 0;

    // 16 or 32 bit values must be swapped to big endian
    uint8_t wbuff[16];

    // Set active refresh rate
    *((uint16_t*)wbuff) = SWPEND16(config->activeRefreshRate);
    err |= iqs5xx_write(dev, ActiveRR_adr, wbuff, 2);
    // Set idle refresh rate
    *((uint16_t*)wbuff) = SWPEND16(config->idleRefreshRate);
    err |= iqs5xx_write(dev, IdleRR_adr, wbuff, 2);

    // Set single finger gestures
    err |= iqs5xx_write(dev, SFGestureEnable_adr, &config->singleFingerGestureMask, 1);
    // Set multi finger gestures
    err |= iqs5xx_write(dev, MFGestureEnable_adr, &config->multiFingerGestureMask, 1);

    // Set tap time
    *((uint16_t*)wbuff) = SWPEND16(config->tapTime);
    err |= iqs5xx_write(dev, TapTime_adr, wbuff, 2);

    // Set tap distance
    *((uint16_t*)wbuff) = SWPEND16(config->tapDistance);
    err |= iqs5xx_write(dev, TapDistance_adr, wbuff, 2);

    // Set touch multiplier
    err |= iqs5xx_write(dev, GlobalTouchSet_adr, &config->touchMultiplier, 1);

    // Set debounce settings
    err |= iqs5xx_write(dev, ProxDb_adr, &config->debounce, 1);
    err |= iqs5xx_write(dev, TouchSnapDb_adr, &config->debounce, 1);

    // Set noise reduction
    err |= iqs5xx_write(dev, ND_ENABLE, 1, 1);

    // Set i2c timeout
    err |= iqs5xx_write(dev, I2CTimeout_adr, &config->i2cTimeout, 1);

    // Set filter settings
    err |= iqs5xx_write(dev, FilterSettings0_adr, &config->filterSettings, 1);
    err |= iqs5xx_write(dev, DynamicBottomBeta_adr, &config->filterDynBottomBeta, 1);
    err |= iqs5xx_write(dev, DynamicLowerSpeed_adr, &config->filterDynLowerSpeed, 1);
    *((uint16_t*)wbuff) = SWPEND16(config->filterDynUpperSpeed);
    err |= iqs5xx_write(dev, DynamicUpperSpeed_adr, wbuff, 2);

    // Set initial scroll distance
    *((uint16_t*)wbuff) = SWPEND16(config->initScrollDistance);
    err |= iqs5xx_write(dev, ScrollInitDistance_adr, wbuff, 2);

    
    // Terminate transaction
    iqs5xx_write(dev, END_WINDOW, 0, 1);

    return err;
}

static int iqs5xx_init(const struct device *dev) {
    struct iqs5xx_data *data = dev->data;
    const struct iqs5xx_config *config = dev->config;
    int err = 0;

    data->dev = dev;
    
    // Configure data ready pin
	gpio_pin_configure(config->dr_port, config->dr_pin, GPIO_INPUT | config->dr_flags);

    #if CONFIG_IQS5XX_INTERRUPT

    // Blocking semaphore as a flag for sensor read
    k_sem_init(&data->gpio_sem, 0, UINT_MAX);
    
    // Initialize interrupt callback
    gpio_init_callback(&data->dr_cb, iqs5xx_callback, BIT(config->dr_pin));
    // Add callback
	err = gpio_add_callback(config->dr_port, &data->dr_cb);

    // Configure data ready interrupt
    err = gpio_pin_interrupt_configure(config->dr_port, config->dr_pin, GPIO_INT_EDGE_TO_ACTIVE);
    #endif

    return 0;
}


static struct iqs5xx_data iqs5xx_data = {
    .i2c = DEVICE_DT_GET(DT_BUS(DT_DRV_INST(0))),
    .data_ready_handler = NULL
};

static const struct iqs5xx_config iqs5xx_config = {
    .dr_port = DEVICE_DT_GET(DT_GPIO_CTLR(DT_DRV_INST(0), dr_gpios)),
    .dr_pin = DT_INST_GPIO_PIN(0, dr_gpios),
    .dr_flags = DT_INST_GPIO_FLAGS(0, dr_gpios),
};

DEVICE_DT_INST_DEFINE(0, iqs5xx_init, NULL, &iqs5xx_data, &iqs5xx_config,
                      POST_KERNEL, CONFIG_APPLICATION_INIT_PRIORITY, NULL);

K_THREAD_DEFINE(thread, 1024, iqs5xx_thread, DEVICE_DT_GET(DT_DRV_INST(0)), NULL, NULL, K_PRIO_COOP(10), 0, 0);