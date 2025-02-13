#define DT_DRV_COMPAT zmk_behavior_haptic_feedback

#include <zephyr/device.h>
#include <drivers/behavior.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

#include <zmk/keymap.h>
#include <zmk/behavior.h>

#include <zmk/ble.h>    
#include <zmk/event_manager.h>
#include <zmk/events/ble_active_profile_changed.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

// NOTE: checked in Kconfig & CMakeLists.txt
// #if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)
#define RUMBLE_NODE	DT_CHOSEN(DT_DRV_COMPAT)

int ACTIVE_DURATION = 100;
int INACTIVE_DURATION = 50;

int ACTIVE = 0;
int INACTIVE = 1;

bool haptic_feedback_active = false;
struct k_work_delayable haptic_feedback_timeout_task;
uint8_t haptic_feedback_count = 0;

static const struct gpio_dt_spec rmbl = GPIO_DT_SPEC_GET(RUMBLE_NODE, gpios);

int _verify_gpio(const struct gpio_dt_spec *spec) {
    if (!gpio_is_ready_dt(&rmbl)) {
		LOG_ERR("Error: rumble device %d is not ready\n", rmbl.pin);
		return -1;
	}

	if (gpio_pin_configure_dt(&rmbl, GPIO_OUTPUT_ACTIVE) < 0) {
        LOG_ERR("Error: Couldn't activate rumble device %d\n", rmbl.pin);
		return -2;
	}

    return 0;
}

void _feedback_activate() {
    if (_verify_gpio(&rmbl) < 0) {
        return;
    }
    haptic_feedback_active = true;
    gpio_pin_set_dt(&rmbl, ACTIVE);
}

void _feedback_deactivate() {
    if (_verify_gpio(&rmbl) < 0) {
        return;
    }
    haptic_feedback_active = false;
    gpio_pin_set_dt(&rmbl, INACTIVE);
}

void _reset_feedback() {
    haptic_feedback_count = 0;
    _feedback_deactivate();
}

static void feedback_state_handler() {
    k_work_cancel_delayable(&haptic_feedback_timeout_task);
    
    if(haptic_feedback_active) {
        _feedback_deactivate();
        if (haptic_feedback_count > 0) {
            haptic_feedback_count--;
            k_work_reschedule(&haptic_feedback_timeout_task, K_MSEC(INACTIVE_DURATION));
        }
    } else {
        _feedback_activate();
        k_work_reschedule(&haptic_feedback_timeout_task, K_MSEC(ACTIVE_DURATION));
    }
}

static int haptic_feedback_keymap_binding_pressed(struct zmk_behavior_binding *binding,
                                     struct zmk_behavior_binding_event event) {
    _reset_feedback();
    haptic_feedback_count = binding->param1 - 1;
    feedback_state_handler();

    return ZMK_BEHAVIOR_OPAQUE;
}

int ble_active_profile_change_listener(const zmk_event_t *eh)
{
    const struct zmk_ble_active_profile_changed *profile_ev = NULL;
    if ((profile_ev = as_zmk_ble_active_profile_changed(eh)) == NULL) {
        return ZMK_EV_EVENT_BUBBLE;
    }

    _reset_feedback();
    haptic_feedback_count = profile_ev->index;
    feedback_state_handler();

    return ZMK_EV_EVENT_BUBBLE;
}

static int haptic_feedback_keymap_binding_released(struct zmk_behavior_binding *binding,
                                      struct zmk_behavior_binding_event event) {
    return ZMK_BEHAVIOR_OPAQUE;
}

static const struct behavior_driver_api behavior_haptic_feedback_driver_api = {
    .binding_pressed = haptic_feedback_keymap_binding_pressed,
    .binding_released = haptic_feedback_keymap_binding_released,
};

static int behavior_haptic_feedback_init(const struct device *dev) {
    k_work_init_delayable(&haptic_feedback_timeout_task, feedback_state_handler);
    return 0; 
};

ZMK_LISTENER(ble_active_profile_change_status, ble_active_profile_change_listener)
#if defined(CONFIG_ZMK_BLE)
    ZMK_SUBSCRIPTION(ble_active_profile_change_status, zmk_ble_active_profile_changed);
#endif

BEHAVIOR_DT_INST_DEFINE(0, 
        behavior_haptic_feedback_init, NULL, 
        NULL, NULL, 
        POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, 
        &behavior_haptic_feedback_driver_api);

// #endif /* DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT) */