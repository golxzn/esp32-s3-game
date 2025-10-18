#pragma once

#include "gzn/core/math.hpp"

namespace gzn::input {

#pragma region ANALOG

using analog_left_stick  = vec2s16;
using analog_right_stick = vec2s16;
using analog_button      = uint8_t;

struct analog_sticks {
	analog_left_stick  left{};
	analog_right_stick right{};
};

struct analog_buttons {
	analog_button l2{};
	analog_button r2{};
};

struct analogs_data {
	analog_sticks  sticks{};
	analog_buttons buttons{};
};

#pragma endregion ANALOG


#pragma region BUTTONS

struct buttons_data {
	uint8_t up    : 1 {};
	uint8_t down  : 1 {};
	uint8_t left  : 1 {};
	uint8_t right : 1 {};

	union { uint8_t triangle  : 1 {}; uint8_t Y : 1; };
	union { uint8_t cross     : 1 {}; uint8_t A : 1; };
	union { uint8_t square    : 1 {}; uint8_t X : 1; };
	union { uint8_t circle    : 1 {}; uint8_t B : 1; };

	uint8_t l1 : 1 {};
	uint8_t l2 : 1 {};
	uint8_t l3 : 1 {};

	uint8_t r1 : 1 {};
	uint8_t r2 : 1 {};
	uint8_t r3 : 1 {};

	union { uint8_t ps : 1 {}; uint8_t xbox : 1; };
	uint8_t options : 1 {};
	uint8_t share   : 1 {};
};

#pragma endregion BUTTONS


struct status_data {
	uint8_t battery{};
	uint8_t charging   : 1 {};
	uint8_t audio      : 1 {};
	uint8_t microphone : 1 {};
};



struct config_data {
	uint8_t small_rumble{};
	uint8_t large_rumple{};
	vec3u8 color{};
	uint8_t flash_on{};
	uint8_t flash_off{}; // 255 = 2.5 sec
};

struct event {
	buttons_data buttons_down{};
	buttons_data butotns_up{};
	analogs_data analog{};
};

} // namespace gzn::input

