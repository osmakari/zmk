#include <zmk/control.h>
#include <zmk/config.h>
#include <logging/log.h>
#include <string.h>
#include <zmk/endpoints.h>
#include <zmk/usb_hid.h>
#include <zmk/hog.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

uint8_t *data_buffer = NULL;
static uint16_t data_buffer_len = 0;
static uint8_t chunk_recv_len = 0;

struct zmk_control_msg_header data_header;

static struct k_sem data_parse_sem;

/**
 * @brief Set configuration values
 * 
 * @return int 
 */
int zmk_control_set_config (uint8_t *buffer, uint16_t len) {

    struct zmk_control_msg_set_config *conf = buffer;

    struct zmk_config_field *field = zmk_config_get(conf->key);
    if(field == NULL) {
        // Field not found
        LOG_ERR("[Control] Field 0x%04X not found!", conf->key);
        return -1;
    }

    if(field->size != conf->size) {
        // Invalid size
        LOG_ERR("[Control] Field 0x%04X size not correct! (%i received, %i defined)", conf->key, conf->size, field->size);
        return -1;
    }
    
    // Copy data
    memcpy(field->data, &conf->data, field->size);

    // Save field if it's saveable
    if(field->flags & ZMK_CONFIG_FIELD_FLAG_SAVEABLE && conf->save) {
        zmk_config_write(field->key);
    }

    if(field->on_update != NULL) {
        // On update callback
        field->on_update(field);
    }

    return 0;
}

uint8_t *_zmk_control_input_buffer = NULL;
int _zmk_control_input_buffer_size = 0;

/**
 * @brief Get configuration values
 * 
 * @return int 
 */
int zmk_control_get_config (uint8_t *buffer, uint16_t len) {

    struct zmk_control_msg_get_config *request = buffer;

    struct zmk_config_field *field = zmk_config_get(request->key);
    if(field == NULL) {
        // Field not found
        LOG_ERR("[Control] Field 0x%04X not found!", request->key);
        return -1;
    }
    // Maximum size is defined in request
    if(field->size > request->size) {
        // Invalid size
        LOG_ERR("[Control] Field 0x%04X size not correct! (%i received < %i defined)", request->key, request->size, field->size);
        return -1;
    }

    // Free input buffer
    if(_zmk_control_input_buffer != NULL) {
        k_free(_zmk_control_input_buffer);
        _zmk_control_input_buffer = NULL;
        _zmk_control_input_buffer_size = 0;
    }
    // Allocate input buffer
    _zmk_control_input_buffer_size = (sizeof(struct zmk_control_msg_get_config) - 1) + field->size;
    _zmk_control_input_buffer = k_malloc(_zmk_control_input_buffer_size);

    if(_zmk_control_input_buffer == NULL) {
        LOG_ERR("[Control] ERROR: Out of memory!");
        _zmk_control_input_buffer_size = 0;
        return -1;
    }

    int total_size = (sizeof(struct zmk_control_msg_get_config) - 1) + field->size;

    uint8_t in_buffer[32];

    struct zmk_control_msg_header *hdr = (struct zmk_control_msg_header *)in_buffer;
    hdr->report_id = 0x05;
    hdr->cmd = ZMK_CONTROL_CMD_GET_CONFIG;
    hdr->crc = 0;
    hdr->chunk_offset = 0;
    hdr->chunk_size = total_size < ZMK_CONTROL_REPORT_DATA_SIZE ? total_size : ZMK_CONTROL_REPORT_DATA_SIZE;
    hdr->size = total_size;
    
    struct zmk_control_msg_get_config *resp = _zmk_control_input_buffer;
    resp->key = request->key;
    resp->size = field->size;
    memcpy(&resp->data, field->data, field->size);

    switch (zmk_endpoints_selected()) {
//#if IS_ENABLED(CONFIG_ZMK_USB)
        case ZMK_ENDPOINT_USB: {
            uint32_t total = 0;
            // Send in 32 byte chunks
            while(total < _zmk_control_input_buffer_size) {
                uint32_t diff = _zmk_control_input_buffer_size - total;
                // Set header info
                hdr->chunk_offset = total;
                hdr->chunk_size = diff < ZMK_CONTROL_REPORT_DATA_SIZE ? diff : ZMK_CONTROL_REPORT_DATA_SIZE;

                // Copy new data
                memcpy(in_buffer + sizeof(struct zmk_control_msg_header), _zmk_control_input_buffer + total, hdr->chunk_size);
                
                int err = zmk_usb_hid_send_report(in_buffer, ZMK_CONTROL_REPORT_SIZE);
                if (err) {
                    LOG_ERR("FAILED TO SEND OVER USB: %d", err);
                    // break or retry ???
                    break;
                }
                
                //LOG_ERR("Send report %i/%i", total, hdr->size);
                k_msleep(10);
                total += hdr->chunk_size;
            }
            k_free(_zmk_control_input_buffer);
            _zmk_control_input_buffer = NULL;
            _zmk_control_input_buffer_size = 0;
            break;
        }
//#endif /* IS_ENABLED(CONFIG_ZMK_USB) */

#if IS_ENABLED(CONFIG_ZMK_BLE)
        case ZMK_ENDPOINT_BLE: {
            return 0;
            /*
            int err = zmk_hog_send_keyboard_report(&keyboard_report->body);
            if (err) {
                LOG_ERR("FAILED TO SEND OVER HOG: %d", err);
            }
            break;
            */
        }
#endif /* IS_ENABLED(CONFIG_ZMK_BLE) */

        default:
            LOG_ERR("Unsupported endpoint %d", zmk_endpoints_selected());
            return -ENOTSUP;
    }

    return 0;
}

/**
 * @brief Parses a message
 * 
 * @param buffer 
 * @param len 
 * @return int 
 */
int zmk_control_parse (uint8_t *buffer, size_t len) {
    int err = 0;
    if(chunk_recv_len == 0 && len < sizeof(struct zmk_control_msg_header)) {
        // Fail on too short message
        return -1;
    }

    if(data_buffer_len == 0) {
        // First chunk, copy header to memory
        memcpy(&data_header, buffer, sizeof(struct zmk_control_msg_header));
        if(data_header.report_id != 0x05) {
            // Report id must be 0x05
            return -3;
        }
        // Allocate buffer
        if(data_buffer != NULL) {
            free(data_buffer);
            data_buffer = NULL;
        }
        chunk_recv_len = 0;
        data_buffer = malloc(data_header.size);
        if(data_buffer == NULL) {
            LOG_ERR("Out of memory");
            return -1;
        }
        memset(data_buffer, 0, data_header.size);
    }

    size_t cpy_len = 0;
    if(chunk_recv_len == 0) {
        // First part of the chunk
        memcpy(&data_header, buffer, sizeof(struct zmk_control_msg_header));
        cpy_len = data_header.chunk_size <= (len - sizeof(struct zmk_control_msg_header)) ? data_header.chunk_size : (len - sizeof(struct zmk_control_msg_header));
        memcpy(data_buffer + data_buffer_len, buffer + sizeof(struct zmk_control_msg_header), cpy_len);
    }
    else {
        // Full chunk not received, add data
        cpy_len = data_header.chunk_size - chunk_recv_len;
        memcpy(data_buffer + data_buffer_len, buffer, cpy_len);
    }
    data_buffer_len += cpy_len;
    chunk_recv_len += cpy_len;
    if(chunk_recv_len >= ZMK_CONTROL_REPORT_DATA_SIZE) {
        // Chunk ended
        chunk_recv_len = 0;
        // TODO: check CRC
    }

    if(data_buffer_len >= data_header.size) {

        data_buffer_len = 0;
        chunk_recv_len = 0;

        k_sem_give(&data_parse_sem);
    }

    return err;
}

// Thread
static void _zmk_control_thread(void *arg, void *unused2, void *unused3) {
    k_sem_init(&data_parse_sem, 0, UINT32_MAX);
    
    while(1) {

        // Wait until message is ready
        k_sem_take(&data_parse_sem, K_FOREVER);
        LOG_ERR("PARSE MESSAGE\n");
        int err = 0;
        
        // Message ready
        switch((enum zmk_control_cmd_t)data_header.cmd) {
            case ZMK_CONTROL_CMD_CONNECT:

                break;
            case ZMK_CONTROL_CMD_SET_CONFIG:
                err = zmk_control_set_config(data_buffer, data_header.size);
                break;
            case ZMK_CONTROL_CMD_GET_CONFIG:
                LOG_ERR("ZMK_CONTROL_GET_CONFIG");
                err = zmk_control_get_config(data_buffer, data_header.size);
                break;
            case ZMK_CONTROL_CMD_INVALID:
            default:
                // Fail unknown cmd
                err = -2;
                break;
        }

        if(data_buffer) {
            free(data_buffer);
            data_buffer = NULL;
        }
        

    }

}

K_THREAD_DEFINE(zmk_control_thr, 4096, _zmk_control_thread, NULL, NULL, NULL, K_PRIO_PREEMPT(10), 0, 0);