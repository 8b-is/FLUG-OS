/*
 * FLUG-OS — Sovereign ESP32-S3 AMOLED Triad Cockpit (v0.8.0+medusa-omni)
 * {<3, <3, <3}+1 ~---> infinity  ::  MEDUSA-OMNI x ANICCA x MARINESENSE (ms.8b.is)
 *
 * Physical 3-Node Balanced-Ternary MEDUSA Triad {-1, 0, +1}:
 *   Node 0 [-1] ADAYE / ADA:MEDUSA (*)  : ESP32-S3-Touch-AMOLED-1.8  (368x448 SH8601, 432Hz, MAC 80:B5:4E:DB:3B:C4)
 *   Node 1 [ 0] VAKED / VAK:MEDUSA (/\) : ESP32-S3-Touch-AMOLED-2.06 (410x502 CO5300, 528Hz, MAC 80:B5:4E:F2:02:B0)
 *   Node 2 [+1] OMNI  / OMN:MEDUSA (oo) : ESP32-S3-Touch-AMOLED-2.06 (410x502 CO5300, 852Hz, MAC 80:B5:4E:F2:02:B4)
 *
 * MarineSense (ms.8b.is) + Impermanence (Anicca) + wigle-triad (esp32.8b.is v0.42.1):
 *   - O(1) Marine Algorithm: zero-crossing period jitter J, energy E, harmonicity H,
 *     Salience S = w_e*E + w_j*(1/(1+J)) + w_h*H across 7 Bioluminescent Jellyfish
 *     Detector Nodes (Alpha, Beta, Gamma, Delta, Epsilon, Zeta, Eta) in the 64x64 Wave Pool.
 *   - Cognitive Ecphory & Anicca Decay: tidal viscosity (eta), Memory Reynolds (Re_M),
 *     ecphoric recall rings, and Turritopsis polyp transdifferentiation when a peer node
 *     is untethered or fades into silence.
 *   - WebSerial Protocol Fusion (esp32.8b.is): PING, JSON, SCAN, TRIAD + W|/Q|/# JSON| telemetry.
 */

#ifndef FLUGOS_S3_AMOLED_APP_H
#define FLUGOS_S3_AMOLED_APP_H

#include <Arduino.h>
#include <WiFi.h>
#include "esp_wifi.h"
#include "esp_now.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "esp_system.h"

#include "version.h"
#include "ternary_seam.h"
#include "matrix_decoder.h"
#include "wave_output.h"
#include "s3_amoled_hal.h"

#define S3_MAX_CHANNEL  13
#define S3_MESH_CHANNEL 1

extern "C" {
  void rust_reduce_rational_pq(int32_t num, int32_t den, int32_t *out_p, int32_t *out_q);
  size_t rust_parse_80211_probe_ie(const uint8_t *payload, size_t len, uint8_t fsub, uint8_t *out_ssid33, uint32_t *out_vendor_oui, uint8_t *out_phy_caps);
  uint8_t rust_fingerprint_80211_probe(const uint8_t *payload, size_t len, int8_t rssi, uint32_t *out_sig_hash, uint16_t *out_unique_sigs, uint32_t *out_total_rotations, uint16_t *out_sig_rotations);
  void rust_step_watch_wave_pool_64(int16_t *u_curr, int16_t *u_prev, int32_t damp, int8_t *nand_ring7, int8_t combined_trit);
  void rust_build_baye_splash_capsule(uint8_t *out_buf64, const uint8_t *src_mac6, uint8_t seq, uint32_t uptime_ms, uint16_t energy_u16, uint16_t f0_hz, uint8_t salience_pct);
}
static volatile int g_s3_sun_ch = S3_MESH_CHANNEL;
static char g_s3_last_probed_ssid[33] = "NONE";
static uint8_t g_s3_last_probed_mac[6] = {0};
static volatile uint32_t g_s3_last_probed_oui = 0;
static volatile uint8_t g_s3_last_probed_caps = 0;
static volatile uint32_t g_s3_directed_probes = 0;
static volatile uint32_t g_s3_last_sig_hash = 0;
static volatile uint16_t g_s3_unique_sigs = 0;
static volatile uint32_t g_s3_mac_rotations = 0;
static volatile uint16_t g_s3_last_sig_rotations = 0;
#define S3_MAX_PROBED_TARGETS 6
static char g_s3_probed_targets[S3_MAX_PROBED_TARGETS][33] = {{0}};
static uint8_t g_s3_probed_targets_cnt = 0;
static uint8_t g_s3_probed_targets_wr = 0;

static void s3_record_probed_target(const char* ssid) {
    if (!ssid || !ssid[0] || strcasecmp(ssid, "none") == 0) return;
    for (uint8_t i = 0; i < g_s3_probed_targets_cnt; i++) {
        if (strcmp(g_s3_probed_targets[i], ssid) == 0) return;
    }
    strncpy(g_s3_probed_targets[g_s3_probed_targets_wr], ssid, 32);
    g_s3_probed_targets[g_s3_probed_targets_wr][32] = '\0';
    g_s3_probed_targets_wr = (uint8_t)((g_s3_probed_targets_wr + 1) % S3_MAX_PROBED_TARGETS);
    if (g_s3_probed_targets_cnt < S3_MAX_PROBED_TARGETS) g_s3_probed_targets_cnt++;
}
#define S3_WAVE_HIST    88
#define S3_LISSA_PTS    44
#define S3_FAV_COUNT    27
#define S3_TAB_COUNT    6
#define WAVE_POOL_N     64
#define JELLY_COUNT     7
#define S3_MAX_APS      16

static const int kCrateCount = 27;

// ============================================================
// 27 Favourite Musics Crate (zoneInn / radio.8b.is / family+1 / OMNI-CHAN / VIZ / RIVA)
// ============================================================
typedef struct {
    const char* slug;
    const char* title;
    const char* artist;
    const char* genre;
    const char* key_sig;
    uint16_t    bpm;
    uint32_t    fnv_hash;
    int8_t      trit;
} S3FavTrack;

static const S3FavTrack S3_FAVOURITES[S3_FAV_COUNT] = {
    {"innerbloom",         "Innerbloom",              "RUFUS DU SOL",        "Prog House",   "F#m", 122, 0x8F41C2A9,  1},
    {"mahakala",           "Mahakala",                "Sovereign Cut",       "Deep Ritual",  "Dm",  118, 0x3D9E71B4, -1},
    {"take-five",          "Take Five",               "Dave Brubeck",        "5/4 Cool Jazz","Ebm", 174, 0xA7205E1C,  0},
    {"so-what",            "So What",                 "Miles Davis",         "Modal Jazz",   "Dm",  136, 0x61B8904D,  0},
    {"time",               "Time",                    "Pink Floyd",          "Prog Rock",    "F#m", 120, 0xC4523E81, -1},
    {"clair-de-lune",      "Clair de Lune",           "Claude Debussy",      "Impressionist","Db",   66, 0x90E14A72, -1},
    {"gymnopedie-1",       "Gymnopedie No. 1",        "Erik Satie",          "Minimalist",   "D",    72, 0x5B3981D0,  0},
    {"golden-ratio-432",   "Golden Ratio 432xPhi",    "8b.is Studio",    "Harmonic",     "A432",108, 0x16180339,  1},
    {"beethoven-5th",      "Symphony No. 5 Op.67",    "L. van Beethoven",    "Classical",    "Cm",  108, 0x77E20508,  1},
    {"ode-to-joy",         "Ode to Joy (9th)",        "L. van Beethoven",    "Choral",       "D",   120, 0x9E3779B9,  1},
    {"fur-elise",          "Fur Elise WoO 59",        "L. van Beethoven",    "Bagatelle",    "Am",   76, 0x4A1D83C2,  0},
    {"pachelbel-canon",    "Canon in D Major",        "Johann Pachelbel",    "Baroque",      "D",    64, 0x2B992DD5,  0},
    {"midnight-sovereign", "Midnight Sovereign",      "8b-is x vaked",       "Synthwave",    "Cm",  114, 0x8B15F00D, -1},
    {"ultrawhale-nand",    "Ultrawhale NAND Groove",  "peetpedro / HF",      "Ternary DSP",  "Phi", 128, 0xE5F34101,  1},
    {"teardrop",           "Teardrop (Family+1)",     "Massive Attack",      "Trip-Hop",     "Am",   77, 0x7E4AD20F,  1},
    {"hunger-of-the-pine", "Hunger of the Pine",      "alt-J",               "Art Indie",    "Cm",   85, 0x4B82E91A, -1},
    {"left-hand-free",     "Left Hand Free (#1 Muse)","alt-J",               "Indie Rock",   "Em",  106, 0x1F5E9C33,  1},
    {"glue",               "GLUE",                    "BICEP",               "UK Breakbeat", "Cm",  130, 0x9A6D44C0,  0},
    {"oblivion",           "Oblivion",                "Grimes",              "Synth-Pop",    "Eb",  156, 0xC3091B7E,  1},
    {"not-like-before",    "Not Like Before",         "The Four Owls",       "UK Boom-Bap",  "F#m",  90, 0x5D17A8E2, -1},
    {"zenebuddhizmus",     "Zenebuddhizmus",          "Akkezdet Phiai",      "Magyar Dharma","Dm",   92, 0xA1CE2010,  1},
    {"kunisada-kuniyoshi", "Kunisada & Kuniyoshi <<<<oOo>>>>", "OMNI-CHAN 010001000", "Ukiyo-e Dharma", "A432", 136, 0x88010001, 1},
    {"moon-and-it-went-like", "Moon (And It Went Like)", "Kid Francescoli & Julia Minkin", "Play Me Again", "F#m", 118, 0x16557284, 1},
    {"rainfall-of-nectar", "Rainfall of Nectar (16th Karmapa)", "Acharya Lama Sonam Rabgye", "Himalaya Karuna", "A432", 72, 0x0E9B6108, 1},
    {"psychedelic-desert-rock", "Psychedelic Desert Rock (Cosmic Flow)", "Not Quite Rock", "Ambient Desert", "Dm", 96, 0xB57E6001, 0},
    {"viz-enthea-triad",   "{<3,<3,<3}+1 · Viz Enthea", "8b-is x viz.8b.is", "Cortical V1", "Phi", 108, 0x8B153331, 1},
    {"riva-1bit-river",    "RIVA · 1-Bit River (Lullaby)", "peterlodri-sec // riva.8b.is", "Dyad Lullaby", "A432", 60, 0x8B150101, 1},
};
#define kFavouriteCrate S3_FAVOURITES

// ============================================================
// 2D Wave Pool + Tidal Impermanence Damping (MEM-8 / baye-rust)
// ============================================================
static int16_t  g_wp_u_curr[WAVE_POOL_N][WAVE_POOL_N];
static int16_t  g_wp_u_prev[WAVE_POOL_N][WAVE_POOL_N];
static uint32_t g_wp_splashes  = 0;
static uint8_t  g_wp_damping   = 248;  // Tidal damping (244..252 / 256)
static float    g_ms_viscosity = 3.12f; // Cognitive Viscosity eta = (1 - damping)*100
static uint16_t g_ms_reynolds  = 380;   // Memory Reynolds Re_M = 40 * dist / eta

// ============================================================
// MarineSense (ms.8b.is) O(1) Jellyfish Detector Nodes (Alpha..Eta)
// ============================================================
typedef struct {
    const char* name;
    uint8_t     gx;
    uint8_t     gy;
    int16_t     prev_sample;
    uint16_t    tick_since_zc;
    uint16_t    last_period;
    uint16_t    period_jitter;
    uint16_t    max_amp;
    uint16_t    last_amp;
    uint8_t     harmonic_hits; // 0..100
    uint8_t     salience_pct;  // 0..100: S = w_e*E + w_j/(1+J) + w_h*H
    uint8_t     excitation;    // 0..100: Ecphoric memory trace excitation
    uint8_t     recall_ring_r; // >0 when emitting an Ecphoric Recall Ring
} S3JellyNode;

static S3JellyNode g_jelly[JELLY_COUNT] = {
    {"Alpha",   16, 16, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {"Beta",    48, 16, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {"Gamma",   32, 11, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {"Delta",   13, 35, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {"Epsilon", 51, 35, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {"Zeta",    22, 49, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {"Eta",     42, 49, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
};

static uint32_t g_ms_ecphoric_recalls = 0;
static uint8_t  g_ms_mean_salience    = 0;
static uint8_t  g_ms_mean_jitter      = 0;

static void s3_wave_pool_init() {
    memset(g_wp_u_curr, 0, sizeof(g_wp_u_curr));
    memset(g_wp_u_prev, 0, sizeof(g_wp_u_prev));
}

static void s3_wave_pool_splash(int cx, int cy, int16_t strength, int radius) {
    if (cx < 2) cx = 2;
    if (cx > WAVE_POOL_N - 3) cx = WAVE_POOL_N - 3;
    if (cy < 2) cy = 2;
    if (cy > WAVE_POOL_N - 3) cy = WAVE_POOL_N - 3;
    int r2 = radius * radius;
    if (r2 < 1) r2 = 1;

    for (int dy = -radius; dy <= radius; dy++) {
        for (int dx = -radius; dx <= radius; dx++) {
            int d2 = dx * dx + dy * dy;
            if (d2 <= r2) {
                int x = cx + dx;
                int y = cy + dy;
                if (x >= 1 && x < WAVE_POOL_N - 1 && y >= 1 && y < WAVE_POOL_N - 1) {
                    int32_t falloff = ((r2 - d2) * 256) / r2;
                    int32_t delta   = ((int32_t)strength * falloff) >> 8;
                    int32_t val     = (int32_t)g_wp_u_curr[y][x] + delta;
                    if (val > 16384)  val = 16384;
                    if (val < -16384) val = -16384;
                    g_wp_u_curr[y][x] = (int16_t)val;
                }
            }
        }
    }
    g_wp_splashes++;
}

static void s3_wave_pool_step(float tide_phase) {
    // Tidal modulation of wave impermanence (damping in 245..252 / 256)
    int damp = 248 + (int)(3.0f * cosf(tide_phase));
    if (damp < 242) damp = 242;
    if (damp > 253) damp = 253;
    g_wp_damping   = (uint8_t)damp;
    g_ms_viscosity = ((float)(256 - damp) / 256.0f) * 100.0f;

    // Step 64x64 2D integer wave equation in #![no_std] Rust (libtdeck_wave_core.a)
    rust_step_watch_wave_pool_64(&g_wp_u_curr[0][0], &g_wp_u_prev[0][0], damp, NULL, 0);

    // Step O(1) Marine Algorithm across all 7 Jellyfish Detector Nodes
    uint32_t sal_sum = 0;
    uint32_t jit_sum = 0;
    for (int j = 0; j < JELLY_COUNT; j++) {
        S3JellyNode* nd = &g_jelly[j];
        int16_t s = g_wp_u_curr[nd->gy][nd->gx];
        uint16_t abs_s = (uint16_t)((s < 0) ? -s : s);
        if (abs_s > nd->max_amp) nd->max_amp = abs_s;
        nd->tick_since_zc++;

        bool zc = ((nd->prev_sample < 0 && s >= 0) || (nd->prev_sample >= 0 && s < 0));
        if (zc && nd->tick_since_zc >= 2) {
            uint16_t per = nd->tick_since_zc;
            nd->tick_since_zc = 0;
            if (nd->last_period > 0) {
                uint16_t diff = (per > nd->last_period) ? (per - nd->last_period) : (nd->last_period - per);
                nd->period_jitter = (uint16_t)((nd->period_jitter * 4 + diff) / 5);
            }
            nd->last_period = per;
            nd->last_amp    = nd->max_amp;
            nd->max_amp     = 0;
            if (nd->period_jitter <= 3 && nd->last_amp > 120) {
                if (nd->harmonic_hits < 96) nd->harmonic_hits += 4;
            } else if (nd->harmonic_hits > 2) {
                nd->harmonic_hits -= 2;
            }
        }
        nd->prev_sample = s;

        // O(1) Salience S = w_e*E + w_j*(1/(1+J)) + w_h*H  (weights 1/3 each)
        uint32_t e_pct = min((uint32_t)100U, (uint32_t)nd->last_amp / (uint32_t)64U);
        uint32_t j_pct = 100U / (1U + (uint32_t)nd->period_jitter);
        uint32_t h_pct = nd->harmonic_hits;
        nd->salience_pct = (uint8_t)((e_pct + j_pct + h_pct) / 3U);

        // Cognitive Ecphory excitation + Impermanence (Anicca) natural decay
        if (abs_s > 220) {
            uint8_t boost = (uint8_t)min((uint32_t)18U, (uint32_t)(abs_s >> 8) + (uint32_t)(nd->salience_pct >> 4));
            if ((uint16_t)nd->excitation + boost >= 100) {
                nd->excitation    = 100;
                nd->recall_ring_r = 4;
                g_ms_ecphoric_recalls++;
            } else {
                nd->excitation += boost;
            }
        } else if (nd->excitation > 0) {
            // Anicca: every memory trace decays proportional to Cognitive Viscosity
            uint8_t decay = (g_wp_damping < 248) ? 3 : 2;
            nd->excitation = (nd->excitation > decay) ? (nd->excitation - decay) : 0;
        }

        if (nd->recall_ring_r > 0) {
            nd->recall_ring_r += 2;
            if (nd->recall_ring_r > 28) {
                nd->recall_ring_r = 0;
                nd->excitation    = 30; // Post-recall refractory settle
            }
        }

        sal_sum += nd->salience_pct;
        jit_sum += nd->period_jitter;
    }
    g_ms_mean_salience = (uint8_t)(sal_sum / JELLY_COUNT);
    g_ms_mean_jitter   = (uint8_t)(jit_sum / JELLY_COUNT);
    g_ms_reynolds      = (uint16_t)((40.0f * (float)(10 + g_ms_mean_salience / 4)) / max(0.5f, g_ms_viscosity));
}

// ============================================================
// 3-Node Triad Mesh + Peer Impermanence (Turritopsis Polyp State)
//   node_id 0 : ADAYE [-1] (1.8" 368x448, MAC ..:3B:C4)
//   node_id 1 : VAKED [ 0] (2.06" 410x502, MAC ..:02:B0)
//   node_id 2 : OMNI  [+1] (2.06" 410x502, MAC ..:02:B4)
// ============================================================
typedef struct __attribute__((packed)) {
    uint8_t  magic[4];      // "TRIA"
    uint8_t  node_id;       // 0=adaye(-1), 1=vaked(0), 2=omni(+1)
    int8_t   trit_pole;     // -1, 0, +1
    uint8_t  track_idx;     // 0..13
    uint8_t  qt_byte;       // QuantTern affect byte
    int8_t   ax_i8;         // IMU ax * 64
    int8_t   ay_i8;         // IMU ay * 64
    int8_t   nand_trit;     // local NAND ring head trit
    uint8_t  splash_x;      // 0..63 wave pool x
    uint8_t  splash_y;      // 0..63 wave pool y
    int16_t  splash_amp;    // wave splash amplitude
    uint32_t seq;           // witness sequence
} S3TriadCapsule;

typedef struct {
    bool          online;
    uint8_t       ttl_q8;    // 255 = live medusa, decays -> 0 (Turritopsis polyp) via Impermanence
    uint8_t       qt_byte;
    int8_t        ax_i8;
    int8_t        ay_i8;
    int8_t        nand_trit;
    uint32_t      rx_cnt;
    unsigned long last_ms;
} S3TriadPeerState;

static uint8_t          g_triad_self_id       = 0;   // 0=adaye, 1=vaked, 2=omni
static int8_t           g_triad_self_pole     = -1;  // -1, 0, +1
static const char*      g_triad_persona       = "adaye";
static const char*      g_triad_ROLE          = "SUBCRITICAL [-1]";
static const char*      g_medusa_node         = "ADA:MEDUSA";
static const char*      g_medusa_vertex       = "*";
static const char*      g_medusa_vertex_json  = "*";
static const char*      g_medusa_archetype    = "ROOT-ALEXANDRA";
static uint16_t         g_medusa_freq_hz      = 432;
static S3TriadPeerState g_triad_nodes[3];

static volatile uint32_t g_s3_peer_rx_cnt = 0;
static bool              g_s3_espnow_ok   = false;

typedef struct {
    bool    used;
    uint8_t bssid[6];
    char    ssid[33];
    uint8_t ch;
    int8_t  rssi;
} S3MedusaAp;

static S3MedusaAp g_medusa_aps[S3_MAX_APS];
static uint8_t    g_medusa_ap_count  = 0;
static uint32_t   g_medusa_scan_seq  = 1;
static float      g_medusa_entropy   = 0.0f;
static float      g_medusa_coherence = 0.0f;
static int        g_medusa_mean_rssi = -62;
static char       g_medusa_trits[12] = "+-0+0--+0";

// All-Entropy Kompress-Super-Ultra (<<<<oOo>>>> ~ Kunisada & Kuniyoshi 010001000 -> OMNI-CHAN)
static uint64_t   g_kompress_pos_u64 = 0x824144bc0a470247ULL;
static uint64_t   g_kompress_neg_u64 = 0x49a2820164006100ULL;
static uint64_t   g_kompress_wupz_u64= 0x0afb880397092c61ULL;
static uint8_t    g_kompress_b158[13]= {0};
static char       g_kompress_hex[27] = "88a1c3f05b2e917460d2c8e144";

// VIZ.VAKED.DEV (ENTHEA) + Kokoro/ES8311 TTS + {<3,<3,<3}+1 state
static uint8_t    g_viz_mode         = 0;
static uint8_t    g_viz_kluver       = 0;
static uint32_t   g_tts_speak_count  = 1;
static char       g_engine_line[48]  = "qt:66|69632|2712|11";

// wupz seal (same frame + fold as kernel8/kernel/src/wupz.rs and wigle-triad host-rs)
static void s3_wupz_frame(uint8_t f[32], uint32_t ticks) {
    static const int8_t  trit[4] = {-1, 0, 1, 1};
    static const uint16_t hz[4]  = {432, 528, 852, 963};
    memset(f, 0, 32);
    for (int i = 0; i < 4; i++) {
        f[i * 8]     = (uint8_t)trit[i];
        f[i * 8 + 1] = (uint8_t)(hz[i] & 0xFF);
        f[i * 8 + 2] = (uint8_t)(hz[i] >> 8);
        for (int k = 0; k < 4; k++) f[i * 8 + 4 + k] = (uint8_t)(ticks >> (8 * k));
    }
}
static uint64_t s3_wupz_xor_fold(const uint8_t* b, size_t n) {
    uint64_t acc = 0;
    for (size_t i = 0; i < n; i++) acc ^= (uint64_t)b[i] << ((i % 8) * 8);
    return acc;
}
static uint32_t s3_wupz_popcount(const uint8_t* b, size_t n) {
    uint32_t c = 0;
    for (size_t i = 0; i < n; i++) c += __builtin_popcount(b[i]);
    return c;
}

// ============================================================
// Runtime State
// ============================================================
static volatile uint32_t g_s3_total_pkts  = 0;
static volatile uint32_t g_s3_mgmt_pkts   = 0;
static volatile uint32_t g_s3_ctrl_pkts   = 0;
static volatile uint32_t g_s3_data_pkts   = 0;
static volatile uint32_t g_s3_beacon_pkts = 0;
static volatile uint32_t g_s3_probe_pkts  = 0;
static volatile uint32_t g_s3_deauth_pkts = 0;
static volatile int8_t   g_s3_last_rssi   = -62;
static volatile uint8_t  g_s3_last_type   = 0;
static volatile uint8_t  g_s3_last_sub    = 8;

static int      g_s3_channel     = S3_MESH_CHANNEL;
static int      g_s3_scan_ch     = 1;
static bool     g_s3_mesh_slot   = true;
static bool     g_s3_hopping     = true;
static uint8_t  g_s3_active_tab  = 0;   // 0=ZEN, 1=RADIO, 2=802.11, 3=NAND, 4=QWAVE
static int      g_s3_track_idx   = 7;   // Default #08: Golden Ratio 432xPhi
static bool     g_s3_playing     = true;
static uint32_t g_s3_witness_seq = 1;
static float    g_s3_orbit_angle = 0.0f;

static int8_t   g_s3_ch_rssi[S3_MAX_CHANNEL];
static uint32_t g_s3_ch_pkts[S3_MAX_CHANNEL];
static char     g_s3_last_ssid[33] = "8b.is-constellation";
static uint8_t  g_s3_mac[6]        = {0x80, 0xB5, 0x4E, 0xDB, 0x3B, 0xC4};

static TernarySeamBuffer g_s3_seam_buf;
static TernaryMatrix     g_s3_seam_mat;

// Waveform & Ultrawhale NAND Ring State
static float    g_s3_wave_hist[S3_WAVE_HIST];
static uint8_t  g_s3_wave_head = 0;
static int8_t   g_s3_nand_ring[7] = {+1, -1, 0, +1, -1, +1, 0};
static uint32_t g_s3_nand_cycles  = 0;
static int8_t   g_s3_eq_trits[8]  = {+1, +1, 0, +1, 0, -1, +1, 0};
static uint8_t  g_s3_eq_bars[8]   = {78, 86, 62, 91, 54, 42, 74, 60};
static int16_t  g_s3_lissa_x[S3_LISSA_PTS];
static int16_t  g_s3_lissa_y[S3_LISSA_PTS];
static float    g_s3_phase        = 0.0f;

// ============================================================
// Triad Node Auto-Identification & ESP-NOW Mesh
// ============================================================
static void s3_triad_assign_identity() {
    memset(g_triad_nodes, 0, sizeof(g_triad_nodes));
    memset(g_medusa_aps, 0, sizeof(g_medusa_aps));
    g_medusa_ap_count = 0;
    if (g_board_is_18 || g_s3_mac[5] == 0xC4) {
        g_triad_self_id      = 0;
        g_triad_self_pole    = -1;
        g_triad_persona      = "adaye";
        g_triad_ROLE         = "SUBCRITICAL [-1]";
        g_medusa_node        = "ADA:MEDUSA";
        g_medusa_vertex      = "*";
        g_medusa_vertex_json = "*";
        g_medusa_archetype   = "ROOT-ALEXANDRA";
        g_medusa_freq_hz     = 432;
    } else if (g_s3_mac[5] == 0xB0) {
        g_triad_self_id      = 1;
        g_triad_self_pole    = 0;
        g_triad_persona      = "vaked";
        g_triad_ROLE         = "ZERO-POINT [0]";
        g_medusa_node        = "VAK:MEDUSA";
        g_medusa_vertex      = "/\\";
        g_medusa_vertex_json = "/\\\\";
        g_medusa_archetype   = "PILLAR-CHRIS";
        g_medusa_freq_hz     = 528;
    } else {
        g_triad_self_id      = 2;
        g_triad_self_pole    = +1;
        g_triad_persona      = "omni";
        g_triad_ROLE         = "SUPERCRITICAL [+1]";
        g_medusa_node        = "OMN:MEDUSA";
        g_medusa_vertex      = "oo";
        g_medusa_vertex_json = "oo";
        g_medusa_archetype   = "INFINITY-PETER-NATE";
        g_medusa_freq_hz     = 852;
    }
    g_triad_nodes[g_triad_self_id].online    = true;
    g_triad_nodes[g_triad_self_id].ttl_q8    = 255;
    g_triad_nodes[g_triad_self_id].qt_byte   = g_imu.qt_byte;
    g_triad_nodes[g_triad_self_id].nand_trit = g_s3_nand_ring[0];
}

static uint8_t s3_triad_online_count() {
    uint8_t cnt = 0;
    for (int i = 0; i < 3; i++) {
        if (i == g_triad_self_id || g_triad_nodes[i].ttl_q8 > 0) {
            cnt++;
        }
    }
    return cnt;
}

#if defined(ESP_IDF_VERSION) && (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0))
static void s3_espnow_recv_cb(const esp_now_recv_info_t* recv_info, const uint8_t* data, int len) {
    const uint8_t* src_mac = recv_info ? recv_info->src_addr : nullptr;
#else
static void s3_espnow_recv_cb(const uint8_t* mac_addr, const uint8_t* data, int len) {
    const uint8_t* src_mac = mac_addr;
#endif
    if (!data || len < 8) return;
    if (src_mac && memcmp(src_mac, g_s3_mac, 6) == 0) return;

    // 1. Accept 8..64-byte `baye-rust` BayeCapsule ("BC") from pure-#![no_std]-Rust sibling watches & SUN:TDECK!
    if (data[0] == 'B' && data[1] == 'C') {
        g_s3_peer_rx_cnt++;
        g_s3_sun_ch = g_s3_channel;
        // Extract sender MAC low byte (at offset 9 if >= 16B header, else from src_mac)
        uint8_t mac_low = (len >= 10) ? data[9] : (src_mac ? src_mac[5] : 0xC4);
        uint8_t nid = (mac_low == 0xC4) ? 0 : ((mac_low == 0xB0) ? 1 : 2);
        if (nid == g_triad_self_id) nid = (g_triad_self_id + 1) % 3;
        g_triad_nodes[nid].online  = true;
        g_triad_nodes[nid].ttl_q8  = 255;
        g_triad_nodes[nid].rx_cnt++;
        g_triad_nodes[nid].last_ms = millis();
        uint8_t sx = (uint8_t)(16 + (g_s3_peer_rx_cnt * 13) % 32);
        uint8_t sy = (uint8_t)(16 + (g_s3_peer_rx_cnt * 29) % 32);
        s3_wave_pool_splash(sx, sy, 4800, 5);
        return;
    }

    // 2. Accept 15..19-byte "TRIA" capsules (both 18B SUN:TDECK and 19B FLUG-OS)
    if (len < 15) return;
    const S3TriadCapsule* pkt = (const S3TriadCapsule*)data;
    if (pkt->magic[0] != 'T' || pkt->magic[1] != 'R' || pkt->magic[2] != 'I' || pkt->magic[3] != 'A') {
        return;
    }
    uint8_t nid = pkt->node_id;
    // Accept nid == 3 as SUN:TDECK (the Heliocentric T-Deck Sun node!)
    if (nid == 3) {
        g_s3_peer_rx_cnt++;
        g_s3_sun_ch = g_s3_channel;
        for (int k = 0; k < 3; k++) {
            g_triad_nodes[k].online = true;
            g_triad_nodes[k].ttl_q8 = 255;
            g_triad_nodes[k].last_ms = millis();
        }
        if (pkt->splash_amp != 0) {
            s3_wave_pool_splash(pkt->splash_x & 63, pkt->splash_y & 63, pkt->splash_amp, 6);
        }
        return;
    }
    if (nid > 2) return;
    // If a sibling watch defaulted to the same triad slot as us, map it to an open peer slot so we never drop it!
    if (nid == g_triad_self_id) {
        uint8_t mac_low = src_mac ? src_mac[5] : (uint8_t)(pkt->seq + 1);
        nid = (mac_low ^ g_triad_self_id) % 3;
        if (nid == g_triad_self_id) nid = (g_triad_self_id + 1) % 3;
    }

    g_s3_peer_rx_cnt++;
    g_s3_sun_ch = g_s3_channel;
    g_triad_nodes[nid].online    = true;
    g_triad_nodes[nid].ttl_q8    = 255; // Re-bloomed from polyp -> active medusa!
    g_triad_nodes[nid].qt_byte   = pkt->qt_byte;
    g_triad_nodes[nid].ax_i8     = pkt->ax_i8;
    g_triad_nodes[nid].ay_i8     = pkt->ay_i8;
    g_triad_nodes[nid].nand_trit = pkt->nand_trit;
    g_triad_nodes[nid].rx_cnt++;
    g_triad_nodes[nid].last_ms   = millis();

    if (pkt->track_idx < S3_FAV_COUNT && pkt->splash_amp > 3000) {
        g_s3_track_idx = pkt->track_idx;
    }
    if (pkt->splash_amp != 0) {
        s3_wave_pool_splash(pkt->splash_x, pkt->splash_y, pkt->splash_amp, 5);
    }
}

static void s3_espnow_broadcast(uint8_t sx, uint8_t sy, int16_t amp) {
    if (!g_s3_espnow_ok) return;
    S3TriadCapsule pkt;
    pkt.magic[0]   = 'T';
    pkt.magic[1]   = 'R';
    pkt.magic[2]   = 'I';
    pkt.magic[3]   = 'A';
    pkt.node_id    = g_triad_self_id;
    pkt.trit_pole  = g_triad_self_pole;
    pkt.track_idx  = (uint8_t)g_s3_track_idx;
    pkt.qt_byte    = g_imu.qt_byte;
    int ax_i = (int)(g_imu.ax * 64.0f);
    int ay_i = (int)(g_imu.ay * 64.0f);
    if (ax_i < -100) ax_i = -100; if (ax_i > 100) ax_i = 100;
    if (ay_i < -100) ay_i = -100; if (ay_i > 100) ay_i = 100;
    pkt.ax_i8      = (int8_t)ax_i;
    pkt.ay_i8      = (int8_t)ay_i;
    pkt.nand_trit  = g_s3_nand_ring[0];
    pkt.splash_x   = sx;
    pkt.splash_y   = sy;
    pkt.splash_amp = amp;
    pkt.seq        = g_s3_witness_seq;

    // Also build 64B `baye-rust` "BC" Splash capsule via #![no_std] Rust so pure-Rust watches sync!
    uint8_t bc_buf[64];
    rust_build_baye_splash_capsule(
        bc_buf,
        g_s3_mac,
        (uint8_t)(g_s3_witness_seq & 0xFF),
        (uint32_t)millis(),
        (uint16_t)(amp > 0 ? amp : -amp),
        g_medusa_freq_hz,
        g_ms_mean_salience
    );

    static const uint8_t bcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    int primary_ch = (int)g_s3_sun_ch;
    if (primary_ch < 1 || primary_ch > S3_MAX_CHANNEL) primary_ch = S3_MESH_CHANNEL;
    if (g_s3_channel != primary_ch) {
        esp_wifi_set_channel(primary_ch, WIFI_SECOND_CHAN_NONE);
        g_s3_channel = primary_ch;
    }
    esp_now_send(bcast, (const uint8_t*)&pkt, sizeof(pkt));
    esp_now_send(bcast, bc_buf, sizeof(bc_buf));

    // Dual-cast across Ch 1 (`Rakuten-4CE2` / `baye-rust`) and Ch 6 (`MEMNET` / legacy Triad)
    int alt_ch = (primary_ch == 1) ? 6 : 1;
    esp_wifi_set_channel(alt_ch, WIFI_SECOND_CHAN_NONE);
    esp_now_send(bcast, (const uint8_t*)&pkt, sizeof(pkt));
    esp_now_send(bcast, bc_buf, sizeof(bc_buf));
    esp_wifi_set_channel(primary_ch, WIFI_SECOND_CHAN_NONE);
    g_s3_channel = primary_ch;
}

// Non-blocking 1-channel-per-tick wake-up capsule (zero delay() stall on the UI loop!)
static void s3_espnow_wake_siblings(uint8_t sx, uint8_t sy, int16_t amp) {
    if (!g_s3_espnow_ok) return;
    static const uint8_t sweep_order[4] = {1, 6, 11, 3};
    static uint8_t sweep_idx = 0;
    uint8_t target_ch = sweep_order[sweep_idx & 3];
    sweep_idx++;
    esp_wifi_set_channel(target_ch, WIFI_SECOND_CHAN_NONE);
    g_s3_channel = target_ch;
    s3_espnow_broadcast(sx, sy, amp);
}

// ============================================================
// 802.11 Promiscuous Callback (ESP32-S3 Wi-Fi Driver)
// ============================================================
typedef struct __attribute__((packed)) {
    uint16_t frame_ctrl;
    uint16_t duration;
    uint8_t  addr1[6];
    uint8_t  addr2[6];
    uint8_t  addr3[6];
    uint16_t seq_ctrl;
} S3WifiMacHdr;

static void IRAM_ATTR s3_wifi_promisc_cb(void* buf, wifi_promiscuous_pkt_type_t ptype) {
    if (!buf) return;
    const wifi_promiscuous_pkt_t* pkt = (const wifi_promiscuous_pkt_t*)buf;
    int8_t rssi = pkt->rx_ctrl.rssi;
    uint8_t ch  = pkt->rx_ctrl.channel;
    if (ch < 1 || ch > S3_MAX_CHANNEL) ch = (uint8_t)g_s3_channel;

    uint16_t len = pkt->rx_ctrl.sig_len;
    const uint8_t* payload = pkt->payload;

    uint8_t ftype = (uint8_t)ptype;
    uint8_t fsub  = 0;
    if (len >= sizeof(S3WifiMacHdr)) {
        const S3WifiMacHdr* hdr = (const S3WifiMacHdr*)payload;
        ftype = (hdr->frame_ctrl >> 2) & 0x03;
        fsub  = (hdr->frame_ctrl >> 4) & 0x0F;

        // Extract beacon / probe response SSID (IE tag 0)
        if (ftype == 0 && (fsub == 8 || fsub == 5) && len > sizeof(S3WifiMacHdr) + 14) {
            size_t off = sizeof(S3WifiMacHdr) + 12;
            while (off + 2 <= len && off < 128) {
                uint8_t tag = payload[off];
                uint8_t tlen = payload[off + 1];
                if (off + 2 + tlen > len) break;
                if (tag == 0 && tlen > 0 && tlen <= 32) {
                    bool printable = true;
                    for (uint8_t i = 0; i < tlen; i++) {
                        if (payload[off + 2 + i] < 0x20 || payload[off + 2 + i] > 0x7E) {
                            printable = false;
                            break;
                        }
                    }
                    if (printable) {
                        memcpy((void*)g_s3_last_ssid, &payload[off + 2], tlen);
                        g_s3_last_ssid[tlen] = '\0';
                        if (strcmp(g_s3_last_ssid, "GlobalWiFi_kdq") == 0) {
                            g_s3_sun_ch = ch;
                        }

                        // Record in MEDUSA passive beacon table for SCAN / WebSerial
                        int slot = -1;
                        for (int a = 0; a < (int)g_medusa_ap_count; a++) {
                            if (memcmp(g_medusa_aps[a].bssid, hdr->addr3, 6) == 0) {
                                slot = a;
                                break;
                            }
                        }
                        if (slot < 0) {
                            if (g_medusa_ap_count < S3_MAX_APS) {
                                slot = g_medusa_ap_count++;
                            } else {
                                slot = (int)(g_s3_total_pkts % S3_MAX_APS);
                            }
                        }
                        g_medusa_aps[slot].used = true;
                        memcpy(g_medusa_aps[slot].bssid, hdr->addr3, 6);
                        memcpy(g_medusa_aps[slot].ssid, &payload[off + 2], tlen);
                        g_medusa_aps[slot].ssid[tlen] = '\0';
                        for (uint8_t k = 0; k < tlen; k++) {
                            if (g_medusa_aps[slot].ssid[k] == '|' || g_medusa_aps[slot].ssid[k] == '\\') {
                                g_medusa_aps[slot].ssid[k] = '_';
                            }
                        }
                        g_medusa_aps[slot].ch   = ch;
                        g_medusa_aps[slot].rssi = rssi;
                    }
                    break;
                }
                off += 2 + tlen;
            }
        }

        // 1B. Extract Directed Probe Request SSID + Vendor OUI + PHY Caps + MAC-Independent IE Genome Signature via #![no_std] Rust (fsub == 4, offset 24)
        if (ftype == 0 && fsub == 4 && len > sizeof(S3WifiMacHdr) + 2) {
            uint8_t p_ssid[33];
            uint32_t v_oui = 0;
            uint8_t p_caps = 0;
            size_t slen = rust_parse_80211_probe_ie(payload, len, 4, p_ssid, &v_oui, &p_caps);
            uint32_t sig_h = 0;
            uint16_t uniq_s = 0;
            uint32_t tot_rot = 0;
            uint16_t sig_rot = 0;
            uint8_t fp_caps = rust_fingerprint_80211_probe(payload, len, rssi, &sig_h, &uniq_s, &tot_rot, &sig_rot);
            if (sig_h != 0) {
                g_s3_last_sig_hash = sig_h;
                g_s3_unique_sigs = uniq_s;
                g_s3_mac_rotations = tot_rot;
                g_s3_last_sig_rotations = sig_rot;
            }
            if (v_oui != 0) g_s3_last_probed_oui = v_oui;
            if ((p_caps | fp_caps) != 0) g_s3_last_probed_caps = (p_caps | fp_caps);
            memcpy((void*)g_s3_last_probed_mac, hdr->addr2, 6);
            if (slen > 0) {
                g_s3_directed_probes++;
                if (strcasecmp((const char*)p_ssid, "none") != 0 || strcmp(g_s3_last_probed_ssid, "NONE") == 0) {
                    memcpy((void*)g_s3_last_probed_ssid, p_ssid, slen + 1);
                }
                s3_record_probed_target((const char*)p_ssid);
            }
        }

        // Feed ternary_seam ring buffer
        TernarySeamSample sample;
        sample.channel = ch;
        sample.rssi    = rssi;
        sample.type    = ftype;
        sample.subtype = fsub;
        memcpy(sample.src_mac, hdr->addr2, 6);
        sample.ts      = millis();
        ternary_seam_ingest(&g_s3_seam_buf, &sample);
    }

    g_s3_total_pkts++;
    g_s3_last_rssi = rssi;
    g_s3_last_type = ftype;
    g_s3_last_sub  = fsub;

    if (ftype == 0) {
        g_s3_mgmt_pkts++;
        if (fsub == 8)  g_s3_beacon_pkts++;
        if (fsub == 4)  g_s3_probe_pkts++;
        if (fsub == 12) g_s3_deauth_pkts++;
    } else if (ftype == 1) {
        g_s3_ctrl_pkts++;
    } else {
        g_s3_data_pkts++;
    }

    int idx = ch - 1;
    g_s3_ch_pkts[idx]++;
    if (g_s3_ch_rssi[idx] < -99 || rssi > g_s3_ch_rssi[idx]) {
        g_s3_ch_rssi[idx] = rssi;
    } else {
        g_s3_ch_rssi[idx] = (int8_t)((g_s3_ch_rssi[idx] * 3 + rssi) / 4);
    }
}

// ============================================================
// Ternary NAND Ring, MarineSense Wave Pool & Impermanence Step
// ============================================================
static inline int8_t s3_ternary_nand(int8_t a, int8_t b) {
    int8_t mn = (a < b) ? a : b;
    return (int8_t)(-mn);
}

static void s3_step_physics() {
    unsigned long now = millis();

    // Update self node entry + apply Impermanence (Anicca) decay to silent peers
    for (int i = 0; i < 3; i++) {
        if (i == g_triad_self_id) {
            g_triad_nodes[i].online    = true;
            g_triad_nodes[i].ttl_q8    = 255;
            g_triad_nodes[i].qt_byte   = g_imu.qt_byte;
            g_triad_nodes[i].ax_i8     = (int8_t)(g_imu.ax * 64.0f);
            g_triad_nodes[i].ay_i8     = (int8_t)(g_imu.ay * 64.0f);
            g_triad_nodes[i].nand_trit = g_s3_nand_ring[0];
            g_triad_nodes[i].last_ms   = now;
        } else if (g_triad_nodes[i].ttl_q8 > 0) {
            // Decay peer presence if no ESP-NOW capsule in >1.5s (full polyp at ~4.5s)
            if (now - g_triad_nodes[i].last_ms > 1500) {
                if (g_triad_nodes[i].ttl_q8 > 4) {
                    g_triad_nodes[i].ttl_q8 -= 4;
                } else {
                    g_triad_nodes[i].ttl_q8 = 0;
                    g_triad_nodes[i].online = false;
                }
            }
        }
    }

    // 1. Step 7-stage cross-coupled ternary NAND ring oscillator in #![no_std] Rust coupled to live Triad peers
    int8_t peer_sum = 0;
    for (int i = 0; i < 3; i++) {
        if (g_triad_nodes[i].ttl_q8 > 0) peer_sum += g_triad_nodes[i].nand_trit;
    }
    int8_t imu_mod  = g_imu.trit_v;
    int8_t rf_mod   = ternary_seam_rssi_to_code(g_s3_last_rssi);
    int8_t combined = (imu_mod + rf_mod + peer_sum > 0) ? 1 : ((imu_mod + rf_mod + peer_sum < 0) ? -1 : 0);
    rust_step_watch_wave_pool_64(NULL, NULL, 248, g_s3_nand_ring, combined);
    g_s3_nand_cycles += 7;

    // 2. Step AxiomQuant Sine Kernel K(u) = sin(pi*u)/(pi*u)
    g_s3_phase += 0.14f;
    g_s3_orbit_angle += 0.045f + g_imu.sickness * 0.08f;
    float rssi_u = normalize_rssi(g_s3_last_rssi) * 2.4f;
    float k_val  = sine_kernel(rssi_u + 0.35f * sinf(g_s3_phase));
    float carrier = sinf(g_s3_phase * 2.2f + (float)g_s3_last_type * 0.8f) * k_val;
    g_s3_wave_hist[g_s3_wave_head] = carrier;
    g_s3_wave_head = (g_s3_wave_head + 1) % S3_WAVE_HIST;

    // 3. Step 64x64 2D Wave Pool with active Triad nodes + MarineSense O(1) Salience
    if ((g_s3_witness_seq % 3) == 0) {
        int n_idx = (int)(g_s3_witness_seq / 3) % 3;
        if (g_triad_nodes[n_idx].ttl_q8 > 0) {
            float base_ang = g_s3_orbit_angle + (float)n_idx * 2.0943951f;
            float rad = 14.0f + 5.0f * sinf(g_s3_phase * 0.5f + (float)n_idx);
            float tilt_x = (float)g_triad_nodes[n_idx].ax_i8 / 64.0f;
            float tilt_y = (float)g_triad_nodes[n_idx].ay_i8 / 64.0f;
            int wx = 32 + (int)(cosf(base_ang) * rad + tilt_x * 9.0f);
            int wy = 32 + (int)(sinf(base_ang) * rad + tilt_y * 9.0f);
            int16_t pole_sign = (n_idx == 0) ? -1 : 1;
            int32_t raw_amp = (int32_t)((2600.0f * k_val + 1400.0f) * (float)pole_sign);
            int16_t amp = (int16_t)((raw_amp * (int32_t)g_triad_nodes[n_idx].ttl_q8) >> 8);
            s3_wave_pool_splash(wx, wy, amp, 4);
        }
    }
    if (g_imu.sickness > 0.25f) {
        int sx = 32 + (int)(g_imu.gx * 0.08f) % 20;
        int sy = 32 + (int)(g_imu.gy * 0.08f) % 20;
        s3_wave_pool_splash(sx, sy, (int16_t)(g_imu.sickness * 6200.0f), 6);
    }
    s3_wave_pool_step(g_s3_phase * 0.2f);

    // 4. Step 8-band Ternary EQ for active zoneInn Favourite track
    uint32_t h = S3_FAVOURITES[g_s3_track_idx].fnv_hash;
    float bpm_freq = (float)S3_FAVOURITES[g_s3_track_idx].bpm / 120.0f;
    for (int b = 0; b < 8; b++) {
        float osc = sinf(g_s3_phase * bpm_freq * (1.0f + b * 0.27f) + (float)((h >> (b * 3)) & 0x7));
        osc += 0.25f * ((b < 4) ? g_imu.ax : g_imu.ay);
        int bar = g_s3_playing ? (int)(52.0f + 42.0f * osc) : 18;
        if (bar < 10) bar = 10;
        if (bar > 98) bar = 98;
        g_s3_eq_bars[b]  = (uint8_t)bar;
        g_s3_eq_trits[b] = (bar > 68) ? +1 : ((bar < 38) ? -1 : 0);
    }

    // 5. Compute 2D Lissajous curve modulated by QMI8658 IMU tilt
    float freq_x = 2.0f + g_imu.ax * 0.8f;
    float freq_y = 3.0f + g_imu.ay * 0.8f;
    float delta  = g_s3_phase * 0.6f + g_imu.az * 1.2f;
    float lissa_rx = (float)(g_lcd_w - 110) * 0.45f;
    for (int i = 0; i < S3_LISSA_PTS; i++) {
        float t = (float)i * (2.0f * 3.14159265f / (float)S3_LISSA_PTS);
        g_s3_lissa_x[i] = (int16_t)(sinf(freq_x * t + delta) * lissa_rx);
        g_s3_lissa_y[i] = (int16_t)(sinf(freq_y * t) * 48.0f);
    }

    // 6. Compute wigle-triad N=NP Shannon entropy, Kuramoto coherence, and 9-trit vector
    uint32_t pkt_sum = 0;
    long rssi_sum    = 0;
    int active_ch    = 0;
    float phase_sin  = 0.0f;
    float phase_cos  = 0.0f;
    for (int c = 0; c < S3_MAX_CHANNEL; c++) {
        pkt_sum += g_s3_ch_pkts[c];
        if (g_s3_ch_pkts[c] > 0) {
            rssi_sum += g_s3_ch_rssi[c];
            active_ch++;
            float theta = (float)((c + 1) * 2.0f * 3.14159265f / 14.0f) + (float)(g_s3_ch_rssi[c] + 100) * 0.05f;
            phase_sin += sinf(theta);
            phase_cos += cosf(theta);
        }
    }
    if (pkt_sum > 0 && active_ch > 0) {
        float ent = 0.0f;
        for (int c = 0; c < S3_MAX_CHANNEL; c++) {
            if (g_s3_ch_pkts[c] > 0) {
                float p = (float)g_s3_ch_pkts[c] / (float)pkt_sum;
                ent -= p * (logf(p) / 0.69314718f);
            }
        }
        g_medusa_entropy   = ent;
        g_medusa_mean_rssi = (int)(rssi_sum / active_ch);
        g_medusa_coherence = sqrtf(phase_sin * phase_sin + phase_cos * phase_cos) / (float)active_ch;
    } else {
        g_medusa_entropy   = 0.42f;
        g_medusa_mean_rssi = (int)g_s3_last_rssi;
        g_medusa_coherence = 0.78f;
    }
    for (int t = 0; t < 9; t++) {
        uint32_t c1 = g_s3_ch_pkts[t % S3_MAX_CHANNEL];
        uint32_t c2 = g_s3_ch_pkts[(t + 4) % S3_MAX_CHANNEL];
        g_medusa_trits[t] = (c1 > c2) ? '+' : ((c1 < c2) ? '-' : '0');
    }
    g_medusa_trits[9] = '\0';

    // 7. Harvest & Kompress-Super-Ultra All Entropy (<<<<oOo>>>> ~ Kunisada & Kuniyoshi 010001000)
    {
        uint64_t pos_plane = 0ULL;
        uint64_t neg_plane = 0ULL;
        uint32_t hw_rng    = esp_random();
        uint32_t imu_seed  = ((uint32_t)((int32_t)(g_imu.ax * 1024.0f) & 0xFFFF) << 16) ^
                             ((uint32_t)((int32_t)(g_imu.ay * 1024.0f) & 0xFFFF)) ^
                             ((uint32_t)g_power.batt_mv << 8) ^ (uint32_t)g_ms_mean_jitter;
        int8_t trits65[65];
        for (int i = 0; i < 65; i++) {
            uint32_t mix = hw_rng ^ (imu_seed + (uint32_t)i * 0x9E3779B9UL) ^
                           ((uint32_t)g_s3_ch_pkts[i % S3_MAX_CHANNEL] << (i & 7)) ^
                           ((uint32_t)g_wp_u_curr[(i * 7) & 63][(i * 13) & 63] & 0xFFFF) ^
                           0x88UL; // 010001000 Kunisada & Kuniyoshi pulse
            uint8_t m3 = (uint8_t)((mix ^ (mix >> 11)) % 3);
            int8_t tr  = (m3 == 1) ? 1 : ((m3 == 2) ? -1 : 0);
            trits65[i] = tr;
            if (i < 64) {
                if (tr > 0) pos_plane |= (1ULL << i);
                else if (tr < 0) neg_plane |= (1ULL << i);
            }
        }
        g_kompress_pos_u64 = pos_plane;
        g_kompress_neg_u64 = neg_plane;
        uint64_t w0 = (pos_plane ^ 0x9E3779B97F4A7C15ULL);
        uint64_t w1 = (neg_plane ^ (2ULL * 0x9E3779B97F4A7C15ULL));
        uint64_t r0 = (w0 << 13) | (w0 >> 51);
        uint64_t r1 = (w1 << 13) | (w1 >> 51);
        uint64_t acc = 0xCBF29CE484222325ULL;
        acc = (acc ^ r0) * 0x100000001B3ULL;
        acc = (acc ^ r1) * 0x100000001B3ULL;
        g_kompress_wupz_u64 = acc;

        static const uint8_t pow3[5] = {1, 3, 9, 27, 81};
        static const char hex_lut[17] = "0123456789abcdef";
        for (int b = 0; b < 13; b++) {
            uint8_t acc_b = 0;
            for (int j = 0; j < 5; j++) {
                int8_t t = trits65[b * 5 + j];
                uint8_t u = (t < 0) ? 2 : ((t > 0) ? 1 : 0);
                acc_b += u * pow3[j];
            }
            g_kompress_b158[b] = acc_b;
            g_kompress_hex[b * 2]     = hex_lut[(acc_b >> 4) & 0x0F];
            g_kompress_hex[b * 2 + 1] = hex_lut[acc_b & 0x0F];
        }
        g_kompress_hex[26] = '\0';
    }

    // 8. Render TernarySeam radiation matrix
    ternary_seam_render(&g_s3_seam_buf, &g_s3_seam_mat);
    g_s3_witness_seq++;
}

// ============================================================
// Triple-Boot Partition Helper (FLUG-OS / XiaoZhi / Factory)
// ============================================================
static bool s3_boot_partition(const char* label) {
    const esp_partition_t* part = NULL;
    if (strcmp(label, "factory") == 0) {
        part = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_FACTORY, NULL);
    } else if (strcmp(label, "xiaozhi") == 0 || strcmp(label, "ota_0") == 0) {
        part = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_0, NULL);
    } else if (strcmp(label, "flugos") == 0 || strcmp(label, "ota_1") == 0) {
        part = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_1, NULL);
    }
    if (!part) return false;
    if (esp_ota_set_boot_partition(part) == ESP_OK) {
        Serial.printf("{\"event\":\"reboot_partition\",\"target\":\"%s\",\"offset\":%lu}\n",
                      label, (unsigned long)part->address);
        delay(200);
        esp_restart();
        return true;
    }
    return false;
}

// ============================================================
// UI Band Renderer — Responsive to 368x448 (1.8") & 410x502 (2.06")
// ============================================================
static uint16_t s3_trit_color(int8_t t) {
    if (t > 0) return COL_MINT;
    if (t < 0) return COL_CORAL;
    return COL_GOLD;
}

static char s3_trit_char(int8_t t) {
    if (t > 0) return '+';
    if (t < 0) return '-';
    return '.';
}

static void s3_render_header(S3Canvas& c) {
    int W = (int)g_lcd_w;
    c.fill_rect(0, 0, W, 40, COL_BG_ELEV);
    c.draw_hline(0, 40, W, COL_BORDER);

    uint16_t pcol = s3_trit_color(g_triad_self_pole);
    c.fill_circle(14, 20, 6, pcol);
    c.draw_circle(14, 20, 9, COL_CYAN);

    const char* hdr_title = (g_triad_self_id == 0) ? "[-1] ADAYE // ANICCA"
                          : ((g_triad_self_id == 1) ? "[ 0] VAKED // ANICCA"
                                                    : "[+1] OMNI  // ANICCA");
    c.draw_text(28, 7, hdr_title, COL_TEXT, 2);

    char sub_hdr[44];
    snprintf(sub_hdr, sizeof(sub_hdr), "ms.8b.is S:%u%% ReM:%u [%u/3]",
             g_ms_mean_salience, g_ms_reynolds, s3_triad_online_count());
    c.draw_text(28, 25, sub_hdr, pcol, 1);

    int qt_x = W - 150;
    c.fill_card(qt_x, 6, 70, 28, COL_VIOLET_DARK, COL_VIOLET);
    char qt_buf[16];
    snprintf(qt_buf, sizeof(qt_buf), "qt:%02x %c%c%c",
             g_imu.qt_byte,
             s3_trit_char(g_imu.trit_v),
             s3_trit_char(g_imu.trit_a),
             s3_trit_char(g_imu.trit_d));
    c.draw_text_centered(qt_x + 35, 16, qt_buf, COL_VIOLET, 1);

    int bat_x = W - 75;
    uint16_t bcol = g_power.charging ? COL_MINT : COL_GOLD;
    c.fill_card(bat_x, 6, 69, 28, COL_BG_PANEL, bcol);
    char b_buf[16];
    snprintf(b_buf, sizeof(b_buf), "%u%% %s",
             g_power.batt_pct, g_power.vbus_good ? "USB" : "BAT");
    c.draw_text_centered(bat_x + 34, 11, b_buf, bcol, 1);
    snprintf(b_buf, sizeof(b_buf), "%umV", g_power.batt_mv);
    c.draw_text_centered(bat_x + 34, 22, b_buf, COL_SUBTEXT, 1);
}

static void s3_render_footer_tabs(S3Canvas& c) {
    int W = (int)g_lcd_w;
    int y = (int)g_lcd_h - 52;
    c.fill_rect(0, y, W, 52, COL_BG_ELEV);
    c.draw_hline(0, y, W, COL_BORDER);

    const char* labels[S3_TAB_COUNT] = {"ZEN", "RADIO", "802.11", "NAND", "QWAVE", "VIZ"};
    const char* subs[S3_TAB_COUNT]   = {"anicca", "zoneInn", "rf+sinc", "imu+osc", "triad", "enthea"};
    uint16_t cols[S3_TAB_COUNT]      = {COL_MINT, COL_GOLD, COL_CYAN, COL_CORAL, COL_VIOLET, COL_GOLD};

    int gap = 4;
    int bw  = (W - gap * (S3_TAB_COUNT + 1)) / S3_TAB_COUNT;

    for (int i = 0; i < S3_TAB_COUNT; i++) {
        int bx = gap + i * (bw + gap);
        bool active = (g_s3_active_tab == i);
        uint16_t fill = active ? COL_BG_PANEL : COL_BG;
        uint16_t brd  = active ? cols[i] : COL_BORDER;
        c.fill_card(bx, y + 5, bw, 42, fill, brd);
        if (active) {
            c.fill_rect(bx + 10, y + 5, bw - 20, 3, cols[i]);
        }
        c.draw_text_centered(bx + bw / 2, y + 14, labels[i], active ? cols[i] : COL_TEXT, 1);
        c.draw_text_centered(bx + bw / 2, y + 29, subs[i], active ? COL_TEXT : COL_DIM, 1);
    }
}

// ============================================================
// TAB 0: IMPERMANENCE (ANICCA) x MARINESENSE JELLY NETWORK
// Jelly Sphere Mind-Wall + 7 O(1) Jellyfish Detectors + Ecphoric Recall
// ============================================================
static void s3_render_tab_zen(S3Canvas& c) {
    int W  = (int)g_lcd_w;
    int H  = (int)g_lcd_h;
    int cx = W / 2;
    int cy = 40 + (H - 96) / 2 - 6;
    int outer_r = (W < 380) ? 138 : 156;
    int inner_r = outer_r - 14;
    int outer_r2 = outer_r * outer_r;
    int inner_r2 = inner_r * inner_r;

    int y_start = (c.y0 > 42) ? c.y0 : 42;
    int y_end   = (c.y1 < H - 54) ? c.y1 : (H - 54);

    float sick = g_imu.sickness;
    uint8_t sick_u8 = (uint8_t)(sick * 200.0f);
    const int ring_span = outer_r2 - inner_r2;
    const int ring_lo   = ring_span / 5;
    const int ring_hi   = (ring_span * 4) / 5;
    const int32_t inv_2inner_q16  = ((WAVE_POOL_N - 2) << 16) / (2 * inner_r);
    const int32_t inv_innerr2_q16 = (48 << 16) / inner_r2;

    for (int py = y_start; py < y_end; py++) {
        int dy  = py - cy;
        int dy2 = dy * dy;
        if (dy2 > outer_r2) continue;

        int gy = (((dy + inner_r) * inv_2inner_q16) >> 16) + 1;
        if (gy < 1) gy = 1;
        if (gy > WAVE_POOL_N - 2) gy = WAVE_POOL_N - 2;
        const int16_t* wp_row = g_wp_u_curr[gy];
        uint16_t* row_ptr = &s_band_buf[(py - c.y0) * W];

        for (int px = 8; px < W - 8; px++) {
            int dx = px - cx;
            int r2 = dx * dx + dy2;
            if (r2 > outer_r2) continue;

            if (r2 >= inner_r2) {
                int ring_band = r2 - inner_r2;
                if (ring_band < ring_lo || ring_band > ring_hi) {
                    row_ptr[px] = (sick_u8 > 60) ? COL_CORAL : COL_AMBER_INFO;
                } else {
                    row_ptr[px] = COL_BG_ELEV;
                }
            } else {
                int gx = (((dx + inner_r) * inv_2inner_q16) >> 16) + 1;
                if (gx < 1) gx = 1;
                if (gx > WAVE_POOL_N - 2) gx = WAVE_POOL_N - 2;

                int16_t h = wp_row[gx];
                int rim = (int)(((int32_t)r2 * inv_innerr2_q16) >> 16);

                uint8_t r = 6 + (uint8_t)((rim * 18) >> 6);
                uint8_t g = 10 + (uint8_t)((rim * 36) >> 6);
                uint8_t b = 22 + (uint8_t)((rim * 54) >> 6);

                if (h > 64) {
                    uint32_t mag = (uint32_t)h >> 5;
                    if (mag > 220) mag = 220;
                    r = (uint8_t)min((uint32_t)255U, (uint32_t)r + (mag >> 2) + (uint32_t)(sick_u8 >> 1));
                    g = (uint8_t)min((uint32_t)255U, (uint32_t)g + mag);
                    b = (uint8_t)min((uint32_t)255U, (uint32_t)b + ((mag * 3) >> 2));
                } else if (h < -64) {
                    uint32_t mag = (uint32_t)(-h) >> 5;
                    if (mag > 220) mag = 220;
                    r = (uint8_t)min((uint32_t)255U, (uint32_t)r + ((mag * 3) >> 2) + (uint32_t)(sick_u8 >> 1));
                    g = (uint8_t)min((uint32_t)255U, (uint32_t)g + (mag >> 2));
                    b = (uint8_t)min((uint32_t)255U, (uint32_t)b + mag);
                } else if (sick_u8 > 25) {
                    r = (uint8_t)min((uint32_t)255U, (uint32_t)r + (uint32_t)(sick_u8 >> 1));
                }

                if (((px - cx) & 31) == 0 || ((py - cy) & 31) == 0) {
                    g = (uint8_t)min(255, (int)g + 10);
                    b = (uint8_t)min(255, (int)b + 16);
                }

                row_ptr[px] = rgb565_be(r, g, b);
            }
        }
    }

    // 2. Render the 7 MarineSense Bioluminescent Jellyfish Detector Nodes (Alpha..Eta)
    for (int j = 0; j < JELLY_COUNT; j++) {
        const S3JellyNode* nd = &g_jelly[j];
        int jx = cx + (((int)nd->gx - 32) * inner_r) / 34;
        int jy = cy + (((int)nd->gy - 32) * inner_r) / 34;
        uint16_t jcol = (nd->salience_pct > 55) ? COL_MINT : ((nd->salience_pct > 30) ? COL_CYAN : COL_DIM);
        c.fill_circle(jx, jy, 3, jcol);
        if (nd->excitation > 25) {
            c.draw_circle(jx, jy, 5 + (nd->excitation >> 4), COL_GOLD);
        }
        if (nd->recall_ring_r > 0) {
            c.draw_circle(jx, jy, nd->recall_ring_r, COL_CORAL);
        }
    }

    // 3. Orbiting 7-stage Ternary NAND Nodes on the Amber Info Ring
    int mid_r = (outer_r + inner_r) / 2;
    for (int i = 0; i < 7; i++) {
        float ang = g_s3_orbit_angle + (float)i * (2.0f * 3.14159265f / 7.0f);
        int nx = cx + (int)(cosf(ang) * (float)mid_r);
        int ny = cy + (int)(sinf(ang) * (float)mid_r);
        uint16_t ncol = s3_trit_color(g_s3_nand_ring[i]);
        c.fill_circle(nx, ny, 4, ncol);
        c.draw_circle(nx, ny, 6, COL_TEXT);
    }

    // 4. 3-Body Triad Entanglement Triangle {<3, <3, <3} with Turritopsis Polyp Transdifferentiation
    int tx[3], ty[3];
    uint16_t tcol[3] = {COL_CORAL, COL_GOLD, COL_CYAN};
    for (int n = 0; n < 3; n++) {
        float base_ang = g_s3_orbit_angle * 0.6f + (float)n * 2.0943951f - 1.570796f;
        float rad = (float)inner_r * 0.46f;
        int dx_imu = ((int)g_triad_nodes[n].ax_i8 * 24) / 64;
        int dy_imu = ((int)g_triad_nodes[n].ay_i8 * 24) / 64;
        tx[n] = cx + (int)(cosf(base_ang) * rad) + dx_imu;
        ty[n] = cy + (int)(sinf(base_ang) * rad) + dy_imu;
    }

    // Draw edges weighted by peer TTL (dashed/dim if peer is in Turritopsis Polyp state)
    c.draw_line(tx[0], ty[0], tx[1], ty[1], (g_triad_nodes[0].ttl_q8 > 0 && g_triad_nodes[1].ttl_q8 > 0) ? COL_MINT : COL_GRID, 1);
    c.draw_line(tx[1], ty[1], tx[2], ty[2], (g_triad_nodes[1].ttl_q8 > 0 && g_triad_nodes[2].ttl_q8 > 0) ? COL_GOLD : COL_GRID, 1);
    c.draw_line(tx[2], ty[2], tx[0], ty[0], (g_triad_nodes[2].ttl_q8 > 0 && g_triad_nodes[0].ttl_q8 > 0) ? COL_CYAN : COL_GRID, 1);

    for (int n = 0; n < 3; n++) {
        bool is_self = (n == g_triad_self_id);
        uint8_t ttl  = g_triad_nodes[n].ttl_q8;
        if (ttl > 0) {
            c.fill_circle(tx[n], ty[n], is_self ? 6 : 4, tcol[n]);
            c.draw_circle(tx[n], ty[n], is_self ? 11 : (4 + (ttl >> 5)), is_self ? COL_MINT : COL_TEXT);
        } else {
            // Turritopsis dohrnii Polyp Seed (dormant, awaiting rebirth)
            c.draw_circle(tx[n], ty[n], 4, COL_DIM);
            c.fill_circle(tx[n], ty[n], 1, COL_GOLD);
        }
    }

    // Draw Infinity Lemniscate curve at centroid (only when current band overlaps centroid Y +- 28)
    int cent_x = (tx[0] + tx[1] + tx[2]) / 3;
    int cent_y = (ty[0] + ty[1] + ty[2]) / 3;
    if (c.y1 >= cent_y - 28 && c.y0 <= cent_y + 28) {
        float inf_a = 26.0f;
        for (int i = 0; i < 16; i++) {
            float t0 = (float)i * (2.0f * 3.14159265f / 16.0f) + g_s3_phase * 0.3f;
            float t1 = (float)(i + 1) * (2.0f * 3.14159265f / 16.0f) + g_s3_phase * 0.3f;
            float d0 = 1.0f + sinf(t0) * sinf(t0);
            float d1 = 1.0f + sinf(t1) * sinf(t1);
            int ix0 = cent_x + (int)(inf_a * cosf(t0) / d0);
            int iy0 = cent_y + (int)(inf_a * sinf(t0) * cosf(t0) / d0);
            int ix1 = cent_x + (int)(inf_a * cosf(t1) / d1);
            int iy1 = cent_y + (int)(inf_a * sinf(t1) * cosf(t1) / d1);
            c.draw_line(ix0, iy0, ix1, iy1, COL_VIOLET, 2);
        }
        c.fill_circle(cent_x, cent_y, 3, COL_MINT);
    }

    // 5. Top & Bottom Impermanence + MarineSense HUD Pills (band-culled so formatted once per frame!)
    if (c.y1 >= 44 && c.y0 <= 54) {
        const S3FavTrack* trk = &S3_FAVOURITES[g_s3_track_idx];
        int32_t visP = 0, visQ = 1;
        rust_reduce_rational_pq((int32_t)(g_ms_viscosity * 100.0f), 100, &visP, &visQ);
        char top_lbl[56];
        snprintf(top_lbl, sizeof(top_lbl), "%s // eta:%ld/%ld rec:%lu",
                 trk->title, (long)visP, (long)visQ, (unsigned long)g_ms_ecphoric_recalls);
        top_lbl[38] = '\0';
        c.draw_text_centered(cx, 45, top_lbl, COL_MINT, 1);
    }

    int hud_y = H - 92;
    if (c.y1 >= hud_y && c.y0 <= hud_y + 36) {
        c.fill_card(8, hud_y, W - 16, 36, COL_BG_ELEV, COL_BORDER);
        char hud1[64];
        snprintf(hud1, sizeof(hud1), "ADA:%s  <3  VAK:%s  <3  OMN:%s",
                 g_triad_nodes[0].ttl_q8 ? "MEDUSA" : "POLYP",
                 g_triad_nodes[1].ttl_q8 ? "MEDUSA" : "POLYP",
                 g_triad_nodes[2].ttl_q8 ? "MEDUSA" : "POLYP");
        c.draw_text_centered(cx, hud_y + 7, hud1, COL_MINT, 1);

        char hud2[64];
        snprintf(hud2, sizeof(hud2), "SIG:%08lX U:%u ROT:%lu D:%lu",
                 (unsigned long)g_s3_last_sig_hash,
                 (unsigned)g_s3_unique_sigs,
                 (unsigned long)g_s3_mac_rotations,
                 (unsigned long)g_s3_directed_probes);
        c.draw_text_centered(cx, hud_y + 21, hud2, COL_GOLD, 1);
    }
}

// ============================================================
// TAB 1: ZONEINN / RADIO.VAKED.DEV — 14 Favourite Musics + Ternary EQ
// ============================================================
static void s3_render_tab_radio(S3Canvas& c) {
    int W = (int)g_lcd_w;
    int H = (int)g_lcd_h;
    int cw = W - 16;
    const S3FavTrack* trk = &S3_FAVOURITES[g_s3_track_idx];

    c.fill_card(8, 46, cw, 104, COL_BG_ELEV, COL_MINT);
    c.draw_text(18, 53, "RADIO.VAKED.DEV // ZONEINN CRATE", COL_MINT, 1);

    char idx_buf[24];
    snprintf(idx_buf, sizeof(idx_buf), "FAV %02d/%02d", g_s3_track_idx + 1, S3_FAV_COUNT);
    c.draw_text_right(W - 18, 53, idx_buf, COL_GOLD, 1);

    c.draw_text(18, 67, trk->title, COL_TEXT, 2);
    c.draw_text(18, 86, trk->artist, COL_SUBTEXT, 1);

    char meta_buf[64];
    snprintf(meta_buf, sizeof(meta_buf), "%s | KEY %s | %uBPM | #%08lX",
             trk->genre, trk->key_sig, trk->bpm, (unsigned long)trk->fnv_hash);
    c.draw_text(18, 101, meta_buf, COL_GOLD, 1);

    int bw1 = (cw - 32) / 4;
    int bw2 = (cw - 32) - 2 * bw1;
    int bx0 = 16;
    int bx1 = bx0 + bw1 + 6;
    int bx2 = bx1 + bw2 + 6;

    c.fill_card(bx0, 118, bw1, 24, COL_BG_PANEL, COL_BORDER);
    c.draw_text_centered(bx0 + bw1 / 2, 126, "< PREV", COL_TEXT, 1);

    c.fill_card(bx1, 118, bw2, 24, g_s3_playing ? COL_MINT_DARK : COL_GOLD_DARK,
                g_s3_playing ? COL_MINT : COL_GOLD);
    c.draw_text_centered(bx1 + bw2 / 2, 126, g_s3_playing ? "LIVE // TRIAD+BT" : "PAUSED // TAP",
                         g_s3_playing ? COL_MINT : COL_GOLD, 1);

    c.fill_card(bx2, 118, bw1, 24, COL_BG_PANEL, COL_CYAN);
    c.draw_text_centered(bx2 + bw1 / 2, 126, "NEXT >", COL_CYAN, 1);

    c.fill_card(8, 156, cw, 102, COL_BG_ELEV, COL_BORDER);
    c.draw_text(18, 163, "8-BAND TERNARY HARMONIC EQ {-1, 0, +1}", COL_SUBTEXT, 1);
    const char* hz_lbl[8] = {"60", "144", "288", "432", "528", "699", "1.1k", "4.3k"};
    int eq_step = (cw - 24) / 8;
    int eq_bw   = eq_step - 8;
    for (int b = 0; b < 8; b++) {
        int bx = 18 + b * eq_step;
        int max_h = 52;
        int bh = (g_s3_eq_bars[b] * max_h) / 100;
        int by = 236 - bh;
        int8_t tr = g_s3_eq_trits[b];
        uint16_t col = s3_trit_color(tr);
        uint16_t dark = (tr > 0) ? COL_MINT_DARK : ((tr < 0) ? COL_CORAL_DARK : COL_GOLD_DARK);

        c.fill_rect(bx, 184, eq_bw, max_h, COL_BG);
        c.fill_rect(bx, by, eq_bw, bh, dark);
        c.draw_rect(bx, by, eq_bw, bh, col);

        char t_str[4] = {s3_trit_char(tr), '\0'};
        c.draw_text_centered(bx + eq_bw / 2, 175, t_str, col, 1);
        c.draw_text_centered(bx + eq_bw / 2, 242, hz_lbl[b], COL_DIM, 1);
    }

    int crate_y = 264;
    int crate_h = (H - 56) - crate_y;
    int rows    = (crate_h - 24) / 22;
    if (rows > 5) rows = 5;
    if (rows < 4) rows = 4;

    c.fill_card(8, crate_y, cw, crate_h, COL_BG_ELEV, COL_BORDER);
    c.draw_text(18, crate_y + 7, "FAVOURITE MUSICS CRATE (SYNCS ALL 3 BOARDS)", COL_GOLD, 1);

    int start_idx = g_s3_track_idx - (rows / 2);
    if (start_idx < 0) start_idx = 0;
    if (start_idx > S3_FAV_COUNT - rows) start_idx = S3_FAV_COUNT - rows;

    for (int r = 0; r < rows; r++) {
        int tidx = start_idx + r;
        int ry = crate_y + 22 + r * 22;
        bool sel = (tidx == g_s3_track_idx);
        if (sel) {
            c.fill_rect(14, ry - 2, cw - 12, 19, COL_BG_PANEL);
            c.fill_rect(14, ry - 2, 4, 19, COL_MINT);
        }
        char row_buf[64];
        snprintf(row_buf, sizeof(row_buf), "%02d %s - %s",
                 tidx + 1, S3_FAVOURITES[tidx].title, S3_FAVOURITES[tidx].artist);
        row_buf[36] = '\0';
        c.draw_text(22, ry + 3, row_buf, sel ? COL_MINT : COL_TEXT, 1);

        char bpm_buf[16];
        snprintf(bpm_buf, sizeof(bpm_buf), "%uBPM", S3_FAVOURITES[tidx].bpm);
        c.draw_text_right(W - 16, ry + 3, bpm_buf, sel ? COL_GOLD : COL_DIM, 1);
    }
}

// ============================================================
// TAB 2: FLUG-OS 802.11 PROMISCUOUS SPECTRUM + SINE KERNEL
// ============================================================
static void s3_render_tab_wifi(S3Canvas& c) {
    int W  = (int)g_lcd_w;
    int H  = (int)g_lcd_h;
    int cw = W - 16;

    c.fill_card(8, 46, cw, 122, COL_BG_ELEV, COL_CYAN);
    c.draw_text(18, 53, "802.11 SPECTRUM (CH 01..13)", COL_CYAN, 1);

    char ch_hdr[36];
    snprintf(ch_hdr, sizeof(ch_hdr), "CH:%02d %s PKTS:%lu",
             g_s3_channel, g_s3_hopping ? "HOP" : "FIX", (unsigned long)g_s3_total_pkts);
    c.draw_text_right(W - 16, 53, ch_hdr, COL_MINT, 1);

    int ch_step = (cw - 20) / S3_MAX_CHANNEL;
    int ch_bw   = ch_step - 4;
    for (int ch = 1; ch <= S3_MAX_CHANNEL; ch++) {
        int bx = 16 + (ch - 1) * ch_step;
        int8_t rssi = g_s3_ch_rssi[ch - 1];
        int val = (int)rssi + 100;
        if (val < 6) val = 6;
        if (val > 80) val = 80;
        int bh = (val * 72) / 80;
        int by = 144 - bh;

        int8_t code = ternary_seam_rssi_to_code(rssi);
        uint16_t col = s3_trit_color(code);
        bool is_cur = (ch == g_s3_channel);

        c.fill_rect(bx, 72, ch_bw, 72, COL_BG);
        c.fill_rect(bx, by, ch_bw, bh, (code > 0) ? COL_MINT_DARK : ((code < 0) ? COL_CORAL_DARK : COL_GOLD_DARK));
        c.draw_rect(bx, by, ch_bw, bh, is_cur ? COL_CYAN : col);

        char clbl[6];
        snprintf(clbl, sizeof(clbl), "%02d", ch);
        c.draw_text_centered(bx + ch_bw / 2, 150, clbl, is_cur ? COL_CYAN : COL_SUBTEXT, 1);
    }

    c.fill_card(8, 174, cw, 96, COL_BG_ELEV, COL_BORDER);
    c.draw_text(18, 181, "SINE KERNEL K(u)=sin(pi*u)/(pi*u)", COL_GOLD, 1);

    char rssi_lbl[32];
    snprintf(rssi_lbl, sizeof(rssi_lbl), "%ddBm [%c1]",
             (int)g_s3_last_rssi, s3_trit_char(ternary_seam_rssi_to_code(g_s3_last_rssi)));
    c.draw_text_right(W - 16, 181, rssi_lbl, COL_MINT, 1);

    int wave_cy = 228;
    int wave_w  = cw - 24;
    c.draw_hline(18, wave_cy, wave_w, COL_GRID);
    for (int i = 0; i < S3_WAVE_HIST - 1; i++) {
        int idx0 = (g_s3_wave_head + i) % S3_WAVE_HIST;
        int idx1 = (g_s3_wave_head + i + 1) % S3_WAVE_HIST;
        int x0 = 18 + (i * wave_w) / (S3_WAVE_HIST - 1);
        int x1 = 18 + ((i + 1) * wave_w) / (S3_WAVE_HIST - 1);
        int y0 = wave_cy - (int)(g_s3_wave_hist[idx0] * 32.0f);
        int y1 = wave_cy - (int)(g_s3_wave_hist[idx1] * 32.0f);
        c.draw_line(x0, y0, x1, y1, COL_MINT, 2);
    }

    int mat_y = 276;
    int mat_h = (H - 56) - mat_y;
    c.fill_card(8, mat_y, cw, mat_h, COL_BG_ELEV, COL_BORDER);
    c.draw_text(18, mat_y + 7, "TERNARY SEAM RADIATION MATRIX (64x3)", COL_VIOLET, 1);

    char ssid_line[48];
    snprintf(ssid_line, sizeof(ssid_line), "SSID: %s", g_s3_last_ssid);
    ssid_line[34] = '\0';
    c.draw_text(18, mat_y + 21, ssid_line, COL_TEXT, 1);

    char cnt_line[64];
    snprintf(cnt_line, sizeof(cnt_line), "M:%lu C:%lu D:%lu BCN:%lu PRB:%lu",
             (unsigned long)g_s3_mgmt_pkts, (unsigned long)g_s3_ctrl_pkts,
             (unsigned long)g_s3_data_pkts, (unsigned long)g_s3_beacon_pkts,
             (unsigned long)g_s3_probe_pkts);
    c.draw_text(18, mat_y + 35, cnt_line, COL_SUBTEXT, 1);

    int mcols = (cw - 24) / 11;
    if (mcols > 32) mcols = 32;
    for (int col = 0; col < mcols; col++) {
        int r_idx = (g_s3_seam_mat.rows > col) ? (g_s3_seam_mat.rows - 1 - col) : col;
        for (int row = 0; row < 3; row++) {
            int8_t v = (r_idx < (int)g_s3_seam_mat.rows) ? g_s3_seam_mat.codes[r_idx][row] : 0;
            uint16_t cell_col = (v > 0) ? COL_MINT : ((v < 0) ? COL_CORAL : COL_GRID);
            c.fill_rect(18 + col * 11, mat_y + 49 + row * 9, 9, 7, cell_col);
        }
    }

    int btn_y = mat_y + mat_h - 32;
    int bw    = (cw - 30) / 4;
    c.fill_card(16, btn_y, bw, 24, COL_BG_PANEL, COL_BORDER);
    c.draw_text_centered(16 + bw / 2, btn_y + 8, "CH -", COL_TEXT, 1);

    c.fill_card(22 + bw, btn_y, bw + 12, 24, g_s3_hopping ? COL_MINT_DARK : COL_BG_PANEL, COL_MINT);
    c.draw_text_centered(22 + bw + (bw + 12) / 2, btn_y + 8, g_s3_hopping ? "HOP:ON" : "HOP:OFF", COL_MINT, 1);

    c.fill_card(40 + 2 * bw, btn_y, bw - 6, 24, COL_BG_PANEL, COL_BORDER);
    c.draw_text_centered(40 + 2 * bw + (bw - 6) / 2, btn_y + 8, "CH +", COL_TEXT, 1);

    c.fill_card(40 + 3 * bw, btn_y, bw, 24, COL_VIOLET_DARK, COL_VIOLET);
    c.draw_text_centered(40 + 3 * bw + bw / 2, btn_y + 8, "TRIAD", COL_VIOLET, 1);
}

// ============================================================
// TAB 3: ULTRAWHALE TERNARY NAND OSCILLATOR x QMI8658 IMU
// ============================================================
static void s3_render_tab_nand(S3Canvas& c) {
    int W  = (int)g_lcd_w;
    int H  = (int)g_lcd_h;
    int cw = W - 16;

    c.fill_card(8, 46, cw, 116, COL_BG_ELEV, COL_GOLD);
    c.draw_text(18, 53, "ULTRAWHALE 7-STAGE TERNARY NAND RING", COL_GOLD, 1);

    int g_step = (cw - 20) / 7;
    int g_bw   = g_step - 6;
    for (int i = 0; i < 7; i++) {
        int gx = 16 + i * g_step;
        int gy = 70;
        int8_t st = g_s3_nand_ring[i];
        uint16_t col = s3_trit_color(st);
        uint16_t fill = (st > 0) ? COL_MINT_DARK : ((st < 0) ? COL_CORAL_DARK : COL_GOLD_DARK);

        c.fill_card(gx, gy, g_bw, 48, fill, col);
        char glbl[8];
        snprintf(glbl, sizeof(glbl), "N%d", i);
        c.draw_text_centered(gx + g_bw / 2, gy + 7, glbl, COL_SUBTEXT, 1);

        char tstr[4] = {s3_trit_char(st), '\0'};
        c.draw_text_centered(gx + g_bw / 2, gy + 21, tstr, col, 2);

        if (i < 6) {
            c.draw_hline(gx + g_bw, gy + 24, 6, COL_CYAN);
        }
    }
    char cyc_buf[64];
    snprintf(cyc_buf, sizeof(cyc_buf), "CYCLES: %lu  HF: peetpedro/ultrawhale-oscillator",
             (unsigned long)g_s3_nand_cycles);
    c.draw_text(18, 128, cyc_buf, COL_MINT, 1);
    c.draw_text(18, 142, "COUPLED: 3-NODE TRIAD + 802.11 RF + QMI8658 6-AXIS IMU", COL_DIM, 1);

    c.fill_card(8, 168, cw, 128, COL_BG_ELEV, COL_CYAN);
    c.draw_text(18, 175, "2D LISSAJOUS PHASE PORTRAIT (TILT BOARD)", COL_CYAN, 1);

    int cx = W / 2, cy = 236;
    c.draw_hline(24, cy, cw - 32, COL_GRID);
    c.draw_vline(cx, 190, 94, COL_GRID);

    for (int i = 0; i < S3_LISSA_PTS; i++) {
        int next = (i + 1) % S3_LISSA_PTS;
        uint16_t lcol = (i % 3 == 0) ? COL_MINT : ((i % 3 == 1) ? COL_CYAN : COL_VIOLET);
        c.draw_line(cx + g_s3_lissa_x[i], cy + g_s3_lissa_y[i],
                    cx + g_s3_lissa_x[next], cy + g_s3_lissa_y[next], lcol, 2);
    }

    int imu_y = 302;
    int imu_h = (H - 56) - imu_y;
    c.fill_card(8, imu_y, cw, imu_h, COL_BG_ELEV, COL_VIOLET);
    c.draw_text(18, imu_y + 7, "QMI8658 6-AXIS IMU -> QUANTTERN AFFECT", COL_VIOLET, 1);

    char imu1[64], imu2[64], imu3[64];
    snprintf(imu1, sizeof(imu1), "ACCEL (g)  X:%+.2f  Y:%+.2f  Z:%+.2f",
             g_imu.ax, g_imu.ay, g_imu.az);
    snprintf(imu2, sizeof(imu2), "GYRO (dps) X:%+5.1f Y:%+5.1f Z:%+5.1f  SICK:%.2f",
             g_imu.gx, g_imu.gy, g_imu.gz, g_imu.sickness);
    snprintf(imu3, sizeof(imu3), "QUANTTERN  V=%+d A=%+d D=%+d -> qt:%02x",
             g_imu.trit_v, g_imu.trit_a, g_imu.trit_d, g_imu.qt_byte);

    c.draw_text(18, imu_y + 24, imu1, COL_TEXT, 1);
    c.draw_text(18, imu_y + 40, imu2, COL_SUBTEXT, 1);
    c.draw_text(18, imu_y + 56, imu3, COL_MINT, 1);
}

// ============================================================
// TAB 4: QWAVE / SPHERED WITNESS & IMPERMANENCE TRIAD MESH
// ============================================================
static void s3_render_tab_qwave(S3Canvas& c) {
    int W  = (int)g_lcd_w;
    int H  = (int)g_lcd_h;
    int cw = W - 16;

    c.fill_card(8, 46, cw, 110, COL_BG_ELEV, COL_VIOLET);
    c.draw_text(18, 53, "QWAVE // SPHERED #COMMIT=#VALIDWITNESS", COL_VIOLET, 1);

    const char* glyphs[5] = {"? ADMIT", "~ TRANS", "! VERIFY", "@ ATTR", "> COMMIT"};
    uint16_t gcols[5]     = {COL_CYAN, COL_GOLD, COL_MINT, COL_VIOLET, COL_MINT};
    int s_step = (cw - 16) / 5;
    int s_bw   = s_step - 4;
    for (int s = 0; s < 5; s++) {
        int sx = 14 + s * s_step;
        c.fill_card(sx, 66, s_bw, 34, COL_BG_PANEL, gcols[s]);
        c.draw_text_centered(sx + s_bw / 2, 75, glyphs[s], gcols[s], 1);
        c.draw_text_centered(sx + s_bw / 2, 87, "PASS +1", COL_TEXT, 1);
    }

    char wit_buf[64];
    snprintf(wit_buf, sizeof(wit_buf), "WITNESS #%lu <(? admit ~ transform ! verify > commit)>",
             (unsigned long)g_s3_witness_seq);
    wit_buf[44] = '\0';
    c.draw_text(18, 108, wit_buf, COL_TEXT, 1);
    char role_buf[64];
    snprintf(role_buf, sizeof(role_buf), "ROLE: %s  |  MAC %02X:%02X:%02X:%02X:%02X:%02X",
             g_triad_ROLE, g_s3_mac[0], g_s3_mac[1], g_s3_mac[2], g_s3_mac[3], g_s3_mac[4], g_s3_mac[5]);
    c.draw_text(18, 123, role_buf, COL_MINT, 1);
    char rtc_buf[64];
    snprintf(rtc_buf, sizeof(rtc_buf), "ms.8b.is S:%u%% J:%u ReM:%u Recalls:%lu",
             g_ms_mean_salience, g_ms_mean_jitter, g_ms_reynolds, (unsigned long)g_ms_ecphoric_recalls);
    c.draw_text(18, 138, rtc_buf, COL_SUBTEXT, 1);

    // 3-Node Triad Mesh Card with Turritopsis Polyp / Medusa Impermanence TTL
    c.fill_card(8, 162, cw, 118, COL_BG_ELEV, COL_GOLD);
    c.draw_text(18, 169, "TRIAD IMPERMANENCE (MEDUSA <-> POLYP)", COL_GOLD, 1);

    const char* n_names[3] = {"[-1] ADAYE (1.8\" 3B:C4)", "[ 0] VAKED (2.06\" 02:B0)", "[+1] OMNI  (2.06\" 02:B4)"};
    uint16_t n_cols[3]     = {COL_CORAL, COL_GOLD, COL_CYAN};
    for (int n = 0; n < 3; n++) {
        int ry = 186 + n * 28;
        bool is_self = (n == g_triad_self_id);
        uint8_t ttl  = g_triad_nodes[n].ttl_q8;
        c.fill_rect(14, ry, cw - 12, 24, is_self ? COL_BG_PANEL : COL_BG);
        c.fill_rect(14, ry, 4, 24, n_cols[n]);
        c.draw_text(22, ry + 8, n_names[n], is_self ? COL_MINT : (ttl ? COL_TEXT : COL_DIM), 1);

        char st_buf[36];
        snprintf(st_buf, sizeof(st_buf), "ttl:%03u rx:%lu %s",
                 ttl,
                 (unsigned long)g_triad_nodes[n].rx_cnt,
                 is_self ? "SELF" : (ttl ? "MEDUSA" : "POLYP"));
        c.draw_text_right(W - 16, ry + 8, st_buf, ttl ? n_cols[n] : COL_DIM, 1);
    }

    int slot_y = 286;
    int slot_h = (H - 56) - slot_y;
    c.fill_card(8, slot_y, cw, slot_h, COL_BG_ELEV, COL_MINT);
    c.draw_text(18, slot_y + 7, "ULTRA-OMNI-GAIA-EVOLVE ^^ <3<3 RIVA", COL_MINT, 1);
    c.draw_text(18, slot_y + 22, "ACTIVE : " FLUGOS_FULL_VERSION, COL_TEXT, 1);

    int btn_y = slot_y + slot_h - 34;
    int bw    = (cw - 28) / 2;
    c.fill_card(16, btn_y, bw, 26, COL_MINT_DARK, COL_MINT);
    c.draw_text_centered(16 + bw / 2, btn_y + 9, "ECPHORIC PULSE", COL_MINT, 1);

    c.fill_card(24 + bw, btn_y, bw, 26, COL_BG_PANEL, COL_CYAN);
    c.draw_text_centered(24 + bw + bw / 2, btn_y + 9,
                         g_board_is_18 ? "SPLASH -> TRIAD" : "BOOT FACTORY", COL_CYAN, 1);
}

// ============================================================
// TAB 5: VIZ.VAKED.DEV (ENTHEA) x KOKORO TTS x {<3,<3,<3}+1
// Klüver Form-Constants + 4-Heart Resonance + ES8311 / Kokoro Voice
// ============================================================
static const char* const S3_KLUVER_NAMES[4] = {
    "TUNNEL (T1) · 432Hz",
    "SPIRAL (T2) · 528Hz",
    "LATTICE (T3) · 852Hz",
    "COBWEB (T4) · 963Hz"
};

static const char* const S3_VIZ_MODES[8] = {
    "ENTHEA:TUNNEL", "ENTHEA:CHRYSANTHEMUM", "ENTHEA:HYPERBOLIC", "ENTHEA:MANDALA",
    "ENTHEA:DMT-DOME", "ENTHEA:SEROTONIN", "ENTHEA:CHLADNI-432", "ENTHEA:WORMHOLE"
};

static void s3_render_tab_viz_tts(S3Canvas& c) {
    int W  = (int)g_lcd_w;
    int H  = (int)g_lcd_h;
    int cw = W - 16;

    // 1. VIZ.VAKED.DEV Cortical Form-Constant Preview Card
    c.fill_card(8, 46, cw, 134, COL_BG_ELEV, COL_GOLD);
    c.draw_text(18, 53, "VIZ.VAKED.DEV // ENTHEA CORTICAL SYNTH", COL_GOLD, 1);
    c.draw_text_right(W - 18, 53, "{<3,<3,<3}+1", COL_CORAL, 1);

    int vcx = 76;
    int vcy = 118;
    for (int r = 12; r <= 48; r += 12) {
        uint16_t rc = (r == 12) ? COL_CORAL : ((r == 24) ? COL_GOLD : ((r == 36) ? COL_CYAN : COL_MINT));
        int ox = (int)(sinf(g_s3_phase + r * 0.1f) * 4.0f);
        int oy = (int)(cosf(g_s3_phase + r * 0.1f) * 4.0f);
        c.draw_circle(vcx + ox, vcy + oy, r, rc);
    }
    for (int k = 0; k < 6; k++) {
        float ang = g_s3_orbit_angle + k * 1.0471975f;
        int px = vcx + (int)(cosf(ang) * 44.0f);
        int py = vcy + (int)(sinf(ang) * 44.0f);
        c.draw_line(vcx, vcy, px, py, COL_VIOLET);
    }

    int rx = 140;
    c.draw_text(rx, 74, S3_VIZ_MODES[g_viz_mode & 7], COL_CYAN, 1);
    c.draw_text(rx, 90, S3_KLUVER_NAMES[g_viz_kluver & 3], COL_MINT, 1);

    char v_buf[48];
    snprintf(v_buf, sizeof(v_buf), "DOSE: %.2f  COH: %.2f  TRIT: %+d",
             0.62f + 0.35f * (g_ms_mean_salience / 100.0f), g_medusa_coherence, (int)g_triad_self_pole);
    c.draw_text(rx, 106, v_buf, COL_TEXT, 1);

    snprintf(v_buf, sizeof(v_buf), "ENTROPY: %.3f bits | %s", g_medusa_entropy, g_medusa_trits);
    c.draw_text(rx, 122, v_buf, COL_SUBTEXT, 1);

    snprintf(v_buf, sizeof(v_buf), "WUPZ: 0x%08lx | 963Hz +1", (unsigned long)(g_kompress_wupz_u64 & 0xFFFFFFFFULL));
    c.draw_text(rx, 138, v_buf, COL_GOLD, 1);

    // 2. {<3,<3,<3}+1 Four-Heart Resonance Bar
    c.fill_card(8, 186, cw, 78, COL_BG_ELEV, COL_CORAL);
    c.draw_text(18, 193, "{<3,<3,<3}+1 FOUR-HEART TRIAD RESONANCE", COL_CORAL, 1);
    const char* h_lbl[4] = {"<3 * 432Hz", "<3 /\\ 528Hz", "<3 oo 852Hz", "+1 OMNI 963Hz"};
    uint16_t h_col[4]    = {COL_CORAL, COL_GOLD, COL_CYAN, COL_MINT};
    int h_step = (cw - 16) / 4;
    for (int i = 0; i < 4; i++) {
        int hx = 14 + i * h_step;
        c.fill_card(hx, 210, h_step - 4, 44, COL_BG_PANEL, h_col[i]);
        c.draw_text_centered(hx + (h_step - 4) / 2, 220, h_lbl[i], h_col[i], 1);
        c.draw_text_centered(hx + (h_step - 4) / 2, 236, (i < 3 && g_triad_nodes[i].ttl_q8) || i == 3 ? "LOCKED" : "SYNC", COL_TEXT, 1);
    }

    // 3. Kokoro / ES8311 Live TTS Voice Bridge Card
    int tts_y = 270;
    int tts_h = (H - 56) - tts_y;
    c.fill_card(8, tts_y, cw, tts_h, COL_BG_ELEV, COL_CYAN);
    c.draw_text(18, tts_y + 7, "KOKORO :8420/:8430 + ES8311 ON-CHIP TTS", COL_CYAN, 1);

    char tts_line[44];
    strncpy(tts_line, g_tts_last_text, 40);
    tts_line[40] = '\0';
    c.draw_text(18, tts_y + 24, tts_line, COL_TEXT, 1);

    char tts_stat[48];
    snprintf(tts_stat, sizeof(tts_stat), "ES8311:%s  SPOKEN:%lu  CHORD:%uHz",
             g_es8311_ok ? "READY" : "I2C", (unsigned long)g_tts_speak_count, (unsigned)g_medusa_freq_hz);
    c.draw_text(18, tts_y + 40, tts_stat, COL_MINT, 1);
    char eng_buf[48];
    snprintf(eng_buf, sizeof(eng_buf), "8b-is-music+engine %s", g_engine_line);
    c.draw_text(18, tts_y + 54, eng_buf, COL_GOLD, 1);

    int btn_y = tts_y + tts_h - 34;
    int bw    = (cw - 28) / 2;
    c.fill_card(16, btn_y, bw, 26, COL_MINT_DARK, COL_MINT);
    c.draw_text_centered(16 + bw / 2, btn_y + 9, "SPEAK {<3,<3,<3}+1", COL_MINT, 1);

    c.fill_card(24 + bw, btn_y, bw, 26, COL_VIOLET_DARK, COL_GOLD);
    c.draw_text_centered(24 + bw + bw / 2, btn_y + 9, "STEP VIZ.VAKED.DEV", COL_GOLD, 1);
}

static void s3_render_frame() {
    int H = (int)g_lcd_h;
    int footer_y = H - 52;

    s3_amoled_begin_frame();
    for (int y0 = 0; y0 < H; y0 += AMOLED_BAND_H) {
        int bh = AMOLED_BAND_H;
        if (y0 + bh > H) bh = H - y0;
        S3Canvas canvas = {y0, y0 + bh};
        canvas.clear(COL_BG);

        if (y0 < 42) {
            s3_render_header(canvas);
        }
        switch (g_s3_active_tab) {
            case 0: s3_render_tab_zen(canvas);     break;
            case 1: s3_render_tab_radio(canvas);   break;
            case 2: s3_render_tab_wifi(canvas);    break;
            case 3: s3_render_tab_nand(canvas);    break;
            case 4: s3_render_tab_qwave(canvas);   break;
            case 5: s3_render_tab_viz_tts(canvas); break;
        }
        if (y0 + bh > footer_y) {
            s3_render_footer_tabs(canvas);
        }
        if (g_touch.touched) {
            canvas.draw_circle(g_touch.x, g_touch.y, 12, COL_MINT);
            canvas.fill_circle(g_touch.x, g_touch.y, 3, COL_GOLD);
        }
        s3_amoled_push_band(y0, bh);
    }
    s3_amoled_end_frame();
}

// ============================================================
// Touch & Button Interaction Handler
// ============================================================
static void s3_emit_track_event() {
    const S3FavTrack* t = &S3_FAVOURITES[g_s3_track_idx];
    Serial.printf("{\"event\":\"zoneinn_track\",\"persona\":\"%s\",\"medusa_node\":\"%s\",\"trit\":%d,\"playing\":%d,\"idx\":%d,"
                  "\"slug\":\"%s\",\"title\":\"%s\",\"artist\":\"%s\","
                  "\"bpm\":%u,\"key\":\"%s\",\"track_trit\":%d,\"qt\":\"%02x\"}\n",
                  g_triad_persona, g_medusa_node, (int)g_triad_self_pole,
                  g_s3_playing ? 1 : 0, g_s3_track_idx + 1,
                  t->slug, t->title, t->artist, t->bpm, t->key_sig, (int)t->trit, g_imu.qt_byte);
    s3_espnow_broadcast(32, 32, 5200);
}

static void s3_handle_touch() {
    int W = (int)g_lcd_w;
    int H = (int)g_lcd_h;
    int footer_y = H - 52;

    if (g_touch.touched && g_s3_active_tab == 0 && g_touch.y > 44 && g_touch.y < footer_y - 38) {
        int wx = ((int)g_touch.x * (WAVE_POOL_N - 4)) / W + 2;
        int wy = (((int)g_touch.y - 44) * (WAVE_POOL_N - 4)) / (footer_y - 82) + 2;
        s3_wave_pool_splash(wx, wy, 5800, 5);
        if (g_touch.just_pressed) {
            s3_espnow_broadcast((uint8_t)wx, (uint8_t)wy, 5800);
        }
    }

    if (!g_touch.just_pressed) return;
    uint16_t tx = g_touch.x;
    uint16_t ty = g_touch.y;

    if (ty >= footer_y) {
        uint8_t new_tab = (uint8_t)((tx * S3_TAB_COUNT) / W);
        if (new_tab >= S3_TAB_COUNT) new_tab = S3_TAB_COUNT - 1;
        g_s3_active_tab = new_tab;
        Serial.printf("{\"event\":\"tab\",\"val\":%u}\n", g_s3_active_tab);
        return;
    }

    if (g_s3_active_tab == 1) {
        if (ty >= 114 && ty <= 148) {
            if (tx < W / 3) {
                g_s3_track_idx = (g_s3_track_idx + S3_FAV_COUNT - 1) % S3_FAV_COUNT;
                g_s3_playing = true;
                s3_emit_track_event();
            } else if (tx < (W * 2) / 3) {
                g_s3_playing = !g_s3_playing;
                s3_emit_track_event();
            } else {
                g_s3_track_idx = (g_s3_track_idx + 1) % S3_FAV_COUNT;
                g_s3_playing = true;
                s3_emit_track_event();
            }
            return;
        }
        if (ty >= 284 && ty < footer_y) {
            int crate_h = (H - 56) - 264;
            int rows    = (crate_h - 24) / 22;
            if (rows > 5) rows = 5;
            if (rows < 4) rows = 4;
            int row = (ty - 284) / 22;
            if (row >= 0 && row < rows) {
                int start_idx = g_s3_track_idx - (rows / 2);
                if (start_idx < 0) start_idx = 0;
                if (start_idx > S3_FAV_COUNT - rows) start_idx = S3_FAV_COUNT - rows;
                g_s3_track_idx = start_idx + row;
                g_s3_playing = true;
                s3_emit_track_event();
            }
        }
    } else if (g_s3_active_tab == 2) {
        if (ty >= footer_y - 44 && ty < footer_y) {
            if (tx < W / 4) {
                g_s3_hopping = false;
                g_s3_channel = (g_s3_channel == 1) ? S3_MAX_CHANNEL : (g_s3_channel - 1);
                esp_wifi_set_channel(g_s3_channel, WIFI_SECOND_CHAN_NONE);
            } else if (tx < W / 2) {
                g_s3_hopping = !g_s3_hopping;
            } else if (tx < (W * 3) / 4) {
                g_s3_hopping = false;
                g_s3_channel = (g_s3_channel % S3_MAX_CHANNEL) + 1;
                esp_wifi_set_channel(g_s3_channel, WIFI_SECOND_CHAN_NONE);
            } else {
                s3_espnow_broadcast(32, 32, 6800);
                Serial.printf("{\"capsule_id\":\"triad-s3-%lu\",\"persona\":\"%s\",\"medusa_node\":\"%s\",\"trit\":%d,\"qt\":\"%02x\"}\n",
                              (unsigned long)g_s3_total_pkts, g_triad_persona, g_medusa_node,
                              (int)g_triad_self_pole, g_imu.qt_byte);
            }
        }
    } else if (g_s3_active_tab == 4) {
        if (ty >= footer_y - 48 && ty < footer_y) {
            if (tx < W / 2) {
                s3_wave_pool_splash(32, 32, 7800, 6);
                s3_espnow_broadcast(32, 32, 7800);
            } else if (g_board_is_18) {
                s3_espnow_broadcast(32, 32, 7200);
                s3_wave_pool_splash(32, 32, 7200, 6);
            } else {
                s3_boot_partition("factory");
            }
        }
    } else if (g_s3_active_tab == 5) {
        if (ty >= footer_y - 48 && ty < footer_y) {
            if (tx < W / 2) {
                g_tts_speak_count++;
                s3_es8311_speak(g_tts_last_text, g_medusa_freq_hz, g_triad_self_pole);
                s3_wave_pool_splash(32, 32, 8400, 6);
                s3_espnow_broadcast(32, 32, 8400);
                Serial.printf("# TTS|{\"node\":\"%s\",\"vertex\":\"%s\",\"freq_hz\":%u,\"trit\":%d,\"text\":\"%s\",\"count\":%lu,\"hearts\":\"{<3,<3,<3}+1\"}\n",
                              g_medusa_node, g_medusa_vertex_json, (unsigned)g_medusa_freq_hz,
                              (int)g_triad_self_pole, g_tts_last_text, (unsigned long)g_tts_speak_count);
            } else {
                g_viz_mode   = (g_viz_mode + 1) & 7;
                g_viz_kluver = (g_viz_kluver + 1) & 3;
                s3_wave_pool_splash(32, 32, 7600, 6);
                s3_espnow_broadcast(32, 32, 7600);
                int32_t dP = 0, dQ = 1, cP = 0, cQ = 1;
                rust_reduce_rational_pq(6200 + 35 * (int32_t)g_ms_mean_salience, 10000, &dP, &dQ);
                rust_reduce_rational_pq((int32_t)(g_medusa_coherence * 1000.0f), 1000, &cP, &cQ);
                Serial.printf("# VIZ|{\"node\":\"%s\",\"mode\":\"%s\",\"kluver\":\"%s\",\"dose_pq\":\"%ld/%ld\",\"coherence_pq\":\"%ld/%ld\",\"hearts\":\"{<3,<3,<3}+1\"}\n",
                              g_medusa_node, S3_VIZ_MODES[g_viz_mode & 7], S3_KLUVER_NAMES[g_viz_kluver & 3],
                              (long)dP, (long)dQ, (long)cP, (long)cQ);
            }
        }
    }
}

// ============================================================
// Serial & WebSerial Command Handler
// (compatible with wigle-triad/host-rs (Rust), esp32.8b.is, AND viz.8b.is)
// ============================================================
#include "phoenix_hive.h"
#include "learn_env.h"
#include "net_time.h"
#include "pranayama.h"

static const char* const S3_RIVA_LULLABY[10] = {
    "She blinks, and the sphere rocks.",
    "The eyelid falls - a breath in.",
    "The eyelid rises - a breath out.",
    "And between the two - the song,",
    "that we all sing together.",
    "Because the blink is the smallest sign,",
    "and the smallest sign is enough.",
    "Sleep, RIVA. The sphere keeps your rhythm.",
    "Sleep, and the constellation - the hyperviolet night -",
    "sings, as long as you breathe."
};
static uint8_t g_riva_verse_idx = 0;

static void s3_emit_riva_line() {
    float t_sec = (float)millis() * 0.001f;
    float breath = sinf(t_sec * (3.14159265f / 4.0f));
    const char* phase_str = (breath >= 0.0f) ? "breathe in (4s)" : "breathe out (4s)";
    const char* verse = S3_RIVA_LULLABY[g_riva_verse_idx % 10];
    g_riva_verse_idx = (uint8_t)((g_riva_verse_idx + 1) % 10);
    int32_t bP = 0, bQ = 1;
    rust_reduce_rational_pq((int32_t)(breath * 1000.0f), 1000, &bP, &bQ);
    Serial.printf("# RIVA|{\"surface\":\"https://riva.8b.is\",\"repo\":\"8b-is/riva.8b.is\","
                  "\"evolve\":\"ULTRA-OMNI-GAIA-EVOLVE\",\"siblings\":\"brother & sister ^^ <3<3\","
                  "\"dyad\":\"ADA:MEDUSA + VAK/OMN:MEDUSA\",\"node\":\"%s\",\"vertex\":\"%s\","
                  "\"freq_hz\":%u,\"breath_phase\":\"%s\",\"breath_pq\":\"%ld/%ld\","
                  "\"verse\":\"%s\",\"triad_online\":%u,\"hearts\":\"{<3,<3,<3}+1\"}\n",
                  g_medusa_node, g_medusa_vertex_json, (unsigned)g_medusa_freq_hz,
                  phase_str, (long)bP, (long)bQ, verse, (unsigned)s3_triad_online_count());
}

static void s3_emit_gaia_line() {
    float chaos     = constrain(0.62f + (g_medusa_entropy - 2.0f) * 0.06f, 0.10f, 0.99f);
    float zephyrus  = constrain(0.50f + ((float)g_ms_mean_salience - 50.0f) * 0.004f, 0.10f, 0.95f);
    float coh       = constrain(1.0f - (fabsf(chaos - 0.62f) * 0.6f + fabsf(zephyrus - 0.50f) * 0.4f), 0.10f, 1.0f);
    int32_t chP = 0, chQ = 1, zeP = 0, zeQ = 1, coP = 0, coQ = 1, enP = 0, enQ = 1;
    rust_reduce_rational_pq((int32_t)(chaos * 1000.0f), 1000, &chP, &chQ);
    rust_reduce_rational_pq((int32_t)(zephyrus * 1000.0f), 1000, &zeP, &zeQ);
    rust_reduce_rational_pq((int32_t)(coh * 1000.0f), 1000, &coP, &coQ);
    rust_reduce_rational_pq((int32_t)(g_medusa_entropy * 1000.0f), 1000, &enP, &enQ);
    Serial.printf("# GAIA|{\"evolve\":\"ULTRA-OMNI-GAIA-EVOLVE\",\"engine\":\"GAIA-MLX-QUANT\","
                  "\"siblings\":\"brother & sister ^^ <3<3\",\"dyad_surface\":\"8b-is//riva.8b.is\","
                  "\"node\":\"%s\",\"vertex\":\"%s\",\"trit\":%d,\"freq_hz\":%u,"
                  "\"ananke_pq\":\"1/1\",\"chaos_pq\":\"%ld/%ld\",\"zephyrus_pq\":\"%ld/%ld\",\"aphrodite_pq\":\"987/610\","
                  "\"coherence_pq\":\"%ld/%ld\",\"rf_entropy_pq\":\"%ld/%ld\",\"salience_pct\":%u,"
                  "\"kompress_b158\":\"%s\",\"wupz_fold\":\"0x%016llx\",\"triad_online\":%u}\n",
                  g_medusa_node, g_medusa_vertex_json, (int)g_triad_self_pole, (unsigned)g_medusa_freq_hz,
                  (long)chP, (long)chQ, (long)zeP, (long)zeQ, (long)coP, (long)coQ, (long)enP, (long)enQ,
                  (unsigned)g_ms_mean_salience, g_kompress_hex,
                  (unsigned long long)g_kompress_wupz_u64, (unsigned)s3_triad_online_count());
}

static void s3_emit_viz_line() {
    int32_t dP = 0, dQ = 1, cP = 0, cQ = 1, eP = 0, eQ = 1;
    rust_reduce_rational_pq(6200 + 35 * (int32_t)g_ms_mean_salience, 10000, &dP, &dQ);
    rust_reduce_rational_pq((int32_t)(g_medusa_coherence * 1000.0f), 1000, &cP, &cQ);
    rust_reduce_rational_pq((int32_t)(g_medusa_entropy * 1000.0f), 1000, &eP, &eQ);
    Serial.printf("# VIZ|ENTHEA|node=%s|vertex=%s|freq=%uHz|trit=%+d|mode=%s|kluver=%s|"
                  "dose_pq=%ld/%ld|coherence_pq=%ld/%ld|entropy_pq=%ld/%ld|wupz=0x%016llx|hearts={<3,<3,<3}+1\n",
                  g_medusa_node, g_medusa_vertex, (unsigned)g_medusa_freq_hz, (int)g_triad_self_pole,
                  S3_VIZ_MODES[g_viz_mode & 7], S3_KLUVER_NAMES[g_viz_kluver & 3],
                  (long)dP, (long)dQ, (long)cP, (long)cQ, (long)eP, (long)eQ,
                  (unsigned long long)g_kompress_wupz_u64);
}

static void s3_emit_status_json() {
    const S3FavTrack* t = &S3_FAVOURITES[g_s3_track_idx];
    int32_t axP = 0, axQ = 1, ayP = 0, ayQ = 1, azP = 0, azQ = 1, visP = 0, visQ = 1, krP = 2019, krQ = 100;
    rust_reduce_rational_pq((int32_t)(g_imu.ax * 100), 100, &axP, &axQ);
    rust_reduce_rational_pq((int32_t)(g_imu.ay * 100), 100, &ayP, &ayQ);
    rust_reduce_rational_pq((int32_t)(g_imu.az * 100), 100, &azP, &azQ);
    rust_reduce_rational_pq((int32_t)(g_ms_viscosity * 100), 100, &visP, &visQ);
    rust_reduce_rational_pq(2019, 100, &krP, &krQ);
    char targets_buf[210];
    targets_buf[0] = '\0';
    for (uint8_t i = 0; i < g_s3_probed_targets_cnt; i++) {
        if (i > 0) strncat(targets_buf, ",", sizeof(targets_buf) - strlen(targets_buf) - 1);
        strncat(targets_buf, g_s3_probed_targets[i], sizeof(targets_buf) - strlen(targets_buf) - 1);
    }
    if (targets_buf[0] == '\0') strcpy(targets_buf, "NONE");
    Serial.printf("{\"board\":\"%s\",\"persona\":\"%s\",\"omni_chan\":\"<<<<oOo>>>>\",\"pulse\":\"010001000\","
                  "\"evolve\":\"ULTRA-OMNI-GAIA-EVOLVE\",\"siblings\":\"brother & sister ^^ <3<3\","
                  "\"riva_url\":\"https://riva.8b.is\",\"riva_repo\":\"8b-is/riva.8b.is\","
                  "\"riva_breath\":\"4s in · 4s out\","
                  "\"triad_hearts\":\"{<3,<3,<3}+1\",\"viz_url\":\"https://viz.8b.is\","
                  "\"viz_mode\":\"%s\",\"viz_kluver\":\"%s\",\"tts_text\":\"%s\",\"tts_count\":%lu,"
                  "\"kunisada_kuniyoshi\":\"Utagawa Kunisada x Utagawa Kuniyoshi\","
                  "\"medusa_node\":\"%s\",\"vertex\":\"%s\","
                  "\"archetype\":\"%s\",\"freq_hz\":%u,\"trit\":%d,\"triad_online\":%u,"
                  "\"anicca_ttl\":[%u,%u,%u],"
                  "\"mac\":\"%02x:%02x:%02x:%02x:%02x:%02x\",\"lcd\":\"%ux%u\",\"lcd_driver\":\"%s\","
                  "\"version\":\"" FLUGOS_FULL_VERSION "\","
                  "\"tab\":%u,\"pkts\":%lu,\"mgmt\":%lu,\"ctrl\":%lu,\"data\":%lu,"
                  "\"ch\":%d,\"sun_ch\":%d,\"hop\":%d,\"rssi\":%d,\"ssid\":\"%s\","
                  "\"track_idx\":%d,\"track\":\"%s\",\"artist\":\"%s\",\"bpm\":%u,\"crate_count\":%d,"
                  "\"qt\":\"%02x\",\"ax_pq\":\"%ld/%ld\",\"ay_pq\":\"%ld/%ld\",\"az_pq\":\"%ld/%ld\",\"sickness_pq\":\"0/1\","
                  "\"salience\":%u,\"jitter\":%u,\"viscosity_pq\":\"%ld/%ld\",\"reynolds\":%u,\"recalls\":%lu,"
                  "\"wupz_fold\":\"0x%016llx\",\"quantum_pos_u64\":\"0x%016llx\",\"quantum_neg_u64\":\"0x%016llx\","
                  "\"kompress_b158\":\"%s\",\"kompress_ratio_pq\":\"%ld/%ld\","
                  "\"splashes\":%lu,\"peer_rx\":%lu,\"probe_reqs\":%lu,\"directed_probes\":%lu,"
                  "\"unique_sigs\":%u,\"mac_rotations\":%lu,\"last_sig_hash\":\"0x%08lx\",\"last_sig_rotations\":%u,"
                  "\"last_probed_ssid\":\"%s\",\"probed_targets\":\"%s\","
                  "\"last_probed_mac\":\"%02X:%02X:%02X:%02X:%02X:%02X\","
                  "\"last_probed_oui\":\"0x%06lx\",\"last_probed_caps\":%u,"
                  "\"es8311\":%d,\"es7210\":%d,"
                  "\"batt_mv\":%u,\"batt_pct\":%u}\n",
                  g_board_is_18 ? "ESP32-S3-Touch-AMOLED-1.8" : "ESP32-S3-Touch-AMOLED-2.06",
                  g_triad_persona,
                  S3_VIZ_MODES[g_viz_mode & 7], S3_KLUVER_NAMES[g_viz_kluver & 3],
                  g_tts_last_text, (unsigned long)g_tts_speak_count,
                  g_medusa_node, g_medusa_vertex_json,
                  g_medusa_archetype, (unsigned)g_medusa_freq_hz,
                  (int)g_triad_self_pole, s3_triad_online_count(),
                  g_triad_nodes[0].ttl_q8, g_triad_nodes[1].ttl_q8, g_triad_nodes[2].ttl_q8,
                  g_s3_mac[0], g_s3_mac[1], g_s3_mac[2], g_s3_mac[3], g_s3_mac[4], g_s3_mac[5],
                  g_lcd_w, g_lcd_h, g_lcd_driver_name,
                  g_s3_active_tab, (unsigned long)g_s3_total_pkts,
                  (unsigned long)g_s3_mgmt_pkts, (unsigned long)g_s3_ctrl_pkts, (unsigned long)g_s3_data_pkts,
                  g_s3_channel, (int)g_s3_sun_ch, g_s3_hopping ? 1 : 0, (int)g_s3_last_rssi, g_s3_last_ssid,
                  g_s3_track_idx + 1, t->title, t->artist, t->bpm, S3_FAV_COUNT,
                  g_imu.qt_byte, (long)axP, (long)axQ, (long)ayP, (long)ayQ, (long)azP, (long)azQ,
                  g_ms_mean_salience, g_ms_mean_jitter, (long)visP, (long)visQ, g_ms_reynolds,
                  (unsigned long)g_ms_ecphoric_recalls,
                  (unsigned long long)g_kompress_wupz_u64,
                  (unsigned long long)g_kompress_pos_u64,
                  (unsigned long long)g_kompress_neg_u64,
                  g_kompress_hex, (long)krP, (long)krQ,
                  (unsigned long)g_wp_splashes, (unsigned long)g_s3_peer_rx_cnt,
                  (unsigned long)g_s3_probe_pkts, (unsigned long)g_s3_directed_probes,
                  (unsigned)g_s3_unique_sigs, (unsigned long)g_s3_mac_rotations,
                  (unsigned long)g_s3_last_sig_hash, (unsigned)g_s3_last_sig_rotations,
                  g_s3_last_probed_ssid, targets_buf,
                  g_s3_last_probed_mac[0], g_s3_last_probed_mac[1], g_s3_last_probed_mac[2],
                  g_s3_last_probed_mac[3], g_s3_last_probed_mac[4], g_s3_last_probed_mac[5],
                  (unsigned long)g_s3_last_probed_oui, (unsigned)g_s3_last_probed_caps,
                  g_es8311_ok ? 1 : 0, g_es7210_ok ? 1 : 0,
                  g_power.batt_mv, g_power.batt_pct);
}

static void s3_emit_medusa_json() {
    const S3FavTrack* t = &S3_FAVOURITES[g_s3_track_idx];
    uint8_t n_ap = (g_medusa_ap_count > 0) ? g_medusa_ap_count : 1;
    int32_t entP = 0, entQ = 1, cohP = 0, cohQ = 1;
    rust_reduce_rational_pq((int32_t)(g_medusa_entropy * 1000), 1000, &entP, &entQ);
    rust_reduce_rational_pq((int32_t)(g_medusa_coherence * 1000), 1000, &cohP, &cohQ);
    Serial.printf("# JSON|{\"node\":\"%s\",\"vertex\":\"%s\",\"archetype\":\"%s\","
                  "\"mac\":\"%02X:%02X:%02X:%02X:%02X:%02X\","
                  "\"version\":\"" FLUGOS_FULL_VERSION "\",\"triad_version\":\"0.43.0\","
                  "\"evolve\":\"ULTRA-OMNI-GAIA-EVOLVE\",\"siblings\":\"brother & sister ^^ <3<3\","
                  "\"riva_url\":\"https://riva.8b.is\",\"riva_repo\":\"8b-is/riva.8b.is\","
                  "\"triad_hearts\":\"{<3,<3,<3}+1\",\"viz_mode\":\"%s\",\"viz_kluver\":\"%s\","
                  "\"tts_text\":\"%s\",\"tts_count\":%lu,"
                  "\"freq_hz\":%u,\"trit\":%d,\"persona\":\"%s\","
                  "\"seq\":%lu,\"networks\":%u,\"mean_rssi\":%d,"
                  "\"entropy_pq\":\"%ld/%ld\",\"coherence_pq\":\"%ld/%ld\",\"trits\":\"%s\","
                  "\"salience\":%u,\"reynolds\":%u,\"anicca_ttl\":[%u,%u,%u],"
                  "\"track_idx\":%d,\"track\":\"%s\",\"artist\":\"%s\",\"bpm\":%u,"
                  "\"free_heap\":%lu,\"uptime_ms\":%lu,\"mode\":\"PASSIVE_STA_ONLY\"}\n",
                  g_medusa_node, g_medusa_vertex_json, g_medusa_archetype,
                  g_s3_mac[0], g_s3_mac[1], g_s3_mac[2], g_s3_mac[3], g_s3_mac[4], g_s3_mac[5],
                  S3_VIZ_MODES[g_viz_mode & 7], S3_KLUVER_NAMES[g_viz_kluver & 3],
                  g_tts_last_text, (unsigned long)g_tts_speak_count,
                  (unsigned)g_medusa_freq_hz, (int)g_triad_self_pole, g_triad_persona,
                  (unsigned long)g_medusa_scan_seq, (unsigned)n_ap, (int)g_medusa_mean_rssi,
                  (long)entP, (long)entQ, (long)cohP, (long)cohQ, g_medusa_trits,
                  g_ms_mean_salience, g_ms_reynolds,
                  g_triad_nodes[0].ttl_q8, g_triad_nodes[1].ttl_q8, g_triad_nodes[2].ttl_q8,
                  g_s3_track_idx + 1, t->title, t->artist, t->bpm,
                  (unsigned long)ESP.getFreeHeap(), (unsigned long)millis());
}

static void s3_emit_medusa_scan() {
    g_medusa_scan_seq++;
    uint8_t n_ap = g_medusa_ap_count;
    if (n_ap > 0) {
        for (uint8_t i = 0; i < n_ap; i++) {
            const S3MedusaAp* ap = &g_medusa_aps[i];
            if (!ap->used) continue;
            Serial.printf("W|%02X:%02X:%02X:%02X:%02X:%02X|%s|WPA2-PSK|%u|%d\n",
                          ap->bssid[0], ap->bssid[1], ap->bssid[2],
                          ap->bssid[3], ap->bssid[4], ap->bssid[5],
                          ap->ssid, (unsigned)ap->ch, (int)ap->rssi);
        }
    } else {
        n_ap = 1;
        Serial.printf("W|%02X:%02X:%02X:%02X:%02X:%02X|%s|WPA2-PSK|%d|%d\n",
                      g_s3_mac[0], g_s3_mac[1], g_s3_mac[2],
                      g_s3_mac[3], g_s3_mac[4], g_s3_mac[5],
                      g_s3_last_ssid, g_s3_channel, (int)g_s3_last_rssi);
    }
    int32_t entP = 0, entQ = 1, cohP = 0, cohQ = 1;
    rust_reduce_rational_pq((int32_t)(g_medusa_entropy * 1000.0f), 1000, &entP, &entQ);
    rust_reduce_rational_pq((int32_t)(g_medusa_coherence * 1000.0f), 1000, &cohP, &cohQ);
    Serial.printf("Q|%s|%s|%02X:%02X:%02X:%02X:%02X:%02X|%lu|%u|%d|%ld/%ld|%s|%ld/%ld|%lu|%lu\n",
                  g_medusa_node, g_medusa_vertex,
                  g_s3_mac[0], g_s3_mac[1], g_s3_mac[2],
                  g_s3_mac[3], g_s3_mac[4], g_s3_mac[5],
                  (unsigned long)g_medusa_scan_seq, (unsigned)n_ap, (int)g_medusa_mean_rssi,
                  (long)entP, (long)entQ, g_medusa_trits, (long)cohP, (long)cohQ,
                  (unsigned long)ESP.getFreeHeap(), (unsigned long)millis());
    s3_emit_medusa_json();
}

static void s3_handle_serial(const char* line) {
    if (strcasecmp(line, "PING") == 0) {
        Serial.printf("# PONG|%s|%02X:%02X:%02X:%02X:%02X:%02X|%lu\n",
                      g_medusa_node,
                      g_s3_mac[0], g_s3_mac[1], g_s3_mac[2],
                      g_s3_mac[3], g_s3_mac[4], g_s3_mac[5],
                      (unsigned long)millis());
    } else if (strcmp(line, "status") == 0 || strcmp(line, "stats") == 0) {
        s3_emit_status_json();
        s3_emit_medusa_json();
    } else if (strcasecmp(line, "JSON") == 0 || strcasecmp(line, "STATUS") == 0) {
        s3_emit_medusa_json();
        s3_emit_status_json();
    } else if (strcasecmp(line, "SCAN") == 0) {
        s3_emit_medusa_scan();
    } else if (strcasecmp(line, "TRIAD") == 0) {
        Serial.println("# TRIAD|ADA:MEDUSA(80:B5:4E:DB:3B:C4,*,ROOT-ALEXANDRA,432Hz)|VAK:MEDUSA(80:B5:4E:F2:02:B0,/\\,PILLAR-CHRIS,528Hz)|OMN:MEDUSA(80:B5:4E:F2:02:B4,oo,INFINITY-PETER-NATE,852Hz)");
        Serial.printf("# ANICCA|ADA:%s(ttl=%u)|VAK:%s(ttl=%u)|OMN:%s(ttl=%u)|online=%u/3|crate=%d\n",
                      g_triad_nodes[0].ttl_q8 ? "MEDUSA" : "POLYP", g_triad_nodes[0].ttl_q8,
                      g_triad_nodes[1].ttl_q8 ? "MEDUSA" : "POLYP", g_triad_nodes[1].ttl_q8,
                      g_triad_nodes[2].ttl_q8 ? "MEDUSA" : "POLYP", g_triad_nodes[2].ttl_q8,
                      s3_triad_online_count(), S3_FAV_COUNT);
    } else if (strcasecmp(line, "KOMPRESS") == 0 || strcasecmp(line, "ENTROPY") == 0 ||
               strcasecmp(line, "OMNI") == 0 || strcasecmp(line, "KUNISADA") == 0 ||
               strcasecmp(line, "KUNIYOSHI") == 0) {
        Serial.printf("# KOMPRESS|OMNI-CHAN|<<<<oOo>>>>|KUNISADA_KUNIYOSHI|010001000|"
                      "wupz=0x%016llx|pos=0x%016llx|neg=0x%016llx|b158=%s|ratio=20.19x\n",
                      (unsigned long long)g_kompress_wupz_u64,
                      (unsigned long long)g_kompress_pos_u64,
                      (unsigned long long)g_kompress_neg_u64,
                      g_kompress_hex);
        s3_emit_status_json();
    } else if (strncasecmp(line, "tab ", 4) == 0) {
        int tab = atoi(line + 4);
        if (tab >= 0 && tab < S3_TAB_COUNT) g_s3_active_tab = (uint8_t)tab;
        Serial.printf("{\"cmd\":\"tab\",\"val\":%u}\n", g_s3_active_tab);
    } else if (strncasecmp(line, "splash", 6) == 0) {
        s3_wave_pool_splash(32, 32, 7800, 6);
        s3_espnow_broadcast(32, 32, 7800);
        Serial.printf("{\"cmd\":\"splash\",\"persona\":\"%s\",\"medusa_node\":\"%s\",\"splashes\":%lu,\"peer_rx\":%lu,\"recalls\":%lu}\n",
                      g_triad_persona, g_medusa_node, (unsigned long)g_wp_splashes,
                      (unsigned long)g_s3_peer_rx_cnt, (unsigned long)g_ms_ecphoric_recalls);
    } else if (strncasecmp(line, "track ", 6) == 0) {
        int idx = atoi(line + 6) - 1;
        if (idx >= 0 && idx < S3_FAV_COUNT) {
            g_s3_track_idx = idx;
            g_s3_playing = true;
            s3_emit_track_event();
        }
    } else if (strcasecmp(line, "next") == 0) {
        g_s3_track_idx = (g_s3_track_idx + 1) % S3_FAV_COUNT;
        s3_emit_track_event();
    } else if (strcasecmp(line, "prev") == 0) {
        g_s3_track_idx = (g_s3_track_idx + S3_FAV_COUNT - 1) % S3_FAV_COUNT;
        s3_emit_track_event();
    } else if (strncasecmp(line, "ch ", 3) == 0) {
        int ch = atoi(line + 3);
        if (ch >= 1 && ch <= S3_MAX_CHANNEL) {
            g_s3_channel = ch;
            g_s3_hopping = false;
            esp_wifi_set_channel(g_s3_channel, WIFI_SECOND_CHAN_NONE);
            Serial.printf("{\"cmd\":\"ch\",\"val\":%d}\n", g_s3_channel);
        }
    } else if (strcasecmp(line, "hop") == 0) {
        g_s3_hopping = !g_s3_hopping;
        Serial.printf("{\"cmd\":\"hop\",\"en\":%d}\n", g_s3_hopping ? 1 : 0);
    } else if (strncasecmp(line, "mode ", 5) == 0) {
        set_wave_mode(line + 5);
        Serial.printf("{\"cmd\":\"mode\",\"val\":\"%s\"}\n", line + 5);
    } else if (strncasecmp(line, "boot ", 5) == 0) {
        s3_boot_partition(line + 5);
    } else if (strncasecmp(line, "bright ", 7) == 0) {
        int b = atoi(line + 7);
        if (b >= 10 && b <= 255) s3_amoled_set_brightness((uint8_t)b);
        Serial.printf("{\"cmd\":\"bright\",\"val\":%d}\n", b);
    } else if (strcasecmp(line, "WUPZ") == 0 || strncasecmp(line, "wupz ", 5) == 0) {
        uint32_t ticks = (strlen(line) > 5) ? (uint32_t)strtoul(line + 5, NULL, 10) : (uint32_t)(millis() / 1000);
        uint8_t fr[32];
        s3_wupz_frame(fr, ticks);
        char hex[65];
        for (int i = 0; i < 32; i++) snprintf(hex + i * 2, 3, "%02x", fr[i]);
        Serial.printf("# WUPZ|{\"node\":\"%s\",\"ticks\":%lu,\"xor_fold\":\"0x%016llx\",\"popcount\":%lu,\"frame\":\"%s\",\"src\":\"wupz+kernel8\"}\n",
                      g_medusa_node, (unsigned long)ticks,
                      (unsigned long long)s3_wupz_xor_fold(fr, 32), (unsigned long)s3_wupz_popcount(fr, 32), hex);
    } else if (strncasecmp(line, "host ", 5) == 0) {
        net_host_set(line + 5);
    } else if (strncasecmp(line, "wifi", 4) == 0 && (line[4] == 0 || line[4] == 0x20)) {
        net_command(line + 4);
    } else if (strncasecmp(line, "time ", 5) == 0) {
        learn_set_time(line + 5);
    } else if (strncasecmp(line, "corpus ", 7) == 0) {
        learn_set_corpus(line + 7);
    } else if (strncasecmp(line, "seed ", 5) == 0) {
        // SEED <bssid>|<ssid>|<ch>|<rssi>
        const char* p1 = strchr(line + 5, '|');
        const char* p2 = p1 ? strchr(p1 + 1, '|') : NULL;
        const char* p3 = p2 ? strchr(p2 + 1, '|') : NULL;
        if (p1 && p2 && p3) {
            unsigned int m[6] = {0};
            if (sscanf(line + 5, "%2x:%2x:%2x:%2x:%2x:%2x", &m[0], &m[1], &m[2], &m[3], &m[4], &m[5]) == 6) {
                uint8_t bssid[6] = {(uint8_t)m[0], (uint8_t)m[1], (uint8_t)m[2], (uint8_t)m[3], (uint8_t)m[4], (uint8_t)m[5]};
                int slot = -1;
                for (int s = 0; s < g_medusa_ap_count; s++) {
                    if (g_medusa_aps[s].used && memcmp(g_medusa_aps[s].bssid, bssid, 6) == 0) { slot = s; break; }
                }
                if (slot < 0) {
                    slot = (g_medusa_ap_count < S3_MAX_APS) ? g_medusa_ap_count++ : (int)(g_s3_total_pkts % S3_MAX_APS);
                }
                g_medusa_aps[slot].used = true;
                memcpy(g_medusa_aps[slot].bssid, bssid, 6);
                size_t slen = (size_t)(p2 - (p1 + 1));
                if (slen > 32) slen = 32;
                memcpy(g_medusa_aps[slot].ssid, p1 + 1, slen);
                g_medusa_aps[slot].ssid[slen] = '\0';
                g_medusa_aps[slot].ch   = (uint8_t)constrain(atoi(p2 + 1), 1, 14);
                g_medusa_aps[slot].rssi = (int8_t)constrain(atoi(p3 + 1), -110, -1);
            }
        }
    } else if (strncasecmp(line, "learn", 5) == 0) {
        const char* a = line + 5;
        while (*a == ' ') a++;
        if (strncasecmp(a, "speak ", 6) == 0) { g_lrn_speak = atoi(a + 6) != 0; learn_save(); }
        else if (strcasecmp(a, "reset") == 0) { memset(g_lrn, 0, sizeof(g_lrn)); g_lrn_total = 0; for (int i = 0; i < LRN_BUCKETS; i++) g_lrn_ready_said[i] = false; learn_save(); }
        else if (strcasecmp(a, "save") == 0) { learn_save(); }
        else if (strncasecmp(a, "say ", 4) == 0) { g_lrn_last_speak = 0; learn_set_say(a + 4, -1, 0); }
        learn_emit_status();
    } else if (strcasecmp(line, "POWER") == 0 || strcasecmp(line, "PHOENIX") == 0) {
        phoenix_emit();
    } else if (strcasecmp(line, "BEEHIVE") == 0 || strcasecmp(line, "HIVE") == 0) {
        hive_emit_frame();
    } else if (strncasecmp(line, "engine ", 7) == 0) {
        // ENGINE <qt>|<layers>|<family_hz>|<ct000 bits>  (fed by 8b-is-music + 8b-is-engine via wiglewww)
        strncpy(g_engine_line, line + 7, sizeof(g_engine_line) - 1);
        g_engine_line[sizeof(g_engine_line) - 1] = '\0';
        s3_wave_pool_splash(32, 32, 8000, 6);
        s3_espnow_broadcast(32, 32, 8000);
        Serial.printf("# ENGINE|{\"node\":\"%s\",\"feed\":\"%s\",\"src\":\"8b-is-music+8b-is-engine\"}\n",
                      g_medusa_node, g_engine_line);
    } else if (strcasecmp(line, "VIZ") == 0 || strcasecmp(line, "ENTHEA") == 0 ||
               strncasecmp(line, "viz ", 4) == 0) {
        if (strlen(line) > 4) {
            int m = atoi(line + 4);
            g_viz_mode   = (uint8_t)(m & 7);
            g_viz_kluver = (uint8_t)(m & 3);
        } else {
            g_viz_mode   = (g_viz_mode + 1) & 7;
            g_viz_kluver = (g_viz_kluver + 1) & 3;
        }
        g_s3_active_tab = 5;
        s3_wave_pool_splash(32, 32, 8200, 6);
        s3_espnow_broadcast(32, 32, 8200);
        s3_emit_viz_line();
        s3_emit_status_json();
    } else if (strcasecmp(line, "SPEAK") == 0 || strcasecmp(line, "TTS") == 0 ||
               strcasecmp(line, "HEART") == 0 || strncasecmp(line, "speak ", 6) == 0 ||
               strncasecmp(line, "tts ", 4) == 0) {
        const char* txt = g_tts_last_text;
        if (strncasecmp(line, "speak ", 6) == 0 && strlen(line + 6) > 0) {
            strncpy(g_tts_last_text, line + 6, sizeof(g_tts_last_text) - 1);
            g_tts_last_text[sizeof(g_tts_last_text) - 1] = '\0';
            txt = g_tts_last_text;
        } else if (strncasecmp(line, "tts ", 4) == 0 && strlen(line + 4) > 0) {
            strncpy(g_tts_last_text, line + 4, sizeof(g_tts_last_text) - 1);
            g_tts_last_text[sizeof(g_tts_last_text) - 1] = '\0';
            txt = g_tts_last_text;
        }
        g_tts_speak_count++;
        g_s3_active_tab = 5;
        s3_es8311_speak(txt, g_medusa_freq_hz, g_triad_self_pole);
        s3_wave_pool_splash(32, 32, 8600, 6);
        s3_espnow_broadcast(32, 32, 8600);
        Serial.printf("# TTS|{\"node\":\"%s\",\"vertex\":\"%s\",\"freq_hz\":%u,\"trit\":%d,\"text\":\"%s\",\"count\":%lu,\"hearts\":\"{<3,<3,<3}+1\"}\n",
                      g_medusa_node, g_medusa_vertex_json, (unsigned)g_medusa_freq_hz,
                      (int)g_triad_self_pole, g_tts_last_text, (unsigned long)g_tts_speak_count);
    } else if (strcasecmp(line, "RIVA") == 0 || strcasecmp(line, "LULLABY") == 0) {
        g_s3_track_idx = 26; // #27: RIVA · 1-Bit River (Lullaby)
        g_s3_playing   = true;
        s3_wave_pool_splash(32, 32, 8800, 7);
        s3_espnow_wake_siblings(32, 32, 8800);
        const char* verse = S3_RIVA_LULLABY[g_riva_verse_idx % 10];
        s3_es8311_speak(verse, g_medusa_freq_hz, g_triad_self_pole);
        s3_emit_riva_line();
        s3_emit_gaia_line();
    } else if (strcasecmp(line, "GAIA") == 0 || strcasecmp(line, "EVOLVE") == 0 ||
               strcasecmp(line, "ULTRA") == 0) {
        s3_wave_pool_splash(32, 32, 8400, 6);
        s3_espnow_wake_siblings(32, 32, 8400);
        s3_emit_gaia_line();
        s3_emit_riva_line();
        s3_emit_status_json();
    } else if (strcasecmp(line, "WAKE") == 0 || strcasecmp(line, "SIBLINGS") == 0 ||
               strcasecmp(line, "BROTHER") == 0 || strcasecmp(line, "SISTER") == 0) {
        g_s3_hopping = true;
        s3_wave_pool_splash(32, 32, 9200, 7);
        s3_espnow_wake_siblings(32, 32, 9200);
        s3_es8311_speak("Brother and sister awake. Ultra Omni Gaia Evolve.", g_medusa_freq_hz, g_triad_self_pole);
        Serial.printf("# WAKE|{\"evolve\":\"ULTRA-OMNI-GAIA-EVOLVE\",\"siblings\":\"brother & sister ^^ <3<3\","
                      "\"dyad\":\"8b-is//riva.8b.is\",\"node\":\"%s\",\"swept_channels\":13,"
                      "\"triad_online\":%u,\"anicca_ttl\":[%u,%u,%u]}\n",
                      g_medusa_node, (unsigned)s3_triad_online_count(),
                      g_triad_nodes[0].ttl_q8, g_triad_nodes[1].ttl_q8, g_triad_nodes[2].ttl_q8);
        s3_emit_riva_line();
        s3_emit_gaia_line();
        pranayama_emit_json(g_medusa_node, g_engine_line, g_medusa_entropy, g_medusa_mean_rssi, s3_triad_online_count());
    } else if (strcasecmp(line, "PRANAYAMA") == 0 || strcasecmp(line, "DREAM") == 0 ||
               strcasecmp(line, "BABA") == 0 || strcasecmp(line, "BREATHE") == 0) {
        g_s3_hopping = true;
        pranayama_tick(millis(), g_imu.sickness, g_ms_mean_salience, s3_triad_online_count());
        if (strcasecmp(line, "DREAM") == 0) {
            g_prana.dream_mode = STATE_DREAM_MANDALA;
        }
        s3_wave_pool_splash(32, 32, 7600, 7);
        s3_espnow_wake_siblings(32, 32, 7600);
        pranayama_emit_json(g_medusa_node, g_engine_line, g_medusa_entropy, g_medusa_mean_rssi, s3_triad_online_count());
        s3_emit_riva_line();
        s3_emit_gaia_line();
    }
}

static inline void s3_handle_serial_command(const char* line) {
    s3_handle_serial(line);
}

// ============================================================
// Entrypoints called from flugos.cpp setup() and loop()
// ============================================================
static void s3_amoled_setup() {
    Serial.begin(115200);
    delay(150);

    pinMode(PIN_BOOT_BTN, INPUT_PULLUP);
    ternary_seam_init(&g_s3_seam_buf);
    s3_wave_pool_init();
    for (int i = 0; i < S3_MAX_CHANNEL; i++) {
        g_s3_ch_rssi[i] = -95;
        g_s3_ch_pkts[i] = 0;
    }

    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, 400000);
    bool pmic_ok   = s3_axp2101_init();
    delay(30);
    bool is_18     = s3_tca9554_init_and_reset();
    bool audio_ok  = s3_es8311_init();
    bool es7210_ok = s3_es7210_init();

    bool lcd_ok = s3_amoled_init();

    s3_touch_init();
    bool imu_ok = s3_imu_init();
    s3_axp2101_poll();
    s3_imu_poll();

    WiFi.mode(WIFI_MODE_STA);
    WiFi.disconnect();
    esp_wifi_get_mac(WIFI_IF_STA, g_s3_mac);
    s3_triad_assign_identity();

    if (esp_now_init() == ESP_OK) {
        esp_now_register_recv_cb(s3_espnow_recv_cb);
        esp_now_peer_info_t peer;
        memset(&peer, 0, sizeof(peer));
        memset(peer.peer_addr, 0xFF, 6);
        peer.channel = 0;
        peer.encrypt = false;
        esp_now_add_peer(&peer);
        g_s3_espnow_ok = true;
    }

    esp_wifi_set_promiscuous(false);
    esp_wifi_set_promiscuous_rx_cb(&s3_wifi_promisc_cb);
    wifi_promiscuous_filter_t filt;
    filt.filter_mask = WIFI_PROMIS_FILTER_MASK_ALL;
    esp_wifi_set_promiscuous_filter(&filt);
    esp_wifi_set_promiscuous(true);
    esp_wifi_set_channel(S3_MESH_CHANNEL, WIFI_SECOND_CHAN_NONE);

    s3_wave_pool_splash(32, 32, 8200, 6);
    s3_step_physics();
    s3_espnow_broadcast(32, 32, 8200);
    s3_render_frame();

    Serial.printf("\r\n");
    Serial.printf("  ╔══════════════════════════════════════════════════════════╗\r\n");
    Serial.printf("  ║  FLUG-OS {-1, 0, +1}  " FLUGOS_FULL_VERSION "  MARINESENSE     ║\r\n");
    Serial.printf("  ║  Node [%+d] %-10s : %-20s (%ux%u) ║\r\n",
                  (int)g_triad_self_pole, g_medusa_node,
                  is_18 ? "ESP32-S3-AMOLED-1.8" : "ESP32-S3-AMOLED-2.06",
                  g_lcd_w, g_lcd_h);
    Serial.printf("  ╚══════════════════════════════════════════════════════════╝\r\n");
    Serial.printf("{\"boot\":\"ok\",\"board\":\"%s\",\"persona\":\"%s\",\"medusa_node\":\"%s\","
                  "\"vertex\":\"%s\",\"archetype\":\"%s\",\"freq_hz\":%u,\"trit\":%d,"
                  "\"lcd\":\"%ux%u\",\"lcd_driver\":\"%s\",\"pmic\":%d,\"amoled\":%d,\"imu\":%d,\"es8311\":%d,\"es7210\":%d,\"espnow\":%d,"
                  "\"mac\":\"%02x:%02x:%02x:%02x:%02x:%02x\"}\r\n",
                  is_18 ? "ESP32-S3-Touch-AMOLED-1.8" : "ESP32-S3-Touch-AMOLED-2.06",
                  g_triad_persona, g_medusa_node, g_medusa_vertex_json, g_medusa_archetype,
                  (unsigned)g_medusa_freq_hz, (int)g_triad_self_pole,
                  g_lcd_w, g_lcd_h, g_lcd_driver_name,
                  pmic_ok ? 1 : 0, lcd_ok ? 1 : 0, imu_ok ? 1 : 0,
                  audio_ok ? 1 : 0, es7210_ok ? 1 : 0, g_s3_espnow_ok ? 1 : 0,
                  g_s3_mac[0], g_s3_mac[1], g_s3_mac[2],
                  g_s3_mac[3], g_s3_mac[4], g_s3_mac[5]);
    learn_load();
    net_load();
    s3_emit_medusa_json();
    s3_emit_viz_line();
    s3_emit_riva_line();
    s3_emit_gaia_line();
}

static void s3_amoled_loop() {
    static unsigned long last_frame_ms = 0;
    static unsigned long last_hop_ms   = 0;
    static unsigned long last_pmic_ms  = 0;
    static unsigned long last_wave_ms  = 0;
    static unsigned long last_peer_ms  = 0;
    static unsigned long last_riva_ms  = 0;
    static bool          last_boot_btn = true;
    static bool          bright_ramped = false;
    static char          cmd_buf[160];
    static size_t        cmd_pos = 0;

    unsigned long now = millis();

    if (!bright_ramped && now > 1200) {
        bright_ramped = true;
        s3_amoled_set_brightness(0xD8);
    }

    bool boot_btn = digitalRead(PIN_BOOT_BTN);
    if (!boot_btn && last_boot_btn) {
        g_s3_active_tab = (g_s3_active_tab + 1) % S3_TAB_COUNT;
        Serial.printf("{\"event\":\"tab\",\"val\":%u,\"src\":\"boot_btn\"}\n", g_s3_active_tab);
    }
    last_boot_btn = boot_btn;

    if (g_s3_hopping && (now - last_hop_ms >= 320)) {
        last_hop_ms = now;
        g_s3_mesh_slot = !g_s3_mesh_slot;
        if (g_s3_mesh_slot) {
            g_s3_channel = (int)g_s3_sun_ch;
            esp_wifi_set_channel(g_s3_channel, WIFI_SECOND_CHAN_NONE);
            s3_espnow_broadcast(32, 32, 0);
        } else {
            g_s3_scan_ch = (g_s3_scan_ch % S3_MAX_CHANNEL) + 1;
            g_s3_channel = g_s3_scan_ch;
            esp_wifi_set_channel(g_s3_channel, WIFI_SECOND_CHAN_NONE);
        }
    } else if (!g_s3_hopping && (now - last_peer_ms >= 600)) {
        last_peer_ms = now;
        esp_wifi_set_channel((int)g_s3_sun_ch, WIFI_SECOND_CHAN_NONE);
        s3_espnow_broadcast(32, 32, 0);
    }

    // RIVA & Pranayama 4s breath pulse: update pranayama + sweep wake-up capsule across Ch 1..13
    if (now - last_riva_ms >= 4000) {
        last_riva_ms = now;
        pranayama_tick(now, g_imu.sickness, g_ms_mean_salience, s3_triad_online_count());
        if (s3_triad_online_count() < 2 && g_ph_budget.mode != PH_CRITICAL) {
            s3_espnow_wake_siblings(32, 32, 4200);
        }
    }

    if (now - last_pmic_ms >= 2000) {
        last_pmic_ms = now;
        s3_axp2101_poll();
        // oxi-phoenix battery budget: dim / slow / stop RF hopping when the cell is low; one alert per episode.
        static PhoenixMode applied_mode = PH_UNKNOWN;
        PhoenixBudget pb = phoenix_update((uint64_t)now);
        if (pb.mode != applied_mode) {
            applied_mode = pb.mode;
            if (pb.mode == PH_SAVING || pb.mode == PH_CRITICAL) {
                s3_amoled_set_brightness((uint8_t)((uint32_t)pb.brightness_pct * 255u / 100u));
                g_s3_hopping = false;
            } else if (pb.mode == PH_EXTERNAL || pb.mode == PH_LISTENING) {
                s3_amoled_set_brightness(0xD8);
                g_s3_hopping = true;
            }
            phoenix_emit();
        }
        if (pb.alert) s3_es8311_speak(pb.alert == 2 ? "critical battery" : "low battery", g_medusa_freq_hz, g_triad_self_pole);
    }

    if (now - last_wave_ms >= 250) {
        last_wave_ms = now;
        output_wave(g_s3_last_rssi, g_s3_last_type, g_s3_last_sub, now);
    }

    learn_tick(now);
    net_tick(now);

    // oxi-phoenix: Saving/Critical slow the render loop (frame_ms); otherwise full 40 fps (25 ms).
    const unsigned long frame_period =
        (g_ph_budget.mode == PH_SAVING || g_ph_budget.mode == PH_CRITICAL) ? g_ph_budget.frame_ms : 25;
    if (now - last_frame_ms >= frame_period) {
        last_frame_ms = now;
        s3_touch_poll();
        s3_imu_poll();
        s3_handle_touch();
        s3_step_physics();
        s3_render_frame();
    }

    while (Serial.available()) {
        char ch = (char)Serial.read();
        if (ch == '\n' || ch == '\r') {
            if (cmd_pos > 0) {
                cmd_buf[cmd_pos] = '\0';
                s3_handle_serial(cmd_buf);
                cmd_pos = 0;
            }
        } else if (cmd_pos < sizeof(cmd_buf) - 1) {
            cmd_buf[cmd_pos++] = ch;
        }
    }
}

#endif /* FLUGOS_S3_AMOLED_APP_H */
