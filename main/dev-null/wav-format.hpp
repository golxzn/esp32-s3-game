#pragma once

#include <span>
#include <utility>
#include <cstdint>

#include <cstdio>

namespace wav {

using char4_t = char[4];

enum class format_t : uint16_t {
	PCM        = 0x0001,
	IEEEFloat  = 0x0003,
	ALaw       = 0x0006,
	MULaw      = 0x0007,
	Extensible = 0xFFFE
};
constexpr const char *to_str(const format_t format) {
	switch (format) {
		case format_t::PCM       : return "PCM";
		case format_t::IEEEFloat : return "IEEEFloat";
		case format_t::ALaw      : return "ALaw";
		case format_t::MULaw     : return "MULaw";
		case format_t::Extensible: return "Extensible";
		default: break;
	}
	return "unknown";
}

struct [[gnu::packed]] header {
	struct {
		char4_t  id    { 'R', 'I', 'F', 'F' };
		uint32_t size  {};
		char4_t  format{ 'W', 'A', 'V', 'E' };
	} descriptor{};

	struct {
		char4_t  id         { 'f', 'm', 't', ' ' };
		uint32_t size       { 16 };
		format_t format     { format_t::PCM };
		uint16_t channels   { 2 };
		uint32_t sample_rate{};
		uint32_t byte_rate  {};
		uint16_t block_align{};
		uint16_t bits_per_sample{};
	} format{};

	struct {
		char4_t  id{ 'd', 'a', 't', 'a' };
		uint32_t size{};
		int16_t  bytes[0];
	} data{};

	[[nodiscard]] [[gnu::always_inline]]
	inline static auto make_view(const std::span<const uint8_t> data) -> const header *{
		if (std::size(data) >= sizeof(header)) {
			return reinterpret_cast<const header *>(std::data(data));
		}
		return nullptr;
	}

	void dump() const {
		std::printf("----------- WAV INFO -----------\n");
		std::printf("descriptor:\n");
		std::printf("  id.............: %.4s\n", descriptor.id);
		std::printf("  size...........: %lu\n",  descriptor.size);
		std::printf("  format.........: %.4s\n", descriptor.format);
		std::printf("format:\n");
		std::printf("  id.............: %.4s\n", format.id);
		std::printf("  size...........: %lu\n",  format.size);
		std::printf("  format.........: %s\n",   to_str(format.format));
		std::printf("  channels.......: %u\n",   format.channels);
		std::printf("  sample_rate....: %lu\n",  format.sample_rate);
		std::printf("  byte_rate......: %lu\n",  format.byte_rate);
		std::printf("  block_align....: %u\n",   format.block_align);
		std::printf("  bits_per_sample: %u\n",   format.bits_per_sample);
		std::printf("data:\n");
		std::printf("  id.............: %.4s\n", data.id);
		std::printf("  size...........: %lu\n",  data.size);
		std::printf("--------------------------------\n");
	}
};

} // namespace wav

