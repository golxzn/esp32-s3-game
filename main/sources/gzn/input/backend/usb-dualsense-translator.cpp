#include <usb/hid_host.h>

#include "gzn/input/backend/translators.hpp"

namespace gzn::input::backend {

/*
enum direction_t {
	neutral = 0x8,
	N       = 0x0,
	NE      = 0x1,
	E       = 0x2,
	SE      = 0x3,
	S       = 0x4,
	SW      = 0x5,
	W       = 0x6,
	NW      = 0x7
};
*/

struct [[gnu::packed]] dualsense_bytes {
	uint8_t protocol{};

	uint8_t lsx{};
	uint8_t lsy{};
	uint8_t rsx{};
	uint8_t rsy{};

	uint8_t l2axis{};
	uint8_t r2axis{};

	uint8_t seq_num{};

	union {
		uint8_t buttons0_bitset{};
		struct {
			uint8_t direction : 4 {}; // see direction_t
			uint8_t square    : 1 {};
			uint8_t cross     : 1 {};
			uint8_t circle    : 1 {};
			uint8_t triangle  : 1 {};
		} buttons0;
	};

	struct alignas(alignof(uint8_t)) {
		uint8_t l1     : 1 {};
		uint8_t r1     : 1 {};
		uint8_t l2     : 1 {};
		uint8_t r2     : 1 {};
		uint8_t create : 1 {};
		uint8_t option : 1 {};
		uint8_t l3     : 1 {};
		uint8_t r3     : 1 {};
	} buttons1{};

	struct alignas(alignof(uint8_t)) {
		uint8_t ps       : 1 {};
		uint8_t touchpad : 1 {};
		uint8_t mute     : 1 {};
		uint8_t reserved : 5 {};
	} buttons2{};
	uint8_t buttons3{};

	uint8_t timestamp0{};
	uint8_t timestamp1{};
	uint8_t timestamp2{};
	uint8_t timestamp3{};

	uint8_t gyro_x0{};
	uint8_t gyro_x1{};
	uint8_t gyro_y0{};
	uint8_t gyro_y1{};
	uint8_t gyro_z0{};
	uint8_t gyro_z1{};

	uint8_t accel_x0{};
	uint8_t accel_x1{};
	uint8_t accel_y0{};
	uint8_t accel_y1{};
	uint8_t accel_z0{};
	uint8_t accel_z1{};

	uint8_t sensor_timestamp0{};
	uint8_t sensor_timestamp1{};
	uint8_t sensor_timestamp2{};
	uint8_t sensor_timestamp3{};

	uint8_t _reserved_0{};

	uint8_t touch00{};
	uint8_t touch01{};
	uint8_t touch02{};
	uint8_t touch03{};
	uint8_t touch10{};
	uint8_t touch11{};
	uint8_t touch12{};
	uint8_t touch13{};

	uint8_t _reserved_1{};

	uint8_t r2feedback{};
	uint8_t l2feedback{};

	uint8_t _reserved_2[9]{};

	uint8_t battery0{};
	uint8_t battery1{};

	uint8_t _reserved_3[5]{};
	uint8_t checksum[4]{};
};
static_assert(sizeof(dualsense_bytes) == 64, "dualsense struct has to be 64 bytes");

[[gnu::always_inline]]
static inline auto normalize(const uint8_t value) -> int8_t {
	return static_cast<int8_t>(static_cast<int16_t>(value) - 128);
}

void dualsense_usb_translator(const std::span<const uint8_t> data, action_array &actions) {
	if (std::size(data) < sizeof(dualsense_bytes)) {
		return;
	}

	const auto &ds_report{
		*reinterpret_cast<const dualsense_bytes *>(std::data(data))
	};

	const auto timestamp{ static_cast<uint64_t>(esp_timer_get_time()) };

	static dualsense_bytes previous_report{};

	actions[action_type::horizontal_move].horizontal = normalize(ds_report.lsx);
	actions[action_type::vertical_move  ].vertical   = normalize(ds_report.lsy);

	const auto update_button{ [&actions, timestamp] (const action_type type, const bool value) {
		auto &action{ actions[type] };
		action.timestamp = timestamp;
		action.pressed   = value;
	} };

	/// @note We could add another actions here
	if (ds_report.buttons0_bitset != previous_report.buttons0_bitset) {
		if (ds_report.buttons0.cross != previous_report.buttons0.cross) {
			update_button(action_type::attack, ds_report.buttons0.cross);
		}
		if (ds_report.buttons0.circle != previous_report.buttons0.circle) {
			update_button(action_type::use, ds_report.buttons0.circle);
		}
	}

	previous_report = ds_report;
}

} // namespace gzn::input::backend

