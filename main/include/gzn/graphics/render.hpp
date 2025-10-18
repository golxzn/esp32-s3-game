#pragma once

#include <span>
#include <array>

#include "gzn/core/math.hpp"
#include "gzn/core/color.hpp"
#include "gzn/graphics/defaults.hpp"


namespace gzn::tft {

class display;

} // namespace gzn::tft


namespace gzn::graphics {

enum class init_status : uint8_t {
	success,
	already_initialized,
	invalid_arguments,
	not_enough_memory,
	failed_to_start_render_thread,
};

class render {
public:
	struct context;

	render() = delete;
	~render() = delete;

	render(const render &) = delete;
	render(render &&) noexcept = delete;

	auto operator=(const render &) -> render & = delete;
	auto operator=(render &&) noexcept -> render & = delete;

	[[nodiscard]] static auto initialize(
		tft::display &display,
		const uint8_t buffers_count = defaults::buffers_count,
		const uint8_t pixel_size = defaults::pixel_size
	) -> init_status;

	[[nodiscard]]
	static auto is_ready() -> bool;

	static void destroy();

	static void update(); ///< should be called in render loop

	/**
	 * @defgroup UnsafeInitializationRequired Unsafe! Initialization required
	 * @{
	 */
	static void submit();

	static auto resolution() noexcept -> gzn::vec2u16;
	static auto next_buffer_id() noexcept -> uint8_t;
	static auto current_buffer_id() noexcept -> uint8_t;

	static auto get_next_buffer() noexcept -> std::span<color>;
	static auto get_current_buffer() noexcept -> std::span<color>;

	static auto display() noexcept -> tft::display &;

	static void draw_rectangle(
		const vec2u16 pos,
		const vec2u16 size,
		const color clr = core::colors::white
	);

	static void draw_grid_pattern(
		const vec2u16 pos,
		const vec2u16 size,
		const std::array<color, 2> colors
	);

	static void draw_vertical_gradient(
		const vec2u16 pos,
		const vec2u16 size,
		const std::array<color, 2> colors
	);

	[[gnu::always_inline]]
	static inline void fill_screen_grid_pattern(const std::array<color, 2> colors) {
		draw_grid_pattern({}, resolution(), colors);
	}

	[[gnu::always_inline]]
	static inline void fill_screen_vertical_gradient(const std::array<color, 2> colors) {
		draw_vertical_gradient({}, resolution(), colors);
	}


#if defined(GZN_ENABLE_FPS)
	static auto fps_color(const uint8_t fps) -> color;
	static void draw_fps(vec2u16 pos, const uint8_t fps);
#endif // defined(GZN_ENABLE_FPS)

	/** @} */

private:
	inline static context *ctx{ nullptr };
};

} // namespace gzn::graphics

