#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the ES3C28P ILI9341 status display and start its low-priority task.
 *
 * Failure is non-fatal: RemoteMapper continues running without a display.
 */
bool status_display_init(void);

bool status_display_is_ready(void);

#ifdef __cplusplus
}
#endif
