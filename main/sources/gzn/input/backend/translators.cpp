
#include <usb/hid_host.h>
#include <usb/hid_usage_keyboard.h>

#include "gzn/input/backend/translators.hpp"

namespace gzn::input::backend {

auto select_translator(const uint8_t hid_protocol, const uint32_t VID_PID) -> translator {
	switch (hid_protocol) {
		case HID_PROTOCOL_KEYBOARD: return keyboard_usb_translator;
		// case HID_PROTOCOL_MOUSE   : return mouse_usb_translator;

		default: break;
	}

	constexpr uint32_t duansense_vid_pid{ 0x054C'0CE6 };
	if (duansense_vid_pid == VID_PID) {
		return dualsense_usb_translator;
	}

	return nullptr;
}


} // namespace gzn::input::backend

