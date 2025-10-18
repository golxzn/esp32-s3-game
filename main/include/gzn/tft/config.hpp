#pragma once

#if __has_include("tft-user-config.hpp")
#include "tft-user-config.hpp"
#endif // __has_include("tft-user-config.hpp")

#if !defined(GZN_TFT_BACKEND)
#define GZN_TFT_BACKEND ili9486
#endif // !defined(GZN_TFT_BACKEND)


#define GZN_TFT_BACKEND_COMMANDS_HPP       <gzn/tft/backend/GZN_TFT_BACKEND/commands.hpp>
#define GZN_TFT_BACKEND_CONSTANTS_HPP      <gzn/tft/backend/GZN_TFT_BACKEND/constants.hpp>
#define GZN_TFT_BACKEND_INITIALIZATION_HPP <gzn/tft/backend/GZN_TFT_BACKEND/initialization.hpp>

