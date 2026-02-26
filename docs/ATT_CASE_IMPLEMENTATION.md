# ATT & CASE Implementation Notes

> **TEST-MODE ONLY** – This implementation is a prototype intended for development
> and interoperability testing.  See `PRODUCTION_README.md` for production hardening.

---

## Overview

This document describes the test-mode Device Attestation and CASE (Certificate-Authenticated
Session Establishment) implementation added to the Viking Bio Matter firmware.

The changes enable the complete Matter commissioning flow:
```
BLE PASE → WiFi join → Device Attestation → CASE Sigma → NOC provisioning
```

---

## New Files

| File | Purpose |
|---|---|
| `src/matter_minimal/security/attestation.h/.c` | Device attestation – cert chain retrieval, TLV generation, ECDSA signing |
| `src/matter_minimal/security/case.h/.c` | CASE Sigma1/2/3 responder – ECDHE, HKDF, AES-CCM, session registration |
| `src/matter_minimal/security/certificate_store.h/.c` | Persistent NOC/ICAC/RCAC storage in LittleFS |
| `platform/pico_w_chip_port/storage_attestation.c` | Platform glue: read/write credential blobs via storage_adapter |
| `host/attestation_verifier.c` | Host CLI: verify DAC→PAI→PAA chain and attestation signature |
| `tools/provision_attestation.py` | Provisioning helper: upload DER certs and key to device via serial |
| `docs/tests/ATT_TEST.md` | End-to-end test instructions |
| `PRODUCTION_README.md` | Security disclaimer and production hardening checklist |

---

## Architecture

### Credential Storage (LittleFS)

```
/certs/dac.der       ← Device Attestation Certificate (DER)
/certs/pai.der       ← Product Attestation Intermediate cert (DER)
/certs/dac_key.der   ← DAC private key (EC PKCS#8 or SEC.1 DER)  [TEST ONLY]
/certs/cd.der        ← Certification Declaration (optional)
/certs/noc.der       ← Node Operational Certificate (written after AddNOC)
/certs/icac.der      ← Intermediate CA cert (optional)
/certs/rcac.der      ← Root CA cert (optional)
```

### Attestation Flow

```
Controller                          Device
    |                                 |
    |--- AttestationRequest(nonce) -->|
    |                                 | attestation_generate_attestation_tlv()
    |                                 |   → TLV { CD, nonce, timestamp }
    |                                 | attestation_sign_challenge(TLV)
    |                                 |   → ECDSA-P256 signature (DAC key)
    |<-- AttestationResponse ---------|
    |    (cert_chain, elements, sig)  |
    |                                 |
    | [Controller verifies DAC→PAI→PAA chain and signature]
```

### CASE Sigma Flow

```
Controller (Initiator)              Device (Responder)
    |                                    |
    |--- Sigma1 (Ieph_pub, nonce) ------>|
    |                                    | case_handle_sigma1()
    |                                    |  1. Parse Ieph_pub
    |                                    |  2. Generate Reph keypair (P-256)
    |                                    |  3. ECDH(Reph_priv, Ieph_pub) → Z
    |                                    |  4. HKDF(Z, T1) → I2R, R2I keys
    |                                    |  5. Encrypt TBE2 (AES-128-CCM)
    |<-- Sigma2 (Reph_pub, TBE2) --------|
    |                                    |
    |--- Sigma3 (TBE3) ----------------->|
    |                                    | case_handle_sigma3()
    |                                    |  1. Decrypt TBE3
    |                                    |  2. [Test-mode: skip full NOC verify]
    |                                    |  3. Store NOC from TBE3
    |                                    |  4. session_create(I2R key)
    |                                    | → CASE session established
```

---

## Cryptographic Primitives Used

All use the mbedTLS instance already present in the project (`pico_mbedtls`):

| Primitive | mbedTLS API | Used for |
|---|---|---|
| ECDHE P-256 | `mbedtls_ecp_gen_keypair`, `mbedtls_ecp_mul` | Sigma ephemeral keys, shared secret |
| HKDF-SHA256 | `mbedtls_hkdf` | Session key derivation (I2R, R2I, TBE keys) |
| AES-128-CCM | `mbedtls_ccm_encrypt_and_tag`, `mbedtls_ccm_auth_decrypt` | TBE2/TBE3 encryption |
| ECDSA-P256 | `mbedtls_pk_sign` | Attestation challenge signing |
| SHA-256 | `mbedtls_sha256` | Transcript hash, challenge hash |
| PK parsing | `mbedtls_pk_parse_key` | Load DAC private key from DER |

---

## Message Routing

New CASE opcode constants in `src/matter_minimal/codec/message_codec.h`:

```c
#define MATTER_SC_OPCODE_CASE_SIGMA1      0x30
#define MATTER_SC_OPCODE_CASE_SIGMA2      0x31
#define MATTER_SC_OPCODE_CASE_SIGMA3      0x32
#define MATTER_SC_OPCODE_CASE_SIGMA2_RESUME 0x33
```

`matter_protocol.c` routes Sigma1/2/3 opcodes to `case_handle_sigma*()` functions
within the existing `PROTOCOL_SECURE_CHANNEL` handler, alongside PASE.

---

## Test-Mode Limitations

| Area | Test-mode behavior | Production requirement |
|---|---|---|
| Private key storage | Flash (LittleFS) | Secure element (e.g., ATECC608A) |
| Sigma3 NOC verification | Accepts any valid TBE3 decryption | Full X.509 chain verify against RCAC |
| Timestamp | Hardcoded 0 | Real-time clock or NTP |
| CASE session resumption | Not supported | Implement Sigma2_resume |
| Certification Declaration | Optional / empty stub | Real CD from Matter certification |
| PAA | Test cert | CSA-issued PAA for production |

---

## How to Test

See `docs/tests/ATT_TEST.md` for step-by-step instructions.

Quick summary:
1. Generate test DAC/PAI/PAA with OpenSSL.
2. `python3 tools/provision_attestation.py --port /dev/ttyACM0 --dac ... --pai ... --key ...`
3. Build and flash firmware.
4. Run `chip-tool pairing ble-wifi ...` for full commissioning.
5. Verify cert chain: `./build-host/attestation_verifier chain dac.der pai.der paa.der`
6. Verify signature: `./build-host/attestation_verifier sig dac.der att_elements.bin sig.der`
