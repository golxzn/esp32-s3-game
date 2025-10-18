#pragma once

#include "gzn/input/actions.hpp"
#include "gzn/input/backend.hpp"
#include "gzn/input/backend/translators.hpp"

namespace gzn::input {

inline constexpr size_t MAX_EVENT_HANDLERS   { 6 };
inline constexpr size_t MAX_CONNECTED_DEVICES{ 2 };

struct device_info {
	uintptr_t           id{};
	backend::translator translator{ nullptr };
	uint16_t            vendor_id{};
	uint16_t            product_id{};
};


using connection_event_handler     = void(*)(const device_info &info, void *user);
using disconnection_event_handler  = void(*)(const device_info &info, void *user);
using transfer_error_event_handler = void(*)(const device_info &info, void *user);

struct event_handler {
	connection_event_handler     on_connect       { nullptr };
	disconnection_event_handler  on_disconnect    { nullptr };
	transfer_error_event_handler on_transfer_error{ nullptr };
	void                        *user_data{ nullptr };

	inline bool empty() const {
		return on_connect    == nullptr
			&& on_disconnect == nullptr;
	}

	[[nodiscard]] constexpr bool operator==(const event_handler &other) const = default;
};



struct context {
	std::array<event_handler, MAX_EVENT_HANDLERS>  event_handlers{};
	core::enum_array<action_type, action_info>     actions{};
	std::array<device_info, MAX_CONNECTED_DEVICES> devices{};
	backend_mask                                   backends{};


	[[gnu::always_inline]] [[nodiscard]]
	inline auto find_device(const uintptr_t id) -> device_info * {
		for (auto &device : devices) {
			if (device.id == id) {
				return &device;
			}
		}
		return nullptr;
	}

	[[gnu::always_inline]] [[nodiscard]]
	inline auto find_device(const uintptr_t id) const -> const device_info * {
		for (const auto &device : devices) {
			if (device.id == id) {
				return &device;
			}
		}
		return nullptr;
	}

	auto push_event_handler(const event_handler &handler) -> bool;
	auto push_device(const device_info &device) -> bool;
};

} // namespace gzn::input

