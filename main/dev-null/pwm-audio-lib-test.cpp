#include <array>
#include <limits>
#include <utility>

#include <driver/ledc.h>
#include <driver/gpio.h>
#include <soc/gpio_sig_map.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <pwm_audio.h>

#include "sound.hpp"
#include "wav-format.hpp"

#include "gzn/utils.hpp"

constexpr uint8_t speaker_pin{ 1 };

void play_wav_file();

void speaker_test() {
	const pwm_audio_config_t config{
		.gpio_num_left      = speaker_pin,
		.gpio_num_right     = speaker_pin,
		.ledc_channel_left  = LEDC_CHANNEL_0,
		.ledc_channel_right = LEDC_CHANNEL_1,
		.ledc_timer_sel     = LEDC_TIMER_0,
		.duty_resolution    = LEDC_TIMER_10_BIT,
		.ringbuf_len        = 1024 * 8
	};
	ESP_ERROR_CHECK(pwm_audio_init(&config));

	play_wav_file();
}


void send_bits_16(const wav::header &header) {
	const uint32_t samples_count{
		header.data.size / header.format.channels / (header.format.bits_per_sample >> 3)
	};
	std::printf("Samples count: %lu\n", samples_count);

	ESP_ERROR_CHECK(pwm_audio_set_param(
		header.format.sample_rate,
		static_cast<ledc_timer_bit_t>(header.format.bits_per_sample),
		header.format.channels
	));
	ESP_ERROR_CHECK(pwm_audio_start());

	constexpr size_t chunk_size{ 4096 };
	using namespace gzn::utils::literals;


	auto data_view{ reinterpret_cast<const uint8_t *>(header.data.bytes) };
	size_t count{};
	for (size_t offset{}; offset <= header.data.size; offset += chunk_size) {
		ESP_ERROR_CHECK(pwm_audio_write(
			const_cast<uint8_t *>(data_view + offset),
			chunk_size,
			&count,
			1000_ms
		));
	}

	pwm_audio_stop();

	// if (header.format.channels == 1) {
	// 	for (uint32_t sample{}; sample < samples_count; ++sample) {
	// 		std::printf("Sending frequency: %u\n", *data_view + std::numeric_limits<int16_t>::max());
	// 		tone.frequency = *data_view;
	// 		xQueueSend(ctx.tone_queue, &tone, portMAX_DELAY);
	// 		++data_view;
	// 	}
	// 	return;
	// }
	//
	// for (uint32_t sample{}; sample < samples_count; ++sample) {
	// 	for (uint16_t channel{}; channel < header.format.channels; ++channel) {
	// 		std::printf("Switching channel: %u\n", channel);
	// 		swap_channel.data.channel = channel;
	// 		xQueueSend(ctx.message_queue, &swap_channel, portMAX_DELAY);
	//
	// 		std::printf("Sending frequency: %u\n", *data_view);
	// 		tone.frequency = *data_view;
	// 		xQueueSend(ctx.tone_queue, &tone, portMAX_DELAY);
	// 		++data_view;
	// 	}
	// }
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

void play_wav_file() {
	auto header{ reinterpret_cast<const wav::header *>(std::data(__sound_wav)) };

	header->dump();

	switch (header->format.bits_per_sample) {
		case 8 : break;
		case 16: send_bits_16(*header); break;
		case 32: break;


		default: break;
	};

}

