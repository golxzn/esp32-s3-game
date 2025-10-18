#pragma once

#include "gzn/audio/context.hpp"

namespace gzn::audio {

class manager {
public:
	struct setup_info{};

	[[nodiscard]]
	static auto initialize(const setup_info &info = {}) -> startup_result;
	static void destroy();

	static void update();

	static auto play(const track_info &info) -> track_id;
	static auto stop(const track_id track) -> bool;
	static auto stop_all() -> size_t;

	[[nodiscard]]
	static inline auto is_playing(const track_id id) -> bool;
};


} // namespace gzn::audio

