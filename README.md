# BLE Environmental Sensing Node

A power-tuned BLE peripheral on the **Nordic nRF52840** running **Zephyr RTOS**.
It samples an environmental sensor on a **data-ready (DRDY) interrupt**, exposes
readings through the standard **Environmental Sensing GATT Service (ESS, 0x181A)**,
and builds an **MCUboot OTA-updatable image** via sysbuild.

## Architecture

| Decision           | Choice                                                          |
| ------------------ | -------------------------------------------------------------- |
| SoC / board        | nRF52840 (nRF52840 DK); CI also builds nRF52833 DK             |
| RTOS               | Zephyr v4.1.0 (pinned in `west.yml`)                           |
| Sensor             | ST **HTS221** (temperature + humidity) over I²C                |
| Sampling pattern   | Sensor **DRDY interrupt** → trigger → dedicated sampling thread |
| Driver             | Zephyr `sensor_*` API, selected via the `env-sensor` DT alias  |
| GATT               | Environmental Sensing Service: Temperature / Humidity / Pressure |
| Connectivity power | ~1 s advertising; 100–150 ms connection interval, latency 4    |
| Liveness           | Hardware watchdog + check-in supervisor thread                 |
| DFU                | MCUboot (sysbuild) + MCUmgr SMP-over-BLE transport             |

> Pressure (0x2A6D) is wired through the service and the sampling loop already;
> populating it is a drop-in second part (e.g. LPS22HB on the same I²C bus).

### Data path

```
   ┌────────┐ DRDY IRQ ┌───────────────┐  k_sem   ┌─────────────────┐ sensor_*  ┌─────────┐ notify
   │ HTS221 │ ───────► │ sensor trigger │ ───────► │ sampling thread │ ───────►  │ ESS     │ ─────► central
   │ (I²C)  │          │ thread (driver)│  give    │  (prio 7)       │  fetch    │ (GATT)  │
   └────────┘          └───────────────┘          └─────────────────┘           └─────────┘
```

The DRDY GPIO ISR (inside the driver) wakes the sensor subsystem's trigger
thread, which calls our handler. The handler does the minimum — `k_sem_give` —
and the dedicated sampling thread performs the blocking I²C read and the GATT
notification. Nothing touches the I²C bus or GATT from interrupt context.

## Modules

| File              | Responsibility                                              |
| ----------------- | ----------------------------------------------------------- |
| `src/main.c`      | Start order: watchdog → Bluetooth → sampling                |
| `src/bluetooth.c` | Controller/host enable, advertising, power-tuned conn params |
| `src/ess.c`       | ESS GATT service, CCC subscriptions, notifications          |
| `src/ess_format.c`| Pure sensor-value → ESS fixed-point conversions (unit tested) |
| `src/sampling.c`  | DRDY trigger install + sampling thread + watchdog check-in  |
| `src/watchdog.c`  | HW watchdog + multi-task check-in supervisor                |

## Hardware

- **nRF52840 DK**
- **HTS221** breakout on the Arduino I²C header (`arduino_i2c`, addr `0x5F`),
  DRDY wired to `P0.03`. See [`app.overlay`](app.overlay).

## Power profile

Supply current per power state (to be filled from bench measurements):

| State                         | Current | Notes                              |
| ----------------------------- | ------: | ---------------------------------- |
| System OFF (RAM retention)    |   _TBD_ | deepest sleep, wake on GPIO        |
| Advertising (~1 s interval)   |   _TBD_ | not connected                      |
| Connected (idle)              |   _TBD_ | 100–150 ms interval, latency 4     |
| Connected (sampling + notify) |   _TBD_ | per-sample peak                    |

## Build & flash

This is a Zephyr **T2 (application = manifest repo)** workspace.

```sh
# one-time workspace setup
west init -m https://github.com/cosmosmining/ble --mr claude/kind-carson-BoClC my-ws
cd my-ws
west update
west zephyr-export

# build + flash the application
west build -p always -b nrf52840dk/nrf52840 ble
west flash
```

## OTA / DFU

The `--sysbuild` image bundles MCUboot **and** the MCUmgr **SMP transport over
BLE**, so a new image can be uploaded wirelessly and swapped in by the
bootloader.

```sh
# build MCUboot + a signed, DFU-capable application image
west build -p always -b nrf52840dk/nrf52840 --sysbuild ble
west flash                                  # initial flash: MCUboot + slot0

# later: push an update over BLE with mcumgr
PEER="peer_name='ble-env-sensor'"
mcumgr --conntype ble --connstring "$PEER" image upload build/ble/zephyr/zephyr.signed.bin
mcumgr --conntype ble --connstring "$PEER" image list
mcumgr --conntype ble --connstring "$PEER" reset
```

MCUboot runs the uploaded image in test mode; once the application reaches a
healthy boot it calls `boot_write_img_confirmed()`, otherwise MCUboot reverts on
the next reset. The SMP/DFU configuration lives in
[`sysbuild/ble.conf`](sysbuild/ble.conf) and is applied only to the OTA build.
A demo recording lands once it's exercised on hardware.

## Testing

```sh
west twister -T tests -p native_sim
```

`tests/ess_format` unit-tests the ESS conversion math on the host (no hardware).

## CI

[`.github/workflows/ci.yml`](.github/workflows/ci.yml) runs three jobs on every
push using the official Zephyr toolchain:

- **build** — firmware across a board matrix (nRF52840 DK, nRF52833 DK)
- **build-ota** — MCUboot/sysbuild signed image for the nRF52840 DK
- **twister** — host unit tests on `native_sim`

## Roadmap

- [x] Buildable scaffold + board-matrix CI
- [x] HTS221 devicetree overlay (DRDY-capable, `env-sensor` alias)
- [x] BLE peripheral: advertising + power-tuned connection parameters
- [x] DRDY-triggered sampling thread (ISR → thread hand-off)
- [x] Environmental Sensing GATT service + notifications
- [x] Watchdog supervisor
- [x] MCUboot / sysbuild OTA image
- [x] BLE SMP (MCUmgr) DFU transport + image-confirm
- [ ] Persist bonds/CCC (settings + NVS)
- [ ] OTA demo recording (needs hardware)
- [ ] Add LPS22HB for pressure channel
- [ ] Bench power measurements → fill the power table

## Design notes

- **ISR vs thread context** — the DRDY callback only does `k_sem_give`; the I²C
  read and GATT notify run in the sampling thread. `k_sem_give` is ISR-safe;
  the I²C transfer (which blocks on a mutex) is not.
- **DMA usage** — for HTS221's few-byte reads at ≤1 Hz, PIO I²C is cheaper than
  DMA setup; on nRF, EasyDMA also requires buffers in RAM (not flash/UICR).
- **Priority inversion** — the I²C bus mutex uses priority inheritance so the
  BLE controller is not blocked behind a low-priority bus user.
- **Watchdog design** — the supervisor only pets the HW WDT once *every*
  registered task has checked in, so a single hung task forces a reset.
