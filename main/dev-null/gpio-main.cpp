#include <span>
#include <array>
#include <cstdio>
#include <utility>
#include <cinttypes>

#include <sdkconfig.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <driver/gpio.h>
#include <driver/dedic_gpio.h>
#include <driver/spi_master.h>

#include <esp_log.h>
#include <esp_task_wdt.h>
#include <soc/gpio_struct.h>

#include "utils.hpp"
#define TFT_PINS_OPTIMIZE_MASK
#include "tft/pins.hpp"
#include "tft/ili9486/commands.hpp"

static constexpr const char *TAG{ "test-proj" };

#define USE_DEDIC_GPIO

constexpr uint32_t LOW { 0u };
constexpr uint32_t HIGH{ 1u };

namespace display {

constexpr uint16_t WIDTH { 320u };
constexpr uint16_t HEIGHT{ 480u };
constexpr uint32_t PIXELS_COUNT{ WIDTH * HEIGHT };

#if defined(USE_DEDIC_GPIO)

dedic_gpio_bundle_handle_t display_data_output_bus{};

[[gnu::always_inline]]
static inline void IRAM_ATTR send_bits8(const uint8_t data) {
	GPIO.out_w1tc = 1 << tft::pins::WRITE; // Clear write pin
	dedic_gpio_bundle_write(display_data_output_bus, 0xFF, data);
	GPIO.out_w1ts = 1 << tft::pins::WRITE; // Set the Write pin. It's like a FIRE button
}

#else

[[gnu::always_inline]]
static inline void IRAM_ATTR send_bits8(const uint8_t data) {
	GPIO.out_w1tc = tft::pins::DATA_PINS_MASK | (1 << tft::pins::WRITE); // Clear bus & write pin
	GPIO.out_w1ts = tft::pins::make_mask(data); // Set the bus pins
	GPIO.out_w1ts = 1 << tft::pins::WRITE; // Set the Write pin. It's like a FIRE button
}

#endif // defined(USE_DEDIC_GPIO)



[[gnu::always_inline]]
static inline void send_bits16(const uint16_t data) {
	send_bits8(data >> 8);
	send_bits8(data);
}

[[gnu::always_inline]]
static inline void send_bits32(const uint32_t data) {
	send_bits8(data >> 24);
	send_bits8(data >> 16);
	send_bits8(data >> 8);
	send_bits8(data);
}

[[gnu::always_inline]]
static inline void write_data(const uint8_t data) {
	GPIO.out_w1tc = 1 << tft::pins::CS;
	GPIO.out_w1ts = 1 << tft::pins::CD;

	send_bits8(data);

	// GPIO.out_w1tc = 1 << tft::pins::CS;
	GPIO.out_w1ts = 1 << tft::pins::CS;
}

static void execute_command(const tft::ili9486::command_id command) {
	GPIO.out_w1tc = 1 << tft::pins::CS;
	GPIO.out_w1tc = 1 << tft::pins::CD; // enter command mode

	send_bits8(std::to_underlying(command));

	GPIO.out_w1ts = 1 << tft::pins::CD;
	GPIO.out_w1ts = 1 << tft::pins::CS;
}



[[gnu::always_inline]]
static inline void execute_command(const tft::ili9486::command &cmd) {
	execute_command(cmd.id);
	for (uint8_t i{}; i < cmd.params_count; ++i) {
		write_data(cmd.params[i]);
	}
}

void execute_commands(const std::span<const tft::ili9486::command> commands) {
	for (size_t i{}; i < std::size(commands); ++i) {
		execute_command(commands[i]);
	}
}

void viewport(
	const uint16_t x0 = 0u,
	const uint16_t y0 = 0u,
	const uint16_t x1 = WIDTH - 1u,
	const uint16_t y1 = HEIGHT - 1u
) {
	using namespace tft::ili9486;

	GPIO.out_w1tc = (1 << tft::pins::CD);
	send_bits8(std::to_underlying(command_id::SET_COLUMN_ADDRESS));
	GPIO.out_w1ts = (1 << tft::pins::CD);
	send_bits16(x0);
	send_bits16(x1);

	GPIO.out_w1tc = (1 << tft::pins::CD);
	send_bits8(std::to_underlying(command_id::SET_PAGE_ADDRESS));
	GPIO.out_w1ts = (1 << tft::pins::CD);
	send_bits16(y0);
	send_bits16(y1);

	GPIO.out_w1tc = (1 << tft::pins::CD);
	send_bits8(std::to_underlying(command_id::WRITE_MEMORY_START));
	GPIO.out_w1ts = (1 << tft::pins::CD);

}

esp_err_t initialize_gpio() {
	const gpio_config_t io_conf{
		.pin_bit_mask = tft::pins::PINS_MASK,
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

	GPIO.out_w1ts = tft::pins::PINS_MASK;

#if defined(USE_DEDIC_GPIO)

	constexpr std::array<int, 8> gpio_pins{
		tft::pins::D0, tft::pins::D1,
		tft::pins::D2, tft::pins::D3,
		tft::pins::D4, tft::pins::D5,
		tft::pins::D6, tft::pins::D7
	};

	const dedic_gpio_bundle_config_t config{
		.gpio_array = std::data(gpio_pins),
		.array_size = std::size(gpio_pins),
		.flags = {
			.in_en      = 0,
			.in_invert  = 0,
			.out_en     = 1,
			.out_invert = 0
		}
	};

	if (const auto error{ dedic_gpio_new_bundle(&config, &display_data_output_bus) }; error != ESP_OK) {
		ESP_LOGE(TAG, "Cannot configure GPIO: %s", esp_err_to_name(error));
		return error;
	}
	ESP_LOGI(TAG, "Dedicated GPIO was configured");

#endif // defined(USE_DEDIC_GPIO)

	return ESP_OK;
}

void setup() {
	using namespace tft::ili9486;
	using namespace utils::literals; // for _ms

	// Put SPI bus in known state for TFT with CS tied low
	execute_command(command_id::NOP);


	gpio_set_level(tft::pins::RESET, HIGH);
	vTaskDelay(5_ms);

	gpio_set_level(tft::pins::RESET, LOW);
	vTaskDelay(20_ms);

	gpio_set_level(tft::pins::RESET, HIGH);
	vTaskDelay(150_ms); // wait reset complition


	// Selecting device
	gpio_set_level(tft::pins::CS, LOW);

	execute_command(command_id::SOFT_RESET);
	vTaskDelay(120_ms); // wait reset complition

	execute_command(command_id::EXIT_SLEEP_MODE);
	vTaskDelay(120_ms); // Sleep out, also SW reset

	ESP_LOGI(TAG, "Reset display");


	constexpr uint8_t pixel_format{
#if defined(DISPLAY_COLOR_FORMAT_R6X2G6X2B6X2)
		0x66 /* DPI(RGB Interface) = 18 bits/pixel, DBI(CPU Interface) = 18 bits/pixel */
#else
		0x55 /* DPI(RGB Interface) = 16 bits/pixel, DBI(CPU Interface) = 16 bits/pixel */
#endif // defined(DISPLAY_COLOR_FORMAT_R6X2G6X2B6X2)
	};

	constexpr uint8_t madctl_brg_pixel_order          { 1 << 3 };
	constexpr uint8_t madctl_row_column_exchange      { 1 << 5 }; // flip orientation
	constexpr uint8_t madctl_column_address_order_swap{ 1 << 6 };
	constexpr uint8_t madctl_row_address_order_swap   { 1 << 7 };
	constexpr uint8_t madctl_rotate_180_degrees{
		madctl_column_address_order_swap | madctl_row_address_order_swap
	};

	constexpr command configuration[] {
		{ command_id::SET_PIXEL_FORMAT,           1u, { pixel_format } },

		{ command_id::EXT_POWER_CONTROL_1,        2u, { 0x0E, 0x0E } },
		{ command_id::EXT_POWER_CONTROL_2,        2u, { 0x41, 0x00 } },
		{ command_id::EXT_POWER_CONTROL_3,        1u, { 0x55 } },

		{ command_id::EXT_VCOM_CONTROL,           4u, { 0x00, 0x00, 0x00, 0x00 } },
		{ command_id::EXT_POSITIVE_GAMMA_CONTROL, 15u, { 0x0F, 0x1F, 0x1C, 0x0C, 0x0F, 0x08, 0x48, 0x98, 0x37, 0x0A, 0x13, 0x04, 0x11, 0x0D, 0x00 } },// { 0x00, 0x2C, 0x2C, 0x0B, 0x0C, 0x04, 0x4C, 0x64, 0x36, 0x03, 0x0E, 0x01, 0x10, 0x01, 0x00 } },
		{ command_id::EXT_NEGATIVE_GAMMA_CONTROL, 15u, { 0x0F, 0x32, 0x2E, 0x0B, 0x0D, 0x05, 0x47, 0x75, 0x37, 0x06, 0x10, 0x03, 0x24, 0x20, 0x00 } }, // { 0x0F, 0x37, 0x37, 0x0C, 0x0F, 0x05, 0x50, 0x32, 0x36, 0x04, 0x0B, 0x00, 0x19, 0x14, 0x0F } },

		{ command_id::EXIT_INVERT_MODE },
		{ command_id::SET_MEMORY_ACCESS,          1u, {
			0x48 // 0b01001000 aka brg pixel order & row address order swap
			// madctl_row_column_exchange
		} },

		// { command_id::EXT_DISPLAY_FUNCTION_CONTROL, 3u, { 0x00, 0x02, 59 } }, // Actual display height = (h+1)*8 so (59+1)*8=480

		{ command_id::ENABLE_DISPLAY },
	};

	using namespace utils::literals; // for _ms

	execute_commands(configuration);
	vTaskDelay(150_ms);

	// Selecting device
	GPIO.out_w1ts = 1u << tft::pins::CS;

	ESP_LOGI(TAG, "Display Configured");
}

enum class rotation : uint8_t {
	portrait,
	landscape,
	inverted_portrait,
	inverted_landscape,
};

static void set_rotation(const rotation rot) {
	using namespace tft::ili9486;

	GPIO.out_w1tc = 1u << tft::pins::CS;

	execute_command(command_id::SET_MEMORY_ACCESS);

	constexpr uint8_t TFT_MAD_MY { 0x80 };
	constexpr uint8_t TFT_MAD_MX { 0x40 };
	constexpr uint8_t TFT_MAD_MV { 0x20 };
	constexpr uint8_t TFT_MAD_ML { 0x10 };
	constexpr uint8_t TFT_MAD_BGR{ 0x08 };
	constexpr uint8_t TFT_MAD_MH { 0x04 };
	constexpr uint8_t TFT_MAD_SS { 0x02 };
	constexpr uint8_t TFT_MAD_GS { 0x01 };
	constexpr uint8_t TFT_MAD_RGB{ 0x00 };

	switch (rot) {
		case rotation::portrait:
			write_data(static_cast<uint8_t>(TFT_MAD_BGR | TFT_MAD_MX));
			break;
		case rotation::landscape:
			write_data(static_cast<uint8_t>(TFT_MAD_BGR | TFT_MAD_MV));
			break;
		case rotation::inverted_portrait:
			write_data(static_cast<uint8_t>(TFT_MAD_BGR | TFT_MAD_MY));
			break;
		case rotation::inverted_landscape:
			write_data(static_cast<uint8_t>(TFT_MAD_BGR | TFT_MAD_MV | TFT_MAD_MX | TFT_MAD_MY));
			break;
	}

	utils::delay_microsecons(10);

	GPIO.out_w1ts = 1u << tft::pins::CS;

	ESP_LOGI(TAG, "Rotation set");
}




consteval uint16_t rgb(const uint8_t red, const uint8_t green, const uint8_t blue) {
	return uint16_t{}
		| (red   & 0x1F) << 11
		| (green & 0x3F) << 5
		| (blue  & 0x1F)
	;
}

void fill_rect(
	uint16_t x, uint16_t y,
	uint16_t w, uint16_t h,
	uint16_t color
) {
	GPIO.out_w1tc = 1u << tft::pins::CS;

	viewport(x, y, x + w - 1, y + h - 1);

	for (uint32_t i{ 2 }; i < PIXELS_COUNT; ++i) {
		send_bits16(color);
	}

	GPIO.out_w1ts = 1u << tft::pins::CS;
}

[[gnu::always_inline]]
inline void clear_screen(const uint16_t color = 0x0) {
	fill_rect(0, 0, WIDTH, HEIGHT, color);
}


} // namespace display

#define TFT_BLACK       0x0000      /*   0,   0,   0 */
#define TFT_NAVY        0x000F      /*   0,   0, 128 */
#define TFT_DARKGREEN   0x03E0      /*   0, 128,   0 */
#define TFT_DARKCYAN    0x03EF      /*   0, 128, 128 */
#define TFT_MAROON      0x7800      /* 128,   0,   0 */
#define TFT_PURPLE      0x780F      /* 128,   0, 128 */
#define TFT_OLIVE       0x7BE0      /* 128, 128,   0 */
#define TFT_LIGHTGREY   0xD69A      /* 211, 211, 211 */
#define TFT_DARKGREY    0x7BEF      /* 128, 128, 128 */
#define TFT_BLUE        0x001F      /*   0,   0, 255 */
#define TFT_GREEN       0x07E0      /*   0, 255,   0 */
#define TFT_CYAN        0x07FF      /*   0, 255, 255 */
#define TFT_RED         0xF800      /* 255,   0,   0 */
#define TFT_MAGENTA     0xF81F      /* 255,   0, 255 */
#define TFT_YELLOW      0xFFE0      /* 255, 255,   0 */
#define TFT_WHITE       0xFFFF      /* 255, 255, 255 */
#define TFT_ORANGE      0xFDA0      /* 255, 180,   0 */
#define TFT_GREENYELLOW 0xB7E0      /* 180, 255,   0 */
#define TFT_PINK        0xFE19      /* 255, 192, 203 */ //Lighter pink, was 0xFC9F
#define TFT_BROWN       0x9A60      /* 150,  75,   0 */
#define TFT_GOLD        0xFEA0      /* 255, 215,   0 */
#define TFT_SILVER      0xC618      /* 192, 192, 192 */
#define TFT_SKYBLUE     0x867D      /* 135, 206, 235 */
#define TFT_VIOLET      0x915C      /* 180,  46, 226 */

void main_loop(void *parameter);

extern "C" void app_main(void) {
	/*
	constexpr const char *const task_name{ "main_loop" };
	constexpr uint32_t stack_size{ 8192 };
	constexpr UBaseType_t priority{ 1 };
	constexpr UBaseType_t core_id{ 1 };

	static TaskHandle_t main_loop_handle{};
	xTaskCreatePinnedToCore(
		main_loop,
		task_name,
		stack_size,
		nullptr, // input
		priority,
		&main_loop_handle,
		core_id
	);
	*/
	main_loop(nullptr);
}

void main_loop(void *parameter) {
	using namespace utils::literals; // for _ms

	ESP_ERROR_CHECK(display::initialize_gpio());
	display::setup();
	display::set_rotation(display::rotation::portrait);

	vTaskDelay(150_ms);

	constexpr uint64_t attempts{ 60 };
	ESP_LOGI(TAG, "[BENCHMARK] Starting %llu attempts", attempts);

	const auto begin{ static_cast<uint64_t>(esp_timer_get_time()) };
	for (uint64_t i{}; i < attempts; ++i) {
		display::clear_screen(TFT_RED);
	}
	const uint64_t time_left{ static_cast<uint64_t>(esp_timer_get_time()) - begin };

	ESP_LOGI(TAG, "[BENCHMARK] %llu clear_screen took %llu microseconds (%llu ms)",
		attempts,
		time_left,
		time_left / 1000u
	);

	ESP_LOGI(TAG, "[BENCHMARK] %lld microseconds (%llu ms) per call",
		time_left / attempts,
		time_left / (attempts * 1000u)
	);

	vTaskDelay(1000_ms);
}

