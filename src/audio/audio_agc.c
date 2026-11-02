#include "audio_agc.h"
#include "app_config.h"
#include <math.h>

void audio_agc_init(audio_agc_t *agc) {
    if (!agc) return;
    agc->target_level = AGC_TARGET_LEVEL;
    agc->decay_rate = AGC_RELEASE_RATE;
    agc->max_gain = AGC_MAX_GAIN;
    agc->noise_floor = AGC_NOISE_FLOOR;
    agc->peak = AGC_INITIAL_PEAK;
    agc->gain = 1.0f;
}

void audio_agc_reset(audio_agc_t *agc) {
    if (!agc) return;
    agc->peak = AGC_INITIAL_PEAK;
    agc->gain = 1.0f;
}

void audio_agc_process(audio_agc_t *agc, int16_t *samples, size_t count) {
    if (!agc || !samples || count == 0) return;

    float block_peak = 0.0f;
    for (size_t i = 0; i < count; ++i) {
        float a = fabsf((float)samples[i]);
        if (a > block_peak) block_peak = a;
    }

    // Fast attack, slow release. Using one gain per frame avoids the
    // per-sample pumping that made close speech sound rough.
    if (block_peak > agc->peak) {
        agc->peak = block_peak;
    } else {
        agc->peak = agc->peak * agc->decay_rate
                   + block_peak * (1.0f - agc->decay_rate);
    }

    float denom = (agc->peak > agc->noise_floor) ? agc->peak : agc->noise_floor;
    float desired_gain = agc->target_level / denom;
    if (desired_gain > agc->max_gain) {
        desired_gain = agc->max_gain;
    }

    // Quiet, distant voices are attenuated instead of being lifted.
    if (block_peak < AGC_GATE_THRESHOLD) {
        desired_gain *= AGC_GATE_ATTENUATION;
    }

    // Attack quickly to avoid clipping on a loud syllable, release gently.
    if (desired_gain < agc->gain) {
        agc->gain = desired_gain;
    } else {
        agc->gain += (desired_gain - agc->gain) * AGC_GAIN_SMOOTHING;
    }

    for (size_t i = 0; i < count; i++) {
        float v = (float)samples[i] * agc->gain;
        float a = fabsf(v);

        // Soft knee prevents hard-clipped consonant spikes.
        if (a > AGC_SOFT_KNEE) {
            a = AGC_SOFT_KNEE + (a - AGC_SOFT_KNEE) * 0.25f;
            if (a > 32767.0f) a = 32767.0f;
            v = (v < 0.0f) ? -a : a;
        }

        samples[i] = (int16_t)v;
    }
}
