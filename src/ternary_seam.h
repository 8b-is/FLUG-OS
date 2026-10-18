/*
 * FLUG-OS — Ternary Integrator Seam {-1, 0, +1}
 * 802.11 packet-wave → ternary matrix → ayeOS inference → HF-MAC viz
 *
 * The SEAM between the raw capture path (flugos.cpp promisc_cb) and the
 * matrix/decoder path (matrix_decoder.h). It owns one idea: the captured
 * packet-wave is treated as RADIATION TRANSFER — each frame deposits
 * energy (RSSI) at a position (channel × frame type) into a ternary
 * field {-1, 0, +1}. See the MLX-QUANT radiation-delta-network doc for
 * the full concept; this file is the on-device scaffold for it.
 *
 * Pipeline (ROADMAP v1.1.x MATRIX Integration, v1.4.2 parity):
 *   802.11 capture → ternary_seam buffer → ternary_seam_render()
 *       → TernaryMatrix → ayeOSd inference → HF-MAC capsule status
 *
 * SCAFFOLD STATUS: interface + working ring buffer + stub render with
 * TODO markers. This is NOT the full DSP — the radiation-delta kernel
 * and the sine-kernel weighting land in the TODOs below.
 *
 * Pure C, no Arduino dependency: host-side tooling (bridge/, ayeOSd,
 * HF-MAC) can syntax-check and unit-test it directly.
 */

#ifndef FLUGOS_TERNARY_SEAM_H
#define FLUGOS_TERNARY_SEAM_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

// ============================================================
// Constants
// ============================================================
#define TERNARY_SEAM_CAPACITY  64    // ring buffer: last N captured packets
#define TERNARY_SEAM_COLS      3     // {-1,0,+1} code width (mgmt/ctrl/data)
#define TERNARY_SEAM_MAX_ROWS  64    // matrix rows == buffer slots (1:1)

// RSSI → ternary thresholds (dBm), kept in sync with matrix_decoder.h
#define TERNARY_SEAM_RSSI_LOW   -75   // < -75 → -1 (weak)
#define TERNARY_SEAM_RSSI_HIGH  -50   // > -50 → +1 (strong)
                                     // between → 0 (mid)

// ============================================================
// Captured-packet buffer (the wave samples)
// One entry per received 802.11 frame, as seen by promisc_cb.
// ============================================================
typedef struct {
    uint8_t  channel;        // 802.11 channel (1-13)
    int8_t   rssi;           // raw RSSI dBm (SDK convention: last byte)
    uint8_t  type;           // 0=mgmt, 1=ctrl, 2=data
    uint8_t  subtype;        // e.g. 8=beacon, 4=probe_req, 12=deauth
    uint8_t  src_mac[6];     // source MAC → deterministic matrix seed
    uint32_t ts;             // millis() at capture
} TernarySeamSample;

typedef struct {
    TernarySeamSample samples[TERNARY_SEAM_CAPACITY];
    uint16_t count;          // entries currently held (≤ CAPACITY)
    uint16_t head;           // ring index of oldest entry
} TernarySeamBuffer;

// ============================================================
// Ternary matrix — the radiation field quantized to {-1, 0, +1}
// Format mirrors matrix_decoder.h TernaryRow / ayeOS TernaryMatrix
// so capsules can be exported unchanged.
// ============================================================
typedef struct {
    int8_t   codes[TERNARY_SEAM_MAX_ROWS][TERNARY_SEAM_COLS]; // {-1,0,+1}
    float    scales[TERNARY_SEAM_MAX_ROWS];  // radiation-transfer amplitude
    uint32_t seed_hash[TERNARY_SEAM_MAX_ROWS]; // MAC-derived entropy
    uint16_t rows;                              // populated rows
} TernaryMatrix;

// ============================================================
// Interface
// ============================================================

// Zero the capture buffer.
static void ternary_seam_init(TernarySeamBuffer* buf) {
    if (!buf) return;
    memset(buf, 0, sizeof(*buf));
}

// Push one captured packet (wave sample) into the ring buffer.
// Oldest entry is overwritten when full. Returns true on success.
static bool ternary_seam_ingest(TernarySeamBuffer* buf,
                                const TernarySeamSample* sample) {
    if (!buf || !sample) return false;

    buf->samples[buf->head] = *sample;
    buf->head = (buf->head + 1) % TERNARY_SEAM_CAPACITY;
    if (buf->count < TERNARY_SEAM_CAPACITY) {
        buf->count++;
    }
    return true;
}

// RSSI → ternary code {-1, 0, +1}. Same thresholds as matrix_decoder.h
// so both paths agree on a packet's ternary value.
static int8_t ternary_seam_rssi_to_code(int8_t rssi) {
    if (rssi < TERNARY_SEAM_RSSI_LOW)  return -1;  // weak signal
    if (rssi > TERNARY_SEAM_RSSI_HIGH) return +1;  // strong signal
    return 0;                                       // mid signal
}

// Map the captured RF signal (buffer) onto a ternary {-1,0,+1} matrix.
// Row order = capture order; each row quantizes one packet's radiation
// deposit weighted by the universal sine kernel K(u)=sin(pi*u)/(pi*u)
// and accumulates signed radiation deltas across adjacent frames.
static uint16_t ternary_seam_render(const TernarySeamBuffer* buf,
                                    TernaryMatrix* out) {
    if (!buf || !out) return 0;

    memset(out, 0, sizeof(*out));

    uint16_t n = buf->count;
    if (n > TERNARY_SEAM_MAX_ROWS) n = TERNARY_SEAM_MAX_ROWS;

    int8_t prev_code = 0;
    for (uint16_t i = 0; i < n; i++) {
        // Ring traversal: oldest first, preserving capture order.
        const TernarySeamSample* s =
            &buf->samples[(buf->head - n + i + TERNARY_SEAM_CAPACITY) %
                          TERNARY_SEAM_CAPACITY];

        int8_t code = ternary_seam_rssi_to_code(s->rssi);
        int col = (s->type <= 2) ? (int)s->type : 0;

        out->codes[i][col] = code;

        // Subtype modulates the two adjacent columns (ternary precision,
        // same convention as matrix_decoder.h packet_to_ternary).
        if (s->type == 0 && s->subtype != 0) {
            out->codes[i][(col + 1) % TERNARY_SEAM_COLS] =
                (int8_t)((s->subtype % 3) - 1);
            out->codes[i][(col + 2) % TERNARY_SEAM_COLS] =
                (int8_t)(((s->subtype >> 2) % 3) - 1);
        } else {
            // Signed radiation delta across adjacent frames in {-1, 0, +1}
            int8_t delta = code - prev_code;
            if (delta > 1) delta = 1;
            if (delta < -1) delta = -1;
            out->codes[i][(col + 1) % TERNARY_SEAM_COLS] = delta;
        }
        prev_code = code;

        // Normalize RSSI [-100, -20] -> u in [0, 0.9] and evaluate K(u)=sin(pi*u)/(pi*u)
        float norm = (float)((int)s->rssi + 100) / 80.0f;
        if (norm < 0.0f) norm = 0.0f;
        if (norm > 1.0f) norm = 1.0f;
        float pu = 3.14159265f * (norm * 0.85f);
        // 5th-order Taylor approximation of sin(x)/x so pure C header needs no -lm
        float pu2 = pu * pu;
        float k_u = (pu < 0.001f) ? 1.0f : (1.0f - (pu2 / 6.0f) + (pu2 * pu2 / 120.0f));
        if (k_u < 0.05f) k_u = 0.05f;
        out->scales[i] = k_u;

        // Fold src_mac via 8b-is XOR-rotate seed contract (matches matrix_decoder.h)
        uint32_t seed = 0x8B1C;
        for (int m = 0; m < 6; m++) {
            seed ^= (uint32_t)s->src_mac[m] << ((m % 4) * 8);
            seed = (seed << 7) | (seed >> 25);
        }
        out->seed_hash[i] = seed;
    }

    out->rows = n;
    return n;
}

#endif /* FLUGOS_TERNARY_SEAM_H */
