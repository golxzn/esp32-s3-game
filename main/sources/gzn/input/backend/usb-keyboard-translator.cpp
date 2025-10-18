#include <cstring>
#include <algorithm>

#include <usb/hid_host.h>
#include <usb/hid_usage_keyboard.h>

#include "gzn/input/backend/translators.hpp"

namespace gzn::input::backend {

namespace {

static auto select_action(const hid_key_t key, action_array &actions) -> action_info * {
	switch (key) {
		case HID_KEY_LEFT: [[fallthrough]];
		case HID_KEY_A:
			return &actions[action_type::horizontal_move];

		case HID_KEY_RIGHT: [[fallthrough]];
		case HID_KEY_D:
			return &actions[action_type::horizontal_move];

		case HID_KEY_DOWN: [[fallthrough]];
		case HID_KEY_S:
			return &actions[action_type::vertical_move];

		case HID_KEY_UP: [[fallthrough]];
		case HID_KEY_W:
			return &actions[action_type::vertical_move];

		case HID_KEY_ESC:
			return &actions[action_type::pause];

		case HID_KEY_E: [[fallthrough]];
		case HID_KEY_SPACE:
			return &actions[action_type::use];

		default: break;
	}
	return nullptr;
};

static void on_key_pressed(const hid_key_t key, action_info &action) {
	switch (key) {
		case HID_KEY_LEFT: [[fallthrough]];
		case HID_KEY_A:
			action.horizontal = -128;
			break;

		case HID_KEY_RIGHT: [[fallthrough]];
		case HID_KEY_D:
			action.horizontal = 127;
			break;

		case HID_KEY_DOWN: [[fallthrough]];
		case HID_KEY_S:
			action.vertical = 127;
			break;

		case HID_KEY_UP: [[fallthrough]];
		case HID_KEY_W:
			action.vertical = -128;
			break;

		default: break;
	}
}

static void on_key_released(const hid_key_t key, action_info &action) {
	switch (key) {
		case HID_KEY_LEFT: [[fallthrough]];
		case HID_KEY_A: [[fallthrough]];
		case HID_KEY_RIGHT: [[fallthrough]];
		case HID_KEY_D:
			action.horizontal = 0;
			break;

		case HID_KEY_DOWN: [[fallthrough]];
		case HID_KEY_S: [[fallthrough]];
		case HID_KEY_UP: [[fallthrough]];
		case HID_KEY_W:
			action.vertical = 0;
			break;

		default: break;
	}
}

} // namespace

void keyboard_usb_translator(const std::span<const uint8_t> data, action_array &actions) {
	if (std::size(data) < sizeof(hid_keyboard_input_report_boot_t)) {
		return;
	}

	const auto kb_report{
		reinterpret_cast<const hid_keyboard_input_report_boot_t *>(std::data(data))
	};

	const auto timestamp{ static_cast<uint64_t>(esp_timer_get_time()) };

	static std::array<uint8_t, HID_KEYBOARD_KEY_MAX> previous_keys{};

	for (size_t i{}; i < HID_KEYBOARD_KEY_MAX; ++i) {
		const auto prev_key{ static_cast<hid_key_t>(previous_keys[i]) };
		if (auto action{ select_action(prev_key, actions) };
			action && std::ranges::contains(kb_report->key, prev_key)
		) {
			action->pressed = false;
			action->timestamp = timestamp;
			on_key_released(prev_key, *action);
		}

		const auto key{ static_cast<hid_key_t>(kb_report->key[i]) };
		if (auto action{ select_action(key, actions) }; action && !std::ranges::contains(previous_keys, key)) {
			action->pressed = true;
			action->timestamp = timestamp;
			on_key_pressed(prev_key, *action);
		}
	}

	std::memcpy(std::data(previous_keys), kb_report->key, HID_KEYBOARD_KEY_MAX);
}

} // namespace gzn::input::backend

