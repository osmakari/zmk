/*
    Manages receiving and sending configurations via BT/USB HID
*/
#pragma once

#include <stdint.h>
#include <stdlib.h>

// Maximum report size including 1 byte of report ID
#define ZMK_CONTROL_REPORT_SIZE     0x20

// Dynamically allocated output buffer
extern uint8_t *_zmk_control_input_buffer;
extern int _zmk_control_input_buffer_size;

// Commands
enum zmk_control_cmd_t {
    // Invalid/reserved command
    ZMK_CONTROL_CMD_INVALID =       0x00,
    // Connection checking command
    ZMK_CONTROL_CMD_CONNECT =       0x01,

    // Sets a configuration value
    ZMK_CONTROL_CMD_SET_CONFIG =    0x11,
    // Gets a configuration value
    ZMK_CONTROL_CMD_GET_CONFIG =    0x12
    
};

// Package header
// TODO: Do the messages need header on every chunk? large overhead but safer transfer...
struct __attribute__((packed)) zmk_control_msg_header {
    // Report ID (should always be ZMK_REPORT_ID_CONTROL)
    uint8_t report_id; // Not received for some reason?
    // Command (enum zmk_control_cmd_t)
    uint8_t cmd;
    // Total message size
    uint16_t size;
    // Chunk size (actual message size)
    uint8_t chunk_size;
    // Message chunk offset
    uint16_t chunk_offset;
    // CRC8
    uint8_t crc;
};

// Size of the message without header
#define ZMK_CONTROL_REPORT_DATA_SIZE (ZMK_CONTROL_REPORT_SIZE - sizeof(struct zmk_control_msg_header))

// Set config message structure
struct __attribute__((packed)) zmk_control_msg_set_config {
    // Config key. config.h/enum zmk_config_key
    uint16_t key;
    // Config size
    uint16_t size;
    // Is the data to be saved to NVS
    uint8_t save;
    // Data. Represented as u8 but will be allocated to contain variable of length "size"
    uint8_t data;
};

// Get config message structure
struct __attribute__((packed)) zmk_control_msg_get_config {
    // Config key. config.h/enum zmk_config_key
    uint16_t key;
    // Config size
    uint16_t size;
    // Data. Represented as u8 but will be allocated to contain variable of length "size"
    uint8_t data;
};

int zmk_control_parse (uint8_t *buffer, size_t len);