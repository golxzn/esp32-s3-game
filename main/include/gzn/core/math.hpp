#pragma once

#include <cmath>
#include <cstdint>

namespace gzn::core {

struct vec2;
struct vec2s16;
struct vec2u16;

struct alignas(alignof(double)) vec2 {
	union { float x, w{}; };
	union { float y, h{}; };

	inline auto operator+=(const vec2 &other) noexcept -> vec2 &;
	inline auto operator-=(const vec2 &other) noexcept -> vec2 &;
	inline auto operator*=(const vec2 &other) noexcept -> vec2 &;
	inline auto operator/=(const vec2 &other) noexcept -> vec2 &;

	inline auto operator +(const vec2 &other) const noexcept -> vec2;
	inline auto operator -(const vec2 &other) const noexcept -> vec2;
	inline auto operator *(const vec2 &other) const noexcept -> vec2;
	inline auto operator /(const vec2 &other) const noexcept -> vec2;

	inline auto operator+=(float value) noexcept -> vec2 &;
	inline auto operator-=(float value) noexcept -> vec2 &;
	inline auto operator*=(float value) noexcept -> vec2 &;
	inline auto operator/=(float value) noexcept -> vec2 &;

	inline auto operator +(float value) const noexcept -> vec2;
	inline auto operator -(float value) const noexcept -> vec2;
	inline auto operator *(float value) const noexcept -> vec2;
	inline auto operator /(float value) const noexcept -> vec2;

	inline auto operator==(const vec2 &other) const noexcept -> bool;
	inline auto operator!=(const vec2 &other) const noexcept -> bool;

	inline static auto make(float x, float y) noexcept { return vec2{ .x = x, .y = y }; }
	inline static auto make(float value) noexcept { return make(value, value); }

	static auto dot(const vec2 &lhv, const vec2 &rhv) noexcept -> float;
	static auto cross(const vec2 &lhv, const vec2 &rhv) noexcept -> float;
	static auto length_squared(const vec2 &v) noexcept -> float;
	static auto length(const vec2 &v) noexcept -> float;
	static auto normalized(const vec2 &v) noexcept -> vec2;
};

struct alignas(alignof(int32_t)) vec2s16 {
	union { int16_t x, w{}; };
	union { int16_t y, h{}; };

	inline auto operator+=(const vec2s16 &other) noexcept -> vec2s16 &;
	inline auto operator-=(const vec2s16 &other) noexcept -> vec2s16 &;
	inline auto operator*=(const vec2s16 &other) noexcept -> vec2s16 &;
	inline auto operator/=(const vec2s16 &other) noexcept -> vec2s16 &;

	inline auto operator +(const vec2s16 &other) const noexcept -> vec2s16;
	inline auto operator -(const vec2s16 &other) const noexcept -> vec2s16;
	inline auto operator *(const vec2s16 &other) const noexcept -> vec2s16;
	inline auto operator /(const vec2s16 &other) const noexcept -> vec2s16;

	inline auto operator+=(int16_t value) noexcept -> vec2s16 &;
	inline auto operator-=(int16_t value) noexcept -> vec2s16 &;
	inline auto operator*=(int16_t value) noexcept -> vec2s16 &;
	inline auto operator/=(int16_t value) noexcept -> vec2s16 &;

	inline auto operator +(int16_t value) const noexcept -> vec2s16;
	inline auto operator -(int16_t value) const noexcept -> vec2s16;
	inline auto operator *(int16_t value) const noexcept -> vec2s16;
	inline auto operator /(int16_t value) const noexcept -> vec2s16;

	inline static auto make(int16_t x, int16_t y) noexcept { return vec2s16{ .x = x, .y = y }; }
	inline static auto make(int16_t value) noexcept { return make(value, value); }
	inline static auto from(vec2u16 other) noexcept -> vec2s16;
};

struct alignas(alignof(uint32_t)) vec2u16 {
	union { uint16_t x, w{}; };
	union { uint16_t y, h{}; };

	inline auto operator+=(const vec2u16 &other) noexcept -> vec2u16 &;
	inline auto operator-=(const vec2u16 &other) noexcept -> vec2u16 &;
	inline auto operator*=(const vec2u16 &other) noexcept -> vec2u16 &;
	inline auto operator/=(const vec2u16 &other) noexcept -> vec2u16 &;

	inline auto operator +(const vec2u16 &other) const noexcept -> vec2u16;
	inline auto operator -(const vec2u16 &other) const noexcept -> vec2u16;
	inline auto operator *(const vec2u16 &other) const noexcept -> vec2u16;
	inline auto operator /(const vec2u16 &other) const noexcept -> vec2u16;

	inline auto operator+=(uint16_t value) noexcept -> vec2u16 &;
	inline auto operator-=(uint16_t value) noexcept -> vec2u16 &;
	inline auto operator*=(uint16_t value) noexcept -> vec2u16 &;
	inline auto operator/=(uint16_t value) noexcept -> vec2u16 &;

	inline auto operator +(uint16_t value) const noexcept -> vec2u16;
	inline auto operator -(uint16_t value) const noexcept -> vec2u16;
	inline auto operator *(uint16_t value) const noexcept -> vec2u16;
	inline auto operator /(uint16_t value) const noexcept -> vec2u16;

	inline static auto make(uint16_t x, uint16_t y) noexcept { return vec2u16{ .x = x, .y = y }; }
	inline static auto make(uint16_t value) noexcept { return make(value, value); }
	inline static auto from(vec2s16 other) noexcept -> vec2u16;
	inline static auto from(vec2 other) noexcept -> vec2u16;
};

[[gnu::always_inline]]
inline auto vec2u16::from(const vec2s16 other) noexcept -> vec2u16 {
	return vec2u16{
		.x = static_cast<uint16_t>(other.x),
		.y = static_cast<uint16_t>(other.y)
	};
}

[[gnu::always_inline]]
inline auto vec2u16::from(const vec2 other) noexcept -> vec2u16 {
	return vec2u16{
		.x = static_cast<uint16_t>(other.x),
		.y = static_cast<uint16_t>(other.y)
	};
}


[[gnu::always_inline]]
inline auto vec2s16::from(const vec2u16 other) noexcept -> vec2s16 {
	return vec2s16{
		.x = static_cast<int16_t>(other.x),
		.y = static_cast<int16_t>(other.y)
	};
}

template<class T>
struct vec3 {
	using value_type = T;

	union { T x, r{}; };
	union { T y, g{}; };
	union { T z, b{}; };

	inline auto operator+=(const vec3 &other) noexcept -> vec3 & {
		x += other.x;
		y += other.y;
		z += other.z;
		return *this;
	}

	inline auto operator-=(const vec3 &other) noexcept -> vec3 & {
		x -= other.x;
		y -= other.y;
		z -= other.z;
		return *this;
	}

	inline auto operator*=(const vec3 &other) noexcept -> vec3 & {
		x *= other.x;
		y *= other.y;
		z *= other.z;
		return *this;
	}

	inline auto operator/=(const vec3 &other) noexcept -> vec3 & {
		x /= other.x;
		y /= other.y;
		z /= other.z;
		return *this;
	}

	inline auto operator +(const vec3 &other) const noexcept -> vec3 {
		return vec3{ .x = x + other.x, .y = y + other.y, .z = z + other.z };
	}

	inline auto operator -(const vec3 &other) const noexcept -> vec3 {
		return vec3{ .x = x - other.x, .y = y - other.y, .z = z - other.z };
	}

	inline auto operator *(const vec3 &other) const noexcept -> vec3 {
		return vec3{ .x = x * other.x, .y = y * other.y, .z = z * other.z };
	}

	inline auto operator /(const vec3 &other) const noexcept -> vec3 {
		return vec3{ .x = x / other.x, .y = y / other.y, .z = z / other.z };
	}

	inline auto operator+=(value_type value) noexcept -> vec3 & {
		x += value;
		y += value;
		z += value;
		return *this;
	}

	inline auto operator-=(value_type value) noexcept -> vec3 & {
		x -= value;
		y -= value;
		z -= value;
		return *this;
	}

	inline auto operator*=(value_type value) noexcept -> vec3 & {
		x *= value;
		y *= value;
		z *= value;
		return *this;
	}

	inline auto operator/=(value_type value) noexcept -> vec3 & {
		x /= value;
		y /= value;
		z /= value;
		return *this;
	}

	inline auto operator +(value_type value) const noexcept -> vec3 {
		return vec3{ .x = x + value, .y = y + value, .z = z + value };
	}

	inline auto operator -(value_type value) const noexcept -> vec3 {
		return vec3{ .x = x - value, .y = y - value, .z = z - value };
	}

	inline auto operator *(value_type value) const noexcept -> vec3 {
		return vec3{ .x = x * value, .y = y * value, .z = z * value };
	}

	inline auto operator /(value_type value) const noexcept -> vec3 {
		return vec3{ .x = x / value, .y = y / value, .z = z / value };
	}
};

} // namespace gzn::core

namespace gzn {

using core::vec2;
using core::vec2s16;
using core::vec2u16;

using vec3s8  = core::vec3< int8_t >;
using vec3u8  = core::vec3<uint8_t >;
using vec3s16 = core::vec3< int16_t>;
using vec3u16 = core::vec3<uint16_t>;
using vec3s32 = core::vec3< int32_t>;
using vec3u32 = core::vec3<uint32_t>;


} // namespace gzn

#include "./math.inl"

