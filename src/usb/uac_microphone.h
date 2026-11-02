#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

bool uac_microphone_init(void);
void uac_microphone_task(void);
bool uac_microphone_is_streaming(void);
bool uac_microphone_is_mounted(void);
uint32_t uac_microphone_get_transfer_count(void);

/**
 * @brief Request a full USB disconnect/reconnect through the UAC watchdog task.
 */
bool uac_microphone_reenumerate(void);

#ifdef __cplusplus
}
#endif
