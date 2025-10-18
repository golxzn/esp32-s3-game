
namespace gzn::core {

#pragma region vec2

[[gnu::always_inline]]
inline auto vec2::operator+=(const vec2 &other) noexcept -> vec2 & {
	x += other.x;
	y += other.y;
	return *this;
}

[[gnu::always_inline]]
inline auto vec2::operator-=(const vec2 &other) noexcept -> vec2 & {
	x -= other.x;
	y -= other.y;
	return *this;
}

[[gnu::always_inline]]
inline auto vec2::operator*=(const vec2 &other) noexcept -> vec2 & {
	x *= other.x;
	y *= other.y;
	return *this;
}

[[gnu::always_inline]]
inline auto vec2::operator/=(const vec2 &other) noexcept -> vec2 & {
	x /= other.x;
	y /= other.y;
	return *this;
}


[[gnu::always_inline]]
inline auto vec2::operator +(const vec2 &other) const noexcept -> vec2 {
	return vec2{
		.x = x + other.x,
		.y = y + other.y
	};
}

[[gnu::always_inline]]
inline auto vec2::operator -(const vec2 &other) const noexcept -> vec2 {
	return vec2{
		.x = x - other.x,
		.y = y - other.y
	};
}

[[gnu::always_inline]]
inline auto vec2::operator *(const vec2 &other) const noexcept -> vec2 {
	return vec2{
		.x = x * other.x,
		.y = y * other.y
	};
}

[[gnu::always_inline]]
inline auto vec2::operator /(const vec2 &other) const noexcept -> vec2 {
	return vec2{
		.x = x / other.x,
		.y = y / other.y
	};
}


[[gnu::always_inline]]
inline auto vec2::operator+=(float value) noexcept -> vec2 & {
	x += value;
	y += value;
	return *this;
}

[[gnu::always_inline]]
inline auto vec2::operator-=(float value) noexcept -> vec2 & {
	x -= value;
	y -= value;
	return *this;
}

[[gnu::always_inline]]
inline auto vec2::operator*=(float value) noexcept -> vec2 & {
	x *= value;
	y *= value;
	return *this;
}

[[gnu::always_inline]]
inline auto vec2::operator/=(float value) noexcept -> vec2 & {
	x /= value;
	y /= value;
	return *this;
}


[[gnu::always_inline]]
inline auto vec2::operator +(float value) const noexcept -> vec2 {
	return vec2{
		.x = x + value,
		.y = y + value
	};
}

[[gnu::always_inline]]
inline auto vec2::operator -(float value) const noexcept -> vec2 {
	return vec2{
		.x = x - value,
		.y = y - value
	};
}

[[gnu::always_inline]]
inline auto vec2::operator *(float value) const noexcept -> vec2 {
	return vec2{
		.x = x * value,
		.y = y * value
	};
}

[[gnu::always_inline]]
inline auto vec2::operator /(float value) const noexcept -> vec2 {
	return vec2{
		.x = x / value,
		.y = y / value
	};
}

[[gnu::always_inline]]
inline auto vec2::operator==(const vec2 &other) const noexcept -> bool {
	return other.x == x && other.y == y;
}

[[gnu::always_inline]]
inline auto vec2::operator!=(const vec2 &other) const noexcept -> bool {
	return other.x != x || other.y != y;
}

[[gnu::always_inline]]
inline auto vec2::dot(const vec2 &lhv, const vec2 &rhv) noexcept -> float {
	return lhv.x * rhv.x + lhv.y * rhv.y;
}

[[gnu::always_inline]]
inline auto vec2::cross(const vec2 &lhv, const vec2 &rhv) noexcept -> float {
	return lhv.x * rhv.y - lhv.y * rhv.x;
}

[[gnu::always_inline]]
inline auto vec2::length_squared(const vec2 &v) noexcept -> float {
	return v.x * v.x + v.y * v.y;
}

[[gnu::always_inline]]
inline auto vec2::length(const vec2 &v) noexcept -> float {
	return std::sqrt(length_squared(v));
}

[[gnu::always_inline]]
inline auto vec2::normalized(const vec2 &v) noexcept -> vec2 {
	return v * (1.0f / length(v));
	// return v / length(v);
}

#pragma endregion vec2


#pragma region vec2s16

[[gnu::always_inline]]
inline auto vec2s16::operator+=(const vec2s16 &other) noexcept -> vec2s16 & {
	x += other.x;
	y += other.y;
	return *this;
}

[[gnu::always_inline]]
inline auto vec2s16::operator-=(const vec2s16 &other) noexcept -> vec2s16 & {
	x -= other.x;
	y -= other.y;
	return *this;
}

[[gnu::always_inline]]
inline auto vec2s16::operator*=(const vec2s16 &other) noexcept -> vec2s16 & {
	x *= other.x;
	y *= other.y;
	return *this;
}

[[gnu::always_inline]]
inline auto vec2s16::operator/=(const vec2s16 &other) noexcept -> vec2s16 & {
	x /= other.x;
	y /= other.y;
	return *this;
}


[[gnu::always_inline]]
inline auto vec2s16::operator +(const vec2s16 &other) const noexcept -> vec2s16 {
	//
	// EE.VADDS.S16 qa, qx, qy
	//
	// This instruction performs a vector addition on 16-bit data in the two
	// registers qx and qy. Then, the 8 results obtained from the calculation
	// are saturated, and the saturated results are written to register qa.


	// This instruction performs a vector addition on 16-bit data in the two
	// registers qx and qy. Then, the 8 results obtained from the calculation
	// are saturated, and the saturated results are written to register qa.
	// During the operation, the instruction forces the lower 4 bits of the
	// access address in register as to 0 and stores the value in register qv
	// to memory. After the access, the value in register as is incremented by
	// 16.

 //    asm volatile("ee.vld.128.ip qx, %0, 16" ::"a"(x));
 //    asm volatile("ee.vld.128.ip q1, %0, 16" ::"a"(y));
	//
	// uint64_t output[2];
	// // EE.VADDS.S16.ST.INCP qv, as, qa, qx, qy
	// asm {
	// 	ee.vld.128.ip qx, %0, 16
	// 	ee.vld.128.ip qy, %1, 16
	// 	ee.vadds.s16.st.incp qv, %0, qa, qx, qy
	// 	: "0"(
	// 	: "=r"(output)
	// };

	return vec2s16{
		.x = static_cast<int16_t>(x + other.x),
		.y = static_cast<int16_t>(y + other.y)
	};
}

[[gnu::always_inline]]
inline auto vec2s16::operator -(const vec2s16 &other) const noexcept -> vec2s16 {
	return vec2s16{
		.x = static_cast<int16_t>(x - other.x),
		.y = static_cast<int16_t>(y - other.y)
	};
}

[[gnu::always_inline]]
inline auto vec2s16::operator *(const vec2s16 &other) const noexcept -> vec2s16 {
	return vec2s16{
		.x = static_cast<int16_t>(x * other.x),
		.y = static_cast<int16_t>(y * other.y)
	};
}

[[gnu::always_inline]]
inline auto vec2s16::operator /(const vec2s16 &other) const noexcept -> vec2s16 {
	return vec2s16{
		.x = static_cast<int16_t>(x / other.x),
		.y = static_cast<int16_t>(y / other.y)
	};
}


[[gnu::always_inline]]
inline auto vec2s16::operator+=(int16_t value) noexcept -> vec2s16 & {
	x += value;
	y += value;
	return *this;
}

[[gnu::always_inline]]
inline auto vec2s16::operator-=(int16_t value) noexcept -> vec2s16 & {
	x -= value;
	y -= value;
	return *this;
}

[[gnu::always_inline]]
inline auto vec2s16::operator*=(int16_t value) noexcept -> vec2s16 & {
	x *= value;
	y *= value;
	return *this;
}

[[gnu::always_inline]]
inline auto vec2s16::operator/=(int16_t value) noexcept -> vec2s16 & {
	x /= value;
	y /= value;
	return *this;
}


[[gnu::always_inline]]
inline auto vec2s16::operator +(int16_t value) const noexcept -> vec2s16 {
	return vec2s16{
		.x = static_cast<int16_t>(x + value),
		.y = static_cast<int16_t>(y + value)
	};
}

[[gnu::always_inline]]
inline auto vec2s16::operator -(int16_t value) const noexcept -> vec2s16 {
	return vec2s16{
		.x = static_cast<int16_t>(x - value),
		.y = static_cast<int16_t>(y - value)
	};
}

[[gnu::always_inline]]
inline auto vec2s16::operator *(int16_t value) const noexcept -> vec2s16 {
	return vec2s16{
		.x = static_cast<int16_t>(x * value),
		.y = static_cast<int16_t>(y * value)
	};
}

[[gnu::always_inline]]
inline auto vec2s16::operator /(int16_t value) const noexcept -> vec2s16 {
	return vec2s16{
		.x = static_cast<int16_t>(x / value),
		.y = static_cast<int16_t>(y / value)
	};
}

#pragma endregion vec2s16


#pragma region vec2u16

[[gnu::always_inline]]
inline auto vec2u16::operator+=(const vec2u16 &other) noexcept -> vec2u16 & {
	x += other.x;
	y += other.y;
	return *this;
}

[[gnu::always_inline]]
inline auto vec2u16::operator-=(const vec2u16 &other) noexcept -> vec2u16 & {
	x -= other.x;
	y -= other.y;
	return *this;
}

[[gnu::always_inline]]
inline auto vec2u16::operator*=(const vec2u16 &other) noexcept -> vec2u16 & {
	x *= other.x;
	y *= other.y;
	return *this;
}

[[gnu::always_inline]]
inline auto vec2u16::operator/=(const vec2u16 &other) noexcept -> vec2u16 & {
	x /= other.x;
	y /= other.y;
	return *this;
}


[[gnu::always_inline]]
inline auto vec2u16::operator +(const vec2u16 &other) const noexcept -> vec2u16 {
	//
	// EE.VADDS.U16 qa, qx, qy
	//
	// This instruction performs a vector addition on 16-bit data in the two
	// registers qx and qy. Then, the 8 results obtained from the calculation
	// are saturated, and the saturated results are written to register qa.


	// This instruction performs a vector addition on 16-bit data in the two
	// registers qx and qy. Then, the 8 results obtained from the calculation
	// are saturated, and the saturated results are written to register qa.
	// During the operation, the instruction forces the lower 4 bits of the
	// access address in register as to 0 and stores the value in register qv
	// to memory. After the access, the value in register as is incremented by
	// 16.

 //    asm volatile("ee.vld.128.ip qx, %0, 16" ::"a"(x));
 //    asm volatile("ee.vld.128.ip q1, %0, 16" ::"a"(y));
	//
	// uint64_t output[2];
	// // EE.VADDS.U16.ST.INCP qv, as, qa, qx, qy
	// asm {
	// 	ee.vld.128.ip qx, %0, 16
	// 	ee.vld.128.ip qy, %1, 16
	// 	ee.vadds.s16.st.incp qv, %0, qa, qx, qy
	// 	: "0"(
	// 	: "=r"(output)
	// };

	return vec2u16{
		.x = static_cast<uint16_t>(x + other.x),
		.y = static_cast<uint16_t>(y + other.y)
	};
}

[[gnu::always_inline]]
inline auto vec2u16::operator -(const vec2u16 &other) const noexcept -> vec2u16 {
	return vec2u16{
		.x = static_cast<uint16_t>(x - other.x),
		.y = static_cast<uint16_t>(y - other.y)
	};
}

[[gnu::always_inline]]
inline auto vec2u16::operator *(const vec2u16 &other) const noexcept -> vec2u16 {
	return vec2u16{
		.x = static_cast<uint16_t>(x * other.x),
		.y = static_cast<uint16_t>(y * other.y)
	};
}

[[gnu::always_inline]]
inline auto vec2u16::operator /(const vec2u16 &other) const noexcept -> vec2u16 {
	return vec2u16{
		.x = static_cast<uint16_t>(x / other.x),
		.y = static_cast<uint16_t>(y / other.y)
	};
}


[[gnu::always_inline]]
inline auto vec2u16::operator+=(uint16_t value) noexcept -> vec2u16 & {
	x += value;
	y += value;
	return *this;
}

[[gnu::always_inline]]
inline auto vec2u16::operator-=(uint16_t value) noexcept -> vec2u16 & {
	x -= value;
	y -= value;
	return *this;
}

[[gnu::always_inline]]
inline auto vec2u16::operator*=(uint16_t value) noexcept -> vec2u16 & {
	x *= value;
	y *= value;
	return *this;
}

[[gnu::always_inline]]
inline auto vec2u16::operator/=(uint16_t value) noexcept -> vec2u16 & {
	x /= value;
	y /= value;
	return *this;
}


[[gnu::always_inline]]
inline auto vec2u16::operator +(uint16_t value) const noexcept -> vec2u16 {
	return vec2u16{
		.x = static_cast<uint16_t>(x + value),
		.y = static_cast<uint16_t>(y + value)
	};
}

[[gnu::always_inline]]
inline auto vec2u16::operator -(uint16_t value) const noexcept -> vec2u16 {
	return vec2u16{
		.x = static_cast<uint16_t>(x - value),
		.y = static_cast<uint16_t>(y - value)
	};
}

[[gnu::always_inline]]
inline auto vec2u16::operator *(uint16_t value) const noexcept -> vec2u16 {
	return vec2u16{
		.x = static_cast<uint16_t>(x * value),
		.y = static_cast<uint16_t>(y * value)
	};
}

[[gnu::always_inline]]
inline auto vec2u16::operator /(uint16_t value) const noexcept -> vec2u16 {
	return vec2u16{
		.x = static_cast<uint16_t>(x / value),
		.y = static_cast<uint16_t>(y / value)
	};
}


#pragma endregion vec2u16

} // namespace gzn::core

