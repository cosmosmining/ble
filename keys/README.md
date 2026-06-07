# Signing keys

This directory holds the **Root-of-Trust signing key** for the secure-boot
chain. See [`docs/secure-boot.md`](../docs/secure-boot.md) for how it fits in.

## `dev-signing-ec-p256.pem`

ECDSA NIST P-256 private key. MCUboot embeds its **public** half as the
device's Root of Trust and verifies every image against it; `west sign` uses the
**private** half to sign the application image during `west build --sysbuild`.

> [!WARNING]
> **This is a development key, committed on purpose so the repo builds a
> verifiable signed image out of the box. It provides ZERO real security — the
> private key is public.** Anyone can sign an image MCUboot will accept. Do not
> ship a product whose Root of Trust is this key.

### For production

1. Generate a fresh key whose private half **never** touches the repo — ideally
   non-exportable in an HSM / cloud KMS (PKCS#11, AWS/GCP KMS, YubiHSM, …):

   ```sh
   ./keys/generate-signing-key.sh keys/prod-signing-ec-p256.pem   # local/offline
   # or have your HSM/KMS generate it and export only the public key
   ```

2. Point the build at it **without committing the private key** — override the
   Kconfig at build time rather than editing `sysbuild.conf`:

   ```sh
   west build -p always -b nrf52840dk/nrf52840 --sysbuild ble -- \
     -DSB_CONFIG_BOOT_SIGNATURE_KEY_FILE=\"$(pwd)/keys/prod-signing-ec-p256.pem\"
   ```

3. Keep the matching **public** key under version control so anyone can audit
   which RoT a device trusts (`openssl ec -in <priv> -pubout`).

### Key rotation

The trusted public key is compiled into the immutable bootloader, so rotating
it means reflashing MCUboot — it is **not** an OTA-only operation. Plan for it:
stage a bootloader that trusts both the old and new key, migrate the fleet, then
retire the old key. Treat a leaked production private key as a full RoT
compromise and rotate immediately.
