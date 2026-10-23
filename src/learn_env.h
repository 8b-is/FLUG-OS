// learn_env.h — on-device online learning from the environment, and speech about what it learned.
//
// Every 5 s the board samples 6 features (Wi-Fi networks seen, channel entropy, mean RSSI, IMU motion,
// triad peers online, packet rate) and keeps a running mean/variance per hour-of-day bucket (24 + 1 unknown).
// When a feature is >3 sigma from what is normal *for this hour*, the board forms a sentence, speaks it on-chip
// (ES8311 formant TTS, 120 s cooldown) and publishes it as `# LEARN|{...}`; the host bridge reads it and speaks it
// again with Kokoro. Statistics persist in NVS (survive reboot). No cloud, no labels, no Python.
#ifndef FLUGOS_LEARN_ENV_H
#define FLUGOS_LEARN_ENV_H

#include <Preferences.h>
#include <math.h>

#define LRN_FEATS 6
#define LRN_BUCKETS 25
struct LrnStat { float mean; float var; uint16_t n; uint16_t pad; };

static LrnStat  g_lrn[LRN_BUCKETS][LRN_FEATS];
static uint8_t  g_lrn_hour      = 24;      // 24 = clock unknown (host sends TIME)
static bool     g_lrn_clock_ok  = false;
static long long g_clk_epoch0   = 0;
static unsigned long g_clk_ms0  = 0;
static long     g_clk_tz        = 9 * 3600;   // default JST until the host/NVS says otherwise
static void learn_update_hour(unsigned long now);
static bool     g_lrn_speak     = true;
static char     g_lrn_say[112]  = "";
static uint32_t g_lrn_say_seq   = 0;
static float    g_lrn_x[LRN_FEATS] = {0}, g_lrn_z[LRN_FEATS] = {0};
static uint32_t g_lrn_total     = 0;
static unsigned long g_lrn_last_tick = 0, g_lrn_last_save = 0, g_lrn_last_speak = 0;
static unsigned long g_lrn_feat_cool[LRN_FEATS] = {0};
static uint32_t g_lrn_prev_pkts = 0;
static bool     g_lrn_ready_said[LRN_BUCKETS] = {false};
static uint32_t g_lrn_corpus_total = 0, g_lrn_corpus_wifi = 0, g_lrn_corpus_ble = 0, g_lrn_corpus_bt = 0, g_lrn_corpus_lte = 0;

static const char* const LRN_NAME[LRN_FEATS] = {"networks", "entropy", "rssi", "motion", "peers", "traffic"};
static const float LRN_SD_FLOOR[LRN_FEATS]   = {1.5f, 0.15f, 3.0f, 0.08f, 0.5f, 20.0f};

static void learn_load() {
    Preferences p;
    if (p.begin("learn", true)) {
        if (p.getBytesLength("ls") == sizeof(g_lrn)) p.getBytes("ls", g_lrn, sizeof(g_lrn));
        g_lrn_speak = p.getBool("spk", true);
        g_clk_tz = p.getLong("tz", g_clk_tz);
        g_lrn_corpus_total = p.getUInt("ctot", 0);
        g_lrn_corpus_wifi  = p.getUInt("cwifi", 0);
        g_lrn_corpus_ble   = p.getUInt("cble", 0);
        g_lrn_corpus_bt    = p.getUInt("cbt", 0);
        g_lrn_corpus_lte   = p.getUInt("clte", 0);
        p.end();
    }
    for (int b = 0; b < LRN_BUCKETS; b++) for (int i = 0; i < LRN_FEATS; i++) g_lrn_total += g_lrn[b][i].n;
    g_lrn_total /= LRN_FEATS;
}

static void learn_save() {
    Preferences p;
    if (p.begin("learn", false)) {
        p.putBytes("ls", g_lrn, sizeof(g_lrn));
        p.putBool("spk", g_lrn_speak);
        p.putUInt("ctot", g_lrn_corpus_total);
        p.putUInt("cwifi", g_lrn_corpus_wifi);
        p.putUInt("cble", g_lrn_corpus_ble);
        p.putUInt("cbt", g_lrn_corpus_bt);
        p.putUInt("clte", g_lrn_corpus_lte);
        p.end();
    }
}

static void net_push_say(const char* say, uint32_t seq);
static void learn_set_say(const char* msg, int feat, float z) {
    strncpy(g_lrn_say, msg, sizeof(g_lrn_say) - 1);
    g_lrn_say[sizeof(g_lrn_say) - 1] = '\0';
    g_lrn_say_seq++;
    Serial.printf("# LEARN|{\"node\":\"%s\",\"seq\":%lu,\"feature\":\"%s\",\"z\":%.2f,\"hour\":%u,\"say\":\"%s\"}\n",
                  g_medusa_node, (unsigned long)g_lrn_say_seq, feat >= 0 ? LRN_NAME[feat] : "milestone",
                  z, (unsigned)g_lrn_hour, g_lrn_say);
    net_push_say(g_lrn_say, g_lrn_say_seq);   // Mac speaks it (Kokoro) while the board speaks on-chip
    unsigned long now = millis();
    if (g_lrn_speak && (g_lrn_last_speak == 0 || now - g_lrn_last_speak >= 120000UL)) {
        g_lrn_last_speak = now;
        s3_es8311_speak(g_lrn_say, g_medusa_freq_hz, g_triad_self_pole);
    }
}

static void learn_tick(unsigned long now) {
    learn_update_hour(now);
    if (now - g_lrn_last_tick < 5000) return;
    float dt = (now - g_lrn_last_tick) / 1000.0f;
    g_lrn_last_tick = now;

    uint32_t pk = g_s3_total_pkts;
    float rate = (g_lrn_prev_pkts == 0 || pk < g_lrn_prev_pkts) ? 0.0f : (float)(pk - g_lrn_prev_pkts) / dt;
    g_lrn_prev_pkts = pk;
    float motion = sqrtf(g_imu.ax * g_imu.ax + g_imu.ay * g_imu.ay + (g_imu.az - 1.0f) * (g_imu.az - 1.0f));
    g_lrn_x[0] = (float)g_medusa_ap_count;
    g_lrn_x[1] = g_medusa_entropy;
    g_lrn_x[2] = (float)g_medusa_mean_rssi;
    g_lrn_x[3] = motion;
    g_lrn_x[4] = (float)s3_triad_online_count();
    g_lrn_x[5] = rate;

    LrnStat* row = g_lrn[g_lrn_hour < 24 ? g_lrn_hour : 24];
    int worst = -1;
    float worst_z = 0;
    for (int i = 0; i < LRN_FEATS; i++) {
        LrnStat& s = row[i];
        float sd = sqrtf(s.var);
        if (sd < LRN_SD_FLOOR[i]) sd = LRN_SD_FLOOR[i];
        float z = (s.n >= 30) ? (g_lrn_x[i] - s.mean) / sd : 0.0f;
        g_lrn_z[i] = z;
        if (fabsf(z) > 3.0f && fabsf(z) > fabsf(worst_z) && (now - g_lrn_feat_cool[i] > 90000UL || g_lrn_feat_cool[i] == 0)) {
            worst = i;
            worst_z = z;
        }
        // online update (Welford-style EMA; 1/n early, floor 1/200 so it keeps adapting)
        if (s.n < 65535) s.n++;
        float a = 1.0f / (float)(s.n < 200 ? s.n : 200);
        float d = g_lrn_x[i] - s.mean;
        s.mean += a * d;
        s.var = (1.0f - a) * (s.var + a * d * d);
    }
    g_lrn_total++;

    int bidx = g_lrn_hour < 24 ? g_lrn_hour : 24;
    if (!g_lrn_ready_said[bidx] && row[0].n == 30) {
        g_lrn_ready_said[bidx] = true;
        learn_set_say(g_lrn_hour < 24 ? "I have learned what this hour usually looks like."
                                       : "I have learned my first baseline of this place.", -1, 0);
        return;
    }
    if (worst >= 0) {
        g_lrn_feat_cool[worst] = now ? now : 1;
        char m[112];
        const LrnStat& s = row[worst];
        bool up = worst_z > 0;
        switch (worst) {
            case 0: snprintf(m, sizeof(m), "%s networks than usual. %d now, usually %d.", up ? "More" : "Fewer", (int)g_lrn_x[0], (int)(s.mean + 0.5f)); break;
            case 1: snprintf(m, sizeof(m), "The radio spectrum is %s than usual.", up ? "busier" : "calmer"); break;
            case 2: snprintf(m, sizeof(m), "Signals are %s than usual.", up ? "stronger" : "weaker"); break;
            case 3: snprintf(m, sizeof(m), up ? "I am being moved." : "I am very still."); break;
            case 4: snprintf(m, sizeof(m), "%d of 3 triad friends are here.", (int)g_lrn_x[4]); break;
            default: snprintf(m, sizeof(m), up ? "Radio traffic is spiking." : "The air has gone quiet."); break;
        }
        learn_set_say(m, worst, worst_z);
    }
    if (now - g_lrn_last_save > 60000UL) { g_lrn_last_save = now; learn_save(); }
}

static void learn_emit_status() {
    const LrnStat* row = g_lrn[g_lrn_hour < 24 ? g_lrn_hour : 24];
    Serial.printf("# LEARN|{\"node\":\"%s\",\"status\":1,\"seq\":%lu,\"say\":\"%s\",\"hour\":%u,\"speak\":%d,\"samples\":%lu,\"bucket_n\":%u,"
                  "\"corpus\":{\"total\":%lu,\"wifi\":%lu,\"ble\":%lu,\"bt\":%lu,\"lte\":%lu},"
                  "\"x\":[%.2f,%.2f,%.1f,%.2f,%.0f,%.1f],\"z\":[%.2f,%.2f,%.2f,%.2f,%.2f,%.2f],"
                  "\"mean\":[%.2f,%.2f,%.1f,%.2f,%.2f,%.1f]}\n",
                  g_medusa_node, (unsigned long)g_lrn_say_seq, g_lrn_say, (unsigned)g_lrn_hour, g_lrn_speak ? 1 : 0,
                  (unsigned long)g_lrn_total, (unsigned)row[0].n,
                  (unsigned long)g_lrn_corpus_total, (unsigned long)g_lrn_corpus_wifi,
                  (unsigned long)g_lrn_corpus_ble, (unsigned long)g_lrn_corpus_bt, (unsigned long)g_lrn_corpus_lte,
                  g_lrn_x[0], g_lrn_x[1], g_lrn_x[2], g_lrn_x[3], g_lrn_x[4], g_lrn_x[5],
                  g_lrn_z[0], g_lrn_z[1], g_lrn_z[2], g_lrn_z[3], g_lrn_z[4], g_lrn_z[5],
                  row[0].mean, row[1].mean, row[2].mean, row[3].mean, row[4].mean, row[5].mean);
}

// CORPUS <total> <wifi> <ble> <bt> <lte> -> stores imported WiGLE corpus baseline in NVS
static void learn_set_corpus(const char* args) {
    unsigned long tot = 0, wf = 0, bl = 0, bt = 0, lt = 0;
    sscanf(args, "%lu %lu %lu %lu %lu", &tot, &wf, &bl, &bt, &lt);
    bool changed = (tot != g_lrn_corpus_total);
    g_lrn_corpus_total = (uint32_t)tot;
    g_lrn_corpus_wifi  = (uint32_t)wf;
    g_lrn_corpus_ble   = (uint32_t)bl;
    g_lrn_corpus_bt    = (uint32_t)bt;
    g_lrn_corpus_lte   = (uint32_t)lt;
    learn_save();
    if (changed && tot > 0) {
        char m[112];
        snprintf(m, sizeof(m), "I learned %lu known Wi-Fi and %lu Bluetooth endpoints from WiGLE.", wf, bl + bt);
        learn_set_say(m, -1, 0);
    }
    learn_emit_status();
}

// TIME <epoch_s> <tz_offset_s>  -> selects the hour-of-day bucket
static void learn_update_hour(unsigned long now) {
    if (!g_lrn_clock_ok) return;
    long long local = g_clk_epoch0 + (long long)((now - g_clk_ms0) / 1000UL) + g_clk_tz;
    g_lrn_hour = (uint8_t)(((local / 3600) % 24 + 24) % 24);
}

static void learn_clock_set(long long epoch) {
    g_clk_epoch0 = epoch;
    g_clk_ms0 = millis();
    g_lrn_clock_ok = true;
    learn_update_hour(g_clk_ms0);
}

// TIME <epoch_s> <tz_offset_s>  -> sets the clock + selects the hour-of-day bucket
static void learn_set_time(const char* args) {
    const char* sp = strchr(args, ' ');
    if (sp) {
        long tz = atol(sp + 1);
        if (tz != g_clk_tz) {
            g_clk_tz = tz;
            Preferences p;
            if (p.begin("learn", false)) { p.putLong("tz", tz); p.end(); }
        }
    }
    learn_clock_set(atoll(args));
}

#endif /* FLUGOS_LEARN_ENV_H */
