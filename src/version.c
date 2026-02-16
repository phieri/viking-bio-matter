/*
 * version.c
 * Build version information implementation
 */

#include <stdio.h>
#include "version.h"

void version_print_info(void) {
    printf("====================================\n");
    printf("  Viking Bio Matter Bridge\n");
    printf("====================================\n");
    printf("Version:    %s\n", FIRMWARE_VERSION);
    printf("Build:      %s\n", BUILD_TIMESTAMP);
    printf("Git Commit: %s\n", GIT_COMMIT_HASH);
    printf("====================================\n\n");
}
