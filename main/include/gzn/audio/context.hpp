#pragma once

#include <array>
#include <limits>
#include <cstdint>
#include <string_view>

#include <driver/ledc.h>

namespace gzn::audio {

using track_id = size_t;

enum class sample_rate : uint32_t {
	SR_8000_HZ  =  8'000,
	SR_11025_HZ = 11'025,
	SR_16000_HZ = 16'000,

	SR_MIN = SR_8000_HZ,
	SR_MAX = SR_16000_HZ,
};


inline constexpr int8_t   BITS_PER_SAMPLE{ 16 };
inline constexpr int8_t   BYTES_PER_SAMPLE{ BITS_PER_SAMPLE / 8 };
inline constexpr auto     DUTY_RESOLUTION{ LEDC_TIMER_14_BIT };
inline constexpr int16_t  VOLUME_0DB     { 16 };
inline constexpr auto     SAMPLE_RATE    { sample_rate::SR_8000_HZ };
inline constexpr auto     TIMER_ID       { LEDC_TIMER_0 };

inline constexpr uint32_t SOUND_TASK_CORE_ID{ 0u };
inline constexpr int32_t  SOUND_TASK_STACK_DEPTH { 2048 };
inline constexpr int32_t  SOUND_TASK_PRIORITY    {    1 };

inline constexpr track_id INVALID_TRACK_ID { (std::numeric_limits<track_id>::max)() };
inline constexpr size_t   MAX_SOUND_TRACKS{ 4 };
inline constexpr uint32_t SOUND_TRACKS_MASK{ []{
	uint32_t value{};
	for (uint32_t i{}; i < MAX_SOUND_TRACKS; ++i) {
		value |= (1 << i);
	}
	return value;
}() };


inline constexpr size_t SAMPLE_BATCH_SIZE{ 512u };

using samples_array = std::array<int16_t, SAMPLE_BATCH_SIZE>;

struct track_info {
	std::string_view filename{};
	int8_t           volume : 7 { 16 }; ///< from -16 to 16
	bool             loop   : 1 { false };
};

enum class startup_result : uint8_t {
	ok,
	already_started,
	invalid_arguments,
	not_enough_memory,
	cannot_create_task,
};

} // namespace gzn::audio

