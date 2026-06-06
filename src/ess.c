#include <zephyr/kernel.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/logging/log.h>

#include "ess.h"
#include "ess_format.h"

LOG_MODULE_REGISTER(ess, LOG_LEVEL_INF);

/* Latest values, already in little-endian ESS wire format. */
static int16_t  temp_le;
static uint16_t humidity_le;
static uint32_t pressure_le;

static bool temp_ntf;
static bool humidity_ntf;
static bool pressure_ntf;

static ssize_t read_le(struct bt_conn *conn, const struct bt_gatt_attr *attr,
		       void *buf, uint16_t len, uint16_t offset, uint16_t vlen)
{
	return bt_gatt_attr_read(conn, attr, buf, len, offset, attr->user_data, vlen);
}

static ssize_t read_u16(struct bt_conn *conn, const struct bt_gatt_attr *attr,
			void *buf, uint16_t len, uint16_t offset)
{
	return read_le(conn, attr, buf, len, offset, sizeof(uint16_t));
}

static ssize_t read_u32(struct bt_conn *conn, const struct bt_gatt_attr *attr,
			void *buf, uint16_t len, uint16_t offset)
{
	return read_le(conn, attr, buf, len, offset, sizeof(uint32_t));
}

static void temp_ccc(const struct bt_gatt_attr *attr, uint16_t value)
{
	temp_ntf = (value == BT_GATT_CCC_NOTIFY);
}

static void humidity_ccc(const struct bt_gatt_attr *attr, uint16_t value)
{
	humidity_ntf = (value == BT_GATT_CCC_NOTIFY);
}

static void pressure_ccc(const struct bt_gatt_attr *attr, uint16_t value)
{
	pressure_ntf = (value == BT_GATT_CCC_NOTIFY);
}

BT_GATT_SERVICE_DEFINE(ess_svc,
	BT_GATT_PRIMARY_SERVICE(BT_UUID_ESS),

	BT_GATT_CHARACTERISTIC(BT_UUID_TEMPERATURE,
			       BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
			       BT_GATT_PERM_READ, read_u16, NULL, &temp_le),
	BT_GATT_CCC(temp_ccc, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),

	BT_GATT_CHARACTERISTIC(BT_UUID_HUMIDITY,
			       BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
			       BT_GATT_PERM_READ, read_u16, NULL, &humidity_le),
	BT_GATT_CCC(humidity_ccc, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),

	BT_GATT_CHARACTERISTIC(BT_UUID_PRESSURE,
			       BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
			       BT_GATT_PERM_READ, read_u32, NULL, &pressure_le),
	BT_GATT_CCC(pressure_ccc, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
);

/*
 * Indices of the characteristic VALUE attributes within ess_svc.attrs[]:
 *   [0] primary service
 *   [1] temperature declaration   [2] temperature value   [3] temperature CCC
 *   [4] humidity declaration      [5] humidity value      [6] humidity CCC
 *   [7] pressure declaration      [8] pressure value      [9] pressure CCC
 */
#define ATTR_TEMP     2
#define ATTR_HUMIDITY 5
#define ATTR_PRESSURE 8

void ess_update_temperature(const struct sensor_value *v)
{
	temp_le = (int16_t)sys_cpu_to_le16((uint16_t)ess_encode_temperature(v));
	if (temp_ntf) {
		bt_gatt_notify(NULL, &ess_svc.attrs[ATTR_TEMP], &temp_le, sizeof(temp_le));
	}
}

void ess_update_humidity(const struct sensor_value *v)
{
	humidity_le = sys_cpu_to_le16(ess_encode_humidity(v));
	if (humidity_ntf) {
		bt_gatt_notify(NULL, &ess_svc.attrs[ATTR_HUMIDITY], &humidity_le,
			       sizeof(humidity_le));
	}
}

void ess_update_pressure(const struct sensor_value *v)
{
	pressure_le = sys_cpu_to_le32(ess_encode_pressure(v));
	if (pressure_ntf) {
		bt_gatt_notify(NULL, &ess_svc.attrs[ATTR_PRESSURE], &pressure_le,
			       sizeof(pressure_le));
	}
}
