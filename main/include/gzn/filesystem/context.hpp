#pragma once

#include <array>
#include <limits>
#include <cstdint>
#include <string_view>

namespace gzn::fs {

constexpr size_t max_mounted_portations{ 8 };
using portations_array = std::array<std::string_view, max_mounted_portations>;

struct context {
	static constexpr auto npos{ (std::numeric_limits<size_t>::max)() };
	struct garbage_collector_config{
		uint8_t cleanup_frequency{ 40 };
		uint8_t cleanup_attempts {  3 };
	};

	portations_array         mounted_portations{};
	garbage_collector_config gc_config{};
	uint8_t                  gc_counter{};

	[[gnu::always_inline]]
	inline auto get_available_mount_id() -> size_t {
		for (size_t i{}; i < std::size(mounted_portations); ++i) {
			if (std::empty(mounted_portations[i])) {
				return i;
			}
		}
		return npos;
	}

	[[gnu::always_inline]]
	inline auto find_portation(const std::string_view portation) const -> bool {
		for (const auto &name : mounted_portations) {
			if (name == portation) {
				return true;
			}
		}
		return false;
	}
};

} // namespace gzn::fs

