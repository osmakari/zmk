/*
    Manages device configurations in NVS
*/
#pragma once

#include <kernel.h>
#include <device.h>
#include <stdint.h>
#include <zmk/behavior.h>

#define ZMK_CONFIG_MAX_FIELD_SIZE       CONFIG_ZMK_CONFIG_MAX_FIELD_SIZE
#define ZMK_CONFIG_MAX_FIELDS           CONFIG_ZMK_CONFIG_MAX_FIELDS
// 64 * 11 = 704 bytes 
#define ZMK_CONFIG_MAX_REBOUND_KEYS     64

/**
 * @brief Configuration keys, casted as uint16_t
 * 
 * More fields to be added
 */
enum zmk_config_key {
    // Invalid key
    ZMK_CONFIG_KEY_INVALID =                0x0000,
    // --------------------------------------------------------------
    // 0x0001 - 0x3FFF: (Recommended) saveable fields
    // Fields that should be saved to NVS, such as keymap or mouse sensitivity
    // --------------------------------------------------------------

    // 0x0001 - 0x0009: Device information fields
    // Device info (struct zmk_config_device_info)
    ZMK_CONFIG_KEY_DEVICE_INFO =            0x0001,
    // 0x000A - 0x001F: Device configuration
    // Sleep timeout (u16) (0 = never sleep)
    ZMK_CONFIG_KEY_SLEEP_TIMEOUT =          0x000A,
    // Peripheral sleep timeout (u16) (0 = never sleep)
    ZMK_CONFIG_KEY_PERIPHERAL_SLEEP_TIMEOUT = 0x000B,


    // 0x0020 - 0x003F: Keyboard configurations 
    // Keymap
    ZMK_CONFIG_KEY_KEYMAP =                 0x0020,

    // 0x0040 - 0x005F: Mouse/trackpad configurations
    // Mouse sensitivity (u8)
    ZMK_CONFIG_KEY_MOUSE_SENSITIVITY =      0x0040,
    // Mouse Y scroll sensitivity (u8)
    ZMK_CONFIG_KEY_SCROLL_SENSITIVITY =     0x0041,
    // Mouse X pan sensitivity (u8)
    ZMK_CONFIG_KEY_PAN_SENSITIVITY =        0x0042,
    // Mouse scroll direction (u8)
    ZMK_CONFIG_KEY_SCROLL_DIRECTION =       0x0043,
    // Touchpad click type (u8) (0 = normal, 1 = left click on left side, right click on right side)
    ZMK_CONFIG_KEY_TP_CLICK_TYPE =          0x0044,

    // 0x0060 - 0x007F: Display configurations
    ZMK_CONFIG_KEY_DISPLAY_CODE =           0x0060,

    // --------------------------------------------------------------
    // 0x4000 - 0x7FFF: (Recommended) Non-saved fields
    // Fields that do not require saving to NVS, such as time or date
    // --------------------------------------------------------------

    // (int32_t[2]) [0] Unix timestamp of time, [1] timezone in seconds
    ZMK_CONFIG_KEY_DATETIME =               0x4000,


    // --------------------------------------------------------------
    // 0x8000 - 0xFFFF: Custom fields
    // Fields that should be used if custom fields are needed
    // --------------------------------------------------------------
    
    // hid:ergo device specific fields
    // IQS5XX register configuration
    ZMK_CONFIG_CUSTOM_IQS5XX_REGS =         0x8001,

};

// Flag if this field should be saved to NVS
#define ZMK_CONFIG_FIELD_FLAG_SAVEABLE    BIT(0)
// Flag if this field has been read from NVS
#define ZMK_CONFIG_FIELD_FLAG_READ        BIT(1)
// Flag if this field has been written to NVS
#define ZMK_CONFIG_FIELD_FLAG_WRITTEN     BIT(2)


/**
 * @brief Configuration field
 * 
 */
struct zmk_config_field {
    // Key identifier (enum zmk_config_key)
    uint16_t key;
    // Bit mask of field flags, see ZMK_CONFIG_FIELD_FLAG_* defs
    uint8_t flags;
    // Mutex lock, if needed for thread safety
    struct k_mutex mutex;
    // Device handle
    struct device *device;
    // Callback to be triggered when data is updated via zmk_control
    void (*on_update)(struct zmk_config_field *field);
    // Allocated size of the field in bytes
    uint16_t size;
    // Local data, should be initialized to NULL
    void *data;
};

enum zmk_config_layout {
    ZMK_CONFIG_LAYOUT_UNKNOWN = 0,
    ZMK_CONFIG_LAYOUT_ISO = 1,
    ZMK_CONFIG_LAYOUT_ANSI = 2
};

/**
 * @brief Device info 
 */
struct __attribute__((packed)) zmk_config_device_info {
    char device_name[32];
    char manufacturer[24];
    char product[24];
    char serial[32];
    
    uint8_t layout;

    uint8_t layer_count;
    uint8_t key_count;
};


/**
 * @brief Initializes configs and NVS
 * 
 * @return int 
 */
int zmk_config_init ();

/**
 * @brief Bind a value to config. Automatically reads the value to `data` from NVS when called.
 * 
 * @param key 
 * @param data 
 * @param size 
 * @param saveable
 * @param update_callback
 * @param device
 * @return struct zmk_config_field* 
 */
struct zmk_config_field *zmk_config_bind (enum zmk_config_key key, void *data, uint16_t size, uint8_t saveable, void (*update_callback)(struct zmk_config_field*), struct device *device);

/**
 * @brief Get config field NOTE: does not read from NVS! Use zmk_config_read to update the field
 * 
 * @param key 
 * @return struct zmk_config_field* 
 */
struct zmk_config_field *zmk_config_get (enum zmk_config_key key);

/**
 * @brief Read field from NVS. If the NVS data length doesn't match with local field's size, NVS data will be rewritten from local memory.
 * @param key
 * @return 0 on success, <0 on fail
 */
int zmk_config_read (enum zmk_config_key key);

/**
 * @brief Writes a config field to NVS
 * 
 * @param key 
 * @return int 
 */
int zmk_config_write (enum zmk_config_key key);


//***********************************
// Config -> keymap transformations
//***********************************
// Move to another file?

struct __attribute__((packed)) zmk_config_keymap_item {
    uint16_t key;
    uint8_t device;
    uint32_t param1;
    uint32_t param2;
};


/**
 * @brief Transform config keymap item to zmk_behavior_binding. Sets binding
 * 
 * @param binding This field is altered if corresponding device is found. Should not be NULL!
 * @param item Config item
 * @return 0 on success, -1 on error
 */
int zmk_config_keymap_conf_to_binding (struct zmk_behavior_binding *binding, struct zmk_config_keymap_item *item);

/**
 * @brief Transform zmk_behavior_binding to zmk_config_keymap_item
 * 
 * @param binding 
 * @param item 
 * @return 0 on success, -1 on error
 */
int zmk_config_keymap_binding_to_conf (struct zmk_behavior_binding *binding, struct zmk_config_keymap_item *item, uint8_t layer, uint16_t key);