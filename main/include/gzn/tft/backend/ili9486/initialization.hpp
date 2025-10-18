#pragma once

#include "gzn/tft/backend/ili9486/commands.hpp"

namespace gzn::tft::backend::ili9486 {

struct display_config {
	enum memory_access : uint8_t {
		column_address_order     = 0x80,
		row_address_order        = 0x40,
		swap_row_column          = 0x20,
		vertical_refresh_order   = 0x10,

		rgb_color_order          = 0x08,
		horizontal_refresh_order = 0x04,
		reserved_1               = 0x02,
		reserved_0               = 0x01,
	};

	enum color_order : uint8_t {
		brg = 0x00,
		rgb = rgb_color_order
	};
	enum orientation : uint8_t {
		portrait           = row_address_order,
		landscape          = swap_row_column,
		inverted_portrait  = column_address_order,
		inverted_landscape = row_address_order | column_address_order | swap_row_column,
	};

	enum pixel_format : uint8_t {
		R6G6B6 = 0x66,
		R5G6B5 = 0x55,
	};
};


constexpr std::array initialization_sequence{
	command{ command_id::SET_PIXEL_FORMAT,            1u, { display_config::R5G6B5 } },

	command{ command_id::EXT_POWER_CONTROL_1,         2u, { 0x0E, 0x0E } },
	command{ command_id::EXT_POWER_CONTROL_2,         2u, { 0x41, 0x00 } },
	command{ command_id::EXT_POWER_CONTROL_3,         1u, { 0x55 } },

	command{ command_id::EXT_VCOM_CONTROL,            4u, { 0x00, 0x00, 0x00, 0x00 } },

	// command{ command_id::EXT_POSITIVE_GAMMA_CONTROL, 15u, { 0x0F, 0x1F, 0x1C, 0x0C, 0x0F, 0x08, 0x48, 0x98, 0x37, 0x0A, 0x13, 0x04, 0x11, 0x0D, 0x00 } },
	// command{ command_id::EXT_NEGATIVE_GAMMA_CONTROL, 15u, { 0x0F, 0x32, 0x2E, 0x0B, 0x0D, 0x05, 0x47, 0x75, 0x37, 0x06, 0x10, 0x03, 0x24, 0x20, 0x00 } },

	// command{ command_id::EXT_POSITIVE_GAMMA_CONTROL, 15u, { 0x00, 0x2C, 0x2C, 0x0B, 0x0C, 0x04, 0x4C, 0x64, 0x36, 0x03, 0x0E, 0x01, 0x10, 0x01, 0x00 } },
	// command{ command_id::EXT_NEGATIVE_GAMMA_CONTROL, 15u, { 0x0F, 0x37, 0x37, 0x0C, 0x0F, 0x05, 0x50, 0x32, 0x36, 0x04, 0x0B, 0x00, 0x19, 0x14, 0x0F } },

	command{ command_id::EXT_FRAME_RATE_CONTROL_NORMAL_MODE_FULL_COLORS, 2u, { 0xE0, 0x1F } },

	command{ command_id::EXIT_INVERT_MODE },
	command{ command_id::SET_MEMORY_ACCESS,           1u, { display_config::portrait } },

	// command{ command_id::EXT_DISPLAY_FUNCTION_CONTROL, 3u, { 0x00, 0x02, 59 } }, // Actual display height = (h+1)*8 so (59+1)*8=480

	command{ command_id::ENABLE_DISPLAY },
};

} // namespace gzn::tft::backend::ili9486

