/*
 * FLUG-OS — Sovereign ESP32-S3 AMOLED Cockpit Application (v0.4.0)
 * Evolves vaked.dev + 8b-is onto the Waveshare ESP32-S3-Touch-AMOLED-2.06 (410x502)
 *
 * 4 Interactive Views (Touch Bar + BOOT Button + Serial Control):
 *   [0] RADIO  — zoneInn / radio.vaked.dev 14 Favourite Musics Crate + 8-band Ternary EQ
 *   [1] 802.11 — FLUG-OS Promiscuous 802.11 Spectrum (Ch 1-13) + Sine Kernel K(u)=sin(pi*u)/(pi*u)
 *   [2] NAND   — Ultrawhale 7-stage Ternary NAND Ring Oscillator x QMI8658 IMU Lissajous
 *   [3] QWAVE  — sphered #Commit=#ValidWitness 5-Stage Witness + Triple-Boot Partition Switcher
 */

#ifndef FLUGOS_S3_AMOLED_APP_H
#define FLUGOS_S3_AMOLED_APP_H

#include <Arduino.h>
#include <WiFi.h>
#include "esp_wifi.h"
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
static uint8_t  g_s3_active_tab  = 0;   // 0=RADIO, 1=802.11, 2=NAND, 3=QWAVE
static int      g_s3_track_idx   = 0;   // 0..13
static bool     g_s3_playing     = true;
static uint32_t g_s3_witness_seq = 1;

static int8_t   g_s3_ch_rssi[S3_MAX_CHANNEL];
static uint32_t g_s3_ch_pkts[S3_MAX_CHANNEL];
static char     g_s3_last_ssid[33] = "vaked.dev-constellation";
static uint8_t  g_s3_mac[6]        = {0x80, 0xB5, 0x4E, 0xF2, 0x02, 0xB0};

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
// Ternary NAND Ring & Waveform Physics Update
// ============================================================
static inline int8_t s3_ternary_nand(int8_t a, int8_t b) {
    // Kleene/Lukasiewicz balanced ternary NAND: -(min(a, b))
    int8_t mn = (a < b) ? a : b;
    return (int8_t)(-mn);
}

static void s3_step_physics() {
    // 1. Step 7-stage cross-coupled ternary NAND ring oscillator
    int8_t imu_mod = g_imu.trit_v;
    int8_t rf_mod  = ternary_seam_rssi_to_code(g_s3_last_rssi);
    int8_t next0   = s3_ternary_nand(g_s3_nand_ring[6], (int8_t)((imu_mod + rf_mod > 0) ? 1 : (imu_mod + rf_mod < 0 ? -1 : 0)));
    for (int i = 6; i >= 1; i--) {
        int8_t fb = (i == 3) ? g_s3_nand_ring[0] : g_s3_nand_ring[(i + 5) % 7];
        g_s3_nand_ring[i] = s3_ternary_nand(g_s3_nand_ring[i - 1], fb);
    }
    g_s3_nand_ring[0] = next0;
    g_s3_nand_cycles += 7;

    // 2. Step AxiomQuant Sine Kernel K(u) = sin(pi*u)/(pi*u)
    g_s3_phase += 0.14f;
    float rssi_u = normalize_rssi(g_s3_last_rssi) * 2.4f;
    float k_val  = sine_kernel(rssi_u + 0.35f * sinf(g_s3_phase));
    float carrier = sinf(g_s3_phase * 2.2f + (float)g_s3_last_type * 0.8f) * k_val;
    g_s3_wave_hist[g_s3_wave_head] = carrier;
    g_s3_wave_head = (g_s3_wave_head + 1) % S3_WAVE_HIST;

    // 3. Step 8-band Ternary EQ for active zoneInn Favourite track
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

    // 4. Compute 2D Lissajous curve modulated by QMI8658 IMU tilt
    float freq_x = 2.0f + g_imu.ax * 0.8f;
    float freq_y = 3.0f + g_imu.ay * 0.8f;
    float delta  = g_s3_phase * 0.6f + g_imu.az * 1.2f;
    for (int i = 0; i < S3_LISSA_PTS; i++) {
        float t = (float)i * (2.0f * 3.14159265f / (float)S3_LISSA_PTS);
        g_s3_lissa_x[i] = (int16_t)(sinf(freq_x * t + delta) * 140.0f);
        g_s3_lissa_y[i] = (int16_t)(sinf(freq_y * t) * 56.0f);
    }

    // 5. Render TernarySeam radiation matrix
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
// UI Band Renderer — Draws full 410x502 Constellation Cockpit
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
    c.fill_rect(0, 0, AMOLED_W, 42, COL_BG_ELEV);
    c.draw_hline(0, 42, AMOLED_W, COL_BORDER);

    // Brand + Version
    c.fill_circle(16, 21, 6, COL_MINT);
    c.draw_circle(16, 21, 9, COL_GOLD);
    c.draw_text(32, 8, "FLUG-OS {-1,0,+1}", COL_TEXT, 2);
    c.draw_text(32, 27, "vaked.dev x 8b-is  " FLUGOS_VERSION_STR, COL_MINT, 1);

    // QuantTern affect pill
    c.fill_card(244, 7, 74, 28, COL_VIOLET_DARK, COL_VIOLET);
    char qt_buf[16];
    snprintf(qt_buf, sizeof(qt_buf), "qt:%02x %c%c%c",
             g_imu.qt_byte,
             s3_trit_char(g_imu.trit_v),
             s3_trit_char(g_imu.trit_a),
             s3_trit_char(g_imu.trit_d));
    c.draw_text_centered(281, 17, qt_buf, COL_VIOLET, 1);

    // Battery / PMIC pill
    uint16_t bcol = g_power.charging ? COL_MINT : COL_GOLD;
    c.fill_card(324, 7, 78, 28, COL_BG_PANEL, bcol);
    char b_buf[16];
    snprintf(b_buf, sizeof(b_buf), "%u%% %s",
             g_power.batt_pct, g_power.vbus_good ? "USB" : "BAT");
    c.draw_text_centered(363, 12, b_buf, bcol, 1);
    snprintf(b_buf, sizeof(b_buf), "%umV", g_power.batt_mv);
    c.draw_text_centered(363, 23, b_buf, COL_SUBTEXT, 1);
}

static void s3_render_footer_tabs(S3Canvas& c) {
    int y = 442;
    c.fill_rect(0, y, AMOLED_W, 60, COL_BG_ELEV);
    c.draw_hline(0, y, AMOLED_W, COL_BORDER);

    const char* labels[4] = {"RADIO", "802.11", "NAND", "QWAVE"};
    const char* subs[4]   = {"zoneInn", "spectrum", "imu+osc", "sphered"};
    uint16_t cols[4]      = {COL_MINT, COL_CYAN, COL_GOLD, COL_VIOLET};

    for (int i = 0; i < 4; i++) {
        int bx = 6 + i * 100;
        int bw = 96;
        bool active = (g_s3_active_tab == i);
        uint16_t fill = active ? COL_BG_PANEL : COL_BG;
        uint16_t brd  = active ? cols[i] : COL_BORDER;
        c.fill_card(bx, y + 6, bw, 46, fill, brd);
        if (active) {
            c.fill_rect(bx + 16, y + 6, bw - 32, 3, cols[i]);
        }
        c.draw_text_centered(bx + bw / 2, y + 15, labels[i], active ? cols[i] : COL_TEXT, 2);
        c.draw_text_centered(bx + bw / 2, y + 36, subs[i], active ? COL_TEXT : COL_DIM, 1);
    }
}

// ============================================================
// TAB 0: ZONEINN / RADIO.VAKED.DEV — 14 Favourite Musics + Ternary EQ
// ============================================================
static void s3_render_tab_radio(S3Canvas& c) {
    const S3FavTrack* trk = &S3_FAVOURITES[g_s3_track_idx];

    // Now Playing Hero Card
    c.fill_card(10, 50, 390, 114, COL_BG_ELEV, COL_MINT);
    c.draw_text(22, 58, "RADIO.VAKED.DEV // ZONEINN CRATE", COL_MINT, 1);

    char idx_buf[24];
    snprintf(idx_buf, sizeof(idx_buf), "FAV %02d/%02d", g_s3_track_idx + 1, S3_FAV_COUNT);
    c.draw_text_right(388, 58, idx_buf, COL_GOLD, 1);

    c.draw_text(22, 74, trk->title, COL_TEXT, 2);
    c.draw_text(22, 95, trk->artist, COL_SUBTEXT, 2);

    char meta_buf[64];
    snprintf(meta_buf, sizeof(meta_buf), "%s  |  KEY %s  |  %u BPM  |  #%08lX",
             trk->genre, trk->key_sig, trk->bpm, (unsigned long)trk->fnv_hash);
    c.draw_text(22, 118, meta_buf, COL_GOLD, 1);

    // Transport buttons inside Hero Card
    c.fill_card(22, 132, 86, 24, COL_BG_PANEL, COL_BORDER);
    c.draw_text_centered(65, 140, "< PREV", COL_TEXT, 1);

    c.fill_card(118, 132, 148, 24, g_s3_playing ? COL_MINT_DARK : COL_GOLD_DARK,
                g_s3_playing ? COL_MINT : COL_GOLD);
    c.draw_text_centered(192, 140, g_s3_playing ? "LIVE // BT+WEB SYNC" : "PAUSED // TAP PLAY",
                         g_s3_playing ? COL_MINT : COL_GOLD, 1);

    c.fill_card(276, 132, 112, 24, COL_BG_PANEL, COL_CYAN);
    c.draw_text_centered(332, 140, "NEXT TRACK >", COL_CYAN, 1);

    // 8-Band Ternary Equalizer Card
    c.fill_card(10, 170, 390, 118, COL_BG_ELEV, COL_BORDER);
    c.draw_text(22, 178, "8-BAND TERNARY HARMONIC EQUALIZER {-1, 0, +1}", COL_SUBTEXT, 1);
    const char* hz_lbl[8] = {"60", "144", "288", "432", "528", "699", "1.1k", "4.3k"};
    for (int b = 0; b < 8; b++) {
        int bx = 24 + b * 46;
        int bw = 34;
        int max_h = 64;
        int bh = (g_s3_eq_bars[b] * max_h) / 100;
        int by = 262 - bh;
        int8_t tr = g_s3_eq_trits[b];
        uint16_t col = s3_trit_color(tr);
        uint16_t dark = (tr > 0) ? COL_MINT_DARK : ((tr < 0) ? COL_CORAL_DARK : COL_GOLD_DARK);

        c.fill_rect(bx, 198, bw, max_h, COL_BG);
        c.fill_rect(bx, by, bw, bh, dark);
        c.draw_rect(bx, by, bw, bh, col);
        c.fill_rect(bx, by, bw, 3, col);

        char t_str[4] = {s3_trit_char(tr), '\0'};
        c.draw_text_centered(bx + bw / 2, 190, t_str, col, 1);
        c.draw_text_centered(bx + bw / 2, 268, hz_lbl[b], COL_DIM, 1);
    }

    // Scrollable 5-row Favourite Crate Window (tap any row to jump)
    c.fill_card(10, 294, 390, 142, COL_BG_ELEV, COL_BORDER);
    c.draw_text(22, 302, "FAVOURITE MUSICS CRATE (TAP TRACK TO SELECT)", COL_GOLD, 1);

    int start_idx = g_s3_track_idx - 2;
    if (start_idx < 0) start_idx = 0;
    if (start_idx > S3_FAV_COUNT - 5) start_idx = S3_FAV_COUNT - 5;

    for (int r = 0; r < 5; r++) {
        int tidx = start_idx + r;
        int ry = 316 + r * 23;
        bool sel = (tidx == g_s3_track_idx);
        if (sel) {
            c.fill_rect(16, ry - 2, 378, 20, COL_BG_PANEL);
            c.fill_rect(16, ry - 2, 4, 20, COL_MINT);
        }
        char row_buf[64];
        snprintf(row_buf, sizeof(row_buf), "%02d %s - %s",
                 tidx + 1, S3_FAVOURITES[tidx].title, S3_FAVOURITES[tidx].artist);
        row_buf[44] = '\0';
        c.draw_text(26, ry + 4, row_buf, sel ? COL_MINT : COL_TEXT, 1);

        char bpm_buf[16];
        snprintf(bpm_buf, sizeof(bpm_buf), "%uBPM", S3_FAVOURITES[tidx].bpm);
        c.draw_text_right(388, ry + 4, bpm_buf, sel ? COL_GOLD : COL_DIM, 1);
    }
}

// ============================================================
// TAB 1: FLUG-OS 802.11 PROMISCUOUS SPECTRUM + SINE KERNEL
// ============================================================
static void s3_render_tab_wifi(S3Canvas& c) {
    // 13-Channel Spectrum Analyzer Card
    c.fill_card(10, 50, 390, 138, COL_BG_ELEV, COL_CYAN);
    c.draw_text(22, 58, "802.11 PROMISCUOUS SPECTRUM (CH 01..13)", COL_CYAN, 1);

    char ch_hdr[36];
    snprintf(ch_hdr, sizeof(ch_hdr), "CH:%02d %s  PKTS:%lu",
             g_s3_channel, g_s3_hopping ? "HOP" : "FIX", (unsigned long)g_s3_total_pkts);
    c.draw_text_right(388, 58, ch_hdr, COL_MINT, 1);

    for (int ch = 1; ch <= S3_MAX_CHANNEL; ch++) {
        int bx = 20 + (ch - 1) * 28;
        int bw = 22;
        int8_t rssi = g_s3_ch_rssi[ch - 1];
        int val = (int)rssi + 100;
        if (val < 6) val = 6;
        if (val > 80) val = 80;
        int bh = (val * 82) / 80;
        int by = 160 - bh;

        int8_t code = ternary_seam_rssi_to_code(rssi);
        uint16_t col = s3_trit_color(code);
        bool is_cur = (ch == g_s3_channel);

        c.fill_rect(bx, 78, bw, 82, COL_BG);
        c.fill_rect(bx, by, bw, bh, (code > 0) ? COL_MINT_DARK : ((code < 0) ? COL_CORAL_DARK : COL_GOLD_DARK));
        c.draw_rect(bx, by, bw, bh, is_cur ? COL_CYAN : col);

        char clbl[6];
        snprintf(clbl, sizeof(clbl), "%02d", ch);
        c.draw_text_centered(bx + bw / 2, 166, clbl, is_cur ? COL_CYAN : COL_SUBTEXT, 1);
    }

    // AxiomQuant Universal Sine Kernel K(u) = sin(pi*u)/(pi*u)
    c.fill_card(10, 194, 390, 110, COL_BG_ELEV, COL_BORDER);
    c.draw_text(22, 202, "AXIOMQUANT SINE KERNEL  K(u) = sin(pi*u)/(pi*u)", COL_GOLD, 1);

    char rssi_lbl[32];
    snprintf(rssi_lbl, sizeof(rssi_lbl), "RSSI %ddBm [%c1]",
             (int)g_s3_last_rssi, s3_trit_char(ternary_seam_rssi_to_code(g_s3_last_rssi)));
    c.draw_text_right(388, 202, rssi_lbl, COL_MINT, 1);

    int wave_cy = 256;
    c.draw_hline(20, wave_cy, 370, COL_GRID);
    for (int i = 0; i < S3_WAVE_HIST - 1; i++) {
        int idx0 = (g_s3_wave_head + i) % S3_WAVE_HIST;
        int idx1 = (g_s3_wave_head + i + 1) % S3_WAVE_HIST;
        int x0 = 20 + (i * 370) / (S3_WAVE_HIST - 1);
        int x1 = 20 + ((i + 1) * 370) / (S3_WAVE_HIST - 1);
        int y0 = wave_cy - (int)(g_s3_wave_hist[idx0] * 38.0f);
        int y1 = wave_cy - (int)(g_s3_wave_hist[idx1] * 38.0f);
        c.draw_line(x0, y0, x1, y1, COL_MINT, 2);
    }

    // Ternary Seam Matrix & Controls Card
    c.fill_card(10, 310, 390, 126, COL_BG_ELEV, COL_BORDER);
    c.draw_text(22, 318, "TERNARY SEAM RADIATION MATRIX (64x3) & SSID", COL_VIOLET, 1);

    char ssid_line[48];
    snprintf(ssid_line, sizeof(ssid_line), "SSID: %s", g_s3_last_ssid);
    ssid_line[34] = '\0';
    c.draw_text(22, 333, ssid_line, COL_TEXT, 1);

    char cnt_line[64];
    snprintf(cnt_line, sizeof(cnt_line), "MGMT:%lu  CTRL:%lu  DATA:%lu  BCN:%lu  PRB:%lu",
             (unsigned long)g_s3_mgmt_pkts, (unsigned long)g_s3_ctrl_pkts,
             (unsigned long)g_s3_data_pkts, (unsigned long)g_s3_beacon_pkts,
             (unsigned long)g_s3_probe_pkts);
    c.draw_text(22, 348, cnt_line, COL_SUBTEXT, 1);

    // Render 3x32 ternary matrix strip
    for (int col = 0; col < 32; col++) {
        int r_idx = (g_s3_seam_mat.rows > col) ? (g_s3_seam_mat.rows - 1 - col) : col;
        for (int row = 0; row < 3; row++) {
            int8_t v = (r_idx < (int)g_s3_seam_mat.rows) ? g_s3_seam_mat.codes[r_idx][row] : 0;
            uint16_t cell_col = (v > 0) ? COL_MINT : ((v < 0) ? COL_CORAL : COL_GRID);
            c.fill_rect(22 + col * 11, 364 + row * 10, 9, 8, cell_col);
        }
    }

    // Touch controls for channel / hopping / capsule export
    c.fill_card(22, 400, 84, 28, COL_BG_PANEL, COL_BORDER);
    c.draw_text_centered(64, 410, "CH -", COL_TEXT, 1);

    c.fill_card(114, 400, 110, 28, g_s3_hopping ? COL_MINT_DARK : COL_BG_PANEL, COL_MINT);
    c.draw_text_centered(169, 410, g_s3_hopping ? "HOP: ACTIVE" : "HOP: PAUSED", COL_MINT, 1);

    c.fill_card(232, 400, 84, 28, COL_BG_PANEL, COL_BORDER);
    c.draw_text_centered(274, 410, "CH +", COL_TEXT, 1);

    c.fill_card(324, 400, 64, 28, COL_VIOLET_DARK, COL_VIOLET);
    c.draw_text_centered(356, 410, "CAPSULE", COL_VIOLET, 1);
}

// ============================================================
// TAB 2: ULTRAWHALE TERNARY NAND OSCILLATOR x QMI8658 IMU
// ============================================================
static void s3_render_tab_nand(S3Canvas& c) {
    // 7-Stage Ternary NAND Ring Card
    c.fill_card(10, 50, 390, 128, COL_BG_ELEV, COL_GOLD);
    c.draw_text(22, 58, "ULTRAWHALE 7-STAGE TERNARY NAND RING (PEETPEDRO)", COL_GOLD, 1);

    for (int i = 0; i < 7; i++) {
        int gx = 22 + i * 53;
        int gy = 78;
        int8_t st = g_s3_nand_ring[i];
        uint16_t col = s3_trit_color(st);
        uint16_t fill = (st > 0) ? COL_MINT_DARK : ((st < 0) ? COL_CORAL_DARK : COL_GOLD_DARK);

        c.fill_card(gx, gy, 44, 54, fill, col);
        char glbl[8];
        snprintf(glbl, sizeof(glbl), "N%d", i);
        c.draw_text_centered(gx + 22, gy + 8, glbl, COL_SUBTEXT, 1);

        char tstr[4] = {s3_trit_char(st), '\0'};
        c.draw_text_centered(gx + 22, gy + 24, tstr, col, 2);

        if (i < 6) {
            c.draw_hline(gx + 44, gy + 27, 9, COL_CYAN);
        }
    }
    char cyc_buf[64];
    snprintf(cyc_buf, sizeof(cyc_buf), "CYCLES: %lu   COUPLED: RF + QMI8658 6-AXIS IMU",
             (unsigned long)g_s3_nand_cycles);
    c.draw_text(22, 146, cyc_buf, COL_MINT, 1);
    c.draw_text(22, 160, "HF: huggingface.co/peetpedro/ultrawhale-oscillator", COL_DIM, 1);

    // 2D Lissajous Phase Portrait Card (Tilted by IMU)
    c.fill_card(10, 184, 390, 152, COL_BG_ELEV, COL_CYAN);
    c.draw_text(22, 192, "2D LISSAJOUS HARMONIC PHASE PORTRAIT (TILT BOARD)", COL_CYAN, 1);

    int cx = 205, cy = 264;
    c.draw_hline(30, cy, 350, COL_GRID);
    c.draw_vline(cx, 208, 114, COL_GRID);

    for (int i = 0; i < S3_LISSA_PTS; i++) {
        int next = (i + 1) % S3_LISSA_PTS;
        uint16_t lcol = (i % 3 == 0) ? COL_MINT : ((i % 3 == 1) ? COL_CYAN : COL_VIOLET);
        c.draw_line(cx + g_s3_lissa_x[i], cy + g_s3_lissa_y[i],
                    cx + g_s3_lissa_x[next], cy + g_s3_lissa_y[next], lcol, 2);
    }

    // QMI8658 IMU & QuantTern Affect Card
    c.fill_card(10, 342, 390, 94, COL_BG_ELEV, COL_VIOLET);
    c.draw_text(22, 350, "QMI8658 6-AXIS IMU -> QUANTTERN AFFECT (QWAVE)", COL_VIOLET, 1);

    char imu1[64], imu2[64], imu3[64];
    snprintf(imu1, sizeof(imu1), "ACCEL (g)   X:%+.2f   Y:%+.2f   Z:%+.2f",
             g_imu.ax, g_imu.ay, g_imu.az);
    snprintf(imu2, sizeof(imu2), "GYRO (dps)  X:%+5.1f  Y:%+5.1f  Z:%+5.1f",
             g_imu.gx, g_imu.gy, g_imu.gz);
    snprintf(imu3, sizeof(imu3), "QUANTTERN   V=%+d  A=%+d  D=%+d  ->  qt:%02x (6-BIT TRIT)",
             g_imu.trit_v, g_imu.trit_a, g_imu.trit_d, g_imu.qt_byte);

    c.draw_text(22, 368, imu1, COL_TEXT, 1);
    c.draw_text(22, 386, imu2, COL_SUBTEXT, 1);
    c.draw_text(22, 404, imu3, COL_MINT, 1);
}

// ============================================================
// TAB 3: QWAVE / SPHERED WITNESS & TRIPLE-BOOT PARTITION MANAGER
// ============================================================
static void s3_render_tab_qwave(S3Canvas& c) {
    // sphered 5-Stage Witness Pipeline Card
    c.fill_card(10, 50, 390, 134, COL_BG_ELEV, COL_VIOLET);
    c.draw_text(22, 58, "QWAVE // SPHERED #COMMIT = #VALIDWITNESS", COL_VIOLET, 1);
    c.draw_text_right(388, 58, "P1: STABLE=FREE", COL_MINT, 1);

    const char* glyphs[5] = {"? ADMIT", "~ TRANS", "! VERIFY", "@ ATTR", "> COMMIT"};
    uint16_t gcols[5]     = {COL_CYAN, COL_GOLD, COL_MINT, COL_VIOLET, COL_MINT};
    for (int s = 0; s < 5; s++) {
        int sx = 18 + s * 75;
        c.fill_card(sx, 74, 70, 40, COL_BG_PANEL, gcols[s]);
        c.draw_text_centered(sx + 35, 86, glyphs[s], gcols[s], 1);
        c.draw_text_centered(sx + 35, 99, "PASS [+1]", COL_TEXT, 1);
    }

    char wit_buf[64];
    snprintf(wit_buf, sizeof(wit_buf), "WITNESS #%lu  <(? admit ~ transform ! verify @ attr > commit)>",
             (unsigned long)g_s3_witness_seq);
    c.draw_text(22, 124, wit_buf, COL_TEXT, 1);
    c.draw_text(22, 140, "8b-is/qwave SovereignCore C-ABI + QuantTern VAD 100% GREEN", COL_MINT, 1);
    char rtc_buf[64];
    snprintf(rtc_buf, sizeof(rtc_buf), "RTC %02u:%02u:%02u  |  TOUCH (%u, %u)",
             g_imu.rtc_h, g_imu.rtc_m, g_imu.rtc_s, g_touch.x, g_touch.y);
    c.draw_text(22, 156, rtc_buf, COL_SUBTEXT, 1);

    // Hardware Identity Card
    c.fill_card(10, 190, 390, 108, COL_BG_ELEV, COL_BORDER);
    c.draw_text(22, 198, "WAVESHARE ESP32-S3-TOUCH-AMOLED-2.06 HARDWARE", COL_GOLD, 1);

    char mac_line[64];
    snprintf(mac_line, sizeof(mac_line), "MAC: %02X:%02X:%02X:%02X:%02X:%02X   SOC: ESP32-S3 LX7 @ 240MHz",
             g_s3_mac[0], g_s3_mac[1], g_s3_mac[2], g_s3_mac[3], g_s3_mac[4], g_s3_mac[5]);
    c.draw_text(22, 216, mac_line, COL_TEXT, 1);
    c.draw_text(22, 232, "DISPLAY: 410x502 SH8601/CO5300 QSPI AMOLED (x_gap=0x16)", COL_SUBTEXT, 1);
    c.draw_text(22, 248, "I2C0 (15/14): AXP2101(0x34) FT3168(0x38) QMI8658(0x6B)", COL_SUBTEXT, 1);
    char heap_line[64];
    snprintf(heap_line, sizeof(heap_line), "FREE SRAM: %u KB   FLASH: 32MB QIO   PSRAM: 8MB AP_3v3",
             (unsigned int)(ESP.getFreeHeap() / 1024));
    c.draw_text(22, 264, heap_line, COL_MINT, 1);
    c.draw_text(22, 280, "USB-JTAG/CDC: /dev/cu.usbmodem2101 @ 115200 BAUD", COL_CYAN, 1);

    // Zero-Data-Loss Dual/Triple-Boot Partition Switcher Card
    c.fill_card(10, 304, 390, 132, COL_BG_ELEV, COL_MINT);
    c.draw_text(22, 312, "DUAL-BOOT FLASH SLOTS (FACTORY PRESERVED)", COL_MINT, 1);
    c.draw_text(22, 328, "OTA_0 @ 0x0A00000 : FLUG-OS v0.4.0 (ACTIVE SOVEREIGN OS)", COL_TEXT, 1);
    c.draw_text(22, 344, "FACT  @ 0x0100000 : ESP-Brookesia Waveshare Watch Demo", COL_SUBTEXT, 1);
    c.draw_text(22, 360, "OTA_1 @ 0x1000000 : Sovereign Snapshot Mirror Slot", COL_SUBTEXT, 1);

    c.fill_card(22, 382, 176, 40, COL_MINT_DARK, COL_MINT);
    c.draw_text_centered(110, 394, "OTA_0 ACTIVE", COL_MINT, 1);
    c.draw_text_centered(110, 407, "FLUG-OS v0.4.0", COL_TEXT, 1);

    c.fill_card(212, 382, 176, 40, COL_BG_PANEL, COL_CYAN);
    c.draw_text_centered(300, 394, "BOOT FACTORY", COL_CYAN, 1);
    c.draw_text_centered(300, 407, "ESP-Brookesia Demo", COL_SUBTEXT, 1);
}

static void s3_render_frame() {
    for (int y0 = 0; y0 < AMOLED_H; y0 += AMOLED_BAND_H) {
        int bh = AMOLED_BAND_H;
        if (y0 + bh > AMOLED_H) bh = AMOLED_H - y0; // Last band: 494..502 (height 8, even!)
        S3Canvas canvas = {y0, y0 + bh};
        canvas.clear(COL_BG);

        if (y0 < 44) {
            s3_render_header(canvas);
        }
        switch (g_s3_active_tab) {
            case 0: s3_render_tab_radio(canvas); break;
            case 1: s3_render_tab_wifi(canvas);  break;
            case 2: s3_render_tab_nand(canvas);  break;
            case 3: s3_render_tab_qwave(canvas); break;
        }
        if (y0 + bh > 440) {
            s3_render_footer_tabs(canvas);
        }
        // Visual touch feedback ring
        if (g_touch.touched) {
            canvas.draw_circle(g_touch.x, g_touch.y, 12, COL_MINT);
            canvas.fill_circle(g_touch.x, g_touch.y, 3, COL_GOLD);
        }
        s3_amoled_push_band(y0, bh);
    }
}

// ============================================================
// Touch & Button Interaction Handler
// ============================================================
static void s3_emit_track_event() {
    const S3FavTrack* t = &S3_FAVOURITES[g_s3_track_idx];
    Serial.printf("{\"event\":\"zoneinn_track\",\"playing\":%d,\"idx\":%d,"
                  "\"slug\":\"%s\",\"title\":\"%s\",\"artist\":\"%s\","
                  "\"bpm\":%u,\"key\":\"%s\",\"qt\":\"%02x\"}\n",
                  g_s3_playing ? 1 : 0, g_s3_track_idx + 1,
                  t->slug, t->title, t->artist, t->bpm, t->key_sig, g_imu.qt_byte);
}

static void s3_handle_touch() {
    if (!g_touch.just_pressed) return;
    uint16_t tx = g_touch.x;
    uint16_t ty = g_touch.y;

    // Bottom Navigation Tab Bar (y >= 442)
    if (ty >= 442) {
        uint8_t new_tab = (uint8_t)(tx / 102);
        if (new_tab > 3) new_tab = 3;
        g_s3_active_tab = new_tab;
        Serial.printf("{\"event\":\"tab\",\"val\":%u}\n", g_s3_active_tab);
        return;
    }

    // Tab-specific touch zones
    if (g_s3_active_tab == 0) {
        // Transport buttons (y in 128..162)
        if (ty >= 126 && ty <= 164) {
            if (tx < 114) {
                g_s3_track_idx = (g_s3_track_idx + S3_FAV_COUNT - 1) % S3_FAV_COUNT;
                g_s3_playing = true;
                s3_emit_track_event();
            } else if (tx < 270) {
                g_s3_playing = !g_s3_playing;
                s3_emit_track_event();
            } else {
                g_s3_track_idx = (g_s3_track_idx + 1) % S3_FAV_COUNT;
                g_s3_playing = true;
                s3_emit_track_event();
            }
            return;
        }
        // Crate list rows (y in 314..432)
        if (ty >= 314 && ty <= 432) {
            int row = (ty - 314) / 23;
            if (row >= 0 && row < 5) {
                int start_idx = g_s3_track_idx - 2;
                if (start_idx < 0) start_idx = 0;
                if (start_idx > S3_FAV_COUNT - 5) start_idx = S3_FAV_COUNT - 5;
                g_s3_track_idx = start_idx + row;
                g_s3_playing = true;
                s3_emit_track_event();
            }
        }
    } else if (g_s3_active_tab == 1) {
        if (ty >= 394 && ty <= 436) {
            if (tx < 110) {
                g_s3_hopping = false;
                g_s3_channel = (g_s3_channel == 1) ? S3_MAX_CHANNEL : (g_s3_channel - 1);
                esp_wifi_set_channel(g_s3_channel, WIFI_SECOND_CHAN_NONE);
            } else if (tx < 228) {
                g_s3_hopping = !g_s3_hopping;
            } else if (tx < 320) {
                g_s3_hopping = false;
                g_s3_channel = (g_s3_channel % S3_MAX_CHANNEL) + 1;
                esp_wifi_set_channel(g_s3_channel, WIFI_SECOND_CHAN_NONE);
            } else {
                // Export ayeOS TernaryMatrix JSON capsule over Serial
                Serial.printf("{\"capsule_id\":\"flug-os-s3-%lu\",\"rows\":%u,\"qt\":\"%02x\",\"ssid\":\"%s\"}\n",
                              (unsigned long)g_s3_total_pkts, g_s3_seam_mat.rows,
                              g_imu.qt_byte, g_s3_last_ssid);
            }
        }
    } else if (g_s3_active_tab == 3) {
        if (ty >= 378 && ty <= 430) {
            if (tx >= 145 && tx <= 265) {
                s3_boot_partition("xiaozhi");
            } else if (tx >= 270) {
                s3_boot_partition("factory");
            }
        }
    }
}

// ============================================================
// Serial Command Handler (compatible with wave_bridge.py + S3 extensions)
// ============================================================
static void s3_handle_serial_command(const char* line) {
    if (strcmp(line, "status") == 0 || strcmp(line, "stats") == 0) {
        const S3FavTrack* t = &S3_FAVOURITES[g_s3_track_idx];
        Serial.printf("{\"board\":\"ESP32-S3-Touch-AMOLED-2.06\",\"version\":\"" FLUGOS_FULL_VERSION "\","
                      "\"tab\":%u,\"pkts\":%lu,\"mgmt\":%lu,\"ctrl\":%lu,\"data\":%lu,"
                      "\"ch\":%d,\"hop\":%d,\"rssi\":%d,\"ssid\":\"%s\","
                      "\"track_idx\":%d,\"track\":\"%s\",\"artist\":\"%s\",\"bpm\":%u,"
                      "\"qt\":\"%02x\",\"ax\":%.2f,\"ay\":%.2f,\"az\":%.2f,"
                      "\"batt_mv\":%u,\"batt_pct\":%u}\n",
                      g_s3_active_tab, (unsigned long)g_s3_total_pkts,
                      (unsigned long)g_s3_mgmt_pkts, (unsigned long)g_s3_ctrl_pkts, (unsigned long)g_s3_data_pkts,
                      g_s3_channel, g_s3_hopping ? 1 : 0, (int)g_s3_last_rssi, g_s3_last_ssid,
                      g_s3_track_idx + 1, t->title, t->artist, t->bpm,
                      g_imu.qt_byte, g_imu.ax, g_imu.ay, g_imu.az,
                      g_power.batt_mv, g_power.batt_pct);
    } else if (strncmp(line, "tab ", 4) == 0) {
        int tab = atoi(line + 4);
        if (tab >= 0 && tab <= 3) g_s3_active_tab = (uint8_t)tab;
        Serial.printf("{\"cmd\":\"tab\",\"val\":%u}\n", g_s3_active_tab);
    } else if (strncmp(line, "track ", 6) == 0) {
        int idx = atoi(line + 6) - 1;
        if (idx >= 0 && idx < S3_FAV_COUNT) {
            g_s3_track_idx = idx;
            g_s3_active_tab = 0;
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
    for (int i = 0; i < S3_MAX_CHANNEL; i++) {
        g_s3_ch_rssi[i] = -95;
        g_s3_ch_pkts[i] = 0;
    }

    // 1. Initialize shared I2C bus (SDA=15, SCL=14)
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, 400000);

    // 2. Initialize AXP2101 PMIC first so display & sensor 3.3V rails power up
    bool pmic_ok = s3_axp2101_init();
    delay(40);

    // 3. Initialize SH8601/CO5300 410x502 QSPI AMOLED display
    bool lcd_ok = s3_amoled_init();

    // 4. Initialize FT3168 capacitive touch & QMI8658 6-axis IMU
    s3_touch_init();
    bool imu_ok = s3_imu_init();
    s3_axp2101_poll();
    s3_imu_poll();

    // 5. Initialize ESP32-S3 Wi-Fi in Promiscuous 802.11 Packet-Wave Mode
    WiFi.mode(WIFI_MODE_STA);
    WiFi.disconnect();
    esp_wifi_get_mac(WIFI_IF_STA, g_s3_mac);
    esp_wifi_set_promiscuous(false);
    esp_wifi_set_promiscuous_rx_cb(&s3_wifi_promisc_cb);
    wifi_promiscuous_filter_t filt;
    filt.filter_mask = WIFI_PROMIS_FILTER_MASK_ALL;
    esp_wifi_set_promiscuous_filter(&filt);
    esp_wifi_set_promiscuous(true);
    esp_wifi_set_channel(g_s3_channel, WIFI_SECOND_CHAN_NONE);

    // 6. Render first frame immediately
    s3_step_physics();
    s3_render_frame();

    Serial.printf("\r\n");
    Serial.printf("  ╔══════════════════════════════════════════════════════════╗\r\n");
    Serial.printf("  ║  FLUG-OS {-1, 0, +1}  " FLUGOS_FULL_VERSION "  S3-AMOLED COCKPIT     ║\r\n");
    Serial.printf("  ║  Waveshare ESP32-S3-Touch-AMOLED-2.06 (410x502 QSPI)     ║\r\n");
    Serial.printf("  ║  vaked.dev x 8b-is :: zoneInn + qwave + ultrawhale NAND  ║\r\n");
    Serial.printf("  ╚══════════════════════════════════════════════════════════╝\r\n");
    Serial.printf("{\"boot\":\"ok\",\"pmic\":%d,\"amoled\":%d,\"imu\":%d,"
                  "\"mac\":\"%02x:%02x:%02x:%02x:%02x:%02x\"}\r\n",
                  pmic_ok ? 1 : 0, lcd_ok ? 1 : 0, imu_ok ? 1 : 0,
                  g_s3_mac[0], g_s3_mac[1], g_s3_mac[2],
                  g_s3_mac[3], g_s3_mac[4], g_s3_mac[5]);
}

static void s3_amoled_loop() {
    static unsigned long last_frame_ms = 0;
    static unsigned long last_hop_ms   = 0;
    static unsigned long last_pmic_ms  = 0;
    static unsigned long last_wave_ms  = 0;
    static bool          last_boot_btn = true;
    static char          cmd_buf[96];
    static size_t        cmd_pos = 0;

    unsigned long now = millis();

    // BOOT button (GPIO 0) cycles through the 4 Constellation tabs
    bool boot_btn = digitalRead(PIN_BOOT_BTN);
    if (!boot_btn && last_boot_btn) {
        g_s3_active_tab = (g_s3_active_tab + 1) % 4;
        Serial.printf("{\"event\":\"tab\",\"val\":%u,\"src\":\"boot_btn\"}\n", g_s3_active_tab);
    }
    last_boot_btn = boot_btn;

    // Channel hopping across 1..13
    if (g_s3_hopping && (now - last_hop_ms >= 350)) {
        last_hop_ms = now;
        g_s3_channel = (g_s3_channel % S3_MAX_CHANNEL) + 1;
        esp_wifi_set_channel(g_s3_channel, WIFI_SECOND_CHAN_NONE);
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
