#pragma once

#include <array>
#include <concepts>

namespace gzn::core {

template<class T>
concept enum_with_count = std::is_enum_v<T> && requires {
	{ static_cast<size_t>(T::COUNT) } -> std::same_as<size_t>;
};

template<enum_with_count Enum, class T>
struct enum_array : public std::array<T, static_cast<size_t>(Enum::COUNT)> {
	using base_class = std::array<T, static_cast<size_t>(Enum::COUNT)>;
	using enum_type  = Enum;

	[[gnu::always_inline]]
	inline auto operator[](const enum_type type) const -> const T & {
		return base_class::operator[](static_cast<base_class::size_type>(type));
	}

	[[gnu::always_inline]]
	inline auto operator[](const enum_type type) -> T & {
		return base_class::operator[](static_cast<base_class::size_type>(type));
	}
};

} // namespace gzn::core

