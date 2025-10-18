#include <span>
#include <array>
#include <cstdio>
#include <utility>
#include <cstring>
#include <cassert>
#include <algorithm>

#include <esp_log.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <driver/gpio.h>
#include <driver/spi_common.h>
#include <driver/spi_master.h>
#include <soc/gpio_struct.h>

#include "utils.hpp"
#include "tft/pins.hpp"
#include "tft/ili9486/commands.hpp"


constexpr const char *TAG{ "spi-test" };

constexpr uint32_t LOW { 0u };
constexpr uint32_t HIGH{ 1u };

constexpr uint16_t WIDTH { 320u };
constexpr uint16_t HEIGHT{ 480u };
constexpr uint32_t PIXELS_COUNT{ WIDTH * HEIGHT };

constexpr const uint16_t PARALLEL_LINES{ 16 };

enum class send_policy : uint32_t {
	command = LOW,
	data    = HIGH
};

spi_device_handle_t display_device{};

[[gnu::always_inline]]
static inline void send_data(
	const std::span<const uint8_t> data,
	const send_policy policy
) {
	spi_transaction_t description{
		// .flags     = SPI_TRANS_MODE_OCT,
		.length    = std::size(data) * size_t{ 8u },
		.user      = reinterpret_cast<void*>(policy),
		.tx_buffer = std::empty(data) ? nullptr : std::data(data),
	};

	ESP_ERROR_CHECK(spi_device_polling_transmit(display_device, &description));
}

[[gnu::always_inline]]
static inline void send_data_wait(
	const std::span<const uint8_t> data,
	const send_policy policy
) {
	spi_transaction_t description{
		.flags     = SPI_TRANS_MODE_OCT,
		.length    = std::size(data) * size_t{ 8u },
		.user      = reinterpret_cast<void*>(policy),
		.tx_buffer = std::empty(data) ? nullptr : std::data(data),
	};

	ESP_ERROR_CHECK(spi_device_transmit(display_device, &description));
}


[[gnu::always_inline]]
static inline void send_bits8_wait(const uint8_t data, const send_policy policy) {
	return send_data_wait(std::span{ &data, sizeof(data) }, policy);
}

[[gnu::always_inline]]
static inline void send_bits16_wait(const uint16_t data, const send_policy policy) {
	return send_data_wait(std::span{ reinterpret_cast<const uint8_t *>(&data), sizeof(data) }, policy);
}

[[gnu::always_inline]]
static inline void send_bits32_wait(const uint32_t data, const send_policy policy) {
	return send_data_wait(std::span{ reinterpret_cast<const uint8_t *>(&data), sizeof(data) }, policy);
}

[[gnu::always_inline]]
static inline void write_data(const uint8_t data) {
	GPIO.out_w1tc = 1 << tft::pins::CS;

	send_bits8_wait(data, send_policy::data);

	GPIO.out_w1ts = 1 << tft::pins::CS;
}

[[gnu::always_inline]]
static inline void execute_command(const tft::ili9486::command_id command) {
	GPIO.out_w1tc = 1 << tft::pins::CS; // Maybe the device does this for us

	send_bits8_wait(std::to_underlying(command), send_policy::command);

	GPIO.out_w1ts = 1 << tft::pins::CS;
}




[[gnu::always_inline]]
static inline void execute_command(const tft::ili9486::command &cmd) {
	execute_command(cmd.id);
	for (uint8_t i{}; i < cmd.params_count; ++i) {
		write_data(cmd.params[i]);
	}

	// execute_command(cmd.id);
	// send_data(tft::ili9486::as_span(cmd), send_policy::data);
}

void execute_commands(const std::span<const tft::ili9486::command> commands) {
	// std::ranges::for_each(commands, execute_command);
	for (auto cur{ std::begin(commands) }; cur != std::end(commands); ++cur) {
		execute_command(*cur);
	}
}

void viewport(
	const uint16_t x0 = 0u,
	const uint16_t y0 = 0u,
	const uint16_t x1 = WIDTH - 1u,
	const uint16_t y1 = HEIGHT - 1u
) {
	using namespace tft::ili9486;

	static std::array params{
		spi_transaction_t{ // 0
			.flags   = SPI_TRANS_USE_TXDATA,
			.length  = 8,
			.user    = reinterpret_cast<void*>(send_policy::command),
			.tx_data = { std::to_underlying(command_id::SET_COLUMN_ADDRESS) }
		},
		spi_transaction_t{ // 1
			.flags   = SPI_TRANS_USE_TXDATA,
			.length  = 8 * 4,
			.user    = reinterpret_cast<void*>(send_policy::command)
		},
		spi_transaction_t{ // 2
			.flags   = SPI_TRANS_USE_TXDATA,
			.length  = 8,
			.user    = reinterpret_cast<void*>(send_policy::command),
			.tx_data = { std::to_underlying(command_id::SET_PAGE_ADDRESS) }
		},
		spi_transaction_t{ // 3
			.flags  = SPI_TRANS_USE_TXDATA, // page address data
			.length = 8 * 4,
			.user   = reinterpret_cast<void*>(send_policy::data)
		},
		spi_transaction_t{ // 4
			.flags   = SPI_TRANS_USE_TXDATA,
			.length  = 8,
			.user    = reinterpret_cast<void*>(send_policy::command),
			.tx_data = { std::to_underlying(command_id::WRITE_MEMORY_START) }
		}
	};

	std::memcpy(params[1].tx_data + 0, &x0, sizeof(x0));
	std::memcpy(params[1].tx_data + 2, &x1, sizeof(x1));

	std::memcpy(params[3].tx_data + 0, &y0, sizeof(y0));
	std::memcpy(params[3].tx_data + 2, &y1, sizeof(y1));

	for (auto &param : params) {
		ESP_ERROR_CHECK(spi_device_queue_trans(display_device, &param, portMAX_DELAY));
	}
}

static void initialize_gpio() {
	const gpio_config_t io_conf{
		.pin_bit_mask = tft::pins::PINS_MASK,
		.mode         = GPIO_MODE_OUTPUT,
		.pull_up_en   = GPIO_PULLUP_ENABLE,
		.pull_down_en = GPIO_PULLDOWN_DISABLE,
		.intr_type    = GPIO_INTR_DISABLE
	};
	ESP_ERROR_CHECK(gpio_config(&io_conf));

	GPIO.out_w1ts = tft::pins::PINS_MASK;

	ESP_LOGI(TAG, "GPIO Configured");
}


static inline void IRAM_ATTR lcd_spi_pre_transfer_callback(spi_transaction_t *t) {
	// user contains HIGH or LOW. By setting user data we could define
	// the command/data selection
	if (send_policy{ reinterpret_cast<uint32_t>(t->user) } == send_policy::command) {
		GPIO.out_w1tc = 1 << tft::pins::CD;
	} else {
		GPIO.out_w1ts = 1 << tft::pins::CD;
	}
}

static inline void IRAM_ATTR lcd_spi_post_transfer_callback(spi_transaction_t *t) {
	// user contains HIGH or LOW. By setting user data we could define
	// the command/data selection

	if (send_policy{ reinterpret_cast<uint32_t>(t->user) } == send_policy::command) {
		GPIO.out_w1ts = 1 << tft::pins::CD;
	}

}


static void initialize_spi() {
	const spi_bus_config_t spi_bus_config{
		.data0_io_num = tft::pins::D0,
		.data1_io_num = tft::pins::D1,
		.sclk_io_num  = -1,// tft::pins::WRITE, // SCLK. Was 15, // Serial Clock IDK How the fuck to use it
		.data2_io_num = tft::pins::D2,
		.data3_io_num = tft::pins::D3,
		.data4_io_num = tft::pins::D4,
		.data5_io_num = tft::pins::D5,
		.data6_io_num = tft::pins::D6,
		.data7_io_num = tft::pins::D7,

		.max_transfer_sz = static_cast<int>(PIXELS_COUNT),
		.flags           = SPICOMMON_BUSFLAG_OCTAL,
	};
	ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &spi_bus_config, SPI_DMA_CH_AUTO));

	ESP_LOGI(TAG, "SPI Bus initialized");

	// Define the SPI clock frequency, this affects the graphics rendering speed. Too
	// fast and the TFT driver will not keep up and display corruption appears.
	// With an ILI9341 display 40MHz works OK, 80MHz sometimes fails
	// With a ST7735 display more than 27MHz may not work (spurious pixels and lines)
	// With an ILI9163 display 27 MHz works OK.

	// #define SPI_FREQUENCY   1_MHz
	// #define SPI_FREQUENCY   5_MHz
	// #define SPI_FREQUENCY  10_MHz
	// #define SPI_FREQUENCY  20_MHz
	// #define SPI_FREQUENCY  27_MHz // << Default value. Should work well
	// #define SPI_FREQUENCY  40_MHz
	// #define SPI_FREQUENCY  55_MHz // STM32 SPI1 only (SPI2 maximum is 27MHz)
	// #define SPI_FREQUENCY  80_MHz

	using namespace utils::literals; // _MHz

	const spi_device_interface_config_t spi_device_confg{
		.mode           = 0b00,
		.duty_cycle_pos = 128,
		.clock_speed_hz = 27_MHz,
		.spics_io_num   = tft::pins::CS,
		// .flags          = SPI_DEVICE_POSITIVE_CS,
		.queue_size     = PARALLEL_LINES >> 1,
		.pre_cb         = lcd_spi_pre_transfer_callback,
		.post_cb        = lcd_spi_post_transfer_callback
	};
	ESP_ERROR_CHECK(spi_bus_add_device(SPI2_HOST, &spi_device_confg, &display_device));
	ESP_LOGI(TAG, "SPI Device added");
};

static void initialize_display() {
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





static void send_line_at(
	uint16_t row_id,
	const std::span<const uint8_t> data
) {
	using namespace tft::ili9486;

	spi_transaction_t param{ // 5
		.flags  = 0, //undo SPI_TRANS_USE_TXDATA flag
		.length = std::size(data) * 8, // WIDTH * 2 * 8 * PARALLEL_LINES,  //Data length, in bits
		.user   = reinterpret_cast<void*>(send_policy::data),
		.tx_buffer = std::data(data)
	};

	viewport(0u, row_id, WIDTH, row_id + PARALLEL_LINES - 1);
	ESP_ERROR_CHECK(spi_device_queue_trans(display_device, &param, portMAX_DELAY));
}

static void send_line_finish() {
	spi_transaction_t *dummy{};
	// Wait for all 6 transactions to be done and get back the results.
	for (uint8_t i{}; i < 6u; ++i) {
		ESP_ERROR_CHECK(spi_device_get_trans_result(display_device, &dummy, portMAX_DELAY));
		// We could inspect dummy now if we received any info back
	}
}


constexpr uint16_t TFT_BLUE{ 0x001F };     /*   0,   0, 255 */
constexpr uint16_t TFT_RED { 0xF800 };     /* 255,   0,   0 */

static void dummy_display_colors() {
	constexpr size_t buffer_size{ WIDTH * PARALLEL_LINES };

	std::array<uint16_t *, 2> lines{};
	//Allocate memory for the pixel buffers
	for (uint8_t i{}; i < std::size(lines); ++i) {
		lines[i] = reinterpret_cast<uint16_t *>(spi_bus_dma_memory_alloc(
			SPI2_HOST,
			buffer_size * sizeof(uint16_t),
			0
		));
		assert(lines[i] != nullptr);
	}

	size_t frame{};
	//Indexes of the line currently being sent to the LCD and the line we're calculating.
	int sending_line{ -1 };
	int calc_line{};

	while (true) {
		++frame;

		const auto begin{ xTaskGetTickCount() };

		for (uint16_t y{}; y < HEIGHT; y += PARALLEL_LINES) {
			// ESP_LOGI(TAG, "Rendering [%u:%u]", y, y + PARALLEL_LINES);
			//Calculate a line.
			std::fill_n(lines[calc_line], buffer_size, calc_line ? TFT_RED : TFT_BLUE);

			//Finish up the sending process of the previous line, if any
			if (sending_line != -1) {
				send_line_finish();
			}
			//Swap sending_line and calc_line
			sending_line = calc_line;
			calc_line = (calc_line == 1) ? 0 : 1;
			//Send the line we currently calculated.
			send_line_at(y, std::span<const uint8_t>{
				reinterpret_cast<const uint8_t *>(lines[sending_line]),
				buffer_size * 2ull
			});
			//The line set is queued up for sending now; the actual sending happens in the
			//background. We can go on to calculate the next line set as long as we do not
			//touch line[sending_line]; the SPI sending process is still reading from that.
		}


		const auto time_left{ xTaskGetTickCount() - begin };
		ESP_LOGI(TAG, "[%.10zu] Frame took %u ticks (or %u ms)",
			frame++, time_left, pdTICKS_TO_MS(time_left)
		);
	}
}




extern "C" void app_main(void) {
	using namespace utils::literals;

	initialize_gpio();
	initialize_spi();
	initialize_display();
	set_rotation(rotation::portrait);

	dummy_display_colors();
}

