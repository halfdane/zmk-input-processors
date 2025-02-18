/*
 * Copyright (c) 2024 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#define DT_DRV_COMPAT zmk_input_processor_tap

#include <drivers/input_processor.h>
#include <zephyr/logging/log.h>
#include "touch_detection.h"

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

struct tap_config {
    union {
        struct touch_detection_config touch_detection_config;
    } touch_detection;

    uint8_t timeout;
};

struct tap_data {
    union {
        struct touch_detection_data touch_detection_data;
    } touch_detection;
    
    bool touching;
    const struct device *dev;
    struct k_work_delayable tap_timeout_work;
};

static int handle_touch_start(const struct device *dev, struct input_event *event, uint32_t param1,
                               uint32_t param2, struct zmk_input_processor_state *state) {
    struct tap_data *data = (struct tap_data *)dev->data;
    struct tap_config *config = (struct tap_config *)dev->config;

    data->touching=true;

    LOG_DBG("Scheduling tap detection in %d ms", config->timeout);
    k_work_reschedule(&data->tap_timeout_work, K_MSEC(config->timeout));

    return 0;
}

static int handle_touch_end(const struct device *dev) {
    struct tap_data *data = (struct tap_data *)dev->data;
    data->touching = false;
    return 0;
}
 
/* Work Queue Callback */
static void tap_timeout_callback(struct k_work *work) {
    struct k_work_delayable *d_work = k_work_delayable_from_work(work);
    const struct device *dev = DEVICE_DT_INST_GET(0);
    struct tap_data *data = (struct tap_data *)dev->data;
    
    if (!data->touching) {
        LOG_DBG("tap detected - sending button presses");
        zmk_hid_mouse_button_press(0);
        zmk_usb_hid_send_mouse_report();    
        zmk_hid_mouse_button_release(0);
        zmk_usb_hid_send_mouse_report();
    }
}


static int tap_init(const struct device *dev) {
    struct tap_data *data = (struct tap_data *)dev->data;
    struct tap_config *config = (struct tap_config *)dev->config;

    k_work_init_delayable(&data->tap_timeout_work, tap_timeout_callback);

    prepare_touch_detection_config(dev);

    return 0;
}

static const struct zmk_input_processor_driver_api tap_driver_api = {
    .handle_event = touch_detection_handle_event,
};

#define TAP_INST(n)                                                                                 \
    static const struct tap_config processor_tap_config_##n = {                                     \
        .timeout = DT_INST_PROP_OR(n, timeout, 50),                                                 \
        .touch_detection = {                                                                        \
            .touch_detection_config = {                                                             \
                .time_between_normal_reports = DT_INST_PROP_OR(n, time_between_normal_reports, 30), \
                .touch_start = &handle_touch_start,                                                 \
                .touch_end = &handle_touch_end,                                                     \
            },                                                                                      \
        },                                                                                          \
    };                                                                                              \
    static struct tap_data processor_tap_data_##n = {                                               \
        .touching = false,                                                                          \
        .touch_detection = {                                                                        \
            .touch_detection_data = {},                                                                                      \
        },                                                                                          \
    };                                                                                              \
    DEVICE_DT_INST_DEFINE(n, tap_init, NULL, &processor_tap_data_##n,                               \
                          &processor_tap_config_##n, POST_KERNEL,                                   \
                          CONFIG_INPUT_PINNACLE_INIT_PRIORITY, &tap_driver_api);

DT_INST_FOREACH_STATUS_OKAY(TAP_INST)