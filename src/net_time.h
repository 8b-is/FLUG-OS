// net_time.h — Sovereign Tokyo Multi-AP hotspot join + NTP time + peer channel lock.
// Prioritizes the laptop's active Wi-Fi (`Rakuten-4CE2`, Channel 1) with automatic multi-AP fallback
// and NVS persistence (`WIFI <ssid> <password>`). Joins briefly, syncs SNTP + locks `g_s3_sun_ch`
// to the AP's channel (Ch 1), then restores promiscuous capture + dual-channel (Ch 1 / Ch 6) ESP-NOW mesh.
#ifndef FLUGOS_NET_TIME_H
#define FLUGOS_NET_TIME_H

#include <time.h>
#include <HTTPClient.h>

struct FlugWifiCred {
    const char* ssid;
    const char* pass;
};

static const FlugWifiCred FLUG_WIFI_APS[] = {
    {"Rakuten-4CE2",           "EYA5EVQWC2U4GDA9"},
    {"GlobalWiFi_kdq",         "10674213"},
    {"BEWIFI-WORK",            "u9rasng8"},
    {"Buffalo-A-1260",         "y5ijxmy7p5jpe"},
    {"_manekineko_free_Wi-Fi", "karaokemanekineko"},
    {"SSID-A2FFA4",            "He74h7Te"},
    {"W1F\xC2\xA1",            "7576147542!."},
    {"THE_CAMPUS_WiFi",        ""}
};
static const int FLUG_NUM_WIFI_APS = (int)(sizeof(FLUG_WIFI_APS) / sizeof(FLUG_WIFI_APS[0]));

static char          g_net_ssid[33] = "Rakuten-4CE2";
static char          g_net_pass[65] = "EYA5EVQWC2U4GDA9";
static unsigned long g_net_last_try = 0;
static bool          g_net_ever = false;
static int           g_net_last_ok = -1;     // -1 never tried, 0 failed, 1 ok
static char          g_net_last_ip[20] = "";

static void net_store() {
    Preferences p;
    if (p.begin("wifi", false)) {
        p.putString("ssid", g_net_ssid);
        p.putString("pass", g_net_pass);
        p.end();
    }
}

static void net_load() {
    Preferences p;
    if (p.begin("wifi", true)) {
        p.getString("ssid", g_net_ssid, sizeof(g_net_ssid));
        p.getString("pass", g_net_pass, sizeof(g_net_pass));
        p.end();
    }
    if (!g_net_ssid[0] || strcmp(g_net_ssid, "GlobalWiFi_kdq") == 0) {
        strncpy(g_net_ssid, FLUG_WIFI_APS[0].ssid, sizeof(g_net_ssid) - 1);
        strncpy(g_net_pass, FLUG_WIFI_APS[0].pass, sizeof(g_net_pass) - 1);
        net_store();
    }
}

// RSSI of a given SSID if the passive scan has seen it, else 0.
static int net_seen_ssid_rssi(const char* ssid) {
    if (!ssid || !ssid[0]) return 0;
    for (int a = 0; a < (int)g_medusa_ap_count; a++)
        if (g_medusa_aps[a].used && strcmp(g_medusa_aps[a].ssid, ssid) == 0) return g_medusa_aps[a].rssi;
    return 0;
}

static int net_seen_rssi() {
    int r = net_seen_ssid_rssi(g_net_ssid);
    if (r != 0) return r;
    for (int i = 0; i < FLUG_NUM_WIFI_APS; i++) {
        r = net_seen_ssid_rssi(FLUG_WIFI_APS[i].ssid);
        if (r != 0) return r;
    }
    return 0;
}

static void net_emit_status(const char* note) {
    Serial.printf("# WIFI|{\"node\":\"%s\",\"configured\":%d,\"ssid\":\"%s\",\"last\":%d,\"ip\":\"%s\",\"seen_rssi\":%d,"
                  "\"mesh_ch\":%d,\"clock_ok\":%d,\"hour\":%u,\"note\":\"%s\"}\n",
                  g_medusa_node, g_net_ssid[0] ? 1 : 0, g_net_ssid, g_net_last_ok, g_net_last_ip,
                  net_seen_rssi(), (int)g_s3_sun_ch, g_lrn_clock_ok ? 1 : 0, (unsigned)g_lrn_hour, note);
}

static bool net_try_join_one(const char* ssid, const char* pass, unsigned long wait_ms) {
    if (!ssid || !ssid[0]) return false;
    esp_wifi_set_promiscuous(false);
    WiFi.begin(ssid, (pass && pass[0]) ? pass : NULL);
    unsigned long t0 = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - t0 < wait_ms) delay(60);
    if (WiFi.status() == WL_CONNECTED) {
        strncpy(g_net_ssid, ssid, sizeof(g_net_ssid) - 1);
        strncpy(g_net_pass, pass ? pass : "", sizeof(g_net_pass) - 1);
        int ch = (int)WiFi.channel();
        if (ch >= 1 && ch <= 13) g_s3_sun_ch = ch;
        return true;
    }
    WiFi.disconnect(false);
    return false;
}

static bool net_connect_best() {
    // 1. First try any known AP from FLUG_WIFI_APS that our passive 802.11 scanner has already heard!
    for (int i = 0; i < FLUG_NUM_WIFI_APS; i++) {
        if (net_seen_ssid_rssi(FLUG_WIFI_APS[i].ssid) != 0) {
            if (net_try_join_one(FLUG_WIFI_APS[i].ssid, FLUG_WIFI_APS[i].pass, 7000UL)) return true;
        }
    }
    // 2. Always try primary laptop Wi-Fi (`Rakuten-4CE2`) and configured NVS SSID
    if (net_try_join_one(FLUG_WIFI_APS[0].ssid, FLUG_WIFI_APS[0].pass, 7000UL)) return true;
    if (g_net_ssid[0] && strcmp(g_net_ssid, FLUG_WIFI_APS[0].ssid) != 0) {
        if (net_try_join_one(g_net_ssid, g_net_pass, 6000UL)) return true;
    }
    return false;
}

static void net_sync() {
    g_net_last_try = millis();
    g_net_ever = true;

    if (net_connect_best()) {
        strncpy(g_net_last_ip, WiFi.localIP().toString().c_str(), sizeof(g_net_last_ip) - 1);
        configTime(0, 0, "pool.ntp.org", "time.google.com", "time.cloudflare.com");
        time_t now = 0;
        unsigned long t1 = millis();
        while ((now = time(nullptr)) < 1700000000 && millis() - t1 < 6000UL) delay(80);
        if (now >= 1700000000) {
            learn_clock_set((long long)now);
            g_net_last_ok = 1;
            net_emit_status("ntp ok");
        } else {
            g_net_last_ok = 1;
            net_emit_status("connected (mesh ch locked)");
        }
    } else {
        g_net_last_ok = 0;
        g_net_last_ip[0] = '\0';
        net_emit_status("offline mesh mode");
    }
    WiFi.disconnect(false);
    // restore mesh channel + promiscuous capture
    esp_wifi_set_promiscuous(true);
    esp_wifi_set_channel((int)g_s3_sun_ch, WIFI_SECOND_CHAN_NONE);
    g_s3_channel = (int)g_s3_sun_ch;
}

// ---- push a learned sentence to the Mac/Laptop over the hotspot (HOST set by the bridge over USB; RAM only) ----
static char     g_host_ip[20] = "";
static uint16_t g_host_port   = 0;
static char     g_host_tok[40] = "";

static void net_host_set(const char* a) {   // HOST <ip> <port> <token>
    char ip[20] = "", tok[40] = "";
    unsigned port = 0;
    if (sscanf(a, " %19s %u %39s", ip, &port, tok) == 3) {
        strncpy(g_host_ip, ip, sizeof(g_host_ip) - 1);
        strncpy(g_host_tok, tok, sizeof(g_host_tok) - 1);
        g_host_port = (uint16_t)port;
    }
}

static void net_push_say(const char* say, uint32_t seq) {
    if (!g_net_ssid[0] || !g_host_ip[0] || !g_host_port) return;
    int code = -1;
    if (net_connect_best()) {
        HTTPClient h;
        String url = String("http://") + g_host_ip + ":" + String(g_host_port) + "/board/say";
        if (h.begin(url)) {
            h.setTimeout(4000);
            h.addHeader("Content-Type", "application/json");
            h.addHeader("X-Triad-Token", g_host_tok);
            String body = String("{\"node\":\"") + g_medusa_node + "\",\"seq\":" + String((unsigned long)seq) + ",\"say\":\"" + say + "\"}";
            code = h.POST(body);
            h.end();
        }
    }
    WiFi.disconnect(false);
    esp_wifi_set_promiscuous(true);
    esp_wifi_set_channel((int)g_s3_sun_ch, WIFI_SECOND_CHAN_NONE);
    g_s3_channel = (int)g_s3_sun_ch;
    Serial.printf("# PUSH|{\"node\":\"%s\",\"host\":\"%s:%u\",\"http\":%d}\n", g_medusa_node, g_host_ip, (unsigned)g_host_port, code);
}

// `WIFI`, `WIFI SYNC`, `WIFI CLEAR`, `WIFI <ssid> <password>`
static void net_command(const char* a) {
    while (*a == ' ') a++;
    if (!*a) { net_emit_status("status"); return; }
    if (strcasecmp(a, "SYNC") == 0) { net_sync(); return; }
    if (strcasecmp(a, "CLEAR") == 0) { g_net_ssid[0] = g_net_pass[0] = '\0'; net_store(); net_emit_status("cleared"); return; }
    const char* sp = strchr(a, ' ');
    size_t n = sp ? (size_t)(sp - a) : strlen(a);
    if (n >= sizeof(g_net_ssid)) n = sizeof(g_net_ssid) - 1;
    memcpy(g_net_ssid, a, n);
    g_net_ssid[n] = '\0';
    g_net_pass[0] = '\0';
    if (sp) { while (*sp == ' ') sp++; strncpy(g_net_pass, sp, sizeof(g_net_pass) - 1); g_net_pass[sizeof(g_net_pass) - 1] = '\0'; }
    net_store();
    net_sync();
}

static void net_tick(unsigned long now) {
    if (now < 6000UL) return;
    if (!g_net_ever) {
        net_sync();
        return;
    }
    // Retry every 45s if offline and passive scan hears a known hotspot, else re-sync every 6h
    if (g_net_last_ok != 1 && net_seen_rssi() != 0 && (now - g_net_last_try > 45000UL)) {
        net_sync();
    } else if (now - g_net_last_try > 6UL * 3600UL * 1000UL) {
        net_sync();
    }
}

#endif /* FLUGOS_NET_TIME_H */
