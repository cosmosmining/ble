#ifndef APP_WATCHDOG_H_
#define APP_WATCHDOG_H_

/* Configure the hardware watchdog and start the supervisor thread. */
int app_watchdog_init(void);

/* Register a supervised task; returns a non-negative id, or negative on error.
 * The returned id is passed to watchdog_task_feed(). */
int watchdog_task_register(const char *name);

/* Record that a supervised task is alive this window. The supervisor only pets
 * the hardware watchdog once every registered task has checked in. */
void watchdog_task_feed(int task_id);

#endif /* APP_WATCHDOG_H_ */
