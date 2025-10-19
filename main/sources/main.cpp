#include <utility>

#include <sdkconfig.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <driver/gpio.h>
#include <driver/dedic_gpio.h>
#include <driver/spi_master.h>

#include <esp_log.h>
#include <esp_random.h>
#include <esp_task_wdt.h>
#include <soc/gpio_struct.h>

#include "gzn/utils.hpp"
#include "gzn/tft/display.hpp"
#include "gzn/input/manager.hpp"
#include "gzn/audio/manager.hpp"
#include "gzn/graphics/render.hpp"
#include "gzn/filesystem/manager.hpp"

static constexpr auto TAG{ "test-proj" };

static void render_loop(void *);
static void update_loop();

static auto initialization() -> bool {
	using namespace gzn;
	using namespace utils::literals;

	ESP_LOGI(TAG, "[PRE  INIT] Available heap size: %zu",
		heap_caps_get_free_size(MALLOC_CAP_8BIT)
	);

	if (!fs::manager::initialize({})) {
		ESP_LOGE(TAG, "Failed to initialize file system");
		return false;
	}
	ESP_LOGI(TAG, "fs::manager initialized");

	const auto mount_result{ fs::manager::mount({
		.base_path                    { "/assets" },
		.partition_label              { "assets" },
		.max_simultanious_opened_files{ 16 },
		.format_if_mount_failed       { false }
	}) };
	if (mount_result != fs::mount_error::ok) {
		ESP_LOGE(TAG, "Failed to mount assets: %u", std::to_underlying(mount_result));
		return false;
	}

	const auto render_task_status{ xTaskCreatePinnedToCore(
		render_loop, "RND",
		graphics::defaults::render_thread_stack_size,
		xTaskGetCurrentTaskHandle(),
		graphics::defaults::render_thread_priority,
		nullptr,
		graphics::defaults::render_thread_core_id
	) };
	if (pdPASS != render_task_status) {
		ESP_LOGE(TAG, "Failed to create render task");
		return false;
	}
	// graphics::render::assign_task(render_task);

	if (audio::startup_result::ok != audio::manager::initialize()) {
		ESP_LOGE(TAG, "Failed to initialize audio");
		return false;
	}
	ESP_LOGI(TAG, "audio::manager initialized");
	if (input::init_error::ok != input::manager::initialize(std::to_underlying(input::backend_type::usb))) {
		ESP_LOGE(TAG, "Failed to initialize input manager");
		return false;
	}
	ESP_LOGI(TAG, "input::manager initialized");
	ulTaskNotifyTake(false, 1000); // wait for the RND

	ESP_LOGI(TAG, "[POST INIT] Available heap size: %zu",
		heap_caps_get_free_size(MALLOC_CAP_8BIT)
	);

	vTaskDelay(100_ms);

	return true;
}

void destruction() {
	gzn::input::manager::destroy();
	gzn::audio::manager::destroy();
	/// Stop render task here somehow?
	gzn::graphics::render::destroy();
	gzn::fs::manager::destroy();
}

extern "C" void app_main(void) {
	if (!initialization()) {
		return;
	}
	using namespace gzn::utils::literals;

	update_loop();
	destruction();
}

void render_loop(void *user) {
	using namespace gzn;
	using namespace graphics;

	/// Display initializes Dedicated GPIO, so it should be created at the same
	/// core as gzn::graphics::defaults::render_thread_core_id. That's tricky
	/// moment and for sure should be handeled better
	tft::display display{};
	display.configure(tft::make_initialization_sequence());
	display.set_orientation(tft::display::orientation::inverted_landscape);

	if (render::initialize(display) != init_status::success) {
		ESP_LOGE(TAG, "Failed to initialize render");
		return;
	}

	xTaskNotifyGive(static_cast<TaskHandle_t>(user));

	while (render::is_ready()) {
		render::update();
	}
}

/** @note WAIT! Don't look yet! Let me explain...
 * I'm testing-testing-testing. And the best way to test shit is in the first place.
 * almost EVERYTHING here will be deleted to the void. I won't make a whole game
 * like that or even similar to it. The game will be designed with DOD or something.
 * I hope u get it, dear reader.
 */
void update_loop() {
	using namespace gzn;
	using namespace utils::literals; // for _ms

	// audio::manager::play({
	// 	.filename = "/assets/sounds/sound-track.wav",
	// 	.loop = true
	// });

	constexpr float bullet_speed{ 100 };
	constexpr gzn::vec2u16 bullet_size{ .w = 4, .h = 4 };
	static auto bullet_color{ core::colors::red };


	static constexpr gzn::vec2 no_pos{
		.x = std::numeric_limits<float>::max(),
		.y = std::numeric_limits<float>::max()
	};

	struct bullet {
		gzn::vec2 pos{ no_pos };
		gzn::vec2 dir{ .x = 1.0f };
	};

	static std::array<bullet, 32> bullets{};
	static size_t last_added_bullet_index{};

	struct player_data {
		gzn::vec2 pos{ .x = 10, .y = 10 };
		gzn::vec2u16 size{ .w = 10, .h = 10 };
		gzn::vec2 dir{};
		core::color color{ core::colors::white };
		float speed{ 50 };
		audio::track_info fire_sound{ .filename = "/assets/sounds/attack.wav" };

		void update(const float delta) {
			constexpr float normalization{ 2.0f / 256 };
			constexpr int32_t pos_threshold{ 10 };
			constexpr int32_t neg_threshold{ -10 };
			using namespace input;

			const auto screen{ graphics::render::resolution() };

			if (const auto horizontal{ manager::get_horizontal_action(action_type::horizontal_move) };
				horizontal > pos_threshold || horizontal < neg_threshold
			) [[unlikely]] {
				const auto norm_hori{ (static_cast<float>(horizontal) * normalization) };
				pos.x += norm_hori * speed * delta;

				if (pos.x + size.w >= screen.w) {
					pos.x = screen.w - size.w;
				} else if (pos.x <= 0) {
					pos.x = 0;
				}
				dir.x = norm_hori;
			}

			if (const auto vertical{ manager::get_horizontal_action(action_type::vertical_move) };
				vertical > pos_threshold || vertical < neg_threshold
			) [[unlikely]] {
				const auto norm_hori{ (static_cast<float>(vertical) * normalization) };
				pos.y += norm_hori * speed * delta;

				if (pos.y + size.h >= screen.h) {
					pos.y = screen.h - size.h;
				} else if (pos.y <= 0) {
					pos.y = 0;
				}
				dir.y = norm_hori;
			}

			if (manager::is_action_just_pressed(action_type::attack)) [[unlikely]] {
				bullet_color = core::colors::red;
				fire();
			} else if (manager::is_action_pressed(action_type::use)) [[unlikely]] {
				bullet_color = core::colors::green;
				fire();
			}
		}

		void fire() {
			audio::manager::play(fire_sound);

			size_t index{ last_added_bullet_index + 1 };
			while (bullets[index].pos != no_pos && index != last_added_bullet_index) {
				index = index + 1 < std::size(bullets) ? index + 1 : 0;
			}
			last_added_bullet_index = index;
			auto &bullet{ bullets[last_added_bullet_index] };
			bullet.dir = vec2::normalized(dir);
			const vec2 half_size{
				.w = static_cast<float>(size.w >> 1),
				.h = static_cast<float>(size.h >> 1)
			};
			bullet.pos = pos + half_size + bullet.dir;
		}

		void draw() {
			graphics::render::draw_rectangle(vec2u16::from(pos), size, color);
		}
	};

	player_data player{};

	uint64_t last_time{ utils::get_time_ms() };
	uint32_t delta_time_ms{};
	float delta_time{};

	const auto screen{ graphics::render::resolution() };
	const uint16_t horizon_y{ static_cast<uint16_t>((screen.h / 3u) << 1) };

	while (true) {
		volatile const auto begin{ static_cast<uint64_t>(esp_timer_get_time()) };

		const auto current_time{ utils::get_time_ms() };
		delta_time_ms = current_time - std::exchange(last_time, current_time);
		delta_time = static_cast<float>(delta_time_ms) * 0.001f;

		// ESP_LOGI("update_loop", "Delta time: %u ms", delta_time_ms);

		// UPDATING
		audio::manager::update();
		player.update(delta_time);

		for (auto &bullet : bullets) {
			if (bullet.pos == no_pos) { continue; }

			bullet.pos += bullet.dir * bullet_speed * delta_time;
			const bool edge_reached{
				(bullet.pos.x <= 0) || (bullet.pos.y <= 0) ||
				(bullet.pos.x + bullet_size.w >= screen.w) ||
				(bullet.pos.y + bullet_size.h >= screen.h)
			};
			if (edge_reached) {
				bullet.pos = no_pos;
			}
		}

		// DRAWING
		graphics::render::draw_vertical_gradient(
			{ .x = 0u, .y = 0 }, { .w = screen.w, .h = horizon_y },
			{ core::colors::skyblue, core::colors::blue }
		);
		graphics::render::draw_vertical_gradient(
			{ .x = 0u, .y = horizon_y }, { .w = screen.w, .h = screen.h },
			{ core::colors::blue, core::colors::black }
		);

		player.draw();

		for (const auto &bullet : bullets) {
			if (bullet.pos == no_pos) { continue; }

			graphics::render::draw_rectangle(
				vec2u16::from(bullet.pos),
				bullet_size,
				bullet_color
			);
		}

#if defined(GZN_ENABLE_FPS)
		graphics::render::draw_rectangle(
			vec2u16{ .x = 3, .y = 3 },
			vec2u16{ .w = 13, .h = 7 },
			core::colors::black
		);
		graphics::render::draw_fps(
			{ .x = 4, .y = 4 },
			1000u / std::max<uint32_t>(delta_time_ms, 1u)
		);
#endif // defined(GZN_ENABLE_FPS)

		std::printf("Frame took %llu microseconds\r",
			static_cast<uint64_t>(esp_timer_get_time()) - begin
		);

		graphics::render::submit();
	}

	vTaskDelay(1000_ms);
}

