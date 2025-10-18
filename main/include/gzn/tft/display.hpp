#pragma once

#if defined(GZN_TFT_USE_DEDICATED_GPIO)
#include <driver/dedic_gpio.h>
#endif // defined(GZN_TFT_USE_DEDICATED_GPIO)

#include "gzn/core/math.hpp"
#include "gzn/core/color.hpp"
#include "gzn/tft/commands.hpp"
#include "gzn/tft/constants.hpp"
#include "gzn/tft/initialization.hpp"

namespace gzn::tft {

class display {
public:
	explicit display();
	~display();

	display(const display &) = delete;
	display(display &&) noexcept = default;

	auto operator=(const display &) -> display & = delete;
	auto operator=(display &&) noexcept -> display & = default;

	enum class orientation : uint8_t {
		portrait,
		landscape,
		inverted_portrait,
		inverted_landscape
	};

	void configure(const std::span<const command> initialize_sequence) noexcept;
	void set_orientation(orientation value) noexcept;
	void reset() noexcept;

	void viewport(
		vec2u16 left_top_bound = vec2u16::make(0u),
		vec2u16 right_bottom_bound = constants::DISPLAY_SIZE - vec2u16::make(1u)
	) noexcept;

	void fill_rect(vec2u16 pos, vec2u16 size, color clr) noexcept;
	void clear_screen(color clr = core::colors::black) noexcept;

	void send_buffer_rect(
		vec2u16 pos, vec2u16 size,
		const std::span<const color> buffer,
		const uint16_t pixel_size
	) noexcept;

	[[gnu::always_inline]]
	inline void send_screen_buffer(
		const std::span<const color> buffer,
		const uint16_t pixel_size
	) noexcept {
		send_buffer_rect({}, m_size, buffer, pixel_size);
	}

	[[nodiscard]]
	auto size() const noexcept -> vec2u16 { return m_size; }

	[[nodiscard]]
		auto pixels_count() const noexcept -> size_t {
		return static_cast<size_t>(m_size.x) * static_cast<size_t>(m_size.y);
	}

	void execute(const command_id cmd) noexcept;
	void execute(const command &cmd) noexcept;
	void execute(const std::span<const command> commands) noexcept;

	void write_data(const uint8_t data) noexcept;

	void send_bits8(const uint8_t bits) noexcept;
	void send_bits16(const uint16_t bits) noexcept;

private:
	vec2u16 m_size{ constants::DISPLAY_SIZE };

	static auto initialize_gpio() -> esp_err_t;

#if defined(GZN_TFT_USE_DEDICATED_GPIO)
	dedic_gpio_bundle_handle_t m_output_bus{};

	static auto make_gpio_bundle() -> dedic_gpio_bundle_handle_t;
#endif // defined(GZN_TFT_USE_DEDICATED_GPIO)
};

} // namespace gzn::tft

