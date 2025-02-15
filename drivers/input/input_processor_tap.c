/*
 * Copyright (c) 2024 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#define DT_DRV_COMPAT zmk_input_processor_tap

#include <drivers/input_processor.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

struct tap_config {
    uint8_t timeout;
    uint8_t time_between_normal_reports;
};

struct tap_data {
    bool touching;
    const struct device *dev;
    struct k_work_delayable tap_timeout_work;
    struct k_work_delayable touch_end_timeout_work;
    uint32_t last_touch_timestamp;
};

static bool is_shortly_after_previous(uint8_t time_between_normal_reports, int64_t last_tapped, int64_t current_time) {
    const bool is_shortly_after = (last_tapped + time_between_normal_reports) > current_time;
    return is_shortly_after;
}

static int handle_touch_start(struct tap_data *data, struct tap_config *config) {
    data->touching = true;

    LOG_DBG("Scheduling tap detection in %d ms", config->timeout);
    k_work_reschedule(&data->tap_timeout_work, K_MSEC(config->timeout));

    LOG_INF("TOUCH STARTED");
    return 0;
}

static int handle_touch_end(struct tap_data *data, struct tap_config *config) {
    data->touching = false;
    LOG_INF("TOUCH ENDED");
    return 0;
}
 
static int tap_handle_event(const struct device *dev, struct input_event *event, uint32_t param1,
                               uint32_t param2, struct zmk_input_processor_state *state) { 
    const struct tap_config *config = dev->config;
    struct tap_data *data = (struct tap_data *)dev->data;

    uint32_t current_time = k_uptime_get();
    k_work_reschedule(&data->touch_end_timeout_work, K_MSEC(config->time_between_normal_reports));
    
    if (!data->touching && !is_shortly_after_previous(config->time_between_normal_reports, data->last_touch_timestamp, current_time)){
        handle_touch_start(data, config);
    }

    data->last_touch_timestamp = current_time;

    return ZMK_INPUT_PROC_CONTINUE;
}

/* Work Queue Callback */
static void tap_timeout_callback(struct k_work *work) {
    struct k_work_delayable *d_work = k_work_delayable_from_work(work);
    const struct device *dev = DEVICE_DT_INST_GET(0);
    struct tap_data *data = (struct tap_data *)dev->data;
    
    if (!data->touching) {
        LOG_INF("tap detected - sending button presses");
        zmk_hid_mouse_button_press(0);
        zmk_usb_hid_send_mouse_report();
        zmk_hid_mouse_button_release(0);
        zmk_usb_hid_send_mouse_report();
    }
}

static void touch_end_timeout_callback(struct k_work *work) {
    struct k_work_delayable *d_work = k_work_delayable_from_work(work);
    const struct device *dev = DEVICE_DT_INST_GET(0);
    struct tap_data *data = (struct tap_data *)dev->data;
    handle_touch_end(data, dev->config);
}

static int tap_init(const struct device *dev) {
    struct tap_data *data = (struct tap_data *)dev->data;

    data->last_touch_timestamp = k_uptime_get();

    k_work_init_delayable(&data->tap_timeout_work, tap_timeout_callback);
    k_work_init_delayable(&data->touch_end_timeout_work, touch_end_timeout_callback);

    return 0;
}

static const struct zmk_input_processor_driver_api tap_driver_api = {
    .handle_event = tap_handle_event,
};

#define TAP_INST(n)                                                                                \
    static struct tap_data processor_tap_data_##n = {                                              \
        .touching = false,                                                                         \
    };                                                                                             \
    static const struct tap_config processor_tap_config_##n = {                                    \
        .timeout = DT_INST_PROP_OR(n, timeout, 50),                                                \
        .time_between_normal_reports = DT_INST_PROP_OR(n, time_between_normal_reports, 30),        \
    };                                                                                             \
    DEVICE_DT_INST_DEFINE(n, tap_init, NULL, &processor_tap_data_##n,                              \
                          &processor_tap_config_##n, POST_KERNEL,                                  \
                          CONFIG_INPUT_PINNACLE_INIT_PRIORITY, &tap_driver_api);

DT_INST_FOREACH_STATUS_OKAY(TAP_INST)