/*
 * matter_network_subscriber.h
 * Network subscriber for sending attribute reports over WiFi
 */

#ifndef MATTER_NETWORK_SUBSCRIBER_H
#define MATTER_NETWORK_SUBSCRIBER_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize the Matter network subscriber
 * Subscribes to attribute changes and sends them to Matter controllers over WiFi
 * @return 0 on success, -1 on failure
 */
int matter_network_subscriber_init(void);

#ifdef __cplusplus
}
#endif

#endif // MATTER_NETWORK_SUBSCRIBER_H
