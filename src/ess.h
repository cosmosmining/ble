#ifndef APP_ESS_H_
#define APP_ESS_H_

#include <zephyr/drivers/sensor.h>

/*
 * Environmental Sensing Service (0x181A) front-end.
 *
 * Each helper stores the freshly sampled value in ESS wire format and, if a
 * client has subscribed, sends a notification. Call from thread context.
 */
void ess_update_temperature(const struct sensor_value *v);
void ess_update_humidity(const struct sensor_value *v);
void ess_update_pressure(const struct sensor_value *v);

#endif /* APP_ESS_H_ */
