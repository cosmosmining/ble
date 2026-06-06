#include "ess_format.h"

/*
 * Zephyr sensor values are val1 + val2 * 1e-6. The ESS conversions below scale
 * into the fixed-point integer units defined by the GATT characteristics and
 * saturate to the wire type's range.
 */

int16_t ess_encode_temperature(const struct sensor_value *v)
{
	/* 0.01 degC units */
	int32_t hundredths = v->val1 * 100 + v->val2 / 10000;

	if (hundredths > INT16_MAX) {
		hundredths = INT16_MAX;
	} else if (hundredths < INT16_MIN) {
		hundredths = INT16_MIN;
	}
	return (int16_t)hundredths;
}

uint16_t ess_encode_humidity(const struct sensor_value *v)
{
	/* 0.01 % units */
	int32_t hundredths = v->val1 * 100 + v->val2 / 10000;

	if (hundredths < 0) {
		hundredths = 0;
	} else if (hundredths > UINT16_MAX) {
		hundredths = UINT16_MAX;
	}
	return (uint16_t)hundredths;
}

uint32_t ess_encode_pressure(const struct sensor_value *v)
{
	/* Sensor pressure is in kPa; ESS wants 0.1 Pa units (kPa * 10000). */
	int64_t tenths_pa = (int64_t)v->val1 * 10000 + (int64_t)v->val2 / 100;

	if (tenths_pa < 0) {
		tenths_pa = 0;
	} else if (tenths_pa > UINT32_MAX) {
		tenths_pa = UINT32_MAX;
	}
	return (uint32_t)tenths_pa;
}
