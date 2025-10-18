#include <array>
#include <cstdio>
#include <cassert>
#include <algorithm>
#include <string.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>
#include <freertos/queue.h>
#include <esp_err.h>
#include <esp_log.h>
#include <usb/usb_host.h>
#include <driver/gpio.h>

#include <usb/hid_host.h>
#include <usb/hid_usage_keyboard.h>

static const char *TAG = "example";
inline constexpr UBaseType_t queue_size{ 10 };

QueueHandle_t app_event_queue{};

enum app_event_group_t {
	APP_EVENT = 0,
	APP_EVENT_HID_HOST
};

/* This event is used for delivering the HID Host event from callback to a task. */
struct app_event_queue_t {
	app_event_group_t event_group;
	/* HID Host - Device related info */
	struct {
		hid_host_device_handle_t handle;
		hid_host_driver_event_t event;
		void *arg;
	} hid_host_device;
};

static const char *hid_proto_name_str[] = {
	"NONE",
	"KEYBOARD"
};

struct key_event_t {
	enum key_state : uint8_t {
		KEY_STATE_PRESSED = 0x00,
		KEY_STATE_RELEASED = 0x01
	} state;
	uint8_t modifier;
	uint8_t key_code;
};

static inline bool hid_keyboard_is_modifier_shift(const uint8_t modifier) {
	return (modifier & HID_LEFT_SHIFT ) == HID_LEFT_SHIFT
		|| (modifier & HID_RIGHT_SHIFT) == HID_RIGHT_SHIFT;
}

static void key_event_callback(const key_event_t key_event) {

}

static inline bool key_found(
	const uint8_t *const src, const uint8_t key, const uint32_t length = HID_KEYBOARD_KEY_MAX
) {
	for (uint32_t i{}; i < length; i++) {
		if (src[i] == key) {
			return true;
		}
	}
	return false;
}

static void hid_host_keyboard_report_callback(const uint8_t *const data, const int length) {
	if (length < sizeof(hid_keyboard_input_report_boot_t)) {
		return;
	}

	const auto kb_report{ reinterpret_cast<const hid_keyboard_input_report_boot_t *const>(data) };

	static std::array<uint8_t, HID_KEYBOARD_KEY_MAX> previous_keys{ HID_KEY_ERROR_UNDEFINED };

	for (int i{}; i < HID_KEYBOARD_KEY_MAX; ++i) {

		// key has been released verification
		if (previous_keys[i] > HID_KEY_ERROR_UNDEFINED && !key_found(kb_report->key, previous_keys[i])) {
			key_event_callback(key_event_t{
				.state = key_event_t::KEY_STATE_RELEASED,
				.modifier = 0,
				.key_code = previous_keys[i]
			});
		}

		// key has been pressed verification
		if (kb_report->key[i] > HID_KEY_ERROR_UNDEFINED && !std::ranges::contains(previous_keys, kb_report->key[i])) {
			key_event_callback(key_event_t{
				.state = key_event_t::KEY_STATE_PRESSED,
				.modifier = kb_report->modifier.val,
				.key_code = kb_report->key[i]
			});
		}
	}

	std::ranges::copy(kb_report->key, std::begin(previous_keys));
	// std::memcpy(std::data(previous_keys), kb_report->key, std::size(previous_keys));
}




struct [[gnu::packed]] dualsense_bytes {
};

static void hid_host_generic_report_callback(const uint8_t *const data, const int length) {
	// 017E82828100009A08000000D98950A1F8FFFBFF0400E5FF741FD205492A12010B80000000800000000009090000000000D0551201110800C40D635A04EDBBF8
	for (int i = 0; i < length; i++) {
		std::printf("%02X", data[i]);
	}
	std::putchar('\r');
}

void hid_host_interface_callback(
	hid_host_device_handle_t hid_device_handle,
	const hid_host_interface_event_t event,
	void *arg
) {
	static std::array<uint8_t, 64> buffer{};
	size_t data_length = 0;
	hid_host_dev_params_t dev_params;
	ESP_ERROR_CHECK(hid_host_device_get_params(hid_device_handle, &dev_params));

	switch (event) {
		case HID_HOST_INTERFACE_EVENT_INPUT_REPORT:
			ESP_ERROR_CHECK(hid_host_device_get_raw_input_report_data(
				hid_device_handle, std::data(buffer), std::size(buffer), &data_length
			));

			if (HID_SUBCLASS_BOOT_INTERFACE == dev_params.sub_class) {
				if (HID_PROTOCOL_KEYBOARD == dev_params.proto) {
					hid_host_keyboard_report_callback(std::data(buffer), data_length);
				}
			} else {
				hid_host_generic_report_callback(std::data(buffer), data_length);
			}
			break;

		case HID_HOST_INTERFACE_EVENT_DISCONNECTED:
			ESP_LOGI(TAG, "HID Device, protocol '%s' DISCONNECTED", hid_proto_name_str[dev_params.proto]);
			ESP_ERROR_CHECK(hid_host_device_close(hid_device_handle));
			break;

		case HID_HOST_INTERFACE_EVENT_TRANSFER_ERROR:
			ESP_LOGI(TAG, "HID Device, protocol '%s' TRANSFER_ERROR", hid_proto_name_str[dev_params.proto]);
			break;

		default:
			ESP_LOGE(TAG, "HID Device, protocol '%s' Unhandled event", hid_proto_name_str[dev_params.proto]);
			break;
	}
}

void hid_host_device_event(
	hid_host_device_handle_t hid_device_handle,
	[[maybe_unused]] const hid_host_driver_event_t event,
	void *arg
) {
	hid_host_dev_params_t dev_params;
	ESP_ERROR_CHECK(hid_host_device_get_params(hid_device_handle, &dev_params));

	ESP_LOGI(TAG, "HID Device params:");
	ESP_LOGI(TAG, "  USB Address of connected HID device: %X", dev_params.addr);
	ESP_LOGI(TAG, "  HID Interface Number:                %X", dev_params.iface_num);
	ESP_LOGI(TAG, "  HID Interface SubClass:              %X", dev_params.sub_class);
	ESP_LOGI(TAG, "  HID Interface Protocol:              %X", dev_params.proto);

	hid_host_dev_info_t info;
	hid_host_get_device_info(hid_device_handle, &info);
	ESP_LOGI(TAG, "HID Device Info");
	ESP_LOGI(TAG, "   Vendor  ID: 0x%X", info.VID);
	ESP_LOGI(TAG, "   Product ID: 0x%X", info.PID);

	// event is always HID_HOST_DRIVER_EVENT_CONNECTED

	ESP_LOGI(TAG, "HID Device, protocol '%s' CONNECTED", hid_proto_name_str[dev_params.proto]);

	const hid_host_device_config_t dev_config{
		.callback = hid_host_interface_callback,
		.callback_arg = nullptr
	};

	ESP_ERROR_CHECK(hid_host_device_open(hid_device_handle, &dev_config));
	if (HID_SUBCLASS_BOOT_INTERFACE == dev_params.sub_class) {
		ESP_ERROR_CHECK(hid_class_request_set_protocol(hid_device_handle, HID_REPORT_PROTOCOL_BOOT));
		if (HID_PROTOCOL_KEYBOARD == dev_params.proto) {
			ESP_ERROR_CHECK(hid_class_request_set_idle(hid_device_handle, 0, 0));
		}
	}

	ESP_ERROR_CHECK(hid_host_device_start(hid_device_handle));
}

static void usb_lib_task(void *arg) {
	const usb_host_config_t host_config{
		.skip_phy_setup = false,
		.intr_flags = ESP_INTR_FLAG_LEVEL6,
	};

	ESP_ERROR_CHECK(usb_host_install(&host_config));
	xTaskNotifyGive(static_cast<TaskHandle_t>(arg));

	uint32_t event_flags;
	while (true) {
		usb_host_lib_handle_events(portMAX_DELAY, &event_flags);

		// In this example, there is only one client registered
		// So, once we deregister the client, this call must succeed with ESP_OK
		if (event_flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS) {
			ESP_ERROR_CHECK(usb_host_device_free_all());
			break;
		}
	}

	ESP_LOGI(TAG, "USB shutdown");

	vTaskDelay(10); // Short delay to allow clients clean-up
	ESP_ERROR_CHECK(usb_host_uninstall());
	vTaskDelete(nullptr);
}

void hid_host_device_callback(
	hid_host_device_handle_t hid_device_handle,
	const hid_host_driver_event_t event,
	void *arg
) {
	if (!app_event_queue) {
		return;
	}

	const app_event_queue_t evt_queue{
		.event_group = APP_EVENT_HID_HOST,
		.hid_host_device = {
			.handle = hid_device_handle,
			.event = event,
			.arg = arg
		}
	};
	xQueueSend(app_event_queue, &evt_queue, 0);
}

void usb_test() {
	ESP_LOGI(TAG, "HID Host example");

	/*
	* Create usb_lib_task to:
	* - initialize USB Host library
	* - Handle USB Host events while APP pin in in HIGH state
	*/
	const auto task_created = xTaskCreatePinnedToCore(
		usb_lib_task,
		"usb_events",
		4096,
		xTaskGetCurrentTaskHandle(),
		2, nullptr, 0
	);
	assert(task_created == pdTRUE);

	// Wait for notification from usb_lib_task to proceed
	ulTaskNotifyTake(false, 1000);

	/*
	* HID host driver configuration
	* - create background task for handling low level event inside the HID driver
	* - provide the device callback to get new HID Device connection event
	*/
	const hid_host_driver_config_t hid_host_driver_config{
		.create_background_task = true,
		.task_priority = 5,
		.stack_size = 4096,
		.core_id = 0,
		.callback = hid_host_device_callback,
		.callback_arg = nullptr
	};

	ESP_ERROR_CHECK(hid_host_install(&hid_host_driver_config));

	// Create queue
	app_event_queue = xQueueCreate(queue_size, sizeof(app_event_queue_t));

	ESP_LOGI(TAG, "Waiting for HID Device to be connected");

	app_event_queue_t evt_queue;
	while (true) {
		if (!xQueueReceive(app_event_queue, &evt_queue, portMAX_DELAY)) {
			continue;
		}

		if (APP_EVENT == evt_queue.event_group) {
			// User pressed button
			usb_host_lib_info_t lib_info;
			ESP_ERROR_CHECK(usb_host_lib_info(&lib_info));
			if (lib_info.num_devices != 0) {
				ESP_LOGW(TAG, "To shutdown example, remove all USB devices and press button again.");
				// Keep polling
				// End while cycle
				continue;
			}
			break;
		}

		if (APP_EVENT_HID_HOST == evt_queue.event_group) {
			hid_host_device_event(
				evt_queue.hid_host_device.handle,
				evt_queue.hid_host_device.event,
				evt_queue.hid_host_device.arg
			);
			continue;
		}
	}

	ESP_LOGI(TAG, "HID Driver uninstall");
	ESP_ERROR_CHECK(hid_host_uninstall());
	xQueueReset(app_event_queue);
	vQueueDelete(app_event_queue);
}

