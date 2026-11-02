#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float peak;             // Smoothed per-frame peak envelope
    float gain;             // Smoothed current gain
    float target_level;     // Target level
    float decay_rate;       // Per-frame envelope release multiplier
    float max_gain;         // Maximum allowed gain boost
    float noise_floor;      // Minimum floor to avoid amplifying silence
} audio_agc_t;

/**
 * @brief Initialize AGC state with default parameters
 */
void audio_agc_init(audio_agc_t *agc);

/**
 * @brief Reset AGC peak envelope (e.g. at speech start)
 */
void audio_agc_reset(audio_agc_t *agc);

/**
 * @brief Process PCM samples in-place with AGC gain and soft clipping
 * @param agc AGC configuration & state
 * @param samples Array of 16-bit PCM samples
 * @param count Number of samples
 */
void audio_agc_process(audio_agc_t *agc, int16_t *samples, size_t count);

#ifdef __cplusplus
}
#endif
