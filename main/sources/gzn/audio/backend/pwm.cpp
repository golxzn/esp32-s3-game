#include <array>
#include <cstdio>
#include <utility>
#include <cassert>
#include <cstring>
#include <algorithm>

#include <esp_log.h>
#include <driver/ledc.h>
#include <driver/gptimer.h>
#include <soc/ledc_struct.h>

#include <freertos/FreeRTOS.h>
#include <freertos/ringbuf.h>
#include <freertos/semphr.h>

#include "gzn/audio/backend/pwm.hpp"

namespace gzn::audio {

namespace {

inline constexpr auto TAG{ "gzn::audio::pwm" };

#define PWMA_CHECK(a, str, ret_val)                               \
	if (!(a)) [[unlikely]] {                                      \
		ESP_LOGE(TAG, "%s(%d): %s", __FUNCTION__, __LINE__, str); \
		return (ret_val);                                         \
	}

inline constexpr auto PARAM_ADDR_ERROR{ "PWM AUDIO PARAM ADDR ERROR" };
inline constexpr auto FRAMERATE_ERROR { "PWM AUDIO FRAMERATE ERROR" };
inline constexpr auto STATUS_ERROR    { "PWM AUDIO STATUS ERROR" };
inline constexpr auto ALLOC_ERROR     { "PWM AUDIO ALLOC ERROR" };
inline constexpr auto RESOLUTION_ERROR{ "PWM AUDIO RESOLUTION ERROR" };
inline constexpr auto NOT_INITIALIZED { "PWM AUDIO Uninitialized" };
inline constexpr auto TG_NUM_ERROR    { "PWM AUDIO TIMER GROUP NUMBER ERROR" };
inline constexpr auto TIMER_NUM_ERROR { "PWM AUDIO TIMER NUMBER ERROR" };



inline constexpr uint8_t    SPEAKER_PIN      {   41 };
inline constexpr uint32_t   RINGBUFFER_LENGTH{ 4096 };
inline constexpr TickType_t SEND_TICKS{ portMAX_DELAY };

inline constexpr uint32_t BUFFER_MIN_SIZE    {  256 };
inline constexpr uint32_t CHANNEL_LEFT_MASK  { 0x01 };
inline constexpr uint32_t CHANNEL_RIGHT_MASK { 0x02 };

inline constexpr uint32_t TIMER_RESOLUTION{ 80000000 / 16 };
inline constexpr uint32_t FREQUENCY{
	APB_CLK_FREQ / static_cast<uint32_t>(1 << DUTY_RESOLUTION)
};

inline constexpr int8_t  SHIFT{ BITS_PER_SAMPLE / DUTY_RESOLUTION };
inline constexpr int32_t NORMALIZE{ 0x7FFF };

portMUX_TYPE ringbuf_crit portMUX_INITIALIZER_UNLOCKED;

using volumes_array = std::array<uint16_t, MAX_SOUND_TRACKS>;

struct static_ringbuffer {
	RingbufHandle_t     handle{};
	StaticRingbuffer_t *info{};
	uint8_t            *data{};
};

struct data_t {
	static_ringbuffer   ringbuf_info{};
	ledc_timer_config_t ledc_timer{};      ///< ledc timer config
	gptimer_handle_t    gptimer{};
	uint32_t            framerate{};       ///< frame rates in Hz

	volumes_array       volumes{};
	int16_t             gpio{};
	ledc_channel_t      channel{};
	status_t            status{ status_t::un_init };
};

data_t *g_pwm_audio_handle{};


/**< ledc some register pointer */
volatile uint32_t *g_ledc_conf0_val { nullptr };
volatile uint32_t *g_ledc_conf1_val { nullptr };
volatile uint32_t *g_ledc_duty_val  { nullptr };

[[gnu::always_inline]]
inline void load_registers(const size_t speed_mode, const size_t channel_id) {
	auto &channel_group{ LEDC.channel_group[speed_mode] };
	auto &channel{ channel_group.channel[channel_id] };

#if defined(CONFIG_IDF_TARGET_ESP32P4) || defined(CONFIG_IDF_TARGET_ESP32C5) || defined(CONFIG_IDF_TARGET_ESP32C61)
	g_ledc_left_duty_val = &channel.duty_init.val;
#else
	g_ledc_duty_val = &channel.duty.val;
#endif // defined(CONFIG_IDF_TARGET_ESP32P4) || defined(CONFIG_IDF_TARGET_ESP32C5) || defined(CONFIG_IDF_TARGET_ESP32C61)

	g_ledc_conf0_val = &channel.conf0.val;
	g_ledc_conf1_val = &channel.conf1.val;
}

/*
 * Note:
 * In order to improve efficiency, register is operated directly
 */
[[gnu::always_inline]]
inline void ledc_set_duty_fast(const uint32_t duty_val) {
	*g_ledc_duty_val = (duty_val) << 4; /* Discard decimal part */
	*g_ledc_conf0_val |= 0x00000014;
	*g_ledc_conf1_val |= 0x80000000;
}

bool IRAM_ATTR timer_group_isr(
	gptimer_handle_t, const gptimer_alarm_event_data_t *, void *
) {
	auto rb{ g_pwm_audio_handle->ringbuf_info.handle };
	size_t received{};
	const auto item{ xRingbufferReceiveUpToFromISR(rb, &received, BYTES_PER_SAMPLE) };
	if (item == nullptr) {
		ledc_set_duty_fast(0u);
		return true;
	}
	/// @todo What if we received less amount?

	ledc_set_duty_fast(*reinterpret_cast<int16_t*>(item));

	auto higher_priority_task_woken{ pdFALSE };
	vRingbufferReturnItemFromISR(rb, item, &higher_priority_task_woken);
	if (pdTRUE == higher_priority_task_woken) {
		portYIELD_FROM_ISR();
	}
	return true;
}

esp_err_t set_sample_rate(const sample_rate rate) {
	auto handle{ g_pwm_audio_handle };
	PWMA_CHECK(nullptr != handle, NOT_INITIALIZED, ESP_ERR_INVALID_STATE);
	PWMA_CHECK(handle->status != status_t::busy, STATUS_ERROR, ESP_ERR_INVALID_ARG);

	handle->framerate = std::clamp(
		std::to_underlying(rate),
		std::to_underlying(sample_rate::SR_MIN),
		std::to_underlying(sample_rate::SR_MAX)
	);

	const gptimer_alarm_config_t alarm_config{
		.alarm_count{ TIMER_RESOLUTION / handle->framerate },
		.reload_count{},
		.flags{ .auto_reload_on_alarm{ true } }, // enable auto-reload
	};
	const auto res{ gptimer_set_alarm_action(handle->gptimer, &alarm_config) };
	PWMA_CHECK(ESP_OK == res, "pwm_audio set sample rate failed", res);
	return res;
}

} // namespace



auto pwm::startup() -> startup_result {

	auto ringbuffer_storage{ static_cast<uint8_t *>(heap_caps_malloc(
		RINGBUFFER_LENGTH,
		MALLOC_CAP_8BIT | MALLOC_CAP_SIMD | MALLOC_CAP_CACHE_ALIGNED | MALLOC_CAP_INTERNAL
	)) };
	PWMA_CHECK(ringbuffer_storage != nullptr, "Cannot allocate space for ringbuffer",
		startup_result::not_enough_memory
	);

	auto ringbuffer_info{ static_cast<StaticRingbuffer_t *>(heap_caps_malloc(
		sizeof(StaticRingbuffer_t), MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL
	)) };
	PWMA_CHECK(ringbuffer_info != nullptr, ALLOC_ERROR,
		startup_result::not_enough_memory
	);

	portENTER_CRITICAL(&ringbuf_crit);
	auto ringbuf_handle{ xRingbufferCreateStatic(
		RINGBUFFER_LENGTH, RINGBUF_TYPE_BYTEBUF,
		ringbuffer_storage, ringbuffer_info
	) };
	portEXIT_CRITICAL(&ringbuf_crit);

	auto handle_storage{ heap_caps_malloc(
		sizeof(data_t), MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL
	) };
	auto handle{ new (handle_storage) data_t{
		.ringbuf_info{
			.handle{ ringbuf_handle },
			.info{ ringbuffer_info },
			.data{ ringbuffer_storage }
		},
		.ledc_timer{
			.duty_resolution{ DUTY_RESOLUTION },
			.timer_num      { TIMER_ID },
			.freq_hz        { FREQUENCY - (FREQUENCY % 1000) }, // fixed PWM frequency, It's a multiple of 1000
#if defined(CONFIG_IDF_TARGET_ESP32S2)
			.clk_cfg        { LEDC_USE_APB_CLK },
#endif // defined(CONFIG_IDF_TARGET_ESP32S2)
		},
		.gpio{ SPEAKER_PIN },
		.channel{ LEDC_CHANNEL_0 },
	} };

	g_pwm_audio_handle = handle;

	PWMA_CHECK(ESP_OK == ledc_timer_config(&handle->ledc_timer),
		"LEDC timer configuration failed", startup_result::invalid_arguments
	);


	/**
	 * config ledc to generate pwm
	 */
	const auto gpio_num{ handle->gpio };
	if (gpio_num < GPIO_NUM_0 || gpio_num > GPIO_NUM_MAX) {
		return startup_result::invalid_arguments;
	}

	const ledc_channel_config_t channel_config{
		.gpio_num  { static_cast<int>(gpio_num) },
		.speed_mode{ handle->ledc_timer.speed_mode },
		.channel   { handle->channel },
		.intr_type { LEDC_INTR_DISABLE },
		.timer_sel { TIMER_ID }
	};
	PWMA_CHECK(ESP_OK == ledc_channel_config(&channel_config),
		"LEDC channel left configuration failed",
		startup_result::invalid_arguments
	);

	/**
	 * Get the address of LEDC register to reduce the addressing time
	 */
	load_registers(handle->ledc_timer.speed_mode, handle->channel);

	const gptimer_config_t timer_config{
		.clk_src      { GPTIMER_CLK_SRC_DEFAULT },
		.direction    { GPTIMER_COUNT_UP },
		.resolution_hz{ TIMER_RESOLUTION },
	};
	const gptimer_event_callbacks_t cbs{
		.on_alarm{ timer_group_isr },
	};

	auto res{ gptimer_new_timer(&timer_config, &handle->gptimer) };
	// PWMA_CHECK(ESP_OK == res, "gptimer configuration failed", res);

	res = gptimer_register_event_callbacks(handle->gptimer, &cbs, nullptr);
	// PWMA_CHECK(ESP_OK == res, "gptimer register event callback failed", res);

	for (size_t i{}; i < std::size(handle->volumes); ++i) {
		set_track_volume(i, 0);
	}

	/**< set a initial parameter */
	res = set_sample_rate(SAMPLE_RATE);
	// PWMA_CHECK(ESP_OK == res, "Set parameter failed", ESP_FAIL);

	handle->status = status_t::idle;
	start();

	return startup_result::ok;
}

void pwm::shutdown() {
	auto handle{ g_pwm_audio_handle };
	// PWMA_CHECK(handle != nullptr, PARAM_ADDR_ERROR, ESP_FAIL);
	handle->status = status_t::un_init;

	const auto res{ gptimer_del_timer(handle->gptimer) };
	// PWMA_CHECK(ESP_OK == res, "gptimer del failed", res);

	const auto gpio_num{ handle->gpio };
	if (gpio_num < GPIO_NUM_0 || gpio_num > GPIO_NUM_MAX) {
		ledc_stop(handle->ledc_timer.speed_mode, handle->channel, 0);
		gpio_set_direction(static_cast<gpio_num_t>(gpio_num), GPIO_MODE_INPUT);
	}

	portENTER_CRITICAL(&ringbuf_crit);
	vRingbufferDelete(handle->ringbuf_info.handle);
	portEXIT_CRITICAL(&ringbuf_crit);
	heap_caps_free(handle->ringbuf_info.info);
	heap_caps_free(handle->ringbuf_info.data);
	heap_caps_free(std::exchange(g_pwm_audio_handle, nullptr));
}

void pwm::start() {
	auto handle{ g_pwm_audio_handle };
	// PWMA_CHECK(nullptr != handle, NOT_INITIALIZED, ESP_ERR_INVALID_STATE);
	// PWMA_CHECK(handle->status == status_t::idle, STATUS_ERROR, ESP_ERR_INVALID_STATE);
	handle->status = status_t::busy;

	auto res{ gptimer_enable(handle->gptimer) };
	// PWMA_CHECK(ESP_OK == res, "gptimer enable fail", res);

	res = gptimer_start(handle->gptimer);
	// PWMA_CHECK(ESP_OK == res, "gptimer start fail", res);
}

void pwm::stop() {

	auto handle{ g_pwm_audio_handle };
	// PWMA_CHECK(nullptr != handle, NOT_INITIALIZED, ESP_ERR_INVALID_STATE);
	// PWMA_CHECK(handle->status == status_t::busy, STATUS_ERROR, ESP_ERR_INVALID_STATE);

	// just disable timer ,keep pwm output to reduce switching nosie timer
	// disable interrupt
	auto res{ gptimer_stop(handle->gptimer) };
	// PWMA_CHECK(ESP_OK == res, "gptimer stop failed", res);

	res = gptimer_disable(handle->gptimer);
	// PWMA_CHECK(ESP_OK == res, "gptimer disable failed", res);


	// Flushing buffer
	UBaseType_t items;
	auto &rb{ handle->ringbuf_info.handle };
	vRingbufferGetInfo(rb, nullptr, nullptr, nullptr, nullptr, &items);

	size_t len{};
	portENTER_CRITICAL(&ringbuf_crit);
	while (items != 0) {
		auto item{ xRingbufferReceive(rb, &len, 0) };
		items -= len;
		vRingbufferReturnItem(rb, item);
	}
	portEXIT_CRITICAL(&ringbuf_crit);

	handle->status = status_t::idle;
}

[[gnu::always_inline]] inline
auto pwm::status() -> status_t {
	return g_pwm_audio_handle->status;
}

void pwm::set_track_volume(const track_id track, const uint8_t value) {
	if (track >= MAX_SOUND_TRACKS) {
		return;
	}

	const auto clamped_volume{ std::clamp<int16_t>(value, -VOLUME_0DB, VOLUME_0DB) };
	g_pwm_audio_handle->volumes[track] = (clamped_volume + VOLUME_0DB) / VOLUME_0DB;
}

void pwm::send_sample(std::span<const int16_t> samples) {
	auto &rb{ g_pwm_audio_handle->ringbuf_info.handle };

	const auto volume{ g_pwm_audio_handle->volumes.front() };
	for (size_t i{}; i < std::size(samples); ++i) {
		const auto value{ static_cast<uint16_t>(
			(samples[i] * volume + NORMALIZE) >> SHIFT
		) };

		xRingbufferSend(rb, &value, sizeof(value), SEND_TICKS);
	}
}

void pwm::send_samples(
	const std::array<samples_array, MAX_SOUND_TRACKS> &samples_batch,
	const std::array<size_t,        MAX_SOUND_TRACKS> &batch_sizes,
	const size_t max_batch_size,
	const uint16_t active_batches_count
) {
	const auto &volumes{ g_pwm_audio_handle->volumes };
	auto &rb{ g_pwm_audio_handle->ringbuf_info.handle };

	uint16_t value;
	int16_t mixed_sample{};
	for (size_t i{}; i < max_batch_size; ++i) {
		for (size_t batch_id{}; batch_id < std::size(samples_batch); ++batch_id) {
			if (i >= batch_sizes[batch_id]) {
				continue;
			}

			mixed_sample += (samples_batch[batch_id][i] / active_batches_count)
				* volumes[batch_id];
		}

		value = static_cast<uint16_t>((mixed_sample + NORMALIZE) >> SHIFT);
		xRingbufferSend(rb, &value, sizeof(value), SEND_TICKS);
	}
}

} // namespace gzn::audio

