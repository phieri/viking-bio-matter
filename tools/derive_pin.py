#!/usr/bin/env python3
"""
Derive Matter setup PIN from device MAC address.

This tool computes the same 8-digit setup PIN that the Viking Bio Matter
firmware derives on boot from the device MAC address. Use this to determine
the commissioning PIN for a device when you only have the MAC address.

Algorithm:
1. Hash MAC || PRODUCT_SALT with SHA-256
2. Take first 4 bytes of hash as big-endian uint32
3. Compute PIN = (hash_value % 100000000)
4. Format as zero-padded 8-digit string
"""

import sys
import hashlib
import re
import argparse

# Must match PRODUCT_SALT in platform/pico_w_chip_port/platform_manager.cpp
PRODUCT_SALT = b"VIKINGBIO-2026"


def parse_mac(mac_str):
    """
    Parse MAC address from string.
    
    Accepts formats:
    - AA:BB:CC:DD:EE:FF (colon-separated)
    - AA-BB-CC-DD-EE-FF (dash-separated)
    - AABBCCDDEEFF (no separators)
    
    Returns:
        bytes: 6-byte MAC address
    """
    # Remove common separators
    mac_str = mac_str.replace(":", "").replace("-", "").replace(" ", "")
    
    # Validate hex string
    if not re.match(r'^[0-9A-Fa-f]{12}$', mac_str):
        raise ValueError(f"Invalid MAC address format: {mac_str}")
    
    # Convert to bytes
    mac_bytes = bytes.fromhex(mac_str)
    
    if len(mac_bytes) != 6:
        raise ValueError(f"MAC address must be 6 bytes, got {len(mac_bytes)}")
    
    return mac_bytes


def derive_pin_from_mac(mac_bytes):
    """
    Derive 8-digit setup PIN from MAC address.
    
    Args:
        mac_bytes (bytes): 6-byte MAC address
        
    Returns:
        str: Zero-padded 8-digit PIN string
    """
    # Prepare input: MAC || PRODUCT_SALT
    input_data = mac_bytes + PRODUCT_SALT
    
    # Compute SHA-256 hash
    hash_obj = hashlib.sha256(input_data)
    hash_bytes = hash_obj.digest()
    
    # Take first 4 bytes as big-endian uint32
    hash_value = int.from_bytes(hash_bytes[:4], byteorder='big')
    
    # Compute PIN = hash_value % 100000000 (8-digit decimal)
    pin = hash_value % 100000000
    
    # Format as zero-padded 8-digit string
    return f"{pin:08d}"


def main():
    parser = argparse.ArgumentParser(
        description="Derive Matter setup PIN from device MAC address",
        epilog="Examples:\n"
               "  %(prog)s AA:BB:CC:DD:EE:FF\n"
               "  %(prog)s AABBCCDDEEFF\n"
               "  %(prog)s 28:CD:C1:00:00:01",
        formatter_class=argparse.RawDescriptionHelpFormatter
    )
    parser.add_argument(
        'mac_address',
        help='Device MAC address (formats: AA:BB:CC:DD:EE:FF, AA-BB-CC-DD-EE-FF, or AABBCCDDEEFF)'
    )
    
    args = parser.parse_args()
    
    try:
        # Parse MAC address
        mac_bytes = parse_mac(args.mac_address)
        
        # Derive PIN
        pin = derive_pin_from_mac(mac_bytes)
        
        # Format MAC for display
        mac_display = ":".join(f"{b:02X}" for b in mac_bytes)
        
        # Print results
        print(f"Device MAC:     {mac_display}")
        print(f"Setup PIN Code: {pin}")
        
    except ValueError as e:
        print(f"Error: {e}", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
