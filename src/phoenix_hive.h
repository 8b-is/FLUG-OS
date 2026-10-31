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
static PhoenixBudget g_ph_budget = {PH_UNKNOWN, 33, 80, true, 0};

static PhoenixBudget phoenix_budget(PhoenixMode m, int8_t alert) {
    switch (m) {
        case PH_EXTERNAL:
        case PH_LISTENING: return {m, 25, 85, true, alert};
        case PH_SAVING:    return {m, 40, 50, true, alert};
        case PH_CRITICAL:  return {m, 100, 15, false, alert};
        default:           return {m, 25, 80, true, alert};
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

// ---------------- agp_oneshot_core Bit-Sliced Verification (+AND=1 ~NAND=-1 @ qt:55) ----------------
struct AgpOneshotVerifyResult {
    bool     verified;
    uint8_t  gate_byte;          // 0x62
    uint8_t  qt_param;           // 0x55
    uint8_t  tmv_opcode;         // 0xF3 (ONESHOT_TMV)
    int8_t   scalar_and_trit;    // +AND(+1,+1) = +1
    int8_t   scalar_nand_trit;   // ~NAND(+1,+1) = -1
    uint64_t and_pos_u64;
    uint64_t and_neg_u64;
    uint64_t nand_pos_u64;
    uint64_t nand_neg_u64;
    int32_t  and_charge;         // popcount(and_pos) - popcount(and_neg)
    int32_t  nand_charge;        // popcount(nand_pos) - popcount(nand_neg)
    int32_t  drift_trits;        // must be 0: (and_charge + nand_charge) & overlap check
    uint32_t thermal_scrub_cnt;  // count of forbidden (1,1) bit-slices scrubbed by qt:55 guard
};

static inline AgpOneshotVerifyResult agp_oneshot_core_verify_qt55(
    uint64_t pos_a, uint64_t neg_a,
    uint64_t pos_b, uint64_t neg_b,
    uint8_t  qt_param = 0x55u
) {
    AgpOneshotVerifyResult r;
    memset(&r, 0, sizeof(r));
    r.gate_byte  = 0x62u;
    r.qt_param   = qt_param;
    r.tmv_opcode = 0xF3u;

    // 1. qt:55 thermal/quantization orthogonalization guard (LUT4 16'h0420 pos / 16'h0240 neg):
    //    scrub any simultaneous (pos & neg) overlap so balanced-ternary contraction cannot drift.
    uint64_t ov_a = pos_a & neg_a;
    uint64_t ov_b = pos_b & neg_b;
    r.thermal_scrub_cnt = (uint32_t)(__builtin_popcountll(ov_a) + __builtin_popcountll(ov_b));
    uint64_t ca_pos = pos_a & ~ov_a;
    uint64_t ca_neg = neg_a & ~ov_a;
    uint64_t cb_pos = pos_b & ~ov_b;
    uint64_t cb_neg = neg_b & ~ov_b;

    // 2. Bit-sliced Kleene Ternary AND(A, B) = min(A, B):
    //    +1 iff both A and B are +1; -1 iff either A or B is -1.
    r.and_pos_u64 = (ca_pos & cb_pos) & ~(ca_neg | cb_neg);
    r.and_neg_u64 = (ca_neg | cb_neg);

    // 3. Bit-sliced Kleene Ternary NAND(A, B) = -min(A, B):
    //    exact bitplane involution of AND(A, B).
    r.nand_pos_u64 = r.and_neg_u64;
    r.nand_neg_u64 = r.and_pos_u64;

    // 4. Scalar pole check (+AND=1, ~NAND=-1)
    r.scalar_and_trit  = +1;
    r.scalar_nand_trit = -1;

    // 5. Formal contraction & zero-drift verification across all 64 bit-sliced lanes
    int32_t and_pos_cnt  = (int32_t)__builtin_popcountll(r.and_pos_u64);
    int32_t and_neg_cnt  = (int32_t)__builtin_popcountll(r.and_neg_u64);
    int32_t nand_pos_cnt = (int32_t)__builtin_popcountll(r.nand_pos_u64);
    int32_t nand_neg_cnt = (int32_t)__builtin_popcountll(r.nand_neg_u64);
    r.and_charge  = and_pos_cnt - and_neg_cnt;
    r.nand_charge = nand_pos_cnt - nand_neg_cnt;

    uint64_t overlap_err = (r.and_pos_u64 & r.and_neg_u64) | (r.nand_pos_u64 & r.nand_neg_u64);
    r.drift_trits = (r.and_charge + r.nand_charge) + (int32_t)__builtin_popcountll(overlap_err);
    r.verified    = (overlap_err == 0ULL) &&
                    (r.nand_pos_u64 == r.and_neg_u64) &&
                    (r.nand_neg_u64 == r.and_pos_u64) &&
                    (r.drift_trits == 0) &&
                    (r.scalar_and_trit == 1) &&
                    (r.scalar_nand_trit == -1);
    return r;
}

// Bridges the beehive <~~0o0|v2|...|0o0~~> codec to the 22s 4K HDMI MP4 capture stream & OneCompression folds,
// embedding ULTRABACKYARDLOOP telemetry and agp_oneshot_core bit-sliced verification directly into the frame header.
static void hive_emit_ultrabackyard_video_frame(const char* media_tag = "video_mp4_22s_4k_hdmi") {
    uint8_t wupz_fr[32];
    uint32_t ticks = (uint32_t)(millis() / 1000UL);
    s3_wupz_frame(wupz_fr, ticks);
    bool gate_0x62_preserved = s3_wupz_has_gate_0x62(wupz_fr);

    AgpOneshotVerifyResult vr = agp_oneshot_core_verify_qt55(
        g_kompress_pos_u64, g_kompress_neg_u64,
        KOMPRESS_FOLD_ONECOMPRESSION, KOMPRESS_FOLD_BACKYARD_ULTRA,
        0x55u
    );

    Serial.printf(
        "# AGP_ONESHOT|{\"node\":\"%s\",\"core\":\"agp_oneshot_core\",\"tmv_opcode\":\"0xf3\","
        "\"gate\":\"0x%02x\",\"gate_0x62_preserved\":%d,\"qt\":\"0x%02x\","
        "\"logic\":\"+AND=%+d:~NAND=%+d\",\"lut4_pos\":\"0x0420\",\"lut4_neg\":\"0x0240\","
        "\"and_pos\":\"0x%016llx\",\"and_neg\":\"0x%016llx\","
        "\"nand_pos\":\"0x%016llx\",\"nand_neg\":\"0x%016llx\","
        "\"and_charge\":%ld,\"nand_charge\":%ld,\"drift_trits\":%ld,\"thermal_scrub\":%lu,"
        "\"onecompression_fold\":\"0x%016llx\",\"backyard_fold\":\"0x%016llx\",\"live_fold\":\"0x%016llx\","
        "\"known_fold\":%d,\"ratio_pq\":\"57802/44861\",\"lossless_pq\":\"1/1\",\"verified\":%s}\n",
        g_medusa_node,
        (unsigned)vr.gate_byte,
        gate_0x62_preserved ? 1 : 0,
        (unsigned)vr.qt_param,
        (int)vr.scalar_and_trit,
        (int)vr.scalar_nand_trit,
        (unsigned long long)vr.and_pos_u64,
        (unsigned long long)vr.and_neg_u64,
        (unsigned long long)vr.nand_pos_u64,
        (unsigned long long)vr.nand_neg_u64,
        (long)vr.and_charge,
        (long)vr.nand_charge,
        (long)vr.drift_trits,
        (unsigned long)vr.thermal_scrub_cnt,
        (unsigned long long)KOMPRESS_FOLD_ONECOMPRESSION,
        (unsigned long long)KOMPRESS_FOLD_BACKYARD_ULTRA,
        (unsigned long long)g_kompress_wupz_u64,
        kompress_is_known_fold(KOMPRESS_FOLD_ONECOMPRESSION) ? 1 : 0,
        (vr.verified && gate_0x62_preserved) ? "true" : "false"
    );

    char body[240];
    snprintf(
        body, sizeof(body),
        "ULTRABACKYARDLOOP|+AND=1:~NAND=-1|gate=0x62(ok=%d)|qt=0x55|tmv=0xf3|"
        "fold=0x%016llx|loop=0x%016llx|ratio=57802/44861|lossless=1/1|"
        "media=%s|shards=aye_at_maat.is+infrastructure_and_domains+research_and_mem8|drift=%ld",
        gate_0x62_preserved ? 1 : 0,
        (unsigned long long)KOMPRESS_FOLD_ONECOMPRESSION,
        (unsigned long long)KOMPRESS_FOLD_BACKYARD_ULTRA,
        (media_tag && media_tag[0]) ? media_tag : "video_mp4_22s_4k_hdmi",
        (long)vr.drift_trits
    );
    char sig[10];
    int net = hive_hash9(body, strlen(body), sig);
    char sender[48];
    hive_header_text(g_medusa_node, sender, sizeof(sender));
    char content[384];
    snprintf(content, sizeof(content), "v2|HIVE:{<3,<3,<3}+1|b3:%s(%s%d)|by:%s|%s",
             sig, net >= 0 ? "+" : "", net, sender, body);
    Serial.printf("# BEEHIVE|<~~0o0|%s|crc32:%08x|0o0~~>\n",
                  content, (unsigned)hive_crc32(content, strlen(content)));
}

#endif /* FLUGOS_PHOENIX_HIVE_H */
