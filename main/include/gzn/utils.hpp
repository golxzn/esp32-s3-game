#pragma once

#include <cstdint>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

namespace gzn::utils {

struct defer_semaphore_giver {
	SemaphoreHandle_t semaphore;

	constexpr explicit defer_semaphore_giver(SemaphoreHandle_t handle) noexcept
		: semaphore{ handle } {}

	defer_semaphore_giver(const defer_semaphore_giver &) = delete;
	defer_semaphore_giver &operator=(const defer_semaphore_giver &) = delete;

	[[gnu::always_inline]]
	inline ~defer_semaphore_giver() {
		xSemaphoreGive(semaphore);
	};
};

[[gnu::always_inline]]
constexpr inline uint32_t get_bit_at(const uint32_t data, const uint32_t id) {
	return (data >> id) & 0x01u;
}


void IRAM_ATTR delay_microsecons(uint32_t us);

[[gnu::always_inline]]
inline uint64_t get_time_ms() {
	return static_cast<uint64_t>(esp_timer_get_time()) / uint64_t{ 1000u };
};


namespace literals {

consteval TickType_t operator""_ns(const uint64_t ns) noexcept {
	return (ns * static_cast<TickType_t>(configTICK_RATE_HZ)) / 1000000u;
}

consteval TickType_t operator""_ms(const uint64_t ms) noexcept {
	return pdMS_TO_TICKS(ms);
}

consteval TickType_t operator""_s(const uint64_t sec) noexcept {
	// return ( ( TickType_t ) ( ( (TickType_t)ms * (TickType_t)configTICK_RATE_HZ) / ( TickType_t ) 1000U ) )
	return sec * static_cast<TickType_t>(configTICK_RATE_HZ);
}


consteval int32_t operator""_Hz(const uint64_t herz) noexcept {
	return herz;
}

consteval int32_t operator""_KHz(const uint64_t herz) noexcept {
	return herz * 1000;
}

consteval int32_t operator""_MHz(const uint64_t herz) noexcept {
	return herz * 1000000;
}

} // namespace literals

} // namespace gzn::utils

