#pragma once

#include <memory>

#include "gzn/input/context.hpp"

namespace gzn::input {

enum class init_error : uint8_t {
	ok,
	already_initialized,
	invalid_argument,
	partial_initialization,
};


class manager {
public:
	[[nodiscard]] static auto initialize(const backend_mask backends) -> init_error;
	static void destroy();

	[[nodiscard]] static auto initialized_backends() -> backend_mask;

	[[nodiscard]] static auto get_horizontal_action(const action_type action) -> int8_t;
	[[nodiscard]] static auto get_vertical_action(const action_type action) -> int8_t;

	[[nodiscard]] static auto is_action_pressed(const action_type action) -> bool;
	[[nodiscard]] static auto is_action_released(const action_type action) -> bool;

	[[nodiscard]] static auto is_action_just_pressed(
		const action_type action,
		const uint64_t treshold = ACTION_JUST_THESHOLD_MICROSECONDS
	) -> bool;
	[[nodiscard]] static auto is_action_just_released(
		const action_type action,
		const uint64_t treshold = ACTION_JUST_THESHOLD_MICROSECONDS
	) -> bool;

	[[nodiscard]] auto follow_connection_events(
		connection_event_handler connection_handler,
		disconnection_event_handler disconnection_handler,
		void *user
	) -> bool;

private:
	inline static std::unique_ptr<context> ctx{};

	static bool initialize_usb();
	static void destroy_usb();
};

} // namespace gzn::input

