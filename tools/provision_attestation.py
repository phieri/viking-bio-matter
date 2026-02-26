#!/usr/bin/env python3
"""
provision_attestation.py
Upload Device Attestation credentials (DAC, PAI, private key, CD) to a
Viking Bio Matter Pico W device via its USB serial (REPL / provisioning)
interface.

WARNING – TEST-MODE ONLY.
  Private keys are stored unencrypted in LittleFS (flash).
  This is intentional for development and interop testing.
  See PRODUCTION_README.md for production hardening requirements.

Usage:
  # Convert PEM → DER first (if your certs are PEM):
  openssl ec -in dac_key.pem -outform DER -out dac_key.der
  openssl x509 -in dac.pem -outform DER -out dac.der
  openssl x509 -in pai.pem -outform DER -out pai.der

  # Upload to device (device must be running firmware):
  python3 tools/provision_attestation.py \\
      --port /dev/ttyACM0 \\
      --dac  certs/dac.der \\
      --pai  certs/pai.der \\
      --key  certs/dac_key.der \\
      --cd   certs/cd.der       # optional

  # Verify what was stored:
  python3 tools/provision_attestation.py --port /dev/ttyACM0 --verify

The script uses a simple text-based command protocol over the USB serial
REPL:  'PROVISION:<path>:<hex_data>\\n'
The device firmware must handle this command in the main serial task
(or this script can be adapted to use a dedicated provisioning channel).

For an alternative approach that writes directly to a UF2/flash image
(useful for mass provisioning), see the --uf2 option below.
"""

import argparse
import sys
import os
import time
import binascii

try:
    import serial
    HAS_SERIAL = True
except ImportError:
    HAS_SERIAL = False


# ── LittleFS paths written on device ────────────────────────────────────────
# Note: flat paths (no subdirectory) – storage_adapter does not create
# parent directories automatically.
PATH_DAC = "/att_dac"
PATH_PAI = "/att_pai"
PATH_KEY = "/att_key"
PATH_CD  = "/att_cd"

# Maximum sizes (must match firmware ATT_MAX_CERT_SIZE / ATT_MAX_KEY_SIZE)
MAX_CERT = 600
MAX_KEY  = 128
MAX_CD   = 512


# ── Utility ──────────────────────────────────────────────────────────────────

def read_der(path: str, max_size: int, label: str) -> bytes:
    """Read a DER file; auto-convert PEM if needed."""
    with open(path, "rb") as f:
        data = f.read()

    # Crude PEM detection
    if data.lstrip().startswith(b"-----"):
        try:
            from cryptography.hazmat.primitives.serialization import (
                Encoding, PrivateFormat, NoEncryption, load_pem_private_key
            )
            from cryptography.x509 import load_pem_x509_certificate
            from cryptography.hazmat.backends import default_backend

            if b"CERTIFICATE" in data:
                cert = load_pem_x509_certificate(data, default_backend())
                data = cert.public_bytes(Encoding.DER)
            else:
                key = load_pem_private_key(data, password=None,
                                           backend=default_backend())
                data = key.private_bytes(Encoding.DER,
                                         PrivateFormat.TraditionalOpenSSL,
                                         NoEncryption())
            print(f"  [{label}] PEM → DER converted ({len(data)} bytes)")
        except ImportError:
            print("  WARN: 'cryptography' package not found; "
                  "PEM→DER conversion skipped.  "
                  "Install with: pip install cryptography")
        except Exception as e:
            sys.exit(f"ERROR: PEM→DER conversion failed for {path}: {e}")

    if len(data) > max_size:
        sys.exit(f"ERROR: {label} is too large ({len(data)} > {max_size} bytes)")

    print(f"  [{label}] {path} → {len(data)} bytes")
    return data


# ── Serial-based provisioning ────────────────────────────────────────────────

def provision_via_serial(port: str, baud: int, credentials: dict):
    """
    Send PROVISION commands over the USB serial port.

    Protocol (ASCII, line-based):
      Request:  PROVISION:<lfs_path>:<hex_data>\\n
      Response: OK:\\n  or  ERROR:<msg>\\n
    """
    if not HAS_SERIAL:
        sys.exit("ERROR: pyserial not installed.  Run: pip install pyserial")

    print(f"\nConnecting to {port} at {baud} baud …")
    with serial.Serial(port, baud, timeout=5) as ser:
        # Flush startup noise
        time.sleep(1)
        ser.reset_input_buffer()

        for lfs_path, data in credentials.items():
            if data is None:
                continue
            hex_data = binascii.hexlify(data).decode("ascii").upper()
            cmd = f"PROVISION:{lfs_path}:{hex_data}\n"
            print(f"  Writing {lfs_path} ({len(data)} bytes) …", end=" ", flush=True)
            ser.write(cmd.encode("ascii"))
            response = ser.readline().decode("ascii", errors="replace").strip()
            if response.startswith("OK"):
                print("OK")
            else:
                print(f"FAILED ({response!r})")
                print("  Tip: ensure the device firmware handles 'PROVISION:' commands.")

        print("\nProvisioning complete.")
        print("Reboot the device to apply the new credentials.")


# ── Verification (read-back check) ──────────────────────────────────────────

def verify_via_serial(port: str, baud: int):
    """
    Request the device to report stored attestation credential sizes.
    Protocol: ATTCHECK:\\n → Response: DAC:<n> PAI:<n> KEY:<n>\\n
    """
    if not HAS_SERIAL:
        sys.exit("ERROR: pyserial not installed.  Run: pip install pyserial")

    print(f"\nVerifying credentials on {port} …")
    with serial.Serial(port, baud, timeout=5) as ser:
        time.sleep(1)
        ser.reset_input_buffer()
        ser.write(b"ATTCHECK:\n")
        response = ser.readline().decode("ascii", errors="replace").strip()
        print(f"  Device reports: {response}")


# ── Main ─────────────────────────────────────────────────────────────────────

def main():
    parser = argparse.ArgumentParser(
        description="Provision Matter attestation credentials to Pico W device"
    )
    parser.add_argument("--port", default="/dev/ttyACM0",
                        help="USB serial port (default: /dev/ttyACM0)")
    parser.add_argument("--baud", type=int, default=115200,
                        help="Baud rate (default: 115200)")
    parser.add_argument("--dac",  help="Path to DAC cert (DER or PEM)")
    parser.add_argument("--pai",  help="Path to PAI cert (DER or PEM)")
    parser.add_argument("--key",  help="Path to DAC private key (DER or PEM)")
    parser.add_argument("--cd",   help="Path to Certification Declaration (DER, optional)")
    parser.add_argument("--verify", action="store_true",
                        help="Verify stored credentials (do not write)")
    args = parser.parse_args()

    if args.verify:
        verify_via_serial(args.port, args.baud)
        return

    if not any([args.dac, args.pai, args.key]):
        parser.error("At least one of --dac, --pai, --key is required")

    print("Viking Bio Matter – Attestation Credential Provisioning")
    print("=========================================================")
    print("WARNING: TEST-MODE ONLY – private key stored in flash.")
    print("See PRODUCTION_README.md for production guidance.\n")

    credentials = {}

    if args.dac:
        credentials[PATH_DAC] = read_der(args.dac, MAX_CERT, "DAC")
    if args.pai:
        credentials[PATH_PAI] = read_der(args.pai, MAX_CERT, "PAI")
    if args.key:
        credentials[PATH_KEY] = read_der(args.key, MAX_KEY,  "DAC key")
    if args.cd:
        credentials[PATH_CD]  = read_der(args.cd,  MAX_CD,   "CD")

    provision_via_serial(args.port, args.baud, credentials)


if __name__ == "__main__":
    main()
