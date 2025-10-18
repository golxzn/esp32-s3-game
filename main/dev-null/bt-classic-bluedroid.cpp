#include <string.h>
#include <stdio.h>

#include "nvs_flash.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_event.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_bt_api.h"
#include "esp_bt_device.h"
#include "esp_bt_defs.h"

#include "esp_hidh.h"
#include "esp_hidh_api.h"
#include "esp_gap_bt_api.h"

static const char *TAG = "dualsense_hid";

static void hidh_event_cb(void *handler_args, esp_event_base_t base, int32_t id, void *event_data) {
	auto p{ static_cast<esp_hidh_event_data_t *>(event_data) };

	switch (static_cast<esp_hidh_event_t>(id)) {
		case ESP_HIDH_START_EVENT:
			ESP_LOGI(TAG, "HID host started -> start discovery");
			esp_bt_gap_start_discovery(ESP_BT_INQ_MODE_GENERAL_INQUIRY, 10, 0);
			break;

		case ESP_HIDH_OPEN_EVENT: {
			ESP_LOGI(TAG, "HID open status %d", p->open.status);
			if (p->open.dev) {
				const uint8_t *b = esp_hidh_dev_bda_get(p->open.dev);
				if (b) ESP_LOGI(TAG, "Device BDA: %02X:%02X:%02X:%02X:%02X:%02X", b[0],b[1],b[2],b[3],b[4],b[5]);
				const char *name = esp_hidh_dev_name_get(p->open.dev);
				if (name) ESP_LOGI(TAG, "Device name: %s", name);
			}
		} break;

		case ESP_HIDH_INPUT_EVENT:
			if (p->input.data && p->input.length > 0) {
				ESP_LOGI(TAG, "INPUT length=%d", p->input.length);
				ESP_LOG_BUFFER_HEXDUMP(TAG, p->input.data, p->input.length, ESP_LOG_INFO);
			}
			break;

		case ESP_HIDH_CLOSE_EVENT:
			ESP_LOGI(TAG, "HID closed");
			break;

		default: break;

	}
}

static void gap_cb(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param) {
	if (event != ESP_BT_GAP_DISC_RES_EVT) return;

	for (int i = 0; i < param->disc_res.num_prop; ++i) {
		auto &pr = param->disc_res.prop[i];

		if (pr.type != ESP_BT_GAP_DEV_PROP_BDNAME) {
			continue;
		}

		if (pr.len > 0) {
			char nm[64]; int n = pr.len < (int)sizeof(nm)-1 ? pr.len : (int)sizeof(nm)-1;
			memcpy(nm, pr.val, n); nm[n] = 0;
			ESP_LOGI(TAG, "Found name: %s", nm);
			if (strstr(nm, "Wireless Controller") || strstr(nm, "DualSense")) {
				ESP_LOGI(TAG, "Match -> stop discovery and request open");
				esp_bt_gap_cancel_discovery();
				esp_hidh_dev_open(param->disc_res.bda, ESP_HID_TRANSPORT_BLE, 0); // transport arg may vary by IDF; see note below
				return;
			}
		}
	}
}


void bluetooth_test() {
	esp_err_t r = nvs_flash_init();
	if (r == ESP_ERR_NVS_NO_FREE_PAGES || r == ESP_ERR_NVS_NEW_VERSION_FOUND) {
		nvs_flash_erase();
		nvs_flash_init();
	}

	esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
	esp_bt_controller_init(&bt_cfg);
	esp_bt_controller_enable(ESP_BT_MODE_CLASSIC_BT);
	esp_bluedroid_init();
	esp_bluedroid_enable();

	esp_event_loop_create_default();
	const esp_hidh_config_t cfg{
		.callback = hidh_event_cb,
		.event_stack_size = 4096,
		.callback_arg = nullptr
	};
	esp_hidh_init(&cfg);

	esp_bt_gap_register_callback(gap_cb);
	esp_bt_gap_set_device_name("ESP32_HID_HOST");
	esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);
}

