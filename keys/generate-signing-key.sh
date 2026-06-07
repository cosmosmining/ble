#!/usr/bin/env sh
# Generate an ECDSA NIST P-256 signing key for the MCUboot secure-boot chain,
# in the same SEC1 PEM format MCUboot's own sample keys use.
#
# Usage:
#   keys/generate-signing-key.sh [output.pem]
#
# Default output is keys/dev-signing-ec-p256.pem. The matching public key (the
# value embedded in MCUboot as the Root of Trust) is printed for auditing.
#
# For production prefer a key generated *inside* an HSM/KMS so the private half
# is never exportable; see keys/README.md.
set -eu

out="${1:-keys/dev-signing-ec-p256.pem}"

if [ -e "$out" ]; then
	printf 'refusing to overwrite existing key: %s\n' "$out" >&2
	exit 1
fi

openssl ecparam -name prime256v1 -genkey -noout -out "$out"
chmod 600 "$out"

printf 'wrote %s\n\npublic key (Root of Trust embedded in MCUboot):\n' "$out"
openssl ec -in "$out" -pubout 2>/dev/null
