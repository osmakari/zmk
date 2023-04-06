/**
 * @file trackpad.c
 * @author Oskari Seppä
 * @brief File to handle iqs5xx data
 * @version 0.1
 * @date 2022-06-18
 * 
 * @copyright Copyright (c) 2022
 * 
 */
#include <device.h>
#include <init.h>
#include <drivers/sensor.h>
#include <iqs5xx.h>
#include <logging/log.h>
#include <devicetree.h>
#include <math.h>
#include <zmk/config.h>

LOG_MODULE_DECLARE(azoteq_iqs5xx, CONFIG_ZMK_LOG_LEVEL);

// Time in ms to release left click after the gesture
#define TRACKPAD_LEFTCLICK_RELEASE_TIME     50
// Time in ms to release right click after the gesture
#define TRACKPAD_RIGHTCLICK_RELEASE_TIME    50
// Time in ms to release right click after the gesture
#define TRACKPAD_MIDDLECLICK_RELEASE_TIME    50
// Minimum distance to travel until a report is sent
#define SCROLL_REPORT_DISTANCE              35

// Time in ms when three fingers are considered to be tapped
#define TRACKPAD_THREE_FINGER_CLICK_TIME    300

static bool isHolding = false;

//LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

static const struct device *trackpad;

// Input active flag
static bool inputEventActive = false;

static uint8_t lastFingerCount = 0;

static int64_t threeFingerPressTime = 0;

static int16_t lastXScrollReport = 0;

static bool threeFingersPressed = false;

#define MOUSE_MAX_SENSITIVITY     2

static uint8_t mouseSensitivity = 128;

struct iqs5xx_reg_config trackpad_registers;

struct {
    float x;
    float y;
} accumPos;

/**
 * @brief Called when `trackpad_registers` is updated via zmk_control/zmk_config
 * 
 * @param field 
 */
void trackpad_config_on_update (struct zmk_config_field *field) {
    // Send new register values to the device
    int err = iqs5xx_registers_init(field->device, &trackpad_registers);
    if(err) {
        LOG_ERR("Failed to refresh IQS5xx registers!\r\n");
    }
}

struct k_timer leftclick_release_timer;
static void trackpad_leftclick_release () {
    zmk_hid_mouse_button_release(0);
    zmk_endpoints_send_mouse_report();
    inputEventActive = false;
}
K_TIMER_DEFINE(leftclick_release_timer, trackpad_leftclick_release, NULL);

struct k_timer rightclick_release_timer;
static void trackpad_rightclick_release () {
    zmk_hid_mouse_button_release(1);
    zmk_endpoints_send_mouse_report();
    inputEventActive = false;
}
K_TIMER_DEFINE(rightclick_release_timer, trackpad_rightclick_release, NULL);

struct k_timer middleclick_release_timer;
static void trackpad_middleclick_release () {
    zmk_hid_mouse_button_release(2);
    zmk_endpoints_send_mouse_report();
    inputEventActive = false;
}
K_TIMER_DEFINE(middleclick_release_timer, trackpad_middleclick_release, NULL);


static inline void trackpad_leftclick () {
    if(isHolding)  {
        zmk_hid_mouse_button_release(0);
        isHolding = false;
        inputEventActive = false;
    } else {
        if(inputEventActive)
            return;

        zmk_hid_mouse_button_press(0);
        k_timer_start(&leftclick_release_timer, K_MSEC(TRACKPAD_LEFTCLICK_RELEASE_TIME), K_NO_WAIT);
        inputEventActive = true;
    }
}

static inline void trackpad_rightclick () {
    if(inputEventActive)
        return;
    zmk_hid_mouse_button_press(1);
    k_timer_start(&rightclick_release_timer, K_MSEC(TRACKPAD_RIGHTCLICK_RELEASE_TIME), K_NO_WAIT);
    inputEventActive = true;
}

static inline void trackpad_middleclick () {
    LOG_ERR("SEND MIDDLECLICK");
    if(inputEventActive)
        return;
    zmk_hid_mouse_button_press(2);
    k_timer_start(&middleclick_release_timer, K_MSEC(TRACKPAD_MIDDLECLICK_RELEASE_TIME), K_NO_WAIT);
    inputEventActive = true;
}


static inline void trackpad_tap_and_hold(bool g) {
    if (!isHolding && g) {
        LOG_ERR("!isHolding and G = true");
        zmk_hid_mouse_button_press(0);
        isHolding = true;
    }
}



// Sensor trigger handler
static void trackpad_trigger_handler(const struct device *dev, const struct iqs5xx_rawdata *data) {


    bool multiTouch = false;
    
    if(data->finger_count > 1) {
        multiTouch = true;
    }

    bool hasGesture = false;

    if(data->finger_count == 3 && !threeFingersPressed) {
        threeFingerPressTime = k_uptime_get();
        threeFingersPressed = true;
    }
    
    if(data->finger_count == 0) {
        accumPos.x = 0;
        accumPos.y = 0;
        if(threeFingersPressed && k_uptime_get() - threeFingerPressTime < TRACKPAD_THREE_FINGER_CLICK_TIME) {
            hasGesture = true;
            //middleclick
            trackpad_middleclick();
            zmk_hid_mouse_movement_set(0,0);
        }
        threeFingersPressed = false;
    }

    if(data->finger_count != 2) {
        // Reset scroll
        zmk_hid_mouse_scroll_set(0, 0);
    }

    // Check if any gesture exists
    if((data->gestures0 || data->gestures1) && !hasGesture) {
        switch(data->gestures1) {
            case GESTURE_TWO_FINGER_TAP:
                hasGesture = true;
                // Right click
                trackpad_rightclick();
                zmk_hid_mouse_movement_set(0,0);
                break;                   
            case GESTURE_SCROLLG:
                hasGesture = true;
                lastXScrollReport += data->rx;
                // Pan can be always reported
                int8_t pan = -data->ry;
                // Report scroll only if a certain distance has been travelled
                int8_t scroll = 0;
                if(abs(lastXScrollReport) - (int16_t)SCROLL_REPORT_DISTANCE > 0) {
                    scroll = lastXScrollReport >= 0 ? 1 : -1;
                    lastXScrollReport = 0;
                }
                zmk_hid_mouse_scroll_set(pan, scroll);
                zmk_hid_mouse_movement_set(0,0);
                //k_msleep(10);
                break;
        }
        switch(data->gestures0) {
            case GESTURE_SINGLE_TAP:
                // Left click
                hasGesture = true;
                trackpad_leftclick();
                zmk_hid_mouse_movement_set(0,0);
                break;
            case GESTURE_TAP_AND_HOLD:
                //drag n drop
                trackpad_tap_and_hold(true);
                zmk_hid_mouse_movement_set(0,0);
                isHolding = true;
        }
    }
    
    //check for tap and hold release

    bool inputMoved = false;

    if(!hasGesture) {
        // No gesture, can send mouse delta position
        if(data->finger_count == 1) {
            float sensMp = (float)mouseSensitivity/128.0F;
            accumPos.x += -data->ry * sensMp;
            accumPos.y += data->rx * sensMp;
            int16_t xp = accumPos.x;
            int16_t yp = accumPos.y;

            uint8_t updatePos = 0;
            if(fabsf(accumPos.x) >= 1) {
                updatePos = 1;
                accumPos.x = 0;
            }
            if(fabsf(accumPos.y) >= 1) {
                updatePos = 1;
                accumPos.y = 0;
            }
            if(updatePos) {
                zmk_hid_mouse_movement_set(xp, yp);
                inputMoved = true;
            }
        }
    }

    lastFingerCount = data->finger_count;

    if(inputMoved || hasGesture) {
        // Send mouse report
        zmk_endpoints_send_mouse_report();
    }
}

static int trackpad_init(const struct device *_arg) {
    trackpad = DEVICE_DT_GET_ANY(azoteq_iqs5xx);
    if (trackpad == NULL) {
        LOG_ERR("Failed to get IQS5XX device");
        return -EINVAL;
    }
    // Bind mouse sensitivity
    if(zmk_config_bind(ZMK_CONFIG_KEY_MOUSE_SENSITIVITY, &mouseSensitivity, sizeof(mouseSensitivity), true, NULL, trackpad) == NULL) {
        LOG_ERR("Failed to bind mouse sensitivity");
    }

    // Initialize default registers, will be overwritten if saved
    trackpad_registers = iqs5xx_reg_config_default();
    // Bind config - mark saveable
    if(zmk_config_bind(ZMK_CONFIG_CUSTOM_IQS5XX_REGS, &trackpad_registers, sizeof(struct iqs5xx_reg_config), true, trackpad_config_on_update, trackpad) == NULL) {
        LOG_ERR("Failed to bind trackpad config");
    }
    
    accumPos.x = 0;
    accumPos.y = 0;

    int err = 0;
    err = iqs5xx_trigger_set(trackpad, trackpad_trigger_handler);
    if(err) {
        
        return -EINVAL;
    }

    return 0;
}

SYS_INIT(trackpad_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);