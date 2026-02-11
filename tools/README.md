# Tools

This directory contains utility tools for the Viking Bio Matter Bridge project.

## derive_pin.py

Derives the Matter setup PIN code from a device MAC address.

### Purpose

The Viking Bio Matter Bridge derives a unique 8-digit setup PIN from the device's MAC address on boot. This tool allows you to compute the same PIN offline when you only have the MAC address (e.g., from device labels or network scans).

### Usage

```bash
./tools/derive_pin.py <MAC_ADDRESS>
```

Supported MAC address formats:
- `AA:BB:CC:DD:EE:FF` (colon-separated)
- `AA-BB-CC-DD-EE-FF` (dash-separated)
- `AABBCCDDEEFF` (no separators)

### Examples

```bash
# Standard format
./tools/derive_pin.py 28:CD:C1:00:00:01
# Output:
# Device MAC:     28:CD:C1:00:00:01
# Setup PIN Code: 24890840

# Dash-separated format
./tools/derive_pin.py AA-BB-CC-DD-EE-FF

# No separators
./tools/derive_pin.py AABBCCDDEEFF
```

### Algorithm

The PIN derivation algorithm matches the C++ implementation in `platform/pico_w_chip_port/platform_manager.cpp`:

1. Concatenate: `MAC_ADDRESS || PRODUCT_SALT` (where `||` means concatenation)
2. Compute: `hash = SHA256(MAC_ADDRESS || PRODUCT_SALT)`
3. Extract: `hash_value = first 4 bytes of hash (big-endian uint32)`
4. Calculate: `pin = hash_value % 100000000`
5. Format: Zero-pad to 8 digits

**Important:** The `PRODUCT_SALT` constant must match between `derive_pin.py` and `platform_manager.cpp`. Current value: `"VIKINGBIO-2026"`

### Security Considerations

- The PIN is deterministic based on MAC address
- Each device with a unique MAC gets a unique PIN
- Changing `PRODUCT_SALT` changes all PINs (requires reflashing firmware)
- For production deployments, consider using a product-specific salt

### Dependencies

- Python 3.6+
- Standard library only (no external packages required)
