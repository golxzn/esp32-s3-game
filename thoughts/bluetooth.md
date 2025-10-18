# De-messing bluetooth

I'm going to use Blueooth (BT) 5.0 since esp32-s3 does support it.
I need to figure out:

1. How to setup BT properly;
2. How to discover & connect devices;
3. How to recieve & send data;
4. Which data format DualSense has;
5. How to support both USB & BT connection.


## BT Protocol stack & L2CAP | BT | BLE | GAP | GATTS | other shit

There are a lot of different terms which seems to be doing same or different
shit, but it's hard to make it clear which functionality I really need to have
and how to set it up properly.

Things are even worser since I to use those functions I need to configure the
idf.py project and include some shit in CMakeLists. Some of those features are
interchangable so it's fucked.

And, at the rest of the fucking day, there are 3 ways to use BT features on the
chip:

1. Native calls;
2. `Bluedroid` (default stack) - Supports both BT Classic & BT Low Energy;
3. `NimBLE` - lightweight BT Low Energy stack.

I'm not sure that I need BT Low Energy, but if I do need it, I'd use NimBLE.

## BT & BLE

`BT`, is a __Common API__. It's the *Native calls* I guess. it `REQUIRES bt` in
CMakeLists.txt. The headers usually starts with `esp_bt_` prefix. There are
only three of them:

1. `esp_bt_defs.h` - some defines and structs. Interesting that it defines
   things like `esp_ble_conn_params_t`. The prefix says that it's BLE shit.
2. `esp_bt_main.h` - enable, disable, init, etc. Some basic stuff;
3. `esp_bt_device.h` - related with `bluedroid`. Some `esp_bt_dev_register_callback`.

I guessed that `BLE` means __Bluetooth Low Energy__, but I'm not sure yet.

[!] The BT LE API docs contains of "BT LE GAP", "BT LE GATT Define", "BT LE
GATT Server", "BT LE GATT Client", and "BT LE BluFi" sections. This section of
bluetooth is more documented comparing with BL Classic.

[?] BT Classic API docs contains of "BT Define", "BT Main", and "BT Device"
sections. It looks like I need to use __BL LE__. (gpt-oss noticed it too)

Anyway, let's dig to those GAP & GATT shit.


## GAP & GATT

- `GAP` (Generic Access Profile):
    - manages device discovery;
    - connection establishment;
    - roles & modes definitions.
- `GATT` (Generic Attribute Protocol):
    - implements attribute-based data exchange in BT LE.

Additional shit which is an eyesores:

- `L2CAP` (Logical Link and Adaptation Protocol) - data segmentation,
  reassembly & multiplexing.
- `SMP` (Security Manager Protocol) - authentification, encryption, secure pairing;
- `SDP` (Service Discovery Protocol) - mainly used in BT Classic for
  advertising & exploration.

# (maybe) Final thought.

Seems like I need the following stack:
- `NimBLE` for main bluetooth things & initialization;
- `GAP` to set the role and start discovery session. I'll find a controller through it;
- `GATT` to communicate using BT LE. Commands here and there, u know.


SOOOOOO, how to initialize all of those crap properly?

## `NimBLE` + `GAP` + `GATT` workaround

`NimBLE` is not as *lightweight* in terms of usage as it is. Since it's Apache
technology:

> Apache MyNewt NimBLE is a highly configurable and Bluetooth® SIG qualifiable
> Bluetooth Low Energy (Bluetooth LE) stack providing both host and controller
> functionalities...

But, happy to hear, it was ported specifically to ESP32 through FreeRTOS.

### [NimBLE] Initialization & Usage

__*Thread Model*__:

Using `nimble_port_freertos_init` we could spawn the thread over FreeRTOS task.

__*Programming Sequence*__:

0. Configure NimBLE for BT Host in `menuconfig`;

1. Initialize `NVS` (Non-Volatile Storage Library) using `nvs_flash_init`;
2. Initialize Host & Controller stack using `nimble_port_init`;
3. Setup the required NimBLE host configuration parameters and callbacks;
4. Perform application specific tasks/initialization (for controller);
5. Run the thread for host stack using `nimble_port_freertos_init`.


#### 1. Initialization `NVS`

```cpp
static void nvs_init_step() {
	esp_err_t ret{ nvs_flash_init() };
	if (ret == ESP_ERR_NVS_NO_FREE_PAGES
	||  ret == ESP_ERR_NVS_NEW_VERSION_FOUND
	) {
		ESP_ERROR_CHECK(nvs_flash_erase());
		ret = nvs_flash_init();
	}
	ESP_ERROR_CHECK(ret);
	ESP_LOGI(TAG, "NVS initialized");
}
```

#### 2. Initialize Host & Controller:




