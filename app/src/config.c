/*
    Manages device configurations in NVS
*/

#include <zmk/config.h>
#include <device.h>
#include <devicetree.h>
#include <logging/log.h>
#include <drivers/flash.h>
#include <storage/flash_map.h>
#include <fs/nvs.h>
#include <string.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

// FS handle
static struct nvs_fs fs;
// Flash info
static struct flash_pages_info flash_info;

#define STORAGE_NODE DT_NODE_BY_FIXED_PARTITION_LABEL(storage)
#define FLASH_NODE DT_MTD_FROM_FIXED_PARTITION(STORAGE_NODE)

// Config initialized flag
static uint8_t _config_initialized = 0;

// Config field buffer
static uint8_t _tmp_buffer[ZMK_CONFIG_MAX_FIELD_SIZE];

// List of read configs
static struct zmk_config_field fields[ZMK_CONFIG_MAX_FIELDS];

struct zmk_config_device_info device_info;

int zmk_config_init () {
    // No need to initialize multiple times
    if(_config_initialized)
        return 0;

    const struct device *flash_dev;

    memset(fields, 0, sizeof(struct zmk_config_field) * ZMK_CONFIG_MAX_FIELDS);
    memset(&device_info, 0, sizeof(struct zmk_config_device_info));

    int err = 0;

    flash_dev = DEVICE_DT_GET(FLASH_NODE);

    if (!device_is_ready(flash_dev)) {
		LOG_ERR("Flash device %s is not ready\n", flash_dev->name);
		return -1;
	}

    fs.offset = FLASH_AREA_OFFSET(storage);
	err = flash_get_page_info_by_offs(flash_dev, fs.offset, &flash_info);
	if (err) {
		LOG_ERR("Unable to get page info\n");
		return -1;
	}
    
    // TODO: maybe 8192B sector size and 4 sectors?
	fs.sector_size = flash_info.size;
	fs.sector_count = 8U;

    err = nvs_init(&fs, flash_dev->name);
	if (err) {
		LOG_ERR("Flash Init failed dev:%s err:%i offset:%i sector:%i sectors:%i", flash_dev->name, err, fs.offset, fs.sector_size, fs.sector_count);
		return -1;
	}
    // Set initialized flag
    _config_initialized = 1;

    // Bind device info
    if(zmk_config_bind(ZMK_CONFIG_KEY_DEVICE_INFO, &device_info, sizeof(device_info), 1, NULL, NULL) == NULL) {
        LOG_ERR("Failed to bind device info");
    }

    return 0;
}

struct zmk_config_field *zmk_config_bind (enum zmk_config_key key, void *data, uint16_t size, uint8_t saveable, void (*update_callback)(struct zmk_config_field*), struct device *device) {
    if(!_config_initialized) {
        if(zmk_config_init() < 0) {
            return NULL;
        }
    }

    int err = 0, len = 0;
    
    // Check if binding exists, can't bind multiple times
    if(zmk_config_get(key) != NULL) {
        LOG_ERR("Can't bind config value twice!");
        return NULL;
    }

    // Push the field
    for(int i = 0; i < ZMK_CONFIG_MAX_FIELDS; i++) {
        if(fields[i].key == ZMK_CONFIG_KEY_INVALID) {
            fields[i].key = key;
            fields[i].data = data;
            fields[i].size = size;
            fields[i].flags = (saveable ? ZMK_CONFIG_FIELD_FLAG_SAVEABLE : 0);
            fields[i].on_update = update_callback;
            fields[i].device = device;
            // Init mutex lock
            k_mutex_init(&fields[i].mutex);

            err = zmk_config_read(key);
            if(err < 0) {
                // Returns error if field does not exist in NVS so this can be ignored for now
            }

            return &fields[i];
        }
    }

    LOG_ERR("Config field array full, increase CONFIG_ZMK_CONFIG_MAX_FIELDS");
    return NULL;
}

struct zmk_config_field *zmk_config_get (enum zmk_config_key key) {
    if(!_config_initialized)
        return NULL;

    for(int i = 0; i < ZMK_CONFIG_MAX_FIELDS; i++) {
        if(fields[i].key == key) {
            return &fields[i];
        }
    }
    return NULL;
}

int zmk_config_read (enum zmk_config_key key) {
    if(!_config_initialized)
        return -1;

    int len = 0;
    struct zmk_config_field *field = zmk_config_get(key);

    // Field not found
    if(field == NULL)
        return -1;

    // Only allow saveable fields to be read
    if((field->flags & ZMK_CONFIG_FIELD_FLAG_SAVEABLE) == 0)
        return -1;
    
    // Update field from NVS
    k_mutex_lock(&field->mutex, K_FOREVER);
    len = nvs_read(&fs, (uint16_t)key, _tmp_buffer, ZMK_CONFIG_MAX_FIELD_SIZE);

    if(len > 0) {
        // Read OK

        // Check field size
        if(field->size == len) {
            // Field size ok, copy new value
            memcpy(field->data, _tmp_buffer, len);
            field->flags |= ZMK_CONFIG_FIELD_FLAG_READ | ZMK_CONFIG_FIELD_FLAG_WRITTEN;
            k_mutex_unlock(&field->mutex);
        }
        else {
            // Unlock mutex, because write locks it again
            k_mutex_unlock(&field->mutex);
            // TODO: replace old value with new one
            if(zmk_config_write(field->key) < 0) {
                field->flags = 0;
                return -1;
            }
        }
        return 0;
    }
    else {
        field->flags &= ~(ZMK_CONFIG_FIELD_FLAG_READ);
        k_mutex_unlock(&field->mutex);
        return -1;
    }


    return -1;
}

int zmk_config_write (enum zmk_config_key key) {
    if(!_config_initialized)    
        return -1;

    int len = 0;
    struct zmk_config_field *field = NULL;
    field = zmk_config_get(key);

    if(field == NULL)
        return -1;

    // Only allow saveable fields to be written
    if((field->flags & ZMK_CONFIG_FIELD_FLAG_SAVEABLE) == 0)
        return -1;

    k_mutex_lock(&field->mutex, K_FOREVER);
    len = nvs_write(&fs, field->key, field->data, field->size);
    k_mutex_unlock(&field->mutex);
    if(len < 0) {
        LOG_ERR("Config failed to write NVS");
        // Clear written flag since it's not written
        field->flags &= ~(ZMK_CONFIG_FIELD_FLAG_WRITTEN);
        return -1;
    }
    field->flags |= ZMK_CONFIG_FIELD_FLAG_READ | ZMK_CONFIG_FIELD_FLAG_WRITTEN;

    return 0;
}

//***********************************
// Config -> keymap transformations
//***********************************
// Move to another file?

// Configuration single bit ID to device name string
const char *CONF_ID_DEVICE[] = {
    "TRANS",                    // 0
    "BCKLGHT",                  // 1
    "BLUETOOTH",                // 2
    "CAPS_WORD",                // 3
    "EXT_POWER",                // 4
    "GRAVE_ESCAPE",             // 5
    "KEY_PRESS",                // 6
    "KEY_REPEAT",               // 7
    "KEY_TOGGLE",               // 8
    "LAYER_TAP",                // 9
    "MAC_TAP",                  // 10
    "MAC_PRESS",                // 11
    "MAC_REL",                  // 12
    "MAC_TAP_TIME",             // 13
    "MAC_WAIT_TIME",            // 14
    "MAC_WAIT_REL",             // 15
    "MOD_TAP",                  // 16
    "MO",                       // 17
    "MOUSE_KEY_PRESS",          // 18
    "MOUSE_MOVE",               // 19
    "MOUSE_SCROLL",             // 20
    "NONE",                     // 21
    "OUTPUTS",                  // 22
    "RESET",                    // 23
    "BOOTLOAD",                 // 24
    "RGB_UG",                   // 25
    "ENC_KEY_PRESS",            // 26
    "STICKY_KEY",               // 27
    "STICKY_LAYER",             // 28
    "TO_LAYER",                 // 29
    "TOGGLE_LAYER",             // 30
};

/**
 * @brief Get keymap device name eg. "KEY_PRESS" from id = 6. id is the index of corresponding CONF_ID_DEVICE row
 * 
 * @param id 
 * @return char* 
 */
char *zmk_config_keymap_device_name (uint8_t id) {
    // Return NULL if not found
    if((id & 0x7F) > sizeof(CONF_ID_DEVICE) / sizeof(const char *))
        return NULL;

    return CONF_ID_DEVICE[(id & 0x7F)];
}

/**
 * @brief Get keymap device id eg. device_name = "KEY_PRESS" returns 6. Inverse of zmk_config_keymap_device_name
 * 
 * @param device_name 
 * @return Device id, -1 on error
 */
int zmk_config_keymap_device_id (char *device_name) {
    
    for(int i = 0; i < sizeof(CONF_ID_DEVICE) / sizeof(const char *); i++) {
        if(strcmp(device_name, CONF_ID_DEVICE[i]) == 0) {
            return i;
        }
    }
    return -1;
}


int zmk_config_keymap_conf_to_binding (struct zmk_behavior_binding *binding, struct zmk_config_keymap_item *item) {
    char *device_name = zmk_config_keymap_device_name(item->device);
    if(device_name == NULL) {
        return -1;
    }

    binding->behavior_dev = device_name;
    binding->param1 = item->param1;
    binding->param2 = item->param2;

    return 0;
}

int zmk_config_keymap_binding_to_conf (struct zmk_behavior_binding *binding, struct zmk_config_keymap_item *item, uint8_t layer, uint16_t key) {
    int id = zmk_config_keymap_device_id(binding->behavior_dev);
    if(id < 0) {
        return -1;
    }
    item->device = (uint8_t)id;
    item->key = (key << 4) | (layer & 0xF);
    item->param1 = binding->param1;
    item->param2 = binding->param2;

    return 0;
}