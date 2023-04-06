/*
 * Copyright (c) 2021 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#define DT_DRV_COMPAT zmk_behavior_mouse_sensitivity

#include <device.h>
#include <drivers/behavior.h>
#include <logging/log.h>
#include <kernel.h>

#include <zmk/config.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

K_SEM_DEFINE(config_saver_sem, 0, 1);

static int behavior_mouse_sensitivity_init(const struct device *dev) { return 0; }

static void save_mouse_sensitivity_thread (void* unused1, void* unused2, void* unused3) {
    while(1) {
        k_sem_take(&config_saver_sem, K_FOREVER);

        if(zmk_config_write(ZMK_CONFIG_KEY_MOUSE_SENSITIVITY) != 0) {
            LOG_ERR("Failed to write mouse sensitivity!");
        }
        k_msleep(1000);
    }
}

// 512 stack size crashes?
K_THREAD_DEFINE(t_save_mouse_sens, 1024, save_mouse_sensitivity_thread, NULL, NULL, NULL, K_PRIO_PREEMPT(10), 0, 0);

static void save_mouse_sensitivity_func () {
    // Write config
    k_sem_give(&config_saver_sem);
}

K_TIMER_DEFINE(save_mouse_sensitivity_timer, save_mouse_sensitivity_func, NULL);

static int on_keymap_binding_pressed(struct zmk_behavior_binding *binding,
                                     struct zmk_behavior_binding_event event) {
    LOG_DBG("position %d keycode 0x%02X", event.position, binding->param1);
    const struct device *dev = device_get_binding(binding->behavior_dev);

    struct zmk_config_field *conf = zmk_config_get(ZMK_CONFIG_KEY_MOUSE_SENSITIVITY);
    if(conf != NULL) {
        uint8_t *val = (uint8_t*)conf->data;
        uint8_t old_val = *val;
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
        if(abs((int)old_val - (int)*val) >= 8) {
            // Write config after 5s
            k_timer_start(&save_mouse_sensitivity_timer, K_MSEC(5000), K_NO_WAIT);
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
