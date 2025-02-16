#include "touch_detection.h"

static int touch_detection_handle_event(const struct device *dev, struct input_event *event, uint32_t param1,
                               uint32_t param2, struct zmk_input_processor_state *state) { 
    struct touch_detection_config *config = dev->config->touch_detection_config;
    struct touch_detection_data *data = (struct touch_detection_data *)dev->data->touch_detection_data;

    uint32_t current_time = k_uptime_get();
    k_work_reschedule(&data->touch_end_timeout_work, K_MSEC(config->time_between_normal_reports));
    
    if (!data->touching && !is_shortly_after_previous(config->time_between_normal_reports, data->last_touch_timestamp, current_time)){
        data->touching = true;
        handle_touch_start(data, config);
    }

    data->last_touch_timestamp = current_time;

    return ZMK_INPUT_PROC_CONTINUE;
}

static void touch_end_timeout_callback(struct k_work *work) {
    struct k_work_delayable *d_work = k_work_delayable_from_work(work);
    const struct device *dev = DEVICE_DT_INST_GET(0);
    struct touch_detection_config *config = dev->config->touch_detection_config;
    struct touch_detection_data *data = (struct touch_detection_data *)dev->data->touch_detection_data;
    data->touching = false;
    config->touch_end(dev);
}


static touch_detection_config prepare(const struct device *dev) {
    struct touch_detection_config *config = dev->config->touch_detection_config;
    struct touch_detection_data *data = (struct touch_detection_data *)dev->data->touch_detection_data;

    data->last_touch_timestamp = k_uptime_get();

    k_work_init_delayable(&data->touch_end_timeout_work, touch_end_timeout_callback);
}