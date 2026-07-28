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
 *   w       — waterfall (channel activity over time)
 *   t       — top SSIDs by activity
 *   s       — scan all channels (RSSI bar chart)
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
#define SPECTRUM_SAMPLES  20
#define WATERFALL_COLS    40   // 40 chars wide waterfall
#define MAX_SSID_LIST     20   // track top 20 SSIDs
#define SSID_NAME_LEN     33

typedef struct {
    int8_t rssi_samples[SPECTRUM_SAMPLES];
    uint8_t sample_idx;
    int8_t peak_rssi;
    uint32_t packet_count;
    uint32_t last_seen;
    // Waterfall: rolling history of activity levels
    uint8_t waterfall[WATERFALL_COLS];  // activity level per time slice
    uint8_t wf_idx;
} ChannelStats;

// Tracked SSID entry
typedef struct {
    char name[SSID_NAME_LEN];
    uint32_t packet_count;
    int8_t last_rssi;
    uint16_t channel;
} SSIDEntry;

static ChannelStats channel_stats[MAX_CHANNEL];
static SSIDEntry ssid_list[MAX_SSID_LIST];
static int ssid_count = 0;

static bool dashboard_active = false;
static bool waterfall_active = false;
static bool top_active = false;
static unsigned long last_dashboard_update = 0;
static unsigned long last_waterfall_col = 0;

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
        cs->wf_idx = 0;
        for (int s = 0; s < SPECTRUM_SAMPLES; s++) {
            cs->rssi_samples[s] = -100;
        }
        for (int w = 0; w < WATERFALL_COLS; w++) {
            cs->waterfall[w] = 0;
        }
    }
    for (int i = 0; i < MAX_SSID_LIST; i++) {
        ssid_list[i].name[0] = '\0';
        ssid_list[i].packet_count = 0;
        ssid_list[i].last_rssi = -120;
        ssid_list[i].channel = 0;
    }
    ssid_count = 0;
}

// ============================================================
// Record a packet into channel stats
// ============================================================
static void record_packet(int channel, int8_t rssi, const char* ssid) {
    if (channel < 1 || channel > MAX_CHANNEL) return;
    ChannelStats* cs = &channel_stats[channel - 1];

    cs->rssi_samples[cs->sample_idx % SPECTRUM_SAMPLES] = rssi;
    cs->sample_idx++;
    cs->packet_count++;
    cs->last_seen = millis();

    if (rssi > cs->peak_rssi) cs->peak_rssi = rssi;

    // Update waterfall column every ~20 packets
    if (cs->packet_count % 20 == 0) {
        // Activity level: 0-9 based on recent rate
        uint8_t level = (cs->waterfall[cs->wf_idx] < 9) ?
                         cs->waterfall[cs->wf_idx] + 1 : 9;
        cs->waterfall[cs->wf_idx] = level;
    }

    // Track SSID
    if (ssid && ssid[0]) {
        // Find existing
        int idx = -1;
        for (int i = 0; i < ssid_count; i++) {
            if (strcmp(ssid_list[i].name, ssid) == 0) {
                idx = i;
                break;
            }
        }
        if (idx >= 0) {
            ssid_list[idx].packet_count++;
            ssid_list[idx].last_rssi = rssi;
            ssid_list[idx].channel = channel;
        } else if (ssid_count < MAX_SSID_LIST) {
            int new_idx = ssid_count++;
            strncpy(ssid_list[new_idx].name, ssid, SSID_NAME_LEN - 1);
            ssid_list[new_idx].name[SSID_NAME_LEN - 1] = '\0';
            ssid_list[new_idx].packet_count = 1;
            ssid_list[new_idx].last_rssi = rssi;
            ssid_list[new_idx].channel = channel;
        }
    }
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
    Serial.printf("  |  w    waterfall display     |\r\n");
    Serial.printf("  |  t    top SSIDs by activity |\r\n");
    Serial.printf("  |  s    scan all channels     |\r\n");
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
// Print waterfall display (channel activity over time)
// ============================================================
static void print_waterfall(void) {
    Serial.printf("\r\n  FLUG-OS WATERFALL  (time ->  activity per channel)\r\n");
    Serial.printf("  .-------------------------------------------------------------------------------.\r\n");
    for (int ch = 1; ch <= MAX_CHANNEL; ch++) {
        ChannelStats* cs = &channel_stats[ch - 1];
        Serial.printf("  |CH %02d| ", ch);
        for (int w = 0; w < WATERFALL_COLS; w++) {
            int idx = (cs->wf_idx + w) % WATERFALL_COLS;
            uint8_t level = cs->waterfall[idx];
            char c = '.';
            if (level >= 8) c = '#';
            else if (level >= 5) c = '=';
            else if (level >= 3) c = '-';
            else if (level >= 1) c = ':';
            Serial.printf("%c", c);
        }
        Serial.printf(" |\r\n");
    }
    Serial.printf("  '-------------------------------------------------------------------------------'\r\n");
    Serial.printf("  [. idle  : low  - medium  = active  # saturated]\r\n");
    Serial.printf("  ch=%d hop=%d\r\n", current_channel, channel_hopping);
}

// ============================================================
// Print top SSIDs
// ============================================================
static void print_top_ssids(void) {
    Serial.printf("\r\n  TOP SSIDs BY ACTIVITY\r\n");
    Serial.printf("  .----------------------------------------------------------.\r\n");
    Serial.printf("  | # | SSID                         | PKTS  | RSSI | CH |\r\n");
    Serial.printf("  |---|------------------------------|-------|------|-----|\r\n");

    // Bubble sort by packet count descending
    for (int i = 0; i < ssid_count - 1; i++) {
        for (int j = 0; j < ssid_count - i - 1; j++) {
            if (ssid_list[j].packet_count < ssid_list[j + 1].packet_count) {
                SSIDEntry tmp = ssid_list[j];
                ssid_list[j] = ssid_list[j + 1];
                ssid_list[j + 1] = tmp;
            }
        }
    }

    int show = ssid_count < 15 ? ssid_count : 15;
    for (int i = 0; i < show; i++) {
        SSIDEntry* e = &ssid_list[i];
        char bar[12];
        rssi_bar(e->last_rssi, bar, 8);
        Serial.printf("  | %2d | %-28s | %5u | %s | %3d |\r\n",
                      i + 1, e->name, e->packet_count, bar, e->channel);
    }
    Serial.printf("  '----------------------------------------------------------'\r\n");
}
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

        char bar[16];
        rssi_bar(rssi_avg, bar, 14);

        char activity = (age < 3000) ? '*' : '.';
        char peak_mark = (peak > -50) ? '!' : ' ';
        unsigned long idle = age / 1000;

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
            waterfall_active = false;
            top_active = false;
            if (dashboard_active) {
                print_dashboard();
            } else {
                Serial.printf("dashboard off\r\n");
            }
            return true;
        case 'w':
        case 'W':
            waterfall_active = !waterfall_active;
            dashboard_active = false;
            top_active = false;
            if (waterfall_active) {
                print_waterfall();
            }
            return true;
        case 't':
        case 'T':
            top_active = !top_active;
            dashboard_active = false;
            waterfall_active = false;
            if (top_active) {
                print_top_ssids();
            }
            return true;
        case 's':
        case 'S':
            scan_channels();
            return true;
        case 'q':
        case 'Q':
            dashboard_active = false;
            waterfall_active = false;
            top_active = false;
            Serial.printf("quiet mode -- packets only\r\n");
            return true;
        default:
            return false;
    }
}

// ============================================================
// Periodic dashboard/waterfall/top refresh
// ============================================================
static void update_dashboard(void) {
    unsigned long now = millis();

    // Advance waterfall column every 2 seconds
    if (now - last_waterfall_col > 2000) {
        last_waterfall_col = now;
        for (int ch = 0; ch < MAX_CHANNEL; ch++) {
            // Shift and decay
            channel_stats[ch].wf_idx = (channel_stats[ch].wf_idx + 1) % WATERFALL_COLS;
            unsigned long age = now - channel_stats[ch].last_seen;
            if (age > 5000) {
                // Decay: no activity for 5s, reduce level
                if (channel_stats[ch].waterfall[channel_stats[ch].wf_idx] > 0) {
                    channel_stats[ch].waterfall[channel_stats[ch].wf_idx]--;
                }
            } else {
                channel_stats[ch].waterfall[channel_stats[ch].wf_idx] =
                    channel_stats[ch].waterfall[channel_stats[ch].wf_idx];
            }
        }
    }

    if (dashboard_active && now - last_dashboard_update > 2000) {
        last_dashboard_update = now;
        print_dashboard();
    }
    if (waterfall_active && now - last_dashboard_update > 2500) {
        last_dashboard_update = now;
        print_waterfall();
    }
    if (top_active && now - last_dashboard_update > 3000) {
        last_dashboard_update = now;
        print_top_ssids();
    }
}

#endif
