#include "../hdl/hdl.h"
#include "compiled/hdl-compiled.h"
#include "../drivers/epd/il0323n.h"
#include <zmk/config.h>
#include <zmk/battery.h>
#include <zmk/usb.h>
#include <kernel.h>
#include <device.h>
#include <logging/log.h>
#include <string.h>
#include <time.h>
#include <math.h>

#if IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
// Central
#include <zmk/split/bluetooth/central.h>

#elif IS_ENABLED(CONFIG_ZMK_SPLIT)
// Peripheral
#include <zmk/split/bluetooth/peripheral.h>
#endif
#include <zmk/keymap.h>
#include <zmk/ble.h>

LOG_MODULE_REGISTER(hdldisp, CONFIG_DISPLAY_LOG_LEVEL);

#define HDL_DATA_MAX_SIZE    1024

// Display device
static const struct device *display;
// Interface object
struct HDL_Interface interface;
// Default font
extern const char HDL_FONT[2048];
// HDL data
uint8_t HDL_DATA[HDL_DATA_MAX_SIZE];
// HDL data length
uint16_t HDL_DATA_LEN = 0;
// HDL initialized
uint8_t hdl_initialized = 0;
// HDL build mutex
K_MUTEX_DEFINE(hdl_mutex);

enum dsp_view {
    VIEW_MAIN,
    VIEW_SLEEP
};

// *******************************
// ZMK_CONFIG_KEY_DATETIME field
// *******************************
// Received timestamp field
struct __attribute__((packed)) {
    int32_t timestamp;
    int32_t offset;
} conf_time;
// Actual updated timestamp
int conf_time_timestamp = 0;
uint64_t _conf_time_last_update = 0;

// Bound values
struct {

    // View
    enum dsp_view view;
    // Battery percentage
    uint8_t batt_percent;
    // Battery sprite
    uint8_t batt_sprite;
    // Charging
    uint8_t charge;
    // RSSI
    int8_t rssi;

    uint16_t sensitivity;

    uint8_t layer;
    uint8_t btProfile;
    uint8_t splitConnected;
    
    // Time and date
    uint8_t hasTime;
    
    uint8_t hours;
    uint8_t minutes;

    uint16_t year;
    uint8_t month;
    uint8_t day;
    
    // 0 = monday
    uint8_t weekDay;

} dsp_binds;

// Refresh clock texts
void conf_time_refresh () {
    if(conf_time.timestamp == 0) {
        dsp_binds.hours = 0;
        dsp_binds.minutes = 0;
        dsp_binds.year = 0;
        dsp_binds.month = 0;
        dsp_binds.day = 0;
        dsp_binds.weekDay = 0;
        dsp_binds.hasTime = 0;
    }
    else {
        uint64_t nclock = k_uptime_get();
        conf_time_timestamp = (conf_time.timestamp + conf_time.offset) + ((nclock - _conf_time_last_update)/1000);
        time_t tmr = conf_time_timestamp;

        struct tm *_tm = localtime(&tmr);
        dsp_binds.hours = _tm->tm_hour;
        dsp_binds.minutes = _tm->tm_min;
        dsp_binds.year = _tm->tm_year + 1900;
        dsp_binds.month = _tm->tm_mon + 1;
        dsp_binds.day = _tm->tm_mday;
        dsp_binds.weekDay = _tm->tm_wday;
        dsp_binds.hasTime = 1;
    }
}

// Time received via zmk_control callback
void conf_time_updated () {
    _conf_time_last_update = k_uptime_get();
    conf_time_refresh();
}

// Display received via zmk_control callback
void conf_display_updated () {

    k_mutex_lock(&hdl_mutex, K_FOREVER);

    struct HDL_Header *hdr = (struct HDL_Header*)HDL_DATA;

    // Get file size from HDL file
    HDL_DATA_LEN = hdr->fileSize;

    if(HDL_DATA_LEN == 0) {
        // Set default data
        memcpy(HDL_DATA, HDL_PAGE_display_right_c, HDL_PAGE_SIZE_display_right_c);
        HDL_DATA_LEN = HDL_PAGE_SIZE_display_right_c;
    }
    
    
    // Free earlier interface
    HDL_Free(&interface);

    HDL_Build(&interface, HDL_DATA, HDL_DATA_LEN);

    if(hdl_initialized) {
        HDL_ForceUpdate(&interface);
        il0323_hibernate(display);
    }

    k_mutex_unlock(&hdl_mutex);

}

// Set sleep view
void display_set_sleep () {
    dsp_binds.view = VIEW_SLEEP;
    HDL_ForceUpdate(&interface);
    il0323_hibernate(display);
}

// Sprite offset for battery icons
#define SPRITES_OFFSET_BATTERY  9
#define SPRITES_BATTERY_CHARGE  SPRITES_OFFSET_BATTERY + 6
#define SPRITES_BATTERY_EMPTY   SPRITES_OFFSET_BATTERY + 4
#define SPRITES_BATTERY_1_4     SPRITES_OFFSET_BATTERY + 3
#define SPRITES_BATTERY_2_4     SPRITES_OFFSET_BATTERY + 2
#define SPRITES_BATTERY_3_4     SPRITES_OFFSET_BATTERY + 1
#define SPRITES_BATTERY_FULL    SPRITES_OFFSET_BATTERY + 0


// Battery sprite count
#define SPRITES_BATTERY_COUNT   5

// Updates battery sprite index 
void update_battery_sprite () {
    if(zmk_usb_is_powered()) {
        // Set charge / sprite
        dsp_binds.charge = 1;
        dsp_binds.batt_sprite = SPRITES_BATTERY_CHARGE;
    }
    else {
        // Set correct battery percentage
        uint8_t b = dsp_binds.batt_percent;
        dsp_binds.charge = 0;
        if(b <= 10) {
            dsp_binds.batt_sprite = SPRITES_BATTERY_EMPTY;
        }
        else if(b > 10 && b <= 30) {
            dsp_binds.batt_sprite = SPRITES_BATTERY_1_4;
        }
        else if(b > 30 && b <= 60) {
            dsp_binds.batt_sprite = SPRITES_BATTERY_2_4;
        }
        else if(b > 60 && b <= 85) {
            dsp_binds.batt_sprite = SPRITES_BATTERY_3_4;
        }
        else if(b > 85) {
            dsp_binds.batt_sprite = SPRITES_BATTERY_FULL;
        }
    }
}

// Interface for clearing an area on the display
void dsp_clear (int16_t x, int16_t y, uint16_t w, uint16_t h) {
    il0323_clear_area(display, x, y, w, h);
}

// Interface for triggering render on the display
void dsp_render (int16_t x, int16_t y, uint16_t w, uint16_t h) {
    il0323_refresh(display, x, y, w, h);
}

// Interface for drawing single pixel on the display
void dsp_pixel (int16_t x, int16_t y) {
    il0323_set_pixel(display, x, y);
}

// Interface for drawing horizontal line on the display
void dsp_hline (int16_t x, int16_t y, int16_t len) {
    il0323_h_line(display, x, y, len);
}

// Interface for drawing vertical line on the display
void dsp_vline (int16_t x, int16_t y, int16_t len) {
    il0323_v_line(display, x, y, len);
}

#define PI 3.14159265

void dsp_arc(int16_t xc, int16_t yc, int16_t radius, uint16_t start_angle, uint16_t end_angle) {
    int x, y;
    float angle;

    // Iterate through the angles from start_angle to end_angle
    for (angle = start_angle; angle < end_angle; angle += 1) {

        x = xc + radius * cosf(angle * PI / 180.0) - 0.5f;
        y = yc + radius * sinf(angle * PI / 180.0) - 0.5f;

        // Plot the point (x, y)
        il0323_set_pixel(display, x, y);
    }
}

// Font
extern const char HDL_FONT[2048];

// Interface for drawing text on the display
void dsp_text (int16_t x, int16_t y, const char *text, uint8_t fontSize) {

    int len = strlen(text);
    int line = 0;
    int acol = 0;

    for (int g = 0; g < len; g++) {
		// Starting character in single quotes

        if (text[g] == '\n') {
			line++;
			acol = 0;
			continue;
		}
		else if (text[g] == ' ') {
			acol++;
		}
		
		for (int py = 0; py < 8; py++) {
			for (int px = 0; px < 6; px++) {
				if ((HDL_FONT[text[g] * 8 + py] >> (7 - px)) & 1) {
                    int rx = x + (px + acol * 6) * fontSize;
                    int ry = y + (py + line * 8) * fontSize;

                    for(int sy = 0; sy < fontSize; sy++) {
                        for(int sx = 0; sx < fontSize; sx++) {
                            dsp_pixel(rx + sx, ry + sy);
                        }
                    }
				}
			}
		}
		acol++;

    }
}

static void update_display_bindings () {
    // Update battery
    dsp_binds.batt_percent = zmk_battery_state_of_charge();
    update_battery_sprite();
    // Update time
    conf_time_refresh();

    #ifdef CONFIG_ZMK_CONFIG
    struct zmk_config_field *sens = zmk_config_get(ZMK_CONFIG_KEY_MOUSE_SENSITIVITY);
    if(sens) {
        dsp_binds.sensitivity = *(uint8_t*)sens->data;
    }
    #endif



    #if IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
    // Central
    dsp_binds.splitConnected = zmk_split_bt_central_is_connected();

    #elif IS_ENABLED(CONFIG_ZMK_SPLIT)
    // Peripheral
    dsp_binds.splitConnected = zmk_split_bt_peripheral_is_connected();
    
    #endif

    #if IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL) || !IS_ENABLED(CONFIG_ZMK_SPLIT)
    dsp_binds.layer = zmk_keymap_highest_layer_active();
    dsp_binds.btProfile = zmk_ble_active_profile_index();
    #endif

    
}

static void display_thread(void *arg, void *unused2, void *unused3) {
    k_msleep(100);
    while(display == NULL) {
        k_msleep(10000);
        display = DEVICE_DT_GET_ANY(gooddisplay_il0323n);
    }
    // Initialize display registers
    il0323_init_regs(display);

    int err = 0;

    // Init interface
    interface = HDL_CreateInterface(80, 128, HDL_COLORS_MONO, HDL_FEAT_TEXT | HDL_FEAT_LINE_HV | HDL_FEAT_BITMAP);

    // Set interface functions
    interface.f_clear = dsp_clear;
    interface.f_renderPart = dsp_render;
    interface.f_pixel = dsp_pixel;
    interface.f_hline = dsp_hline;
    interface.f_vline = dsp_vline;
    interface.f_text = dsp_text;
    interface.f_arc = dsp_arc;

    // Create bindings
    HDL_SetBinding(&interface, "VIEW",          1, &dsp_binds.view, HDL_TYPE_I8);
    HDL_SetBinding(&interface, "BATT_PERCENT",  2, &dsp_binds.batt_percent, HDL_TYPE_I8);
    HDL_SetBinding(&interface, "BATT_SPRITE",   3, &dsp_binds.batt_sprite, HDL_TYPE_I8);
    HDL_SetBinding(&interface, "CHRG",          4, &dsp_binds.charge, HDL_TYPE_BOOL);
    HDL_SetBinding(&interface, "RSSI",          5, &dsp_binds.rssi, HDL_TYPE_I8);

    HDL_SetBinding(&interface, "SENSITIVITY",   6, &dsp_binds.sensitivity, HDL_TYPE_I16);

    HDL_SetBinding(&interface, "LAYER",         7, &dsp_binds.layer, HDL_TYPE_I8);
    HDL_SetBinding(&interface, "BTPROFILE",     8, &dsp_binds.btProfile, HDL_TYPE_I8);
    HDL_SetBinding(&interface, "SPLITCONNECTED", 9, &dsp_binds.splitConnected, HDL_TYPE_BOOL);

    // Time and date
    HDL_SetBinding(&interface, "HASTIME",       20, &dsp_binds.hasTime, HDL_TYPE_BOOL);
    HDL_SetBinding(&interface, "HOURS",         21, &dsp_binds.hours, HDL_TYPE_I8);
    HDL_SetBinding(&interface, "MINUTES",       22, &dsp_binds.minutes, HDL_TYPE_I8);
    
    HDL_SetBinding(&interface, "YEAR",          23, &dsp_binds.year, HDL_TYPE_I16);
    HDL_SetBinding(&interface, "MONTH",         24, &dsp_binds.month, HDL_TYPE_I8);
    HDL_SetBinding(&interface, "DAY",           25, &dsp_binds.day, HDL_TYPE_I8);
    HDL_SetBinding(&interface, "WEEKDAY",       26, &dsp_binds.weekDay, HDL_TYPE_I8);

    // Add preloaded images
    // Preloaded images' id's must have the MSb as 1 (>0x8000)
    // Normal icons
    err = HDL_PreloadBitmap(&interface, 0x8001, HDL_IMG_kb_icons_bmp_c, HDL_IMG_SIZE_kb_icons_bmp_c);
    // Big icons
    err = HDL_PreloadBitmap(&interface, 0x8002, HDL_IMG_kb_icons_big_bmp_c, HDL_IMG_SIZE_kb_icons_big_bmp_c);

    // Set text width and height, used to center text
    interface.textHeight = 6;
    interface.textWidth = 5;

    // Set automatic update intervals
    // min: 300ms, max: 30s
    HDL_SetUpdateInterval(&interface, 300, 30000);

    // Load data and build
    conf_display_updated();

    hdl_initialized = 1;

    while(1) {

        update_display_bindings();

        k_mutex_lock(&hdl_mutex, K_FOREVER);

        if(HDL_Update(&interface, k_uptime_get()) > 0) {
            il0323_hibernate(display);
        }

        k_mutex_unlock(&hdl_mutex);

        // Sleep for 1sec
        k_msleep(1000);
    }
}

static int display_init () {

    display = DEVICE_DT_GET_ANY(gooddisplay_il0323n);

    memset(HDL_DATA, 0, sizeof(HDL_DATA));

    // Bind timestamp
#if IS_ENABLED(CONFIG_ZMK_CONFIG)
    if(zmk_config_bind(ZMK_CONFIG_KEY_DATETIME, &conf_time, sizeof(conf_time), false, conf_time_updated, display) == NULL) {
        LOG_ERR("Failed to bind timestamp");
    }
    if(zmk_config_bind(ZMK_CONFIG_KEY_DISPLAY_CODE, HDL_DATA, sizeof(HDL_DATA), true, conf_display_updated, display) == NULL) {
        LOG_ERR("Failed to bind display");
    }

#endif
    if (display == NULL) {
        LOG_ERR("Failed to get il0323n device");
        return -EINVAL;
    }

    conf_time.offset = 0;
    conf_time.timestamp = 0;
    // Initialize config/time strings
    conf_time_refresh();
    // Init binds
    dsp_binds.view = VIEW_MAIN;
    dsp_binds.charge = 0;
    dsp_binds.batt_percent = 0;
    dsp_binds.batt_sprite = 0;

    return 0;
}

SYS_INIT(display_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
K_THREAD_DEFINE(display_thr, 4096, display_thread, NULL, NULL, NULL, K_PRIO_PREEMPT(10), 0, 0);