#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace gzn::input { struct context; }

namespace gzn::input::backend {

struct usb {
	static constexpr size_t recieve_buffer_length{ 64 };

	static auto startup(context &ctx) -> bool;
	static void shutdown();
};

} // namespace gzn::input::backend

