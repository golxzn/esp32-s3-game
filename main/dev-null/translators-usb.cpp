#include <span>
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

#include "gzn/input/actions.hpp"

static const char *TAG = "example";

inline constexpr UBaseType_t queue_size{ 10 };

namespace gzn::input::backend {

using translator_function = void(*)(
	const std::span<const uint8_t> data,
	action_array &actions,
);


void keyboard_translator(
	const std::span<const uint8_t> data,
	core::enum_array<action_type, action_info> &actions
) {
	if (std::size(data) < sizeof(hid_keyboard_input_report_boot_t)) {
		return;
	}

	const auto kb_report{
		reinterpret_cast<const hid_keyboard_input_report_boot_t *>(std::data(data))
	};

	const auto timestamp{ static_cast<uint64_t>(esp_timer_get_time()) };

	for (size_t i{}; i < HID_KEYBOARD_KEY_MAX; ++i) {
		const auto key{ static_cast<hid_key_t>(kb_report->key[i]) };

		action_info *action{ nullptr };
		switch (key) {
			case HID_KEY_LEFT: [[fallthrough]];
			case HID_KEY_A: {
				action = &actions[action_type::horizontal_move];
				action->payload[0] = 0xFF;
				action->payload[1] = 0x00;
			} break;

			case HID_KEY_RIGHT: [[fallthrough]];
			case HID_KEY_D: {
				action = &actions[action_type::horizontal_move];
				action->payload[0] = 0x00;
				action->payload[1] = 0xFF;
			} break;

			case HID_KEY_DOWN: [[fallthrough]];
			case HID_KEY_S: {
				action = &actions[action_type::vertical_move];
				action->payload[0] = 0x00;
				action->payload[1] = 0xFF;
			} break;

			case HID_KEY_UP: [[fallthrough]];
			case HID_KEY_W: {
				action = &actions[action_type::vertical_move];
				action->payload[0] = 0xFF;
				action->payload[1] = 0x00;
			} break;

			case HID_KEY_ESC: {
				action = &actions[action_type::pause];
				action->pressed = true;
			} break;

			case HID_KEY_E: [[fallthrough]];
			case HID_KEY_SPACE: {
				action = &actions[action_type::use];
				action->pressed = true;
			} break;

			default: continue;
		}
		action->timestamp = timestamp;
	}

	for (auto &action : actions) {
		if (action.timestamp != timestamp) {
			action.pressed = false;
		}
	}
}

void mouse_translator(
	const std::span<const uint8_t> data,
	core::enum_array<action_type, action_info> &actions
) {
}


void dualsense_usb_translator(
	const std::span<const uint8_t> data,
	core::enum_array<action_type, action_info> &actions
) {
}

auto select_translator(
	const hid_protocol_t protocol, const uint32_t VID_PID
) -> translator_function {
	switch (protocol) {
		case HID_PROTOCOL_KEYBOARD: return keyboard_translator;
		case HID_PROTOCOL_MOUSE   : return mouse_translator;

		default: break;
	}

	constexpr uint32_t duansense_vid_pid{ 0x054C'0CE6 };
	if (duansense_vid_pid == VID_PID) {
		return dualsense_usb_translator;
	}

	return nullptr;
}

struct device_info {
	uintptr_t           id{};
	translator_function translator{ nullptr };
	uint16_t            vendor_id{};
	uint16_t            product_id{};
};
constexpr size_t max_devices_count{ 4 };

struct context {
	core::enum_array<action_type, action_info> actions{};
	std::array<device_info, max_devices_count> devices{};


	[[gnu::always_inline]]
	inline auto find_device(const uintptr_t id) -> device_info * {
		const auto found{ std::ranges::find(devices, id, &device_info::id) };
		return found != std::end(devices) ? &(*found) : nullptr;
	}

	[[gnu::always_inline]]
	inline auto find_device(const uintptr_t id) const -> const device_info * {
		const auto found{ std::ranges::find(devices, id, &device_info::id) };
		return found != std::end(devices) ? &(*found) : nullptr;
	}
};

} // namespace backend

void hid_host_interface_callback(
	hid_host_device_handle_t hid_device_handle,
	const hid_host_interface_event_t event,
	void *arg
) {
	auto ctx{ static_cast<gzn::input::backend::context *>(arg) };
	auto device{ ctx->find_device(reinterpret_cast<uintptr_t>(hid_device_handle)) };
	if (!device) {
		return;
	}

	switch (event) {
		case HID_HOST_INTERFACE_EVENT_INPUT_REPORT: {
			static std::array<uint8_t, 64> buffer{};
			size_t data_length{};
			ESP_ERROR_CHECK(hid_host_device_get_raw_input_report_data(
				hid_device_handle, std::data(buffer), std::size(buffer), &data_length
			));
			device->translator({ std::data(buffer), data_length }, ctx->actions);
		} break;

		case HID_HOST_INTERFACE_EVENT_DISCONNECTED:
			ESP_LOGI(TAG, "HID Device, protocol '0x%X' DISCONNECTED", device->id);
			ESP_ERROR_CHECK(hid_host_device_close(hid_device_handle));
			*device = {};
			break;

		case HID_HOST_INTERFACE_EVENT_TRANSFER_ERROR:
			ESP_LOGI(TAG, "HID Device, protocol '0x%X' TRANSFER_ERROR", device->id);
			break;

		default:
			ESP_LOGE(TAG, "HID Device, protocol '0x%X' Unhandled event", device->id);
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

	const uintptr_t device_id{ reinterpret_cast<uintptr_t>(hid_device_handle) };
	auto ctx{ static_cast<gzn::input::backend::context *>(arg) };

	gzn::input::backend::device_info *selected{ nullptr };
	for (auto &device : ctx->devices) {
		if (device.id == 0 && selected == nullptr) {
			selected = &device;
		}
		if (device.id == device_id) {
			ESP_LOGE(TAG, "Device with id 0x%X is already connected!", device_id);
			return;
		}
	}

	if (selected == nullptr) {
		ESP_LOGW(TAG, "Cannot connect new device. Max devices count had been reached!");
		return;
	}

	const auto protocol{ static_cast<hid_protocol_t>(dev_params.proto) };
	*selected = gzn::input::backend::device_info{
		.id         = device_id,
		.translator = gzn::input::backend::select_translator(protocol, (info.VID << 16) | info.PID),
		.vendor_id  = info.VID,
		.product_id = info.PID,
	};

	const hid_host_device_config_t dev_config{
		.callback     = hid_host_interface_callback,
		.callback_arg = arg
	};

	ESP_ERROR_CHECK(hid_host_device_open(hid_device_handle, &dev_config));
	if (HID_SUBCLASS_BOOT_INTERFACE == dev_params.sub_class) {
		ESP_ERROR_CHECK(hid_class_request_set_protocol(hid_device_handle, HID_REPORT_PROTOCOL_BOOT));
		if (HID_PROTOCOL_KEYBOARD == protocol) {
			ESP_ERROR_CHECK(hid_class_request_set_idle(hid_device_handle, 0, 0));
		}
	}

	ESP_ERROR_CHECK(hid_host_device_start(hid_device_handle));
}

static void usb_lib_task(void *arg) {
	const usb_host_config_t host_config{
		.skip_phy_setup = false,
		.intr_flags = ESP_INTR_FLAG_LEVEL1,
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

	gzn::input::backend::context ctx{};

	/*
	* HID host driver configuration
	* - create background task for handling low level event inside the HID driver
	* - provide the device callback to get new HID Device connection event
	*/
	const hid_host_driver_config_t hid_host_driver_config{
		.create_background_task = true,
		.task_priority          = 5,
		.stack_size             = 4096,
		.core_id                = 0,
		.callback               = hid_host_device_event,
		.callback_arg           = &ctx
	};

	ESP_ERROR_CHECK(hid_host_install(&hid_host_driver_config));

	while(true) {
		vTaskDelay(1000);
	}

	ESP_LOGI(TAG, "HID Driver uninstall");
	ESP_ERROR_CHECK(hid_host_uninstall());
}

/*
I (259) main_task: Started on CPU0
I (278) main_task: Calling app_main()
I (278) example: HID Host example
E (278) HCD DWC: Interrupt alloc error: ESP_ERR_INVALID_ARG
E (279) USB HOST: HCD install error: ESP_ERR_INVALID_ARG
ESP_ERROR_CHECK failed: esp_err_t 0x102 (ESP_ERR_INVALID_ARG) at 0x42008d2a
--- 0x42008d2a: usb_lib_task(void*) at G:/projects/embedded/esp32/test-proj/main/sources/usb.cpp:276
file: "./main/sources/usb.cpp" line 276
func: void usb_lib_task(void*)
expression: usb_host_install(&host_config)

abort() was called at PC 0x4037dc87 on core 0
--- 0x4037dc87: _esp_error_check_failed at G:/sdk/Espressif/frameworks/esp-idf-v5.5/components/esp_system/esp_err.c:49


Backtrace: 0x40379a91:0x3fcea790 0x4037dc91:0x3fcea7b0 0x40383a66:0x3fcea7d0 0x4037dc87:0x3fcea840 0x42008d2a:0x3fcea870
--- 0x40379a91: panic_abort at G:/sdk/Espressif/frameworks/esp-idf-v5.5/components/esp_system/panic.c:469
--- 0x4037dc91: esp_system_abort at G:/sdk/Espressif/frameworks/esp-idf-v5.5/components/esp_system/port/esp_system_chip.c:87
--- 0x40383a66: abort at G:/sdk/Espressif/frameworks/esp-idf-v5.5/components/newlib/src/abort.c:38
--- 0x4037dc87: _esp_error_check_failed at G:/sdk/Espressif/frameworks/esp-idf-v5.5/components/esp_system/esp_err.c:49
--- 0x42008d2a: usb_lib_task(void*) at G:/projects/embedded/esp32/test-proj/main/sources/usb.cpp:276

   */
