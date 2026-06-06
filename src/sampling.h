#ifndef APP_SAMPLING_H_
#define APP_SAMPLING_H_

/* Bring up the environmental sensor, install its data-ready trigger, and start
 * the sampling thread. Returns 0 on success, negative errno otherwise. */
int app_sampling_init(void);

#endif /* APP_SAMPLING_H_ */
