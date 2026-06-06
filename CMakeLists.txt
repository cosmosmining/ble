cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(ble_env_sensor LANGUAGES C)

target_sources(app PRIVATE
  src/main.c
  src/bluetooth.c
  src/ess.c
  src/ess_format.c
  src/sampling.c
  src/watchdog.c
)
