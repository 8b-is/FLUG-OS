// net_time.h — Sovereign Tokyo Multi-AP hotspot join + NTP time + peer channel lock + HTTP OTA + ESP-NOW TWIF Wi-Fi Share + BLE.
// Prioritizes the laptop's active Wi-Fi (`Rakuten-4CE2`, Channel 1) with automatic multi-AP fallback,
// NVS persistence (`WIFI <ssid> <password>`), ESP-NOW Triad Wi-Fi sharing (`TWIF`), BLE Nordic UART provisioning,
// and HTTP OTA auto-update. Joins briefly, syncs SNTP + locks `g_s3_sun_ch` to the AP's channel, then restores
// promiscuous capture + dual-channel (Ch 1 / Ch 6) ESP-NOW mesh.
#ifndef FLUGOS_NET_TIME_H
#define FLUGOS_NET_TIME_H

#include <time.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <Update.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

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
        if (g_host_ip[0]) p.putString("host_ip", g_host_ip);
        if (g_host_port)  p.putUShort("host_port", g_host_port);
        p.putBool("ota_auto", g_ota_auto);
        p.end();
    }
}

static void net_load() {
    Preferences p;
    if (p.begin("wifi", true)) {
        p.getString("ssid", g_net_ssid, sizeof(g_net_ssid));
        p.getString("pass", g_net_pass, sizeof(g_net_pass));
        p.getString("host_ip", g_host_ip, sizeof(g_host_ip));
        uint16_t hp = p.getUShort("host_port", 8089);
        if (hp > 0) g_host_port = hp;
        g_ota_auto = p.getBool("ota_auto", true);
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

static void net_ble_notify(const char* msg) {
    if (g_ble_active && g_ble_connected && g_ble_tx_char && msg) {
        g_ble_tx_char->setValue((uint8_t*)msg, strlen(msg));
        g_ble_tx_char->notify();
    }
}

static void net_emit_status(const char* note) {
    char buf[256];
    snprintf(buf, sizeof(buf),
             "# WIFI|{\"node\":\"%s\",\"configured\":%d,\"ssid\":\"%s\",\"last\":%d,\"ip\":\"%s\",\"seen_rssi\":%d,"
             "\"mesh_ch\":%d,\"clock_ok\":%d,\"hour\":%u,\"ble\":%d,\"ota\":\"%s\",\"version\":\"" FLUGOS_VERSION_STR "\",\"note\":\"%s\"}\n",
             g_medusa_node, g_net_ssid[0] ? 1 : 0, g_net_ssid, g_net_last_ok, g_net_last_ip,
             net_seen_rssi(), (int)g_s3_sun_ch, g_lrn_clock_ok ? 1 : 0, (unsigned)g_lrn_hour,
             g_ble_active ? 1 : 0, g_ota_last_status, note);
    Serial.print(buf);
    net_ble_notify(buf);
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

// Broadcast Wi-Fi credentials (+ optional host IP & OTA trigger) across all 13 channels via ESP-NOW
// so the other 2 sibling Triad boards automatically join the same Wi-Fi without needing USB/serial!
static void net_share_wifi_espnow(bool trigger_ota = false) {
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
            delay(3);
            esp_now_send(bcast, (const uint8_t*)&cap, sizeof(cap));
        }
    }
    esp_wifi_set_channel((int)g_s3_sun_ch, WIFI_SECOND_CHAN_NONE);
    g_s3_channel = (int)g_s3_sun_ch;
    Serial.printf("# TWIF|{\"event\":\"wifi_shared\",\"from\":\"%s\",\"ssid\":\"%s\",\"ota\":%d,\"swept_channels\":13}\n",
                  g_medusa_node, g_net_ssid, trigger_ota ? 1 : 0);
}

// Called from s3_espnow_recv_cb when a "TWIF" capsule arrives from a sibling Triad board
static void net_on_espnow_twif(const uint8_t* data, int len) {
    if (!data || len < (int)sizeof(S3TriadWifiCapsule)) return;
    const S3TriadWifiCapsule* cap = (const S3TriadWifiCapsule*)data;
    if (cap->magic[0] != 'T' || cap->magic[1] != 'W' || cap->magic[2] != 'I' || cap->magic[3] != 'F') return;
    if (cap->node_id > 2 || cap->node_id == g_triad_self_id) return;

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
    if (cap->host_ip[0]) {
        strncpy(g_host_ip, cap->host_ip, sizeof(g_host_ip) - 1);
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
        // Automatic OTA version check while connected to Wi-Fi if host IP is known
        if (g_ota_auto && g_host_ip[0]) {
            net_ota_check_and_update(nullptr, false);
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
        std::string val = pCharacteristic->getValue();
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

// `WIFI`, `WIFI SYNC`, `WIFI SHARE`, `WIFI CLEAR`, `WIFI <ssid> <password>`
static void net_command(const char* a) {
    while (*a == ' ') a++;
    if (!*a) { net_emit_status("status"); return; }
    if (strcasecmp(a, "SYNC") == 0) { net_sync(); return; }
    if (strcasecmp(a, "SHARE") == 0) { net_share_wifi_espnow(false); return; }
    if (strcasecmp(a, "SHARE OTA") == 0) { net_share_wifi_espnow(true); return; }
    if (strcasecmp(a, "CLEAR") == 0) { g_net_ssid[0] = g_net_pass[0] = '\0'; net_store(); net_emit_status("cleared"); return; }
    const char* sp = strchr(a, ' ');
    size_t n = sp ? (size_t)(sp - a) : strlen(a);
    if (n >= sizeof(g_net_ssid)) n = sizeof(g_net_ssid) - 1;
    memcpy(g_net_ssid, a, n);
    g_net_ssid[n] = '\0';
    g_net_pass[0] = '\0';
    if (sp) { while (*sp == ' ') sp++; strncpy(g_net_pass, sp, sizeof(g_net_pass) - 1); g_net_pass[sizeof(g_net_pass) - 1] = '\0'; }
    net_store();
    // Automatically share the newly provisioned Wi-Fi credentials with the other 2 Triad boards over ESP-NOW!
    net_share_wifi_espnow(false);
    net_sync();
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
            net_ota_check_and_update(nullptr, true);
        }
    }
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
