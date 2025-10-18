#pragma once

#include <cstdint>

namespace gzn::graphics::defaults {

inline constexpr uint8_t minimal_buffers_count{ 1u };
inline constexpr uint8_t maximum_buffers_count{ 3u };
inline constexpr uint8_t buffers_count{ 2u };

inline constexpr uint8_t pixel_size{ 4u };

inline constexpr uint32_t render_thread_core_id   {    1u };
inline constexpr uint32_t render_thread_stack_size{ 8192u };
inline constexpr uint32_t render_thread_priority  {    2u };

} // namespace gzn::graphics::defaults


