/*
 * FLUG-OS — Wave Output Mode
 * Aligned with axiomquant.org universal threshold kernel.
 *
 * Streams raw 802.11 packet features as numerical wave values
 * through the sine kernel K(u) = sin(πu)/(πu) — the universal
 * kernel governing phase transitions across all five threshold
 * domains: Erdős β₁, BitNet ternary γ, Markowitz λ, Black-Scholes σ,
 * and Rényi dyadic β.
 *
 * Three output modes:
 *   raw    — RSSI + density + threshold domain [-1, 0, +1]
 *   wave   — sine kernel sample K(rssi) * density
 *   json   — structured JSON with threshold markers
 *
 * Threshold domain tags:
 *   -1  below critical (subcritical)
 *    0  at critical (phase transition)
 *   +1  above critical (supercritical)
 *
 * Switch with UART command: `mode raw|wave|json`
 */

#ifndef FLUGOS_WAVE_OUTPUT_H
#define FLUGOS_WAVE_OUTPUT_H

#include <Arduino.h>

extern "C" {
  void rust_reduce_rational_pq(int32_t num, int32_t den, int32_t *out_p, int32_t *out_q);
}

#define DENSITY_WINDOW   100
#define RSSI_FLOOR      -100
#define RSSI_CEIL        -20

// Threshold domain: packet density → critical regime
#define DENSITY_CRITICAL_LOW   0.3f   // below critical
#define DENSITY_CRITICAL_HIGH  0.7f   // above critical

typedef enum {
    MODE_RAW,
    MODE_WAVE,
    MODE_JSON
} WaveMode;

static WaveMode wave_mode = MODE_RAW;
static unsigned long last_density_reset = 0;
static uint16_t packet_count = 0;

static void set_wave_mode(const char* mode) {
    if (!mode) return;
    if (strcmp(mode, "raw") == 0) wave_mode = MODE_RAW;
    else if (strcmp(mode, "wave") == 0) wave_mode = MODE_WAVE;
    else if (strcmp(mode, "json") == 0) wave_mode = MODE_JSON;
}

// Normalize RSSI to [0, 1] for sine kernel input
static float normalize_rssi(int8_t rssi) {
    if (rssi < RSSI_FLOOR) return 0.0f;
    if (rssi > RSSI_CEIL) return 1.0f;
    return (float)(rssi - RSSI_FLOOR) / (float)(RSSI_CEIL - RSSI_FLOOR);
}

// Axiom Quant: universal sine kernel K(u) = sin(πu) / (πu)
// Governs eigenvalue spacing, zeta zero correlation, and threshold sharpness.
static float sine_kernel(float u) {
    if (u < 0.001f) return 1.0f;  // limit as u→0 = 1
    float pu = PI * u;
    return sinf(pu) / pu;
}

// Map frame type/subtype to spectral frequency (axiomquant.org §3)
static float type_to_frequency(int type, int subtype) {
    switch (type) {
        case 0:
            switch (subtype) {
                case 8:  return 220.0f;    // Beacon → A3 (Erdős β₁ witness)
                case 4:  return 277.18f;   // Probe Req → C#4 (dyadic bridge)
                case 5:  return 329.63f;   // Probe Resp → E4 (transfer operator)
                case 11: return 349.23f;   // Auth → F4 (Markowitz λ)
                case 12: return 415.30f;   // Deauth → G#4 (Black-Scholes volatility)
                default: return 261.63f;   // Other mgmt → C4 (universal kernel)
            }
        case 1: return 440.0f;   // Control → A4 (spectral rigidity)
        case 2: return 523.25f;  // Data → C5 (quantum chaos)
        default: return 130.81f; // Unknown → C3 (noise floor)
    }
}

// Determine threshold domain: {-1, 0, +1} from packet density
// axiomquant.org: five threshold domains share the same phase transition structure
static int8_t threshold_domain(float density) {
    if (density < DENSITY_CRITICAL_LOW)  return -1;  // below critical
    if (density > DENSITY_CRITICAL_HIGH) return +1;  // above critical
    return 0;                                          // at critical
}

static void output_wave(int8_t rssi, int type, int subtype, unsigned long ts) {
    unsigned long now = millis();
    packet_count++;

    if (now - last_density_reset >= DENSITY_WINDOW) {
        last_density_reset = now;
        packet_count = 0;
    }

    float rssi_norm = normalize_rssi(rssi);
    float freq = type_to_frequency(type, subtype);
    float density = (float)packet_count / (float)DENSITY_WINDOW;
    if (density > 1.0f) density = 1.0f;

    // Axiom Quant: sine kernel applied to RSSI as spectral parameter
    // Scale RSSI [0,1] to spectral range [0,3] — first lobe of K(u)
    float kernel_val = sine_kernel(rssi_norm * 3.0f);
    int8_t domain = threshold_domain(density);

    int32_t rP = 0, rQ = 1, dP = 0, dQ = 1, kP = 0, kQ = 1, fP = 0, fQ = 1;
    int32_t r_num = (int32_t)rssi - RSSI_FLOOR;
    if (r_num < 0) r_num = 0;
    if (r_num > (RSSI_CEIL - RSSI_FLOOR)) r_num = (RSSI_CEIL - RSSI_FLOOR);
    rust_reduce_rational_pq(r_num, RSSI_CEIL - RSSI_FLOOR, &rP, &rQ);
    int32_t d_num = (packet_count > DENSITY_WINDOW) ? DENSITY_WINDOW : (int32_t)packet_count;
    rust_reduce_rational_pq(d_num, DENSITY_WINDOW, &dP, &dQ);
    rust_reduce_rational_pq((int32_t)(kernel_val * 1000.0f), 1000, &kP, &kQ);
    rust_reduce_rational_pq((int32_t)(freq * 100.0f), 100, &fP, &fQ);

    switch (wave_mode) {
        case MODE_RAW:
            // RSSI_pq  density_pq  frame_type  threshold_domain  sine_kernel_pq
            Serial.printf("~ %ld/%ld %ld/%ld %d %d %ld/%ld\n",
                          (long)rP, (long)rQ, (long)dP, (long)dQ, type, domain, (long)kP, (long)kQ);
            break;

        case MODE_WAVE:
            {
                float t = (float)(ts % 10000) / 10000.0f;
                float phase = (float)(type * 100 + subtype * 10);
                // wave = sine envelope × carrier, domain = threshold tag, K = kernel value
                float wave = sinf(2.0f * (float)PI * (freq / 440.0f) * t + phase) * kernel_val;
                int32_t wP = 0, wQ = 1;
                rust_reduce_rational_pq((int32_t)(wave * 1000.0f), 1000, &wP, &wQ);
                Serial.printf("~ %ld/%ld %d %ld/%ld\n", (long)wP, (long)wQ, domain, (long)kP, (long)kQ);
            }
            break;

        case MODE_JSON:
            Serial.printf(
                "~ {\"rssi_pq\":\"%ld/%ld\",\"density_pq\":\"%ld/%ld\",\"freq_pq\":\"%ld/%ld\","
                "\"type\":%d,\"subtype\":%d,"
                "\"domain\":%d,"
                "\"kernel_pq\":\"%ld/%ld\","
                "\"ts\":%lu}\n",
                (long)rP, (long)rQ, (long)dP, (long)dQ, (long)fP, (long)fQ,
                type, subtype,
                domain,
                (long)kP, (long)kQ, ts);
            break;
    }
}

#endif
