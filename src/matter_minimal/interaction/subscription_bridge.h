/*
 * subscription_bridge.h
 * Bridge between C++ matter_attributes and C subscribe_handler
 */

#ifndef SUBSCRIPTION_BRIDGE_H
#define SUBSCRIPTION_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize subscription bridge
 * Connects matter_attributes change notifications to subscribe_handler
 * Must be called after both matter_attributes_init() and subscribe_handler_init()
 * 
 * @return 0 on success, -1 on failure
 */
int subscription_bridge_init(void);

#ifdef __cplusplus
}
#endif

#endif // SUBSCRIPTION_BRIDGE_H
