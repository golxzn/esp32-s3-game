#include <span>
#include <array>
#include <cstdio>
#include <cstdint>

#include <esp_random.h>

struct vec2simd {
	union {
		struct { int16_t x{}, y{}; };
		uint32_t data;
	};

	[[gnu::always_inline]]
	inline auto operator+=(const vec2simd other) noexcept -> vec2simd & {
		asm volatile (R"asm(
			ee.movi.32.q q0, %0, 0
			ee.movi.32.q q1, %1, 0
			ee.vadds.s16 q0, q0, q1
			ee.movi.32.a q0, %0, 0
		)asm"
			:
			: "r"(&data), "r"(&other.data)  // %1 <- lhs32, %2 <- rhs32
			: "memory"                    // mention memory because we read/write *this
		);

		return *this;
	}
};

struct vec4naiv {
	union {
		struct { int16_t x{}, y{}, z{}, w{}; };
		uint64_t data;
	};

	[[gnu::always_inline]]
	inline auto operator+=(const vec4naiv &other) noexcept -> vec4naiv & {
		x += other.x;
		y += other.y;
		z += other.z;
		w += other.w;
		return *this;
	}
};

struct alignas(16) vec4simd {
	union {
		struct { int16_t x{}, y{}, z{}, w{}; };
		uint64_t data;
	};

	[[gnu::always_inline]]
	inline auto operator+=(const vec4simd &other) noexcept -> vec4simd & {
		asm volatile (R"(
			ee.vld.l.64.ip q0, %0, 0   // load this->data[63:0] -> q0
			ee.vld.l.64.ip q1, %1, 0   // load other.data[63:0] -> q1
			ee.vadds.s16   q0, q0, q1  // saturated 16-bit lane-wise add, result in q0
			ee.vst.l.64.ip q0, %0, 0   // store q0[63:0] -> this->data
		)"
			:
			: "r"(&data), "r"(&other.data)
			: "memory"
		);
		return *this;
	}
};

struct alignas(16) vec8simd {
	std::array<int16_t, 8> data{};

	[[gnu::always_inline]]
	inline auto operator+=(const vec8simd &other) noexcept -> vec8simd & {
		asm volatile (R"(
			ee.vld.128.ip  q0, %0, 0   // load this->data[63:0] -> q0
			ee.vld.128.ip  q1, %1, 0   // load other.data[63:0] -> q1
			ee.vadds.s16   q0, q0, q1  // saturated 16-bit lane-wise add, result in q0
			ee.vst.128.ip  q0, %0, 0   // store q0[63:0] -> this->data
		)"
			:
			: "r"(std::data(data)), "r"(std::data(other.data))
			: "memory"
		);
		return *this;
	}
};

struct alignas(16) vec8naiv {
	std::array<int16_t, 8> data{};

	[[gnu::always_inline]]
	inline auto operator+=(const vec8naiv &other) noexcept -> vec8naiv & {
		#pragma GCC unroll 8
		for (uint8_t i{}; i < std::size(data); ++i) {
			data[i] += other.data[i];
		}
		return *this;
	}
};


template<class T, class Fn>
std::pair<uint64_t, T> benchmark(const size_t attempts, Fn &&fn) {
	const volatile auto begin{ static_cast<uint64_t>(esp_timer_get_time()) };

	T test{};
	for (size_t i{}; i < attempts; ++i) {
		test = fn(test);
		asm volatile("" ::: "memory");
	}

	const volatile auto end{ static_cast<uint64_t>(esp_timer_get_time()) };
	return std::make_pair(end - begin, test);
}

void simd_benchmarks() {
	std::printf("-------------------------------------------------------------------------\n");
	constexpr size_t attempts{ 1'000 };
	const std::array<int16_t, 8> v{
		static_cast<int16_t>(esp_random() % 10),
		static_cast<int16_t>(esp_random() % 10),
		static_cast<int16_t>(esp_random() % 10),
		static_cast<int16_t>(esp_random() % 10),
		static_cast<int16_t>(esp_random() % 10),
		static_cast<int16_t>(esp_random() % 10),
		static_cast<int16_t>(esp_random() % 10),
		static_cast<int16_t>(esp_random() % 10)
	};

	constexpr auto print_array{ [] (const std::span<const int16_t> array) {
		std::printf("[%d", array.front());
		for (size_t i{ 1 }; i < std::size(array); ++i) {
			std::printf(", %.2d", array[i]);
		}
		std::printf("]\n");
	} };

	std::printf("test vector: ");
	print_array(v);
	std::printf("Operating vecN<type>::operator+= %zu times:\n\n", attempts);

	{
		const vec4naiv test_vec{ .x = v[0], .y = v[1], .z = v[2], .w = v[3] };
		const auto [time, value]{ benchmark<vec4naiv>(attempts, [&test_vec] (auto other) {
			return other += test_vec;
		}) };
		std::printf("vec4naiv: %4llu microseconds [%d, %d, %d, %d]\n",
			time, value.x, value.y, value.z, value.w
		);
	}

	{
		const vec4simd test_vec{ .x = v[0], .y = v[1], .z = v[2], .w = v[3] };
		const auto [time, value]{ benchmark<vec4simd>(attempts, [&test_vec] (auto other) {
			return other += test_vec;
		}) };
		std::printf("vec4simd: %4llu microseconds [%d, %d, %d, %d]\n",
			time, value.x, value.y, value.z, value.w
		);
	}

	{
		const vec8naiv test_vec{ .data = v };
		const auto [time, value]{ benchmark<vec8naiv>(attempts, [&test_vec] (auto other) {
			return other += test_vec;
		}) };
		std::printf("vec4naiv: %4llu microseconds ", time);
		print_array(value.data);
	}

	{
		const vec8simd test_vec{ .data = v };
		const auto [time, value]{ benchmark<vec8simd>(attempts, [&test_vec] (auto other) {
			return other += test_vec;
		}) };
		std::printf("vec4simd: %4llu microseconds ", time);
		print_array(value.data);
	}


	std::printf("-------------------------------------------------------------------------\n");
}


