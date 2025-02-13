#pragma once

struct tap_data {
    bool touch_start, is_tap;
    const struct device *dev;
    struct k_work work;
};

struct tap_config {
    uint8_t timeout;
};

