#pragma once

#include <array>
#include <cstdint>
#include <soc/gpio_num.h>

namespace gzn::tft::pins {

// Controlling pins
inline constexpr auto RESET{ GPIO_NUM_9  };
inline constexpr auto CS   { GPIO_NUM_10 }; // Chip select control pin
inline constexpr auto RS   { GPIO_NUM_11 }; // Data/Command (DC). When HIGH == data mode, LOW == command
inline constexpr auto WRITE{ GPIO_NUM_12 };
inline constexpr auto READ { GPIO_NUM_13 };

inline constexpr auto CD{ RS }; // alias

// Data pins
inline constexpr auto D2{ GPIO_NUM_4  };
inline constexpr auto D3{ GPIO_NUM_5  };
inline constexpr auto D4{ GPIO_NUM_6  };
inline constexpr auto D5{ GPIO_NUM_7  };
inline constexpr auto D6{ GPIO_NUM_15 };
inline constexpr auto D7{ GPIO_NUM_16 };

inline constexpr auto D0{ GPIO_NUM_17 };
inline constexpr auto D1{ GPIO_NUM_18 };

inline constexpr uint32_t DATA_PINS_MASK{ 0u
	| 1u << D0
	| 1u << D1
	| 1u << D2
	| 1u << D3
	| 1u << D4
	| 1u << D5
	| 1u << D6
	| 1u << D7
};

inline constexpr uint64_t PINS_MASK{
	static_cast<uint64_t>(DATA_PINS_MASK)
	| 1ull << READ
	| 1ull << WRITE
	| 1ull << RESET
	| 1ull << CS
	| 1ull << RS
};

#if !defined(GZN_TFT_USE_DEDICATED_GPIO)

/*
When we use GPIO.out_w1t[cs] to write to those pins faster, we should
calculate the mask to write. We could cache those calculation and just lookup
for the mask we deserve to use.
Here's the definitions showcase from TFT_eSPI library:

A lookup table is used to set the different bit patterns, this uses 1kByte of RAM
#define set_mask(C) xset_mask[C] // 63fps Sprite rendering test 33% faster, graphicstest only 1.8% faster than shifting in real time

Real-time shifting alternative to above to save 1KByte RAM, 47 fps Sprite rendering test
#define set_mask(C) (((C)&0x80)>>7)<<tft::pins::D7 | (((C)&0x40)>>6)<<tft::pins::D6 | (((C)&0x20)>>5)<<tft::pins::D5 | (((C)&0x10)>>4)<<tft::pins::D4 | \
                    (((C)&0x08)>>3)<<tft::pins::D3 | (((C)&0x04)>>2)<<tft::pins::D2 | (((C)&0x02)>>1)<<tft::pins::D1 | (((C)&0x01)>>0)<<tft::pins::D0

*/
consteval decltype(auto) make_cached_mask_lookup_table() {
	std::array<uint32_t, 256> mask{};
	for (uint8_t i{}; i != 0xFF; ++i) {
		if (i & 0x01) mask[i] |= 1 << (tft::pins::D0);
		if (i & 0x02) mask[i] |= 1 << (tft::pins::D1);
		if (i & 0x04) mask[i] |= 1 << (tft::pins::D2);
		if (i & 0x08) mask[i] |= 1 << (tft::pins::D3);
		if (i & 0x10) mask[i] |= 1 << (tft::pins::D4);
		if (i & 0x20) mask[i] |= 1 << (tft::pins::D5);
		if (i & 0x40) mask[i] |= 1 << (tft::pins::D6);
		if (i & 0x80) mask[i] |= 1 << (tft::pins::D7);
	}
	return mask;
}
inline constexpr auto cached_masks_table{ make_cached_mask_lookup_table() };

[[gnu::always_inline]]
inline uint32_t make_mask(const uint8_t data) {
	return cached_masks_table[data];
}

// [[gnu::always_inline]]
// inline uint32_t make_mask(const uint8_t data) {
// 	return 0u
// 		| ((data >> 0) & 0x01u) << tft::pins::D0
// 		| ((data >> 1) & 0x01u) << tft::pins::D1
// 		| ((data >> 2) & 0x01u) << tft::pins::D2
// 		| ((data >> 3) & 0x01u) << tft::pins::D3
// 		| ((data >> 4) & 0x01u) << tft::pins::D4
// 		| ((data >> 5) & 0x01u) << tft::pins::D5
// 		| ((data >> 6) & 0x01u) << tft::pins::D6
// 		| ((data >> 7) & 0x01u) << tft::pins::D7
// 	;
// }

#endif // !defined(GZN_TFT_USE_DEDICATED_GPIO)

} // namespace gzn::tft::pins

