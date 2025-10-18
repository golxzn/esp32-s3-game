#pragma once

#include <cstdint>

namespace gzn::core {

using color = uint16_t;

namespace colors {

inline constexpr color black       { 0x0000u }; /*   0,   0,   0 */
inline constexpr color navy        { 0x000Fu }; /*   0,   0, 128 */
inline constexpr color dark_green  { 0x03E0u }; /*   0, 128,   0 */
inline constexpr color dark_cyan   { 0x03EFu }; /*   0, 128, 128 */
inline constexpr color maroon      { 0x7800u }; /* 128,   0,   0 */
inline constexpr color purple      { 0x780Fu }; /* 128,   0, 128 */
inline constexpr color olive       { 0x7BE0u }; /* 128, 128,   0 */
inline constexpr color light_grey  { 0xD69Au }; /* 211, 211, 211 */
inline constexpr color dark_grey   { 0x7BEFu }; /* 128, 128, 128 */
inline constexpr color blue        { 0x001Fu }; /*   0,   0, 255 */
inline constexpr color green       { 0x07E0u }; /*   0, 255,   0 */
inline constexpr color cyan        { 0x07FFu }; /*   0, 255, 255 */
inline constexpr color red         { 0xF800u }; /* 255,   0,   0 */
inline constexpr color magenta     { 0xF81Fu }; /* 255,   0, 255 */
inline constexpr color yellow      { 0xFFE0u }; /* 255, 255,   0 */
inline constexpr color white       { 0xFFFFu }; /* 255, 255, 255 */
inline constexpr color orange      { 0xFDA0u }; /* 255, 180,   0 */
inline constexpr color green_yellow{ 0xB7E0u }; /* 180, 255,   0 */
inline constexpr color pink        { 0xFE19u }; /* 255, 192, 203 */ //Lighter pink, was 0xFC9F
inline constexpr color brown       { 0x9A60u }; /* 150,  75,   0 */
inline constexpr color gold        { 0xFEA0u }; /* 255, 215,   0 */
inline constexpr color silver      { 0xC618u }; /* 192, 192, 192 */
inline constexpr color skyblue     { 0x867Du }; /* 135, 206, 235 */
inline constexpr color violet      { 0x915Cu }; /* 180,  46, 226 */

} // namespace colors

} // gzn::core


namespace gzn {

using core::color;

} // namespace gzn

