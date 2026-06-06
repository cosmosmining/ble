#include <zephyr/kernel.h>
#include <zephyr/sys/util.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gap.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/logging/log.h>

#if IS_ENABLED(CONFIG_SETTINGS)
#include <zephyr/settings/settings.h>
#endif

#include "bluetooth.h"

LOG_MODULE_REGISTER(app_bt, LOG_LEVEL_INF);

/* Power-tuned connection: 100-150 ms interval with peripheral latency so the
 * link costs little while idle between samples. Units: 1.25 ms / 10 ms. */
#define CONN_INT_MIN 80
#define CONN_INT_MAX 120
#define CONN_LATENCY 4
#define CONN_TIMEOUT 400

static const struct bt_data ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
	BT_DATA_BYTES(BT_DATA_UUID16_ALL, BT_UUID_16_ENCODE(BT_UUID_ESS_VAL)),
};

static const struct bt_data sd[] = {
	BT_DATA(BT_DATA_NAME_COMPLETE, CONFIG_BT_DEVICE_NAME,
		sizeof(CONFIG_BT_DEVICE_NAME) - 1),
};

/* ~1 s connectable advertising keeps the not-connected average current low. */
static const struct bt_le_adv_param adv_param = BT_LE_ADV_PARAM_INIT(
	BT_LE_ADV_OPT_CONN, BT_GAP_ADV_SLOW_INT_MIN, BT_GAP_ADV_SLOW_INT_MAX, NULL);

static void start_adv(void)
{
	int err = bt_le_adv_start(&adv_param, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));

	if (err) {
		LOG_ERR("advertising start failed (%d)", err);
		return;
	}
	LOG_INF("advertising as \"%s\"", CONFIG_BT_DEVICE_NAME);
}

static void connected(struct bt_conn *conn, uint8_t err)
{
	if (err) {
		LOG_WRN("connection failed (0x%02x)", err);
		start_adv();
		return;
	}

	LOG_INF("connected");

	struct bt_le_conn_param *param =
		BT_LE_CONN_PARAM(CONN_INT_MIN, CONN_INT_MAX, CONN_LATENCY, CONN_TIMEOUT);

	(void)bt_conn_le_param_update(conn, param);
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	ARG_UNUSED(conn);
	LOG_INF("disconnected (0x%02x)", reason);
	start_adv();
}

static void le_param_updated(struct bt_conn *conn, uint16_t interval,
			     uint16_t latency, uint16_t timeout)
{
	ARG_UNUSED(conn);
	/* interval is in 1.25 ms units; report the integer millisecond value. */
	LOG_INF("conn params updated: interval ~%u ms, latency %u, timeout %u ms",
		(interval * 5) / 4, latency, timeout * 10);
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
	.connected = connected,
	.disconnected = disconnected,
	.le_param_updated = le_param_updated,
};

#if defined(CONFIG_BT_SMP)
static void pairing_complete(struct bt_conn *conn, bool bonded)
{
	ARG_UNUSED(conn);
	LOG_INF("pairing complete (bonded=%d)", bonded);
}

static void pairing_failed(struct bt_conn *conn, enum bt_security_err reason)
{
	ARG_UNUSED(conn);
	LOG_WRN("pairing failed (reason %d)", reason);
}

static struct bt_conn_auth_info_cb auth_info_cb = {
	.pairing_complete = pairing_complete,
	.pairing_failed = pairing_failed,
};
#endif /* CONFIG_BT_SMP */

int app_bt_init(void)
{
	int err = bt_enable(NULL);

	if (err) {
		LOG_ERR("bt_enable failed (%d)", err);
		return err;
	}

#if IS_ENABLED(CONFIG_SETTINGS)
	settings_load();
#endif

#if defined(CONFIG_BT_SMP)
	(void)bt_conn_auth_info_cb_register(&auth_info_cb);
#endif

	start_adv();
	return 0;
}
