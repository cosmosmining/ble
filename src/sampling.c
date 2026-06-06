#include <errno.h>

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/logging/log.h>

#include "sampling.h"
#include "ess.h"
#include "watchdog.h"

LOG_MODULE_REGISTER(sampling, LOG_LEVEL_INF);

#define ENV_SENSOR_NODE DT_ALIAS(env_sensor)
BUILD_ASSERT(DT_NODE_HAS_STATUS(ENV_SENSOR_NODE, okay),
	     "env-sensor alias is missing/disabled in the devicetree overlay");

static const struct device *const env_sensor = DEVICE_DT_GET(ENV_SENSOR_NODE);

static K_SEM_DEFINE(drdy_sem, 0, 1);
static int wdt_task_id = -1;

#define SAMPLING_STACK_SIZE 2048
#define SAMPLING_PRIORITY   7
K_THREAD_STACK_DEFINE(sampling_stack, SAMPLING_STACK_SIZE);
static struct k_thread sampling_thread;

static void drdy_handler(const struct device *dev, const struct sensor_trigger *trig)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(trig);

	/* The DRDY GPIO ISR woke the sensor subsystem's trigger thread, which
	 * calls us here. Keep it minimal: wake the sampling thread and return.
	 * No I2C, no GATT, no blocking primitives in this path. */
	k_sem_give(&drdy_sem);
}

static void sample_once(void)
{
	struct sensor_value val;

	if (sensor_sample_fetch(env_sensor) < 0) {
		LOG_WRN("sample fetch failed");
		return;
	}

	if (sensor_channel_get(env_sensor, SENSOR_CHAN_AMBIENT_TEMP, &val) == 0) {
		ess_update_temperature(&val);
	}
	if (sensor_channel_get(env_sensor, SENSOR_CHAN_HUMIDITY, &val) == 0) {
		ess_update_humidity(&val);
	}
	if (sensor_channel_get(env_sensor, SENSOR_CHAN_PRESS, &val) == 0) {
		ess_update_pressure(&val);
	}
}

static void sampling_fn(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	struct sensor_trigger trig = {
		.type = SENSOR_TRIG_DATA_READY,
		.chan = SENSOR_CHAN_ALL,
	};
	bool triggered = (sensor_trigger_set(env_sensor, &trig, drdy_handler) == 0);

	if (!triggered) {
		LOG_WRN("DATA_READY trigger unavailable; falling back to 1 Hz polling");
	}

	for (;;) {
		if (triggered) {
			/* Block on DRDY. The 2 s cap means a missed interrupt
			 * still yields a sample and a watchdog check-in. */
			(void)k_sem_take(&drdy_sem, K_SECONDS(2));
		} else {
			k_sleep(K_SECONDS(1));
		}

		sample_once();
		watchdog_task_feed(wdt_task_id);
	}
}

int app_sampling_init(void)
{
	if (!device_is_ready(env_sensor)) {
		LOG_ERR("%s not ready", env_sensor->name);
		return -ENODEV;
	}

	wdt_task_id = watchdog_task_register("sampling");

	k_thread_create(&sampling_thread, sampling_stack, SAMPLING_STACK_SIZE,
			sampling_fn, NULL, NULL, NULL, SAMPLING_PRIORITY, 0, K_NO_WAIT);
	k_thread_name_set(&sampling_thread, "sampling");

	return 0;
}
