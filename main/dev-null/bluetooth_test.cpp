
#include <string_view>

#include <esp_log.h>
#include <esp_bt.h>
#include <esp_bt_main.h>

#include <os/os_mempool.h>

#include <nvs_flash.h>

#include <nimble/ble.h>
#include <nimble/transport.h>
#include <nimble/npl_freertos.h>
#include <nimble/nimble_port.h>
#include <nimble/nimble_port_freertos.h>
#include <esp_nimble_hci.h>

#include <host/ble_hs.h>
#include <host/util/util.h>
#include <host/ble_esp_gap.h>
#include <host/ble_esp_gatt.h>

// #include <esp_gap_bt_api.h>
#include <esp_gap_ble_api.h>
#include <esp_hidh_api.h>        // esp_hid_host API

/* BLE */
#include <services/gap/ble_svc_gap.h>



static const char *TAG = "PS5_HID_HOST";
static constexpr std::string_view TARGET_NAME{ "Wireless Controller" }; // DualSense advertises this

// ---------- Forward (each logical step = function) ----------

static void nvs_init_step();
static void bt_controller_init_step();
static void nimble_init_step();
static void hid_host_register_and_init_step();
static void gap_register_and_start_scan_step();

// static void on_gap_event(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param);
// static void on_gap_event(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);
// static void on_hidh_event(esp_hidh_cb_event_t event, esp_hidh_cb_param_t *param);
// static void send_output_report_step(const esp_bd_addr_t bd_addr, const uint8_t *data, size_t len);

// ---------- Implementation ----------

static void nvs_init_step() {
	esp_err_t ret{ nvs_flash_init() };
	if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
		ESP_ERROR_CHECK(nvs_flash_erase());
		ret = nvs_flash_init();
	}
	ESP_ERROR_CHECK(ret);
	ESP_LOGI(TAG, "NVS initialized");
}

static void bt_controller_init_step() {
	ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));

	esp_bt_controller_config_t cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_bt_controller_init(&cfg));
	// enable dual mode (BT Classic + BLE) on chips that support it; for
	// classic-only flow classic is fine
	// ESP_ERROR_CHECK(esp_bt_controller_enable(ESP_BT_MODE_BTDM));
	ESP_ERROR_CHECK(esp_bt_controller_enable(ESP_BT_MODE_BLE));
	ESP_LOGI(TAG, "BT controller enabled");
}

static void reset_callback(int reason) {
}
static void sync_callback() {
}

static void nimble_init_step() {

	ESP_ERROR_CHECK(nimble_port_init());
/*
	npl_freertos_funcs_init(); // function pointers for OS porting
	// npl_freertos_funcs_deinit(); SHOULD BE CALLED ON DESTRUCOR


	ESP_ERROR_CHECK(esp_nimble_hci_init());
	// esp_nimble_hci_deinit(); SHOULD BE CALLED ON DESTRUCOR

	ble_npl_eventq_init(&g_eventq_dflt);
	// ble_npl_eventq_deinit(&g_eventq_dflt); SHOULD BE CALLED ON DESTRUCOR

	os_mempool_module_init();
	os_msys_init();

	ble_transport_hs_init();
	// ble_transport_ll_deinit(); SHOULD BE CALLED ON DESTRUCOR
*/
	ESP_LOGI(TAG, "NimBLE initialized");

	ble_hs_cfg.reset_cb = reset_callback;
	ble_hs_cfg.sync_cb  = sync_callback;
	ble_hs_cfg.store_status_cb = ble_store_util_status_rr;

}

static void gap_gatts_start_and_scan_step() {



	// ESP_ERROR_CHECK(esp_ble_gap_register_callback(on_gap_event));
}


/*
static void hid_host_register_and_init_step() {
	// register HID host callback
	ESP_ERROR_CHECK(esp_bt_hid_host_register_callback(on_hidh_event));
	ESP_ERROR_CHECK(esp_bt_hid_host_init());
	ESP_LOGI(TAG, "HID host initialized");
}

static void gap_register_and_start_scan_step() {
	ESP_ERROR_CHECK(esp_bt_gap_register_callback(on_gap_event));
	esp_bt_gap_set_scan_mode(
		ESP_BT_CONNECTABLE,
		ESP_BT_GENERAL_DISCOVERABLE
	);
	ESP_ERROR_CHECK(esp_bt_gap_start_discovery(ESP_BT_INQ_MODE_GENERAL_INQUIRY, 10, 0));
	ESP_LOGI(TAG, "Started classic inquiry");
}
*/

// GAP callback: look for device with name "Wireless Controller"
//
static void on_gap_event(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
	if (event == ESP_GAP_SEARCH_DISC_BLE_RES_EVT) {
		auto &r{ param->scan_rst };

		int i{};
		for (; i < r.num_prop; ++i) {
			auto &prop{ r.prop[i] };
			if (prop.type != ESP_BT_GAP_DEV_PROP_BDNAME) {
				continue;
			}

			const std::string_view name{
				static_cast<const char *>(prop.val),
				static_cast<size_t>(prop.len)
			};
			if (name == TARGET_NAME) {
				break;
			}
		}

		if (i != r.num_prop) {
			esp_ble_gap_cancel_discovery();
			esp_ble_hid_host_connect(r.bda);

			ESP_LOGI(TAG, "Found PS5 at %02x:%02x:%02x:%02x:%02x:%02x",
				 r.bda[0],r.bda[1],r.bda[2],r.bda[3],r.bda[4],r.bda[5]
			);
		}
	}
}
/*
static void on_gap_event(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param) {
	if (event == ESP_BT_GAP_DISC_RES_EVT) {
		auto &r{ param->disc_res };

		int i{};
		for (; i < r.num_prop; ++i) {
			auto &prop{ r.prop[i] };
			if (prop.type != ESP_BT_GAP_DEV_PROP_BDNAME) {
				continue;
			}

			const std::string_view name{
				static_cast<const char *>(prop.val),
				static_cast<size_t>(prop.len)
			};
			if (name == TARGET_NAME) {
				break;
			}
		}

		if (i != r.num_prop) {
			esp_bt_gap_cancel_discovery();
			esp_bt_hid_host_connect(r.bda);

			ESP_LOGI(TAG, "Found PS5 at %02x:%02x:%02x:%02x:%02x:%02x",
				 r.bda[0],r.bda[1],r.bda[2],r.bda[3],r.bda[4],r.bda[5]
			);
		}
	}
}
*/

// HID host callback: handle init/open/data/close
static void on_hidh_event(esp_hidh_cb_event_t event, esp_hidh_cb_param_t *param) {
	switch (event) {
		case ESP_HIDH_INIT_EVT: {
			ESP_LOGI(TAG, "HIDH init ok");
		} break;

		case ESP_HIDH_OPEN_EVT: {
			ESP_LOGI(TAG, "HID connected");
		} break;

		case ESP_HIDH_CLOSE_EVT: {
			ESP_LOGI(TAG, "HID disconnected");
		} break;

		case ESP_HIDH_GET_RPT_EVT: {
			ESP_LOGI(TAG, "HID RPT event len=%d", param->get_rpt.len);
		} break;

		case ESP_HIDH_DATA_IND_EVT: {
			// input report received
			ESP_LOGI(TAG, "HID input len=%d", param->data_ind.len);
			// parse DualSense report here (buttons, axes, gyro, etc.)
		} break;

		default: {
			ESP_LOGI(TAG, "HID event=%d", event);
		} break;
	}
}

static void send_output_report_step(
	esp_bd_addr_t bd_addr, uint8_t *data, size_t len
) {
	// Set/Send an OUTPUT report (rumble/led). Type = OUTPUT
	const auto ret{ esp_bt_hid_host_set_report(
		bd_addr, ESP_HIDH_REPORT_TYPE_OUTPUT, data, len
	) };

	if (ret != ESP_OK) {
		ESP_LOGE(TAG, "set_report failed: %s", esp_err_to_name(ret));
	}
}

// ---------- App entry ----------
void bluetooth_test() {
	nvs_init_step();
	bt_controller_init_step();
	nimble_init_step();
	// hid_host_register_and_init_step();
	// gap_register_and_start_scan_step();

	gap_gatts_start_and_scan_step();

	// now wait for callbacks (hidh_cb / gap_cb). To send example output later:
	// esp_bd_addr_t bd = {0xAA,0xBB,0xCC,0x11,0x22,0x33};
	// uint8_t rumble[8] = { ... }; send_output_report_step(bd, rumble, sizeof(rumble));
}

