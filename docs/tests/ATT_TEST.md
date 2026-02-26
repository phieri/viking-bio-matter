# Attestation & CASE Testing Guide

> **TEST-MODE ONLY** – See `PRODUCTION_README.md` for production hardening requirements.

This guide explains how to test the Matter Device Attestation and CASE (Sigma) flows on the Viking Bio Matter Bridge.

---

## Prerequisites

- Raspberry Pi Pico W flashed with the latest firmware.
- USB serial terminal (`screen /dev/ttyACM0 115200` or `./run.sh monitor`).
- `chip-tool` from the Matter SDK or any compliant Matter controller.
- Python 3 with `pyserial` (`pip install pyserial`).
- OpenSSL or mbedTLS installed on the host for running `attestation_verifier`.

---

## 1. Generate Test Certificates (one-time setup)

Use the Matter SDK test PKI or generate your own test certs with OpenSSL.

### Option A – Matter SDK test certs
```bash
# From the connectedhomeip repo
python3 src/credentials/generate-attestation-certificates.py \
    --vid 0xFFF1 --pid 0x8001 \
    --out test_certs/
```

### Option B – Self-signed test certs with OpenSSL
```bash
mkdir test_certs && cd test_certs

# 1. PAA (Product Attestation Authority) – self-signed root
openssl ecparam -name prime256v1 -genkey -noout -out paa_key.pem
openssl req -new -x509 -key paa_key.pem -out paa.pem -days 3650 \
    -subj "/O=Test PAA/CN=Viking Bio Test PAA"
openssl x509 -in paa.pem -outform DER -out paa.der

# 2. PAI (Product Attestation Intermediate) – signed by PAA
openssl ecparam -name prime256v1 -genkey -noout -out pai_key.pem
openssl req -new -key pai_key.pem -out pai.csr \
    -subj "/O=Test PAI/CN=Viking Bio Test PAI"
openssl x509 -req -in pai.csr -CA paa.pem -CAkey paa_key.pem \
    -CAcreateserial -out pai.pem -days 3650
openssl x509 -in pai.pem -outform DER -out pai.der

# 3. DAC (Device Attestation Certificate) – signed by PAI
openssl ecparam -name prime256v1 -genkey -noout -out dac_key.pem
openssl req -new -key dac_key.pem -out dac.csr \
    -subj "/O=Test DAC/CN=Viking Bio Device 0001"
openssl x509 -req -in dac.csr -CA pai.pem -CAkey pai_key.pem \
    -CAcreateserial -out dac.pem -days 3650
openssl x509 -in dac.pem -outform DER -out dac.der
openssl ec -in dac_key.pem -outform DER -out dac_key.der
```

---

## 2. Provision Credentials onto the Device

```bash
python3 tools/provision_attestation.py \
    --port /dev/ttyACM0 \
    --dac  test_certs/dac.der \
    --pai  test_certs/pai.der \
    --key  test_certs/dac_key.der
```

Verify the credentials were stored:
```bash
python3 tools/provision_attestation.py --port /dev/ttyACM0 --verify
```

> **Note on LittleFS paths**: credentials are stored with flat names (e.g., `/att_dac`)
> rather than in a `/certs/` subdirectory, because the storage adapter does not
> automatically create parent directories.

---

## 3. Build the Host Attestation Verifier

```bash
# From the repository root
mkdir build-host && cd build-host
cmake ../host
make -j$(nproc)
```

---

## 4. Run Attestation Chain Verification

```bash
./build-host/attestation_verifier chain \
    test_certs/dac.der \
    test_certs/pai.der \
    test_certs/paa.der
```

Expected output:
```
[OK] Certificate chain verified: DAC → PAI → PAA
```

---

## 5. Verify Attestation Signature

After collecting the attestation elements and signature from the device (e.g., via `chip-tool attestation` or a custom test harness):

```bash
./build-host/attestation_verifier sig \
    test_certs/dac.der \
    att_elements.bin \
    att_signature.der
```

Expected output:
```
[OK] Attestation signature verified with DAC public key
```

---

## 6. End-to-End Commissioning with chip-tool

### Step 1 – BLE PASE (WiFi provisioning)
```bash
chip-tool pairing ble-wifi \
    <node-id> <ssid> <password> <discriminator> <setup-pin>
```

### Step 2 – Device joins WiFi, starts mDNS

Monitor serial output for the device IP and mDNS advertisement.

### Step 3 – Attestation (automatic via chip-tool)

chip-tool will automatically perform attestation as part of commissioning.
The device responds with its DAC+PAI cert chain and a signed nonce.
chip-tool verifies the chain against the PAA (test PAA needs to be imported
into chip-tool's credential store).

### Step 4 – CASE session establishment (automatic)

After attestation, chip-tool establishes a CASE session (Sigma1 → Sigma2 → Sigma3).
The device's CASE handler derives session keys and registers the session.

### Step 5 – AddNOC (NOC provisioning)

chip-tool sends `AddNOC` with the device's Operational Certificate.
The firmware stores it via `certificate_store_save_noc()`.

Verify NOC was stored:
```
[CertStore] Saved <N> bytes to /certs/noc.der
```

---

## 7. CASE Interop Testing (TODO)

A Python initiator for testing CASE Sigma exchange without chip-tool is planned.
See the TODO section below.

---

## Known Limitations (Test-Mode)

| Limitation | Impact | Production fix |
|---|---|---|
| Private key stored in flash | Key can be extracted via physical access | Use secure element (e.g., ATECC608A) |
| No full NOC chain verification in Sigma3 | Any valid TBE3 decryption passes | Verify NOC against RCAC using mbedTLS X.509 |
| No resumption ID support | Cannot resume CASE sessions | Implement Sigma2_resume |
| Timestamp = 0 in AttestationElements | Controller may warn | Use RTC or NTP time |

---

## TODO / Next Steps

- [ ] `tests/case_initiator_test.py` – Python CASE initiator for Sigma interop test
- [ ] Full NOC chain verification in `case_handle_sigma3()`
- [ ] CASE session resumption (Sigma2_resume)
- [ ] Real timestamp in `attestation_generate_attestation_tlv()`
- [ ] Secure element integration (`crypto_se.h` abstraction)
