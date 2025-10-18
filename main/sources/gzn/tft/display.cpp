#include <utility>

#include <esp_log.h>
#include <driver/gpio.h>
#include <soc/gpio_struct.h>

#include "gzn/tft/display.hpp"

#include "gzn/utils.hpp"
#include "gzn/tft/pins.hpp"

namespace gzn::tft {

/** @note In the first place, I stupidly designed "backend" shit to support
 * other displays, but at the end of the day, it's too resource-consuming to
 * support it unless it's at compile time. And since this display class should
 * be rewrited with Octal-SPI or something better, I don't care much.
 */

inline constexpr auto TAG{ "[tft::display]" };

display::display() {
	ESP_ERROR_CHECK(initialize_gpio());
#if defined(GZN_TFT_USE_DEDICATED_GPIO)
	m_output_bus = make_gpio_bundle();
#endif // defined(GZN_TFT_USE_DEDICATED_GPIO)
}

display::~display() {
#if defined(GZN_TFT_USE_DEDICATED_GPIO)
	dedic_gpio_del_bundle(m_output_bus);
#endif // defined(GZN_TFT_USE_DEDICATED_GPIO)
}


void display::viewport(vec2u16 lt_bound, vec2u16 rb_bound) noexcept {
	GPIO.out_w1tc = (1 << pins::CD);
	send_bits8(std::to_underlying(command_id::SET_COLUMN_ADDRESS));
	GPIO.out_w1ts = (1 << pins::CD);
	send_bits16(lt_bound.x);
	send_bits16(rb_bound.x);

	GPIO.out_w1tc = (1 << pins::CD);
	send_bits8(std::to_underlying(command_id::SET_PAGE_ADDRESS));
	GPIO.out_w1ts = (1 << pins::CD);
	send_bits16(lt_bound.y);
	send_bits16(rb_bound.y);

	GPIO.out_w1tc = (1 << pins::CD);
	send_bits8(std::to_underlying(command_id::WRITE_MEMORY_START));
	GPIO.out_w1ts = (1 << pins::CD);
}

void display::fill_rect(vec2u16 pos, vec2u16 size, color clr) noexcept {
	GPIO.out_w1tc = 1u << tft::pins::CS;

	viewport(pos, pos + size - vec2u16::make(1u));

	for (uint32_t i{ 2 }; i < constants::PIXELS_COUNT; ++i) {
		send_bits16(clr);
	}

	GPIO.out_w1ts = 1u << tft::pins::CS;
}

[[gnu::always_inline]]
inline void display::clear_screen(color clr) noexcept {
	fill_rect({}, m_size, clr);
}

void display::send_buffer_rect(
	vec2u16 pos, vec2u16 size,
	const std::span<const color> buffer,
	const uint16_t pixel_size
) noexcept {
	GPIO.out_w1tc = 1u << tft::pins::CS;

	viewport(pos, pos + size - vec2u16::make(1u));

	const auto columns_count{ static_cast<size_t>(size.w / pixel_size) };
	// const auto rows_count{ static_cast<size_t>(size.h / pixel_size) };

	for (size_t c{}; c < size.h; ++c) {
		const size_t offset{ (c / pixel_size) * columns_count };
		for (size_t r{}; r < size.w; ++r) {
			send_bits16(buffer[offset + r / pixel_size]);
		}
	}

	GPIO.out_w1ts = 1u << tft::pins::CS;
}

void display::configure(const std::span<const command> commands) noexcept {
	using namespace utils::literals; // for _ms

	// Put SPI bus in known state for TFT with CS tied low
	execute(command_id::NOP);

	reset();

	gpio_set_level(pins::CS, 0);

	execute(command_id::SOFT_RESET);
	vTaskDelay(120_ms); // wait reset complition

	execute(command_id::EXIT_SLEEP_MODE);
	vTaskDelay(120_ms); // Sleep out, also SW reset

	execute(commands);
	vTaskDelay(150_ms);

	gpio_set_level(pins::CS, 1);
}

void display::set_orientation(orientation value) noexcept {
	GPIO.out_w1tc = 1u << pins::CS;

	execute(command_id::SET_MEMORY_ACCESS);

	enum memory_access : uint8_t {
		row_address_order        = 0x80, // MY
		column_address_order     = 0x40, // MX
		swap_row_column          = 0x20, // MV
		vertical_refresh_order   = 0x10, // ML

		rgb_color_order          = 0x08, // BRG
		horizontal_refresh_order = 0x04, // MH
		reserved_1               = 0x02,
		reserved_0               = 0x01,
	};
	constexpr uint8_t portrait_value { rgb_color_order | column_address_order };
	constexpr uint8_t landscape_value{ rgb_color_order | swap_row_column      };
	constexpr uint8_t inverted_portrait_value{ rgb_color_order | row_address_order };
	constexpr uint8_t inverted_landscape_value{
		rgb_color_order | column_address_order | row_address_order | swap_row_column
	};

	switch (value) {
		case orientation::portrait:
			write_data(portrait_value);
			m_size = constants::DISPLAY_SIZE;
			break;
		case orientation::landscape:
			write_data(landscape_value);
			m_size = vec2u16{
				.w = constants::DISPLAY_SIZE.h,
				.h = constants::DISPLAY_SIZE.w
			};
			break;
		case orientation::inverted_portrait:
			write_data(inverted_portrait_value);
			m_size = constants::DISPLAY_SIZE;
			break;
		case orientation::inverted_landscape:
			write_data(inverted_landscape_value);
			m_size = vec2u16{
				.w = constants::DISPLAY_SIZE.h,
				.h = constants::DISPLAY_SIZE.w
			};
			break;

		// Other orientationals or mental issues?

		default: break;
	}

	utils::delay_microsecons(10);

	GPIO.out_w1ts = 1u << pins::CS;
}

[[gnu::always_inline]]
inline void display::reset() noexcept {
	using namespace utils::literals;

	gpio_set_level(pins::RESET, 1);
	vTaskDelay(5_ms);

	gpio_set_level(pins::RESET, 0);
	vTaskDelay(20_ms);

	gpio_set_level(pins::RESET, 1);
	vTaskDelay(150_ms); // wait reset complition
}

[[gnu::always_inline]]
inline void display::execute(const std::span<const command> commands) noexcept {
	for (const auto &cmd : commands) {
		execute(cmd);
	}
}

[[gnu::always_inline]]
inline void display::execute(const command &cmd) noexcept {
	execute(cmd.id);
	for (uint8_t i{}; i < cmd.params_count; ++i) {
		write_data(cmd.params[i]);
	}
}

[[gnu::always_inline]]
inline void display::execute(const command_id cmd) noexcept {
	GPIO.out_w1tc = 1 << tft::pins::CS;
	GPIO.out_w1tc = 1 << tft::pins::CD; // enter command mode

	send_bits8(std::to_underlying(cmd));

	GPIO.out_w1ts = 1 << tft::pins::CD;
	GPIO.out_w1ts = 1 << tft::pins::CS;
}

[[gnu::always_inline]]
inline void display::write_data(const uint8_t data) noexcept {
	GPIO.out_w1tc = 1 << tft::pins::CS;
	GPIO.out_w1ts = 1 << tft::pins::CD;

	send_bits8(data);

	// GPIO.out_w1tc = 1 << tft::pins::CS;
	GPIO.out_w1ts = 1 << tft::pins::CS;
}


[[gnu::always_inline]]
inline void display::send_bits8(const uint8_t data) noexcept {
	GPIO.out_w1tc = pins::DATA_PINS_MASK | (1 << pins::WRITE); // Clear bus & write pin

#if defined(GZN_TFT_USE_DEDICATED_GPIO)
	dedic_gpio_bundle_write(m_output_bus, 0xFF, data);
	asm volatile("nop\nnop\nnop\nnop\nnop\nnop"); // Prevent gliches
#else
	GPIO.out_w1ts = pins::make_mask(data); // Set the bus pins
#endif // defined(GZN_TFT_USE_DEDICATED_GPIO)

	GPIO.out_w1ts = 1 << pins::WRITE; // Set the Write pin. It's like a FIRE button
}

[[gnu::always_inline]]
inline void display::send_bits16(const uint16_t bits) noexcept {
	send_bits8(static_cast<uint8_t>(bits >> 8));
	send_bits8(static_cast<uint8_t>(bits));
}

auto display::initialize_gpio() -> esp_err_t {
	const gpio_config_t io_conf{
		.pin_bit_mask = pins::PINS_MASK,
		.mode         = GPIO_MODE_OUTPUT,
		.pull_up_en   = GPIO_PULLUP_DISABLE,
		.pull_down_en = GPIO_PULLDOWN_DISABLE,
		.intr_type    = GPIO_INTR_DISABLE
	};
	if (const auto error{ gpio_config(&io_conf) }; error != ESP_OK) [[unlikely]] {
		ESP_LOGE(TAG, "Cannot configure GPIO: %s", esp_err_to_name(error));
		return error;
	}
	ESP_LOGI(TAG, "GPIO Configured");

	GPIO.out_w1ts = io_conf.pin_bit_mask;

	return ESP_OK;
}

#if defined(GZN_TFT_USE_DEDICATED_GPIO)

auto display::make_gpio_bundle() -> dedic_gpio_bundle_handle_t {
	constexpr std::array<int, 8> gpio_data_pins{
		pins::D0, pins::D1, pins::D2, pins::D3,
		pins::D4, pins::D5, pins::D6, pins::D7
	};

	const dedic_gpio_bundle_config_t config{
		.gpio_array = std::data(gpio_data_pins),
		.array_size = std::size(gpio_data_pins),
		.flags = {
			.in_en      = 0,
			.in_invert  = 0,
			.out_en     = 1,
			.out_invert = 0
		}
	};

	dedic_gpio_bundle_handle_t bundle{};
	ESP_ERROR_CHECK(dedic_gpio_new_bundle(&config, &bundle));
	return bundle;
}

#endif // defined(GZN_TFT_USE_DEDICATED_GPIO)


} // namespace gzn::tft

