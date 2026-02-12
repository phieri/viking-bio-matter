/*
 * matter_reporter.h
 * Example Matter attribute report subscriber
 */

#ifndef MATTER_REPORTER_H
#define MATTER_REPORTER_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize the Matter attribute reporter
 * Subscribes to all attribute changes and logs them
 * @return 0 on success, -1 on failure
 */
int matter_reporter_init(void);

#ifdef __cplusplus
}
#endif

#endif // MATTER_REPORTER_H
