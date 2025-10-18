#include <esp_log.h>

#include "gzn/input/context.hpp"

namespace gzn::input {

constexpr auto TAG{ "input::context" };

auto context::push_event_handler(const event_handler &new_handler) -> bool {
	if (std::empty(new_handler)) {
		return false;
	}

	event_handler *insert_pos{ nullptr };
	for (auto &handler : event_handlers) {
		if (handler == new_handler) {
			ESP_LOGI(TAG, "Handler already exists");
			return false;
		}

		if (!insert_pos && std::empty(handler)) {
			insert_pos = &handler;
		}
	}
	if (insert_pos == nullptr) {
		ESP_LOGI(TAG, "Cannot add new handler. All slots are taken");
		return false; // no space
	}

	*insert_pos = new_handler;
	return true;
}

auto context::push_device(const device_info &new_device) -> bool {
	device_info *selected{ nullptr };
	for (auto &device : devices) {
		if (device.id == new_device.id) {
			ESP_LOGE(TAG, "Device with id 0x%X is already connected!", new_device.id);
			return false;
		}

		if (device.id == 0 && selected == nullptr) {
			selected = &device;
		}
	}

	if (selected == nullptr) {
		ESP_LOGW(TAG, "Cannot connect new device. All slots are taken!");
		return false;
	}

	*selected = new_device;

	return true;
}

} // namespace gzn::input

