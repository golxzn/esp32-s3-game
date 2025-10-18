#pragma once

#include <span>
#include <array>
#include <cstdint>

#include "gzn/audio/context.hpp"

namespace gzn::audio {

enum class status_t : uint8_t {
	un_init = 0, ///< pwm audio uninitializedn
	idle    = 1, ///< pwm audio idlen
	busy    = 2, ///< pwm audio busy
};

struct pwm {
	static auto startup() -> startup_result;
	static void shutdown();

	static void start();
	static void stop();

	[[nodiscard]]
	static auto status() -> status_t;

	static void set_track_volume(const track_id track, const uint8_t value);
	static void send_sample(std::span<const int16_t> samples);
	static void send_samples(
		const std::array<samples_array, MAX_SOUND_TRACKS> &samples_batch,
		const std::array<size_t,        MAX_SOUND_TRACKS> &batch_sizes,
		const size_t max_batch_size,
		const uint16_t active_batches_count
	);
};

using backend = pwm;

} // namespace gzn::audio

