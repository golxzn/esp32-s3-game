#pragma once

#include <cstdint>
#include <string_view>

namespace gzn::core {

constexpr uint32_t fnv1a(const std::string_view str) {
	constexpr uint32_t fnv_offset_basis{ 0x811C9DC5 };
	constexpr uint32_t fnv_prime{ 0x01000193 };

	const auto length{ std::size(str) / sizeof(uint32_t) };
	auto current{ reinterpret_cast<const uint32_t *>(std::data(str)) };

	uint32_t hash{ fnv_offset_basis };
	for (const auto end{ std::next(current, length) }; current != end; ++current) {
		hash = (hash ^ *current) * fnv_prime;
	}

	const auto last{ std::size(str) % sizeof(uint32_t) };
	auto lcur{ std::rbegin(str) };
	for (const auto lend{ std::next(lcur, last) }; lcur != lend; ++lcur) {
		hash = (hash ^ static_cast<uint32_t>(*lcur)) * fnv_prime;
	}
	return hash;
}

namespace literals {

constexpr uint32_t operator ""_hash(const char *value, size_t len) {
	return fnv1a({ value, len });
}

} // namespace literals

} // namespace gzn::core

