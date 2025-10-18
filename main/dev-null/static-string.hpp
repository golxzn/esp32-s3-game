#pragma once

#include <array>
#include <algorithm>
#include <string_view>

namespace gzn {

template<class Symbol = char, size_t MaxLength = 256>
class basic_static_string {
public:
	using size_type   = size_t;
	using value_type  = Symbol;
	using pointer     = Symbol *;
	using string_view = std::basic_string_view<Symbol>;

	constexpr explicit basic_static_string(const string_view data = {}) noexcept {
		assign(data);
	}
	~basic_static_string() = default;

	constexpr basic_static_string(const basic_static_string &other) noexcept {
		assign(other.to_string_view());
	}
	constexpr basic_static_string(basic_static_string &&other) noexcept = default;

	constexpr auto operator=(const basic_static_string &other) noexcept -> basic_static_string & {
		if (this != &other) {
			assign(other.to_string_view());
		}
		return *this;
	}
	constexpr auto operator=(const string_view other) noexcept -> basic_static_string & {
		if (data() != std::data(other)) {
			assign(other);
		}
		return *this;
	}
	constexpr auto operator=(basic_static_string &&other) noexcept -> basic_static_string & = default;

	constexpr void assign(const string_view data) noexcept {
		length = std::min(std::size(data), MaxLength - 1);
		std::copy_n(std::begin(data), length, std::begin(buffer));
		buffer[length] = value_type{ '\0' };
	}

	void reset() noexcept { length = 0; }

	[[nodiscard]] constexpr auto c_str() const noexcept -> pointer {
		return std::data(buffer);
	}

	[[nodiscard]] constexpr auto data() const noexcept -> pointer {
		return std::data(buffer);
	}

	[[nodiscard]] constexpr auto size() const noexcept -> size_type {
		return std::size(buffer);
	}

	[[nodiscard]] constexpr auto max_size() const noexcept -> size_type {
		return MaxLength;
	}

	[[nodiscard]] constexpr auto empty() const noexcept -> bool {
		return length == 0;
	}

	[[nodiscard]] constexpr auto to_string_view() const noexcept -> string_view {
		return string_view{ std::data(buffer), length };
	}

private:
	std::array<Symbol, MaxLength> buffer{};
	size_t length{};
};

using static_string = basic_static_string<>;

} // namespace gzn

