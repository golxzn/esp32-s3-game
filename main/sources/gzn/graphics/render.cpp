#include <span>
#include <limits>
#include <utility>
#include <functional>

#include <cstdio>
#include <esp_timer.h>
#include <esp_heap_caps.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "gzn/graphics/render.hpp"

#include "gzn/tft/display.hpp"
#include "gzn/utils.hpp"

namespace gzn::graphics {

namespace {

inline constexpr auto RNDR_TAG{ "gzn::graphics::render" };

} // namespace

struct render::context {
	std::reference_wrapper<tft::display> display;
	gzn::vec2u16 resolution{};

	color *buffers{ nullptr };
	size_t buffer_length{};

	SemaphoreHandle_t render_fence{};
	SemaphoreHandle_t update_fence{};

	TaskHandle_t rendering_task_handle{};

	uint8_t current_rendering_buffer{};
	uint8_t buffers_count{};
	uint8_t pixel_size{};

	inline auto is_running() const noexcept {
		return current_rendering_buffer != std::numeric_limits<uint8_t>::max();
	};

	inline void swap_buffers() noexcept {
		current_rendering_buffer = get_next_buffer_id();
	}

	inline auto get_next_buffer_id() noexcept -> uint8_t {
		return (current_rendering_buffer + 1) % buffers_count;
	}

	inline auto get_next_buffer() noexcept -> std::span<color> {
		return std::span<color>{
			std::next(buffers, buffer_length * static_cast<size_t>(get_next_buffer_id())),
			buffer_length
		};
	}

	inline auto get_current_buffer() noexcept -> std::span<color> {
		return std::span<color>{
			std::next(buffers, buffer_length * static_cast<size_t>(current_rendering_buffer)),
			buffer_length
		};
	}

};

// static void render_loop(void *);

auto render::initialize(
	tft::display &display,
	const uint8_t buffers_count,
	const uint8_t pixel_size
) -> init_status {
	if (ctx) [[unlikely]] {
		return init_status::already_initialized;
	}

	if (buffers_count < defaults::minimal_buffers_count
	||  buffers_count > defaults::maximum_buffers_count
	) {
		return init_status::invalid_arguments;
	}

	const auto resolution{ display.size() / static_cast<uint16_t>(pixel_size) };
	const auto buffer_length{
		static_cast<size_t>(resolution.w) * static_cast<size_t>(resolution.y)
	};
	const auto all_buffers_length{ buffer_length * static_cast<size_t>(buffers_count) };

	const auto required_memory{ all_buffers_length * sizeof(color) };
	const auto available_memory{ heap_caps_get_free_size(MALLOC_CAP_8BIT) };
	if (required_memory >= available_memory) {
		return init_status::not_enough_memory;
	}

	auto buffers{ static_cast<color *>(heap_caps_malloc(
		required_memory, MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL)
	) };
	if (!buffers) {
		return init_status::not_enough_memory;
	}

	ctx = new context{
		.display       = std::ref(display),
		.resolution    = resolution,
		.buffers       = buffers,
		.buffer_length = buffer_length,
		.render_fence  = xSemaphoreCreateBinary(),
		.update_fence  = xSemaphoreCreateBinary(),
		.buffers_count = buffers_count,
		.pixel_size    = pixel_size
	};
	if (!ctx) {
		heap_caps_free(buffers);
		return init_status::not_enough_memory;
	}
	if (!ctx->render_fence || !ctx->update_fence) {
		destroy();
		return init_status::failed_to_start_render_thread;
	}
/*
	const auto status{ xTaskCreatePinnedToCore(
		render_loop, "RND",
		defaults::render_thread_stack_size,
		ctx,
		defaults::render_thread_priority,
		&ctx->rendering_task_handle,
		defaults::render_thread_core_id
	) };
	if (status == pdPASS) {
*/
		xSemaphoreGive(ctx->update_fence);
		return init_status::success;
/*
	}

	destroy();
	return init_status::failed_to_start_render_thread;
*/
}

[[gnu::always_inline]]
inline auto render::is_ready() -> bool {
	return ctx != nullptr;
}

[[gnu::always_inline]]
inline void render::update() {
	xSemaphoreTake(ctx->render_fence, portMAX_DELAY);

	if (is_ready()) [[likely]] {
		display().send_screen_buffer(
			ctx->get_current_buffer(),
			ctx->pixel_size
		);

		/// @think maybe we should move it to submit or even make render::swap_buffers()
		ctx->swap_buffers();
	}

	xSemaphoreGive(ctx->update_fence);
}

void render::destroy() {
	if (ctx == nullptr) {
		return;
	}

	vSemaphoreDelete(ctx->render_fence);
	vSemaphoreDelete(ctx->update_fence);
	heap_caps_free(ctx->buffers);
	delete std::exchange(ctx, nullptr);
}

[[gnu::always_inline]]
inline void render::submit() {
	xSemaphoreTake(ctx->update_fence, portMAX_DELAY);
	xSemaphoreGive(ctx->render_fence);
}

[[gnu::always_inline]]
inline auto render::resolution() noexcept -> gzn::vec2u16 {
	return ctx->resolution;
}

[[gnu::always_inline]]
inline auto render::next_buffer_id() noexcept -> uint8_t {
	return ctx->get_next_buffer_id();
}

[[gnu::always_inline]]
inline auto render::current_buffer_id() noexcept -> uint8_t {
	return ctx->current_rendering_buffer;
}


[[gnu::always_inline]]
inline auto render::get_next_buffer() noexcept -> std::span<color> {
	return ctx->get_next_buffer();
}

[[gnu::always_inline]]
inline auto render::get_current_buffer() noexcept -> std::span<color> {
	return ctx->get_current_buffer();
}

[[gnu::always_inline]]
inline auto render::display() noexcept -> tft::display & {
	return ctx->display.get();
}


void render::draw_rectangle(const vec2u16 pos, const vec2u16 size, const color clr) {
	const auto screen_w{ static_cast<size_t>(ctx->resolution.w) };
	const auto screen_h{ static_cast<size_t>(ctx->resolution.h) };

	const size_t top_bound { std::min<size_t>(screen_h, pos.y + size.h) };
	const size_t left_bound{ std::min<size_t>(screen_w, pos.x + size.w) };

	if (top_bound == pos.y || left_bound == pos.x) {
		return;
	}

	auto buffer{ ctx->get_next_buffer() };
	for (size_t column{ static_cast<size_t>(pos.y) }; column < top_bound; ++column) {
		const size_t offset{ static_cast<size_t>(column) * screen_w };
		for (size_t row{ static_cast<size_t>(pos.x) }; row < left_bound; ++row) {
			buffer[offset + row] = clr;
		}
	}
}

void render::draw_grid_pattern(
	const vec2u16 pos,
	const vec2u16 size,
	const std::array<color, 2> colors
) {
	const auto screen_w{ static_cast<size_t>(ctx->resolution.w) };
	const auto screen_h{ static_cast<size_t>(ctx->resolution.h) };

	const size_t top_bound { std::min<size_t>(screen_h, pos.y + size.h) };
	const size_t left_bound{ std::min<size_t>(screen_w, pos.x + size.w) };

	if (top_bound == pos.y || left_bound == pos.x) {
		return;
	}

	auto buffer{ ctx->get_next_buffer() };
	for (size_t column{ static_cast<size_t>(pos.y) }; column < top_bound; ++column) {
		const size_t offset{ static_cast<size_t>(column) * screen_w };
		for (size_t row{ static_cast<size_t>(pos.x) }; row < left_bound; ++row) {
			buffer[offset + row] = colors[(row + column) & 1];
		}
	}
}

void render::draw_vertical_gradient(
	const vec2u16 pos,
	const vec2u16 size,
	const std::array<color, 2> colors
) {
	const auto screen_w{ static_cast<size_t>(ctx->resolution.w) };
	const auto screen_h{ static_cast<size_t>(ctx->resolution.h) };

	const size_t top_bound { std::min<size_t>(screen_h, pos.y + size.h) };
	const size_t left_bound{ std::min<size_t>(screen_w, pos.x + size.w) };

	if (top_bound == pos.y || left_bound == pos.x) {
		return;
	}


	auto red_0{ static_cast<float>(colors[0] >> 11) };
	auto green_0{ static_cast<float>((colors[0] >> 5) & 0x3F) };
	auto blue_0{ static_cast<float>(colors[0] & 0x1F) };

	const auto red_diff  { static_cast<float>(colors[1] >> 11)         - red_0 };
	const auto green_diff{ static_cast<float>((colors[1] >> 5) & 0x3F) - green_0 };
	const auto blue_diff { static_cast<float>(colors[1] & 0x1F)        - blue_0 };

	const float flipped_distance_steps{ 1.0f / static_cast<float>(top_bound - pos.y) };
	const float red_step  { red_diff   * flipped_distance_steps };
	const float green_step{ green_diff * flipped_distance_steps };
	const float blue_step { blue_diff  * flipped_distance_steps };


	auto buffer{ ctx->get_next_buffer() };
	for (size_t column{ static_cast<size_t>(pos.y) }; column < top_bound; ++column) {
		red_0   += red_step;
		green_0 += green_step;
		blue_0  += blue_step;

		const auto clr{ static_cast<color>(
			(static_cast<uint32_t>(red_0  )) << 11 |
			(static_cast<uint32_t>(green_0)) << 5 |
			(static_cast<uint32_t>(blue_0 ))
		) };

		const size_t offset{ static_cast<size_t>(column) * screen_w };
		for (size_t row{ static_cast<size_t>(pos.x) }; row < left_bound; ++row) {
			buffer[offset + row] = clr;
		}
	}
}



#pragma region RENDERING_LOOP
/*
static void render_loop(void *user) {
	using namespace utils::literals;
	auto &ctx{ *static_cast<render::context *>(user) };

	/// Display initializes Dedicated GPIO, so it should be created at the same
	/// core as gzn::graphics::defaults::render_thread_core_id. That's tricky
	/// moment and for sure should be handeled better
	// ctx.display = std::make_optional<tft::display>();
	auto &display{ ctx.display.get() };
	// display.configure(tft::make_initialization_sequence());
	// display.set_orientation(tft::display::orientation::landscape);

	while (true) {
		xSemaphoreTake(ctx.render_fence, portMAX_DELAY);

		display.send_screen_buffer(
			ctx.get_current_buffer(),
			ctx.pixel_size
		);

		/// @think maybe we should move it to submit or even make render::swap_buffers()
		ctx.swap_buffers();

		xSemaphoreGive(ctx.update_fence);
	}
}
*/
#pragma endregion RENDERING_LOOP


#if defined(GZN_ENABLE_FPS)

auto render::fps_color(const uint8_t fps) -> color {
	// Maybe we should configure it? Nah, it's debug shit
	if (fps >= 30) { return core::colors::green; }
	if (fps >= 15) { return core::colors::yellow; }
	return core::colors::red;
}

// debug mess, so don't actually care
void render::draw_fps(vec2u16 pos, const uint8_t fps) {

	const color color{ fps_color(fps) };
	auto data{ ctx->get_next_buffer() };
	const auto resolution_width{ resolution().w };
	const std::array lines{
		(pos.y + 0) * resolution_width,
		(pos.y + 1) * resolution_width,
		(pos.y + 2) * resolution_width,
		(pos.y + 3) * resolution_width,
		(pos.y + 4) * resolution_width,
	};
	const auto fill_matrix{ [&lines, &data, &pos] (const auto &m) {
		data[lines[0] + pos.x + 0] = m[ 0]; data[lines[0] + pos.x + 1] = m[ 1]; data[lines[0] + pos.x + 2] = m[ 2];
		data[lines[1] + pos.x + 0] = m[ 3]; data[lines[1] + pos.x + 1] = m[ 4]; data[lines[1] + pos.x + 2] = m[ 5];
		data[lines[2] + pos.x + 0] = m[ 6]; data[lines[2] + pos.x + 1] = m[ 7]; data[lines[2] + pos.x + 2] = m[ 8];
		data[lines[3] + pos.x + 0] = m[ 9]; data[lines[3] + pos.x + 1] = m[10]; data[lines[3] + pos.x + 2] = m[11];
		data[lines[4] + pos.x + 0] = m[12]; data[lines[4] + pos.x + 1] = m[13]; data[lines[4] + pos.x + 2] = m[14];
	} };

	const auto draw_number{ [&fill_matrix, c{ color }] (uint8_t number) {
		const auto b{ core::colors::black };

		switch (number) {
			case 0: {
				fill_matrix(std::array{
					c, c, c,
					c, b, c,
					c, b, c,
					c, b, c,
					c, c, c,
				});
			} break;

			case 1: {
				fill_matrix(std::array{
					b, b, c,
					b, b, c,
					b, b, c,
					b, b, c,
					b, b, c,
				});
			} break;

			case 2: {
				fill_matrix(std::array{
					c, c, c,
					b, b, c,
					c, c, c,
					c, b, b,
					c, c, c,
				});
			} break;

			case 3: {
				fill_matrix(std::array{
					c, c, c,
					b, b, c,
					c, c, c,
					b, b, c,
					c, c, c,
				});
			} break;

			case 4: {
				fill_matrix(std::array{
					c, b, c,
					c, b, c,
					c, c, c,
					b, b, c,
					b, b, c,
				});
			} break;

			case 5: {
				fill_matrix(std::array{
					c, c, c,
					c, b, b,
					c, c, c,
					b, b, c,
					c, c, c,
				});
			} break;

			case 6: {
				fill_matrix(std::array{
					c, c, c,
					c, b, b,
					c, c, c,
					c, b, c,
					c, c, c,
				});
			} break;

			case 7: {
				fill_matrix(std::array{
					c, c, c,
					b, b, c,
					b, b, c,
					b, b, c,
					b, b, c,
				});
			} break;

			case 8: {
				fill_matrix(std::array{
					c, c, c,
					c, b, c,
					c, c, c,
					c, b, c,
					c, c, c,
				});
			} break;

			case 9: {
				fill_matrix(std::array{
					c, c, c,
					c, b, c,
					c, c, c,
					b, b, c,
					c, c, c,
				});
			} break;

			default:
				break;
		}
	} };

	const auto first_number{ (fps / 100) % 10 };
	if (first_number != 0) {
		draw_number(first_number);
	}
	pos.x += 4;

	if (const auto second_number{ (fps / 10) % 10 }; second_number != 0 || first_number != 0) {
		draw_number(second_number);
	}
	pos.x += 4;

	draw_number(fps % 10);
}

#endif // defined(GZN_ENABLE_FPS)

} // namespace gzn::graphics

