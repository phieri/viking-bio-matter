/*
 * version.h
 * Build version information for Viking Bio Matter Bridge
 */

#pragma once

// Version information
// These are defined at compile time via CMake
#ifndef FIRMWARE_VERSION
#define FIRMWARE_VERSION "unknown"
#endif

#ifndef BUILD_TIMESTAMP
#define BUILD_TIMESTAMP "unknown"
#endif

#ifndef GIT_COMMIT_HASH
#define GIT_COMMIT_HASH "unknown"
#endif

// Function to print version information to console
void version_print_info(void);
