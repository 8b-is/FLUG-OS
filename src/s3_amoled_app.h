/*
 * FLUG-OS — Sovereign ESP32-S3 AMOLED Cockpit Application (v0.5.0+ultra-omni-zen)
 * Unified across:
 *   1. Waveshare ESP32-S3-Touch-AMOLED-1.8  (368x448 SH8601 QSPI, TCA9554, ES8311, "adaye")
 *   2. Waveshare ESP32-S3-Touch-AMOLED-2.06 (410x502 SH8601/CO5300 QSPI, "vaked")
 *
 * 5 Interactive Views (Touch Bar + BOOT Button + Serial Control + ESP-NOW Peer Sync):
 *   [0] ZEN    — ULTRA-OMNI-ZEN: baye-rust Jelly Sphere Mind-Wall + 64x64 2D Wave Pool + Eye of Aye
 *   [1] RADIO  — zoneInn / radio.vaked.dev 14 Favourite Musics Crate + 8-band Ternary EQ
 *   [2] 802.11 — FLUG-OS Promiscuous 802.11 Spectrum (Ch 1-13) + Sine Kernel K(u)=sin(pi*u)/(pi*u)
 *   [3] NAND   — Ultrawhale 7-stage Ternary NAND Ring Oscillator x QMI8658 IMU Lissajous
 *   [4] QWAVE  — sphered #Commit=#ValidWitness 5-Stage Witness + Hardware & Partition Switcher
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
#define S3_WAVE_HIST    88
#define S3_LISSA_PTS    44
#define S3_FAV_COUNT    14
#define S3_TAB_COUNT    5
#define WAVE_POOL_N     64

// ============================================================
// 14 Favourite Musics Crate (zoneInn / radio.vaked.dev parity)
// ============================================================
typedef struct {
    const char* slug;
    const char* title;
    const char* artist;
    const char* genre;
    const char* key_sig;
    uint16_t    bpm;
    uint32_t    fnv_hash;
} S3FavTrack;

static const S3FavTrack S3_FAVOURITES[S3_FAV_COUNT] = {
    {"innerbloom",        "Innerbloom",            "RUFUS DU SOL",        "Prog House",   "F#m", 122, 0x8F41C2A9},
    {"mahakala",          "Mahakala",              "Sovereign Cut",       "Deep Ritual",  "Dm",  118, 0x3D9E71B4},
    {"take-five",         "Take Five",             "Dave Brubeck",        "5/4 Cool Jazz","Ebm", 174, 0xA7205E1C},
    {"so-what",           "So What",               "Miles Davis",         "Modal Jazz",   "Dm",  136, 0x61B8904D},
    {"time",              "Time",                  "Pink Floyd",          "Prog Rock",    "F#m", 120, 0xC4523E81},
    {"clair-de-lune",     "Clair de Lune",         "Claude Debussy",      "Impressionist","Db",   66, 0x90E14A72},
    {"gymnopedie-1",      "Gymnopedie No. 1",      "Erik Satie",          "Minimalist",   "D",    72, 0x5B3981D0},
    {"golden-ratio-432",  "Golden Ratio 432xPhi",  "vaked.dev Studio",    "Harmonic",     "A432",108, 0x16180339},
    {"beethoven-5th",     "Symphony No. 5 Op.67",  "L. van Beethoven",    "Classical",    "Cm",  108, 0x77E20508},
    {"ode-to-joy",        "Ode to Joy (9th)",      "L. van Beethoven",    "Choral",       "D",   120, 0x9E3779B9},
    {"fur-elise",         "Fur Elise WoO 59",      "L. van Beethoven",    "Bagatelle",    "Am",   76, 0x4A1D83C2},
    {"pachelbel-canon",   "Canon in D Major",      "Johann Pachelbel",    "Baroque",      "D",    64, 0x2B992DD5},
    {"midnight-sovereign","Midnight Sovereign",    "8b-is x vaked",       "Synthwave",    "Cm",  114, 0x8B15F00D},
    {"ultrawhale-nand",   "Ultrawhale NAND Groove","peetpedro / HF",      "Ternary DSP",  "Phi", 128, 0xE5F34101},
};

// ============================================================
// 2D Wave Pool (64x64 Integer Laplacian Solver from baye-rust)
// ============================================================
static int16_t  g_wp_u_curr[WAVE_POOL_N][WAVE_POOL_N];
static int16_t  g_wp_u_prev[WAVE_POOL_N][WAVE_POOL_N];
static uint32_t g_wp_splashes = 0;

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

static void s3_wave_pool_step() {
    for (int y = 1; y < WAVE_POOL_N - 1; y++) {
        for (int x = 1; x < WAVE_POOL_N - 1; x++) {
            int32_t center = g_wp_u_curr[y][x];
            int32_t prev   = g_wp_u_prev[y][x];
            int32_t lap    = (int32_t)g_wp_u_curr[y - 1][x]
                           + (int32_t)g_wp_u_curr[y + 1][x]
                           + (int32_t)g_wp_u_curr[y][x - 1]
                           + (int32_t)g_wp_u_curr[y][x + 1]
                           - 4 * center;
            int32_t next   = 2 * center - prev + (lap >> 2);
            // Damping 251/256 (~0.9805) matching baye-rust WavePool2D
            int32_t damped = (next * 251) >> 8;
            if (damped > 16384)  damped = 16384;
            if (damped < -16384) damped = -16384;
            g_wp_u_prev[y][x] = (int16_t)damped;
        }
    }
    // Absorbing boundary conditions
    for (int i = 0; i < WAVE_POOL_N; i++) {
        g_wp_u_prev[0][i]               = 0;
        g_wp_u_prev[WAVE_POOL_N - 1][i] = 0;
        g_wp_u_prev[i][0]               = 0;
        g_wp_u_prev[i][WAVE_POOL_N - 1] = 0;
    }
    // Swap u_curr and u_prev
    for (int y = 0; y < WAVE_POOL_N; y++) {
        for (int x = 0; x < WAVE_POOL_N; x++) {
            int16_t tmp       = g_wp_u_curr[y][x];
            g_wp_u_curr[y][x] = g_wp_u_prev[y][x];
            g_wp_u_prev[y][x] = tmp;
        }
    }
}

// ============================================================
// ESP-NOW Peer Capsule Protocol (adaye 1.8" <-> vaked 2.06")
// ============================================================
typedef struct __attribute__((packed)) {
    uint8_t  magic[4];      // "OMNI"
    uint8_t  board_is_18;   // 1 = 1.8" adaye, 0 = 2.06" vaked
    uint8_t  track_idx;     // 0..13
    uint8_t  qt_byte;       // QuantTern affect byte
    uint8_t  splash_x;      // 0..63 wave pool x
    uint8_t  splash_y;      // 0..63 wave pool y
    int16_t  splash_amp;    // wave splash amplitude
    uint32_t seq;           // witness sequence
} S3OmniPeerPacket;

static volatile uint32_t g_s3_peer_rx_cnt = 0;
static volatile uint8_t  g_s3_peer_qt     = 0x15;
static volatile uint8_t  g_s3_peer_seen   = 0;
static unsigned long     g_s3_peer_last_ms = 0;
static bool              g_s3_espnow_ok   = false;

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

static int      g_s3_channel     = 6;
static bool     g_s3_hopping     = true;
static uint8_t  g_s3_active_tab  = 0;   // 0=ZEN, 1=RADIO, 2=802.11, 3=NAND, 4=QWAVE
static int      g_s3_track_idx   = 0;   // 0..13
static bool     g_s3_playing     = true;
static uint32_t g_s3_witness_seq = 1;
static float    g_s3_orbit_angle = 0.0f;

static int8_t   g_s3_ch_rssi[S3_MAX_CHANNEL];
static uint32_t g_s3_ch_pkts[S3_MAX_CHANNEL];
static char     g_s3_last_ssid[33] = "vaked.dev-constellation";
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
// ESP-NOW Receive & Broadcast Helpers
// ============================================================
static void s3_espnow_recv_cb(const uint8_t* mac_addr, const uint8_t* data, int len) {
    if (!data || len < (int)sizeof(S3OmniPeerPacket)) return;
    const S3OmniPeerPacket* pkt = (const S3OmniPeerPacket*)data;
    if (pkt->magic[0] != 'O' || pkt->magic[1] != 'M' || pkt->magic[2] != 'N' || pkt->magic[3] != 'I') {
        return;
    }
    g_s3_peer_rx_cnt++;
    g_s3_peer_qt      = pkt->qt_byte;
    g_s3_peer_seen    = 1;
    g_s3_peer_last_ms = millis();
    if (pkt->track_idx < S3_FAV_COUNT) {
        g_s3_track_idx = pkt->track_idx;
    }
    if (pkt->splash_amp != 0) {
        s3_wave_pool_splash(pkt->splash_x, pkt->splash_y, pkt->splash_amp, 5);
    }
}

static void s3_espnow_broadcast(uint8_t sx, uint8_t sy, int16_t amp) {
    if (!g_s3_espnow_ok) return;
    S3OmniPeerPacket pkt;
    pkt.magic[0]    = 'O';
    pkt.magic[1]    = 'M';
    pkt.magic[2]    = 'N';
    pkt.magic[3]    = 'I';
    pkt.board_is_18 = g_board_is_18 ? 1 : 0;
    pkt.track_idx   = (uint8_t)g_s3_track_idx;
    pkt.qt_byte     = g_imu.qt_byte;
    pkt.splash_x    = sx;
    pkt.splash_y    = sy;
    pkt.splash_amp  = amp;
    pkt.seq         = g_s3_witness_seq;
    static const uint8_t bcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    esp_now_send(bcast, (const uint8_t*)&pkt, sizeof(pkt));
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

        // Detect sibling Waveshare board over the air (OUI 80:B5:4E)
        if (hdr->addr2[0] == 0x80 && hdr->addr2[1] == 0xB5 && hdr->addr2[2] == 0x4E &&
            (hdr->addr2[5] != g_s3_mac[5])) {
            g_s3_peer_seen = 1;
            g_s3_peer_rx_cnt++;
        }

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
                    }
                    break;
                }
                off += 2 + tlen;
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
// Ternary NAND Ring, 2D Wave Pool & Waveform Physics Update
// ============================================================
static inline int8_t s3_ternary_nand(int8_t a, int8_t b) {
    int8_t mn = (a < b) ? a : b;
    return (int8_t)(-mn);
}

static void s3_step_physics() {
    // 1. Step 7-stage cross-coupled ternary NAND ring oscillator
    int8_t imu_mod = g_imu.trit_v;
    int8_t rf_mod  = ternary_seam_rssi_to_code(g_s3_last_rssi);
    int8_t next0   = s3_ternary_nand(g_s3_nand_ring[6],
                     (int8_t)((imu_mod + rf_mod > 0) ? 1 : (imu_mod + rf_mod < 0 ? -1 : 0)));
    for (int i = 6; i >= 1; i--) {
        int8_t fb = (i == 3) ? g_s3_nand_ring[0] : g_s3_nand_ring[(i + 5) % 7];
        g_s3_nand_ring[i] = s3_ternary_nand(g_s3_nand_ring[i - 1], fb);
    }
    g_s3_nand_ring[0] = next0;
    g_s3_nand_cycles += 7;

    // 2. Step AxiomQuant Sine Kernel K(u) = sin(pi*u)/(pi*u)
    g_s3_phase += 0.14f;
    g_s3_orbit_angle += 0.045f + g_imu.sickness * 0.08f;
    float rssi_u = normalize_rssi(g_s3_last_rssi) * 2.4f;
    float k_val  = sine_kernel(rssi_u + 0.35f * sinf(g_s3_phase));
    float carrier = sinf(g_s3_phase * 2.2f + (float)g_s3_last_type * 0.8f) * k_val;
    g_s3_wave_hist[g_s3_wave_head] = carrier;
    g_s3_wave_head = (g_s3_wave_head + 1) % S3_WAVE_HIST;

    // 3. Step 64x64 2D Wave Pool + periodic harmonic / IMU / RF excitation
    if ((g_s3_witness_seq % 4) == 0) {
        // Orbiting harmonic wave source driven by active zoneInn BPM + IMU tilt
        float rad = 14.0f + 6.0f * sinf(g_s3_phase * 0.5f);
        int wx = 32 + (int)(cosf(g_s3_orbit_angle) * rad + g_imu.ax * 10.0f);
        int wy = 32 + (int)(sinf(g_s3_orbit_angle) * rad + g_imu.ay * 10.0f);
        int16_t amp = (int16_t)(2800.0f * k_val + (float)g_s3_nand_ring[0] * 1400.0f);
        s3_wave_pool_splash(wx, wy, amp, 4);
    }
    if (g_imu.sickness > 0.25f) {
        int sx = 32 + (int)(g_imu.gx * 0.08f) % 20;
        int sy = 32 + (int)(g_imu.gy * 0.08f) % 20;
        s3_wave_pool_splash(sx, sy, (int16_t)(g_imu.sickness * 6200.0f), 6);
    }
    s3_wave_pool_step();

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

    // 6. Render TernarySeam radiation matrix
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

    // Brand + Board Persona
    c.fill_circle(14, 20, 5, COL_MINT);
    c.draw_circle(14, 20, 8, COL_GOLD);
    c.draw_text(28, 7, g_board_is_18 ? "OMNI-ZEN // ADAYE" : "OMNI-ZEN // VAKED", COL_TEXT, 2);
    c.draw_text(28, 25, g_board_is_18 ? "1.8\" 368x448 v0.5.0" : "2.06\" 410x502 v0.5.0", COL_MINT, 1);

    // QuantTern affect pill
    int qt_x = W - 150;
    c.fill_card(qt_x, 6, 70, 28, COL_VIOLET_DARK, COL_VIOLET);
    char qt_buf[16];
    snprintf(qt_buf, sizeof(qt_buf), "qt:%02x %c%c%c",
             g_imu.qt_byte,
             s3_trit_char(g_imu.trit_v),
             s3_trit_char(g_imu.trit_a),
             s3_trit_char(g_imu.trit_d));
    c.draw_text_centered(qt_x + 35, 16, qt_buf, COL_VIOLET, 1);

    // Battery / PMIC pill
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

    const char* labels[S3_TAB_COUNT] = {"ZEN", "RADIO", "802.11", "NAND", "QWAVE"};
    const char* subs[S3_TAB_COUNT]   = {"sphere", "zoneInn", "rf+sinc", "imu+osc", "sphered"};
    uint16_t cols[S3_TAB_COUNT]      = {COL_MINT, COL_GOLD, COL_CYAN, COL_CORAL, COL_VIOLET};

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
// TAB 0: ULTRA-OMNI-ZEN — Jelly Sphere Mind-Wall + 2D Wave Pool
// (Evolves baye-rust adaye + zoneInn + ultrawhale NAND + qwave)
// ============================================================
static void s3_render_tab_zen(S3Canvas& c) {
    int W  = (int)g_lcd_w;
    int H  = (int)g_lcd_h;
    int cx = W / 2;
    int cy = 40 + (H - 92) / 2 - 6;
    int outer_r = (W < 380) ? 142 : 158;
    int inner_r = outer_r - 14;
    int outer_r2 = outer_r * outer_r;
    int inner_r2 = inner_r * inner_r;

    // 1. Render 2D Wave Pool + Jelly Sphere Mind-Wall per scanline in band
    int y_start = (c.y0 > 42) ? c.y0 : 42;
    int y_end   = (c.y1 < H - 54) ? c.y1 : (H - 54);

    float sick = g_imu.sickness;
    uint8_t sick_u8 = (uint8_t)(sick * 200.0f);

    for (int py = y_start; py < y_end; py++) {
        int dy  = py - cy;
        int dy2 = dy * dy;
        if (dy2 > outer_r2) continue;

        uint16_t* row_ptr = &s_band_buf[(py - c.y0) * W];

        for (int px = 8; px < W - 8; px++) {
            int dx = px - cx;
            int r2 = dx * dx + dy2;
            if (r2 > outer_r2) continue;

            if (r2 >= inner_r2) {
                // Orbiting Amber/Mint Info Ring Membrane
                int ring_band = r2 - inner_r2;
                int ring_span = outer_r2 - inner_r2;
                if (ring_band < (ring_span / 5) || ring_band > (ring_span * 4 / 5)) {
                    row_ptr[px] = (sick_u8 > 60) ? COL_CORAL : COL_AMBER_INFO;
                } else {
                    row_ptr[px] = COL_BG_ELEV;
                }
            } else {
                // Inside the Jelly Sphere: sample 64x64 2D Wave Pool
                int gx = ((dx + inner_r) * (WAVE_POOL_N - 2)) / (2 * inner_r) + 1;
                int gy = ((dy + inner_r) * (WAVE_POOL_N - 2)) / (2 * inner_r) + 1;
                if (gx < 1) gx = 1;
                if (gx > WAVE_POOL_N - 2) gx = WAVE_POOL_N - 2;
                if (gy < 1) gy = 1;
                if (gy > WAVE_POOL_N - 2) gy = WAVE_POOL_N - 2;

                int16_t h = g_wp_u_curr[gy][gx];
                // Radial Fresnel rim glow near inner_r
                int rim = (r2 * 48) / inner_r2;

                uint8_t r = 6 + (uint8_t)((rim * 18) >> 6);
                uint8_t g = 10 + (uint8_t)((rim * 36) >> 6);
                uint8_t b = 22 + (uint8_t)((rim * 54) >> 6);

                if (h > 64) {
                    uint32_t mag = (uint32_t)h >> 5;
                    if (mag > 220) mag = 220;
                    // Sovereign Mint / Cyan constructive wave crest
                    r = (uint8_t)min(255U, (uint32_t)r + (mag >> 2) + (sick_u8 >> 1));
                    g = (uint8_t)min(255U, (uint32_t)g + mag);
                    b = (uint8_t)min(255U, (uint32_t)b + ((mag * 3) >> 2));
                } else if (h < -64) {
                    uint32_t mag = (uint32_t)(-h) >> 5;
                    if (mag > 220) mag = 220;
                    // QuantTern Violet / Indigo wave trough
                    r = (uint8_t)min(255U, (uint32_t)r + ((mag * 3) >> 2) + (sick_u8 >> 1));
                    g = (uint8_t)min(255U, (uint32_t)g + (mag >> 2));
                    b = (uint8_t)min(255U, (uint32_t)b + mag);
                } else if (sick_u8 > 25) {
                    r = (uint8_t)min(255U, (uint32_t)r + (sick_u8 >> 1));
                }

                // Subtle grid lines every 8 cells inside the sphere
                if (((px - cx) & 31) == 0 || ((py - cy) & 31) == 0) {
                    g = (uint8_t)min(255, (int)g + 10);
                    b = (uint8_t)min(255, (int)b + 16);
                }

                row_ptr[px] = rgb565_be(r, g, b);
            }
        }
    }

    // 2. Orbiting 7-stage Ternary NAND Nodes on the Amber Info Ring
    int mid_r = (outer_r + inner_r) / 2;
    for (int i = 0; i < 7; i++) {
        float ang = g_s3_orbit_angle + (float)i * (2.0f * 3.14159265f / 7.0f);
        int nx = cx + (int)(cosf(ang) * (float)mid_r);
        int ny = cy + (int)(sinf(ang) * (float)mid_r);
        uint16_t ncol = s3_trit_color(g_s3_nand_ring[i]);
        c.fill_circle(nx, ny, 4, ncol);
        c.draw_circle(nx, ny, 6, COL_TEXT);
    }

    // 3. Eye of Aye Central Spark + IMU Tilt Reticle
    int eye_x = cx + (int)(g_imu.ax * 36.0f);
    int eye_y = cy + (int)(g_imu.ay * 36.0f);
    c.draw_circle(cx, cy, inner_r / 2, COL_GRID);
    c.draw_line(cx - 14, cy, cx + 14, cy, COL_GRID, 1);
    c.draw_line(cx, cy - 14, cx, cy + 14, COL_GRID, 1);
    c.fill_circle(eye_x, eye_y, 5, (sick > 0.3f) ? COL_CORAL : COL_MINT);
    c.draw_circle(eye_x, eye_y, 10, COL_GOLD);
    c.draw_circle(eye_x, eye_y, 15, COL_CYAN);

    // 4. Top & Bottom Zen HUD Pills
    const S3FavTrack* trk = &S3_FAVOURITES[g_s3_track_idx];
    char top_lbl[56];
    snprintf(top_lbl, sizeof(top_lbl), "%s // %s (%uBPM)",
             trk->title, trk->key_sig, trk->bpm);
    top_lbl[36] = '\0';
    c.draw_text_centered(cx, 46, top_lbl, COL_MINT, 1);

    // Bottom Zen Telemetry Bar (above footer tabs)
    int hud_y = H - 90;
    c.fill_card(10, hud_y, W - 20, 34, COL_BG_ELEV, COL_BORDER);
    char hud1[64];
    snprintf(hud1, sizeof(hud1), "WAVE:%lu  PKTS:%lu  CH:%02d (%ddBm)",
             (unsigned long)g_wp_splashes, (unsigned long)g_s3_total_pkts,
             g_s3_channel, (int)g_s3_last_rssi);
    c.draw_text(18, hud_y + 6, hud1, COL_TEXT, 1);

    char hud2[64];
    snprintf(hud2, sizeof(hud2), "PEER:%s (rx:%lu)  TAP SPHERE=SPLASH  BTN=TAB",
             g_s3_peer_seen ? (g_board_is_18 ? "VAKED-2.06" : "ADAYE-1.8") : "SCANNING",
             (unsigned long)g_s3_peer_rx_cnt);
    c.draw_text(18, hud_y + 19, hud2, g_s3_peer_seen ? COL_MINT : COL_GOLD, 1);
}

// ============================================================
// TAB 1: ZONEINN / RADIO.VAKED.DEV — 14 Favourite Musics + Ternary EQ
// ============================================================
static void s3_render_tab_radio(S3Canvas& c) {
    int W = (int)g_lcd_w;
    int H = (int)g_lcd_h;
    int cw = W - 16;
    const S3FavTrack* trk = &S3_FAVOURITES[g_s3_track_idx];

    // Now Playing Hero Card
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

    // Transport buttons inside Hero Card
    int bw1 = (cw - 32) / 4;
    int bw2 = (cw - 32) - 2 * bw1;
    int bx0 = 16;
    int bx1 = bx0 + bw1 + 6;
    int bx2 = bx1 + bw2 + 6;

    c.fill_card(bx0, 118, bw1, 24, COL_BG_PANEL, COL_BORDER);
    c.draw_text_centered(bx0 + bw1 / 2, 126, "< PREV", COL_TEXT, 1);

    c.fill_card(bx1, 118, bw2, 24, g_s3_playing ? COL_MINT_DARK : COL_GOLD_DARK,
                g_s3_playing ? COL_MINT : COL_GOLD);
    c.draw_text_centered(bx1 + bw2 / 2, 126, g_s3_playing ? "LIVE // ES8311+BT" : "PAUSED // TAP",
                         g_s3_playing ? COL_MINT : COL_GOLD, 1);

    c.fill_card(bx2, 118, bw1, 24, COL_BG_PANEL, COL_CYAN);
    c.draw_text_centered(bx2 + bw1 / 2, 126, "NEXT >", COL_CYAN, 1);

    // 8-Band Ternary Equalizer Card
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

    // Scrollable Favourite Crate Window (tap any row to jump)
    int crate_y = 264;
    int crate_h = (H - 56) - crate_y;
    int rows    = (crate_h - 24) / 22;
    if (rows > 5) rows = 5;
    if (rows < 4) rows = 4;

    c.fill_card(8, crate_y, cw, crate_h, COL_BG_ELEV, COL_BORDER);
    c.draw_text(18, crate_y + 7, "FAVOURITE MUSICS CRATE (TAP ROW TO PLAY)", COL_GOLD, 1);

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

    // 13-Channel Spectrum Analyzer Card
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

    // AxiomQuant Universal Sine Kernel K(u) = sin(pi*u)/(pi*u)
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

    // Ternary Seam Matrix & Controls Card
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

    // Render 3x28 ternary matrix strip
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

    // Touch controls for channel / hopping / capsule export
    int btn_y = mat_y + mat_h - 32;
    int bw    = (cw - 30) / 4;
    c.fill_card(16, btn_y, bw, 24, COL_BG_PANEL, COL_BORDER);
    c.draw_text_centered(16 + bw / 2, btn_y + 8, "CH -", COL_TEXT, 1);

    c.fill_card(22 + bw, btn_y, bw + 12, 24, g_s3_hopping ? COL_MINT_DARK : COL_BG_PANEL, COL_MINT);
    c.draw_text_centered(22 + bw + (bw + 12) / 2, btn_y + 8, g_s3_hopping ? "HOP:ON" : "HOP:OFF", COL_MINT, 1);

    c.fill_card(40 + 2 * bw, btn_y, bw - 6, 24, COL_BG_PANEL, COL_BORDER);
    c.draw_text_centered(40 + 2 * bw + (bw - 6) / 2, btn_y + 8, "CH +", COL_TEXT, 1);

    c.fill_card(40 + 3 * bw, btn_y, bw, 24, COL_VIOLET_DARK, COL_VIOLET);
    c.draw_text_centered(40 + 3 * bw + bw / 2, btn_y + 8, "SYNC", COL_VIOLET, 1);
}

// ============================================================
// TAB 3: ULTRAWHALE TERNARY NAND OSCILLATOR x QMI8658 IMU
// ============================================================
static void s3_render_tab_nand(S3Canvas& c) {
    int W  = (int)g_lcd_w;
    int H  = (int)g_lcd_h;
    int cw = W - 16;

    // 7-Stage Ternary NAND Ring Card
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
    c.draw_text(18, 142, "COUPLED: 802.11 RF + QMI8658 6-AXIS IMU + 2D WAVE POOL", COL_DIM, 1);

    // 2D Lissajous Phase Portrait Card (Tilted by IMU)
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

    // QMI8658 IMU & QuantTern Affect Card
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
// TAB 4: QWAVE / SPHERED WITNESS & HARDWARE IDENTITY
// ============================================================
static void s3_render_tab_qwave(S3Canvas& c) {
    int W  = (int)g_lcd_w;
    int H  = (int)g_lcd_h;
    int cw = W - 16;

    // sphered 5-Stage Witness Pipeline Card
    c.fill_card(8, 46, cw, 118, COL_BG_ELEV, COL_VIOLET);
    c.draw_text(18, 53, "QWAVE // SPHERED #COMMIT=#VALIDWITNESS", COL_VIOLET, 1);

    const char* glyphs[5] = {"? ADMIT", "~ TRANS", "! VERIFY", "@ ATTR", "> COMMIT"};
    uint16_t gcols[5]     = {COL_CYAN, COL_GOLD, COL_MINT, COL_VIOLET, COL_MINT};
    int s_step = (cw - 16) / 5;
    int s_bw   = s_step - 4;
    for (int s = 0; s < 5; s++) {
        int sx = 14 + s * s_step;
        c.fill_card(sx, 68, s_bw, 36, COL_BG_PANEL, gcols[s]);
        c.draw_text_centered(sx + s_bw / 2, 78, glyphs[s], gcols[s], 1);
        c.draw_text_centered(sx + s_bw / 2, 91, "PASS +1", COL_TEXT, 1);
    }

    char wit_buf[64];
    snprintf(wit_buf, sizeof(wit_buf), "WITNESS #%lu <(? admit ~ transform ! verify > commit)>",
             (unsigned long)g_s3_witness_seq);
    wit_buf[44] = '\0';
    c.draw_text(18, 112, wit_buf, COL_TEXT, 1);
    c.draw_text(18, 127, "8b-is/qwave SovereignCore + QuantTern + baye-rust 2D Wave", COL_MINT, 1);
    char rtc_buf[64];
    snprintf(rtc_buf, sizeof(rtc_buf), "RTC %02u:%02u:%02u | TOUCH (%u,%u) | ES8311:%s",
             g_imu.rtc_h, g_imu.rtc_m, g_imu.rtc_s, g_touch.x, g_touch.y,
             g_es8311_ok ? "OK" : "IDLE");
    c.draw_text(18, 142, rtc_buf, COL_SUBTEXT, 1);

    // Hardware Identity Card
    c.fill_card(8, 170, cw, 104, COL_BG_ELEV, COL_BORDER);
    c.draw_text(18, 177, g_board_is_18
                ? "WAVESHARE ESP32-S3-TOUCH-AMOLED-1.8 (ADAYE)"
                : "WAVESHARE ESP32-S3-TOUCH-AMOLED-2.06 (VAKED)", COL_GOLD, 1);

    char mac_line[64];
    snprintf(mac_line, sizeof(mac_line), "MAC: %02X:%02X:%02X:%02X:%02X:%02X  LX7 @ 240MHz",
             g_s3_mac[0], g_s3_mac[1], g_s3_mac[2], g_s3_mac[3], g_s3_mac[4], g_s3_mac[5]);
    c.draw_text(18, 193, mac_line, COL_TEXT, 1);

    char disp_line[64];
    snprintf(disp_line, sizeof(disp_line), "SH8601 QSPI %ux%u (x_gap=%u) TCA9554:%s",
             g_lcd_w, g_lcd_h, g_lcd_x_gap, g_board_is_18 ? "0x20" : "N/A");
    c.draw_text(18, 208, disp_line, COL_SUBTEXT, 1);
    c.draw_text(18, 223, "I2C(15/14): AXP2101(34) FT3168(38) QMI8658(6B) ES8311(18)", COL_SUBTEXT, 1);

    char heap_line[64];
    snprintf(heap_line, sizeof(heap_line), "SRAM: %uKB  PSRAM: %uKB  SPLASHES: %lu",
             (unsigned int)(ESP.getFreeHeap() / 1024),
             (unsigned int)(ESP.getFreePsram() / 1024),
             (unsigned long)g_wp_splashes);
    c.draw_text(18, 238, heap_line, COL_MINT, 1);
    c.draw_text(18, 253, g_board_is_18
                ? "PORT: /dev/cu.usbmodem1101 <-> PEER: 2101"
                : "PORT: /dev/cu.usbmodem2101 <-> PEER: 1101", COL_CYAN, 1);

    // Flash Slot & Backup Status Card
    int slot_y = 280;
    int slot_h = (H - 56) - slot_y;
    c.fill_card(8, slot_y, cw, slot_h, COL_BG_ELEV, COL_MINT);
    c.draw_text(18, slot_y + 7, "SOVEREIGN FLASH & BACKUP INTEGRITY", COL_MINT, 1);
    c.draw_text(18, slot_y + 22, "ACTIVE : FLUG-OS v0.5.0+ultra-omni-zen", COL_TEXT, 1);
    c.draw_text(18, slot_y + 37, g_board_is_18
                ? "BACKUP : /tmp/amoled18_adaye_full_backup.bin (1.22MB)"
                : "FACTORY: 0x0100000 ESP-Brookesia Preserved", COL_SUBTEXT, 1);

    int btn_y = slot_y + slot_h - 36;
    int bw    = (cw - 28) / 2;
    c.fill_card(16, btn_y, bw, 28, COL_MINT_DARK, COL_MINT);
    c.draw_text_centered(16 + bw / 2, btn_y + 10, "OMNI-ZEN ACTIVE", COL_MINT, 1);

    c.fill_card(24 + bw, btn_y, bw, 28, COL_BG_PANEL, COL_CYAN);
    c.draw_text_centered(24 + bw + bw / 2, btn_y + 10,
                         g_board_is_18 ? "PING PEER 2.06" : "BOOT FACTORY", COL_CYAN, 1);
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
            case 0: s3_render_tab_zen(canvas);   break;
            case 1: s3_render_tab_radio(canvas); break;
            case 2: s3_render_tab_wifi(canvas);  break;
            case 3: s3_render_tab_nand(canvas);  break;
            case 4: s3_render_tab_qwave(canvas); break;
        }
        if (y0 + bh > footer_y) {
            s3_render_footer_tabs(canvas);
        }
        // Visual touch feedback ring
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
    Serial.printf("{\"event\":\"zoneinn_track\",\"board\":\"%s\",\"playing\":%d,\"idx\":%d,"
                  "\"slug\":\"%s\",\"title\":\"%s\",\"artist\":\"%s\","
                  "\"bpm\":%u,\"key\":\"%s\",\"qt\":\"%02x\"}\n",
                  g_board_is_18 ? "ESP32-S3-AMOLED-1.8" : "ESP32-S3-AMOLED-2.06",
                  g_s3_playing ? 1 : 0, g_s3_track_idx + 1,
                  t->slug, t->title, t->artist, t->bpm, t->key_sig, g_imu.qt_byte);
    s3_espnow_broadcast(32, 32, 4200);
}

static void s3_handle_touch() {
    int W = (int)g_lcd_w;
    int H = (int)g_lcd_h;
    int footer_y = H - 52;

    // Continuous touch drag inside Tab 0 (ZEN) injects live ripples into 2D Wave Pool
    if (g_touch.touched && g_s3_active_tab == 0 && g_touch.y > 44 && g_touch.y < footer_y - 38) {
        int wx = ((int)g_touch.x * (WAVE_POOL_N - 4)) / W + 2;
        int wy = (((int)g_touch.y - 44) * (WAVE_POOL_N - 4)) / (footer_y - 82) + 2;
        s3_wave_pool_splash(wx, wy, 5400, 5);
        if (g_touch.just_pressed) {
            s3_espnow_broadcast((uint8_t)wx, (uint8_t)wy, 5400);
        }
    }

    if (!g_touch.just_pressed) return;
    uint16_t tx = g_touch.x;
    uint16_t ty = g_touch.y;

    // Bottom Navigation Tab Bar
    if (ty >= footer_y) {
        uint8_t new_tab = (uint8_t)((tx * S3_TAB_COUNT) / W);
        if (new_tab >= S3_TAB_COUNT) new_tab = S3_TAB_COUNT - 1;
        g_s3_active_tab = new_tab;
        Serial.printf("{\"event\":\"tab\",\"val\":%u}\n", g_s3_active_tab);
        return;
    }

    // Tab-specific touch zones
    if (g_s3_active_tab == 1) {
        // Transport buttons (y in 114..148)
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
        // Crate list rows (y >= 284)
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
                s3_espnow_broadcast(32, 32, 6000);
                Serial.printf("{\"capsule_id\":\"omni-zen-s3-%lu\",\"rows\":%u,\"qt\":\"%02x\",\"ssid\":\"%s\"}\n",
                              (unsigned long)g_s3_total_pkts, g_s3_seam_mat.rows,
                              g_imu.qt_byte, g_s3_last_ssid);
            }
        }
    } else if (g_s3_active_tab == 4) {
        if (ty >= footer_y - 48 && ty < footer_y && tx >= W / 2) {
            if (g_board_is_18) {
                s3_espnow_broadcast(32, 32, 7200);
                s3_wave_pool_splash(32, 32, 7200, 6);
            } else {
                s3_boot_partition("factory");
            }
        }
    }
}

// ============================================================
// Serial Command Handler (compatible with s3_amoled_bridge.py)
// ============================================================
static void s3_handle_serial_command(const char* line) {
    if (strcmp(line, "status") == 0 || strcmp(line, "stats") == 0) {
        const S3FavTrack* t = &S3_FAVOURITES[g_s3_track_idx];
        Serial.printf("{\"board\":\"%s\",\"persona\":\"%s\",\"lcd\":\"%ux%u\","
                      "\"version\":\"" FLUGOS_FULL_VERSION "\","
                      "\"tab\":%u,\"pkts\":%lu,\"mgmt\":%lu,\"ctrl\":%lu,\"data\":%lu,"
                      "\"ch\":%d,\"hop\":%d,\"rssi\":%d,\"ssid\":\"%s\","
                      "\"track_idx\":%d,\"track\":\"%s\",\"artist\":\"%s\",\"bpm\":%u,"
                      "\"qt\":\"%02x\",\"ax\":%.2f,\"ay\":%.2f,\"az\":%.2f,\"sickness\":%.2f,"
                      "\"splashes\":%lu,\"peer_rx\":%lu,\"es8311\":%d,"
                      "\"batt_mv\":%u,\"batt_pct\":%u}\n",
                      g_board_is_18 ? "ESP32-S3-Touch-AMOLED-1.8" : "ESP32-S3-Touch-AMOLED-2.06",
                      g_board_is_18 ? "adaye" : "vaked",
                      g_lcd_w, g_lcd_h,
                      g_s3_active_tab, (unsigned long)g_s3_total_pkts,
                      (unsigned long)g_s3_mgmt_pkts, (unsigned long)g_s3_ctrl_pkts, (unsigned long)g_s3_data_pkts,
                      g_s3_channel, g_s3_hopping ? 1 : 0, (int)g_s3_last_rssi, g_s3_last_ssid,
                      g_s3_track_idx + 1, t->title, t->artist, t->bpm,
                      g_imu.qt_byte, g_imu.ax, g_imu.ay, g_imu.az, g_imu.sickness,
                      (unsigned long)g_wp_splashes, (unsigned long)g_s3_peer_rx_cnt,
                      g_es8311_ok ? 1 : 0,
                      g_power.batt_mv, g_power.batt_pct);
    } else if (strncmp(line, "tab ", 4) == 0) {
        int tab = atoi(line + 4);
        if (tab >= 0 && tab < S3_TAB_COUNT) g_s3_active_tab = (uint8_t)tab;
        Serial.printf("{\"cmd\":\"tab\",\"val\":%u}\n", g_s3_active_tab);
    } else if (strncmp(line, "splash", 6) == 0) {
        s3_wave_pool_splash(32, 32, 7500, 6);
        s3_espnow_broadcast(32, 32, 7500);
        Serial.printf("{\"cmd\":\"splash\",\"splashes\":%lu}\n", (unsigned long)g_wp_splashes);
    } else if (strncmp(line, "track ", 6) == 0) {
        int idx = atoi(line + 6) - 1;
        if (idx >= 0 && idx < S3_FAV_COUNT) {
            g_s3_track_idx = idx;
            g_s3_playing = true;
            s3_emit_track_event();
        }
    } else if (strcmp(line, "next") == 0) {
        g_s3_track_idx = (g_s3_track_idx + 1) % S3_FAV_COUNT;
        s3_emit_track_event();
    } else if (strcmp(line, "prev") == 0) {
        g_s3_track_idx = (g_s3_track_idx + S3_FAV_COUNT - 1) % S3_FAV_COUNT;
        s3_emit_track_event();
    } else if (strncmp(line, "ch ", 3) == 0) {
        int ch = atoi(line + 3);
        if (ch >= 1 && ch <= S3_MAX_CHANNEL) {
            g_s3_channel = ch;
            g_s3_hopping = false;
            esp_wifi_set_channel(g_s3_channel, WIFI_SECOND_CHAN_NONE);
            Serial.printf("{\"cmd\":\"ch\",\"val\":%d}\n", g_s3_channel);
        }
    } else if (strcmp(line, "hop") == 0) {
        g_s3_hopping = !g_s3_hopping;
        Serial.printf("{\"cmd\":\"hop\",\"en\":%d}\n", g_s3_hopping ? 1 : 0);
    } else if (strncmp(line, "mode ", 5) == 0) {
        set_wave_mode(line + 5);
        Serial.printf("{\"cmd\":\"mode\",\"val\":\"%s\"}\n", line + 5);
    } else if (strncmp(line, "boot ", 5) == 0) {
        s3_boot_partition(line + 5);
    } else if (strncmp(line, "bright ", 7) == 0) {
        int b = atoi(line + 7);
        if (b >= 10 && b <= 255) s3_amoled_set_brightness((uint8_t)b);
        Serial.printf("{\"cmd\":\"bright\",\"val\":%d}\n", b);
    }
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

    // 1. Initialize shared I2C bus (SDA=15, SCL=14 @ 400kHz)
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, 400000);

    // 2. Auto-detect 1.8" (TCA9554 @ 0x20) vs 2.06" and sequence rail resets
    bool is_18 = s3_tca9554_init_and_reset();

    // 3. Initialize AXP2101 PMIC & ES8311 Audio Codec
    bool pmic_ok = s3_axp2101_init();
    delay(20);
    bool audio_ok = s3_es8311_init();

    // 4. Initialize SH8601 QSPI AMOLED display (368x448 or 410x502)
    bool lcd_ok = s3_amoled_init();

    // 5. Initialize FT3168 capacitive touch & QMI8658 6-axis IMU
    s3_touch_init();
    bool imu_ok = s3_imu_init();
    s3_axp2101_poll();
    s3_imu_poll();

    // 6. Initialize ESP32-S3 Wi-Fi in Promiscuous + ESP-NOW Peer Mode
    WiFi.mode(WIFI_MODE_STA);
    WiFi.disconnect();
    esp_wifi_get_mac(WIFI_IF_STA, g_s3_mac);

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
    esp_wifi_set_channel(g_s3_channel, WIFI_SECOND_CHAN_NONE);

    // 7. Initial central splash + first frame
    s3_wave_pool_splash(32, 32, 8200, 6);
    s3_step_physics();
    s3_render_frame();

    Serial.printf("\r\n");
    Serial.printf("  ╔══════════════════════════════════════════════════════════╗\r\n");
    Serial.printf("  ║  FLUG-OS {-1, 0, +1}  " FLUGOS_FULL_VERSION "  ULTRA-OMNI-ZEN   ║\r\n");
    Serial.printf("  ║  Board: %-28s (%ux%u QSPI) ║\r\n",
                  is_18 ? "ESP32-S3-Touch-AMOLED-1.8" : "ESP32-S3-Touch-AMOLED-2.06",
                  g_lcd_w, g_lcd_h);
    Serial.printf("  ║  vaked.dev x 8b-is :: adaye + zoneInn + qwave + NAND     ║\r\n");
    Serial.printf("  ╚══════════════════════════════════════════════════════════╝\r\n");
    Serial.printf("{\"boot\":\"ok\",\"board\":\"%s\",\"persona\":\"%s\",\"lcd\":\"%ux%u\","
                  "\"pmic\":%d,\"amoled\":%d,\"imu\":%d,\"es8311\":%d,\"espnow\":%d,"
                  "\"mac\":\"%02x:%02x:%02x:%02x:%02x:%02x\"}\r\n",
                  is_18 ? "ESP32-S3-Touch-AMOLED-1.8" : "ESP32-S3-Touch-AMOLED-2.06",
                  is_18 ? "adaye" : "vaked",
                  g_lcd_w, g_lcd_h,
                  pmic_ok ? 1 : 0, lcd_ok ? 1 : 0, imu_ok ? 1 : 0,
                  audio_ok ? 1 : 0, g_s3_espnow_ok ? 1 : 0,
                  g_s3_mac[0], g_s3_mac[1], g_s3_mac[2],
                  g_s3_mac[3], g_s3_mac[4], g_s3_mac[5]);
}

static void s3_amoled_loop() {
    static unsigned long last_frame_ms = 0;
    static unsigned long last_hop_ms   = 0;
    static unsigned long last_pmic_ms  = 0;
    static unsigned long last_wave_ms  = 0;
    static unsigned long last_peer_ms  = 0;
    static bool          last_boot_btn = true;
    static char          cmd_buf[96];
    static size_t        cmd_pos = 0;

    unsigned long now = millis();

    // BOOT button (GPIO 0) cycles through the 5 Constellation tabs
    bool boot_btn = digitalRead(PIN_BOOT_BTN);
    if (!boot_btn && last_boot_btn) {
        g_s3_active_tab = (g_s3_active_tab + 1) % S3_TAB_COUNT;
        Serial.printf("{\"event\":\"tab\",\"val\":%u,\"src\":\"boot_btn\"}\n", g_s3_active_tab);
    }
    last_boot_btn = boot_btn;

    // Channel hopping across 1..13
    if (g_s3_hopping && (now - last_hop_ms >= 350)) {
        last_hop_ms = now;
        g_s3_channel = (g_s3_channel % S3_MAX_CHANNEL) + 1;
        esp_wifi_set_channel(g_s3_channel, WIFI_SECOND_CHAN_NONE);
    }

    // Periodic ESP-NOW peer heartbeat every 1.2s
    if (now - last_peer_ms >= 1200) {
        last_peer_ms = now;
        s3_espnow_broadcast(32, 32, 0);
    }

    // Poll AXP2101 battery every 2s
    if (now - last_pmic_ms >= 2000) {
        last_pmic_ms = now;
        s3_axp2101_poll();
    }

    // Stream wave output every 250ms for host bridge compatibility
    if (now - last_wave_ms >= 250) {
        last_wave_ms = now;
        output_wave(g_s3_last_rssi, g_s3_last_type, g_s3_last_sub, now);
    }

    // Poll touch, IMU, physics, and refresh AMOLED at ~25 FPS (40ms)
    if (now - last_frame_ms >= 40) {
        last_frame_ms = now;
        s3_touch_poll();
        s3_imu_poll();
        s3_handle_touch();
        s3_step_physics();
        s3_render_frame();
    }

    // Process Serial commands
    while (Serial.available()) {
        char ch = (char)Serial.read();
        if (ch == '\n' || ch == '\r') {
            if (cmd_pos > 0) {
                cmd_buf[cmd_pos] = '\0';
                s3_handle_serial_command(cmd_buf);
                cmd_pos = 0;
            }
        } else if (cmd_pos < sizeof(cmd_buf) - 1) {
            cmd_buf[cmd_pos++] = ch;
        }
    }
}

#endif /* FLUGOS_S3_AMOLED_APP_H */
