#pragma once

#include <span>
#include <array>
#include <cstdint>

namespace gzn::tft::backend::ili9486 {

enum class command_id : uint8_t {
	NOP                                     = 0x00,
	SOFT_RESET                              = 0x01,
	GET_RED_CHANNEL                         = 0x06,
	GET_GREEN_CHANNEL                       = 0x07,
	GET_BLUE_CHANNEL                        = 0x08,
	GET_POWER_MODE                          = 0x0A,
	GET_ADDRESS_MODE                        = 0x0B,
	GET_PIXEL_FORMAT                        = 0x0C,
	GET_DISPLAY_MODE                        = 0x0D,
	GET_SIGNAL_MODE                         = 0x0E,
	GET_DIAGNOSTIC_RESULT                   = 0x0F,
	ENTER_SLEEP_MODE                        = 0x10,
	EXIT_SLEEP_MODE                         = 0x11,
	ENTER_PARTIAL_MODE                      = 0x12,
	ENTER_NORMAL_MODE                       = 0x13,
	EXIT_INVERT_MODE                        = 0x20,
	ENTER_INVERT_MODE                       = 0x21,
	DISABLE_DISPLAY                         = 0x28,
	ENABLE_DISPLAY                          = 0x29,
	SET_COLUMN_ADDRESS                      = 0x2A,
	SET_PAGE_ADDRESS                        = 0x2B,
	WRITE_MEMORY_START                      = 0x2C,
	READ_MEMORY_START                       = 0x2E,
	SET_PARTIAL_AREA                        = 0x30,
	SET_SCROLL_AREA                         = 0x33,
	SET_TEAR_OFF                            = 0x34,
	SET_TEAR_ON                             = 0x35,
	SET_MEMORY_ACCESS                       = 0x36,
	SET_SCROLL_START                        = 0x37,
	EXIT_IDLE_MODE                          = 0x38,
	ENTER_IDLE_MODE                         = 0x39,
	SET_PIXEL_FORMAT                        = 0x3A,
	WRITE_MEMORY_CONTINUE                   = 0x3C,
	READ_MEMORY_CONTINUE                    = 0x3E,
	SET_TEAR_SCANLINE                       = 0x44,
	GET_SCANLINE                            = 0x45,
	SET_DISPLAY_BRIGHTESS                   = 0x51,
	GET_DISPLAY_BRIGHTESS                   = 0x52,
	SET_CTRL_DISPLAY                        = 0x53,
	GET_CTRL_DISPLAY                        = 0x54,
	SET_CONTENT_ADAPTIVE_BRIGHTNESS_CONTROL = 0x55,
	GET_CONTENT_ADAPTIVE_BRIGHTNESS_CONTROL = 0x56,
	SET_CABC_MINIMUM_BRIGHTNESS             = 0x5E,
	GET_CABC_MINIMUM_BRIGHTNESS             = 0x5F,
	READ_FIRST_CHECKSUM                     = 0xAA,
	READ_CONTINUE_CHECKSUM                  = 0xAF,

// Extended Command Set (EXT
	EXT_INTERFACE_MODE_CONTROL                      = 0xB0,
	EXT_FRAME_RATE_CONTROL_NORMAL_MODE_FULL_COLORS  = 0xB1,
	EXT_FRAME_RATE_CONTROL_IDLE_MODE_8_COLORS       = 0xB2,
	EXT_FRAME_RATE_CONTROL_PARTIAL_MODE_FULL_COLORS = 0xB3,
	EXT_DISPLAY_INVERSION_CONTROL                   = 0xB4,
	EXT_BLANKING_PORCH_CONTROL                      = 0xB5,
	EXT_DISPLAY_FUNCTION_CONTROL                    = 0xB6,
	EXT_ENTRY_MODE_SET                              = 0xB7,
	EXT_POWER_CONTROL_1                             = 0xC0,
	EXT_POWER_CONTROL_2                             = 0xC1,
	EXT_POWER_CONTROL_3                             = 0xC2,
	EXT_POWER_CONTROL_4                             = 0xC3,
	EXT_POWER_CONTROL_5                             = 0xC4,
	EXT_VCOM_CONTROL                                = 0xC5,
	EXT_CABC_CONTROL_1                              = 0xC6,
	EXT_CABC_CONTROL_2                              = 0xC8,
	EXT_CABC_CONTROL_3                              = 0xC9,
	EXT_CABC_CONTROL_4                              = 0xCA,
	EXT_CABC_CONTROL_5                              = 0xCB,
	EXT_CABC_CONTROL_6                              = 0xCC,
	EXT_CABC_CONTROL_7                              = 0xCD,
	EXT_CABC_CONTROL_8                              = 0xCE,
	EXT_CABC_CONTROL_9                              = 0xCF,
	EXT_NV_MEMORY_WRITE                             = 0xD0,
	EXT_NV_MEMORY_PROTECTION_KEY                    = 0xD1,
	EXT_NV_MEMORY_STATUS_READ                       = 0xD2,
	EXT_READ_ID4                                    = 0xD3,
	EXT_POSITIVE_GAMMA_CONTROL                      = 0xE0,
	EXT_NEGATIVE_GAMMA_CONTROL                      = 0xE1,
	EXT_DIGITAL_GAMMA_CONTROL_1                     = 0xE2,
	EXT_DIGITAL_GAMMA_CONTROL_2                     = 0xE3,
	EXT_SPI_READ_COMMAND_SETTING                    = 0xFB,

	INVALID                                         = 0xFF
};

struct command {
	command_id id{ command_id::INVALID };
	uint8_t params_count{};
	std::array<uint8_t, 15> params{};
};

[[gnu::always_inline]]
inline auto as_span(const command &cmd) noexcept -> std::span<const uint8_t> {
	return std::span<const uint8_t>{
		std::data(cmd.params), static_cast<size_t>(cmd.params_count)
	};
}

} // namespace gzn::tft::backend::ili9486

