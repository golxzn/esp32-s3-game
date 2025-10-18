#pragma once

#include <cstdint>
#include <esp_timer.h>

#include "gzn/core/math.hpp"
#include "gzn/core/enum_array.hpp"

namespace gzn::input {

enum class action_type : uint8_t {
	invalid,

	horizontal_move,
	vertical_move,
	attack,
	use,
	pause,

	COUNT
};

inline constexpr uint64_t ACTION_JUST_THESHOLD_MICROSECONDS{ 50'000u };

struct action_info {
	uint64_t timestamp{};
	union {
		uint64_t payload{};
		int8_t   horizontal;
		int8_t   vertical;
		bool     pressed;
	};

	[[gnu::always_inline]]
	inline bool just_pressed(const uint64_t theshold) const {
		const auto diff{ static_cast<uint64_t>(esp_timer_get_time()) - timestamp };
		return pressed && diff <= theshold;
	}

	[[gnu::always_inline]]
	inline bool just_released(const uint64_t theshold) const {
		return !pressed && (static_cast<uint64_t>(esp_timer_get_time()) - timestamp) <= theshold;
	}
};

using action_array = core::enum_array<action_type, action_info>;

} // namespace gzn::input

