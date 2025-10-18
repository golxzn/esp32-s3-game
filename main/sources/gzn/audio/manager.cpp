#include <span>
#include <cstdio>
#include <cerrno>
#include <cstring>
#include <utility>
#include <esp_log.h>

#include "gzn/audio/manager.hpp"

#include "gzn/utils.hpp"
#include "gzn/audio/wav-format.hpp"
#include "gzn/audio/backend/pwm.hpp"

namespace gzn::audio {

namespace {

inline constexpr auto TAG{ "gzn::audio" };

struct file_info {
	std::FILE        *file_descr{ nullptr };
	SemaphoreHandle_t file_guard{};
	size_t            content_offset{}; ///< if not 0, then it's looping file

	[[gnu::always_inline]]
	inline void close(const BaseType_t delay = portMAX_DELAY) {
		xSemaphoreTake(file_guard, delay);
		content_offset = 0;
		std::fclose(std::exchange(file_descr, nullptr));
		xSemaphoreGive(file_guard);
	}
};

struct sound_streaming_task_context {
	std::array<file_info,     MAX_SOUND_TRACKS> streams{};
	std::array<samples_array, MAX_SOUND_TRACKS> batches{};
	std::array<size_t,        MAX_SOUND_TRACKS> batches_sizes{};
};

struct pwm_context {
	sound_streaming_task_context sound_streaming_context{};
	portMUX_TYPE                 file_guard_lock portMUX_INITIALIZER_UNLOCKED;
	TaskHandle_t                 sound_streaming_handle{};
	uint32_t                     tracks_bitset{};
	bool                         running{ true };

	[[gnu::always_inline]]
	inline auto can_add_track() const -> bool {
		return (tracks_bitset & SOUND_TRACKS_MASK) != SOUND_TRACKS_MASK;
	}

	[[gnu::always_inline]]
	inline auto is_track_booked(const size_t index) const -> bool {
		return static_cast<bool>(tracks_bitset & (1 << index));
	}

	[[gnu::always_inline]]
	inline void book_track(const size_t index) {
		tracks_bitset |= (1 << index);
	}

	[[gnu::always_inline]]
	inline void unbook_track(const size_t index) {
		tracks_bitset &= ~(1 << index);
	}

	[[gnu::always_inline]]
	inline auto get_available_track_id() const -> size_t {
		for (size_t i{}; i < MAX_SOUND_TRACKS; ++i) {
			if (!(tracks_bitset & (1 << i))) {
				return i;
			}
		}
		return MAX_SOUND_TRACKS;
	}
};

pwm_context *backend_ctx{};

void IRAM_ATTR sound_streaming_task(void *user) {
	using namespace utils::literals;

	constexpr auto file_guard_timeout{ 100 };

	auto &ctx{ *reinterpret_cast<sound_streaming_task_context *>(user) };

	auto &batches_sizes{ ctx.batches_sizes };
	auto &batches{ ctx.batches };

	while (backend_ctx->running) {
		// STEP 0. Fetch data from files & manage their EOFs

		size_t max_batch_size{};
		size_t active_batches_count{};
		size_t last_batch_id{};
		for (size_t i{}; i < std::size(batches); ++i) {
			auto &stream{ ctx.streams[i] };

			if (stream.file_descr == nullptr) {
				batches_sizes[i] = 0;
				continue;
			}
			if (pdFALSE == xSemaphoreTake(stream.file_guard, file_guard_timeout)) {
				continue;
			}
			const utils::defer_semaphore_giver defer{ stream.file_guard };

			last_batch_id = i;
			++active_batches_count;
			batches_sizes[i] = std::fread(
				std::data(batches[i]), sizeof(batches[i][0]),
				std::size(batches[i]), stream.file_descr
			);
			if (max_batch_size < batches_sizes[i]) {
				max_batch_size = batches_sizes[i];
			}

			if (!std::feof(stream.file_descr)) {
				continue;
			}

			if (0 != stream.content_offset
			&&  0 == std::fseek(stream.file_descr, stream.content_offset, SEEK_SET)
			) [[likely]] {
				continue;
			}

			std::fclose(std::exchange(stream.file_descr, nullptr));
		}

		if (active_batches_count == 0 || max_batch_size == 0) {
			vTaskDelay(100_ms);
			// if (backend::status() == status_t::busy) {
			// 	backend::stop();
			// }
			continue;
		}
		// if (backend::status() != status_t::busy) {
		// 	backend::start();
		// }


		if (active_batches_count == 1) {
			backend::send_sample(std::span{
				std::data(batches[last_batch_id]),
				batches_sizes[last_batch_id]
			});
			continue;
		}

		backend::send_samples(
			batches,
			batches_sizes,
			max_batch_size,
			active_batches_count
		);
	}
}

} // namespace

auto manager::initialize(const setup_info &info) -> startup_result {
	if (const auto err{ backend::startup() }; startup_result::ok != err) {
		return err;
	}

	using namespace gzn::utils::literals;

	if (backend_ctx) {
		ESP_LOGE(TAG, "Audio backend is already up!");
		return startup_result::already_started;
	}

	auto backend_storage{ heap_caps_malloc(sizeof(pwm_context),
		MALLOC_CAP_8BIT | MALLOC_CAP_SIMD
	) };

	backend_ctx = new (backend_storage) pwm_context{};
	if (!backend_ctx) {
		ESP_LOGE(TAG, "Cannot allocate backend context!");
		return startup_result::not_enough_memory;
	}

	for (auto &info : backend_ctx->sound_streaming_context.streams) {
		portENTER_CRITICAL(&backend_ctx->file_guard_lock);
		info.file_guard = xSemaphoreCreateMutex();
		portEXIT_CRITICAL(&backend_ctx->file_guard_lock);
	}

	const auto status{ xTaskCreatePinnedToCore(
		sound_streaming_task, "sound_streaming_task",
		SOUND_TASK_STACK_DEPTH, &backend_ctx->sound_streaming_context,
		SOUND_TASK_PRIORITY, &backend_ctx->sound_streaming_handle,
		SOUND_TASK_CORE_ID
	) };
	if (status != pdPASS) {
		destroy();
		return startup_result::cannot_create_task;
	}

	return startup_result::ok;
}

void manager::destroy() {
	using namespace gzn::utils::literals;

	backend_ctx->running = false;

	for (auto &info : backend_ctx->sound_streaming_context.streams) {
		if (info.file_descr) {
			info.close(1000_ms);
		}
	}

	vTaskDelay(100_ms);

	for (auto &info : backend_ctx->sound_streaming_context.streams) {
		portENTER_CRITICAL(&backend_ctx->file_guard_lock);
		vSemaphoreDelete(info.file_guard);
		portEXIT_CRITICAL(&backend_ctx->file_guard_lock);
	}

	vTaskDelete(backend_ctx->sound_streaming_handle);

	heap_caps_free(std::exchange(backend_ctx, nullptr));

	backend::shutdown();
}


void manager::update() {
	const auto &streams{ backend_ctx->sound_streaming_context.streams };
	for (size_t i{}; i < std::size(streams); ++i) {
		if (streams[i].file_descr == nullptr) {
			backend_ctx->unbook_track(i);
		}
	}
}

auto manager::play(const track_info &info) -> track_id {
	if (!backend_ctx->can_add_track()) [[unlikely]] {
		ESP_LOGW(TAG, R"(No available space to load "%.*s" sound)",
			static_cast<int>(std::size(info.filename)), std::data(info.filename)
		);
		return INVALID_TRACK_ID;
	}

	using namespace utils::literals;
	constexpr auto file_guard_timeout{ 50_ms };

	static std::array<uint8_t, sizeof(wav::header)> header_data{};
	auto &ctx{ backend_ctx->sound_streaming_context };
	while (backend_ctx->can_add_track()) {
		const auto id{ backend_ctx->get_available_track_id() };
		auto &stream{ ctx.streams[id] };

		if (pdFALSE == xSemaphoreTake(stream.file_guard, file_guard_timeout)) {
			continue;
		}

		const utils::defer_semaphore_giver defer{ stream.file_guard };

		if (stream.file_descr) [[unlikely]] {
			backend_ctx->book_track(id);
			continue;
		}

		stream.file_descr = std::fopen(std::data(info.filename), "rb");
		if (!stream.file_descr) [[unlikely]] {
			ESP_LOGW(TAG, R"(Cannot open "%.*s" audio file: %s)",
				static_cast<int>(std::size(info.filename)),
				std::data(info.filename), std::strerror(errno)
			);
			break;
		}

		const auto read{ std::fread(
			std::data(header_data), sizeof(uint8_t), std::size(header_data),
			stream.file_descr
		) };
		if (read < std::size(header_data)) [[unlikely]] {
			std::fclose(std::exchange(stream.file_descr, nullptr));
			ESP_LOGW(TAG, R"(File size of "%.*s" is less than WAV file header)",
				static_cast<int>(std::size(info.filename)),
				std::data(info.filename)
			);
			break;
		}

		const auto header{ wav::header::make_view(header_data) };
#if defined(GZN_DEBUG)
		header->dump();
#endif // defined(GZN_DEBUG)

		if (BITS_PER_SAMPLE != header->format.bits_per_sample) [[unlikely]] {
			std::fclose(std::exchange(stream.file_descr, nullptr));
			ESP_LOGE(TAG, "Sound has %zu BPS, but %zu required.",
				header->format.bits_per_sample, BITS_PER_SAMPLE
			);
			break;
		}

		backend::set_track_volume(id, info.volume);

		if (info.loop) {
			stream.content_offset = read + 1;
		}

		backend_ctx->book_track(id);
		return id;
	}

	return INVALID_TRACK_ID;
}

auto manager::stop(const track_id track) -> bool {
	if (!backend_ctx->is_track_booked(track)) {
		ESP_LOGW(TAG, "Failed to unload %zu track. Not found", track);
		return false;
	}

	auto &stream{ backend_ctx->sound_streaming_context.streams[track] };
	if (stream.file_descr != nullptr) {
		stream.close();
	}

	backend_ctx->unbook_track(track);
	return true;
}

auto manager::stop_all() -> size_t {
	size_t count{};
	for (track_id track{}; track < MAX_SOUND_TRACKS; ++track) {
		count += static_cast<size_t>(stop(track));
	}
	return count;
}

[[gnu::always_inline]]
inline auto manager::is_playing(const track_id track) -> bool {
	return backend_ctx->is_track_booked(track);
}

} // namespace gzn::audio

