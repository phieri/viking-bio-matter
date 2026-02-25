/*
 * basic.h
 * Matter Basic Information Cluster (0x0028)
 * Provides device identification strings to Matter controllers.
 */

#ifndef BASIC_H
#define BASIC_H

#include <stdint.h>
#include "../interaction/interaction_model.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Basic Information Cluster ID */
#define CLUSTER_BASIC_INFORMATION 0x0028u

/* Basic Information attribute IDs (Matter Core Spec §11.1) */
#define ATTR_BASIC_DATA_MODEL_REVISION    0x0000u
#define ATTR_BASIC_VENDOR_NAME            0x0001u
#define ATTR_BASIC_VENDOR_ID              0x0002u
#define ATTR_BASIC_PRODUCT_NAME           0x0003u
#define ATTR_BASIC_PRODUCT_ID             0x0004u
#define ATTR_BASIC_NODE_LABEL             0x0005u
#define ATTR_BASIC_LOCATION               0x0006u
#define ATTR_BASIC_HARDWARE_VERSION       0x0007u
#define ATTR_BASIC_SOFTWARE_VERSION       0x0009u
#define ATTR_BASIC_SOFTWARE_VERSION_STR   0x000Au

/**
 * Read a Basic Information cluster attribute.
 *
 * @param endpoint    Endpoint number (0 = root node)
 * @param attr_id     Attribute identifier
 * @param value       Output: attribute value
 * @param type        Output: attribute type
 * @return 0 on success, -1 if unsupported
 */
int cluster_basic_read(uint8_t endpoint, uint32_t attr_id,
                       attribute_value_t *value, attribute_type_t *type);

#ifdef __cplusplus
}
#endif

#endif /* BASIC_H */
