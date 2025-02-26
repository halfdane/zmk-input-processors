/*
 * Copyright (c) 2024 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#define DT_DRV_COMPAT zmk_input_processor_absolute_to_relative

#include <drivers/input_processor.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(absolute_to_relative, CONFIG_ZMK_LOG_LEVEL);

struct absolute_to_relative_config {
    uint8_t time_between_normal_reports;
};

struct absolute_to_relative_data {
    uint16_t previous_x, previous_y;
    bool touching_x, touching_y;
    const struct device *dev;
    struct k_work_delayable touch_end_timeout_work;
};
 
static int absolute_to_relative_handle_event(const struct device *dev, struct input_event *event, uint32_t param1,
                               uint32_t param2, struct zmk_input_processor_state *state) { 
    const struct absolute_to_relative_config *config = dev->config;
    struct absolute_to_relative_data *data = (struct absolute_to_relative_data *)dev->data;

    k_work_reschedule(&data->touch_end_timeout_work, K_MSEC(config->time_between_normal_reports));

    if (event->type == INPUT_EV_ABS) {
        uint16_t value = event->value;

        if (data->touching_x && event->code == INPUT_ABS_X) {
                event->type = INPUT_EV_REL;
                event->code = INPUT_REL_X;
                event->value -= data->previous_x;
                data->previous_x = value;
        } else if (data->touching_y && event->code == INPUT_ABS_Y) {
                event->type = INPUT_EV_REL;
                event->code = INPUT_REL_Y;
                event->value -= data->previous_y;
                data->previous_y = value;
        } else if (!data->touching_x && event->code == INPUT_ABS_X) {
            data->touching_x = true;
            data->previous_x = event->value;
        } else if (!data->touching_y && event->code == INPUT_ABS_Y) { 
            data->touching_y = true;
            data->previous_y = event->value;
        }
    }

    return ZMK_INPUT_PROC_CONTINUE;
}

static void touch_end_timeout_callback(struct k_work *work) {
    const struct device *dev = DEVICE_DT_INST_GET(0);
    struct absolute_to_relative_data *data = (struct absolute_to_relative_data *)dev->data;
    data->touching_x = false;
    data->touching_y = false;
}

static int absolute_to_relative_init(const struct device *dev) {
    struct absolute_to_relative_data *data = (struct absolute_to_relative_data *)dev->data;
    k_work_init_delayable(&data->touch_end_timeout_work, touch_end_timeout_callback);

    return 0;
}

static const struct zmk_input_processor_driver_api absolute_to_relative_driver_api = {
    .handle_event = absolute_to_relative_handle_event,
};

#define ABSOLUTE_TO_RELATIVE_INST(n)                                                                    \
    static struct absolute_to_relative_data processor_absolute_to_relative_data_##n = {                 \
        .touching_x = false,                                                                              \
        .touching_y = false,                                                                              \
    };                                                                                                  \
    static const struct absolute_to_relative_config processor_absolute_to_relative_config_##n = {       \
        .time_between_normal_reports = DT_INST_PROP_OR(n, time_between_normal_reports, 30),             \
    };                                                                                                  \
    DEVICE_DT_INST_DEFINE(n, absolute_to_relative_init, NULL, &processor_absolute_to_relative_data_##n, \
                          &processor_absolute_to_relative_config_##n, POST_KERNEL,                      \
                          CONFIG_INPUT_PINNACLE_INIT_PRIORITY, &absolute_to_relative_driver_api);

DT_INST_FOREACH_STATUS_OKAY(ABSOLUTE_TO_RELATIVE_INST)