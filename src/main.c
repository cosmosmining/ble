/*
 * BLE Environmental Sensing node — application entry point.
 *
 * Wiring order matters: bring the watchdog supervisor up first, then the BLE
 * link, then the sampling thread (which registers itself with the watchdog).
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "bluetooth.h"
#include "sampling.h"
#include "watchdog.h"

LOG_MODULE_REGISTER(app, LOG_LEVEL_INF);

#if defined(CONFIG_BOOTLOADER_MCUBOOT)
#include <zephyr/dfu/mcuboot.h>

/* This image is only linked into an MCUboot slot for the OTA build, which must
 * also carry the SMP transport that receives updates. Fail the build loudly if
 * the OTA overlay (sysbuild/ble.conf) was not applied to this image. */
BUILD_ASSERT(IS_ENABLED(CONFIG_MCUMGR_TRANSPORT_BT),
	     "OTA image is missing the SMP DFU transport (sysbuild/ble.conf not applied)");

static void confirm_running_image(void)
{
	if (boot_is_img_confirmed()) {
		return;
	}

	/* A freshly DFU'd image boots in test mode; reaching a healthy boot
	 * confirms it so MCUboot keeps it instead of reverting on next reset. */
	if (boot_write_img_confirmed() == 0) {
		LOG_INF("running image confirmed");
	} else {
		LOG_ERR("failed to confirm running image");
	}
}
#else
static inline void confirm_running_image(void) { }
#endif

int main(void)
{
	LOG_INF("ble env-sensor node starting");

	if (app_watchdog_init() != 0) {
		LOG_ERR("watchdog init failed");
	}
	if (app_bt_init() != 0) {
		LOG_ERR("bluetooth init failed");
	}
	if (app_sampling_init() != 0) {
		LOG_ERR("sampling init failed");
	}

	confirm_running_image();

	LOG_INF("init complete");

	/* Work runs in dedicated threads from here on. */
	return 0;
}
