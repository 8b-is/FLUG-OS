// phoenix_hive.h — two recent 8b-is projects, ported into FLUG-OS:
//   * oxi-phoenix/baye-rust/src/power_policy.rs  -> PhoenixPolicy (battery budget, hysteresis, one-shot alerts)
//   * beehive/web/index.html (Kompress v2 frame)  -> <~~0o0| ... |crc32:..|0o0~~> frames from LIVE triad presence
// Included from s3_amoled_app.h after g_power / g_triad_nodes exist.
#ifndef FLUGOS_PHOENIX_HIVE_H
#define FLUGOS_PHOENIX_HIVE_H

// ---------------- oxi-phoenix power policy ----------------
enum PhoenixMode : uint8_t { PH_UNKNOWN = 0, PH_EXTERNAL, PH_LISTENING, PH_SAVING, PH_CRITICAL };
static const char* const PHOENIX_MODE_NAME[5] = {"Unknown", "ExternalPower", "Listening", "Saving", "Critical"};

struct PhoenixBudget {
    PhoenixMode mode;
    uint16_t    frame_ms;
    uint8_t     brightness_pct;
    bool        allow_rf_scan;
    int8_t      alert;  // 0 none, 1 low, 2 critical
};

static PhoenixMode g_ph_mode = PH_UNKNOWN, g_ph_cand = PH_UNKNOWN;
static uint64_t    g_ph_cand_since = 0, g_ph_last = 0;
static bool        g_ph_have_last = false, g_ph_warn_low = false, g_ph_warn_crit = false;
static PhoenixBudget g_ph_budget = {PH_UNKNOWN, 200, 30, false, 0};

static PhoenixBudget phoenix_budget(PhoenixMode m, int8_t alert) {
    switch (m) {
        case PH_EXTERNAL:
        case PH_LISTENING: return {m, 100, 80, true, alert};
        case PH_SAVING:    return {m, 200, 30, false, alert};
        case PH_CRITICAL:  return {m, 1000, 8, false, alert};
        default:           return {m, 200, 30, false, alert};
    }
}

// Call at ~1 Hz with monotonic ms. Missing data conserves power; never invents an empty battery.
static PhoenixBudget phoenix_update(uint64_t now_ms) {
    const bool present = g_power.present;
    const bool ext     = g_power.vbus_good;
    const uint16_t mv  = g_power.batt_mv;
    const bool pct_ok  = present && g_power.batt_pct <= 100;
    const uint8_t pct  = g_power.batt_pct;

    if (g_ph_have_last && (now_ms < g_ph_last || now_ms - g_ph_last > 3000)) {
        g_ph_mode = PH_UNKNOWN;
        g_ph_cand = PH_UNKNOWN;
    }
    g_ph_last = now_ms;
    g_ph_have_last = true;

    PhoenixMode next;
    if (ext) {
        next = PH_EXTERNAL;
    } else if (present && mv >= 2500 && mv <= 4500) {
        if (mv <= 3300 || (pct_ok && pct <= 8))        next = PH_CRITICAL;
        else if (mv <= 3500 || (pct_ok && pct <= 20))  next = PH_SAVING;
        else if (mv >= 3650 && pct_ok && pct >= 25)    next = PH_LISTENING;
        else if (g_ph_mode == PH_SAVING || g_ph_mode == PH_CRITICAL) next = g_ph_mode;
        else                                           next = PH_UNKNOWN;
    } else {
        next = PH_UNKNOWN;
    }
    if (next != g_ph_cand) {
        g_ph_cand = next;
        g_ph_cand_since = now_ms;
    }
    const bool stable = now_ms - g_ph_cand_since >= 3000;
    if (next == PH_UNKNOWN || next == PH_EXTERNAL || stable) g_ph_mode = next;
    if (stable && next == PH_LISTENING) { g_ph_warn_low = false; g_ph_warn_crit = false; }

    int8_t alert = 0;
    if (g_ph_mode == PH_CRITICAL && !g_ph_warn_crit) { g_ph_warn_low = true; g_ph_warn_crit = true; alert = 2; }
    else if (g_ph_mode == PH_SAVING && !g_ph_warn_low) { g_ph_warn_low = true; alert = 1; }
    g_ph_budget = phoenix_budget(g_ph_mode, alert);
    return g_ph_budget;
}

static void phoenix_emit() {
    Serial.printf("# POWER|{\"policy\":\"oxi-phoenix\",\"mode\":\"%s\",\"frame_ms\":%u,\"brightness_pct\":%u,"
                  "\"allow_rf_scan\":%d,\"batt_mv\":%u,\"batt_pct\":%u,\"vbus\":%d,\"charging\":%d}\n",
                  PHOENIX_MODE_NAME[g_ph_budget.mode], (unsigned)g_ph_budget.frame_ms,
                  (unsigned)g_ph_budget.brightness_pct, g_ph_budget.allow_rf_scan ? 1 : 0,
                  (unsigned)g_power.batt_mv, (unsigned)g_power.batt_pct,
                  g_power.vbus_good ? 1 : 0, g_power.charging ? 1 : 0);
}

// ---------------- beehive <~~0o0 frame ----------------
static uint32_t hive_crc32(const char* s, size_t n) {
    uint32_t crc = 0xffffffffu;
    for (size_t i = 0; i < n; i++) {
        crc ^= (uint8_t)s[i];
        for (int b = 0; b < 8; b++) crc = (crc >> 1) ^ ((crc & 1) ? 0xedb88320u : 0);
    }
    return crc ^ 0xffffffffu;
}

// beehive ternaryHash9: returns signature chars (T/0/1) and net charge
static int hive_hash9(const char* s, size_t n, char sig[10]) {
    int8_t t[9] = {0};
    for (size_t i = 0; i < n; i++) {
        int b = (uint8_t)s[i];
        int idx = (int)((i + b) % 9);
        t[idx] = (int8_t)((((t[idx] + (b % 3) - 1 + 4) % 3)) - 1);
    }
    int net = 0;
    for (int i = 0; i < 9; i++) { sig[i] = t[i] < 0 ? 'T' : (t[i] > 0 ? '1' : '0'); net += t[i]; }
    sig[9] = '\0';
    return net;
}

// RFC3986 unreserved passthrough, everything else %XX (matches beehive headerText)
static void hive_header_text(const char* in, char* out, size_t cap) {
    size_t o = 0;
    for (; *in && o + 4 < cap; in++) {
        uint8_t c = (uint8_t)*in;
        if (isalnum(c) || c == '_' || c == '.' || c == '~' || c == '-') out[o++] = (char)c;
        else o += snprintf(out + o, cap - o, "%%%02X", c);
    }
    out[o] = '\0';
}

// Frame carries MEASURED ESP-NOW presence of the 3 triad nodes (not demo data).
static void hive_emit_frame() {
    char body[160];
    snprintf(body, sizeof(body), "HIVE nodes=%u/3 ada.ttl=%u vak.ttl=%u omn.ttl=%u presence=ESPNOW-MEASURED power=%s",
             (unsigned)s3_triad_online_count(), g_triad_nodes[0].ttl_q8, g_triad_nodes[1].ttl_q8,
             g_triad_nodes[2].ttl_q8, PHOENIX_MODE_NAME[g_ph_budget.mode]);
    char sig[10];
    int net = hive_hash9(body, strlen(body), sig);
    char sender[48];
    hive_header_text(g_medusa_node, sender, sizeof(sender));
    char content[320];
    snprintf(content, sizeof(content), "v2|HIVE:{<3,<3,<3}+1|b3:%s(%s%d)|by:%s|%s",
             sig, net >= 0 ? "+" : "", net, sender, body);
    Serial.printf("# BEEHIVE|<~~0o0|%s|crc32:%08x|0o0~~>\n", content, (unsigned)hive_crc32(content, strlen(content)));
}

#endif /* FLUGOS_PHOENIX_HIVE_H */
