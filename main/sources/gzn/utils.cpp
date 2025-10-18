#include "gzn/utils.hpp"

namespace gzn::utils {

void delay_microsecons(uint32_t us) {
	if (!us) {
		return;
	}

	const uint64_t m{ static_cast<uint64_t>(esp_timer_get_time()) };
	const uint64_t e{ m + us };
	if (m > e) { //overflow
		while (static_cast<uint64_t>(esp_timer_get_time()) > e) {
			asm volatile ("nop");
		}
	}

	while (static_cast<uint64_t>(esp_timer_get_time()) < e) {
		asm volatile ("nop");
	}
}

} // namespace gzn::utils

