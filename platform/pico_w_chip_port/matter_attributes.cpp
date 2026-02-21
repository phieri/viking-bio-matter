/*
 * matter_attributes.cpp
 * Matter attribute storage and reporting system implementation
 */

#include "matter_attributes.h"
#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include "pico/stdlib.h"

// Maximum number of attributes we can track
#define MAX_ATTRIBUTES 16

// Attribute storage
static matter_attribute_t attributes[MAX_ATTRIBUTES];
static size_t attribute_count = 0;

// Subscriber storage
static matter_subscriber_callback_t subscribers[MATTER_MAX_SUBSCRIBERS];
static bool subscriber_active[MATTER_MAX_SUBSCRIBERS];

// Initialization flag
static bool initialized = false;
// Access is expected from the single-threaded main loop only.

int matter_attributes_init(void) {
    if (initialized) {
        return 0;
    }
    
    // Clear all attributes
    memset(attributes, 0, sizeof(attributes));
    attribute_count = 0;
    
    // Clear subscribers
    memset(subscribers, 0, sizeof(subscribers));
    memset(subscriber_active, 0, sizeof(subscriber_active));
    
    initialized = true;
    printf("Matter: Attribute system initialized\n");
    
    return 0;
}

int matter_attributes_register(uint8_t endpoint, uint32_t cluster_id, 
                               uint32_t attribute_id, matter_attr_type_t type,
                               const matter_attr_value_t *initial_value) {
    if (!initialized) {
        return -1;
    }
    
    if (attribute_count >= MAX_ATTRIBUTES) {
        printf("[Matter] ERROR: Maximum attributes reached\n");
        return -1;
    }
    
    // Check if attribute already exists
    for (size_t i = 0; i < attribute_count; i++) {
        if (attributes[i].endpoint == endpoint &&
            attributes[i].cluster_id == cluster_id &&
            attributes[i].attribute_id == attribute_id) {
            printf("Matter: Attribute already registered (EP:%u, CL:0x%04lX, AT:0x%04lX)\n",
                   endpoint, (unsigned long)cluster_id, (unsigned long)attribute_id);
            return 0;  // Already registered
        }
    }
    
    // Register new attribute
    matter_attribute_t *attr = &attributes[attribute_count];
    attr->endpoint = endpoint;
    attr->cluster_id = cluster_id;
    attr->attribute_id = attribute_id;
    attr->type = type;
    attr->dirty = false;
    
    if (initial_value) {
        attr->value = *initial_value;
    } else {
        memset(&attr->value, 0, sizeof(attr->value));
    }
    
    attribute_count++;
    
    printf("Matter: Registered attribute (EP:%u, CL:0x%04" PRIx32 ", AT:0x%04" PRIx32 ")\n",
           endpoint, cluster_id, attribute_id);
    
    return 0;
}

static matter_attribute_t* find_attribute(uint8_t endpoint, uint32_t cluster_id, uint32_t attribute_id) {
    for (size_t i = 0; i < attribute_count; i++) {
        if (attributes[i].endpoint == endpoint &&
            attributes[i].cluster_id == cluster_id &&
            attributes[i].attribute_id == attribute_id) {
            return &attributes[i];
        }
    }
    return NULL;
}

static bool values_equal(const matter_attr_value_t *a, const matter_attr_value_t *b, matter_attr_type_t type) {
    switch (type) {
        case MATTER_TYPE_BOOL:
            return a->bool_val == b->bool_val;
        case MATTER_TYPE_UINT8:
            return a->uint8_val == b->uint8_val;
        case MATTER_TYPE_INT16:
            return a->int16_val == b->int16_val;
        case MATTER_TYPE_UINT32:
            return a->uint32_val == b->uint32_val;
        default:
            return false;
    }
}

int matter_attributes_update(uint8_t endpoint, uint32_t cluster_id,
                            uint32_t attribute_id, const matter_attr_value_t *value) {
    if (!initialized || !value) {
        return -1;
    }
    
    matter_attribute_t *attr = find_attribute(endpoint, cluster_id, attribute_id);
    if (!attr) {
        printf("Matter: WARNING - Attribute not found (EP:%u, CL:0x%04" PRIx32 ", AT:0x%04" PRIx32 ")\n",
               endpoint, cluster_id, attribute_id);
        return -1;
    }
    
    // Check if value actually changed
    if (!values_equal(&attr->value, value, attr->type)) {
        attr->value = *value;
        attr->dirty = true;
        
        // Make a copy of subscriber info before releasing lock
        matter_subscriber_callback_t active_subscribers[MATTER_MAX_SUBSCRIBERS];
        int active_count = 0;
        for (int i = 0; i < MATTER_MAX_SUBSCRIBERS; i++) {
            if (subscriber_active[i] && subscribers[i]) {
                active_subscribers[active_count++] = subscribers[i];
            }
        }
        
        // Log the change
        printf("Matter: Attribute changed (EP:%u, CL:0x%04" PRIx32 ", AT:0x%04" PRIx32 ") = ",
               endpoint, cluster_id, attribute_id);
        
        switch (attr->type) {
            case MATTER_TYPE_BOOL:
                printf("%s\n", attr->value.bool_val ? "true" : "false");
                break;
            case MATTER_TYPE_UINT8:
                printf("%u\n", attr->value.uint8_val);
                break;
            case MATTER_TYPE_INT16:
                printf("%d\n", attr->value.int16_val);
                break;
            case MATTER_TYPE_UINT32:
                printf("%lu\n", (unsigned long)attr->value.uint32_val);
                break;
        }
        
        // Notify subscribers of the change (outside of critical section)
        for (int i = 0; i < active_count; i++) {
            active_subscribers[i](endpoint, cluster_id, attribute_id, value);
        }
    }
    
    return 0;
}

int matter_attributes_get(uint8_t endpoint, uint32_t cluster_id,
                         uint32_t attribute_id, matter_attr_value_t *value) {
    if (!initialized || !value) {
        return -1;
    }
    
    matter_attribute_t *attr = find_attribute(endpoint, cluster_id, attribute_id);
    if (!attr) {
        return -1;
    }
    
    *value = attr->value;
    
    return 0;
}

int matter_attributes_subscribe(matter_subscriber_callback_t callback) {
    if (!initialized || !callback) {
        return -1;
    }
    
    // Find free subscriber slot
    for (int i = 0; i < MATTER_MAX_SUBSCRIBERS; i++) {
        if (!subscriber_active[i]) {
            subscribers[i] = callback;
            subscriber_active[i] = true;
            printf("Matter: Subscriber %d registered\n", i);
            return i;
        }
    }
    printf("[Matter] ERROR: Maximum subscribers reached\n");
    return -1;
}

void matter_attributes_unsubscribe(int subscriber_id) {
    if (!initialized || subscriber_id < 0 || subscriber_id >= MATTER_MAX_SUBSCRIBERS) {
        return;
    }
    
    subscriber_active[subscriber_id] = false;
    subscribers[subscriber_id] = NULL;
    
    printf("Matter: Subscriber %d unregistered\n", subscriber_id);
}

void matter_attributes_process_reports(void) {
    if (!initialized) {
        return;
    }
    
    matter_attribute_t dirty_attrs[MAX_ATTRIBUTES];
    size_t dirty_count = 0;
    
    matter_subscriber_callback_t active_subscribers[MATTER_MAX_SUBSCRIBERS];
    int active_count = 0;

    // Collect dirty attributes
    for (size_t i = 0; i < attribute_count; i++) {
        if (attributes[i].dirty) {
            dirty_attrs[dirty_count++] = attributes[i];
            // Clear dirty flag now
            attributes[i].dirty = false;
        }
    }
    
    // Collect active subscribers
    for (int s = 0; s < MATTER_MAX_SUBSCRIBERS; s++) {
        if (subscriber_active[s] && subscribers[s]) {
            active_subscribers[active_count++] = subscribers[s];
        }
    }

    for (size_t i = 0; i < dirty_count; i++) {
        matter_attribute_t *attr = &dirty_attrs[i];
        
        for (int s = 0; s < active_count; s++) {
            active_subscribers[s](attr->endpoint, attr->cluster_id, 
                                 attr->attribute_id, &attr->value);
        }
    }
}

size_t matter_attributes_count(void) {
    return attribute_count;
}

void matter_attributes_clear(void) {
    if (!initialized) {
        return;
    }
    
    memset(attributes, 0, sizeof(attributes));
    attribute_count = 0;
    
    memset(subscribers, 0, sizeof(subscribers));
    memset(subscriber_active, 0, sizeof(subscriber_active));
    
    printf("Matter: All attributes cleared\n");
}
