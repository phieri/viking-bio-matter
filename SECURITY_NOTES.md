# Security Notes - Viking Bio Matter Bridge

**Last Updated:** February 15, 2026  
**Document Version:** 1.0

---

## Overview

This document provides important security considerations for the Viking Bio Matter Bridge firmware. It is intended for developers, security auditors, and deployment engineers.

## Critical Security Status

✅ **All Critical Security Issues Resolved**

As of February 15, 2026, all critical security vulnerabilities identified in the comprehensive codebase review have been addressed. See [CODEBASE_REVIEW_FINDINGS.md](CODEBASE_REVIEW_FINDINGS.md) for detailed information.

---

## Known Limitations

### 1. PASE (Password-Authenticated Session Establishment) Simplification

**File:** `src/matter_minimal/security/pase.c` (lines 412-424)  
**Severity:** Medium (for development), High (for production)  
**Status:** Documented Known Limitation

#### Description

The current PASE implementation uses a **simplified SPAKE2+ calculation** that is suitable for development and testing but not for production deployment requiring full Matter certification.

**Code Comment:**
```c
// Simplified approach: For minimal implementation, use pA directly
// Full implementation would properly compute pA - w0*M
if (mbedtls_ecp_copy(&point_temp, &point_pA) != 0) {
    // This is NOT the correct SPAKE2+ calculation
```

#### Impact

- **Development/Testing:** Acceptable - allows Matter commissioning to function for development purposes
- **Production:** Not recommended - authentication may be weaker than full SPAKE2+ specification
- **Certification:** Would not pass full Matter certification with this simplification

#### Mitigation Options

Choose one of the following approaches for production deployment:

1. **Use Official Matter SDK**
   - Integrate with official ConnectedHomeIP SDK
   - Provides full SPAKE2+ implementation
   - Certified for Matter compliance
   - **Recommended for production**

2. **Implement Full SPAKE2+**
   - Follow Matter specification exactly
   - Properly compute `pA - w0*M` using elliptic curve operations
   - Verify against test vectors
   - Submit for security audit

3. **Alternative Commissioning**
   - Disable PASE if not required
   - Use alternative commissioning method
   - Document security model clearly

4. **Accept Risk (Development Only)**
   - Current implementation suitable for:
     - Development environments
     - Internal testing
     - Non-production deployments
   - NOT suitable for:
     - Production deployments
     - Matter certification
     - Security-critical applications

#### Current Usage Recommendation

**✅ Acceptable:**
- Development and testing environments
- Internal prototypes
- Proof-of-concept demonstrations
- Non-production lab testing

**❌ Not Acceptable:**
- Production deployments
- Customer-facing products
- Matter-certified devices
- Security-critical applications

---

## Security Best Practices

### WiFi Credentials

**File:** `platform/pico_w_chip_port/network_adapter.cpp` (lines 10-11)

WiFi credentials are currently hardcoded for development purposes:
```cpp
#define WIFI_SSID "YourNetworkName"
#define WIFI_PASSWORD "YourPassword"
```

**Best Practices:**
1. ⚠️ **Never commit real credentials** to version control
2. Use commissioning mode to configure WiFi dynamically
3. Store credentials in flash via Matter network commissioning
4. For production, implement secure credential storage
5. Consider using SoftAP mode for initial setup (already implemented at 192.168.4.1)

### Firmware Updates

**Current Status:** No OTA (Over-The-Air) update mechanism

**Security Implications:**
- Physical access required for firmware updates (USB BOOTSEL mode)
- Positive: Prevents remote firmware tampering
- Negative: Difficult to patch security issues in deployed devices

**Recommendation:**
- For production, implement secure OTA with:
  - Cryptographic signature verification
  - Rollback protection
  - Secure boot chain

### Cryptographic RNG

**File:** `platform/pico_w_chip_port/crypto_adapter.cpp` (lines 14-49)

**Known Issue:** DRBG/RNG functions are stubbed due to Pico SDK 1.5.1 mbedTLS limitations.

```cpp
int crypto_adapter_random(uint8_t *buffer, size_t length) {
    // TODO: Implement proper RNG when Pico SDK mbedTLS issues are resolved
    return -1;  // Always fails
}
```

**Mitigation:**
- SHA256 and AES functions work correctly
- Monitor Pico SDK updates for mbedTLS fixes
- For now, commissioning must not rely on this RNG
- Hardware RNG from RP2040 not yet integrated

**Action Items:**
- Track Pico SDK issue for mbedTLS RNG support
- Consider implementing hardware RNG wrapper
- Document RNG limitations in commissioning flow

---

## Fixed Security Issues

All issues listed below have been **successfully fixed** as of February 15, 2026:

### Memory Safety (Fixed ✅)
1. Buffer overflow in protocol parser - Fixed with correct loop bounds
2. Use-after-free in Matter protocol - Fixed with static buffer
3. TLV buffer over-read - Fixed with bounds checking
4. Stack overflow protection - Fixed with bounded loops

### Concurrency (Fixed ✅)
1. Serial buffer race conditions - Fixed with interrupt disabling
2. Matter attribute races - Fixed with critical sections
3. UDP transport races - Fixed with critical sections

### Error Handling (Fixed ✅)
1. Missing error checks in initialization - Fixed with comprehensive checking
2. Ignored return values - Fixed with error logging
3. Watchdog protection - Added with 8-second timeout

See [CODEBASE_REVIEW_FINDINGS.md](CODEBASE_REVIEW_FINDINGS.md) for detailed information on all fixes.

---

## Security Disclosure

If you discover a security vulnerability in this codebase:

1. **Do not** open a public GitHub issue
2. Email security concerns to: [Repository Owner]
3. Provide detailed information:
   - Description of the vulnerability
   - Steps to reproduce
   - Potential impact assessment
   - Suggested fix (if any)

We aim to acknowledge security reports within 48 hours and provide fixes for valid issues within 30 days.

---

## Compliance and Certification

### Matter Certification

**Current Status:** Not certified

**Certification Path:**
1. Address PASE limitation (see above)
2. Implement full Matter specification requirements
3. Pass Matter certification tests
4. Obtain official Matter certification

### Security Audits

**Recommended:**
- Independent security audit before production deployment
- Penetration testing of Matter protocol implementation
- Code review by security experts
- Fuzzing of protocol parsers

---

## Change Log

### Version 1.0 (February 15, 2026)
- Initial security documentation
- Documented PASE limitation
- Listed all fixed security issues
- Added security best practices
- Created security disclosure process

---

## References

1. [Matter Specification](https://csa-iot.org/developer-resource/specifications-download-request/)
2. [SPAKE2+ RFC](https://datatracker.ietf.org/doc/html/draft-irtf-cfrg-spake2)
3. [Raspberry Pi Pico SDK Documentation](https://raspberrypi.github.io/pico-sdk-doxygen/)
4. [mbedTLS Security Advisories](https://mbed-tls.readthedocs.io/en/latest/security-advisories/)

---

**Document Maintenance:**
- Review and update this document with each security-related change
- Update version and date at the top
- Keep change log current
- Ensure all known limitations are documented
