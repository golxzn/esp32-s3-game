#include <array>
#include <limits>
#include <utility>

#include <driver/ledc.h>
#include <driver/gpio.h>
#include <soc/gpio_sig_map.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "gzn/utils.hpp"

constexpr uint8_t speaker_pin{ 8 };

namespace {

constexpr soc_periph_ledc_clk_src_legacy_t ledc_default_clk{
#if defined(SOC_LEDC_SUPPORT_XTAL_CLOCK)
	LEDC_USE_XTAL_CLK
#else
	LEDC_AUTO_CLK
#endif // defined(SOC_LEDC_SUPPORT_XTAL_CLOCK)
};

constexpr size_t ledc_channels{
#if defined(SOC_LEDC_SUPPORT_HS_MODE)
	(SOC_LEDC_CHANNEL_NUM << 1)
#else
	(SOC_LEDC_CHANNEL_NUM)
#endif // defined(SOC_LEDC_SUPPORT_HS_MODE)
};

constexpr BaseType_t tone_queue_length   { 128 };
constexpr BaseType_t message_queue_length{  32 };

std::array<uint8_t, ledc_channels> channels_resolution{};

} // namespace

struct [[gnu::packed]] tone_info {
	uint32_t duration{};
	uint32_t frequency{};
};

enum class message_type {
	invalid,
	interrupt,
	pin_changed,
	channel_changed,
};

struct message {
	message_type type{};
	union {
		uint8_t channel;
		uint8_t pin;
	} data;
};

struct player_context {
	QueueHandle_t tone_queue{};
	QueueHandle_t message_queue{};
	bool          running{ true };

	[[gnu::always_inline]]
	inline bool valid() const {
		return tone_queue != nullptr && message_queue != nullptr;
	}
};

struct ledc_info {
	uint8_t        hash{};
	ledc_mode_t    mode{};
	ledc_channel_t channel{};
	ledc_timer_t   timer{};

	[[gnu::always_inline]]
	inline static auto make(const uint8_t channel) -> ledc_info {
		return ledc_info{
			.hash    = channel,
			.mode    = static_cast<ledc_mode_t   >(channel >> 3),
			.channel = static_cast<ledc_channel_t>(channel & 0b111),
			.timer   = static_cast<ledc_timer_t  >((channel >> 1) & 0x11)
		};
	};
};

static void ledc_attach_pin(uint8_t pin, const ledc_info info);
static void ledc_detach_pin(uint8_t pin);
static auto ledc_write_tone(const ledc_info channel, uint32_t frequency) -> uint32_t;

void player_task(void *arg) {
	using namespace gzn::utils::literals;
	constexpr auto tone_timeout   { 20_ms };
	constexpr auto message_timeout{ 10_ms };

	tone_info tone{};
	ledc_info ledc{};
	message   msg {};
	uint8_t   pin {};

	auto ctx{ static_cast<player_context *>(arg) };

	while (ctx->running) {
		if (xQueueReceive(ctx->tone_queue, &tone, tone_timeout)) {
			ledc_attach_pin(pin, ledc);
			ledc_write_tone(ledc, tone.frequency);

			if (tone.duration != 0) {
				// gzn::utils::delay_microsecons(tone.duration);
				vTaskDelay(tone.duration / portTICK_PERIOD_MS);
				ledc_detach_pin(pin);
				ledc_write_tone(ledc, 0u);
			}
		}

		if (!xQueueReceive(ctx->message_queue, &msg, message_timeout)) {
			continue;
		}

		switch (msg.type) {
			case message_type::interrupt:
				ledc_detach_pin(pin);
				ledc_write_tone(ledc, 0u);
				break;

			case message_type::pin_changed:
				pin = msg.data.pin;
				break;

			case message_type::channel_changed:
				ledc = ledc_info::make(msg.data.channel);
				break;


			default: break;
		}
	}
}

static void ledc_attach_pin(const uint8_t pin, const ledc_info info) {
	const ledc_channel_config_t channel_config{
		.gpio_num   = pin,
		.speed_mode = info.mode,
		.channel    = info.channel,
		.intr_type  = LEDC_INTR_DISABLE,
		.timer_sel  = info.timer,
		.duty       = ledc_get_duty(info.mode, info.channel),
	};
	ledc_channel_config(&channel_config);
}

static void ledc_detach_pin(const uint8_t pin) {
	gpio_iomux_out(pin, SIG_GPIO_OUT_IDX, false);
}


static void ledc_write(const ledc_info info, const uint32_t duty) {
	if (info.hash >= std::size(channels_resolution)) { return; }

	const auto max_duty{ (1 << channels_resolution[info.hash]) - 1 };
	const auto target_duty{
		(duty == max_duty) && (max_duty != 1) ? max_duty + 1 : duty
	};

	ledc_set_duty(info.mode, info.channel, target_duty);
	ledc_update_duty(info.mode, info.channel);
}

static auto ledc_write_tone(const ledc_info info, const uint32_t frequency) -> uint32_t {
	if (frequency == 0) {
		ledc_write(info, 0);
		return 0u;
	}

	const ledc_timer_config_t config{
		.speed_mode      = info.mode,
		.duty_resolution = LEDC_TIMER_10_BIT,
		.timer_num       = info.timer,
		.freq_hz         = frequency,
		.clk_cfg         = ledc_default_clk
	};
	if (ESP_OK != ledc_timer_config(&config)) {
		return 0u;
	};
	channels_resolution[info.hash] = static_cast<uint8_t>(config.duty_resolution);

	const auto freq{ ledc_get_freq(info.mode, info.timer) };
	ledc_write(info, 0x1FF);
	return freq;
}

auto initialize_ledc() -> esp_err_t {
	// const gpio_config_t io_conf{
	// 	.pin_bit_mask = (1 << speaker_pin),
	// 	.mode         = GPIO_MODE_OUTPUT,
	// 	.pull_up_en   = GPIO_PULLUP_DISABLE,
	// 	.pull_down_en = GPIO_PULLDOWN_DISABLE,
	// 	.intr_type    = GPIO_INTR_DISABLE
	// };
	// if (const auto error{ gpio_config(&io_conf) }; error != ESP_OK) [[unlikely]] {
	// 	return error;
	// }

	const ledc_timer_config_t timer_cfg{
		.speed_mode      = LEDC_LOW_SPEED_MODE,
		.duty_resolution = LEDC_TIMER_10_BIT,
		.timer_num       = LEDC_TIMER_0,
		.freq_hz         = 10000,
		.clk_cfg         = ledc_default_clk
	};
	if (const auto error{ ledc_timer_config(&timer_cfg) }; error != ESP_OK) [[unlikely]] {
		return error;
	}

	const ledc_channel_config_t channel_cfg{
		.gpio_num   = speaker_pin,
		.speed_mode = timer_cfg.speed_mode,
		.channel    = LEDC_CHANNEL_0,
		.intr_type  = LEDC_INTR_DISABLE,
		.timer_sel  = timer_cfg.timer_num,
	};
	if (const auto error{ ledc_channel_config(&channel_cfg) }; error != ESP_OK) [[unlikely]] {
		return error;
	}

	return ESP_OK;
}

auto make_context() -> player_context {
	return player_context{
		.tone_queue    = xQueueCreate(tone_queue_length, sizeof(tone_info)),
		.message_queue = xQueueCreate(message_queue_length, sizeof(message))
	};
}

void destroy_context(player_context &ctx) {
	vQueueDelete(std::exchange(ctx.tone_queue, nullptr));
	vQueueDelete(std::exchange(ctx.message_queue, nullptr));
}

auto start_player_task(player_context &ctx) -> TaskHandle_t {
	TaskHandle_t handle{};
	xTaskCreate(player_task, "player_task", 3500, &ctx, 1, &handle);
	return handle;
}


// auto play_note(const uint8_t channel, const note n, const uint8_t octave) -> uint32_t {
// 	if (octave > 8) { return 0u; }
//
// 	return ledc_write_tone(ledc_info::make(channel),
// 		sfx{ .note = n, .octave = octave }.to_frequency()
// 	);
// };

void play_melody(player_context &ctx);
void play_wav_file(player_context &ctx);

player_context ctx{};

void speaker_test() {
	ESP_ERROR_CHECK(initialize_ledc());

	ctx = make_context();
	if (!ctx.valid()) {
		destroy_context(ctx);
		return;
	}

	const message msg{
		.type = message_type::pin_changed,
		.data = { .pin = speaker_pin }
	};
	xQueueSend(ctx.message_queue, &msg, portMAX_DELAY);

	start_player_task(ctx);

	play_wav_file(ctx);
	play_melody(ctx);

	using namespace gzn::utils::literals;
	// vTaskDelay(1000_s);

	// destroy_context(ctx);
}


enum class note_type : uint16_t {
	N  = 0,

	C  = 4186,
	Cs = 4435,
	D  = 4699,
	Eb = 4978,
	E  = 5274,
	F  = 5588,
	Fs = 5920,
	G  = 6272,
	Gs = 6645,
	A  = 7040,
	As = 7459,
	B  = 7902,
};

constexpr uint8_t max_octaves{ 8u };

struct sfx {
	note_type note{ note_type::C };
	uint8_t octave{ 4 };
	uint32_t duration{};

	[[gnu::always_inline]]
	inline auto to_frequency() const -> uint32_t {
		return static_cast<uint32_t>(note) / static_cast<uint32_t>(1 << (max_octaves - octave));
	};
};

void play_melody(player_context &ctx) {

	constexpr std::array melody{
		sfx{ .note = note_type::D,  .octave = 3, .duration = 32 },
		sfx{ .note = note_type::D,  .octave = 3, .duration = 32 },
		sfx{ .note = note_type::D,  .octave = 4, .duration = 16 },
		sfx{ .note = note_type::A,  .octave = 3, .duration = 16 },
		sfx{ .note = note_type::N,  .octave = 3, .duration = 32 },
		sfx{ .note = note_type::Gs, .octave = 3, .duration = 32 },
		sfx{ .note = note_type::N,  .octave = 3, .duration = 32 },
		sfx{ .note = note_type::G,  .octave = 3, .duration = 16 },
		sfx{ .note = note_type::F,  .octave = 3, .duration = 16 },
		sfx{ .note = note_type::D,  .octave = 3, .duration = 32 },
		sfx{ .note = note_type::F,  .octave = 3, .duration = 32 },
		sfx{ .note = note_type::G,  .octave = 3, .duration = 32 },

		sfx{ .note = note_type::C,  .octave = 3, .duration = 32 },
		sfx{ .note = note_type::C,  .octave = 3, .duration = 32 },
		sfx{ .note = note_type::D,  .octave = 4, .duration = 16 },
		sfx{ .note = note_type::A,  .octave = 3, .duration = 16 },
		sfx{ .note = note_type::N,  .octave = 3, .duration = 32 },
		sfx{ .note = note_type::Gs, .octave = 3, .duration = 32 },
		sfx{ .note = note_type::N,  .octave = 3, .duration = 32 },
		sfx{ .note = note_type::G,  .octave = 3, .duration = 16 },
		sfx{ .note = note_type::F,  .octave = 3, .duration = 16 },
		sfx{ .note = note_type::D,  .octave = 3, .duration = 32 },
		sfx{ .note = note_type::F,  .octave = 3, .duration = 32 },
		sfx{ .note = note_type::G,  .octave = 3, .duration = 32 },

		sfx{ .note = note_type::B,  .octave = 2, .duration = 32 },
		sfx{ .note = note_type::B,  .octave = 2, .duration = 32 },
		sfx{ .note = note_type::D,  .octave = 4, .duration = 16 },
		sfx{ .note = note_type::A,  .octave = 3, .duration = 16 },
		sfx{ .note = note_type::N,  .octave = 3, .duration = 32 },
		sfx{ .note = note_type::Gs, .octave = 3, .duration = 32 },
		sfx{ .note = note_type::N,  .octave = 3, .duration = 32 },
		sfx{ .note = note_type::G,  .octave = 3, .duration = 16 },
		sfx{ .note = note_type::F,  .octave = 3, .duration = 16 },
		sfx{ .note = note_type::D,  .octave = 3, .duration = 32 },
		sfx{ .note = note_type::F,  .octave = 3, .duration = 32 },
		sfx{ .note = note_type::G,  .octave = 3, .duration = 32 },

		sfx{ .note = note_type::As, .octave = 2, .duration = 32 },
		sfx{ .note = note_type::As, .octave = 2, .duration = 32 },
		sfx{ .note = note_type::D,  .octave = 4, .duration = 16 },
		sfx{ .note = note_type::A,  .octave = 3, .duration = 16 },
		sfx{ .note = note_type::N,  .octave = 3, .duration = 32 },
		sfx{ .note = note_type::Gs, .octave = 3, .duration = 32 },
		sfx{ .note = note_type::N,  .octave = 3, .duration = 32 },
		sfx{ .note = note_type::G,  .octave = 3, .duration = 16 },
		sfx{ .note = note_type::F,  .octave = 3, .duration = 16 },
		sfx{ .note = note_type::D,  .octave = 3, .duration = 32 },
		sfx{ .note = note_type::F,  .octave = 3, .duration = 32 },
		sfx{ .note = note_type::G,  .octave = 3, .duration = 32 },


		sfx{ .note = note_type::D,  .octave = 4, .duration = 32 },
		sfx{ .note = note_type::D,  .octave = 4, .duration = 32 },
		sfx{ .note = note_type::D,  .octave = 5, .duration = 16 },
		sfx{ .note = note_type::A,  .octave = 4, .duration = 16 },
		sfx{ .note = note_type::N,  .octave = 4, .duration = 32 },
		sfx{ .note = note_type::Gs, .octave = 4, .duration = 32 },
		sfx{ .note = note_type::N,  .octave = 4, .duration = 32 },
		sfx{ .note = note_type::G,  .octave = 4, .duration = 16 },
		sfx{ .note = note_type::F,  .octave = 4, .duration = 16 },
		sfx{ .note = note_type::D,  .octave = 4, .duration = 32 },
		sfx{ .note = note_type::F,  .octave = 4, .duration = 32 },
		sfx{ .note = note_type::G,  .octave = 4, .duration = 32 },

		sfx{ .note = note_type::C,  .octave = 4, .duration = 32 },
		sfx{ .note = note_type::C,  .octave = 4, .duration = 32 },
		sfx{ .note = note_type::D,  .octave = 5, .duration = 16 },
		sfx{ .note = note_type::A,  .octave = 4, .duration = 16 },
		sfx{ .note = note_type::N,  .octave = 4, .duration = 32 },
		sfx{ .note = note_type::Gs, .octave = 4, .duration = 32 },
		sfx{ .note = note_type::N,  .octave = 4, .duration = 32 },
		sfx{ .note = note_type::G,  .octave = 4, .duration = 16 },
		sfx{ .note = note_type::F,  .octave = 4, .duration = 16 },
		sfx{ .note = note_type::D,  .octave = 4, .duration = 32 },
		sfx{ .note = note_type::F,  .octave = 4, .duration = 32 },
		sfx{ .note = note_type::G,  .octave = 4, .duration = 32 },

		sfx{ .note = note_type::B,  .octave = 3, .duration = 32 },
		sfx{ .note = note_type::B,  .octave = 3, .duration = 32 },
		sfx{ .note = note_type::D,  .octave = 5, .duration = 16 },
		sfx{ .note = note_type::A,  .octave = 4, .duration = 16 },
		sfx{ .note = note_type::N,  .octave = 4, .duration = 32 },
		sfx{ .note = note_type::Gs, .octave = 4, .duration = 32 },
		sfx{ .note = note_type::N,  .octave = 4, .duration = 32 },
		sfx{ .note = note_type::G,  .octave = 4, .duration = 16 },
		sfx{ .note = note_type::F,  .octave = 4, .duration = 16 },
		sfx{ .note = note_type::D,  .octave = 4, .duration = 32 },
		sfx{ .note = note_type::F,  .octave = 4, .duration = 32 },
		sfx{ .note = note_type::G,  .octave = 4, .duration = 32 },

		sfx{ .note = note_type::As, .octave = 3, .duration = 32 },
		sfx{ .note = note_type::As, .octave = 3, .duration = 32 },
		sfx{ .note = note_type::D,  .octave = 5, .duration = 16 },
		sfx{ .note = note_type::A,  .octave = 4, .duration = 16 },
		sfx{ .note = note_type::N,  .octave = 4, .duration = 32 },
		sfx{ .note = note_type::Gs, .octave = 4, .duration = 32 },
		sfx{ .note = note_type::N,  .octave = 4, .duration = 32 },
		sfx{ .note = note_type::G,  .octave = 4, .duration = 16 },
		sfx{ .note = note_type::F,  .octave = 4, .duration = 16 },
		sfx{ .note = note_type::D,  .octave = 4, .duration = 32 },
		sfx{ .note = note_type::F,  .octave = 4, .duration = 32 },
		sfx{ .note = note_type::G,  .octave = 4, .duration = 32 },
	};

	for (const auto sound : melody) {
		// To calculate the note duration, take one second divided by the note type.
		const tone_info tone{
			.duration  = 5000u / sound.duration,
			.frequency = sound.to_frequency()
		};
		xQueueSend(ctx.tone_queue, &tone, portMAX_DELAY);
	}
}


#include "wav-format.hpp"
#include "sound.hpp"

void send_bits_8(player_context &ctx, const wav::header &header) {
	const uint32_t samples_count{
		header.data.size / header.format.channels / (header.format.bits_per_sample >> 3)
	};
	std::printf("Samples count: %lu\n", samples_count);

	message swap_channel{ .type = message_type::channel_changed };
	tone_info tone{ .duration = 1000 / header.format.sample_rate };

	auto data_view{ reinterpret_cast<const uint8_t *>(header.data.bytes) };
	if (header.format.channels == 1) {
		for (uint32_t sample{}; sample < samples_count; ++sample) {
			tone.frequency = *data_view;
			xQueueSend(ctx.tone_queue, &tone, portMAX_DELAY);
			++data_view;
		}
		return;
	}

	for (uint32_t sample{}; sample < samples_count; ++sample) {
		for (uint16_t channel{}; channel < header.format.channels; ++channel) {
			swap_channel.data.channel = channel;
			xQueueSend(ctx.message_queue, &swap_channel, portMAX_DELAY);

			tone.frequency = *data_view;
			xQueueSend(ctx.tone_queue, &tone, portMAX_DELAY);
			++data_view;
		}
	}
}

void send_bits_16(player_context &ctx, const wav::header &header) {
	const uint32_t samples_count{
		header.data.size / header.format.channels / (header.format.bits_per_sample >> 3)
	};
	std::printf("Samples count: %lu\n", samples_count);

	message swap_channel{ .type = message_type::channel_changed };
	// sample_rate = samples per second. So, for each sample
	// one second holds 44100 samples. One sample should be played for 0.0000226757 sec or 0.0226757370 ms or 22.6 micro what
	tone_info tone{ .duration = 0/*1'000'000 / header.format.sample_rate*/ };

	auto data_view{ reinterpret_cast<const uint8_t *>(header.data.bytes) };
	if (header.format.channels == 1) {
		for (uint32_t sample{}; sample < samples_count; ++sample) {
			std::printf("Sending frequency: %u\n", *data_view + std::numeric_limits<int16_t>::max());
			tone.frequency = *data_view;
			xQueueSend(ctx.tone_queue, &tone, portMAX_DELAY);
			++data_view;
		}
		return;
	}

	for (uint32_t sample{}; sample < samples_count; ++sample) {
		for (uint16_t channel{}; channel < header.format.channels; ++channel) {
			std::printf("Switching channel: %u\n", channel);
			swap_channel.data.channel = channel;
			xQueueSend(ctx.message_queue, &swap_channel, portMAX_DELAY);

			std::printf("Sending frequency: %u\n", *data_view);
			tone.frequency = *data_view;
			xQueueSend(ctx.tone_queue, &tone, portMAX_DELAY);
			++data_view;
		}
	}
}
/*
----------- WAV INFO -----------
descriptor:
  id.............: RIFF
  size...........: 18242
  format.........: WAVE
format:
  id.............: fmt
  size...........: 16
  format.........: PCM
  channels.......: 1
  sample_rate....: 44100
  byte_rate......: 88200
  block_align....: 2
  bits_per_sample: 16
data:
  id.............: data
  size...........: 18206
--------------------------------
*/

void play_wav_file(player_context &ctx) {
	auto header{ reinterpret_cast<const wav::header *>(std::data(__sound_wav)) };

	header->dump();

	switch (header->format.bits_per_sample) {
		case 8 : send_bits_8(ctx, *header); break;
		case 16: send_bits_16(ctx, *header); break;
		case 32:  break;


		default: break;
	};

}

