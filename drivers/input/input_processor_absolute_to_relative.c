/*
 * Copyright (c) 2024 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#define DT_DRV_COMPAT zmk_input_processor_absolute_to_relative

#include <drivers/input_processor.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

struct absolute_to_relative_config {
    uint8_t time_between_normal_reports;
};

struct absolute_to_relative_data {
    uint16_t previous_x, previous_y;
    bool touching;
    const struct device *dev;
    struct k_work_delayable touch_end_timeout_work;
    uint32_t last_touch_timestamp;
};

static bool is_shortly_after_previous(uint8_t time_between_normal_reports, int64_t last_touched, int64_t current_time) {
    const bool is_shortly_after = (last_touched + time_between_normal_reports) > current_time;
    return is_shortly_after;
}

static int handle_touch_start(struct absolute_to_relative_data *data) {
    data->touching = true;
    LOG_INF("TOUCH STARTED");
    return 0;
}

static int handle_touch_end(struct absolute_to_relative_data *data) {
    data->touching = false;
    data->previous_x = 0;
    data->previous_y = 0;
    LOG_INF("TOUCH ENDED");
    return 0;
}
 
static int absolute_to_relative_handle_event(const struct device *dev, struct input_event *event, uint32_t param1,
                               uint32_t param2, struct zmk_input_processor_state *state) { 
    const struct absolute_to_relative_config *config = dev->config;
    struct absolute_to_relative_data *data = (struct absolute_to_relative_data *)dev->data;

    uint32_t current_time = k_uptime_get();
    k_work_reschedule(&data->touch_end_timeout_work, K_MSEC(config->time_between_normal_reports));
    
    if (!data->touching && !is_shortly_after_previous(config->time_between_normal_reports, data->last_touch_timestamp, current_time)){
        if (event->type == INPUT_EV_ABS) {
            if (event->code == INPUT_ABS_X) {
                data->previous_x = event->value;
            } else if (event->code == INPUT_ABS_Y) {
                data->previous_y = event->value;
            }
        }
    } else {
        if (event->type == INPUT_EV_ABS) {
            if (event->code == INPUT_ABS_X) {
                zmk_hid_mouse_movement_update(event->value - data->previous_x, data->previous_y);
                data->previous_x = event->value;
            } else if (event->code == INPUT_ABS_Y) {
                zmk_hid_mouse_movement_update(data->previous_x, event->value - data->previous_y);
                data->previous_y = event->value;
            }
            zmk_usb_hid_send_mouse_report();
        }
    }

    data->last_touch_timestamp = current_time;

    return ZMK_INPUT_PROC_CONTINUE;
}

static void touch_end_timeout_callback(struct k_work *work) {
    struct k_work_delayable *d_work = k_work_delayable_from_work(work);
    const struct device *dev = DEVICE_DT_INST_GET(0);
    struct absolute_to_relative_data *data = (struct absolute_to_relative_data *)dev->data;
    handle_touch_end(data);
}

static int absolute_to_relative_init(const struct device *dev) {
    struct absolute_to_relative_data *data = (struct absolute_to_relative_data *)dev->data;

    data->last_touch_timestamp = k_uptime_get();

    k_work_init_delayable(&data->touch_end_timeout_work, touch_end_timeout_callback);

    return 0;
}

static const struct zmk_input_processor_driver_api absolute_to_relative_driver_api = {
    .handle_event = absolute_to_relative_handle_event,
};

#define ABSOLUTE_TO_RELATIVE_INST(n)                                                                    \
    static struct absolute_to_relative_data processor_absolute_to_relative_data_##n = {                 \
        .touching = false,                                                                              \
    };                                                                                                  \
    static const struct absolute_to_relative_config processor_absolute_to_relative_config_##n = {       \
        .time_between_normal_reports = DT_INST_PROP_OR(n, time_between_normal_reports, 30),             \
    };                                                                                                  \
    DEVICE_DT_INST_DEFINE(n, absolute_to_relative_init, NULL, &processor_absolute_to_relative_data_##n, \
                          &processor_absolute_to_relative_config_##n, POST_KERNEL,                      \
                          CONFIG_INPUT_PINNACLE_INIT_PRIORITY, &absolute_to_relative_driver_api);

DT_INST_FOREACH_STATUS_OKAY(ABSOLUTE_TO_RELATIVE_INST)