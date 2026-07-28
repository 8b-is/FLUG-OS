/*
 * FLUG-OS — ASCII Dashboard
 * Real-time spectrum visualization + interactive menu (pure ASCII).
 *
 * No ANSI codes. No escape sequences. Pure printable ASCII that
 * works on any terminal (screen, minicom, Putty, serial monitor).
 *
 * Commands (interactive menu):
 *   m       — show menu
 *   d       — dashboard (real-time spectrum + stats)
 *   s       — scan all channels (RSSI bar chart)
 *   w       — wave output toggle
 *   q       — quiet mode (packets only)
 *
 * Standard commands still work: ch, hop, mode, filter, stats, reset
 */

#ifndef FLUGOS_UI_H
#define FLUGOS_UI_H

#include <Arduino.h>

// ============================================================
// Channel RSSI history (for spectrum display)
// ============================================================
#define SPECTRUM_SAMPLES  20  // rolling samples per channel

typedef struct {
    int8_t rssi_samples[SPECTRUM_SAMPLES];  // rolling buffer
    uint8_t sample_idx;
    int8_t peak_rssi;        // highest seen on this channel
    uint32_t packet_count;
    uint32_t last_seen;      // ms of last packet
} ChannelStats;

static ChannelStats channel_stats[MAX_CHANNEL];
static bool dashboard_active = false;
static unsigned long last_dashboard_update = 0;

// ============================================================
// Initialize channel stats
// ============================================================
static void init_ui(void) {
    for (int i = 0; i < MAX_CHANNEL; i++) {
        ChannelStats* cs = &channel_stats[i];
        cs->peak_rssi = -120;
        cs->packet_count = 0;
        cs->sample_idx = 0;
        cs->last_seen = 0;
        for (int s = 0; s < SPECTRUM_SAMPLES; s++) {
            cs->rssi_samples[s] = -100;
        }
    }
}

// ============================================================
// Record a packet into channel stats
// ============================================================
static void record_packet(int channel, int8_t rssi) {
    if (channel < 1 || channel > MAX_CHANNEL) return;
    ChannelStats* cs = &channel_stats[channel - 1];

    cs->rssi_samples[cs->sample_idx % SPECTRUM_SAMPLES] = rssi;
    cs->sample_idx++;
    cs->packet_count++;
    cs->last_seen = millis();

    if (rssi > cs->peak_rssi) cs->peak_rssi = rssi;
}

// ============================================================
// Get average RSSI for a channel
// ============================================================
static int8_t avg_rssi(int channel) {
    if (channel < 1 || channel > MAX_CHANNEL) return -100;
    ChannelStats* cs = &channel_stats[channel - 1];
    int samples = cs->sample_idx < SPECTRUM_SAMPLES ? cs->sample_idx : SPECTRUM_SAMPLES;
    if (samples == 0) return -100;
    int32_t sum = 0;
    for (int i = 0; i < samples; i++) {
        sum += cs->rssi_samples[i];
    }
    return (int8_t)(sum / samples);
}

// ============================================================
// RSSI to ASCII bar (10 chars wide)
// ============================================================
static void rssi_bar(int8_t rssi, char* out, int width) {
    // RSSI range: -100 to -20 → 0 to 80
    int val = rssi + 100;
    if (val < 0) val = 0;
    if (val > 80) val = 80;
    int filled = (val * width) / 80;
    for (int i = 0; i < width; i++) {
        out[i] = (i < filled) ? '#' : '.';
    }
    out[width] = '\0';
}

// ============================================================
// Print menu
// ============================================================
static void print_menu(void) {
    Serial.printf("\r\n");
    Serial.printf("  .------------------------------.\r\n");
    Serial.printf("  | FLUG-OS {-1, 0, +1}  v0.3.0  |\r\n");
    Serial.printf("  |------ ASCII DASHBOARD --------|\r\n");
    Serial.printf("  |                              |\r\n");
    Serial.printf("  |  d    dashboard (real-time)  |\r\n");
    Serial.printf("  |  s    scan all channels     |\r\n");
    Serial.printf("  |  w    wave output toggle    |\r\n");
    Serial.printf("  |  m    show this menu        |\r\n");
    Serial.printf("  |  q    quiet mode            |\r\n");
    Serial.printf("  |                              |\r\n");
    Serial.printf("  |  ch 6    set channel 1-13   |\r\n");
    Serial.printf("  |  hop     channel hopping    |\r\n");
    Serial.printf("  |  mode raw|wave|json         |\r\n");
    Serial.printf("  |  filter mgmt|data|ctrl|all  |\r\n");
    Serial.printf("  |  stats   | reset            |\r\n");
    Serial.printf("  |                              |\r\n");
    Serial.printf("  '------------------------------'\r\n");
}

// ============================================================
// Print spectrum dashboard
// ============================================================
static void print_dashboard(void) {
    Serial.printf("\r\n");
    Serial.printf("  FLUG-OS SPECTRUM ANALYZER  {-1, 0, +1}\r\n");
    Serial.printf("  .------------------------------------------------------.\r\n");
    for (int ch = 1; ch <= MAX_CHANNEL; ch++) {
        unsigned long age = millis() - channel_stats[ch - 1].last_seen;
        int8_t rssi_avg = avg_rssi(ch);
        int8_t peak = channel_stats[ch - 1].peak_rssi;
        uint32_t pkts = channel_stats[ch - 1].packet_count;

        char bar[20];
        rssi_bar(rssi_avg, bar, 14);

        // Activity indicator: * = active (< 5s), . = stale
        char activity = (age < 5000) ? '*' : '.';

        // Peak indicator: ! if peak > -50 (strong signal)
        char peak_mark = (peak > -50) ? '!' : ' ';

        Serial.printf("  | CH %02d %s %c %c pkts:%5u |\r\n",
                      ch, bar, activity, peak_mark, pkts);
    }
    Serial.printf("  '------------------------------------------------------'\r\n");
    Serial.printf("  [* active  ! strong signal  . stale]  ");
    Serial.printf("ch=%d hop=%d filter=", current_channel, channel_hopping);
    switch (filter_type) {
        case 0: Serial.printf("mgmt"); break;
        case 1: Serial.printf("ctrl"); break;
        case 2: Serial.printf("data"); break;
        default: Serial.printf("all"); break;
    }
    Serial.printf("\r\n");
}

// ============================================================
// Scan all channels: fast sweep measuring RSSI
// ============================================================
static void scan_channels(void) {
    Serial.printf("\r\n  SCANNING ALL CHANNELS...\r\n\r\n");
    int rssi_readings[MAX_CHANNEL];

    for (int ch = 1; ch <= MAX_CHANNEL; ch++) {
        wifi_set_channel(ch);
        delay(50);  // settle time
        // Read RSSI from the radio (ESP8266's wifi RSSI)
        // In promiscuous mode we use the stored values
        rssi_readings[ch - 1] = avg_rssi(ch);
    }

    // Print results as bar chart
    Serial.printf("  CH  RSSI  SIGNAL\r\n");
    Serial.printf("  --  ----  ------\r\n");
    for (int ch = 1; ch <= MAX_CHANNEL; ch++) {
        char bar[30];
        rssi_bar(rssi_readings[ch - 1], bar, 24);
        Serial.printf("  %02d  %4d  |%s|\r\n", ch, rssi_readings[ch - 1], bar);
    }
    Serial.printf("\r\n  Best channel: ");
    int best_ch = 1;
    int8_t best_rssi = rssi_readings[0];
    for (int ch = 2; ch <= MAX_CHANNEL; ch++) {
        if (rssi_readings[ch - 1] > best_rssi) {
            best_rssi = rssi_readings[ch - 1];
            best_ch = ch;
        }
    }
    Serial.printf("CH %02d (RSSI %d dBm)\r\n", best_ch, best_rssi);

    // Restore current channel
    wifi_set_channel(current_channel);
}

// ============================================================
// Packet line display with ASCII prefix
// ============================================================
static void print_packet_line(int type, int subtype, int8_t rssi,
                              const char* src, const char* ssid) {
    // Activity bar based on RSSI
    char bar[12];
    rssi_bar(rssi, bar, 10);

    const char* type_str = "";
    if (type == 0) {
        switch (subtype) {
            case 8:  type_str = "BCN"; break;
            case 4:  type_str = "PRB"; break;
            case 5:  type_str = "PRS"; break;
            case 11: type_str = "AUTH"; break;
            case 12: type_str = "DEA"; break;
            default: type_str = "MGT"; break;
        }
    } else if (type == 1) {
        type_str = "CTL";
    } else if (type == 2) {
        type_str = "DAT";
    }

    // Print ASCII packet line with signal bar
    char ssid_display[MAX_SSID_LEN + 1];
    if (ssid[0]) {
        snprintf(ssid_display, sizeof(ssid_display), " %s", ssid);
    } else {
        ssid_display[0] = '\0';
    }

    // Pure ASCII, no ANSI: use [|||||....] format for signal
    Serial.printf("%s [%s] src=%s%s ch=%d rssi=%d\r\n",
                  type_str, bar, src, ssid_display, current_channel, rssi);
}

// ============================================================
// Process UI commands (single char)
// Returns true if handled, false if should fall through to normal parser
// ============================================================
static bool handle_ui_command(char c) {
    switch (c) {
        case 'm':
        case 'M':
            print_menu();
            return true;
        case 'd':
        case 'D':
            dashboard_active = !dashboard_active;
            if (dashboard_active) {
                print_dashboard();
            } else {
                Serial.printf("dashboard off\r\n");
            }
            return true;
        case 's':
        case 'S':
            scan_channels();
            return true;
        case 'q':
        case 'Q':
            dashboard_active = false;
            Serial.printf("quiet mode -- packets only\r\n");
            return true;
        default:
            return false;
    }
}

// ============================================================
// Periodic dashboard refresh
// ============================================================
static void update_dashboard(void) {
    if (!dashboard_active) return;
    unsigned long now = millis();
    if (now - last_dashboard_update > 2000) {  // every 2s
        last_dashboard_update = now;
        print_dashboard();
    }
}

#endif
