/*
 * FLUG-OS — Wave Output Mode
 * Streams raw 802.11 packet features as numerical wave values
 * for sonification and generative music input (music.vaked.dev).
 *
 * Three output modes:
 *   raw    — RSSI + packet density as floats, one per line
 *   wave   — synthesized waveform from packet activity
 *   json   — structured JSON with musical parameters
 *
 * Switch with: `mode raw|wave|json`
 */

#ifndef FLUGOS_WAVE_OUTPUT_H
#define FLUGOS_WAVE_OUTPUT_H

#include <Arduino.h>
#include <string.h>

#define WAVE_BUF_SIZE    64
#define DENSITY_WINDOW   100
#define RSSI_FLOOR      -100
#define RSSI_CEIL        -20

typedef enum {
    MODE_RAW,
    MODE_WAVE,
    MODE_JSON
} WaveMode;

static WaveMode wave_mode = MODE_RAW;
static unsigned long last_density_reset = 0;
static int packet_count = 0;
static float amplitude_acc = 0.0f;

static void set_wave_mode(const char* mode) {
    if (strcmp(mode, "raw") == 0) wave_mode = MODE_RAW;
    else if (strcmp(mode, "wave") == 0) wave_mode = MODE_WAVE;
    else if (strcmp(mode, "json") == 0) wave_mode = MODE_JSON;
}

static float normalize_rssi(int8_t rssi) {
    if (rssi < RSSI_FLOOR) return 0.0f;
    if (rssi > RSSI_CEIL) return 1.0f;
    return (float)(rssi - RSSI_FLOOR) / (float)(RSSI_CEIL - RSSI_FLOOR);
}

static float type_to_frequency(int type, int subtype) {
    switch (type) {
        case 0:
            switch (subtype) {
                case 8:  return 220.0f;
                case 4:  return 277.18f;
                case 5:  return 329.63f;
                case 11: return 349.23f;
                case 12: return 415.30f;
                default: return 261.63f;
            }
        case 1: return 440.0f;
        case 2: return 523.25f;
        default: return 130.81f;
    }
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
    float density = packet_count / (float)(DENSITY_WINDOW) * 100.0f;
    if (density > 1.0f) density = 1.0f;

    amplitude_acc += rssi_norm;
    if (amplitude_acc > 1.0f) amplitude_acc = 0.0f;

    switch (wave_mode) {
        case MODE_RAW:
            Serial.printf("%.3f %.3f %d\n", rssi_norm, density, type);
            break;

        case MODE_WAVE:
            {
                float t = (ts % 10000) / 10000.0f;
                float phase = (float)(type * 100 + subtype * 10);
                float wave = sin(2.0f * PI * (freq / 440.0f) * t + phase) * rssi_norm;
                Serial.printf("%.4f\n", wave);
            }
            break;

        case MODE_JSON:
            Serial.printf("{\"rssi\":%.3f,\"density\":%.3f,\"freq\":%.2f,"
                          "\"type\":%d,\"subtype\":%d,\"ts\":%lu}\n",
                          rssi_norm, density, freq, type, subtype, ts);
            break;
    }
}

#endif
