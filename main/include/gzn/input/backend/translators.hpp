#pragma once

#include <span>

#include "gzn/input/actions.hpp"

namespace gzn::input::backend {

using translator = void(*)(
	const std::span<const uint8_t> data,
	action_array &actions
);


void keyboard_usb_translator(
	const std::span<const uint8_t> data,
	action_array &actions
);

void dualsense_usb_translator(
	const std::span<const uint8_t> data,
	action_array &actions
);

[[nodiscard]]
auto select_translator(
	const uint8_t hid_protocol,
	const uint32_t VID_PID
) -> translator;

} // namespace gzn::input::backend

