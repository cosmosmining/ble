#include <errno.h>

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/watchdog.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/sys/util.h>
#include <zephyr/logging/log.h>

#include "watchdog.h"

LOG_MODULE_REGISTER(wdt, LOG_LEVEL_INF);

#define WDT_TIMEOUT_MS      10000
#define WDT_CHECK_PERIOD_MS 3000
#define MAX_TASKS           4

static const struct device *const wdt = DEVICE_DT_GET(DT_NODELABEL(wdt0));
static int wdt_channel;

static atomic_t registered_mask;
static atomic_t checkin_mask;
static int task_count;

#define SUP_STACK_SIZE 1024
#define SUP_PRIORITY   5
K_THREAD_STACK_DEFINE(sup_stack, SUP_STACK_SIZE);
static struct k_thread sup_thread;

int watchdog_task_register(const char *name)
{
	if (task_count >= MAX_TASKS) {
		LOG_ERR("no watchdog slots left for %s", name);
		return -1;
	}

	int id = task_count++;

	atomic_or(&registered_mask, BIT(id));
	LOG_INF("watchdog task %d = %s", id, name);
	return id;
}

void watchdog_task_feed(int task_id)
{
	if (task_id < 0) {
		return;
	}
	atomic_or(&checkin_mask, BIT(task_id));
}

static void supervisor_fn(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	for (;;) {
		k_sleep(K_MSEC(WDT_CHECK_PERIOD_MS));

		uint32_t reg = (uint32_t)atomic_get(&registered_mask);
		uint32_t chk = (uint32_t)atomic_get(&checkin_mask);

		if ((chk & reg) == reg) {
			/* All supervised tasks checked in. Pet the dog, then
			 * demand a fresh round. A hung task leaves its bit clear,
			 * so we withhold the feed and the SoC resets. */
			(void)wdt_feed(wdt, wdt_channel);
			atomic_clear(&checkin_mask);
		} else {
			LOG_WRN("watchdog: tasks 0x%x missed check-in; withholding feed",
				reg & ~chk);
		}
	}
}

int app_watchdog_init(void)
{
	if (!device_is_ready(wdt)) {
		LOG_ERR("watchdog device not ready");
		return -ENODEV;
	}

	struct wdt_timeout_cfg cfg = {
		.window = { .min = 0U, .max = WDT_TIMEOUT_MS },
		.callback = NULL,
		.flags = WDT_FLAG_RESET_SOC,
	};

	wdt_channel = wdt_install_timeout(wdt, &cfg);
	if (wdt_channel < 0) {
		LOG_ERR("wdt_install_timeout failed (%d)", wdt_channel);
		return wdt_channel;
	}

	int err = wdt_setup(wdt, WDT_OPT_PAUSE_HALTED_BY_DBG);

	if (err) {
		LOG_ERR("wdt_setup failed (%d)", err);
		return err;
	}

	k_thread_create(&sup_thread, sup_stack, SUP_STACK_SIZE,
			supervisor_fn, NULL, NULL, NULL, SUP_PRIORITY, 0, K_NO_WAIT);
	k_thread_name_set(&sup_thread, "wdt_supervisor");

	return 0;
}
