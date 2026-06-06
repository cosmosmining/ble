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

	LOG_INF("init complete");

	/* Work runs in dedicated threads from here on. */
	return 0;
}
