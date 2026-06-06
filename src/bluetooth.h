#ifndef APP_BLUETOOTH_H_
#define APP_BLUETOOTH_H_

/* Enable the controller + host, register connection callbacks, and start
 * power-tuned connectable advertising. Returns 0 on success. */
int app_bt_init(void);

#endif /* APP_BLUETOOTH_H_ */
