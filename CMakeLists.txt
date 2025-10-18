# https://docs.espressif.com/projects/esp-idf/en/v5.5/esp32s3/api-guides/build-system.html#component-common-requirements

cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# "Trim" the build. Include the minimal set of components, main, and anything
# it depends on.
idf_build_set_property(MINIMAL_BUILD ON)

project(test-proj)

# target_link_libraries(${project_elf} PRIVATE "-Wl,--wrap=esp_panic_handler")

spiffs_create_partition_image(assets "./assets/" FLASH_IN_PROJECT)

file(CREATE_LINK
	"${CMAKE_BINARY_DIR}/compile_commands.json"
	"${CMAKE_CURRENT_SOURCE_DIR}/compile_commands.json"
	COPY_ON_ERROR
	SYMBOLIC
)

