#pragma once

#include <span>
#include <memory>
#include <functional>

#include "gzn/filesystem/context.hpp"

namespace gzn::fs {

inline constexpr size_t default_chunk_size{ 4096 };
using file_id = uint32_t;

struct mount_info {
	std::string_view base_path{};
	std::string_view portation_label{};
	size_t           max_simultanious_opened_files{};
	bool             format_if_mount_failed{ false };
};
enum class mount_error {
	ok,
	invalid_argument,
	not_found,
	not_enough_memory,
	already_mounted,
	already_mounted_or_encrypted,
	mount_failed,
	max_mounted_portations_reached,
};


using fetch_callback = std::function<std::span<uint8_t>(const size_t count)>;
using open_callback  = bool(*)(const fetch_callback &fetch, void *user);
using read_callback  = bool(*)(const fetch_callback &fetch, void *user);

// The stupiest idea for embedded, so nowhere used. I dedinitely should remove it...
struct file_callbacks {
	open_callback on_open{ nullptr };
	read_callback on_read{ nullptr };

	[[gnu::always_inline]]
	inline auto empty() const -> bool { return !on_open && !on_read; }
};

class manager {
public:
	static constexpr char   sep{ '/' };
	static constexpr size_t buffer_length{ 128 };

	struct setup_info {
		context::garbage_collector_config gc_config{};
	};

	[[nodiscard]]
	static auto initialize(const setup_info &info) -> bool;
	static void destroy();
	static void update();

	[[nodiscard]]
	static auto mount(const mount_info &info) -> mount_error;
	static void unmount(const std::string_view portation);
	[[nodiscard]]
	static auto is_mounted(const std::string_view portation) -> bool;
	static auto format(const std::string_view portation) -> bool;

	[[nodiscard]]
	static auto can_open(const std::string_view file) -> bool;

	static auto read_file(
		const std::string_view file,
		const file_callbacks &callbacks,
		void *user = nullptr
	) -> bool;

	static auto read_file_loop(
		const std::string_view file,
		const file_callbacks &callbacks,
		void *user = nullptr
	) -> bool;

	static void collect_garbage();

private:
	inline static std::unique_ptr<context> ctx{};
};

} // namespace gzn::fs


