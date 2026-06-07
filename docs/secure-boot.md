# Secure boot

This node boots through a verified chain rooted in a key we control. MCUboot is
the verification engine; this document is the part that matters in an
interview or a security review — the **trust model, the policy, and the key
lifecycle** around it.

> "From scratch" here means establishing the whole chain and our own Root of
> Trust — key, signing, verification, rollback policy — not hand-rolling a
> crypto bootloader. Re-implementing image verification and swap logic would be
> strictly worse than the audited MCUboot that already ships in the tree; the
> engineering that matters is owning the policy on top of it. That is also what
> real platform-security requirements (e.g. NVIDIA DGX firmware: "Root of Trust",
> SPDM/MCTP attestation) actually ask for — a hardened, measured RoT, not a
> bespoke boot ROM.

## Threat model

What the chain defends against:

| Threat | Defense | Where |
| --- | --- | --- |
| Forged / modified firmware over OTA | Image signature verified before install **and** on every boot | `BOOT_SIGNATURE_TYPE_ECDSA_P256` + `BOOT_VALIDATE_SLOT0` |
| In-place tamper of slot0 (debugger, glitch, flash fault) | Signature re-checked **every boot**, not just after upgrade | `BOOT_VALIDATE_SLOT0=y` |
| Rollback to a known-vulnerable but validly-signed image | Reject images with a lower security counter | `MCUBOOT_DOWNGRADE_PREVENTION[_SECURITY_COUNTER]` + `--security-counter` |
| A bad update bricking the device | Swap mode keeps the prior image; unconfirmed images revert | `MCUBOOT_MODE_SWAP_USING_MOVE` + `boot_write_img_confirmed()` |

Explicitly **out of scope** for this stage (see [Deferred](#deferred-hardening),
each with the config to close it):

- An attacker who can run code with the device's flash/debug access and also
  holds the private signing key. The dev key in `keys/` is public — it is for
  reproducible builds, not protection (see [`keys/README.md`](../keys/README.md)).
- Physical attacks the nRF52840 can't resist without provisioning: APPROTECT
  must be locked to stop a debugger from reading/replacing flash, and the
  rollback counter is only truly monotonic with a hardware non-volatile counter.
- Confidentiality of the image (no encryption here; the firmware is signed, not
  secret).

## The chain

```
   ┌─────────────────────────────────────────────────────────────────┐
   │ Root of Trust: ECDSA P-256 public key compiled into MCUboot       │
   │  (immutable bootloader; private half lives off-device — keys/)     │
   └───────────────────────────────┬───────────────────────────────────┘
                                    │ every boot
                                    ▼
   verify slot0 signature ── fail ─► do not execute
        │ pass
        ▼
   check security counter ≥ stored ── lower ─► reject update (anti-rollback)
        │ ok
        ▼
   hand off to application ──► app confirms itself (boot_write_img_confirmed)
        │ not confirmed by next reset
        ▼
   MCUboot reverts to the previous, already-trusted image
```

1. **Root of Trust.** `west build --sysbuild` builds MCUboot with our P-256
   *public* key embedded. That key, in the immutable bootloader, is the trust
   anchor — nothing it can't trace back to that key runs.
2. **Authenticity, every boot.** `CONFIG_BOOT_VALIDATE_SLOT0=y` re-verifies the
   slot0 signature on every reset. This is the line between *secure boot* and
   *OTA with a checksum*: integrity is enforced continuously, not just at
   install time.
3. **Anti-rollback.** The signed image carries a monotonic **security counter**
   TLV (`--security-counter`, set in [`sysbuild/ble.conf`](../sysbuild/ble.conf));
   MCUboot refuses any image whose counter is lower than the running one. Keying
   off a dedicated counter rather than the marketing version means ordinary
   `0.1.0 → 0.2.0` bumps don't trip rollback protection — you raise the counter
   only for releases that fix a vulnerability you must never roll back past.
4. **Fail-safe updates.** Swap mode retains the previous image. A freshly
   DFU'd image boots *for test*; `main.c` calls `boot_write_img_confirmed()`
   once it reaches a healthy boot. If it never confirms, MCUboot reverts on the
   next reset. Revert is not a rollback: the previous image's counter isn't
   lower, so anti-rollback and fail-safe coexist.

`main.c` reads the image header back at startup and logs
`verified image vX.Y.Z+B` — reaching that log line is runtime proof the
signature check passed.

## Configuration map

| Property | Symbol | File |
| --- | --- | --- |
| Build MCUboot + sign app | `SB_CONFIG_BOOTLOADER_MCUBOOT` | `sysbuild.conf` |
| Signature algorithm (P-256) | `SB_CONFIG_BOOT_SIGNATURE_TYPE_ECDSA_P256` | `sysbuild.conf` |
| Root-of-Trust key path (default) | `BOOT_SIGNATURE_KEY_FILE` default | `Kconfig.sysbuild` |
| Swap mode (revert-capable) | `SB_CONFIG_MCUBOOT_MODE_SWAP_USING_MOVE` | `sysbuild.conf` |
| Verify slot0 every boot | `CONFIG_BOOT_VALIDATE_SLOT0` | `sysbuild/mcuboot.conf` |
| Anti-rollback policy | `CONFIG_MCUBOOT_DOWNGRADE_PREVENTION[_SECURITY_COUNTER]` | `sysbuild/mcuboot.conf` |
| Per-image security counter | `CONFIG_MCUBOOT_EXTRA_IMGTOOL_ARGS="--security-counter N"` | `sysbuild/ble.conf` |

> The key path must be **absolute**: the bootloader image resolves a relative
> key against its own source/conf dir while the application image resolves it
> against the west workspace topdir, so no single relative path satisfies both.
> [`zephyr/module.yml`](../zephyr/module.yml) registers this repo as a Zephyr
> module so `$(ZEPHYR_BLE_ENV_SENSOR_MODULE_DIR)` gives an absolute path both
> images agree on — the same trick MCUboot uses for its own default key. That
> macro only expands inside a Kconfig *source* (not in a `.conf` value), so the
> key path is set as a Kconfig default in [`Kconfig.sysbuild`](../Kconfig.sysbuild)
> rather than as `SB_CONFIG_BOOT_SIGNATURE_KEY_FILE` in `sysbuild.conf`.

## Verifying it

```sh
# Build the signed image. Signing FAILS THE BUILD if the key can't be found,
# so a green build already proves the RoT key resolved and signing ran.
west build -p always -b nrf52840dk/nrf52840 --sysbuild ble

# Cryptographically verify the produced image against our public key.
imgtool verify -k keys/dev-signing-ec-p256.pem build/ble/zephyr/zephyr.signed.bin

# Negative test: a different key must FAIL to verify (proves it's the signature,
# not just a hash, doing the work).
keys/generate-signing-key.sh /tmp/other.pem
imgtool verify -k /tmp/other.pem build/ble/zephyr/zephyr.signed.bin   # -> verification fails
```

CI runs the build and the `imgtool verify` step on every push
([`.github/workflows/ci.yml`](../.github/workflows/ci.yml)), so the signature
chain is regression-tested without hardware.

On hardware, the full chain (every-boot verification, test/confirm/revert,
counter rejection) is exercised over the BLE SMP transport — see the OTA section
of the [README](../README.md). That demo is pending a board, like the bench
power numbers.

## Deferred hardening

Real next steps, each with the lever that closes it. Left off by default so the
CI build stays deterministic and honest about what's actually been verified.

- **Lock the SoC (highest priority on real hardware).** Enable nRF52840
  APPROTECT so a debugger cannot read or rewrite flash. Without this, secure
  boot is bypassable with a wire — the signing policy assumes flash can't be
  freely rewritten.
- **Hardware-monotonic rollback counter.** The software security counter stops
  an OTA from installing an older image, but a debugger could still rewrite the
  counter. A true non-volatile monotonic counter (`MCUBOOT_HW_ROLLBACK_PROT`)
  needs a backend the bare nRF52840 + upstream MCUboot doesn't provide — it
  comes with NSIB/TF-M on Nordic's nRF Connect SDK. The version-counter scheme
  here is the right shape; the hardware counter is the productization step.
- **Measured boot → attestation.** MCUboot can record a measurement (image
  hash, security counter, boot status) into a shared memory region for the
  application or a TF-M secure service to read and report — the device-side half
  of an **SPDM/MCTP** attestation exchange. On this platform it needs a retained
  RAM region shared by both images:

  ```ini
  # sysbuild/mcuboot.conf
  CONFIG_MEASURED_BOOT=y
  CONFIG_BOOT_SHARE_BACKEND_RETENTION=y
  ```

  plus a `zephyr,retention` region (devicetree) carved from SRAM and declared
  identically in the MCUboot and application overlays, and an app-side reader.
  It's left out of the default build because the carved-RAM overlay can't be
  validated without flashing — including an unverified linker/DT change would be
  the kind of "looks right, silently broken" risk this repo avoids elsewhere.
- **Image encryption** (`CONFIG_BOOT_ENCRYPTION`) if the firmware must stay
  confidential in the secondary slot / in transit. Not needed for an open sensor
  node; signed-but-not-secret is the right default here.

## Adjacent tracks (not in this repo)

Two firmware-security/RTOS efforts that belong in their own repos, noted so the
scope of *this* one is clear:

- **FreeRTOS port to a custom RV32IM SoC** — context switch, machine-timer tick,
  PLIC integration. A different target (custom RISC-V silicon, not the nRF52840)
  and a different RTOS than this Zephyr node; it earns the "ported an RTOS to
  silicon I designed" story on its own, separate from secure boot.
- **Bench power characterization with a PPK2** — turns the design-target current
  figures in the README power table into measured µA. Needs the board + meter on
  a desk, so it lands with the OTA hardware demo, not in CI.
