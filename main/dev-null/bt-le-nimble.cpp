#include <string_view>

#include <esp_log.h>
#include <nvs_flash.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <nimble/nimble_port.h>
#include <nimble/nimble_port_freertos.h>

#include <host/ble_hs.h>
#include <host/ble_gap.h>



static const char *TAG="dualsense";
static uint8_t own_type{};

void print_info(const ble_hs_adv_fields &fields) {
	if (fields.name != 0) {
		ESP_LOGI(TAG, "-------------------------");
		ESP_LOGI(TAG, "Found name: %.*s (%s)",
			static_cast<int>(fields.name_len),
			reinterpret_cast<const char *>(fields.name),
			(fields.name_is_complete ? "complete" : "incomplete")
		);
	}
}

static int gap_cb(ble_gap_event *ev, void *arg) {
	switch (ev->type) {
		case BLE_GAP_EVENT_DISC: {
			ble_hs_adv_fields f;
			if (ble_hs_adv_parse_fields(&f, ev->disc.data, ev->disc.length_data) != 0) {
				return 0;
			}

			if(f.name_len) {
				const std::string_view name{
					reinterpret_cast<const char *>(f.name),
					std::min<size_t>(f.name_len, 32u)
				};

				print_info(f);

				if (name == "Wireless Controller") {
					ESP_LOGI(TAG, "found -> connect");
					ble_gap_disc_cancel();
					ble_gap_connect(
						own_type,
						&ev->disc.addr,
						30000, // duration ms
						nullptr,
						gap_cb,
						nullptr
					);
				}
			}
		} break;

		case BLE_GAP_EVENT_CONNECT: {
			if (ev->connect.status == 0) {
				ESP_LOGI(TAG,"connected");
			} else {
				ESP_LOGI(TAG,"connect failed %d", ev->connect.status);
			}
		} break;

		case BLE_GAP_EVENT_DISCONNECT: {
			ESP_LOGI(TAG, "disconnected");
		} break;
	}

	return 0;
}

static void start_scan(void){
	const ble_gap_disc_params params{
		.itvl   = 0x0010,
		.window = 0x0010
	};
	ble_gap_disc(own_type, BLE_HS_FOREVER, &params, gap_cb, nullptr);
}

static void ble_on_sync() {
	ble_hs_id_infer_auto(0, &own_type);
	start_scan();
}

static void ble_on_reset(int reason) {
	ESP_LOGI(TAG,"reset %d",reason);
}

static void host_task(void *param) {
	nimble_port_run();
	nimble_port_freertos_deinit();
}

static void nvs_init_step() {
	esp_err_t ret{ nvs_flash_init() };
	if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
		ESP_ERROR_CHECK(nvs_flash_erase());
		ret = nvs_flash_init();
	}
	ESP_ERROR_CHECK(ret);
}

void bluetooth_test() {
	nvs_init_step();
	nimble_port_init();
	ble_hs_cfg.reset_cb = ble_on_reset;
	ble_hs_cfg.sync_cb = ble_on_sync;
	nimble_port_freertos_init(host_task);
}

