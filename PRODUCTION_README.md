# Production Hardening Requirements

> This document applies to the Device Attestation and CASE (Sigma) features
> introduced in the `copilot/add-device-attestation-support` branch.

---

## ⚠️ CRITICAL: This is TEST-MODE Code

The attestation and CASE implementation in this PR is explicitly a
**prototype for development and interoperability testing only**.

**DO NOT deploy this firmware in production without completing the steps below.**

---

## What Makes This Test-Mode

| Item | Test-mode behavior | Risk |
|---|---|---|
| **Private key in flash** | DAC private key stored unencrypted in LittleFS (`/certs/dac_key.der`) | Key can be extracted via SWD/JTAG or by reading flash |
| **Test PAA/DAC/PAI** | Any self-signed test certs work | Not recognized by production Matter controllers |
| **No Sigma3 NOC chain verify** | `case_handle_sigma3()` skips full X.509 chain verification | A rogue initiator with any valid TBE3 ciphertext could establish a session |
| **Timestamp = 0** | `attestation_generate_attestation_tlv()` uses timestamp 0 | Controllers may warn or reject; certificates may appear expired |
| **No secure boot** | Firmware can be replaced without attestation | Firmware tampering is undetected |
| **No CASE resumption** | Each connection re-runs full Sigma exchange | Performance impact for battery-powered controllers |

---

## Required Steps Before Production

### 1. Secure Element for Private Key Storage

Replace the LittleFS-based key storage with a secure element:
- Recommended: **Microchip ATECC608A/B** (compatible with Pico via I²C)
- Add a `crypto_se.h` abstraction (placeholder already in the design)
- The secure element stores the DAC private key in hardware
- Signing is performed inside the element; the private key never leaves it

```c
/* Future: crypto_se.h interface */
int crypto_se_sign(const uint8_t *hash, size_t hash_len,
                   uint8_t *sig, size_t *sig_len);
```

### 2. Per-Device DAC from Vendor PKI

- Each device must have a **unique DAC** signed by your PAI.
- Your PAI must be signed by a **CSA-approved PAA**.
- Enroll in the [CSA Matter Certification](https://csa-iot.org/certification/) program.
- Provision unique DAC/PAI during manufacturing (not via USB serial).

### 3. Full NOC Chain Verification in CASE Sigma3

Enable `MBEDTLS_X509_CRT_PARSE_C` in `mbedtls_config.h` and implement:
```c
/* In case_handle_sigma3(): */
mbedtls_x509_crt noc_cert, rcac_cert;
/* Parse initiator NOC from TBE3 Tag 1 */
/* Load RCAC from certificate_store_load_rcac() */
/* Verify noc_cert against rcac_cert */
/* Extract public key from NOC */
/* Verify Sigma3 signature (TBE3 Tag 3) with that public key */
```

### 4. Real Timestamps

Use an RTC chip (e.g., DS3231) or NTP synchronization to provide real
Unix timestamps in `attestation_generate_attestation_tlv()`.

### 5. Secure Boot

Enable the RP2040 secure boot feature:
- Generate a signing key pair.
- Sign firmware images with `picotool sign`.
- Burn the public key OTP fuses.
- The bootloader verifies firmware signature on every boot.

### 6. OTA Firmware Updates with Signature Verification

Implement OTA using Matter's BDX (Bulk Data Exchange) protocol with
firmware image signature verification before applying updates.

### 7. Remove Test Provisioning Command Handler

Remove or restrict the `PROVISION:` serial command handler in the main
firmware before shipping.  This handler is only appropriate for development.

### 8. Factory Provisioning Flow

Replace `tools/provision_attestation.py` with a secure manufacturing-floor
provisioning process that:
- Uses a Hardware Security Module (HSM) for key generation.
- Programs credentials via a secure, authenticated channel (not plain serial).
- Records serial/certificate associations in a production database.

---

## Certification Requirements Summary

To achieve Matter certification, you must:
- [ ] Obtain a VID/PID from the CSA.
- [ ] Obtain a PAI signed by a CSA-approved PAA.
- [ ] Generate per-device DACs signed by your PAI.
- [ ] Pass the Matter Test Harness (TH) test suite.
- [ ] Undergo a certified test lab evaluation.
- [ ] Complete CSA membership and certification agreements.

See: https://csa-iot.org/developer-resource/specifications-download-request/

---

## Security Contact

To report security vulnerabilities, open a private security advisory at:
https://github.com/phieri/viking-bio-matter/security/advisories/new
