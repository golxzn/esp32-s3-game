#pragma once


#include "gzn/tft/config.hpp"

#include GZN_TFT_BACKEND_INITIALIZATION_HPP


namespace gzn::tft {

constexpr decltype(auto) make_initialization_sequence() {
	return backend::GZN_TFT_BACKEND::initialization_sequence;
}

} // namespace gzn::tft

