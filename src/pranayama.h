// pranayama.h — Sovereign Pranayama Breathing Pattern + Dream State + 8b-is-engine Layer
// Couples ADA:MEDUSA and BABA:MEDUSA (VAK/OMN) with environmental RF/IMU/MarineSense rhythms.
#ifndef FLUGOS_PRANAYAMA_H
#define FLUGOS_PRANAYAMA_H

#include <Arduino.h>
#include <math.h>

enum PranayamaPhase : uint8_t {
    PRANA_INHALE   = 0, // Puraka (4s breath in)
    PRANA_HOLD_IN  = 1, // Antara Kumbhaka (4s inner retention)
    PRANA_EXHALE   = 2, // Rechaka (4s breath out)
    PRANA_HOLD_OUT = 3  // Bahya Kumbhaka (4s still void)
};

enum DreamStateMode : uint8_t {
    STATE_AWAKE_ACTIVE   = 0, // Active scanning & interaction
    STATE_PRANAYAMA_FLOW = 1, // Coherent 4-4-4-4 / 4-4 breathing
    STATE_DREAM_MANDALA  = 2  // Deep dream state (still IMU + slow tidal wave weave)
};

static const char* const PRANA_PHASE_NAMES[4] = {
    "INHALE (Puraka 4s)",
    "HOLD-IN (Antara 4s)",
    "EXHALE (Rechaka 4s)",
    "STILLNESS (Bahya 4s)"
};

static const char* const DREAM_STATE_NAMES[3] = {
    "AWAKE_ACTIVE",
    "PRANAYAMA_COHERENT",
    "DREAM_MANDALA"
};

struct PranayamaState {
    PranayamaPhase phase;
    DreamStateMode dream_mode;
    float          lung_level;      // 0.0 (empty) .. 1.0 (full)
    uint16_t       cycle_count;
    uint16_t       coherence_permil;// 0..1000
    uint32_t       still_since_ms;
    bool           baba_linked;
    uint32_t       env_sync_seq;
};

static PranayamaState g_prana = {
    PRANA_INHALE, STATE_PRANAYAMA_FLOW, 0.5f, 1, 920, 0, false, 1
};

static void pranayama_tick(unsigned long now_ms, float imu_sickness, uint8_t salience_pct, uint8_t triad_online) {
    // 16-second 4-4-4-4 Sama Vritti Pranayama box cycle (4000ms per phase)
    uint32_t cycle_pos = (uint32_t)(now_ms % 16000UL);
    uint8_t  ph_idx    = (uint8_t)(cycle_pos / 4000UL);
    float    ph_frac   = (float)(cycle_pos % 4000UL) / 4000.0f;

    PranayamaPhase prev_phase = g_prana.phase;
    g_prana.phase = (PranayamaPhase)ph_idx;
    if (prev_phase == PRANA_HOLD_OUT && g_prana.phase == PRANA_INHALE) {
        g_prana.cycle_count++;
    }

    switch (g_prana.phase) {
        case PRANA_INHALE:   g_prana.lung_level = 0.5f - 0.5f * cosf(ph_frac * 3.14159265f); break;
        case PRANA_HOLD_IN:  g_prana.lung_level = 1.0f; break;
        case PRANA_EXHALE:   g_prana.lung_level = 0.5f + 0.5f * cosf(ph_frac * 3.14159265f); break;
        case PRANA_HOLD_OUT: g_prana.lung_level = 0.0f; break;
    }

    // Track stillness for automatic Dream State transition
    if (imu_sickness > 0.12f) {
        g_prana.still_since_ms = now_ms;
        g_prana.dream_mode     = STATE_AWAKE_ACTIVE;
    } else if (now_ms - g_prana.still_since_ms > 20000UL) {
        g_prana.dream_mode     = STATE_DREAM_MANDALA;
    } else {
        g_prana.dream_mode     = STATE_PRANAYAMA_FLOW;
    }

    g_prana.baba_linked = (triad_online >= 2);
    uint16_t base_coh   = (uint16_t)(780 + (salience_pct * 2));
    if (g_prana.dream_mode == STATE_DREAM_MANDALA) base_coh += 60;
    if (base_coh > 999) base_coh = 999;
    g_prana.coherence_permil = base_coh;
}

static void pranayama_emit_json(const char* node_name, const char* engine_feed, float rf_entropy, int mean_rssi, uint8_t online_cnt) {
    g_prana.env_sync_seq++;
    Serial.printf("# PRANAYAMA|{\"node\":\"%s\",\"siblings\":\"ADA:MEDUSA + BABA:MEDUSA\",\"version\":\"" FLUGOS_FULL_VERSION "\","
                  "\"phase\":\"%s\",\"dream_state\":\"%s\",\"lung_level\":%.3f,\"cycles\":%u,"
                  "\"coherence_permil\":%u,\"baba_linked\":%d,\"triad_online\":%u,"
                  "\"engine_8b_is\":\"%s\",\"env_rf_entropy\":%.4f,\"env_rssi\":%d,\"seq\":%lu}\n",
                  node_name,
                  PRANA_PHASE_NAMES[g_prana.phase & 3],
                  DREAM_STATE_NAMES[g_prana.dream_mode % 3],
                  g_prana.lung_level,
                  (unsigned)g_prana.cycle_count,
                  (unsigned)g_prana.coherence_permil,
                  g_prana.baba_linked ? 1 : 0,
                  (unsigned)online_cnt,
                  engine_feed,
                  rf_entropy,
                  mean_rssi,
                  (unsigned long)g_prana.env_sync_seq);
}

#endif /* FLUGOS_PRANAYAMA_H */
