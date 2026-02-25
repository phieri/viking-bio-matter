/*
 * basic.c
 * Matter Basic Information Cluster (0x0028) implementation.
 * Returns device identification strings to Matter controllers so that
 * iOS Home and other controllers display the device by name instead of
 * the generic "Matter Accessory" placeholder.
 */

#include "basic.h"
#include "CHIPDevicePlatformConfig.h"

/**
 * Read a Basic Information cluster attribute.
 *
 * String attributes return pointers into static storage (macro string
 * literals) — no allocation or copying is needed.
 */
int cluster_basic_read(uint8_t endpoint, uint32_t attr_id,
                       attribute_value_t *value, attribute_type_t *type) {
    if (!value || !type) {
        return -1;
    }

    /* Basic Information cluster exists on endpoint 0 (root node) only */
    if (endpoint != 0) {
        return -1;
    }

    switch (attr_id) {
        case ATTR_BASIC_DATA_MODEL_REVISION:
            value->uint16_val = 1u;
            *type = ATTR_TYPE_UINT16;
            return 0;

        case ATTR_BASIC_VENDOR_NAME:
            value->string_val.str = CHIP_DEVICE_CONFIG_DEVICE_VENDOR_NAME;
            value->string_val.len = (uint16_t)(sizeof(
                    CHIP_DEVICE_CONFIG_DEVICE_VENDOR_NAME) - 1u);
            *type = ATTR_TYPE_UTF8_STRING;
            return 0;

        case ATTR_BASIC_VENDOR_ID:
            value->uint16_val = CHIP_DEVICE_CONFIG_DEVICE_VENDOR_ID;
            *type = ATTR_TYPE_UINT16;
            return 0;

        case ATTR_BASIC_PRODUCT_NAME:
            value->string_val.str = CHIP_DEVICE_CONFIG_DEVICE_PRODUCT_NAME;
            value->string_val.len = (uint16_t)(sizeof(
                    CHIP_DEVICE_CONFIG_DEVICE_PRODUCT_NAME) - 1u);
            *type = ATTR_TYPE_UTF8_STRING;
            return 0;

        case ATTR_BASIC_PRODUCT_ID:
            value->uint16_val = CHIP_DEVICE_CONFIG_DEVICE_PRODUCT_ID;
            *type = ATTR_TYPE_UINT16;
            return 0;

        case ATTR_BASIC_NODE_LABEL:
            /* NodeLabel is user-configurable; return empty string. */
            value->string_val.str = "";
            value->string_val.len = 0u;
            *type = ATTR_TYPE_UTF8_STRING;
            return 0;

        case ATTR_BASIC_LOCATION:
            value->string_val.str = "";
            value->string_val.len = 0u;
            *type = ATTR_TYPE_UTF8_STRING;
            return 0;

        case ATTR_BASIC_HARDWARE_VERSION:
            value->uint16_val = CHIP_DEVICE_CONFIG_DEVICE_HARDWARE_VERSION;
            *type = ATTR_TYPE_UINT16;
            return 0;

        case ATTR_BASIC_SOFTWARE_VERSION:
            value->uint32_val = CHIP_DEVICE_CONFIG_DEVICE_SOFTWARE_VERSION;
            *type = ATTR_TYPE_UINT32;
            return 0;

        case ATTR_BASIC_SOFTWARE_VERSION_STR:
            value->string_val.str = CHIP_DEVICE_CONFIG_DEVICE_SOFTWARE_VERSION_STRING;
            value->string_val.len = (uint16_t)(sizeof(
                    CHIP_DEVICE_CONFIG_DEVICE_SOFTWARE_VERSION_STRING) - 1u);
            *type = ATTR_TYPE_UTF8_STRING;
            return 0;

        default:
            return -1;
    }
}
