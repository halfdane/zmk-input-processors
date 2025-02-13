/*
 * Copyright (c) 2024 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#define DT_DRV_COMPAT zmk_input_processor_tap

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <drivers/input_processor.h>

#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

struct tap_config {
    uint8_t timeout;
};

static int tap_handle_event(const struct device *dev, struct input_event *event, uint32_t param1,
                               uint32_t param2, struct zmk_input_processor_state *state) {
    LOG_INF("Handling tap event");
    return ZMK_INPUT_PROC_CONTINUE;
}

static int tap_init(const struct device *dev) {
    LOG_INF("creating tap input processor");

    // for (int i = 0; i < MAX_LAYERS; i++) {
    //     k_work_init_delayable(&layer_disable_works[i], layer_disable_callback);
    // }

    return 0;
}

static struct zmk_input_processor_driver_api tap_driver_api = {
    .handle_event = tap_handle_event,
};

#define TAP_INST(n)                                                                                \
    static struct tap_data processor_tap_data_##n = {};                                            \
    static const struct tap_config processor_tap_config_##n = {                                    \
        .timeout = DT_INST_PROP_OR(n, timeout, 20),                                                \
    };                                                                                             \
    DEVICE_DT_INST_DEFINE(n, tap_init, NULL, &processor_tap_data_##n,                              \
                          &processor_tap_config_##n, POST_KERNEL,                                  \
                          CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &tap_driver_api);

DT_INST_FOREACH_STATUS_OKAY(TAP_INST)