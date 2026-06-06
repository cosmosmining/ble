#ifndef APP_ESS_FORMAT_H_
#define APP_ESS_FORMAT_H_

#include <stdint.h>
#include <zephyr/drivers/sensor.h>

/*
 * Pure conversions from Zephyr sensor values to GATT Environmental Sensing
 * representations. Kept free of Bluetooth dependencies so they can be unit
 * tested on the host (see tests/ess_format).
 *
 *   Temperature (0x2A6E): sint16, 0.01 degC
 *   Humidity    (0x2A6F): uint16, 0.01 %
 *   Pressure    (0x2A6D): uint32, 0.1 Pa
 */
int16_t  ess_encode_temperature(const struct sensor_value *v);
uint16_t ess_encode_humidity(const struct sensor_value *v);
uint32_t ess_encode_pressure(const struct sensor_value *v);

#endif /* APP_ESS_FORMAT_H_ */
