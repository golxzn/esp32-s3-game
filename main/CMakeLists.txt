
option(GZN_DEBUG                   "Just debug"           OFF )
option(GZN_TFT_USE_DEDICATED_GPIO  "Use Dedicated GPIO"   ON )
option(GZN_TFT_GPIO_STRUCTURE      "Use GPIO.out_w1ts"    OFF)
option(GZN_TFT_GPIO_CACHE_BIT_MASK "Cache GPIO bit masks" ${GZN_GRAPHICS_GPIO_STRUCTURE})
option(GZN_ENABLE_FPS              "Draw FPS"             ON )

idf_component_register(
	INCLUDE_DIRS "./include/"

	SRC_DIRS
		"./sources/"
		"./sources/gzn"
		"./sources/gzn/tft"
		"./sources/gzn/graphics"
		"./sources/gzn/filesystem"
		"./sources/gzn/input"
		"./sources/gzn/input/backend"
		"./sources/gzn/audio"
		"./sources/gzn/audio/backend"

	PRIV_REQUIRES
		esp_driver_ledc
		esp_driver_gpio
		esp_driver_spi
		esp_ringbuf
		esp_timer
		esp_hid
		spiffs
		driver
		usb
)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-missing-field-initializers)

macro(define_option option_name)
	if(${option_name})
		target_compile_definitions(${COMPONENT_LIB} PUBLIC ${option_name})
	endif()
endmacro()

define_option(GZN_DEBUG)
define_option(GZN_TFT_USE_DEDICATED_GPIO)
define_option(GZN_TFT_GPIO_STRUCTURE)
define_option(GZN_TFT_GPIO_CACHE_BIT_MASK)
define_option(GZN_ENABLE_FPS)

