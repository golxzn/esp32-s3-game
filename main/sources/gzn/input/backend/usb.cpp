#include <array>
#include <cassert>

#include <esp_log.h>
#include <usb/usb_host.h>
#include <usb/hid_host.h>

#include "gzn/input/backend/usb.hpp"
#include "gzn/input/context.hpp"

namespace gzn::input::backend {

namespace {

constexpr auto TAG{ "input::backend::usb" };

TaskHandle_t g_usb_task{};

void hid_host_interface_callback(
	hid_host_device_handle_t hid_device_handle,
	const hid_host_interface_event_t event,
	void *arg
) {
	auto ctx{ static_cast<context *>(arg) };
	auto device{ ctx->find_device(reinterpret_cast<uintptr_t>(hid_device_handle)) };
	if (!device) {
		return;
	}

	switch (event) {
		case HID_HOST_INTERFACE_EVENT_INPUT_REPORT: {
			static std::array<uint8_t, usb::recieve_buffer_length> buffer{};
			size_t data_length{};
			ESP_ERROR_CHECK(hid_host_device_get_raw_input_report_data(
				hid_device_handle, std::data(buffer), std::size(buffer), &data_length
			));
			device->translator({ std::data(buffer), data_length }, ctx->actions);
		} break;

		case HID_HOST_INTERFACE_EVENT_DISCONNECTED:
			ESP_LOGI(TAG, "HID Device, protocol '0x%X' DISCONNECTED", device->id);
			ESP_ERROR_CHECK(hid_host_device_close(hid_device_handle));
			for (auto &handler : ctx->event_handlers) {
				if (handler.on_disconnect) {
					handler.on_disconnect(*device, handler.user_data);
				}
			}
			*device = {};
			break;

		case HID_HOST_INTERFACE_EVENT_TRANSFER_ERROR:
			ESP_LOGI(TAG, "HID Device, protocol '0x%X' TRANSFER_ERROR", device->id);
			for (auto &handler : ctx->event_handlers) {
				if (handler.on_transfer_error) {
					handler.on_transfer_error(*device, handler.user_data);
				}
			}
			break;

		default:
			ESP_LOGE(TAG, "HID Device, protocol '0x%X' Unhandled event", device->id);
			break;
	}
}

void on_hid_device_connected(
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
	auto ctx{ static_cast<context *>(arg) };

	const device_info device{
		.id         = device_id,
		.translator = select_translator(dev_params.proto, (info.VID << 16) | info.PID),
		.vendor_id  = info.VID,
		.product_id = info.PID,
	};
	if (!ctx->push_device(device)) {
		return;
	}

	const hid_host_device_config_t dev_config{
		.callback     = hid_host_interface_callback,
		.callback_arg = arg
	};

	ESP_ERROR_CHECK(hid_host_device_open(hid_device_handle, &dev_config));
	/// @todo Seems like it fails since we're not in the main thread?
/*
E (6286) hid-host: Control Transfer Timeout
E (6286) USB HOST: Get EP handle error: ESP_ERR_INVALID_ARG
E (6286) hid-host: hid_control_transfer(752): Unable to HALT EP
ESP_ERROR_CHECK failed: esp_err_t 0x102 (ESP_ERR_INVALID_ARG) at 0x4200a1b8
--- 0x4200a1b8: gzn::input::backend::(anonymous namespace)::on_hid_device_connected(hid_interface*, hid_host_driver_event_t, void*) at G:/projects/embedded/esp32/test-proj/main/sources/gzn/input/backend/usb.cpp:106
file: "./main/sources/gzn/input/backend/usb.cpp" line 106
func: void gzn::input::backend::{anonymous}::on_hid_device_connected(hid_host_device_handle_t, hid_host_driver_event_t, void*)
expression: hid_class_request_set_protocol(hid_device_handle, HID_REPORT_PROTOCOL_BOOT)

abort() was called at PC 0x4037de33 on core 0
--- 0x4037de33: _esp_error_check_failed at G:/sdk/Espressif/frameworks/esp-idf-v5.5/components/esp_system/esp_err.c:49


Backtrace: 0x40379abd:0x3fcee3b0 0x4037de3d:0x3fcee3d0 0x40383c66:0x3fcee3f0 0x4037de33:0x3fcee460 0x4200a1b8:0x3fcee490 0x4200b3d7:0x3fcee590 0x4200c7ae:0x3fcee600 0x4200aaf1:0x3fcee630
--- 0x40379abd: panic_abort at G:/sdk/Espressif/frameworks/esp-idf-v5.5/components/esp_system/panic.c:469
--- 0x4037de3d: esp_system_abort at G:/sdk/Espressif/frameworks/esp-idf-v5.5/components/esp_system/port/esp_system_chip.c:87
--- 0x40383c66: abort at G:/sdk/Espressif/frameworks/esp-idf-v5.5/components/newlib/src/abort.c:38
--- 0x4037de33: _esp_error_check_failed at G:/sdk/Espressif/frameworks/esp-idf-v5.5/components/esp_system/esp_err.c:49
--- 0x4200a1b8: gzn::input::backend::(anonymous namespace)::on_hid_device_connected(hid_interface*, hid_host_driver_event_t, void*) at G:/projects/embedded/esp32/test-proj/main/sources/gzn/input/backend/usb.cpp:106
--- 0x4200b3d7: hid_host_user_device_callback at G:/projects/embedded/esp32/test-proj/managed_components/espressif__usb_host_hid/hid_host.c:329
--- (inlined by) hid_host_notify_interface_connected at G:/projects/embedded/esp32/test-proj/managed_components/espressif__usb_host_hid/hid_host.c:423
--- (inlined by) hid_host_interface_list_create at G:/projects/embedded/esp32/test-proj/managed_components/espressif__usb_host_hid/hid_host.c:478
--- (inlined by) hid_host_device_init_attempt at G:/projects/embedded/esp32/test-proj/managed_components/espressif__usb_host_hid/hid_host.c:508
--- (inlined by) client_event_cb
*/
	if (HID_SUBCLASS_BOOT_INTERFACE == dev_params.sub_class) {
		ESP_ERROR_CHECK(hid_class_request_set_protocol(hid_device_handle, HID_REPORT_PROTOCOL_BOOT));
		if (HID_PROTOCOL_KEYBOARD == dev_params.proto) {
			ESP_ERROR_CHECK(hid_class_request_set_idle(hid_device_handle, 0, 0));
		}
	}

	for (auto &handler : ctx->event_handlers) {
		if (handler.on_connect) {
			handler.on_connect(device, handler.user_data);
		}
	}

	ESP_ERROR_CHECK(hid_host_device_start(hid_device_handle));
}
} // namespace

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

	vTaskDelay(10); // Short delay to allow clients clean-up
	ESP_ERROR_CHECK(usb_host_uninstall());
	// vTaskDelete(nullptr);
}


auto usb::startup(context &ctx) -> bool {
	if (g_usb_task != nullptr) {
		return false;
	}

	const auto task_created{ xTaskCreatePinnedToCore(
		usb_lib_task,
		"usb_events",
		4096,
		xTaskGetCurrentTaskHandle(),
		2, &g_usb_task, 0
	) };
	if (task_created != pdTRUE) {
		return false;
	}
	ulTaskNotifyTake(false, 1000);
	ESP_LOGI(TAG, "USB hast task was created");

	const hid_host_driver_config_t hid_host_driver_config{
		.create_background_task = true,
		.task_priority          = 5,
		.stack_size             = 4096,
		.core_id                = 0,
		.callback               = on_hid_device_connected,
		.callback_arg           = &ctx
	};

	ESP_ERROR_CHECK(hid_host_install(&hid_host_driver_config));
	ESP_LOGI(TAG, "HID was intalled");

	return true;
}

void usb::shutdown() {
	vTaskDelete(g_usb_task);
	ESP_ERROR_CHECK(hid_host_uninstall());
}

} // namespace gzn::input::backend

