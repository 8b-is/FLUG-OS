/*
 * FLUG-OS — Fluid OS {-1, 0, +1}
 * 802.11 Packet Analyzer for Flipper Zero WiFi Module v1 (ESP8285)
 *
 * Captures frames in promiscuous mode, parses 802.11 headers,
 * outputs structured JSON and raw wave data over UART.
 *
 * Part of the 8b-is stack. Ternary by design. {-1, 0, +1}
 * For educational/defensive use on your own networks only.
 */

#include <ESP8266WiFi.h>
#include "wave_output.h"
#include "ascii_ui.h"
#include "version.h"

// ============================================================
// Configuration
// ============================================================
#define UART_BAUD       115200
#define MAX_CHANNEL     13
#define CHANNEL_HOP_MS  1000
#define STATS_INTERVAL  10000
#define MAX_SSID_LEN    32
#define MAX_SRC_LEN     19      // "xx:xx:xx:xx:xx:xx\0" = 18 chars + null
#define OUTPUT_BUF_LEN  256
#define CMD_BUF_LEN     64

#define TYPE_MGMT       0
#define TYPE_CTRL       1
#define TYPE_DATA       2

#define SUB_BEACON      8
#define SUB_PROBE_REQ   4
#define SUB_PROBE_RESP  5
#define SUB_AUTH        11
#define SUB_DEAUTH      12
#define SUB_ASSOC_REQ   0
#define SUB_ASSOC_RESP  1

// ============================================================
// Global state
// ============================================================
static int current_channel = 1;
static bool channel_hopping = false;
static unsigned long last_hop = 0;
static unsigned long last_stats = 0;
static int filter_type = -1;

typedef struct {
    uint32_t total;
    uint32_t mgmt;
    uint32_t ctrl;
    uint32_t data;
    uint32_t beacon;
    uint32_t deauth;
    uint32_t probe_req;
    uint32_t unknown;
    int8_t   min_rssi;
    int8_t   max_rssi;
} Stats;

static Stats stats = {0};

// ============================================================
// Type name helpers
// ============================================================
static const char* type_name(int type) {
    switch (type) {
        case TYPE_MGMT: return "mgmt";
        case TYPE_CTRL: return "ctrl";
        case TYPE_DATA: return "data";
        default:        return "unk";
    }
}

static const char* subtype_name(int type, int subtype) {
    if (type == TYPE_MGMT) {
        switch (subtype) {
            case SUB_BEACON:     return "beacon";
            case SUB_PROBE_REQ:  return "probe_req";
            case SUB_PROBE_RESP: return "probe_resp";
            case SUB_AUTH:       return "auth";
            case SUB_DEAUTH:     return "deauth";
            case SUB_ASSOC_REQ:  return "assoc_req";
            case SUB_ASSOC_RESP: return "assoc_resp";
            default:             return "mgmt";
        }
    }
    if (type == TYPE_DATA) return "data";
    if (type == TYPE_CTRL) return "ctrl";
    return "unk";
}

static void mac_str(const uint8_t* mac, char* out) {
    sprintf(out, "%02x:%02x:%02x:%02x:%02x:%02x",
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

// ============================================================
// 802.11 frame structures
// ============================================================
typedef struct __attribute__((packed)) {
    uint16_t frame_control;
    uint16_t duration;
    uint8_t  addr1[6];
    uint8_t  addr2[6];
    uint8_t  addr3[6];
    uint16_t seq_ctrl;
} WifiFrameHeader;

typedef struct __attribute__((packed)) {
    uint64_t timestamp;
    uint16_t beacon_interval;
    uint16_t capability;
} BeaconFixed;

// ============================================================
// Promiscuous callback
// ============================================================
static void promisc_cb(uint8_t* buf, uint16_t len) {
    if (len < sizeof(WifiFrameHeader)) return;

    // RSSI from last byte (ESP8266 SDK convention)
    int8_t rssi = (len >= 2) ? (int8_t)buf[len - 1] : 0;

    WifiFrameHeader* hdr = (WifiFrameHeader*)buf;
    uint16_t fctl = hdr->frame_control;
    int type = (fctl >> 2) & 0x03;
    int subtype = (fctl >> 4) & 0x0F;

    if (filter_type >= 0 && type != filter_type) return;

    stats.total++;
    if (rssi < stats.min_rssi) stats.min_rssi = rssi;
    if (rssi > stats.max_rssi) stats.max_rssi = rssi;

    switch (type) {
        case TYPE_MGMT: stats.mgmt++; break;
        case TYPE_CTRL: stats.ctrl++; break;
        case TYPE_DATA: stats.data++; break;
        default: stats.unknown++; break;
    }
    if (type == TYPE_MGMT && subtype == SUB_BEACON) stats.beacon++;
    if (type == TYPE_MGMT && subtype == SUB_DEAUTH) stats.deauth++;
    if (type == TYPE_MGMT && subtype == SUB_PROBE_REQ) stats.probe_req++;

    // MAC addresses
    char src[MAX_SRC_LEN], dst[MAX_SRC_LEN], bssid[MAX_SRC_LEN];
    mac_str(hdr->addr2, src);
    mac_str(hdr->addr1, dst);
    mac_str(hdr->addr3, bssid);

    // Beacon/probe SSID + params
    char ssid[MAX_SSID_LEN + 1] = "";
    uint16_t beacon_interval = 0;
    uint16_t cap_info = 0;

    if (type == TYPE_MGMT && (subtype == SUB_BEACON || subtype == SUB_PROBE_RESP)) {
        size_t offset = sizeof(WifiFrameHeader);
        if (subtype == SUB_BEACON && offset + sizeof(BeaconFixed) <= len) {
            BeaconFixed* bcn = (BeaconFixed*)(buf + offset);
            beacon_interval = bcn->beacon_interval;
            cap_info = bcn->capability;
            offset += sizeof(BeaconFixed);
        } else if (subtype == SUB_PROBE_RESP) {
            offset += sizeof(BeaconFixed);
        }
        // Tag parser (SSID is tag 0)
        while (offset + 2 <= len) {
            uint8_t tag = buf[offset];
            uint8_t tag_len = buf[offset + 1];
            if (tag_len > 50) break;  // malformed guard
            if (offset + 2 + tag_len > len) break;
            if (tag == 0 && tag_len > 0 && tag_len <= MAX_SSID_LEN) {
                memcpy(ssid, &buf[offset + 2], tag_len);
                ssid[tag_len] = '\0';
                break;
            }
            offset += 2 + tag_len;
        }
    }

    // Deauth reason code
    uint16_t reason = 0;
    if (type == TYPE_MGMT && subtype == SUB_DEAUTH) {
        size_t off = sizeof(WifiFrameHeader);
        if (off + 2 <= len) {
            reason = buf[off] | (buf[off + 1] << 8);
        }
    }

    // JSON output
    char out[OUTPUT_BUF_LEN];
    int n = snprintf(out, sizeof(out),
        "🌊 {\"ts\":%lu,\"len\":%u,\"rssi\":%d,"
        "\"type\":\"%s\",\"subtype\":\"%s\","
        "\"src\":\"%s\",\"dst\":\"%s\",\"bssid\":\"%s\","
        "\"chan\":%d",
        millis(), len, rssi,
        type_name(type), subtype_name(type, subtype),
        src, dst, bssid,
        current_channel);

    if (ssid[0]) {
        n += snprintf(out + n, sizeof(out) - n, ",\"ssid\":\"%s\"", ssid);
    }
    if (beacon_interval) {
        n += snprintf(out + n, sizeof(out) - n, ",\"bi\":%u", beacon_interval);
    }
    if (reason) {
        n += snprintf(out + n, sizeof(out) - n, ",\"reason\":%u", reason);
    }
    snprintf(out + n, sizeof(out) - n, "}\n");
    Serial.print(out);

    // Record for UI visualization
    record_packet(current_channel, rssi, ssid);

    // Wave output for sonification
    output_wave(rssi, type, subtype, millis());
}

// ============================================================
// Command parser
// ============================================================
static void handle_command(const char* line) {
    if (strncmp(line, "ch ", 3) == 0) {
        int ch = atoi(line + 3);
        if (ch >= 1 && ch <= MAX_CHANNEL) {
            current_channel = ch;
            wifi_set_channel(current_channel);
            Serial.printf("{\"cmd\":\"ch\",\"val\":%d}\n", current_channel);
        }
    }
    else if (strcmp(line, "hop") == 0) {
        channel_hopping = !channel_hopping;
        Serial.printf("{\"cmd\":\"hop\",\"en\":%d}\n", channel_hopping);
    }
    else if (strncmp(line, "mode ", 5) == 0) {
        set_wave_mode(line + 5);
        Serial.printf("{\"cmd\":\"mode\",\"val\":\"%s\"}\n", line + 5);
    }
    else if (strncmp(line, "filter ", 7) == 0) {
        const char* f = line + 7;
        if (strcmp(f, "mgmt") == 0) filter_type = TYPE_MGMT;
        else if (strcmp(f, "data") == 0) filter_type = TYPE_DATA;
        else if (strcmp(f, "ctrl") == 0) filter_type = TYPE_CTRL;
        else if (strcmp(f, "all") == 0) filter_type = -1;
        Serial.printf("{\"cmd\":\"filter\",\"val\":\"%s\"}\n", f);
    }
    else if (strcmp(line, "stats") == 0) {
        Serial.printf("{\"cmd\":\"stats\",\"total\":%u,\"mgmt\":%u,"
                      "\"ctrl\":%u,\"data\":%u,\"beacon\":%u,"
                      "\"deauth\":%u,\"probe\":%u,"
                      "\"rssi_min\":%d,\"rssi_max\":%d}\n",
                      stats.total, stats.mgmt, stats.ctrl, stats.data,
                      stats.beacon, stats.deauth, stats.probe_req,
                      stats.min_rssi, stats.max_rssi);
    }
    else if (strcmp(line, "reset") == 0) {
        memset(&stats, 0, sizeof(stats));
        Serial.printf("{\"cmd\":\"reset\"}\n");
    }
    else {
        Serial.printf("{\"cmd\":\"?\",\"line\":\"%s\"}\n", line);
    }
}

// ============================================================
// Setup
// ============================================================
void setup() {
    Serial.begin(UART_BAUD);
    delay(200);

    // Pure ASCII boot screen — Flipper dolphin, ternary creed
    Serial.printf("\r\n");
    Serial.printf("  ╔══════════════════════════════════════╗\r\n");
    Serial.printf("  ║        FLUG-OS  {-1, 0, +1}         ║\r\n");
    Serial.printf("  ║    Fluid Operating System " FLUGOS_FULL_VERSION "   ║\r\n");
    Serial.printf("  ╚══════════════════════════════════════╝\r\n");
    Serial.printf("\r\n");
    Serial.printf("          _ __    ___   _ __  ___\r\n");
    Serial.printf("         | '_ \\  / _ \\ | '_ \\/ __|   🜂\r\n");
    Serial.printf("         | | | || (_) || | | \\__ \\  {-1,0,+1}\r\n");
    Serial.printf("         |_| |_| \\___/ |_| |_|___/  ternary\r\n");
    Serial.printf("\r\n");
    Serial.printf("  ╔══════════════════════════════════════╗\r\n");
    Serial.printf("  ║  ~( 802.11 packet-wave sampler )~   ║\r\n");
    Serial.printf("  ║  RSSI → amplitude · type → freq     ║\r\n");
    Serial.printf("  ║  Every packet is a note              ║\r\n");
    Serial.printf("  ╚══════════════════════════════════════╝\r\n");
    Serial.printf("\r\n");
    Serial.printf("      .-～～～-.\r\n");
    Serial.printf("     /  🜂  🜂  \\     8b-is constellation\r\n");
    Serial.printf("    |  {-1,0,+1} |     ternary by design\r\n");
    Serial.printf("     \\  ~~~~~  /\r\n");
    Serial.printf("      `-_____-'\r\n");
    Serial.printf("\r\n");

    wifi_set_opmode(STATION_MODE);
    wifi_set_channel(current_channel);

    wifi_promiscuous_enable(0);
    wifi_set_promiscuous_rx_cb(promisc_cb);
    wifi_promiscuous_enable(1);
    wifi_promiscuous_set_filter(WIFI_PKT_MGMT | WIFI_PKT_CTRL | WIFI_PKT_DATA);

    Serial.printf("  ║  802.11 ⊗ sine kernel · axiomquant  ║\r\n");
    Serial.printf("  ║  K(u)=sin(πu)/(πu) · 5 thresholds    ║\r\n");
    Serial.printf("  ║  Erdős β₁ · BitNet γ · Markowitz λ   ║\r\n");
    Serial.printf("  ║  Black-Scholes σ · Rényi dyadic β    ║\r\n");
    Serial.printf("  ║  m=menu d=dashboard s=scan q=quiet  ║\r\n");
    Serial.printf("  ║  ch <n> · hop · mode · filter · stats║\r\n");
    Serial.printf("  ╚══════════════════════════════════════╝\r\n");
    Serial.printf("\r\n");
    Serial.printf("type 'm' for menu\r\n");
    Serial.printf("\r\n");

    init_ui();

    last_hop = millis();
    last_stats = millis();
    stats.min_rssi = 0;
    stats.max_rssi = -120;
}

// ============================================================
// Main loop
// ============================================================
void loop() {
    unsigned long now = millis();

    // Channel hopping
    if (channel_hopping && (now - last_hop >= CHANNEL_HOP_MS)) {
        current_channel = (current_channel % MAX_CHANNEL) + 1;
        wifi_set_channel(current_channel);
        last_hop = now;
    }

    // Feed the watchdog
    ESP.wdtFeed();

    // Periodic stats heartbeat (quiet, every STATS_INTERVAL)
    if (now - last_stats >= STATS_INTERVAL) {
        last_stats = now;
    }

    // Serial command buffer
    static char cmd_buf[CMD_BUF_LEN];
    static size_t cmd_pos = 0;

    while (Serial.available()) {
        char c = Serial.read();
        // Single-char UI commands (intercept before full command parser)
        if (cmd_pos == 0 && handle_ui_command(c)) {
            continue;
        }
        if (c == '\n' || c == '\r') {
            if (cmd_pos > 0) {
                cmd_buf[cmd_pos] = '\0';
                handle_command(cmd_buf);
                cmd_pos = 0;
            }
        } else if (cmd_pos < sizeof(cmd_buf) - 1) {
            cmd_buf[cmd_pos++] = c;
        }
    }

    // Periodic dashboard refresh
    update_dashboard();

    yield();
}
