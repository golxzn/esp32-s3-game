#include <cstdio>
#include <memory>
#include <vector>
#include <ranges>
#include <algorithm>
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
#include "gzn/graphics/render.hpp"

static constexpr const char *TAG{ "test-proj" };

void update_loop(void *);

extern "C" void app_main(void) {
	using namespace gzn;

	/// Display initializes Dedicated GPIO, so it should be created at the same
	/// core as gzn::graphics::defaults::render_thread_core_id. That's tricky
	/// moment and for sure should be handeled better
	tft::display view{};
	view.set_orientation(tft::display::orientation::landscape);

	ESP_LOGI(TAG, "[PRE  INIT] Available heap size: %zu",
		heap_caps_get_free_size(MALLOC_CAP_8BIT)
	);
	if (graphics::render::initialize(view) != graphics::init_status::success) {
		ESP_LOGE(TAG, "Failed to initialize render");
		return;
	}
	ESP_LOGI(TAG, "[POST INIT] Available heap size: %zu",
		heap_caps_get_free_size(MALLOC_CAP_8BIT)
	);

	constexpr uint32_t stack_size{ 8192 };
	constexpr UBaseType_t priority{ 1 };
	TaskHandle_t update_handle{};
	xTaskCreatePinnedToCore(
		update_loop,
		"update_loop",
		stack_size,
		nullptr,
		priority,
		&update_handle,
		1 // core id
	);

	using namespace utils::literals;

	while (true) {
		vTaskDelay(5_s);
	};

	graphics::render::destroy();
}

struct moving_rect {
	gzn::vec2 pos{ .x = 30, .y = 5 };
	gzn::vec2 size{ .w = 10, .y = 10 };
	gzn::vec2 velocity{ .x = -30, .y = 30 };
	gzn::color color{ gzn::core::colors::red };

	void update(const float delta) {
		const auto screen{ gzn::graphics::render::resolution() };

		pos += velocity * delta;
		if (pos.x + size.w >= screen.w) {
			velocity.x = -velocity.x;
		} else if (pos.x <= 0) {
			velocity.x = -velocity.x;
		}

		if (pos.y + size.y >= screen.h) {
			velocity.y = -velocity.y;
		} else if (pos.y <= 0) {
			velocity.y = -velocity.y;
		}
	}

	void draw() const {
		gzn::graphics::render::draw_rectangle(
			gzn::vec2u16::from(pos),
			gzn::vec2u16::from(size),
			color
		);
	}
};

struct rect_system_stupid {
	std::vector<std::unique_ptr<moving_rect>> rects{};

	explicit rect_system_stupid(const size_t rect_count) : rects{ rect_count } {
		std::ranges::generate(rects, std::make_unique<moving_rect>);
	}

	void update(const float delta) {
		for (auto &rect : rects) {
			rect->update(delta);
		}
	}

	void draw() const {
		for (const auto &rect : rects) {
			rect->draw();
		}
	}
};

struct rect_system_primitive {
	std::vector<moving_rect> rects{};

	explicit rect_system_primitive(const size_t rect_count) : rects{ rect_count } {}

	void update(const float delta) {
		for (auto &rect : rects) {
			rect.update(delta);
		}
	}

	void draw() const {
		for (const auto &rect : rects) {
			rect.draw();
		}
	}
};

struct rect_system_seq {
	struct [[gnu::packed]] rect_data {
		gzn::vec2 position{};
		gzn::vec2 size{};
		gzn::vec2 velocity{};
		gzn::color color{};
	};

	std::unique_ptr<uint8_t[]> buffer{};
	std::span<rect_data> rects{};

	explicit rect_system_seq (const size_t rect_count)
		: buffer{ std::make_unique<uint8_t[]>(sizeof(rect_data) * rect_count) }
		, rects{ reinterpret_cast<rect_data*>(buffer.get()), rect_count }
	{ }

	void randomize(
		const gzn::vec2u16 min_size, const gzn::vec2u16 max_size,
		const gzn::vec2 min_velocity, const gzn::vec2 max_velocity
	) {
		const auto screen{ gzn::graphics::render::resolution() };
		for (auto &rect : rects) {
			rect.position = gzn::vec2{
				.x = static_cast<float>(static_cast<uint16_t>(std::rand()) % screen.w),
				.y = static_cast<float>(static_cast<uint16_t>(std::rand()) % screen.h)
			};
		}

		for (auto &rect : rects) {
			rect.size = gzn::vec2{
				.x = static_cast<float>(std::max<uint16_t>(min_size.w, static_cast<uint16_t>(std::rand()) % max_size.w)),
				.y = static_cast<float>(std::max<uint16_t>(min_size.h, static_cast<uint16_t>(std::rand()) % max_size.h))
			};
		}

		const auto offset{ (max_velocity - min_velocity) / 2.f };
		for (auto &rect : rects) {
			rect.velocity = gzn::vec2{
				.x = std::max<float>(min_velocity.w, static_cast<float>(std::rand() % static_cast<uint16_t>(max_velocity.x)) - offset.x),
				.y = std::max<float>(min_velocity.h, static_cast<float>(std::rand() % static_cast<uint16_t>(max_velocity.y)) - offset.y)
			};
		}

		for (auto &rect : rects) {
			rect.color = static_cast<gzn::color>(std::rand() & 0b1111111111111111);
		}
	}

	void update(const float delta) {
		const auto screen{ gzn::graphics::render::resolution() };

		for (auto &rect : rects) {
			rect.position += rect.velocity * delta;

			if (rect.position.x + rect.size.w >= screen.w) {
				rect.velocity.x = -rect.velocity.x;
			} else if (rect.position.x <= 0) {
				rect.velocity.x = -rect.velocity.x;
			}

			if (rect.position.y + rect.size.y >= screen.h) {
				rect.velocity.y = -rect.velocity.y;
			} else if (rect.position.y <= 0) {
				rect.velocity.y = -rect.velocity.y;
			}
		}
	}

	void draw() const {
		using namespace gzn;
		for (const auto &rect : rects) {
			graphics::render::draw_rectangle(
				vec2u16::from(rect.position),
				vec2u16::from(rect.size),
				rect.color
			);
		}
	}
};


struct rect_system {
	size_t count{};
	std::unique_ptr<uint8_t[]> buffer{};
	gzn::vec2 *positions{ nullptr };
	gzn::vec2 *sizes{ nullptr };
	gzn::vec2 *velocities{ nullptr };
	gzn::color *colors{ nullptr };

	static constexpr auto rect_bytes_size{ sizeof(gzn::vec2) * 3 + sizeof(gzn::color) };

	explicit rect_system(const size_t rect_count)
		: count{ std::max(size_t{ 1 }, rect_count) }
		, buffer{ std::make_unique<uint8_t[]>(rect_bytes_size * count) }
		, positions { buffer ? reinterpret_cast<gzn::vec2  *>(buffer.get()) : nullptr }
		, sizes     { buffer ? reinterpret_cast<gzn::vec2  *>(positions ) + count : nullptr }
		, velocities{ buffer ? reinterpret_cast<gzn::vec2  *>(sizes     ) + count : nullptr }
		, colors    { buffer ? reinterpret_cast<gzn::color *>(velocities) + count : nullptr }
	{ }

	void randomize(
		const gzn::vec2u16 min_size, const gzn::vec2u16 max_size,
		const gzn::vec2 min_velocity, const gzn::vec2 max_velocity
	) {
		const auto screen{ gzn::graphics::render::resolution() };
		for (size_t i{}; i < count; ++i) {
			positions[i] = gzn::vec2{
				.x = static_cast<float>(static_cast<uint16_t>(std::rand()) % screen.w),
				.y = static_cast<float>(static_cast<uint16_t>(std::rand()) % screen.h)
			};
		}

		for (size_t i{}; i < count; ++i) {
			sizes[i] = gzn::vec2{
				.x = static_cast<float>(std::max<uint16_t>(min_size.w, static_cast<uint16_t>(std::rand()) % max_size.w)),
				.y = static_cast<float>(std::max<uint16_t>(min_size.h, static_cast<uint16_t>(std::rand()) % max_size.h))
			};
		}

		const auto offset{ (max_velocity - min_velocity) / 2.f };
		for (size_t i{}; i < count; ++i) {
			velocities[i] = gzn::vec2{
				.x = std::max<float>(min_velocity.w, static_cast<float>(std::rand() % static_cast<uint16_t>(max_velocity.x)) - offset.x),
				.y = std::max<float>(min_velocity.h, static_cast<float>(std::rand() % static_cast<uint16_t>(max_velocity.y)) - offset.y)
			};
		}

		for (size_t i{}; i < count; ++i) {
			colors[i] = static_cast<gzn::color>(std::rand() & 0b1111111111111111);
		}
	}

	void update(const float delta) {

		// for (size_t i{}; i < count; ++i) {
		// 	positions[i] += velocities[i] * delta;
		// }

		const auto screen{ gzn::graphics::render::resolution() };
		for (size_t i{}; i < count; ++i) {
			auto &pos{ positions[i] };
			const auto size{ sizes[i] };
			auto &velocity{ velocities[i] };

			pos += velocity * delta;

			if (pos.x + size.w >= screen.w) {
				velocity.x = -velocity.x;
			} else if (pos.x <= 0) {
				velocity.x = -velocity.x;
			}

			if (pos.y + size.y >= screen.h) {
				velocity.y = -velocity.y;
			} else if (pos.y <= 0) {
				velocity.y = -velocity.y;
			}
		}
	}

	void draw() const {
		using namespace gzn;
		for (size_t i{}; i < count; ++i) {
			graphics::render::draw_rectangle(
				vec2u16::from(positions[i]),
				vec2u16::from(sizes[i]),
				colors[i]
			);
		}
	}
};


void randomize(
	std::span<moving_rect> rects,
	const gzn::vec2u16 min_size, const gzn::vec2u16 max_size,
	const gzn::vec2 min_velocity, const gzn::vec2 max_velocity
) {
	const auto count{ std::size(rects) };
	const auto screen{ gzn::graphics::render::resolution() };
	for (size_t i{}; i < count; ++i) {
		rects[i].pos = gzn::vec2{
			.x = static_cast<float>(static_cast<uint16_t>(std::rand()) % screen.w),
			.y = static_cast<float>(static_cast<uint16_t>(std::rand()) % screen.h)
		};
	}

	for (size_t i{}; i < count; ++i) {
		rects[i].size = gzn::vec2{
			.x = static_cast<float>(std::max<uint16_t>(min_size.w, static_cast<uint16_t>(std::rand()) % max_size.w)),
			.y = static_cast<float>(std::max<uint16_t>(min_size.h, static_cast<uint16_t>(std::rand()) % max_size.h))
		};
	}

	const auto offset{ (max_velocity - min_velocity) / 2.f };
	for (size_t i{}; i < count; ++i) {
		rects[i].velocity = gzn::vec2{
			.x = std::max<float>(min_velocity.w, static_cast<float>(std::rand() % static_cast<uint16_t>(max_velocity.x)) - offset.x),
			.y = std::max<float>(min_velocity.h, static_cast<float>(std::rand() % static_cast<uint16_t>(max_velocity.y)) - offset.y)
		};
	}

	for (size_t i{}; i < count; ++i) {
		rects[i].color = static_cast<gzn::color>(std::rand() & 0b1111111111111111);
	}
}

void randomize(
	std::vector<std::unique_ptr<moving_rect>> &rects,
	const gzn::vec2u16 min_size, const gzn::vec2u16 max_size,
	const gzn::vec2 min_velocity, const gzn::vec2 max_velocity
) {
	const auto count{ std::size(rects) };
	const auto screen{ gzn::graphics::render::resolution() };
	for (size_t i{}; i < count; ++i) {
		rects[i]->pos = gzn::vec2{
			.x = static_cast<float>(static_cast<uint16_t>(std::rand()) % screen.w),
			.y = static_cast<float>(static_cast<uint16_t>(std::rand()) % screen.h)
		};
	}

	for (size_t i{}; i < count; ++i) {
		rects[i]->size = gzn::vec2{
			.x = static_cast<float>(std::max<uint16_t>(min_size.w, static_cast<uint16_t>(std::rand()) % max_size.w)),
			.y = static_cast<float>(std::max<uint16_t>(min_size.h, static_cast<uint16_t>(std::rand()) % max_size.h))
		};
	}

	const auto offset{ (max_velocity - min_velocity) / 2.f };
	for (size_t i{}; i < count; ++i) {
		rects[i]->velocity = gzn::vec2{
			.x = std::max<float>(min_velocity.w, static_cast<float>(std::rand() % static_cast<uint16_t>(max_velocity.x)) - offset.x),
			.y = std::max<float>(min_velocity.h, static_cast<float>(std::rand() % static_cast<uint16_t>(max_velocity.y)) - offset.y)
		};
	}

	for (size_t i{}; i < count; ++i) {
		rects[i]->color = static_cast<gzn::color>(std::rand() & 0b1111111111111111);
	}
}

void update_loop(void *) {
	using namespace gzn;
	using namespace utils::literals; // for _ms

	// moving_rect rect{};
	const size_t rects_count{ heap_caps_get_free_size(MALLOC_CAP_8BIT) / rect_system::rect_bytes_size };
	ESP_LOGI(TAG, "[RECTS] Making %zu rects", rects_count);

	rect_system_stupid rects{ 8'000 };
	// rects.randomize(
	// 	vec2u16{ .w = 2, .h = 2 }, vec2u16{ .w = 5, .h = 5 },
	// 	vec2{ .x = -100, .y = -100 }, vec2{ .x = 100, .y = 100 }
	// );

	// const size_t count{ 9'500 };
	// auto array{ new moving_rect[count] };
	// std::span<moving_rect> rects{ array, count };
	randomize(rects.rects,
		vec2u16{ .w = 2, .h = 2 }, vec2u16{ .w = 5, .h = 5 },
		vec2{ .x = -100, .y = -100 }, vec2{ .x = 100, .y = 100 }
	);

	ESP_LOGI(TAG, "[RECTS] Now we have only %zu bytes heap available 💀", heap_caps_get_free_size(MALLOC_CAP_8BIT));


	uint64_t last_time{ utils::get_time_ms() };
	uint32_t delta_time_ms{};
	float delta_time{};

	while (true) {
		volatile const auto begin{ static_cast<uint64_t>(esp_timer_get_time()) };

		const auto current_time{ utils::get_time_ms() };
		delta_time_ms = current_time - std::exchange(last_time, current_time);
		delta_time = static_cast<float>(delta_time_ms) * 0.001f;

		// ESP_LOGI("update_loop", "Delta time: %u ms", delta_time_ms);

		// UPDATING
		// rect.update(delta_time);
		rects.update(delta_time);
		// for (auto &rect : rects) {
		// 	rect.update(delta_time);
		// }


		// DRAWING
		graphics::render::fill_screen_grid_pattern(
			{ core::colors::green, core::colors::black }
		);

		// rect.draw();
		rects.draw();
		// for (auto &rect : rects) {
		// 	rect.draw();
		// }


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

		ESP_LOGI(TAG, "Frame took %llu microseconds",
			static_cast<uint64_t>(esp_timer_get_time()) - begin
		);

		graphics::render::submit();
	}

	vTaskDelay(1000_ms);
}

