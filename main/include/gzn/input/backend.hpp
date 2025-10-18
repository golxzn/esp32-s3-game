#pragma once

#include <cstdint>

#include "gzn/input/actions.hpp"

namespace gzn::input {

namespace backend {

class usb;
// class bluetooth;

} // namespace backend

enum class backend_type : uint8_t {
	none      = 0x00,
	usb       = 0x01,
	// bluetooth = 0x02,
};

using backend_mask = uint8_t;

[[nodiscard]]
constexpr auto operator |(const backend_type lhv, const backend_type rhv) -> backend_mask {
	return static_cast<backend_mask>(static_cast<uint32_t>(lhv) | static_cast<uint32_t>(rhv));
}

[[nodiscard]]
constexpr auto operator |(const backend_mask lhv, const backend_type rhv) -> backend_mask {
	return static_cast<backend_mask>(static_cast<uint32_t>(lhv) | static_cast<uint32_t>(rhv));
}

[[nodiscard]]
constexpr auto operator |(const backend_type lhv, const backend_mask rhv) -> backend_mask {
	return static_cast<backend_mask>(static_cast<uint32_t>(lhv) | static_cast<uint32_t>(rhv));
}


[[nodiscard]]
constexpr auto operator &(const backend_type lhv, const backend_type rhv) -> backend_mask {
	return static_cast<backend_mask>(static_cast<uint32_t>(lhv) & static_cast<uint32_t>(rhv));
}

[[nodiscard]]
constexpr auto operator &(const backend_mask lhv, const backend_type rhv) -> backend_mask {
	return static_cast<backend_mask>(static_cast<uint32_t>(lhv) & static_cast<uint32_t>(rhv));
}

[[nodiscard]]
constexpr auto operator &(const backend_type lhv, const backend_mask rhv) -> backend_mask {
	return static_cast<backend_mask>(static_cast<uint32_t>(lhv) & static_cast<uint32_t>(rhv));
}


[[nodiscard]]
constexpr auto operator ^(const backend_type lhv, const backend_type rhv) -> backend_mask {
	return static_cast<backend_mask>(static_cast<uint32_t>(lhv) ^ static_cast<uint32_t>(rhv));
}

[[nodiscard]]
constexpr auto operator ^(const backend_mask lhv, const backend_type rhv) -> backend_mask {
	return static_cast<backend_mask>(static_cast<uint32_t>(lhv) ^ static_cast<uint32_t>(rhv));
}

[[nodiscard]]
constexpr auto operator ^(const backend_type lhv, const backend_mask rhv) -> backend_mask {
	return static_cast<backend_mask>(static_cast<uint32_t>(lhv) ^ static_cast<uint32_t>(rhv));
}


[[nodiscard]] constexpr auto operator ~(const backend_type value) -> backend_mask {
	return static_cast<backend_mask>(~static_cast<uint32_t>(value));
}


constexpr auto operator|=(backend_mask &lhv, const backend_type rhv) -> backend_mask & {
	return lhv = lhv | rhv;
}

constexpr auto operator&=(backend_mask &lhv, const backend_type rhv) -> backend_mask & {
	return lhv = lhv | rhv;
}

constexpr auto operator^=(backend_mask &lhv, const backend_type rhv) -> backend_mask & {
	return lhv = lhv | rhv;
}

} // namespace gzn::input


