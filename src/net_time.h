// net_time.h — Sovereign Tokyo Multi-AP hotspot join + NTP time + peer channel lock + HTTP OTA + ESP-NOW TWIF Wi-Fi Share + BLE.
// Prioritizes the laptop's active Wi-Fi (`Rakuten-4CE2`, Channel 1) with automatic multi-AP fallback,
// NVS persistence (`WIFI <ssid> <password>`), ESP-NOW Triad Wi-Fi sharing (`TWIF`), BLE Nordic UART provisioning,
// and HTTP OTA auto-update. Joins briefly, syncs SNTP + locks `g_s3_sun_ch` to the AP's channel, then restores
// promiscuous capture + dual-channel (Ch 1 / Ch 6) ESP-NOW mesh.
#ifndef FLUGOS_NET_TIME_H
#define FLUGOS_NET_TIME_H

#include <time.h>
#include <WiFiUdp.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <Update.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

// ---- Anicca (Impermanence) Learn-Share-Forget Wi-Fi Pool (Zero Hardcoded SSIDs!) ----
#define FLUG_WIFI_POOL_SLOTS 8

struct FlugLearnedWifi {
    uint8_t  used;
    uint8_t  ttl_q8;          // 255 = verified/fresh, decays via Anicca when unseen or failing -> 0 = FORGOTTEN
    uint8_t  is_portal;       // 1 if captive portal auto-accept was used
    uint8_t  fail_streak;     // consecutive join/portal failures (>= 3 triggers immediate forget for ephemeral slots)
    uint16_t success_cnt;     // verified connections (generate_204 ok)
    int8_t   last_rssi;       // last observed RSSI dBm
    uint8_t  hotspot_tier;    // 0=ephemeral, 1=pinned user, 2=GL.iNet/GlobalWiFi router, 3=Samsung Galaxy S25 Ultra
    char     ssid[33];
    char     pass[65];
};

static FlugLearnedWifi g_wifi_pool[FLUG_WIFI_POOL_SLOTS];
static char          g_net_ssid[33] = "";
static char          g_net_pass[65] = "";
static unsigned long g_net_last_try = 0;
static bool          g_net_ever = false;
static int           g_net_last_ok = -1;     // -1 never tried, 0 failed, 1 ok
static uint8_t       g_net_fail_streak = 0;
static char          g_net_last_ip[20] = "";
static char          g_portal_status[32] = "idle";
static char          g_portal_url[96] = "";
static uint32_t      g_portal_accept_cnt = 0;
static uint32_t      g_wifi_learned_cnt = 0;
static uint32_t      g_wifi_forgotten_cnt = 0;
static bool          g_hotspot_keepalive = true;
static uint8_t       g_hotspot_active_tier = 0;
static uint32_t      g_hotspot_join_ms = 0;
static uint8_t       g_hotspot_last_ch = 0;
static bool          g_net_prev_seen_hotspot = false;
static WiFiUDP       g_memnet_udp;
static bool          g_memnet_udp_ready = false;
static uint8_t       g_memnet_udp_tx_buf[64] = {0};
static volatile bool g_memnet_udp_tx_pending = false;
static bool          g_ota_auto = true;      // Auto-check OTA on net_sync when host is known
static char          g_ota_last_status[48] = "idle";

// ---- Host bridge endpoint (set via `HOST <ip> <port> <token>` or learned over TWIF; NVS + RAM) ----
static char     g_host_ip[20]  = "";
static uint16_t g_host_port    = 8089;
static char     g_host_tok[40] = "";

// ---- ESP-NOW Triad Wi-Fi & OTA Share Capsule ("TWIF") ----
typedef struct __attribute__((packed)) {
    char     magic[4];      // "TWIF"
    uint8_t  node_id;       // 0=ADA, 1=VAK, 2=OMN
    uint8_t  flags;         // bit 0: trigger sync, bit 1: trigger OTA
    uint16_t host_port;     // default 8089
    char     host_ip[20];   // optional host bridge IP
    uint8_t  masked_ssid[33];
    uint8_t  masked_pass[65];
    uint32_t seq;
} S3TriadWifiCapsule;

static volatile bool g_net_twif_rx_pending = false;
static volatile bool g_net_twif_rx_ota     = false;
static volatile uint8_t g_net_twif_from_id = 0xFF;
static uint32_t      g_net_twif_seq        = 0;
static uint32_t      g_net_twif_peer_seq[3] = {0, 0, 0};

// ---- BLE Nordic UART Provisioning State ----
#define FLUG_BLE_SERVICE_UUID "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define FLUG_BLE_CHAR_RX_UUID "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define FLUG_BLE_CHAR_TX_UUID "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

static bool               g_ble_active      = false;
static bool               g_ble_connected   = false;
static BLECharacteristic* g_ble_tx_char     = nullptr;
static volatile bool      g_ble_cmd_pending = false;
static char               g_ble_cmd_buf[128] = "";

static inline uint8_t net_twif_mask_byte(size_t idx, uint32_t seq) {
    return (uint8_t)(0x8B ^ ((idx * 0x5Au) & 0xFFu) ^ (seq & 0xFFu));
}

static void net_store() {
    Preferences p;
    if (p.begin("wifi", false)) {
        p.putString("ssid", g_net_ssid);
        p.putString("pass", g_net_pass);
        p.putBytes("pool", g_wifi_pool, sizeof(g_wifi_pool));
        if (g_host_ip[0]) p.putString("host_ip", g_host_ip);
        if (g_host_port)  p.putUShort("host_port", g_host_port);
        p.putBool("ota_auto", g_ota_auto);
        p.end();
    }
}

// Classify hotspot tier: 3=Samsung Galaxy S25 Ultra, 2=GL.iNet/GlobalWiFi travel router, 1=user-pinned/WPA, 0=ephemeral
static uint8_t net_classify_hotspot_tier(const char* ssid, const char* pass, uint8_t explicit_tier = 0) {
    if (explicit_tier >= 2) return explicit_tier;
    if (!ssid || !ssid[0]) return 0;
    if (strncasecmp(ssid, "Galaxy", 6) == 0 ||
        strncasecmp(ssid, "SM-S928", 7) == 0 ||
        strncasecmp(ssid, "S25", 3) == 0 ||
        strncasecmp(ssid, "W1F", 3) == 0 ||
        strncasecmp(ssid, "AndroidAP", 9) == 0) {
        return 3; // Samsung Galaxy S25 Ultra Mobile Hotspot
    }
    if (strncasecmp(ssid, "GL-", 3) == 0 ||
        strncasecmp(ssid, "GlobalWiFi", 10) == 0 ||
        strncasecmp(ssid, "Rakuten", 7) == 0 ||
        strncasecmp(ssid, "Beryl", 5) == 0 ||
        strncasecmp(ssid, "Slate", 5) == 0 ||
        strncasecmp(ssid, "Mudi", 4) == 0) {
        return 2; // GL.iNet / GlobalWiFi Travel Router
    }
    if (explicit_tier == 1 || (pass && pass[0] != '\0')) {
        return 1; // User-provisioned WPA2/WPA3 hotspot
    }
    return 0;
}

static const char* net_hotspot_tier_name(uint8_t tier) {
    if (tier >= 3) return "GALAXY_S25_ULTRA";
    if (tier == 2) return "GL_INET_ROUTER";
    if (tier == 1) return "PINNED_WPA";
    return "ANICCA_OPEN";
}

static void net_learn_wifi(const char* ssid, const char* pass, int8_t rssi, bool verified, bool is_portal, uint8_t explicit_tier = 0) {
    if (!ssid || !ssid[0]) return;
    uint8_t incoming_tier = net_classify_hotspot_tier(ssid, pass, explicit_tier);
    int slot = -1;
    int weakest_unverified_slot = -1;
    uint8_t min_unverified_ttl = 255;
    int weakest_any_slot = -1;
    uint8_t min_any_ttl = 255;

    for (int i = 0; i < FLUG_WIFI_POOL_SLOTS; i++) {
        if (g_wifi_pool[i].used && strcmp(g_wifi_pool[i].ssid, ssid) == 0) {
            slot = i;
            break;
        }
        if (!g_wifi_pool[i].used && slot < 0) {
            slot = i;
        }
        if (g_wifi_pool[i].used) {
            // Never evict Galaxy Ultra (tier 3) or GL Router (tier 2) slots for lower-tier networks
            if (g_wifi_pool[i].hotspot_tier <= incoming_tier && g_wifi_pool[i].ttl_q8 < min_any_ttl) {
                min_any_ttl = g_wifi_pool[i].ttl_q8;
                weakest_any_slot = i;
            }
            if (g_wifi_pool[i].hotspot_tier == 0 && g_wifi_pool[i].success_cnt == 0 &&
                g_wifi_pool[i].ttl_q8 < min_unverified_ttl) {
                min_unverified_ttl = g_wifi_pool[i].ttl_q8;
                weakest_unverified_slot = i;
            }
        }
    }
    if (slot < 0) {
        if (weakest_unverified_slot >= 0) {
            slot = weakest_unverified_slot;
        } else if ((verified || incoming_tier >= 2 || min_any_ttl < 64) && weakest_any_slot >= 0) {
            slot = weakest_any_slot;
        } else {
            // Preserve attested/verified pool entries against unverified beacon eviction
            return;
        }
    }

    bool is_new = !g_wifi_pool[slot].used || (strcmp(g_wifi_pool[slot].ssid, ssid) != 0);
    bool was_verified = !is_new && (g_wifi_pool[slot].success_cnt > 0);
    bool had_pass = !is_new && (g_wifi_pool[slot].pass[0] != '\0');

    g_wifi_pool[slot].used = 1;
    strncpy(g_wifi_pool[slot].ssid, ssid, sizeof(g_wifi_pool[slot].ssid) - 1);
    g_wifi_pool[slot].ssid[sizeof(g_wifi_pool[slot].ssid) - 1] = '\0';

    if (pass && pass[0]) {
        if (verified || incoming_tier >= 2 || !was_verified || !had_pass) {
            strncpy(g_wifi_pool[slot].pass, pass, sizeof(g_wifi_pool[slot].pass) - 1);
            g_wifi_pool[slot].pass[sizeof(g_wifi_pool[slot].pass) - 1] = '\0';
            g_wifi_pool[slot].is_portal = 0;
        }
    } else if (is_new) {
        g_wifi_pool[slot].pass[0] = '\0';
        g_wifi_pool[slot].is_portal = is_portal ? 1 : 0;
    }

    uint8_t final_tier = net_classify_hotspot_tier(g_wifi_pool[slot].ssid, g_wifi_pool[slot].pass, explicit_tier);
    if (is_new || final_tier > g_wifi_pool[slot].hotspot_tier) {
        g_wifi_pool[slot].hotspot_tier = final_tier;
    }

    if (rssi != 0) g_wifi_pool[slot].last_rssi = rssi;
    if (is_portal && g_wifi_pool[slot].pass[0] == '\0') {
        g_wifi_pool[slot].is_portal = 1;
    }
    if (verified || g_wifi_pool[slot].hotspot_tier >= 2) {
        g_wifi_pool[slot].ttl_q8      = 255;
        g_wifi_pool[slot].fail_streak = 0;
        if (verified) g_wifi_pool[slot].success_cnt++;
    } else if (is_new) {
        g_wifi_pool[slot].ttl_q8      = (g_wifi_pool[slot].hotspot_tier >= 1) ? 220 : 140;
        g_wifi_pool[slot].fail_streak = 0;
        g_wifi_pool[slot].success_cnt = 0;
    }
    if (is_new) g_wifi_learned_cnt++;
    net_store();
}

static void net_forget_slot(int slot, const char* reason) {
    if (slot < 0 || slot >= FLUG_WIFI_POOL_SLOTS || !g_wifi_pool[slot].used) return;
    char forgotten_ssid[33];
    strncpy(forgotten_ssid, g_wifi_pool[slot].ssid, sizeof(forgotten_ssid) - 1);
    forgotten_ssid[sizeof(forgotten_ssid) - 1] = '\0';
    memset(&g_wifi_pool[slot], 0, sizeof(FlugLearnedWifi));
    g_wifi_forgotten_cnt++;
    if (strcmp(g_net_ssid, forgotten_ssid) == 0) {
        g_net_ssid[0] = '\0';
        g_net_pass[0] = '\0';
    }
    net_store();
    Serial.printf("# WIFI_FORGET|{\"node\":\"%s\",\"ssid\":\"%s\",\"reason\":\"%s\",\"forgotten_total\":%lu}\n",
                  g_medusa_node, forgotten_ssid, reason ? reason : "anicca",
                  (unsigned long)g_wifi_forgotten_cnt);
}

static void net_forget_ssid(const char* ssid) {
    if (!ssid || !ssid[0] || strcasecmp(ssid, "ALL") == 0) {
        for (int i = 0; i < FLUG_WIFI_POOL_SLOTS; i++) {
            if (g_wifi_pool[i].used) net_forget_slot(i, "manual_forget_all");
        }
        g_net_ssid[0] = '\0';
        g_net_pass[0] = '\0';
        net_store();
        return;
    }
    for (int i = 0; i < FLUG_WIFI_POOL_SLOTS; i++) {
        if (g_wifi_pool[i].used && strcmp(g_wifi_pool[i].ssid, ssid) == 0) {
            net_forget_slot(i, "manual_forget");
        }
    }
}

static void net_load() {
    memset(g_wifi_pool, 0, sizeof(g_wifi_pool));
    Preferences p;
    if (p.begin("wifi", true)) {
        p.getString("ssid", g_net_ssid, sizeof(g_net_ssid));
        p.getString("pass", g_net_pass, sizeof(g_net_pass));
        if (p.getBytesLength("pool") == sizeof(g_wifi_pool)) {
            p.getBytes("pool", g_wifi_pool, sizeof(g_wifi_pool));
        }
        p.getString("host_ip", g_host_ip, sizeof(g_host_ip));
        uint16_t hp = p.getUShort("host_port", 8089);
        if (hp > 0) g_host_port = hp;
        g_ota_auto = p.getBool("ota_auto", true);
        p.end();
    }
    if (g_net_ssid[0]) {
        net_learn_wifi(g_net_ssid, g_net_pass, -60, false, false);
    }
}

// Lookup strongest visible AP entry for a given SSID in passive 802.11 table (returns RSSI, channel, and BSSID)
static int net_find_seen_ap(const char* ssid, uint8_t* out_ch = nullptr, uint8_t* out_bssid = nullptr) {
    if (!ssid || !ssid[0]) return 0;
    int best_rssi = 0;
    for (int a = 0; a < (int)g_medusa_ap_count; a++) {
        if (g_medusa_aps[a].used && strcmp(g_medusa_aps[a].ssid, ssid) == 0) {
            if (best_rssi == 0 || g_medusa_aps[a].rssi > best_rssi) {
                best_rssi = g_medusa_aps[a].rssi;
                if (out_ch) *out_ch = g_medusa_aps[a].ch;
                if (out_bssid) memcpy(out_bssid, g_medusa_aps[a].bssid, 6);
            }
        }
    }
    return best_rssi;
}

// RSSI of a given SSID if the passive scan has seen it, else 0.
static int net_seen_ssid_rssi(const char* ssid) {
    return net_find_seen_ap(ssid, nullptr, nullptr);
}

// Anicca (Impermanence) decay: refresh seen networks, decay unseen ephemeral networks, and preserve Galaxy/GL/pinned hotspots
static void net_decay_and_forget_pool() {
    // Opportunistically learn strong open networks visible in the passive 802.11 table
    for (int a = 0; a < (int)g_medusa_ap_count; a++) {
        if (g_medusa_aps[a].used && g_medusa_aps[a].is_open &&
            g_medusa_aps[a].ssid[0] != '\0' && g_medusa_aps[a].rssi >= -82) {
            net_learn_wifi(g_medusa_aps[a].ssid, "", g_medusa_aps[a].rssi, false, true);
        }
    }
    for (int i = 0; i < FLUG_WIFI_POOL_SLOTS; i++) {
        if (!g_wifi_pool[i].used) continue;
        if (g_wifi_pool[i].hotspot_tier == 0 && g_wifi_pool[i].pass[0] != '\0') {
            g_wifi_pool[i].hotspot_tier = net_classify_hotspot_tier(g_wifi_pool[i].ssid, g_wifi_pool[i].pass, 1);
        }
        int seen_r = net_seen_ssid_rssi(g_wifi_pool[i].ssid);
        if (seen_r != 0) {
            g_wifi_pool[i].last_rssi = (int8_t)seen_r;
            if (g_wifi_pool[i].ttl_q8 <= 245) g_wifi_pool[i].ttl_q8 += 10;
            else g_wifi_pool[i].ttl_q8 = 255;
        } else if (g_wifi_pool[i].hotspot_tier >= 1) {
            // Sovereign Galaxy Ultra (3), GL Router (2), and pinned WPA (1) hotspots never expire when out of range
            if (g_wifi_pool[i].ttl_q8 < 200) g_wifi_pool[i].ttl_q8 = 200;
        } else {
            // Ephemeral open network is no longer around us -> decay TTL via Anicca
            if (g_wifi_pool[i].ttl_q8 > 24) g_wifi_pool[i].ttl_q8 -= 24;
            else net_forget_slot(i, "anicca_unseen_expired");
        }
    }
}

static const char* net_find_best_open_ssid() {
    int best_idx = -1;
    int8_t best_rssi = -95;
    for (int a = 0; a < (int)g_medusa_ap_count; a++) {
        if (g_medusa_aps[a].used && g_medusa_aps[a].is_open &&
            g_medusa_aps[a].ssid[0] != '\0' && g_medusa_aps[a].rssi > best_rssi) {
            best_rssi = g_medusa_aps[a].rssi;
            best_idx  = a;
        }
    }
    return (best_idx >= 0) ? g_medusa_aps[best_idx].ssid : nullptr;
}

static uint8_t net_pool_used_count() {
    uint8_t c = 0;
    for (int i = 0; i < FLUG_WIFI_POOL_SLOTS; i++) if (g_wifi_pool[i].used) c++;
    return c;
}

static int net_seen_rssi() {
    int best_tier = -1;
    int best_r = 0;
    for (int i = 0; i < FLUG_WIFI_POOL_SLOTS; i++) {
        if (!g_wifi_pool[i].used) continue;
        int r = net_seen_ssid_rssi(g_wifi_pool[i].ssid);
        if (r != 0 && ((int)g_wifi_pool[i].hotspot_tier > best_tier ||
                       ((int)g_wifi_pool[i].hotspot_tier == best_tier && r > best_r))) {
            best_tier = (int)g_wifi_pool[i].hotspot_tier;
            best_r = r;
        }
    }
    if (best_r != 0) return best_r;
    int r = net_seen_ssid_rssi(g_net_ssid);
    if (r != 0) return r;
    const char* open_s = net_find_best_open_ssid();
    if (open_s) return net_seen_ssid_rssi(open_s);
    return 0;
}

static void net_ble_notify(const char* msg) {
    if (g_ble_active && g_ble_connected && g_ble_tx_char && msg) {
        g_ble_tx_char->setValue((uint8_t*)msg, strlen(msg));
        g_ble_tx_char->notify();
    }
}

static void net_emit_pool() {
    for (int i = 0; i < FLUG_WIFI_POOL_SLOTS; i++) {
        if (!g_wifi_pool[i].used) continue;
        int32_t vp = 0, vq = 1;
        rust_reduce_rational_pq((int32_t)g_wifi_pool[i].ttl_q8, 255, &vp, &vq);
        Serial.printf("# WIFI_POOL|{\"node\":\"%s\",\"slot\":%d,\"ssid\":\"%s\",\"profile\":\"%s\",\"tier\":%u,"
                      "\"open\":%d,\"portal\":%u,\"ttl_q8\":%u,\"salience_pq\":\"%ld/%ld\",\"ok_cnt\":%u,\"fails\":%u,\"rssi\":%d}\n",
                      g_medusa_node, i, g_wifi_pool[i].ssid,
                      net_hotspot_tier_name(g_wifi_pool[i].hotspot_tier),
                      (unsigned)g_wifi_pool[i].hotspot_tier,
                      g_wifi_pool[i].pass[0] ? 0 : 1,
                      (unsigned)g_wifi_pool[i].is_portal,
                      (unsigned)g_wifi_pool[i].ttl_q8,
                      (long)vp, (long)vq,
                      (unsigned)g_wifi_pool[i].success_cnt,
                      (unsigned)g_wifi_pool[i].fail_streak,
                      (int)g_wifi_pool[i].last_rssi);
    }
}

static void net_emit_status(const char* note) {
    char buf[400];
    snprintf(buf, sizeof(buf),
             "# WIFI|{\"node\":\"%s\",\"configured\":%d,\"ssid\":\"%s\",\"profile\":\"%s\",\"tier\":%u,"
             "\"last\":%d,\"ip\":\"%s\",\"join_ms\":%lu,\"keepalive\":%d,\"seen_rssi\":%d,"
             "\"pool_active\":%u,\"learned\":%lu,\"forgotten\":%lu,"
             "\"mesh_ch\":%d,\"clock_ok\":%d,\"hour\":%u,\"ble\":%d,\"portal\":\"%s\",\"portal_accepts\":%lu,"
             "\"ota\":\"%s\",\"version\":\"" FLUGOS_VERSION_STR "\",\"note\":\"%s\"}\n",
             g_medusa_node, g_net_ssid[0] ? 1 : 0, g_net_ssid,
             net_hotspot_tier_name(g_hotspot_active_tier), (unsigned)g_hotspot_active_tier,
             g_net_last_ok, g_net_last_ip, (unsigned long)g_hotspot_join_ms, g_hotspot_keepalive ? 1 : 0,
             net_seen_rssi(), (unsigned)net_pool_used_count(),
             (unsigned long)g_wifi_learned_cnt, (unsigned long)g_wifi_forgotten_cnt,
             (int)g_s3_sun_ch, g_lrn_clock_ok ? 1 : 0, (unsigned)g_lrn_hour,
             g_ble_active ? 1 : 0, g_portal_status, (unsigned long)g_portal_accept_cnt,
             g_ota_last_status, note);
    Serial.print(buf);
    net_ble_notify(buf);
}

// Helper: case-insensitive substring search in bounded buffer
static const char* net_strcasestr_n(const char* hay, const char* needle) {
    if (!hay || !needle || !*needle) return nullptr;
    size_t nlen = strlen(needle);
    for (const char* p = hay; *p; p++) {
        if (strncasecmp(p, needle, nlen) == 0) return p;
    }
    return nullptr;
}

// Helper: extract attribute value `attr="val"` or `attr='val'` inside an HTML tag
static bool net_extract_html_attr(const char* tag_start, const char* tag_end, const char* attr, char* out, size_t out_cap) {
    if (!tag_start || !tag_end || !attr || !out || out_cap == 0) return false;
    out[0] = '\0';
    size_t alen = strlen(attr);
    for (const char* p = tag_start; p + alen < tag_end; p++) {
        if (strncasecmp(p, attr, alen) == 0) {
            const char* q = p + alen;
            while (q < tag_end && (*q == ' ' || *q == '\t')) q++;
            if (q >= tag_end || *q != '=') continue;
            q++;
            while (q < tag_end && (*q == ' ' || *q == '\t')) q++;
            if (q >= tag_end) break;
            char quote = 0;
            if (*q == '"' || *q == '\'') quote = *q++;
            size_t k = 0;
            while (q < tag_end && k + 1 < out_cap) {
                if (quote ? (*q == quote) : (*q == ' ' || *q == '>' || *q == '\r' || *q == '\n')) break;
                out[k++] = *q++;
            }
            out[k] = '\0';
            return true;
        }
    }
    return false;
}

// Resolve a relative action URL against a base URL (`http://host[:port]/path`)
static void net_resolve_url(const char* base_url, const char* rel, char* out, size_t out_cap) {
    if (!rel || !rel[0]) {
        strncpy(out, base_url ? base_url : "", out_cap - 1);
        out[out_cap - 1] = '\0';
        return;
    }
    if (strncasecmp(rel, "http://", 7) == 0 || strncasecmp(rel, "https://", 8) == 0) {
        strncpy(out, rel, out_cap - 1);
        out[out_cap - 1] = '\0';
        return;
    }
    // Extract scheme://host[:port] from base_url
    const char* p = strstr(base_url, "://");
    if (!p) {
        snprintf(out, out_cap, "http://%s/%s", WiFi.gatewayIP().toString().c_str(), (rel[0] == '/') ? (rel + 1) : rel);
        return;
    }
    const char* slash = strchr(p + 3, '/');
    size_t origin_len = slash ? (size_t)(slash - base_url) : strlen(base_url);
    if (origin_len >= out_cap - 2) origin_len = out_cap - 2;
    char origin[96];
    if (origin_len >= sizeof(origin)) origin_len = sizeof(origin) - 1;
    memcpy(origin, base_url, origin_len);
    origin[origin_len] = '\0';
    if (rel[0] == '/') {
        snprintf(out, out_cap, "%s%s", origin, rel);
    } else {
        snprintf(out, out_cap, "%s/%s", origin, rel);
    }
}

// ---- Automatic Simple "Accept Button" Captive Portal Detector & Auto-Clicker ----
static bool net_captive_portal_auto_accept() {
    if (WiFi.status() != WL_CONNECTED) return false;

    HTTPClient h;
    const char* header_keys[] = {"Location", "location"};
    h.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
    h.setTimeout(2500);
    if (!h.begin("http://connectivitycheck.gstatic.com/generate_204")) return false;
    h.collectHeaders(header_keys, 2);
    int code = h.GET();
    String loc = h.header("Location");
    if (loc.length() == 0) loc = h.header("location");
    String body = (code == 200) ? h.getString() : String("");
    h.end();

    if (code == 204) {
        strncpy(g_portal_status, "open_204", sizeof(g_portal_status) - 1);
        return true;
    }

    // Determine captive portal landing URL from Redirect Location, meta-refresh, or Gateway IP
    char portal_url[128] = "";
    if (loc.length() > 0) {
        strncpy(portal_url, loc.c_str(), sizeof(portal_url) - 1);
    } else {
        snprintf(portal_url, sizeof(portal_url), "http://%s/", WiFi.gatewayIP().toString().c_str());
    }
    strncpy(g_portal_url, portal_url, sizeof(g_portal_url) - 1);

    // Fetch captive portal HTML if we don't already have it
    if (body.length() < 16 && portal_url[0]) {
        HTTPClient hp;
        hp.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
        hp.setTimeout(3000);
        if (hp.begin(portal_url)) {
            int pcode = hp.GET();
            if (pcode > 0) body = hp.getString();
            hp.end();
        }
    }

    // Parse simple `<form action="..." method="...">` + `<input name="..." value="...">`
    const char* html = body.c_str();
    char action_rel[96] = "";
    char method[12]     = "POST";
    char form_body[256] = "accept=1&agree=yes&terms=on&buttonClicked=4";

    const char* form_tag = net_strcasestr_n(html, "<form");
    if (form_tag) {
        const char* form_tag_end = strchr(form_tag, '>');
        if (form_tag_end) {
            net_extract_html_attr(form_tag, form_tag_end, "action", action_rel, sizeof(action_rel));
            char m_tmp[12] = "";
            if (net_extract_html_attr(form_tag, form_tag_end, "method", m_tmp, sizeof(m_tmp)) && m_tmp[0]) {
                strncpy(method, m_tmp, sizeof(method) - 1);
            }
            const char* form_close = net_strcasestr_n(form_tag_end, "</form");
            if (!form_close) form_close = html + strlen(html);
            const char* inp = form_tag_end;
            while ((inp = net_strcasestr_n(inp, "<input")) != nullptr && inp < form_close) {
                const char* inp_end = strchr(inp, '>');
                if (!inp_end) break;
                char iname[40] = "", ival[64] = "";
                if (net_extract_html_attr(inp, inp_end, "name", iname, sizeof(iname)) && iname[0]) {
                    net_extract_html_attr(inp, inp_end, "value", ival, sizeof(ival));
                    if (!ival[0]) strncpy(ival, "1", sizeof(ival) - 1);
                    size_t cur = strlen(form_body);
                    if (cur + strlen(iname) + strlen(ival) + 3 < sizeof(form_body)) {
                        snprintf(form_body + cur, sizeof(form_body) - cur, "&%s=%s", iname, ival);
                    }
                }
                inp = inp_end + 1;
            }
        }
    } else {
        // Check for a direct `<a href="...accept/login...">` accept button link
        const char* a_tag = html;
        while ((a_tag = net_strcasestr_n(a_tag, "<a ")) != nullptr) {
            const char* a_end = strchr(a_tag, '>');
            if (!a_end) break;
            char href[96] = "";
            if (net_extract_html_attr(a_tag, a_end, "href", href, sizeof(href)) &&
                (net_strcasestr_n(href, "accept") || net_strcasestr_n(href, "login") ||
                 net_strcasestr_n(href, "agree")  || net_strcasestr_n(href, "auth"))) {
                strncpy(action_rel, href, sizeof(action_rel) - 1);
                strncpy(method, "GET", sizeof(method) - 1);
                break;
            }
            a_tag = a_end + 1;
        }
    }

    char submit_url[144];
    net_resolve_url(portal_url, action_rel, submit_url, sizeof(submit_url));

    int submit_code = -1;
    HTTPClient hs;
    hs.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
    hs.setTimeout(3000);
    if (hs.begin(submit_url)) {
        if (strcasecmp(method, "GET") == 0) {
            submit_code = hs.GET();
        } else {
            hs.addHeader("Content-Type", "application/x-www-form-urlencoded");
            submit_code = hs.POST((uint8_t*)form_body, strlen(form_body));
        }
        hs.end();
    }

    // Re-check generate_204 after submitting the Captive Portal Accept form
    int verify_code = -1;
    HTTPClient hv;
    hv.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
    hv.setTimeout(2000);
    if (hv.begin("http://connectivitycheck.gstatic.com/generate_204")) {
        verify_code = hv.GET();
        hv.end();
    }

    g_portal_accept_cnt++;
    if (verify_code == 204) {
        strncpy(g_portal_status, "accepted_204", sizeof(g_portal_status) - 1);
    } else {
        snprintf(g_portal_status, sizeof(g_portal_status), "submitted_%d", submit_code);
    }

    char pbuf[256];
    snprintf(pbuf, sizeof(pbuf),
             "# PORTAL|{\"node\":\"%s\",\"ssid\":\"%s\",\"ip\":\"%s\",\"gateway\":\"%s\","
             "\"portal_url\":\"%s\",\"submit_url\":\"%s\",\"submit_http\":%d,\"verify_204\":%d,\"status\":\"%s\"}\n",
             g_medusa_node, g_net_ssid, WiFi.localIP().toString().c_str(),
             WiFi.gatewayIP().toString().c_str(), portal_url, submit_url,
             submit_code, verify_code, g_portal_status);
    Serial.print(pbuf);
    net_ble_notify(pbuf);
    return (verify_code == 204 || (submit_code >= 200 && submit_code < 400));
}

static bool net_try_join_one(const char* ssid, const char* pass, unsigned long wait_ms, uint8_t tier_hint = 0) {
    if (!ssid || !ssid[0]) return false;
    unsigned long join_t0 = millis();
    esp_wifi_set_promiscuous(false);
    // Disable Wi-Fi modem sleep & force 20MHz HT20 for rock-solid WPA2/WPA3-SAE + DHCP handshake
    // against Samsung Galaxy S25 Ultra (Android 16) and GL.iNet (GL-MT3000/GL-A1300/GlobalWiFi) hotspots
    WiFi.setSleep(false);
    esp_wifi_set_ps(WIFI_PS_NONE);
    esp_wifi_set_bandwidth(WIFI_IF_STA, WIFI_BW_HT20);
    WiFi.setAutoReconnect(true);

    const char* pw = (pass && pass[0]) ? pass : NULL;
    uint8_t seen_ch = 0;
    uint8_t seen_bssid[6] = {0};
    int seen_r = net_find_seen_ap(ssid, &seen_ch, seen_bssid);

    // Stage 1: Direct Channel + BSSID fast-lock when passive 802.11 sniffer has already seen the AP
    if (seen_r != 0 && seen_ch >= 1 && seen_ch <= 13) {
        unsigned long fast_wait = (wait_ms > 2200UL) ? 2200UL : wait_ms;
        WiFi.begin(ssid, pw, (int32_t)seen_ch, seen_bssid, true);
        unsigned long t0 = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - t0 < fast_wait) {
            vTaskDelay(pdMS_TO_TICKS(25));
        }
    }

    // Stage 2: Full-spectrum fallback (handles hotspot channel jump or waking mobile tethering)
    if (WiFi.status() != WL_CONNECTED) {
        WiFi.disconnect(false);
        vTaskDelay(pdMS_TO_TICKS(20));
        WiFi.begin(ssid, pw);
        unsigned long t0 = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - t0 < wait_ms) {
            vTaskDelay(pdMS_TO_TICKS(30));
        }
    }

    if (WiFi.status() == WL_CONNECTED) {
        g_hotspot_join_ms = (uint32_t)(millis() - join_t0);
        strncpy(g_net_ssid, ssid, sizeof(g_net_ssid) - 1);
        g_net_ssid[sizeof(g_net_ssid) - 1] = '\0';
        strncpy(g_net_pass, pass ? pass : "", sizeof(g_net_pass) - 1);
        g_net_pass[sizeof(g_net_pass) - 1] = '\0';
        int ch = (int)WiFi.channel();
        if (ch >= 1 && ch <= 13) {
            g_s3_sun_ch = ch;
            g_hotspot_last_ch = (uint8_t)ch;
        }
        uint8_t gw0 = WiFi.gatewayIP()[0];
        uint8_t gw1 = WiFi.gatewayIP()[1];
        uint8_t gw2 = WiFi.gatewayIP()[2];
        uint8_t det_tier = net_classify_hotspot_tier(ssid, pass, tier_hint);
        if (det_tier < 2) {
            if (gw0 == 192 && gw1 == 168 && gw2 == 8) det_tier = 2; // GL.iNet default 192.168.8.1
            else if (gw0 == 192 && gw1 == 168 && (gw2 == 43 || gw2 == 189 || gw2 == 212)) det_tier = 3; // Galaxy/Android hotspot
        }
        g_hotspot_active_tier = det_tier;
        return true;
    }
    WiFi.disconnect(false);
    return false;
}

static bool net_connect_best(bool allow_blind = false) {
    // If already connected to our active hotspot and IP is valid, reuse immediately
    if (WiFi.status() == WL_CONNECTED && WiFi.localIP()[0] != 0) {
        int ch = (int)WiFi.channel();
        if (ch >= 1 && ch <= 13) g_s3_sun_ch = ch;
        return true;
    }

    // 0. Run Anicca decay & opportunistic open-network learning from passive 802.11 beacons
    net_decay_and_forget_pool();

    // 1. Rank visible networks in g_wifi_pool by (hotspot_tier DESC, seen_rssi DESC, ttl_q8 DESC)
    // so Samsung Galaxy S25 Ultra (tier 3) and GL.iNet Router (tier 2) always connect first!
    int order[FLUG_WIFI_POOL_SLOTS];
    int count = 0;
    for (int i = 0; i < FLUG_WIFI_POOL_SLOTS; i++) {
        if (!g_wifi_pool[i].used) continue;
        if (net_seen_ssid_rssi(g_wifi_pool[i].ssid) != 0) {
            order[count++] = i;
        }
    }
    for (int a = 0; a < count; a++) {
        for (int b = a + 1; b < count; b++) {
            int ia = order[a], ib = order[b];
            int ra = net_seen_ssid_rssi(g_wifi_pool[ia].ssid);
            int rb = net_seen_ssid_rssi(g_wifi_pool[ib].ssid);
            bool swap_it = false;
            if (g_wifi_pool[ib].hotspot_tier > g_wifi_pool[ia].hotspot_tier) swap_it = true;
            else if (g_wifi_pool[ib].hotspot_tier == g_wifi_pool[ia].hotspot_tier && rb > ra) swap_it = true;
            if (swap_it) {
                order[a] = ib;
                order[b] = ia;
            }
        }
    }

    for (int k = 0; k < count; k++) {
        int i = order[k];
        uint8_t tier = g_wifi_pool[i].hotspot_tier;
        unsigned long timeout_ms = (tier >= 2) ? 4500UL : 3500UL;
        if (net_try_join_one(g_wifi_pool[i].ssid, g_wifi_pool[i].pass, timeout_ms, tier)) {
            g_wifi_pool[i].hotspot_tier = g_hotspot_active_tier;
            return true;
        } else {
            g_wifi_pool[i].fail_streak++;
            if (tier == 0 && (g_wifi_pool[i].fail_streak >= 3 || g_wifi_pool[i].ttl_q8 <= 80)) {
                net_forget_slot(i, "join_failed_3x");
            } else if (tier == 0) {
                g_wifi_pool[i].ttl_q8 -= 80;
            }
        }
    }

    // 2. Try primary NVS-remembered SSID if visible
    if (g_net_ssid[0] && net_seen_ssid_rssi(g_net_ssid) != 0) {
        uint8_t tier = net_classify_hotspot_tier(g_net_ssid, g_net_pass, 1);
        if (net_try_join_one(g_net_ssid, g_net_pass, (tier >= 2) ? 4500UL : 3500UL, tier)) return true;
    }

    // 3. Opportunistically join the strongest OPEN 2.4GHz network heard right now
    const char* open_ssid = net_find_best_open_ssid();
    if (open_ssid && open_ssid[0]) {
        if (net_try_join_one(open_ssid, "", 3500UL, 0)) {
            net_learn_wifi(open_ssid, "", (int8_t)WiFi.RSSI(), false, true, 0);
            return true;
        }
    }

    if (!allow_blind) return false;

    // 4. Explicit manual/blind sync: try Galaxy Ultra & GL hotspots first, then remaining pool entries
    if (g_net_ssid[0]) {
        uint8_t tier = net_classify_hotspot_tier(g_net_ssid, g_net_pass, 1);
        if (net_try_join_one(g_net_ssid, g_net_pass, 4500UL, tier)) return true;
    }
    for (int tier_pass = 3; tier_pass >= 0; tier_pass--) {
        for (int i = 0; i < FLUG_WIFI_POOL_SLOTS; i++) {
            if (g_wifi_pool[i].used && (int)g_wifi_pool[i].hotspot_tier == tier_pass) {
                if (net_try_join_one(g_wifi_pool[i].ssid, g_wifi_pool[i].pass, (tier_pass >= 2) ? 4500UL : 3500UL, (uint8_t)tier_pass)) {
                    return true;
                }
            }
        }
    }
    return false;
}

// Broadcast Wi-Fi credentials (+ optional host IP & OTA trigger) across all 13 channels via ESP-NOW
// Executed on Core 0 async worker so the 40 FPS AMOLED UI never stalls!
static void net_share_wifi_espnow_impl(bool trigger_ota = false) {
    if (!g_s3_espnow_ok || !g_net_ssid[0]) {
        net_emit_status("share skipped (no ssid)");
        return;
    }
    S3TriadWifiCapsule cap;
    memset(&cap, 0, sizeof(cap));
    cap.magic[0]  = 'T';
    cap.magic[1]  = 'W';
    cap.magic[2]  = 'I';
    cap.magic[3]  = 'F';
    cap.node_id   = g_triad_self_id;
    cap.flags     = 0x01 | (trigger_ota ? 0x02 : 0x00);
    cap.host_port = g_host_port ? g_host_port : 8089;
    strncpy(cap.host_ip, g_host_ip, sizeof(cap.host_ip) - 1);
    cap.seq       = ++g_net_twif_seq;

    for (size_t i = 0; i < sizeof(cap.masked_ssid); i++) {
        cap.masked_ssid[i] = ((uint8_t)g_net_ssid[i]) ^ net_twif_mask_byte(i, cap.seq);
    }
    for (size_t i = 0; i < sizeof(cap.masked_pass); i++) {
        cap.masked_pass[i] = ((uint8_t)g_net_pass[i]) ^ net_twif_mask_byte(i + 33, cap.seq);
    }

    static const uint8_t bcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    static const uint8_t sweep_order[13] = {6, 1, 11, 2, 3, 4, 5, 7, 8, 9, 10, 12, 13};
    for (int pass = 0; pass < 2; pass++) {
        for (int i = 0; i < 13; i++) {
            esp_wifi_set_channel(sweep_order[i], WIFI_SECOND_CHAN_NONE);
            vTaskDelay(pdMS_TO_TICKS(3));
            esp_now_send(bcast, (const uint8_t*)&cap, sizeof(cap));
        }
    }
    esp_wifi_set_channel((int)g_s3_sun_ch, WIFI_SECOND_CHAN_NONE);
    g_s3_channel = (int)g_s3_sun_ch;
    Serial.printf("# TWIF|{\"event\":\"wifi_shared\",\"from\":\"%s\",\"ssid\":\"%s\",\"ota\":%d,\"swept_channels\":13}\n",
                  g_medusa_node, g_net_ssid, trigger_ota ? 1 : 0);
}

// Called from s3_espnow_recv_cb when a "TWIF" capsule arrives from a sibling Triad board
static void net_on_espnow_twif(const uint8_t* src_mac, const uint8_t* data, int len) {
    if (!src_mac || !data || len < (int)sizeof(S3TriadWifiCapsule)) return;
    guardian_init();
    if (!guardian_is_trusted_fnv32(guardian_mac_fnv32(src_mac))) return;

    const S3TriadWifiCapsule* cap = (const S3TriadWifiCapsule*)data;
    if (cap->magic[0] != 'T' || cap->magic[1] != 'W' || cap->magic[2] != 'I' || cap->magic[3] != 'F') return;
    if (cap->node_id > 2 || cap->node_id == g_triad_self_id) return;
    if (cap->seq == 0 || cap->seq <= g_net_twif_peer_seq[cap->node_id]) return;
    g_net_twif_peer_seq[cap->node_id] = cap->seq;

    char rx_ssid[33];
    char rx_pass[65];
    for (size_t i = 0; i < sizeof(rx_ssid); i++) {
        rx_ssid[i] = (char)(cap->masked_ssid[i] ^ net_twif_mask_byte(i, cap->seq));
    }
    rx_ssid[32] = '\0';
    for (size_t i = 0; i < sizeof(rx_pass); i++) {
        rx_pass[i] = (char)(cap->masked_pass[i] ^ net_twif_mask_byte(i + 33, cap->seq));
    }
    rx_pass[64] = '\0';

    if (!rx_ssid[0]) return;
    strncpy(g_net_ssid, rx_ssid, sizeof(g_net_ssid) - 1);
    strncpy(g_net_pass, rx_pass, sizeof(g_net_pass) - 1);
    net_learn_wifi(rx_ssid, rx_pass, -55, true, rx_pass[0] == '\0');
    if (cap->host_ip[0]) {
        strncpy(g_host_ip, cap->host_ip, sizeof(g_host_ip) - 1);
        g_host_ip[sizeof(g_host_ip) - 1] = '\0';
    }
    if (cap->host_port > 0) {
        g_host_port = cap->host_port;
    }
    g_net_twif_from_id    = cap->node_id;
    g_net_twif_rx_ota     = (cap->flags & 0x02) != 0;
    g_net_twif_rx_pending = true;
}

// ---- HTTP OTA Firmware Auto-Update ----
// Pulls `/firmware/esp32s3_18.bin` (for 1.8" SH8601) or `/firmware/esp32s3_206.bin` (for 2.06" CO5300)
// from `custom_url` or `http://<g_host_ip>:<g_host_port>/firmware/...`.
static bool net_ota_check_and_update(const char* custom_url = nullptr, bool force = false) {
    bool was_connected = (WiFi.status() == WL_CONNECTED);
    if (!was_connected && !net_connect_best()) {
        strncpy(g_ota_last_status, "wifi_join_failed", sizeof(g_ota_last_status) - 1);
        esp_wifi_set_promiscuous(true);
        esp_wifi_set_channel((int)g_s3_sun_ch, WIFI_SECOND_CHAN_NONE);
        g_s3_channel = (int)g_s3_sun_ch;
        net_emit_status("ota: wifi join failed");
        return false;
    }

    String bin_url;
    uint16_t port = g_host_port ? g_host_port : 8089;
    const char* bin_name = g_board_is_18 ? "esp32s3_18.bin" : "esp32s3_206.bin";

    if (custom_url && custom_url[0]) {
        bin_url = String(custom_url);
    } else if (g_host_ip[0]) {
        // First check /firmware/version.json unless forced
        if (!force) {
            HTTPClient hv;
            String ver_url = String("http://") + g_host_ip + ":" + String(port) + "/firmware/version.json";
            if (hv.begin(ver_url)) {
                hv.setTimeout(4000);
                int code = hv.GET();
                if (code == 200) {
                    String payload = hv.getString();
                    hv.end();
                    if (payload.indexOf(FLUGOS_VERSION_STR) >= 0) {
                        strncpy(g_ota_last_status, "up_to_date:" FLUGOS_VERSION_STR, sizeof(g_ota_last_status) - 1);
                        Serial.printf("# OTA|{\"node\":\"%s\",\"status\":\"up_to_date\",\"version\":\"" FLUGOS_VERSION_STR "\"}\n", g_medusa_node);
                        if (!was_connected) {
                            WiFi.disconnect(false);
                            esp_wifi_set_promiscuous(true);
                            esp_wifi_set_channel((int)g_s3_sun_ch, WIFI_SECOND_CHAN_NONE);
                            g_s3_channel = (int)g_s3_sun_ch;
                        }
                        return false;
                    }
                } else {
                    hv.end();
                }
            }
        }
        bin_url = String("http://") + g_host_ip + ":" + String(port) + "/firmware/" + bin_name;
    } else {
        strncpy(g_ota_last_status, "no_host_ip", sizeof(g_ota_last_status) - 1);
        if (!was_connected) {
            WiFi.disconnect(false);
            esp_wifi_set_promiscuous(true);
            esp_wifi_set_channel((int)g_s3_sun_ch, WIFI_SECOND_CHAN_NONE);
            g_s3_channel = (int)g_s3_sun_ch;
        }
        return false;
    }

    Serial.printf("# OTA|{\"node\":\"%s\",\"status\":\"downloading\",\"url\":\"%s\",\"current\":\"" FLUGOS_VERSION_STR "\"}\n",
                  g_medusa_node, bin_url.c_str());
    net_ble_notify("# OTA|downloading\n");

    WiFiClient client;
    httpUpdate.rebootOnUpdate(false);
    t_httpUpdate_return ret = httpUpdate.update(client, bin_url);

    if (ret == HTTP_UPDATE_OK) {
        strncpy(g_ota_last_status, "ok_rebooting", sizeof(g_ota_last_status) - 1);
        Serial.printf("# OTA|{\"node\":\"%s\",\"status\":\"success\",\"rebooting\":1}\n", g_medusa_node);
        net_ble_notify("# OTA|success_rebooting\n");
        delay(400);
        ESP.restart();
        return true;
    } else if (ret == HTTP_UPDATE_NO_UPDATES) {
        strncpy(g_ota_last_status, "no_updates", sizeof(g_ota_last_status) - 1);
    } else {
        snprintf(g_ota_last_status, sizeof(g_ota_last_status), "err_%d", httpUpdate.getLastError());
        Serial.printf("# OTA|{\"node\":\"%s\",\"status\":\"failed\",\"err\":%d,\"msg\":\"%s\"}\n",
                      g_medusa_node, httpUpdate.getLastError(), httpUpdate.getLastErrorString().c_str());
    }

    if (!was_connected) {
        WiFi.disconnect(false);
        esp_wifi_set_promiscuous(true);
        esp_wifi_set_channel((int)g_s3_sun_ch, WIFI_SECOND_CHAN_NONE);
        g_s3_channel = (int)g_s3_sun_ch;
    }
    return false;
}

static void net_memnet_udp_exchange_while_connected() {
    if (WiFi.status() != WL_CONNECTED) return;
    if (!g_memnet_udp_ready) {
        if (g_memnet_udp.begin(8888)) {
            g_memnet_udp_ready = true;
        }
    }
    if (!g_memnet_udp_ready) return;

    // Broadcast latest 64-byte MemnetCapsule64 across the Wi-Fi / Captive Portal LAN (UDP 8888)
    if (g_memnet_udp_tx_pending) {
        g_memnet_udp_tx_pending = false;
        IPAddress bcast(255, 255, 255, 255);
        g_memnet_udp.beginPacket(bcast, 8888);
        g_memnet_udp.write(g_memnet_udp_tx_buf, 64);
        g_memnet_udp.endPacket();
    }

    // Receive any peer MemnetCapsule64 packets over Wi-Fi UDP 8888
    for (int k = 0; k < 4; k++) {
        int pktsz = g_memnet_udp.parsePacket();
        if (pktsz < 64) break;
        uint8_t rx64[64];
        int n = g_memnet_udp.read(rx64, 64);
        if (n >= 64 && rx64[0] == 'N' && rx64[1] == 'M') {
            memnet_on_espnow_rx(rx64, 64, (int8_t)WiFi.RSSI());
        }
    }
}

#define FLUG_OFFLINE_LOG_SLOTS 8
static char     g_offline_log_ring[FLUG_OFFLINE_LOG_SLOTS][96];
static uint8_t  g_offline_log_head = 0;
static uint8_t  g_offline_log_cnt  = 0;
static uint32_t g_offline_log_total = 0;

static void net_record_offline_log(const char* event_tag) {
    uint8_t slot;
    if (g_offline_log_cnt < FLUG_OFFLINE_LOG_SLOTS) {
        slot = (uint8_t)((g_offline_log_head + g_offline_log_cnt) % FLUG_OFFLINE_LOG_SLOTS);
        g_offline_log_cnt++;
    } else {
        slot = g_offline_log_head;
        g_offline_log_head = (uint8_t)((g_offline_log_head + 1) % FLUG_OFFLINE_LOG_SLOTS);
    }
    g_offline_log_total++;
    const char* best_open = net_find_best_open_ssid();
    snprintf(g_offline_log_ring[slot], sizeof(g_offline_log_ring[slot]),
             "t=%lus tag=%s aps=%u pool=%u open=%s fails=%u",
             (unsigned long)(millis() / 1000UL),
             event_tag ? event_tag : "offline",
             (unsigned)g_medusa_ap_count,
             (unsigned)net_pool_used_count(),
             (best_open && best_open[0]) ? best_open : "none",
             (unsigned)g_net_fail_streak);
    Preferences p;
    if (p.begin("wifi", false)) {
        p.putUInt("off_tot", g_offline_log_total);
        p.putUChar("off_cnt", g_offline_log_cnt);
        p.putString("off_last", g_offline_log_ring[slot]);
        p.end();
    }
    Serial.printf("# OFFLINE_LOG|{\"node\":\"%s\",\"queued\":%u,\"total\":%lu,\"entry\":\"%s\"}\n",
                  g_medusa_node, (unsigned)g_offline_log_cnt,
                  (unsigned long)g_offline_log_total, g_offline_log_ring[slot]);
}

static void net_upload_cheet_telemetry() {
    if (WiFi.status() != WL_CONNECTED) return;
    HTTPClient hc;
    hc.setTimeout(3500);
    if (!hc.begin("http://49.13.101.247:8795/api/wa-archive/ingest")) return;
    hc.addHeader("Content-Type", "application/json");

    char logs_summary[256] = "";
    size_t pos = 0;
    for (uint8_t i = 0; i < g_offline_log_cnt; i++) {
        uint8_t idx = (uint8_t)((g_offline_log_head + i) % FLUG_OFFLINE_LOG_SLOTS);
        int w = snprintf(logs_summary + pos, sizeof(logs_summary) - pos,
                         "%s[%s]", (i == 0) ? "" : " ", g_offline_log_ring[idx]);
        if (w <= 0 || (size_t)w >= sizeof(logs_summary) - pos) break;
        pos += (size_t)w;
    }

    char payload[640];
    snprintf(payload, sizeof(payload),
             "{\"sender\":\"%s\",\"jid\":\"120363413133092554@g.us\",\"use_llm\":false,"
             "\"text\":\"📡 *ESP32-S3 Watch Auto-Checkin (`%s`)* — SSID=`%s` (`%d dBm`, portal=`%s`) · "
             "WiFiPool=`%u/%u` (learned=`%lu`, forgotten=`%lu`) · APs=`%u` · FlushedOfflineLogs=`%u` (total=`%lu`) %s\"}",
             g_medusa_node, g_medusa_node, g_net_ssid, (int)WiFi.RSSI(), g_portal_status,
             (unsigned)net_pool_used_count(), (unsigned)FLUG_WIFI_POOL_SLOTS,
             (unsigned long)g_wifi_learned_cnt, (unsigned long)g_wifi_forgotten_cnt,
             (unsigned)g_medusa_ap_count, (unsigned)g_offline_log_cnt,
             (unsigned long)g_offline_log_total, logs_summary);

    int code = hc.POST((uint8_t*)payload, strlen(payload));
    hc.end();
    if (code >= 200 && code < 300) {
        g_offline_log_cnt = 0;
        g_offline_log_head = 0;
        Preferences p;
        if (p.begin("wifi", false)) {
            p.putUChar("off_cnt", 0);
            p.end();
        }
    }
    Serial.printf("# CHEET_UPLOAD|{\"node\":\"%s\",\"ssid\":\"%s\",\"http\":%d}\n",
                  g_medusa_node, g_net_ssid, code);
}

static void net_sync_impl(bool allow_blind = false) {
    g_net_last_try = millis();
    g_net_ever = true;

    if (!allow_blind && WiFi.status() != WL_CONNECTED && net_seen_rssi() == 0) {
        g_net_last_ok = 0;
        g_net_last_ip[0] = '\0';
        net_record_offline_log("no_known_or_open_ap");
        net_emit_status("offline mesh mode (fast)");
        return;
    }

    if (net_connect_best(allow_blind)) {
        g_net_fail_streak = 0;
        strncpy(g_net_last_ip, WiFi.localIP().toString().c_str(), sizeof(g_net_last_ip) - 1);
        g_net_last_ip[sizeof(g_net_last_ip) - 1] = '\0';

        // 1. For password-protected Galaxy S25 Ultra / GL.iNet hotspots, skip slow HTML scraping if generate_204 succeeds
        bool portal_ok = true;
        if (g_hotspot_active_tier >= 2 && g_net_pass[0] != '\0') {
            strncpy(g_portal_status, "hotspot_direct", sizeof(g_portal_status) - 1);
        } else {
            portal_ok = net_captive_portal_auto_accept();
        }

        // 2. Reinforce learned Wi-Fi entry & share verified network with sibling nodes over ESP-NOW TWIF!
        net_learn_wifi(g_net_ssid, g_net_pass, (int8_t)WiFi.RSSI(), portal_ok, g_net_pass[0] == '\0', g_hotspot_active_tier);
        if (portal_ok) {
            net_share_wifi_espnow_impl(false);
        }
        // 3. Exchange MemnetCapsule64 waves over Wi-Fi UDP 8888 while on the LAN
        net_memnet_udp_exchange_while_connected();
        // 4. Upload health + flushed offline logs to cheet.is (49.13.101.247:8795)
        net_upload_cheet_telemetry();
        // 5. Sync NTP clock
        configTime(0, 0, "pool.ntp.org", "time.google.com", "time.cloudflare.com");
        time_t now = 0;
        unsigned long t1 = millis();
        while ((now = time(nullptr)) < 1700000000 && millis() - t1 < 2200UL) {
            vTaskDelay(pdMS_TO_TICKS(50));
        }
        if (now >= 1700000000) {
            learn_clock_set((long long)now);
            g_net_last_ok = 1;
            net_emit_status((g_hotspot_active_tier >= 2) ? "hotspot + ntp ok" : "ntp + portal ok");
        } else {
            g_net_last_ok = 1;
            net_emit_status((g_hotspot_active_tier >= 2) ? "hotspot connected" : "connected + portal checked");
        }
        // Automatic OTA version check while connected to Wi-Fi if host IP is known
        if (g_ota_auto && g_host_ip[0]) {
            net_ota_check_and_update(nullptr, false);
        }

        // Keep STA association alive on sovereign Galaxy S25 Ultra / GL.iNet hotspots when keepalive is enabled
        if (g_hotspot_keepalive && g_hotspot_active_tier >= 2 && WiFi.status() == WL_CONNECTED) {
            int ch = (int)WiFi.channel();
            if (ch >= 1 && ch <= 13) {
                g_s3_sun_ch = ch;
                g_s3_channel = ch;
            }
            g_s3_hopping = false;
            esp_wifi_set_promiscuous(true);
            return;
        }
    } else {
        if (g_net_fail_streak < 250) g_net_fail_streak++;
        g_net_last_ok = 0;
        g_net_last_ip[0] = '\0';
        net_record_offline_log("join_timeout");
        net_emit_status("offline mesh mode");
    }
    WiFi.disconnect(false);
    // restore mesh channel + promiscuous capture
    esp_wifi_set_promiscuous(true);
    esp_wifi_set_channel((int)g_s3_sun_ch, WIFI_SECOND_CHAN_NONE);
    g_s3_channel = (int)g_s3_sun_ch;
}

static void net_host_set(const char* a) {   // HOST <ip> <port> <token>
    char ip[20] = "", tok[40] = "";
    unsigned port = 0;
    if (sscanf(a, " %19s %u %39s", ip, &port, tok) >= 2) {
        strncpy(g_host_ip, ip, sizeof(g_host_ip) - 1);
        if (tok[0]) strncpy(g_host_tok, tok, sizeof(g_host_tok) - 1);
        g_host_port = (uint16_t)port;
        net_store();
    }
}

static void net_push_say_impl(const char* say, uint32_t seq) {
    if (!g_net_ssid[0] || !g_host_ip[0] || !g_host_port || (WiFi.status() != WL_CONNECTED && net_seen_rssi() == 0)) return;
    bool was_connected = (WiFi.status() == WL_CONNECTED);
    int code = -1;
    if (was_connected || net_connect_best(false)) {
        HTTPClient h;
        String url = String("http://") + g_host_ip + ":" + String(g_host_port) + "/board/say";
        if (h.begin(url)) {
            h.setTimeout(2500);
            h.addHeader("Content-Type", "application/json");
            h.addHeader("X-Triad-Token", g_host_tok);
            String body = String("{\"node\":\"") + g_medusa_node + "\",\"seq\":" + String((unsigned long)seq) + ",\"say\":\"" + say + "\"}";
            code = h.POST(body);
            h.end();
        }
    }
    if (!was_connected && !(g_hotspot_keepalive && g_hotspot_active_tier >= 2)) {
        WiFi.disconnect(false);
    }
    esp_wifi_set_promiscuous(true);
    esp_wifi_set_channel((int)g_s3_sun_ch, WIFI_SECOND_CHAN_NONE);
    g_s3_channel = (int)g_s3_sun_ch;
    Serial.printf("# PUSH|{\"node\":\"%s\",\"host\":\"%s:%u\",\"http\":%d}\n", g_medusa_node, g_host_ip, (unsigned)g_host_port, code);
}

// ---- Non-Blocking Async Queue for Core 0 Network Worker ----
static volatile bool g_net_req_sync       = false;
static volatile bool g_net_req_blind_sync = false;
static volatile bool g_net_req_share      = false;
static volatile bool g_net_req_share_ota  = false;
static volatile bool g_net_req_ota_force  = false;
static volatile bool g_net_req_push       = false;
static char          g_net_req_ota_url[96] = "";
static char          g_net_push_say_buf[112] = "";
static uint32_t      g_net_push_say_seq   = 0;
static bool          g_net_worker_started = false;

static void net_sync(bool allow_blind = false) {
    g_net_last_try = millis();
    g_net_ever = true;
    if (allow_blind) g_net_req_blind_sync = true;
    else g_net_req_sync = true;
}

static void net_share_wifi_espnow(bool trigger_ota = false) {
    if (trigger_ota) g_net_req_share_ota = true;
    else g_net_req_share = true;
}

static void net_push_say(const char* say, uint32_t seq) {
    if (!say || !say[0] || !g_host_ip[0]) return;
    strncpy(g_net_push_say_buf, say, sizeof(g_net_push_say_buf) - 1);
    g_net_push_say_buf[sizeof(g_net_push_say_buf) - 1] = '\0';
    g_net_push_say_seq = seq;
    g_net_req_push = true;
}

static void net_queue_ota(const char* custom_url, bool force) {
    if (custom_url && custom_url[0]) {
        strncpy(g_net_req_ota_url, custom_url, sizeof(g_net_req_ota_url) - 1);
        g_net_req_ota_url[sizeof(g_net_req_ota_url) - 1] = '\0';
    } else {
        g_net_req_ota_url[0] = '\0';
    }
    g_net_req_ota_force = force;
}

static void s3_net_async_worker_task(void* arg) {
    (void)arg;
    for (;;) {
        if (g_net_req_share || g_net_req_share_ota) {
            bool ota = g_net_req_share_ota;
            g_net_req_share = false;
            g_net_req_share_ota = false;
            g_net_busy = true;
            net_share_wifi_espnow_impl(ota);
            g_net_busy = false;
        }
        if (g_net_req_blind_sync || g_net_req_sync) {
            bool blind = g_net_req_blind_sync;
            g_net_req_blind_sync = false;
            g_net_req_sync = false;
            g_net_busy = true;
            net_sync_impl(blind);
            g_net_busy = false;
        }
        if (g_net_req_ota_force) {
            g_net_req_ota_force = false;
            char url[96];
            strncpy(url, g_net_req_ota_url, sizeof(url) - 1);
            url[sizeof(url) - 1] = '\0';
            g_net_busy = true;
            net_ota_check_and_update(url[0] ? url : nullptr, true);
            g_net_busy = false;
        }
        if (g_net_req_push) {
            g_net_req_push = false;
            char say[112];
            strncpy(say, g_net_push_say_buf, sizeof(say) - 1);
            say[sizeof(say) - 1] = '\0';
            g_net_busy = true;
            net_push_say_impl(say, g_net_push_say_seq);
            g_net_busy = false;
        }
        vTaskDelay(pdMS_TO_TICKS(25));
    }
}

static void s3_net_async_start_worker() {
    if (g_net_worker_started) return;
    g_net_worker_started = true;
    xTaskCreatePinnedToCore(s3_net_async_worker_task, "s3_net_async", 6144, NULL, 1, NULL, 0);
}

// ---- BLE Nordic UART Service (accepts `WIFI <ssid> <pass>`, `WIFI SHARE`, `OTA`, etc. from phone/laptop) ----
class FlugBleServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) override {
        (void)pServer;
        g_ble_connected = true;
    }
    void onDisconnect(BLEServer* pServer) override {
        g_ble_connected = false;
        if (g_ble_active && pServer) {
            pServer->startAdvertising();
        }
    }
};

class FlugBleRxCallbacks : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic* pCharacteristic) override {
        if (!pCharacteristic) return;
        auto val = pCharacteristic->getValue();
        if (val.length() > 0) {
            size_t n = val.length();
            if (n >= sizeof(g_ble_cmd_buf)) n = sizeof(g_ble_cmd_buf) - 1;
            memcpy(g_ble_cmd_buf, val.c_str(), n);
            g_ble_cmd_buf[n] = '\0';
            while (n > 0 && (g_ble_cmd_buf[n - 1] == '\r' || g_ble_cmd_buf[n - 1] == '\n')) {
                g_ble_cmd_buf[--n] = '\0';
            }
            if (n > 0) g_ble_cmd_pending = true;
        }
    }
};

static void net_ble_start() {
    if (g_ble_active) {
        BLEDevice::startAdvertising();
        net_emit_status("ble advertising");
        return;
    }
    char dev_name[28];
    snprintf(dev_name, sizeof(dev_name), "FLUG-%s", g_medusa_node);
    BLEDevice::init(dev_name);
    BLEServer* pServer = BLEDevice::createServer();
    pServer->setCallbacks(new FlugBleServerCallbacks());
    BLEService* pService = pServer->createService(FLUG_BLE_SERVICE_UUID);
    g_ble_tx_char = pService->createCharacteristic(
        FLUG_BLE_CHAR_TX_UUID,
        BLECharacteristic::PROPERTY_NOTIFY | BLECharacteristic::PROPERTY_READ
    );
    g_ble_tx_char->addDescriptor(new BLE2902());
    BLECharacteristic* pRxChar = pService->createCharacteristic(
        FLUG_BLE_CHAR_RX_UUID,
        BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR
    );
    pRxChar->setCallbacks(new FlugBleRxCallbacks());
    pService->start();
    BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(FLUG_BLE_SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    BLEDevice::startAdvertising();
    g_ble_active = true;
    Serial.printf("# BLE|{\"node\":\"%s\",\"name\":\"%s\",\"service\":\"" FLUG_BLE_SERVICE_UUID "\",\"status\":\"advertising\"}\n",
                  g_medusa_node, dev_name);
}

// `HOTSPOT`, `HOTSPOT STATUS`, `HOTSPOT GALAXY <ssid> [pass]`, `HOTSPOT GL <ssid> [pass]`, `HOTSPOT KEEP <0|1>`
static void net_hotspot_command(const char* a) {
    while (*a == ' ') a++;
    if (!*a || strcasecmp(a, "STATUS") == 0) {
        net_emit_status("hotspot_status");
        net_emit_pool();
        return;
    }
    if (strncasecmp(a, "KEEP ", 5) == 0) {
        const char* v = a + 5;
        while (*v == ' ') v++;
        g_hotspot_keepalive = (*v != '0' && strcasecmp(v, "OFF") != 0 && strcasecmp(v, "FALSE") != 0);
        net_emit_status(g_hotspot_keepalive ? "hotspot_keepalive_on" : "hotspot_keepalive_off");
        return;
    }
    uint8_t explicit_tier = 1;
    if (strncasecmp(a, "GALAXY ", 7) == 0) {
        explicit_tier = 3;
        a += 7;
    } else if (strncasecmp(a, "GL ", 3) == 0) {
        explicit_tier = 2;
        a += 3;
    }
    while (*a == ' ') a++;
    if (!*a) {
        net_emit_status("hotspot_status");
        return;
    }
    const char* sp = strchr(a, ' ');
    size_t n = sp ? (size_t)(sp - a) : strlen(a);
    if (n >= sizeof(g_net_ssid)) n = sizeof(g_net_ssid) - 1;
    memcpy(g_net_ssid, a, n);
    g_net_ssid[n] = '\0';
    g_net_pass[0] = '\0';
    if (sp) {
        while (*sp == ' ') sp++;
        strncpy(g_net_pass, sp, sizeof(g_net_pass) - 1);
        g_net_pass[sizeof(g_net_pass) - 1] = '\0';
    }
    g_hotspot_active_tier = net_classify_hotspot_tier(g_net_ssid, g_net_pass, explicit_tier);
    net_learn_wifi(g_net_ssid, g_net_pass, -48, true, g_net_pass[0] == '\0', explicit_tier);
    net_share_wifi_espnow(false);
    net_sync(true);
}

// `WIFI`, `WIFI POOL`, `WIFI FORGET [ssid]`, `WIFI SYNC`, `WIFI PORTAL`, `WIFI SHARE`, `WIFI <ssid> [password]`
static void net_command(const char* a) {
    while (*a == ' ') a++;
    if (!*a) { net_emit_status("status"); net_emit_pool(); return; }
    if (strcasecmp(a, "POOL") == 0) { net_decay_and_forget_pool(); net_emit_pool(); return; }
    if (strcasecmp(a, "SYNC") == 0 || strcasecmp(a, "PORTAL") == 0 || strcasecmp(a, "ACCEPT") == 0) { net_sync(true); return; }
    if (strcasecmp(a, "SHARE") == 0) { net_share_wifi_espnow(false); return; }
    if (strcasecmp(a, "SHARE OTA") == 0) { net_share_wifi_espnow(true); return; }
    if (strcasecmp(a, "CLEAR") == 0 || strcasecmp(a, "FORGET") == 0) { net_forget_ssid("ALL"); net_emit_status("forgotten_all"); return; }
    if (strncasecmp(a, "FORGET ", 7) == 0) {
        const char* target = a + 7;
        while (*target == ' ') target++;
        net_forget_ssid(target);
        net_emit_status("forgotten_ssid");
        return;
    }
    if (strncasecmp(a, "HOTSPOT", 7) == 0) {
        net_hotspot_command(a + 7);
        return;
    }
    if (strncasecmp(a, "LEARN ", 6) == 0) {
        a += 6;
        while (*a == ' ') a++;
    }
    const char* sp = strchr(a, ' ');
    size_t n = sp ? (size_t)(sp - a) : strlen(a);
    if (n >= sizeof(g_net_ssid)) n = sizeof(g_net_ssid) - 1;
    memcpy(g_net_ssid, a, n);
    g_net_ssid[n] = '\0';
    g_net_pass[0] = '\0';
    if (sp) { while (*sp == ' ') sp++; strncpy(g_net_pass, sp, sizeof(g_net_pass) - 1); g_net_pass[sizeof(g_net_pass) - 1] = '\0'; }
    uint8_t tier = net_classify_hotspot_tier(g_net_ssid, g_net_pass, 1);
    g_hotspot_active_tier = tier;
    net_learn_wifi(g_net_ssid, g_net_pass, -50, true, g_net_pass[0] == '\0', tier);
    // Automatically share the newly learned Wi-Fi credentials with the other Triad boards over ESP-NOW!
    net_share_wifi_espnow(false);
    net_sync(true);
}

static void net_tick(unsigned long now) {
    // Handle incoming ESP-NOW TWIF Wi-Fi share from a sibling board
    if (g_net_twif_rx_pending) {
        g_net_twif_rx_pending = false;
        bool do_ota = g_net_twif_rx_ota;
        g_net_twif_rx_ota = false;
        net_store();
        Serial.printf("# TWIF|{\"event\":\"wifi_received\",\"node\":\"%s\",\"from_id\":%u,\"ssid\":\"%s\",\"ota\":%d}\n",
                      g_medusa_node, (unsigned)g_net_twif_from_id, g_net_ssid, do_ota ? 1 : 0);
        net_sync();
        if (do_ota) {
            net_queue_ota(nullptr, true);
        }
    }
    if (now < 4500UL) return;
    if (!g_net_ever) {
        net_sync();
        return;
    }

    // While connected in hotspot keepalive mode, service UDP 8888 MEMNET packets continuously
    if (WiFi.status() == WL_CONNECTED) {
        net_memnet_udp_exchange_while_connected();
        if (now - g_net_last_try > 6UL * 3600UL * 1000UL) {
            net_sync();
        }
        return;
    }

    // Fast-trigger join as soon as passive 802.11 sniffer hears a known Galaxy Ultra / GL router / pool hotspot
    int seen_r = net_seen_rssi();
    bool currently_seen = (seen_r != 0);
    if (currently_seen && !g_net_prev_seen_hotspot && (now - g_net_last_try > 4000UL)) {
        g_net_prev_seen_hotspot = true;
        net_sync();
        return;
    }
    g_net_prev_seen_hotspot = currently_seen;

    // Periodic retry: every 25s when a known hotspot is visible in RF spectrum, or every 6h otherwise
    if (currently_seen && g_net_last_ok != 1 && (now - g_net_last_try > 25000UL)) {
        net_sync();
    } else if (now - g_net_last_try > 6UL * 3600UL * 1000UL) {
        net_sync();
    }
}

#endif /* FLUGOS_NET_TIME_H */
