/*
 * Copyright (c) 2021 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#define DT_DRV_COMPAT zmk_behavior_mouse_sensitivity

#include <device.h>
#include <drivers/behavior.h>
#include <logging/log.h>

#include <zmk/config.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

static int behavior_mouse_sensitivity_init(const struct device *dev) { return 0; };

static int on_keymap_binding_pressed(struct zmk_behavior_binding *binding,
                                     struct zmk_behavior_binding_event event) {
    LOG_DBG("position %d keycode 0x%02X", event.position, binding->param1);
    const struct device *dev = device_get_binding(binding->behavior_dev);

    struct zmk_config_field *conf = zmk_config_get(ZMK_CONFIG_KEY_MOUSE_SENSITIVITY);
    if(conf != NULL) {
        uint8_t *val = (uint8_t*)conf->data;
        int8_t dir = (int8_t)binding->param1;
        int nval = (int)*val + (int)dir;
        // Check overflows
        if(nval > 255) {
            *val = 255;
        }
        else if(nval < 1) {
            *val = 1;
        }
        else {
            *val = nval;
        }
    }
    else {
        return 1;
    }
    return 0;
}

static int on_keymap_binding_released(struct zmk_behavior_binding *binding,
                                      struct zmk_behavior_binding_event event) {
    return 0;
}

static const struct behavior_driver_api behavior_mouse_sensitivity_driver_api = {
    .binding_pressed = on_keymap_binding_pressed, .binding_released = on_keymap_binding_released};

struct mouse_config_sens {
    int delay_ms;
}; 

#define KP_INST(n)                                                                          \
    static struct mouse_config_sens behavior_mouse_scroll_config_##n = {                                \
        .delay_ms = DT_INST_PROP(n, delay_ms)                                               \
    };                                                                                      \
                                                                                            \
    DEVICE_DT_INST_DEFINE(n, behavior_mouse_sensitivity_init, NULL, NULL,                   \
                          &behavior_mouse_scroll_config_##n, APPLICATION,                                                \
                          CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &behavior_mouse_sensitivity_driver_api);

DT_INST_FOREACH_STATUS_OKAY(KP_INST)

#endif /* DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT) */
