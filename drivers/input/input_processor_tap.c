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
    bool touching, is_tap;
    const struct device *dev;
    struct k_work_delayable tap_timeout_work;
    struct k_work_delayable touch_end_timeout_work;
    struct k_work_delayable tap_end_timeout_work;
    uint32_t last_touch_timestamp;
};

static bool is_shortly_after_previous(uint8_t time_between_normal_reports, int64_t last_tapped, int64_t current_time) {
    const bool is_shortly_after = (last_tapped + time_between_normal_reports) > current_time;
    return is_shortly_after;
}

static int handle_touch_start(struct tap_data *data, struct tap_config *config) {
    data->touching = true;

    LOG_ERR("Reschedule tap detection in %d ms", config->timeout);
    k_work_reschedule(&data->tap_timeout_work, K_MSEC(config->timeout));

    LOG_ERR("TOUCH STARTED");
    return 0;
}

static int handle_touch_end(struct tap_data *data, struct tap_config *config) {
    data->touching = false;
    LOG_ERR("TOUCH ENDED");
    return 0;
}
 
static int tap_handle_event(const struct device *dev, struct input_event *event, uint32_t param1,
                               uint32_t param2, struct zmk_input_processor_state *state) { 
    const struct tap_config *config = dev->config;
    struct tap_data *data = (struct tap_data *)dev->data;

    char* type = "UNKNOWN";
    char* code = "UNKNOWN";

    if (event->type == INPUT_EV_ABS) {
        type = "ABS";
    }
    if (event->type == INPUT_EV_REL) {
        type = "REL";
    }
    if (event->code == INPUT_REL_X) {
        code = "[rel x]";
    }
    if (event->code == INPUT_REL_Y) {
        code = "[rel y]";
    }
    if (event->code == INPUT_ABS_X) {
        code = "[abs x]";
    }
    if (event->code == INPUT_ABS_Y) {
        code = "[abs y]";
    }

    input_report_rel(dev, INPUT_REL_X, -100, false, K_FOREVER);
    input_report_rel(dev, INPUT_REL_Y, 0, true, K_FOREVER);

    uint32_t current_time = k_uptime_get();
    k_work_reschedule(&data->touch_end_timeout_work, K_MSEC(config->time_between_normal_reports));
    
    if (!data->touching && !is_shortly_after_previous(config->time_between_normal_reports, data->last_touch_timestamp, current_time)){
        handle_touch_start(data, config);
    }

    data->last_touch_timestamp = current_time;
    

    // LOG_ERR("Got value: %d [%s] [%s]", event->value, type, code);
    

    return ZMK_INPUT_PROC_CONTINUE;
}

/* Work Queue Callback */
static void tap_timeout_callback(struct k_work *work) {
    const struct device *dev = DEVICE_DT_INST_GET(0);
    struct tap_data *data = (struct tap_data *)dev->data;

    if (data->touching) {
        LOG_ERR("got a timeout but still touching- it's not a tap");
    } else {
        LOG_ERR("TAP");
        input_report_key(dev, 256, 1, false, K_FOREVER);
        input_report_rel(dev, INPUT_REL_X, 100, false, K_FOREVER);
        input_report_rel(dev, INPUT_REL_Y, 100, true, K_FOREVER);

        k_work_reschedule(&data->tap_end_timeout_work, K_MSEC(200));
    }

    data->is_tap = false;
}

static void tap_end_timeout_callback(struct k_work *work) {
    const struct device *dev = DEVICE_DT_INST_GET(0);
    struct tap_data *data = (struct tap_data *)dev->data;
    input_report_key(dev, 256, 0, false, K_FOREVER);
    input_report_rel(dev, INPUT_REL_X, -100, false, K_FOREVER);
    input_report_rel(dev, INPUT_REL_Y, 0, true, K_FOREVER);
    LOG_ERR("TAP DONE");
}

static void touch_end_timeout_callback(struct k_work *work) {
    struct k_work_delayable *d_work = k_work_delayable_from_work(work);

    const struct device *dev = DEVICE_DT_INST_GET(0);
    struct tap_data *data = (struct tap_data *)dev->data;
    handle_touch_end(data, dev->config);
}

static int tap_init(const struct device *dev) {
    LOG_ERR("creating tap input processor");
    struct tap_data *data = (struct tap_data *)dev->data;

    data->last_touch_timestamp = k_uptime_get();

    k_work_init_delayable(&data->tap_timeout_work, tap_timeout_callback);
    k_work_init_delayable(&data->touch_end_timeout_work, touch_end_timeout_callback);
    k_work_init_delayable(&data->tap_end_timeout_work, tap_end_timeout_callback);

    return 0;
}

static const struct zmk_input_processor_driver_api tap_driver_api = {
    .handle_event = tap_handle_event,
};

#define TAP_INST(n)                                                                                \
    static struct tap_data processor_tap_data_##n = {                                              \
        .touching = false,                                                                         \
        .is_tap = false,                                                                           \
    };                                                                                             \
    static const struct tap_config processor_tap_config_##n = {                                    \
        .timeout = DT_INST_PROP_OR(n, timeout, 50),                                                \
        .time_between_normal_reports = DT_INST_PROP_OR(n, time_between_normal_reports, 30),        \
    };                                                                                             \
    DEVICE_DT_INST_DEFINE(n, tap_init, NULL, &processor_tap_data_##n,                              \
                          &processor_tap_config_##n, POST_KERNEL,                                  \
                          CONFIG_INPUT_PINNACLE_INIT_PRIORITY, &tap_driver_api);

DT_INST_FOREACH_STATUS_OKAY(TAP_INST)