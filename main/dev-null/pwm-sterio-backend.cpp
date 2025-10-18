#include <array>
#include <utility>
#include <cassert>
#include <cstring>
#include <algorithm>

#include <esp_log.h>
#include <driver/ledc.h>
#include <driver/timer.h>
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

enum channel_id : size_t {
	CHANNEL_LEFT_INDEX,
	CHANNEL_RIGHT_INDEX,
	MAX_CHANNELS_COUNT
};

struct static_ringbuffer {
	RingbufHandle_t     handle{};
	StaticRingbuffer_t *info{};
	uint8_t            *data{};
};

struct data_t {
	std::array<int16_t,        MAX_CHANNELS_COUNT> gpios{};
	std::array<ledc_channel_t, MAX_CHANNELS_COUNT> channels{};
	std::array<int16_t,        MAX_SOUND_TRACKS>   volumes{};
	timer_group_t    tg_num        { TIMER_GROUP_0 };     ///< timer group number (0 - 1)
	timer_idx_t      timer_num     { TIMER_0 };           ///< timer number  (0 - 1)

	static_ringbuffer     ringbuf_info{};
	ledc_timer_config_t   ledc_timer{};      ///< ledc timer config
	gptimer_handle_t      gptimer{};
	uint32_t              framerate{};       ///< frame rates in Hz
	uint32_t              channel_mask{};    ///< channel gpio mask
	uint8_t               channel_set_num{}; ///< channel audio set number
	int8_t                volume{};          ///< the volume(-VOLUME_0DB ~ VOLUME_0DB)
	status_t              status{ status_t::un_init };
};

data_t *g_pwm_audio_handle{};


/**< ledc some register pointer */
volatile uint32_t *g_ledc_left_conf0_val { nullptr };
volatile uint32_t *g_ledc_left_conf1_val { nullptr };
volatile uint32_t *g_ledc_left_duty_val  { nullptr };
volatile uint32_t *g_ledc_right_conf0_val{ nullptr };
volatile uint32_t *g_ledc_right_conf1_val{ nullptr };
volatile uint32_t *g_ledc_right_duty_val { nullptr };

[[gnu::always_inline]]
inline void load_registers(
	const size_t speed_mode,
	const size_t left_channel_id,
	const size_t right_channel_id
) {
	auto &channel_group{ LEDC.channel_group[speed_mode] };

	auto &left_channel{ channel_group.channel[left_channel_id] };

#if defined(CONFIG_IDF_TARGET_ESP32P4) || defined(CONFIG_IDF_TARGET_ESP32C5) || defined(CONFIG_IDF_TARGET_ESP32C61)
	g_ledc_left_duty_val = &left_channel.duty_init.val;
#else
	g_ledc_left_duty_val = &left_channel.duty.val;
#endif // defined(CONFIG_IDF_TARGET_ESP32P4) || defined(CONFIG_IDF_TARGET_ESP32C5) || defined(CONFIG_IDF_TARGET_ESP32C61)

	g_ledc_left_conf0_val = &left_channel.conf0.val;
	g_ledc_left_conf1_val = &left_channel.conf1.val;


	auto &right_channel{ channel_group.channel[right_channel_id] };
#if defined(CONFIG_IDF_TARGET_ESP32P4) || defined(CONFIG_IDF_TARGET_ESP32C5) || defined(CONFIG_IDF_TARGET_ESP32C61)
	g_ledc_right_duty_val = &right_channel.duty_init.val;
#else
	g_ledc_right_duty_val = &right_channel.duty.val;
#endif // defined(CONFIG_IDF_TARGET_ESP32P4) || defined(CONFIG_IDF_TARGET_ESP32C5) || defined(CONFIG_IDF_TARGET_ESP32C61)

	g_ledc_right_conf0_val = &right_channel.conf0.val;
	g_ledc_right_conf1_val = &right_channel.conf1.val;
}

/*
 * Note:
 * In order to improve efficiency, register is operated directly
 */
[[gnu::always_inline]]
inline void ledc_set_left_duty_fast(const uint32_t duty_val) {
	*g_ledc_left_duty_val = (duty_val) << 4; /* Discard decimal part */
	*g_ledc_left_conf0_val |= 0x00000014;
	*g_ledc_left_conf1_val |= 0x80000000;
}

[[gnu::always_inline]]
inline void ledc_set_right_duty_fast(const uint32_t duty_val) {
	*g_ledc_right_duty_val = (duty_val) << 4; /* Discard decimal part */
	*g_ledc_right_conf0_val |= 0x00000014;
	*g_ledc_right_conf1_val |= 0x80000000;
}

bool IRAM_ATTR timer_group_isr(
	gptimer_handle_t timer,
	const gptimer_alarm_event_data_t *edata,
	void *user_ctx
) {
	auto handle{ g_pwm_audio_handle };
	if (handle == nullptr) {
		return false;
	}

	constexpr size_t max_bytes_to_recieve{ sizeof(int16_t) * MAX_CHANNELS_COUNT };
	const size_t bytes_to_recieve{ handle->channel_set_num *
		static_cast<size_t>(BITS_PER_SAMPLE >> 3 /* divide by 8 bits*/)
	};

	std::array<uint8_t, max_bytes_to_recieve> buffer;

	auto rb{ handle->ringbuf_info.handle };
	size_t received{};
	auto higher_priority_task_woken{ pdFALSE };

	const auto item{ xRingbufferReceiveUpToFromISR(rb, &received, bytes_to_recieve) };
	if (item == nullptr) {
		return true;
	}

	std::memcpy(std::data(buffer), item, received);
	vRingbufferReturnItemFromISR(rb, item, &higher_priority_task_woken);

	const auto data{ std::data(buffer) };


	uint32_t duty{};

	if (handle->channel_mask & CHANNEL_LEFT_MASK) {
		if (received == 4) {
			duty = reinterpret_cast<const int16_t *>(data)[0];
		} else if (received == 2) {
			duty = data[0];
		}
		ledc_set_left_duty_fast(duty);
	}

	if (handle->channel_mask & CHANNEL_RIGHT_MASK) {
		if (handle->channel_set_num != 1) {
			if (received == 4) {
				duty = reinterpret_cast<const int16_t *>(data)[1];
			} else if (received == 2) {
				duty = data[1];
			}
		}
		ledc_set_right_duty_fast(duty);
	}

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


esp_err_t set_param(
	const sample_rate rate,
	const uint8_t channels_count
) {
	if (const auto err{ set_sample_rate(rate) }; ESP_OK != err) {
		return err;
	}

	PWMA_CHECK(channels_count <= 2 && channels_count >= 1,
		"Unsupported channel number, only support mono and stereo",
		ESP_ERR_INVALID_ARG
	);

	auto handle{ g_pwm_audio_handle };
	handle->channel_set_num = channels_count;
	return ESP_OK;
}

esp_err_t get_param(sample_rate *rate, int *channel) {
	auto handle{ g_pwm_audio_handle };
	PWMA_CHECK(nullptr != handle, NOT_INITIALIZED, ESP_ERR_INVALID_STATE);

	if (nullptr != rate) {
		*rate = sample_rate{ handle->framerate };
	}
	if (nullptr != channel) {
		*channel = handle->channel_set_num;
	}

	return ESP_OK;
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
		.gpios{ SPEAKER_PIN, 42 },
		.channels{ LEDC_CHANNEL_0, LEDC_CHANNEL_1 },
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
		}
	} };

	g_pwm_audio_handle = handle;

	PWMA_CHECK(ESP_OK == ledc_timer_config(&handle->ledc_timer),
		"LEDC timer configuration failed", startup_result::invalid_arguments
	);


	/**
	 * config ledc to generate pwm
	 */
	for (size_t ch_id{}; ch_id < MAX_CHANNELS_COUNT; ++ch_id) {
		const auto gpio_num{ handle->gpios[ch_id] };
		if (gpio_num < GPIO_NUM_0 || gpio_num > GPIO_NUM_MAX) {
			continue;
		}

		const ledc_channel_config_t channel_config{
			.gpio_num  { static_cast<int>(gpio_num) },
			.speed_mode{ handle->ledc_timer.speed_mode },
			.channel   { handle->channels[ch_id] },
			.intr_type { LEDC_INTR_DISABLE },
			.timer_sel { TIMER_ID }
		};
		PWMA_CHECK(ESP_OK == ledc_channel_config(&channel_config),
			"LEDC channel left configuration failed",
			startup_result::invalid_arguments
		);
		handle->channel_mask |= (1 << ch_id);
		++handle->channel_set_num;
	}

	PWMA_CHECK(0 != handle->channel_mask,
		"Assign at least one channel gpio",
		startup_result::invalid_arguments
	);

	/**
	 * Get the address of LEDC register to reduce the addressing time
	 */
	load_registers(
		handle->ledc_timer.speed_mode,
		handle->channels[CHANNEL_LEFT_INDEX],
		handle->channels[CHANNEL_RIGHT_INDEX]
	);

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

	handle->volumes.fill(8);

	/**< set a initial parameter */
	res = set_param(SAMPLE_RATE, MAX_CHANNELS_COUNT);
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

	for (size_t i{}; i < MAX_CHANNELS_COUNT; ++i) {
		const auto gpio_num{ handle->gpios[i] };
		if (gpio_num < GPIO_NUM_0 || gpio_num > GPIO_NUM_MAX) {
			ledc_stop(handle->ledc_timer.speed_mode, handle->channels[i], 0);
		}
	}

	for (size_t i{}; i < MAX_CHANNELS_COUNT; ++i) {
		const auto gpio_num{ handle->gpios[i] };
		if (gpio_num < GPIO_NUM_0 || gpio_num > GPIO_NUM_MAX) {
			gpio_set_direction(static_cast<gpio_num_t>(gpio_num), GPIO_MODE_INPUT);
		}
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

void pwm::set_track_volume(const track_id track, const uint8_t value) {
	if (track > MAX_SOUND_TRACKS) {
		return;
	}

	g_pwm_audio_handle->volumes[track] = VOLUME_0DB + std::clamp<int16_t>(
		value, -VOLUME_0DB, VOLUME_0DB
	);
}

void pwm::send_sample(std::span<const int16_t> samples) {
	auto &rb{ g_pwm_audio_handle->ringbuf_info.handle };
	const auto volume{ g_pwm_audio_handle->volumes.front() };

	uint16_t value[2];
	for (size_t i{}; i < std::size(samples); i += 2) {
		value[0] = static_cast<uint16_t>((samples[i + 0] * volume + NORMALIZE) >> SHIFT);
		value[1] = static_cast<uint16_t>((samples[i + 1] * volume + NORMALIZE) >> SHIFT);
		xRingbufferSend(rb, &value, sizeof(value), SEND_TICKS);
	}
}

void pwm::send_samples(
	const std::array<samples_array, MAX_SOUND_TRACKS> &samples_batch,
	const std::array<size_t,        MAX_SOUND_TRACKS> &batch_sizes,
	const size_t max_batch_size,
	const size_t active_batches_count
) {
	const auto &volumes{ g_pwm_audio_handle->volumes };
	auto &rb{ g_pwm_audio_handle->ringbuf_info.handle };

	uint16_t value[2];
	int16_t mixed_sample[2]{};
	for (size_t i{}; i < max_batch_size; i += 2) {
		for (size_t batch_id{}; batch_id < std::size(samples_batch); ++batch_id) {
			if (i >= batch_sizes[batch_id]) {
				continue;
			}

			mixed_sample[0] += (samples_batch[batch_id][i + 0] / active_batches_count) * volumes[batch_id];
			mixed_sample[1] += (samples_batch[batch_id][i + 1] / active_batches_count) * volumes[batch_id];
		}

		value[0] = static_cast<uint16_t>((mixed_sample[0] + NORMALIZE) >> SHIFT);
		value[1] = static_cast<uint16_t>((mixed_sample[1] + NORMALIZE) >> SHIFT);
		xRingbufferSend(rb, &value, sizeof(value), SEND_TICKS);
	}
}

} // namespace gzn::audio

