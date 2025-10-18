#include <esp_log.h>

#include "gzn/input/manager.hpp"

#include "gzn/input/backend/usb.hpp"
// #include "gzn/input/backend/bluetooth.hpp"

namespace gzn::input {

constexpr auto TAG{ "input::manager" };

auto manager::initialize(const backend_mask backends) -> init_error {
	if (ctx) {
		return init_error::already_initialized;
	}

	if (!backends) {
		return init_error::invalid_argument;
	}

	ctx = std::make_unique<context>();

	backend_mask initialized_backends{};
	if ((backend_type::usb & backends) && initialize_usb()) {
		initialized_backends |= backend_type::usb;
	}

	// if ((backend_type::bluetooth & backends) && initialize_bluetooth()) {
	// 	initialized_backends |= backend_type::bluetooth;
	// }

	ctx->backends = initialized_backends;
	if (backends != initialized_backends) {
		return init_error::partial_initialization;
	}

	return init_error::ok;
}

void manager::destroy() {
	/// @todo disconnect everything

	if (backend_type::usb & ctx->backends) {
		destroy_usb();
	}
	// if (backend_type::bluetooth & ctx->backends) {
	// 	destroy_bluetooth();
	// }
	ctx.reset();
}


auto manager::initialized_backends() -> backend_mask {
	return ctx ? ctx->backends : backend_mask{};
}

auto manager::get_horizontal_action(const action_type action) -> int8_t {
	return ctx->actions[action].horizontal;
}

auto manager::get_vertical_action(const action_type action) -> int8_t {
	return ctx->actions[action].vertical;
}

auto manager::is_action_pressed(const action_type action) -> bool {
	return ctx->actions[action].pressed;
}

auto manager::is_action_released(const action_type action) -> bool {
	return !ctx->actions[action].pressed;
}

auto manager::is_action_just_pressed(
	const action_type action, const uint64_t theshold
) -> bool {
	return ctx->actions[action].just_pressed(theshold);
}

auto manager::is_action_just_released(
	const action_type action, const uint64_t theshold
) -> bool {
	return ctx->actions[action].just_released(theshold);
}

auto manager::follow_connection_events(
	connection_event_handler connection_handler,
	disconnection_event_handler disconnection_handler,
	void *user
) -> bool {
	return ctx->push_event_handler(event_handler{
		.on_connect    = connection_handler,
		.on_disconnect = disconnection_handler,
		.user_data     = user
	});
}

auto manager::initialize_usb() -> bool {
	ESP_LOGI(TAG, "Startup USB backend");
	return backend::usb::startup(*ctx);
}

void manager::destroy_usb() {
	backend::usb::shutdown();
}

} // namespace gzn::input

