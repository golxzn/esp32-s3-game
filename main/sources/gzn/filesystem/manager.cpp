#include <cerrno>
#include <cstdio>
#include <cstring>
#include <utility>

#include <esp_log.h>
#include <esp_spiffs.h>

#include "gzn/filesystem/manager.hpp"


namespace gzn::fs {

static constexpr auto TAG{ "fs::manager" };

auto manager::initialize(const setup_info &info) -> bool {
	if (ctx) {
		return false;
	}

	ctx = std::make_unique<context>();
	if (!ctx) {
		return false;
	}

	ctx->gc_config = info.gc_config;
	return true;
};

void manager::destroy() {
	if (!ctx) { return; }

	for (auto &portation : ctx->mounted_portations) {
		if (!std::empty(portation)) {
			esp_vfs_spiffs_unregister(std::data(portation));
			portation = std::string_view{};
		}
	}
};

void manager::update() {
	assert(ctx != nullptr && "Use manager::update before successful manager::initialization!");

	if (ctx->gc_counter == ctx->gc_config.cleanup_frequency) {
		collect_garbage();
		ctx->gc_counter = 0u;
	}
}

auto manager::mount(const mount_info &info) -> mount_error {
	if (std::empty(info.base_path) || std::empty(info.portation_label)) {
		return mount_error::invalid_argument;
	}

	const auto mount_id{ ctx->get_available_mount_id() };
	if (mount_id == context::npos) {
		return mount_error::max_mounted_portations_reached;
	}

	if (is_mounted(info.portation_label) || is_mounted(info.base_path)) {
		return mount_error::already_mounted;
	}

	const esp_vfs_spiffs_conf_t config{
		.base_path              = std::data(info.base_path),
		.partition_label        = std::data(info.portation_label),
		.max_files              = info.max_simultanious_opened_files,
		.format_if_mount_failed = info.format_if_mount_failed
	};
	switch (esp_vfs_spiffs_register(&config)) {
		case ESP_ERR_NO_MEM       : return mount_error::not_enough_memory;
		case ESP_ERR_INVALID_STATE: return mount_error::already_mounted_or_encrypted;
		case ESP_ERR_NOT_FOUND    : return mount_error::not_found;
		case ESP_FAIL             : return mount_error::mount_failed;
		default: break;
	}

	ctx->mounted_portations[mount_id] = info.portation_label;

	return mount_error::ok;
}


void manager::unmount(const std::string_view portation) {
	std::ignore = esp_vfs_spiffs_unregister(std::data(portation));
}

auto manager::is_mounted(const std::string_view portation) -> bool {
	return esp_spiffs_mounted(std::data(portation));
}

auto manager::format(const std::string_view portation) -> bool {
	return esp_spiffs_format(std::data(portation)) == ESP_OK;
}

auto manager::can_open(const std::string_view file) -> bool {
	if (std::empty(file)) {
		ESP_LOGE(TAG, "Cannot open empty file!");
		return false;
	}

	const auto second_sep{ file.find_first_of(sep, 2) };
	if (second_sep == std::string_view::npos) {
		ESP_LOGE(TAG, R"(Cannot find second '%c' in "%.*s" filename)", sep,
			static_cast<int>(std::size(file)), std::data(file)
		);
		return false;
	}

	if (const auto port{ file.substr(1, second_sep - 1) }; !ctx->find_portation(port)) {
		ESP_LOGE(TAG, R"(Cannot find portation '%.*s' of "%.*s" file!)",
			static_cast<int>(std::size(port)), std::data(port),
			static_cast<int>(std::size(file)), std::data(file)
		);
		return false;
	}

	return true;
}

auto manager::read_file(
	const std::string_view file,
	const file_callbacks &callbacks,
	void *const user
) -> bool {
	if (std::empty(callbacks) || !can_open(file)) {
		return false;
	}

	auto file_descriptor{ std::fopen(std::data(file), "rb") };
	if (!file_descriptor) {
		ESP_LOGE(TAG, R"(Cannot open "%.*s" file: %s)",
			static_cast<int>(std::size(file)), std::data(file),
			std::strerror(errno)
		);
		return false;
	}

	std::array<uint8_t, buffer_length> buffer{};

	const auto read_cb{ [&buffer, file_descriptor](const size_t count){
		const size_t read{ std::fread(
			std::data(buffer), sizeof(uint8_t),
			std::min(count, std::size(buffer)),
			file_descriptor
		) };
		return std::span{ std::data(buffer), read };
	} };

	if (callbacks.on_open && !callbacks.on_open(read_cb, user)) {
		std::fclose(file_descriptor);
		return true;
	}

	while (!std::feof(file_descriptor) && callbacks.on_read(read_cb, user)) {}

	std::fclose(file_descriptor);
	return true;
}


auto manager::read_file_loop(
	const std::string_view file,
	const file_callbacks &callbacks,
	void *const user
) -> bool {
	if (std::empty(callbacks) || !can_open(file)) {
		return false;
	}

	auto file_descriptor{ std::fopen(std::data(file), "rb") };
	if (!file_descriptor) {
		ESP_LOGE(TAG, R"(Cannot open "%.*s" file: %s)",
			static_cast<int>(std::size(file)), std::data(file),
			std::strerror(errno)
		);
		return false;
	}

	std::array<uint8_t, buffer_length> buffer{};
	const auto read_cb{ [&buffer, file_descriptor](const size_t count){
		const size_t read{ std::fread(
			std::data(buffer), sizeof(uint8_t),
			std::min(count, std::size(buffer)),
			file_descriptor
		) };
		return std::span{ std::data(buffer), read };
	} };

	if (callbacks.on_open && !callbacks.on_open(read_cb, user)) {
		std::fclose(file_descriptor);
		return true;
	}

	const auto base_offset{ std::ftell(file_descriptor) };
	while (callbacks.on_read(read_cb, user)) {
		if (std::feof(file_descriptor)) {
			std::fseek(file_descriptor, base_offset, SEEK_SET);
		}
	}

	std::fclose(file_descriptor);
	return true;
}


void manager::collect_garbage() {
	ESP_LOGI(TAG, "Collecting garbage");
	for (const auto portation : ctx->mounted_portations) {
		if (std::empty(portation)) {
			continue;
		}

		for (size_t attemts{ ctx->gc_config.cleanup_attempts }; attemts != 0; --attemts) {
			if (ESP_ERR_NOT_FINISHED != esp_spiffs_gc(std::data(portation), 1024 * 1024)) {
				break;
			}
		}
	}
}

} // namespace gzn::fs

