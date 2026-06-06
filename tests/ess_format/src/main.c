#include <zephyr/ztest.h>

#include "ess_format.h"

ZTEST_SUITE(ess_format, NULL, NULL, NULL, NULL, NULL);

ZTEST(ess_format, test_temperature)
{
	struct sensor_value v = { .val1 = 23, .val2 = 450000 }; /* 23.45 C */

	zassert_equal(ess_encode_temperature(&v), 2345, "got %d",
		      ess_encode_temperature(&v));

	struct sensor_value neg = { .val1 = -5, .val2 = -120000 }; /* -5.12 C */

	zassert_equal(ess_encode_temperature(&neg), -512, "got %d",
		      ess_encode_temperature(&neg));
}

ZTEST(ess_format, test_humidity)
{
	struct sensor_value v = { .val1 = 50, .val2 = 0 }; /* 50.00 % */

	zassert_equal(ess_encode_humidity(&v), 5000, "got %u",
		      ess_encode_humidity(&v));
}

ZTEST(ess_format, test_pressure)
{
	struct sensor_value v = { .val1 = 101, .val2 = 325000 }; /* 101.325 kPa */

	/* 101325 Pa expressed in 0.1 Pa units. */
	zassert_equal(ess_encode_pressure(&v), 1013250U, "got %u",
		      ess_encode_pressure(&v));
}

ZTEST(ess_format, test_humidity_saturates_low)
{
	struct sensor_value v = { .val1 = -1, .val2 = 0 };

	zassert_equal(ess_encode_humidity(&v), 0, "negative humidity must clamp");
}
