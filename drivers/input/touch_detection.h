#pragma once

typedef int (*touch_start_t)(const struct device *dev, struct input_event *event, uint32_t param1,
                               uint32_t param2, struct zmk_input_processor_state *state);

typedef int (*touch_end_t)(const struct device *dev);

struct touch_detection_config {
    touch_start_t touch_start;
    touch_end_t touch_end;
    
    uint8_t time_between_normal_reports;
};

struct touch_detection_data {
    bool touching;
    const struct device *dev;
    struct k_work_delayable touch_end_timeout_work;
    uint32_t last_touch_timestamp;
};

void prepare_touch_detection_config(struct device *dev);

int touch_detection_handle_event(const struct device *dev, struct input_event *event, uint32_t param1,
                               uint32_t param2, struct zmk_input_processor_state *state);
                               