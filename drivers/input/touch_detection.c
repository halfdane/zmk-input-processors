#include <drivers/input_processor.h>
#include <zephyr/logging/log.h>
#include "touch_detection.h"

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

static bool is_shortly_after_previous(uint8_t time_between_normal_reports, int64_t last_tapped, int64_t current_time) {
    const bool is_shortly_after = (last_tapped + time_between_normal_reports) > current_time;
    return is_shortly_after;
}

int touch_detection_handle_event(const struct device *dev, struct input_event *event, uint32_t param1,
                               uint32_t param2, struct zmk_input_processor_state *state) {
    struct touch_detection_config *config = (struct touch_detection_config *)dev->config;
    struct touch_detection_data *data = (struct touch_detection_data *)dev->data;

    uint32_t current_time = k_uptime_get();
    k_work_reschedule(&data->touch_end_timeout_work, K_MSEC(config->time_between_normal_reports));
    
    if (!data->touching){
        data->touching = true;
        if (config->touch_start != 0) {
            config->touch_start(dev, event, param1, param2, state);
        } else {
            LOG_WRN("Can't call touch start handler without configuration");
        }
    }

    data->last_touch_timestamp = current_time;

    return ZMK_INPUT_PROC_CONTINUE;
}

static void touch_end_timeout_callback(struct k_work *work) {
    struct k_work_delayable *d_work = k_work_delayable_from_work(work);
    struct touch_detection_data *data = CONTAINER_OF(d_work, struct touch_detection_data, touch_end_timeout_work);
    const struct device *dev = (const struct device *)data->dev;
    struct touch_detection_config *config = (struct touch_detection_config *)dev->config;
    data->touching = false;

    if (config->touch_end != 0) {
        config->touch_end(dev);
    }  else {
        LOG_WRN("Can't call touch end handler without configuration");
    }
}

void prepare_touch_detection_config(struct device *dev) {
    struct touch_detection_data *data = (struct touch_detection_data *)dev->data;
    struct touch_detection_config *config = (struct touch_detection_config *)dev->config;

    if (config->touch_start == 0) {
        LOG_ERR("Missing configuration for touch start handler - ignoring for now, but it will probably result in wrong behavior!");
    }
    if (config->touch_end == 0) {
        LOG_ERR("Missing configuration for touch end handler - ignoring for now, but it will probably result in wrong behavior!");
    }

    data->dev = dev;
    data->last_touch_timestamp = k_uptime_get();

    k_work_init_delayable(&data->touch_end_timeout_work, touch_end_timeout_callback);
}