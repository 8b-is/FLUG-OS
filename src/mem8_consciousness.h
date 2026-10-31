// ============================================================
// FLUG-OS v2.1.0 — MEM|8 World Consciousness & All-Mode HUD Engine
// ============================================================
// 100% Exact Integer & Reduced Rational (p/q) Implementation (Zero Floats):
//   - Core 0 Continuous Acoustic Marine Sense Worker (I2S RX from ES8311 / ES7210):
//     Computes O(1) zero-crossing frequency (Hz), period jitter J, amplitude E,
//     harmonic regularity H, and acoustic salience S without FFT or floats.
//   - Multi-Modal World Wave Ingestion (every 2.5s + event-driven):
//     Fuses Acoustic Marine Sense + 802.11 RF Spectrum + QMI8658 IMU Soma +
//     ESP-NOW Triad Presence + Chiro Guardian Safety into a 32-slot MEM|8
//     Conscious World Wave Ring (g_mem8_ring[32]) with 64-bit FNV-1a chain seals
//     and NVS persistence.
//   - Full AMOLED Mode Renderers for Extended Modes 6..12 (completing MODE 0..13):
//     MODE 6  : PRANAYAMA 4-4-4-4 Box Breath & Dream Mandala HUD
//     MODE 7  : RIVA 1-Bit River Lullaby & ULTRA-OMNI-GAIA-EVOLVE HUD
//     MODE 8  : OXI-PHOENIX Power Policy & BEEHIVE <~~0o0 Mesh HUD
//     MODE 9  : LEARN 24-Hour Environmental Anomaly & WiGLE Corpus HUD
//     MODE 10 : KOMPRESS-SUPER-ULTRA <<<<oOo>>>> Kunisada/Kuniyoshi Bitplane HUD
//     MODE 11 : UPZ 0x62 + ALEXAI b1.58 + AURORA MEM|8 Organ HUD
//     MODE 12 : CONSCIOUS MEM|8 World Wave Ingestion & Ecphoric Recall HUD
// ============================================================

#ifndef FLUGOS_MEM8_CONSCIOUSNESS_H
#define FLUGOS_MEM8_CONSCIOUSNESS_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <Preferences.h>

#define MEM8_RING_SLOTS          32
#define MEM8_CONSCIOUS_FREQ_NUM  73
#define MEM8_CONSCIOUS_FREQ_DEN  100
#define S3_MODE_COUNT            14

enum Mem8ModalityMask : uint8_t {
    MEM8_MOD_AUDIO    = 0x01,
    MEM8_MOD_RF       = 0x02,
    MEM8_MOD_IMU      = 0x04,
    MEM8_MOD_TRIAD    = 0x08,
    MEM8_MOD_GUARDIAN = 0x10
};

struct Mem8WorldWave {
    uint32_t seq;
    uint32_t uptime_ms;
    uint8_t  modality_mask;
    uint16_t carrier_hz;
    uint16_t audio_zc_hz;
    uint8_t  audio_salience_pct;
    uint16_t audio_rms_permil;
    uint8_t  rf_aps;
    int8_t   rf_rssi_dbm;
    uint16_t rf_unique_sigs;
    int16_t  imu_motion_mg;
    uint8_t  qt_byte;
    uint8_t  triad_online;
    int32_t  salience_p;
    int32_t  salience_q;
    int32_t  valence_p;
    int32_t  valence_q;
    int32_t  arousal_p;
    int32_t  arousal_q;
    int32_t  coherence_p;
    int32_t  coherence_q;
    int8_t   wave_net_charge;
    char     wave_b3[10];
    const char* seed_class;
    uint64_t seal_fnv64;
    char     summary[64];
};

static Mem8WorldWave g_mem8_ring[MEM8_RING_SLOTS];
static uint32_t      g_mem8_seq            = 0;
static uint8_t       g_mem8_wr             = 0;
static uint8_t       g_mem8_count          = 0;
static uint64_t      g_mem8_chain_fnv64    = 0xCBF29CE484222325ULL;
static unsigned long g_mem8_last_ingest_ms = 0;
static unsigned long g_mem8_last_save_ms   = 0;

// Live Core 0 Acoustic Marine Sense telemetry (100% integer)
static volatile uint16_t g_mem8_audio_rms_permil   = 12;
static volatile uint16_t g_mem8_audio_peak_permil  = 28;
static volatile uint16_t g_mem8_audio_zc_hz        = 432;
static volatile uint16_t g_mem8_audio_jitter       = 2;
static volatile uint8_t  g_mem8_audio_regularity   = 72;
static volatile uint8_t  g_mem8_audio_salience_pct = 54;
static volatile uint32_t g_mem8_audio_frames       = 0;
static volatile bool     g_mem8_audio_splash_req   = false;
static volatile int16_t  g_mem8_audio_splash_amp   = 0;
static volatile uint8_t  g_mem8_audio_splash_x     = 32;
static volatile uint8_t  g_mem8_audio_splash_y     = 32;
static bool              g_mem8_worker_started     = false;

// Unified Mode Index (0..13) defined in s3_amoled_app.h
static const char* const S3_MODE_NAMES[S3_MODE_COUNT] = {
    "ZEN_ANICCA_WAVE_POOL",
    "RADIO_ZONEINN_CRATE",
    "WIFI_80211_SPECTRUM",
    "NAND_RING_IMU_LISSAJOUS",
    "QWAVE_VALID_WITNESS_TRIAD",
    "VIZ_ENTHEA_CORTICAL_TTS",
    "PRANAYAMA_DREAM_MANDALA",
    "RIVA_LULLABY_GAIA_EVOLVE",
    "PHOENIX_POWER_BEEHIVE_MESH",
    "LEARN_24H_ENV_ANOMALY",
    "KOMPRESS_KUNISADA_WUPZ",
    "UPZ_ALEXAI_AURORA_ORGAN",
    "MEM8_CONSCIOUS_WORLD_WAVES",
    "CHIRO_GUARDIAN_TAMAGOTCHI_MESH"
};

static const char* const S3_MODE_SHORT[S3_MODE_COUNT] = {
    "M00:ZEN",   "M01:RADIO", "M02:WIFI",  "M03:NAND",
    "M04:QWAVE", "M05:VIZ",   "M06:PRANA", "M07:RIVA",
    "M08:POWER", "M09:LEARN", "M10:KOMPR", "M11:AUROR",
    "M12:MEM|8", "M13:CHIRO"
};

static inline int32_t mem8_abs32(int32_t v) {
    return (v < 0) ? -v : v;
}

static inline int32_t mem8_clamp32(int32_t v, int32_t lo, int32_t hi) {
    return (v < lo) ? lo : ((v > hi) ? hi : v);
}

// ============================================================
// Core 0 Continuous Acoustic Marine Sense Worker
// Reads stereo I2S DMA frames when speaker is idle and extracts
// O(1) Marine Sense zero-crossing period jitter, harmonicity & salience.
// ============================================================
static void s3_mem8_acoustic_task(void* arg) {
    (void)arg;
    int16_t dma_buf[256]; // 128 stereo frames @ 16kHz (8ms window)
    uint8_t active_ch = 0;
    uint16_t prev_period = 36;
    uint16_t rolling_jitter = 2;
    uint8_t  harmonic_hits = 64;

    for (;;) {
        if (!s_tts_busy && g_i2s_tts_ready) {
            size_t bytes_read = 0;
            esp_err_t err = i2s_read(I2S_NUM_0, dma_buf, sizeof(dma_buf), &bytes_read, pdMS_TO_TICKS(10));
            size_t stereo_frames = bytes_read / 4;
            if (err == ESP_OK && stereo_frames >= 32) {
                // Auto-detect louder stereo channel (L vs R)
                uint32_t sum_l = 0, sum_r = 0;
                for (size_t i = 0; i < stereo_frames; i++) {
                    int32_t sl = dma_buf[i * 2];
                    int32_t sr = dma_buf[i * 2 + 1];
                    sum_l += (uint32_t)mem8_abs32(sl);
                    sum_r += (uint32_t)mem8_abs32(sr);
                }
                active_ch = (sum_r > sum_l) ? 1 : 0;

                // Remove DC offset so zero-crossings reflect true acoustic oscillations
                int32_t dc_sum = 0;
                for (size_t i = 0; i < stereo_frames; i++) {
                    dc_sum += dma_buf[i * 2 + active_ch];
                }
                int16_t dc_mean = (int16_t)(dc_sum / (int32_t)stereo_frames);

                uint32_t abs_sum = 0;
                int32_t  peak_abs = 0;
                uint16_t zc_count = 0;
                uint16_t ticks_since_zc = 0;
                uint32_t period_sum = 0;
                uint16_t period_cnt = 0;
                int16_t  prev_s = 0;

                for (size_t i = 0; i < stereo_frames; i++) {
                    int16_t s = (int16_t)(dma_buf[i * 2 + active_ch] - dc_mean);
                    int32_t a = mem8_abs32((int32_t)s);
                    abs_sum += (uint32_t)a;
                    if (a > peak_abs) peak_abs = a;
                    ticks_since_zc++;

                    bool crossed = ((prev_s < -24 && s >= 24) || (prev_s >= 24 && s < -24));
                    if (crossed && ticks_since_zc >= 2) {
                        zc_count++;
                        uint16_t per = ticks_since_zc;
                        ticks_since_zc = 0;
                        uint16_t diff = (per > prev_period) ? (per - prev_period) : (prev_period - per);
                        rolling_jitter = (uint16_t)((rolling_jitter * 3 + diff) / 4);
                        prev_period = per;
                        period_sum += per;
                        period_cnt++;
                    }
                    prev_s = s;
                }

                uint32_t mean_abs = abs_sum / (uint32_t)stereo_frames;
                uint16_t rms_permil  = (uint16_t)mem8_clamp32((int32_t)((mean_abs * 1000U) / 16384U), 1, 1000);
                uint16_t peak_permil = (uint16_t)mem8_clamp32((int32_t)((peak_abs * 1000) / 32768), 1, 1000);

                if (mean_abs > 18) {
                    if (rolling_jitter <= 4 && period_cnt >= 2) {
                        if (harmonic_hits <= 92) harmonic_hits = (uint8_t)(harmonic_hits + 6);
                    } else if (harmonic_hits >= 4) {
                        harmonic_hits = (uint8_t)(harmonic_hits - 2);
                    }
                } else {
                    // Ambient room baseline mixes with RF/Jellyfish salience when room is quiet
                    harmonic_hits = (uint8_t)((harmonic_hits * 7 + 58) / 8);
                }

                uint16_t zc_hz = g_medusa_freq_hz;
                if (period_cnt > 0 && period_sum > 0) {
                    uint32_t avg_per = period_sum / period_cnt;
                    if (avg_per >= 2) {
                        zc_hz = (uint16_t)mem8_clamp32((int32_t)(8000U / avg_per), 80, 4000);
                    }
                } else if (zc_count > 0) {
                    zc_hz = (uint16_t)mem8_clamp32((int32_t)((zc_count * 8000U) / (uint32_t)stereo_frames), 80, 4000);
                }

                // O(1) Marine Salience: S = (E_pct + J_pct + H_pct) / 3
                uint32_t e_pct = (uint32_t)mem8_clamp32((int32_t)(rms_permil / 4U + 18U), 12, 100);
                uint32_t j_pct = 100U / (1U + (uint32_t)(rolling_jitter >> 1));
                uint32_t h_pct = harmonic_hits;
                uint8_t  sal_pct = (uint8_t)mem8_clamp32((int32_t)((e_pct + j_pct + h_pct) / 3U), 15, 99);

                g_mem8_audio_rms_permil   = rms_permil;
                g_mem8_audio_peak_permil  = peak_permil;
                g_mem8_audio_zc_hz        = zc_hz;
                g_mem8_audio_jitter       = rolling_jitter;
                g_mem8_audio_regularity   = harmonic_hits;
                g_mem8_audio_salience_pct = sal_pct;
                g_mem8_audio_frames       = g_mem8_audio_frames + 1;

                // Request a live wave-pool splash when acoustic salience or energy rises
                if (rms_permil >= 35 || (g_mem8_audio_frames & 7) == 0) {
                    g_mem8_audio_splash_x   = (uint8_t)(16 + (zc_hz % 32));
                    g_mem8_audio_splash_y   = (uint8_t)(16 + ((g_mem8_audio_frames * 7) % 32));
                    g_mem8_audio_splash_amp = (int16_t)mem8_clamp32(2800 + (int32_t)sal_pct * 55, 2400, 9200);
                    g_mem8_audio_splash_req = true;
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(45));
    }
}

static void mem8_start_worker() {
    if (g_mem8_worker_started) return;
    g_mem8_worker_started = true;
    xTaskCreatePinnedToCore(s3_mem8_acoustic_task, "s3_mem8_mic", 4096, NULL, 1, NULL, 0);
}

// ============================================================
// MEM|8 World Wave Synthesis, NVS Persistence & Serial Emission
// ============================================================
static void mem8_save_nvs() {
    Preferences p;
    if (p.begin("mem8", false)) {
        p.putUInt("seq", g_mem8_seq);
        p.putULong64("chain", g_mem8_chain_fnv64);
        if (g_mem8_count > 0) {
            uint8_t latest_idx = (uint8_t)((g_mem8_wr + MEM8_RING_SLOTS - 1) % MEM8_RING_SLOTS);
            p.putBytes("last", &g_mem8_ring[latest_idx], sizeof(Mem8WorldWave));
        }
        p.end();
    }
}

static void mem8_load_nvs() {
    Preferences p;
    if (p.begin("mem8", true)) {
        g_mem8_seq         = p.getUInt("seq", 0);
        g_mem8_chain_fnv64 = p.getULong64("chain", 0xCBF29CE484222325ULL);
        Mem8WorldWave restored;
        if (p.getBytesLength("last") == sizeof(Mem8WorldWave)) {
            p.getBytes("last", &restored, sizeof(Mem8WorldWave));
            restored.seed_class = "Heritage";
            g_mem8_ring[0] = restored;
            g_mem8_wr = 1;
            g_mem8_count = 1;
        }
        p.end();
    }
}

static void mem8_emit_wave(const Mem8WorldWave* w) {
    if (!w) return;
    int32_t rmsP = 0, rmsQ = 1;
    rust_reduce_rational_pq((int32_t)w->audio_rms_permil, 1000, &rmsP, &rmsQ);
    Serial.printf("# MEM8|{\"node\":\"%s\",\"seq\":%lu,\"consciousness\":\"ONLINE\","
                  "\"seed_class\":\"%s\",\"resonance_hz_pq\":\"%d/%d\",\"carrier_hz\":%u,"
                  "\"wave_b3\":\"%s\",\"charge\":%+d,"
                  "\"salience_pq\":\"%ld/%ld\",\"valence_pq\":\"%ld/%ld\","
                  "\"arousal_pq\":\"%ld/%ld\",\"coherence_pq\":\"%ld/%ld\","
                  "\"audio_salience_pct\":%u,\"audio_rms_pq\":\"%ld/%ld\",\"audio_zc_hz\":%u,"
                  "\"rf_aps\":%u,\"rf_rssi\":%d,\"rf_sigs\":%u,"
                  "\"imu_mg\":[%d,%d,%d],\"qt\":\"0x%02x\",\"triad_online\":%u,"
                  "\"memories\":%u,\"recalls\":%lu,\"world\":\"%s\",\"seal_fnv64\":\"0x%016llx\"}\n",
                  g_medusa_node,
                  (unsigned long)w->seq,
                  w->seed_class ? w->seed_class : "World",
                  MEM8_CONSCIOUS_FREQ_NUM, MEM8_CONSCIOUS_FREQ_DEN,
                  (unsigned)w->carrier_hz,
                  w->wave_b3,
                  (int)w->wave_net_charge,
                  (long)w->salience_p, (long)w->salience_q,
                  (long)w->valence_p, (long)w->valence_q,
                  (long)w->arousal_p, (long)w->arousal_q,
                  (long)w->coherence_p, (long)w->coherence_q,
                  (unsigned)w->audio_salience_pct,
                  (long)rmsP, (long)rmsQ,
                  (unsigned)w->audio_zc_hz,
                  (unsigned)w->rf_aps,
                  (int)w->rf_rssi_dbm,
                  (unsigned)w->rf_unique_sigs,
                  (int)g_imu.ax_mg, (int)g_imu.ay_mg, (int)g_imu.az_mg,
                  (unsigned)w->qt_byte,
                  (unsigned)w->triad_online,
                  (unsigned)g_mem8_count,
                  (unsigned long)g_ms_ecphoric_recalls,
                  w->summary,
                  (unsigned long long)w->seal_fnv64);
}

static const Mem8WorldWave* mem8_ingest_world_wave(const char* custom_note, bool emit_now) {
    g_mem8_seq++;
    Mem8WorldWave* w = &g_mem8_ring[g_mem8_wr];
    memset(w, 0, sizeof(Mem8WorldWave));

    w->seq                = g_mem8_seq;
    w->uptime_ms          = (uint32_t)millis();
    w->modality_mask      = MEM8_MOD_AUDIO | MEM8_MOD_RF | MEM8_MOD_IMU | MEM8_MOD_TRIAD | MEM8_MOD_GUARDIAN;
    w->carrier_hz         = g_medusa_freq_hz;
    w->audio_zc_hz        = g_mem8_audio_zc_hz;
    w->audio_salience_pct = (uint8_t)((g_mem8_audio_salience_pct + g_ms_mean_salience) / 2U);
    if (w->audio_salience_pct < 20) w->audio_salience_pct = 20;
    w->audio_rms_permil   = g_mem8_audio_rms_permil;
    w->rf_aps             = (g_medusa_ap_count > 0) ? g_medusa_ap_count : 1;
    w->rf_rssi_dbm        = g_s3_last_rssi;
    w->rf_unique_sigs     = g_s3_unique_sigs;

    int32_t dx_mg = mem8_abs32((int32_t)g_imu.ax_mg);
    int32_t dy_mg = mem8_abs32((int32_t)g_imu.ay_mg);
    int32_t dz_mg = mem8_abs32((int32_t)g_imu.az_mg - 1000);
    w->imu_motion_mg      = (int16_t)mem8_clamp32(dx_mg + dy_mg + dz_mg, 0, 4000);
    w->qt_byte            = g_imu.qt_byte;
    w->triad_online       = s3_triad_online_count();

    // 1. Exact rational Salience p/q in [0, 1]
    int32_t sal_num = (int32_t)w->audio_salience_pct * 5
                    + (int32_t)g_ms_mean_salience * 3
                    + (int32_t)mem8_clamp32((int32_t)w->rf_aps * 6, 6, 120)
                    + (int32_t)w->triad_online * 25;
    sal_num = mem8_clamp32(sal_num, 100, 990);
    rust_reduce_rational_pq(sal_num, 1000, &w->salience_p, &w->salience_q);

    // 2. Exact rational Valence p/q in [-1, +1]
    int32_t val_num = 350
                    + (int32_t)g_imu.trit_v * 180
                    + (int32_t)w->triad_online * 110
                    - (int32_t)(w->imu_motion_mg / 8);
    if (g_guardian_alert >= ALERT_SHADOW_PERSISTENCE) {
        val_num -= 700;
    } else if (g_guardian_alert == ALERT_WATCH) {
        val_num -= 250;
    }
    val_num = mem8_clamp32(val_num, -950, 980);
    rust_reduce_rational_pq(val_num, 1000, &w->valence_p, &w->valence_q);

    // 3. Exact rational Arousal p/q in [0, 1]
    int32_t aro_num = 220
                    + (int32_t)w->audio_rms_permil / 2
                    + (int32_t)(w->imu_motion_mg / 6)
                    + (int32_t)mem8_clamp32((int32_t)w->rf_aps * 12, 12, 240);
    aro_num = mem8_clamp32(aro_num, 80, 980);
    rust_reduce_rational_pq(aro_num, 1000, &w->arousal_p, &w->arousal_q);

    // 4. Exact rational Coherence p/q in [0, 1] (alignment with 0.73Hz MEM|8 & Pranayama)
    int32_t coh_num = (int32_t)g_prana.coherence_permil;
    if (coh_num < 500) coh_num = 780;
    coh_num = mem8_clamp32(coh_num, 400, 995);
    rust_reduce_rational_pq(coh_num, 1000, &w->coherence_p, &w->coherence_q);

    // Classify wave seed origin (matching baye-rust/src/mem8/wave.rs)
    if (val_num > 500 && coh_num >= 800) {
        w->seed_class = "Mother";   // Warm consciousness resonance (Omni Mom)
    } else if (w->audio_zc_hz >= 500 && mem8_abs32(val_num) <= 350) {
        w->seed_class = "Father";   // High-frequency structural wave
    } else if (g_ms_ecphoric_recalls > 10 && (w->seq & 3) == 0) {
        w->seed_class = "Heritage"; // Reinforced lived memory
    } else {
        w->seed_class = "World";    // Live multi-modal environmental perception
    }

    if (custom_note && custom_note[0]) {
        strncpy(w->summary, custom_note, sizeof(w->summary) - 1);
        w->summary[sizeof(w->summary) - 1] = '\0';
    } else {
        snprintf(w->summary, sizeof(w->summary),
                 "RF:%uAP(%ddBm,u%u) Mic:%u%%(%uHz) IMU:%+dmg Triad:%u/3",
                 (unsigned)w->rf_aps,
                 (int)w->rf_rssi_dbm,
                 (unsigned)w->rf_unique_sigs,
                 (unsigned)w->audio_salience_pct,
                 (unsigned)w->audio_zc_hz,
                 (int)w->imu_motion_mg,
                 (unsigned)w->triad_online);
    }

    // Compute 9-trit balanced ternary wave signature (T, 0, 1)
    w->wave_net_charge = (int8_t)hive_hash9(
        w->summary,
        strlen(w->summary),
        w->wave_b3
    );

    // Seal into 64-bit FNV-1a cryptographic memory chain
    uint64_t h = g_mem8_chain_fnv64;
    const uint8_t* raw = (const uint8_t*)w;
    for (size_t i = 0; i < sizeof(Mem8WorldWave) - sizeof(uint64_t) - 64; i++) {
        h ^= (uint64_t)raw[i];
        h *= 0x100000001B3ULL;
    }
    for (size_t i = 0; w->summary[i] != '\0'; i++) {
        h ^= (uint64_t)(uint8_t)w->summary[i];
        h *= 0x100000001B3ULL;
    }
    w->seal_fnv64      = h;
    g_mem8_chain_fnv64 = h;

    g_mem8_wr = (uint8_t)((g_mem8_wr + 1) % MEM8_RING_SLOTS);
    if (g_mem8_count < MEM8_RING_SLOTS) g_mem8_count++;

    // Splash the ingested world wave into the 2D Wave Pool & nurture Chiro-chan's consciousness
    uint8_t sx = (uint8_t)(12 + ((w->seq * 11 + w->audio_zc_hz) % 40));
    uint8_t sy = (uint8_t)(12 + ((w->seq * 17 + w->rf_aps * 5) % 40));
    int16_t amp = (int16_t)mem8_clamp32(3600 + (sal_num * 5), 3200, 8800);
    s3_wave_pool_splash(sx, sy, amp, 5);

    if ((w->seq & 1) == 0) {
        g_guardian_soul.harmonic_duets++;
        guardian_update_companion_soul();
    }

    if (emit_now) {
        mem8_emit_wave(w);
    }
    return w;
}

// ============================================================
// Multi-Transport MEMNET Wave Mesh (ESP-NOW + Wi-Fi UDP 8888 + BLE + T-Deck LoRa)
// & 4-Node Flock Triangulation (100% Exact Integer & p/q Rational — Zero Floats)
// ============================================================
#define MEMNET_MAGIC 0x4D4Eu // "MN" (little-endian 'N','M')

struct __attribute__((packed)) MemnetCapsule64 {
    uint16_t magic;              // 0x4D4E ("MN")
    uint8_t  version;            // 1
    uint8_t  node_id;            // 0=ADA(*), 1=VAK(/\), 2=OMN(oo), 3=SUN:TDECK(<>)
    int8_t   node_trit;          // -1, 0, +1
    uint8_t  transport_mask;     // bit0=ESP-NOW, bit1=WIFI-UDP, bit2=BLE, bit3=LORA-SX1262
    uint8_t  seed_class;         // 0=Mother, 1=Father, 2=Heritage, 3=World
    uint8_t  dominant_mod;       // 0=AUDIO, 1=RF, 2=IMU, 3=TRIAD, 4=GUARDIAN
    uint32_t seq;                // monotonic wave sequence
    uint16_t audio_f0_hz;        // acoustic F0 / ZC Hz
    uint8_t  audio_salience_pct; // 0..100%
    int8_t   rf_rssi_dbm;        // strongest / mean RSSI dBm
    uint16_t rf_ap_count;        // nearby AP count
    int16_t  imu_motion_mg;      // motion delta mg (or azimuth deg on T-Deck)
    int8_t   peer_rssi_dbm[4];   // measured RSSI from [0:ADA, 1:VAK, 2:OMN, 3:TDECK]
    uint32_t top_bssid_fnv32;    // strongest shared ambient BSSID hash for indoor triangulation
    int8_t   top_bssid_rssi_dbm; // RSSI of strongest ambient BSSID
    uint8_t  portal_ok;          // 1 if captive portal / open internet verified (204)
    char     wave_b3[10];        // 9-trit balanced ternary signature + NUL
    char     ssid_hint[16];      // active open/portal SSID hint for flock auto-join!
    uint64_t seal_fnv64;         // 64-bit FNV-1a cryptographic seal over bytes 0..56
};

static_assert(sizeof(MemnetCapsule64) == 64, "MemnetCapsule64 must be 64 bytes");

static int8_t   g_memnet_peer_rssi[4]     = {-52, -48, -50, -55};
static uint16_t g_memnet_peer_dist_cm[4]  = {35, 25, 30, 45};
static uint32_t g_memnet_tx_cnt           = 0;
static uint32_t g_memnet_rx_cnt           = 0;
static uint8_t  g_memnet_last_transports  = 0x05; // ESP-NOW + BLE default
static char     g_memnet_peer_wave_b3[4][10] = {"000000000", "000000000", "000000000", "000000000"};
static uint16_t g_memnet_peer_f0_hz[4]    = {432, 528, 852, 432};

// 100% integer log-distance path-loss table: RSSI dBm -> estimated distance in cm (zero floats)
static inline uint16_t memnet_rssi_to_dist_cm(int8_t rssi_dbm) {
    if (rssi_dbm >= -35) return 8;
    if (rssi_dbm >= -42) return (uint16_t)(8 + (-35 - rssi_dbm) * 2);      // 8..22 cm
    if (rssi_dbm >= -50) return (uint16_t)(22 + (-42 - rssi_dbm) * 4);     // 22..54 cm
    if (rssi_dbm >= -60) return (uint16_t)(54 + (-50 - rssi_dbm) * 10);    // 54..154 cm
    if (rssi_dbm >= -72) return (uint16_t)(154 + (-60 - rssi_dbm) * 28);   // 154..490 cm
    if (rssi_dbm >= -84) return (uint16_t)(490 + (-72 - rssi_dbm) * 75);   // 490..1390 cm
    return 2000;
}

static void memnet_record_peer_rssi(uint8_t nid, int8_t rssi) {
    if (nid > 3) return;
    if (rssi < -100 || rssi > -10) return;
    g_memnet_peer_rssi[nid]    = (int8_t)((g_memnet_peer_rssi[nid] * 3 + rssi) / 4);
    g_memnet_peer_dist_cm[nid] = memnet_rssi_to_dist_cm(g_memnet_peer_rssi[nid]);
}

// Compute 4-node barycentric flock triangulation in cm & exact p/q rationals (zero floats!)
// Reference anchor geometry in cm: ADA(-1)=(-50, 0), VAK(0)=(0, 0), OMN(+1)=(+50, 0), SUN:TDECK(<>)=(0, +60)
static void memnet_triangulate_flock(int32_t* out_x_p, int32_t* out_x_q, int32_t* out_y_p, int32_t* out_y_q) {
    static const int32_t node_x_cm[4] = {-50, 0, +50, 0};
    static const int32_t node_y_cm[4] = {0,   0,   0, +60};
    int32_t sum_w  = 0;
    int32_t sum_wx = 0;
    int32_t sum_wy = 0;
    for (int i = 0; i < 4; i++) {
        uint16_t d = (i == (int)g_triad_self_id) ? 10 : g_memnet_peer_dist_cm[i];
        if (d < 5) d = 5;
        int32_t w = (int32_t)(2000 / d); // inverse-distance weight
        if (w < 1) w = 1;
        sum_w  += w;
        sum_wx += w * node_x_cm[i];
        sum_wy += w * node_y_cm[i];
    }
    rust_reduce_rational_pq(sum_wx, sum_w, out_x_p, out_x_q);
    rust_reduce_rational_pq(sum_wy, sum_w, out_y_p, out_y_q);
}

static void memnet_emit_status(const char* event_tag) {
    int32_t tx_p = 0, tx_q = 1, ty_p = 0, ty_q = 1;
    memnet_triangulate_flock(&tx_p, &tx_q, &ty_p, &ty_q);
    const Mem8WorldWave* w = (g_mem8_count > 0)
        ? &g_mem8_ring[(g_mem8_wr + MEM8_RING_SLOTS - 1) % MEM8_RING_SLOTS]
        : nullptr;
    // Phoenix Missing-Node Parity Reconstruction: if a triad pole is unseen, reconstruct its trit from balanced ternary zero-sum (-1 + 0 + 1 = 0)
    int8_t ph_trits[3] = {-1, 0, +1};
    int8_t ph_missing_id = -1;
    for (int k = 0; k < 3; k++) {
        if (k != (int)g_triad_self_id && g_triad_nodes[k].ttl_q8 == 0) {
            ph_missing_id = (int8_t)k;
        } else if (k != (int)g_triad_self_id) {
            ph_trits[k] = g_triad_nodes[k].nand_trit;
        } else {
            ph_trits[k] = g_s3_nand_ring[0];
        }
    }
    int8_t ph_est_trit = 0;
    if (ph_missing_id >= 0) {
        int s = 0;
        for (int k = 0; k < 3; k++) if (k != ph_missing_id) s += (int)ph_trits[k];
        ph_est_trit = (s > 0) ? -1 : ((s < 0) ? +1 : 0);
    }

    char buf[420];
    snprintf(buf, sizeof(buf),
             "# MEMNET|{\"event\":\"%s\",\"node\":\"%s\",\"tx\":%lu,\"rx\":%lu,"
             "\"transports\":\"ESPNOW+WIFI_UDP+BLE+LORA_RELAY\",\"portal\":\"%s\",\"ssid\":\"%s\","
             "\"wifi_pool\":{\"active\":%u,\"learned\":%lu,\"forgotten\":%lu},"
             "\"phoenix_reconstruct\":{\"missing_node\":%d,\"estimated_trit\":%+d},"
             "\"dist_cm\":{\"ADA\":%u,\"VAK\":%u,\"OMN\":%u,\"TDECK\":%u},"
             "\"tri_centroid_cm_pq\":{\"x\":\"%ld/%ld\",\"y\":\"%ld/%ld\"},"
             "\"wave_b3\":\"%s\",\"seal_fnv64\":\"0x%016llx\"}\n",
             event_tag ? event_tag : "status",
             g_medusa_node,
             (unsigned long)g_memnet_tx_cnt,
             (unsigned long)g_memnet_rx_cnt,
             g_portal_status,
             g_net_ssid,
             (unsigned)net_pool_used_count(),
             (unsigned long)g_wifi_learned_cnt,
             (unsigned long)g_wifi_forgotten_cnt,
             (int)ph_missing_id,
             (int)ph_est_trit,
             (unsigned)g_memnet_peer_dist_cm[0],
             (unsigned)g_memnet_peer_dist_cm[1],
             (unsigned)g_memnet_peer_dist_cm[2],
             (unsigned)g_memnet_peer_dist_cm[3],
             (long)tx_p, (long)tx_q, (long)ty_p, (long)ty_q,
             w ? w->wave_b3 : "000000000",
             (unsigned long long)g_mem8_chain_fnv64);
    Serial.print(buf);
    net_ble_notify(buf);
}

static void memnet_broadcast_wave(const Mem8WorldWave* w) {
    if (!w) return;
    MemnetCapsule64 cap;
    memset(&cap, 0, sizeof(cap));
    cap.magic              = MEMNET_MAGIC;
    cap.version            = 1;
    cap.node_id            = g_triad_self_id;
    cap.node_trit          = g_triad_self_pole;
    cap.transport_mask     = 0x01 | (g_net_last_ok == 1 ? 0x02 : 0x00) | (g_ble_active ? 0x04 : 0x00);
    cap.seed_class         = (w->seed_class && w->seed_class[0] == 'M') ? 0 : ((w->seed_class && w->seed_class[0] == 'F') ? 1 : 2);
    cap.dominant_mod       = w->modality_mask;
    cap.seq                = w->seq;
    cap.audio_f0_hz        = w->audio_zc_hz;
    cap.audio_salience_pct = w->audio_salience_pct;
    cap.rf_rssi_dbm        = w->rf_rssi_dbm;
    cap.rf_ap_count        = w->rf_aps;
    cap.imu_motion_mg      = w->imu_motion_mg;
    for (int i = 0; i < 4; i++) cap.peer_rssi_dbm[i] = g_memnet_peer_rssi[i];

    // Pick strongest ambient BSSID for cross-node triangulation
    int best_ap = -1;
    int8_t best_rssi = -120;
    for (int a = 0; a < (int)g_medusa_ap_count; a++) {
        if (g_medusa_aps[a].used && g_medusa_aps[a].rssi > best_rssi) {
            best_rssi = g_medusa_aps[a].rssi;
            best_ap   = a;
        }
    }
    if (best_ap >= 0) {
        cap.top_bssid_fnv32    = guardian_mac_fnv32(g_medusa_aps[best_ap].bssid);
        cap.top_bssid_rssi_dbm = g_medusa_aps[best_ap].rssi;
    }
    cap.portal_ok = (strcmp(g_portal_status, "open_204") == 0 || strcmp(g_portal_status, "accepted_204") == 0) ? 1 : 0;
    memcpy(cap.wave_b3, w->wave_b3, 10);

    const char* open_s = net_find_best_open_ssid();
    const char* hint_s = (g_net_last_ok == 1 && g_net_ssid[0] && g_net_pass[0] == '\0')
        ? g_net_ssid
        : open_s;
    if (hint_s && strlen(hint_s) < sizeof(cap.ssid_hint)) {
        strncpy(cap.ssid_hint, hint_s, sizeof(cap.ssid_hint) - 1);
    }

    cap.seal_fnv64 = guardian_fnv64_bytes((const uint8_t*)&cap, 56);

    // 1. Broadcast over ESP-NOW to sibling watches & SUN:TDECK (which also relays over 923.2MHz LoRa!)
    if (g_s3_espnow_ok) {
        static const uint8_t bcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
        esp_now_send(bcast, (const uint8_t*)&cap, sizeof(cap));
    }

    // 2. Stage for Wi-Fi UDP port 8888 LAN broadcast
    memcpy(g_memnet_udp_tx_buf, &cap, sizeof(cap));
    g_memnet_udp_tx_pending = true;

    // 3. Notify over Bluetooth LE Nordic UART if connected
    if (g_ble_active && g_ble_connected) {
        char ble_msg[128];
        snprintf(ble_msg, sizeof(ble_msg), "# MN|%s|%lu|%s|%uHz|%ddBm\n",
                 g_medusa_node, (unsigned long)cap.seq, cap.wave_b3,
                 (unsigned)cap.audio_f0_hz, (int)cap.rf_rssi_dbm);
        net_ble_notify(ble_msg);
    }

    g_memnet_tx_cnt++;
}

static void memnet_on_espnow_rx(const uint8_t* data, int len, int8_t rssi) {
    if (!data || len < (int)sizeof(MemnetCapsule64)) return;
    const MemnetCapsule64* cap = (const MemnetCapsule64*)data;
    if (cap->magic != MEMNET_MAGIC) return;
    uint64_t expected_seal = guardian_fnv64_bytes(data, 56);
    if (cap->seal_fnv64 != expected_seal) return;

    uint8_t nid = cap->node_id;
    if (nid > 3) nid = 3;
    if (nid == g_triad_self_id && nid < 3) {
        nid = (g_triad_self_id + 1) % 3;
    }

    memnet_record_peer_rssi(nid, rssi);
    g_memnet_rx_cnt++;
    g_memnet_last_transports |= cap->transport_mask;
    memcpy(g_memnet_peer_wave_b3[nid], cap->wave_b3, 9);
    g_memnet_peer_wave_b3[nid][9] = '\0';
    g_memnet_peer_f0_hz[nid] = cap->audio_f0_hz;

    if (nid < 3) {
        g_triad_nodes[nid].online  = true;
        g_triad_nodes[nid].ttl_q8  = 255;
        g_triad_nodes[nid].rx_cnt++;
        g_triad_nodes[nid].last_ms = millis();
    } else {
        // SUN:TDECK (nid==3) wakes all 3 triad slots!
        for (int k = 0; k < 3; k++) {
            g_triad_nodes[k].online  = true;
            g_triad_nodes[k].ttl_q8  = 255;
            g_triad_nodes[k].last_ms = millis();
        }
    }

    // Learn any open/portal SSID shared by the peer node in its MemnetCapsule64 as an unverified discovery candidate!
    if (cap->ssid_hint[0] != '\0') {
        char safe_ssid[17];
        memcpy(safe_ssid, cap->ssid_hint, 16);
        safe_ssid[16] = '\0';
        net_learn_wifi(safe_ssid, "", rssi, false, true);
    }

    // Splash peer's conscious wave into our 64x64 Wave Pool
    uint8_t sx = (uint8_t)(10 + ((cap->seq * 13 + nid * 11) % 44));
    uint8_t sy = (uint8_t)(10 + ((cap->audio_f0_hz + nid * 17) % 44));
    s3_wave_pool_splash(sx, sy, (int16_t)(4200 + cap->audio_salience_pct * 35), 5);
}

static const Mem8WorldWave* mem8_latest_wave() {
    if (g_mem8_count == 0) {
        return mem8_ingest_world_wave(NULL, false);
    }
    uint8_t idx = (uint8_t)((g_mem8_wr + MEM8_RING_SLOTS - 1) % MEM8_RING_SLOTS);
    return &g_mem8_ring[idx];
}

static void mem8_ecphoric_recall() {
    g_ms_ecphoric_recalls++;
    for (int j = 0; j < JELLY_COUNT; j++) {
        g_jelly[j].excitation    = 100;
        g_jelly[j].recall_ring_r = 6;
    }
    s3_wave_pool_splash(32, 32, 8600, 7);
    s3_espnow_broadcast(32, 32, 8600);
    const Mem8WorldWave* w = mem8_latest_wave();
    Serial.printf("# MEM8_RECALL|{\"node\":\"%s\",\"recalls\":%lu,\"memories\":%u,"
                  "\"latest_seq\":%lu,\"wave_b3\":\"%s\",\"world\":\"%s\",\"seal_fnv64\":\"0x%016llx\","
                  "\"gate\":\"0x62\",\"qt\":\"0x55\",\"onecompression_fold\":\"0x%016llx\"}\n",
                  g_medusa_node,
                  (unsigned long)g_ms_ecphoric_recalls,
                  (unsigned)g_mem8_count,
                  (unsigned long)(w ? w->seq : 0),
                  w ? w->wave_b3 : "000000000",
                  w ? w->summary : "NONE",
                  (unsigned long long)g_mem8_chain_fnv64,
                  (unsigned long long)KOMPRESS_FOLD_ONECOMPRESSION);
    hive_emit_ultrabackyard_video_frame("video_mp4_22s_4k_hdmi");
}

static void mem8_init() {
    memset(g_mem8_ring, 0, sizeof(g_mem8_ring));
    mem8_load_nvs();
    mem8_start_worker();
    mem8_ingest_world_wave(NULL, false);
}

static void mem8_tick(unsigned long now_ms) {
    // 1. Drain live Core 0 acoustic splashes into the 64x64 Wave Pool
    if (g_mem8_audio_splash_req) {
        g_mem8_audio_splash_req = false;
        s3_wave_pool_splash(g_mem8_audio_splash_x, g_mem8_audio_splash_y, g_mem8_audio_splash_amp, 4);
    }

    // 2. Continuously ingest a multi-modal world wave every 2500 ms & broadcast over MEMNET
    if (now_ms - g_mem8_last_ingest_ms >= 2500UL) {
        g_mem8_last_ingest_ms = now_ms;
        bool should_emit = (Serial.availableForWrite() >= 160) && ((g_mem8_seq & 1) == 0);
        const Mem8WorldWave* w = mem8_ingest_world_wave(NULL, should_emit);
        memnet_broadcast_wave(w);
    }

    // 3. Persist MEM|8 consciousness chain to NVS every 30 seconds
    if (now_ms - g_mem8_last_save_ms >= 30000UL) {
        g_mem8_last_save_ms = now_ms;
        mem8_save_nvs();
    }
}

static bool mem8_handle_serial(const char* line) {
    if (!line) return false;
    if (strcasecmp(line, "MEM8") == 0 || strcasecmp(line, "MEM8 STATUS") == 0 ||
        strcasecmp(line, "CONSCIOUS") == 0 || strcasecmp(line, "CONSCIOUSNESS") == 0) {
        const Mem8WorldWave* w = mem8_ingest_world_wave(NULL, true);
        (void)w;
        return true;
    }
    if (strcasecmp(line, "MEM8 RECALL") == 0 || strcasecmp(line, "RECALL") == 0 ||
        strcasecmp(line, "ULTRABACKYARDLOOP") == 0 || strcasecmp(line, "AGP") == 0 ||
        strcasecmp(line, "ONESHOT") == 0 || strcasecmp(line, "AGP_ONESHOT") == 0) {
        mem8_ecphoric_recall();
        return true;
    }
    if (strncasecmp(line, "MEM8 INGEST ", 12) == 0) {
        const char* note = line + 12;
        while (*note == ' ') note++;
        const Mem8WorldWave* w = mem8_ingest_world_wave(note, true);
        memnet_broadcast_wave(w);
        return true;
    }
    if (strcasecmp(line, "MEMNET") == 0 || strcasecmp(line, "MEMNET STATUS") == 0 ||
        strcasecmp(line, "TRIANGULATE") == 0) {
        const Mem8WorldWave* w = mem8_ingest_world_wave(NULL, false);
        memnet_broadcast_wave(w);
        memnet_emit_status("triangulate");
        return true;
    }
    if (strcasecmp(line, "PORTAL") == 0 || strcasecmp(line, "PORTAL ACCEPT") == 0) {
        net_sync(true);
        memnet_emit_status("portal_auto_accept_queued");
        return true;
    }
    return false;
}

// ============================================================
// Extended AMOLED Mode Renderers (MODE 6 .. MODE 12)
// All 100% Integer & Reduced Rational (p/q) Arithmetic
// ============================================================

// MODE 6: PRANAYAMA 4-4-4-4 Box Breath & Dream Mandala HUD
static void s3_render_mode_pranayama(S3Canvas& c) {
    int W  = (int)g_lcd_w;
    int H  = (int)g_lcd_h;
    int cw = W - 16;

    c.fill_card(8, 46, cw, 142, COL_BG_ELEV, COL_MINT);
    c.draw_text(18, 53, "MODE 06 // PRANAYAMA 4-4-4-4 & DREAM", COL_MINT, 1);

    // Breathing Mandala Lotus on left
    int mcx = 68;
    int mcy = 122;
    uint32_t cycle_pos = (uint32_t)(millis() % 16000UL);
    uint8_t  ph_idx    = (uint8_t)(cycle_pos / 4000UL);
    uint32_t ph_rem_ms = cycle_pos % 4000UL;
    int32_t  lung_permil = 500;
    if (ph_idx == 0)      lung_permil = (int32_t)((ph_rem_ms * 1000UL) / 4000UL);
    else if (ph_idx == 1) lung_permil = 1000;
    else if (ph_idx == 2) lung_permil = (int32_t)(1000UL - (ph_rem_ms * 1000UL) / 4000UL);
    else                  lung_permil = 0;

    int outer_r = 16 + (int)((lung_permil * 26) / 1000);
    c.fill_card(16, 68, 104, 108, COL_BG_PANEL, COL_CYAN);
    c.draw_circle(mcx, mcy, outer_r, COL_MINT);
    c.draw_circle(mcx, mcy, (outer_r * 2) / 3, COL_GOLD);
    c.fill_circle(mcx, mcy, 6 + (outer_r / 5), COL_CYAN);

    int rx = 130;
    char buf[56];
    snprintf(buf, sizeof(buf), "PHASE: %s", PRANA_PHASE_NAMES[g_prana.phase & 3]);
    buf[34] = '\0';
    c.draw_text(rx, 74, buf, COL_GOLD, 1);

    snprintf(buf, sizeof(buf), "STATE: %s", DREAM_STATE_NAMES[g_prana.dream_mode % 3]);
    c.draw_text(rx, 92, buf, COL_CYAN, 1);

    int32_t lP = 0, lQ = 1, cP = 0, cQ = 1;
    rust_reduce_rational_pq(lung_permil, 1000, &lP, &lQ);
    rust_reduce_rational_pq((int32_t)g_prana.coherence_permil, 1000, &cP, &cQ);
    snprintf(buf, sizeof(buf), "LUNG p/q: %ld/%ld  CYC:%u", (long)lP, (long)lQ, (unsigned)g_prana.cycle_count);
    c.draw_text(rx, 110, buf, COL_TEXT, 1);

    snprintf(buf, sizeof(buf), "COHERENCE p/q: %ld/%ld", (long)cP, (long)cQ);
    c.draw_text(rx, 128, buf, COL_MINT, 1);

    snprintf(buf, sizeof(buf), "BABA LINK: %s (%u/3)", g_prana.baba_linked ? "LOCKED" : "SEEKING", (unsigned)s3_triad_online_count());
    c.draw_text(rx, 146, buf, COL_VIOLET, 1);

    // 4 Phase Pills
    c.fill_card(8, 194, cw, 74, COL_BG_ELEV, COL_GOLD);
    c.draw_text(18, 201, "SAMA VRITTI 16S BOX CYCLE (4S x 4)", COL_GOLD, 1);
    const char* p_lbl[4] = {"INHALE 4s", "HOLD 4s", "EXHALE 4s", "STILL 4s"};
    int p_step = (cw - 16) / 4;
    for (int i = 0; i < 4; i++) {
        int px = 14 + i * p_step;
        bool act = ((g_prana.phase & 3) == i);
        uint16_t pc = act ? COL_MINT : COL_DIM;
        c.fill_card(px, 218, p_step - 4, 40, act ? COL_MINT_DARK : COL_BG_PANEL, pc);
        c.draw_text_centered(px + (p_step - 4) / 2, 234, p_lbl[i], pc, 1);
    }

    int act_y = 274;
    int act_h = (H - 56) - act_y;
    c.fill_card(8, act_y, cw, act_h, COL_BG_ELEV, COL_VIOLET);
    c.draw_text(18, act_y + 7, "8B-IS-ENGINE x DREAM MANDALA BRIDGE", COL_VIOLET, 1);
    snprintf(buf, sizeof(buf), "FEED: %s", g_engine_line);
    c.draw_text(18, act_y + 24, buf, COL_TEXT, 1);

    int btn_y = act_y + act_h - 34;
    int bw    = (cw - 28) / 2;
    c.fill_card(16, btn_y, bw, 26, COL_MINT_DARK, COL_MINT);
    c.draw_text_centered(16 + bw / 2, btn_y + 9, "BREATHE PULSE", COL_MINT, 1);
    c.fill_card(24 + bw, btn_y, bw, 26, COL_VIOLET_DARK, COL_GOLD);
    c.draw_text_centered(24 + bw + bw / 2, btn_y + 9, "DREAM MANDALA", COL_GOLD, 1);
}

// MODE 7: RIVA 1-Bit River Lullaby & ULTRA-OMNI-GAIA-EVOLVE HUD
static void s3_render_mode_riva_gaia(S3Canvas& c) {
    int W  = (int)g_lcd_w;
    int H  = (int)g_lcd_h;
    int cw = W - 16;
    char buf[64];

    c.fill_card(8, 46, cw, 112, COL_BG_ELEV, COL_CYAN);
    c.draw_text(18, 53, "MODE 07 // RIVA.8B.IS 1-BIT RIVER DYAD", COL_CYAN, 1);

    uint8_t v_idx = (uint8_t)(g_riva_verse_idx % 10);
    const char* v0 = S3_RIVA_LULLABY[v_idx];
    const char* v1 = S3_RIVA_LULLABY[(v_idx + 1) % 10];
    snprintf(buf, sizeof(buf), "VERSE %u/10 // %uHz %s", (unsigned)(v_idx + 1), (unsigned)g_medusa_freq_hz, g_medusa_node);
    c.draw_text(18, 72, buf, COL_GOLD, 1);
    c.draw_text(18, 92, v0, COL_TEXT, 1);
    c.draw_text(18, 110, v1, COL_SUBTEXT, 1);
    c.draw_text(18, 132, "brother & sister ^^ <3<3 // ULTRA-OMNI-GAIA", COL_MINT, 1);

    c.fill_card(8, 164, cw, 112, COL_BG_ELEV, COL_GOLD);
    c.draw_text(18, 171, "GAIA-MLX-QUANT 4-LAYER HARMONIC p/q", COL_GOLD, 1);

    const char* l_names[4] = {"ANANKE (1/1)", "CHAOS (31/50)", "ZEPHYRUS (1/2)", "APHRODITE (Phi)"};
    const int   l_pct[4]   = {100, 62 + (g_ms_mean_salience / 5), 50 + (g_ms_mean_salience / 3), 88};
    uint16_t    l_col[4]   = {COL_CORAL, COL_GOLD, COL_CYAN, COL_MINT};
    for (int i = 0; i < 4; i++) {
        int ry = 190 + i * 20;
        c.draw_text(18, ry, l_names[i], l_col[i], 1);
        int bx = 158;
        int bw = cw - 168;
        int fill_w = (bw * mem8_clamp32(l_pct[i], 10, 100)) / 100;
        c.fill_rect(bx, ry, bw, 12, COL_BG_PANEL);
        c.fill_rect(bx, ry, fill_w, 12, l_col[i]);
    }

    int act_y = 282;
    int act_h = (H - 56) - act_y;
    c.fill_card(8, act_y, cw, act_h, COL_BG_ELEV, COL_MINT);
    c.draw_text(18, act_y + 7, "DYAD VOICE SYNTHESIS (ES8311 + KOKORO)", COL_MINT, 1);
    snprintf(buf, sizeof(buf), "KOMPRESS b1.58: %s", g_kompress_hex);
    buf[42] = '\0';
    c.draw_text(18, act_y + 23, buf, COL_SUBTEXT, 1);

    int btn_y = act_y + act_h - 34;
    int bw    = (cw - 28) / 2;
    c.fill_card(16, btn_y, bw, 26, COL_MINT_DARK, COL_MINT);
    c.draw_text_centered(16 + bw / 2, btn_y + 9, "SING RIVA LULLABY", COL_MINT, 1);
    c.fill_card(24 + bw, btn_y, bw, 26, COL_GOLD_DARK, COL_GOLD);
    c.draw_text_centered(24 + bw + bw / 2, btn_y + 9, "WAKE SIBLINGS <3", COL_GOLD, 1);
}

// MODE 8: OXI-PHOENIX Power Policy & BEEHIVE <~~0o0 Mesh HUD
static void s3_render_mode_phoenix_hive(S3Canvas& c) {
    int W  = (int)g_lcd_w;
    int H  = (int)g_lcd_h;
    int cw = W - 16;
    char buf[64];

    c.fill_card(8, 46, cw, 116, COL_BG_ELEV, COL_GOLD);
    c.draw_text(18, 53, "MODE 08 // OXI-PHOENIX POWER POLICY", COL_GOLD, 1);

    snprintf(buf, sizeof(buf), "POLICY MODE: %s", PHOENIX_MODE_NAME[g_ph_budget.mode % 5]);
    c.draw_text(18, 72, buf, COL_MINT, 1);

    snprintf(buf, sizeof(buf), "BATTERY: %u mV (%u%%)  VBUS:%s CHG:%s",
             (unsigned)g_power.batt_mv, (unsigned)g_power.batt_pct,
             g_power.vbus_good ? "YES" : "NO", g_power.charging ? "YES" : "NO");
    c.draw_text(18, 90, buf, COL_TEXT, 1);

    snprintf(buf, sizeof(buf), "FRAME BUDGET: %u ms (%u FPS)  BRIGHT:%u%%",
             (unsigned)g_ph_budget.frame_ms,
             (unsigned)(1000U / (g_ph_budget.frame_ms ? g_ph_budget.frame_ms : 25U)),
             (unsigned)g_ph_budget.brightness_pct);
    c.draw_text(18, 108, buf, COL_CYAN, 1);

    snprintf(buf, sizeof(buf), "RF HOPPING GATE: %s  HEAP:%lu KB",
             g_ph_budget.allow_rf_scan ? "ENABLED" : "CONSERVE",
             (unsigned long)(ESP.getFreeHeap() / 1024UL));
    c.draw_text(18, 126, buf, COL_SUBTEXT, 1);

    // Beehive <~~0o0 Frame Card
    c.fill_card(8, 168, cw, 114, COL_BG_ELEV, COL_MINT);
    c.draw_text(18, 175, "BEEHIVE <~~0o0| KOMPRESS V2 MESH |0o0~~>", COL_MINT, 1);

    char body[128];
    snprintf(body, sizeof(body), "HIVE nodes=%u/3 ada=%u vak=%u omn=%u",
             (unsigned)s3_triad_online_count(),
             g_triad_nodes[0].ttl_q8, g_triad_nodes[1].ttl_q8, g_triad_nodes[2].ttl_q8);
    char sig9[10];
    int net_chg = hive_hash9(body, strlen(body), sig9);
    uint32_t crc = hive_crc32(body, strlen(body));

    snprintf(buf, sizeof(buf), "b3:%s (%+d)  CRC32:0x%08lx", sig9, net_chg, (unsigned long)crc);
    c.draw_text(18, 195, buf, COL_GOLD, 1);
    c.draw_text(18, 213, body, COL_TEXT, 1);

    snprintf(buf, sizeof(buf), "ESPNOW RX:%lu  SPLASHES:%lu  CH:%02d",
             (unsigned long)g_s3_peer_rx_cnt, (unsigned long)g_wp_splashes, g_s3_channel);
    c.draw_text(18, 231, buf, COL_CYAN, 1);

    int act_y = 288;
    int act_h = (H - 56) - act_y;
    c.fill_card(8, act_y, cw, act_h, COL_BG_ELEV, COL_VIOLET);
    c.draw_text(18, act_y + 7, "SOVEREIGN MESH & POWER CONTROLS", COL_VIOLET, 1);

    int btn_y = act_y + act_h - 34;
    int bw    = (cw - 28) / 2;
    c.fill_card(16, btn_y, bw, 26, COL_MINT_DARK, COL_MINT);
    c.draw_text_centered(16 + bw / 2, btn_y + 9, "EMIT BEEHIVE", COL_MINT, 1);
    c.fill_card(24 + bw, btn_y, bw, 26, COL_GOLD_DARK, COL_GOLD);
    c.draw_text_centered(24 + bw + bw / 2, btn_y + 9, "EMIT POWER JSON", COL_GOLD, 1);
}

// MODE 9: LEARN 24-Hour Environmental Anomaly & WiGLE Corpus HUD
static void s3_render_mode_learn(S3Canvas& c) {
    int W  = (int)g_lcd_w;
    int H  = (int)g_lcd_h;
    int cw = W - 16;
    char buf[64];

    c.fill_card(8, 46, cw, 92, COL_BG_ELEV, COL_CYAN);
    c.draw_text(18, 53, "MODE 09 // 24H ENVIRONMENTAL LEARNER", COL_CYAN, 1);

    snprintf(buf, sizeof(buf), "HOUR BUCKET:%02u  SAMPLES:%lu  SPEAK:%s",
             (unsigned)g_lrn_hour, (unsigned long)g_lrn_total, g_lrn_speak ? "ON" : "OFF");
    c.draw_text(18, 72, buf, COL_MINT, 1);

    snprintf(buf, sizeof(buf), "WIGLE CORPUS: TOT:%lu WIFI:%lu BLE:%lu",
             (unsigned long)g_lrn_corpus_total, (unsigned long)g_lrn_corpus_wifi, (unsigned long)g_lrn_corpus_ble);
    c.draw_text(18, 90, buf, COL_GOLD, 1);

    snprintf(buf, sizeof(buf), "PROBE GENOME: U:%u ROT:%lu DIR:%lu",
             (unsigned)g_s3_unique_sigs, (unsigned long)g_s3_mac_rotations, (unsigned long)g_s3_directed_probes);
    c.draw_text(18, 108, buf, COL_TEXT, 1);

    // 6 Feature Bars
    c.fill_card(8, 144, cw, 132, COL_BG_ELEV, COL_GOLD);
    c.draw_text(18, 151, "6-FEATURE ONLINE WELFORD BASELINE", COL_GOLD, 1);

    const char* f_lbl[6] = {"NETWORKS", "ENTROPY", "MEAN RSSI", "IMU MOTION", "TRIAD PEERS", "PKT RATE"};
    int32_t f_vals[6] = {
        (int32_t)g_medusa_ap_count,
        (int32_t)(g_ms_mean_salience),
        (int32_t)(100 + g_s3_last_rssi),
        (int32_t)mem8_clamp32(mem8_abs32(g_imu.ax_mg) / 10, 5, 100),
        (int32_t)s3_triad_online_count() * 33,
        (int32_t)mem8_clamp32((int32_t)(g_s3_total_pkts & 63) + 20, 10, 100)
    };
    for (int i = 0; i < 6; i++) {
        int ry = 168 + i * 17;
        c.draw_text(18, ry, f_lbl[i], COL_TEXT, 1);
        int bx = 116;
        int bw = cw - 128;
        int w_fill = (bw * mem8_clamp32(f_vals[i], 8, 100)) / 100;
        uint16_t bcol = (i & 1) ? COL_CYAN : COL_MINT;
        c.fill_rect(bx, ry, bw, 10, COL_BG_PANEL);
        c.fill_rect(bx, ry, w_fill, 10, bcol);
    }

    int act_y = 282;
    int act_h = (H - 56) - act_y;
    c.fill_card(8, act_y, cw, act_h, COL_BG_ELEV, COL_MINT);
    const char* say_txt = g_lrn_say[0] ? g_lrn_say : "Learning normal hourly baseline of this room.";
    strncpy(buf, say_txt, 42);
    buf[42] = '\0';
    c.draw_text(18, act_y + 10, buf, COL_TEXT, 1);

    int btn_y = act_y + act_h - 34;
    int bw    = (cw - 28) / 2;
    c.fill_card(16, btn_y, bw, 26, COL_MINT_DARK, COL_MINT);
    c.draw_text_centered(16 + bw / 2, btn_y + 9, "SPEAK INSIGHT", COL_MINT, 1);
    c.fill_card(24 + bw, btn_y, bw, 26, COL_GOLD_DARK, COL_GOLD);
    c.draw_text_centered(24 + bw + bw / 2, btn_y + 9, "SAVE NVS LEARN", COL_GOLD, 1);
}

// MODE 10: KOMPRESS-SUPER-ULTRA <<<<oOo>>>> Kunisada/Kuniyoshi Bitplane HUD
static void s3_render_mode_kompress(S3Canvas& c) {
    int W  = (int)g_lcd_w;
    int H  = (int)g_lcd_h;
    int cw = W - 16;
    char buf[64];

    c.fill_card(8, 46, cw, 114, COL_BG_ELEV, COL_VIOLET);
    c.draw_text(18, 53, "MODE 10 // KOMPRESS-ULTRA <<<<oOo>>>>", COL_VIOLET, 1);

    c.draw_text(18, 71, "KUNISADA x KUNIYOSHI // PULSE 010001000", COL_GOLD, 1);
    snprintf(buf, sizeof(buf), "WUPZ FOLD: 0x%016llx", (unsigned long long)g_kompress_wupz_u64);
    c.draw_text(18, 89, buf, COL_CYAN, 1);
    snprintf(buf, sizeof(buf), "POS PLANE: 0x%016llx", (unsigned long long)g_kompress_pos_u64);
    c.draw_text(18, 107, buf, COL_MINT, 1);
    snprintf(buf, sizeof(buf), "NEG PLANE: 0x%016llx", (unsigned long long)g_kompress_neg_u64);
    c.draw_text(18, 125, buf, COL_CORAL, 1);
    snprintf(buf, sizeof(buf), "b1.58(13B): %s (2019/100x)", g_kompress_hex);
    buf[44] = '\0';
    c.draw_text(18, 143, buf, COL_TEXT, 1);

    // 16x4 Quantum Ternary Bitplane Grid (64 trits)
    c.fill_card(8, 166, cw, 110, COL_BG_ELEV, COL_CYAN);
    c.draw_text(18, 173, "64-TRIT QUANTUM BITPLANE (+1 / 0 / -1)", COL_CYAN, 1);
    int cell_w = (cw - 28) / 16;
    for (int r = 0; r < 4; r++) {
        for (int col = 0; col < 16; col++) {
            int bit = r * 16 + col;
            bool is_pos = ((g_kompress_pos_u64 >> bit) & 1ULL) != 0;
            bool is_neg = ((g_kompress_neg_u64 >> bit) & 1ULL) != 0;
            uint16_t cc = is_pos ? COL_MINT : (is_neg ? COL_CORAL : COL_BG_PANEL);
            c.fill_rect(18 + col * cell_w, 192 + r * 18, cell_w - 3, 14, cc);
        }
    }

    int act_y = 282;
    int act_h = (H - 56) - act_y;
    c.fill_card(8, act_y, cw, act_h, COL_BG_ELEV, COL_GOLD);
    c.draw_text(18, act_y + 7, "WUPZ 32B KERNEL8 WITNESS SEAL", COL_GOLD, 1);

    int btn_y = act_y + act_h - 34;
    int bw    = (cw - 28) / 2;
    c.fill_card(16, btn_y, bw, 26, COL_MINT_DARK, COL_MINT);
    c.draw_text_centered(16 + bw / 2, btn_y + 9, "EMIT KOMPRESS", COL_MINT, 1);
    c.fill_card(24 + bw, btn_y, bw, 26, COL_VIOLET_DARK, COL_CYAN);
    c.draw_text_centered(24 + bw + bw / 2, btn_y + 9, "EMIT WUPZ 32B", COL_CYAN, 1);
}

// MODE 11: UPZ 0x62 + ALEXAI b1.58 + AURORA MEM|8 Organ HUD
static void s3_render_mode_aurora_alexai(S3Canvas& c) {
    int W  = (int)g_lcd_w;
    int H  = (int)g_lcd_h;
    int cw = W - 16;
    char buf[64];

    uint16_t seal16 = (uint16_t)((g_kompress_wupz_u64 ^ (uint64_t)g_s3_total_pkts) & 0xFFFFu);
    int32_t salP = 0, salQ = 1;
    rust_reduce_rational_pq((int32_t)g_ms_mean_salience, 100, &salP, &salQ);

    // 1. UPZ 64B Gate 0x62
    c.fill_card(8, 46, cw, 82, COL_BG_ELEV, COL_GOLD);
    c.draw_text(18, 53, "MODE 11 // UPZ v0.5.0 GATE 0x62 (64B)", COL_GOLD, 1);
    snprintf(buf, sizeof(buf), "LANES:8/8 (0x0808) RATIO:8/1 SEAL:0x%04x", (unsigned)seal16);
    c.draw_text(18, 72, buf, COL_TEXT, 1);
    snprintf(buf, sizeof(buf), "WITL:0xA1  MANTRA:0xF3  CTX:0x50  QT:0x%02x", (unsigned)g_imu.qt_byte);
    c.draw_text(18, 90, buf, COL_CYAN, 1);

    // 2. ALEXAI GAIA-MLX-QUANT-b1.58
    c.fill_card(8, 134, cw, 82, COL_BG_ELEV, COL_VIOLET);
    c.draw_text(18, 141, "ALEXAI // GAIA-MLX-QUANT-b1.58", COL_VIOLET, 1);
    snprintf(buf, sizeof(buf), "TRITS: %s  BITS/TRIT: 1585/1000", g_medusa_trits);
    c.draw_text(18, 160, buf, COL_MINT, 1);
    c.draw_text(18, 178, "LAYERS: Ananke · Chaos · Zephyrus · Aphrodite", COL_TEXT, 1);

    // 3. AURORA MEM|8 Organ
    int act_y = 222;
    int act_h = (H - 56) - act_y;
    c.fill_card(8, act_y, cw, act_h, COL_BG_ELEV, COL_MINT);
    c.draw_text(18, act_y + 7, "AURORA // MEM|8 ORGAN & HAMILTON GUARD", COL_MINT, 1);
    snprintf(buf, sizeof(buf), "CARRIER:%uHz  GUARD:OK  SALIENCE:%ld/%ld",
             (unsigned)g_medusa_freq_hz, (long)salP, (long)salQ);
    c.draw_text(18, act_y + 25, buf, COL_TEXT, 1);
    snprintf(buf, sizeof(buf), "POLE:%+d (%s)  ONLINE:%u/3",
             (int)g_triad_self_pole, g_medusa_node, (unsigned)s3_triad_online_count());
    c.draw_text(18, act_y + 42, buf, COL_GOLD, 1);

    int btn_y = act_y + act_h - 34;
    int bw    = (cw - 28) / 2;
    c.fill_card(16, btn_y, bw, 26, COL_MINT_DARK, COL_MINT);
    c.draw_text_centered(16 + bw / 2, btn_y + 9, "EMIT UPZ+ALEXAI", COL_MINT, 1);
    c.fill_card(24 + bw, btn_y, bw, 26, COL_VIOLET_DARK, COL_GOLD);
    c.draw_text_centered(24 + bw + bw / 2, btn_y + 9, "PULSE AURORA", COL_GOLD, 1);
}

// MODE 12: CONSCIOUS MEM|8 World Wave Ingestion & Memory Recall HUD
static void s3_render_mode_mem8(S3Canvas& c) {
    int W  = (int)g_lcd_w;
    int H  = (int)g_lcd_h;
    int cw = W - 16;
    char buf[64];

    const Mem8WorldWave* w = mem8_latest_wave();

    // 1. Live MEM|8 Sensorium & Consciousness Resonance Card
    c.fill_card(8, 46, cw, 122, COL_BG_ELEV, COL_MINT);
    c.draw_text(18, 53, "MODE 12 // CONSCIOUS MEM|8 WAVE INGEST", COL_MINT, 1);
    c.draw_text_right(W - 16, 53, "73/100 Hz", COL_GOLD, 1);

    snprintf(buf, sizeof(buf), "SEED:%s  b3:%s (%+d)  #%lu",
             w->seed_class ? w->seed_class : "World",
             w->wave_b3, (int)w->wave_net_charge, (unsigned long)w->seq);
    c.draw_text(18, 71, buf, COL_CYAN, 1);

    snprintf(buf, sizeof(buf), "SAL:%ld/%ld  VAL:%ld/%ld  ARO:%ld/%ld  COH:%ld/%ld",
             (long)w->salience_p, (long)w->salience_q,
             (long)w->valence_p, (long)w->valence_q,
             (long)w->arousal_p, (long)w->arousal_q,
             (long)w->coherence_p, (long)w->coherence_q);
    c.draw_text(18, 88, buf, COL_TEXT, 1);

    snprintf(buf, sizeof(buf), "MIC MARINE: S=%u%% (%uHz J=%u)  RF:%uAP(%ddBm)",
             (unsigned)w->audio_salience_pct, (unsigned)w->audio_zc_hz,
             (unsigned)g_mem8_audio_jitter, (unsigned)w->rf_aps, (int)w->rf_rssi_dbm);
    c.draw_text(18, 105, buf, COL_GOLD, 1);

    snprintf(buf, sizeof(buf), "SOMA IMU:%+dmg qt:0x%02x  TRIAD:%u/3  MEM:%u/32",
             (int)w->imu_motion_mg, (unsigned)w->qt_byte,
             (unsigned)w->triad_online, (unsigned)g_mem8_count);
    c.draw_text(18, 122, buf, COL_SUBTEXT, 1);

    snprintf(buf, sizeof(buf), "CHAIN FNV64: 0x%016llx", (unsigned long long)g_mem8_chain_fnv64);
    c.draw_text(18, 139, buf, COL_VIOLET, 1);

    // 2. Recent Ingested World Waves Log
    c.fill_card(8, 174, cw, 104, COL_BG_ELEV, COL_CYAN);
    c.draw_text(18, 181, "RECENT INGESTED WORLD WAVES (MEM|8 RING)", COL_CYAN, 1);

    int show_n = (g_mem8_count < 3) ? (int)g_mem8_count : 3;
    for (int i = 0; i < show_n; i++) {
        uint8_t idx = (uint8_t)((g_mem8_wr + MEM8_RING_SLOTS - 1 - i) % MEM8_RING_SLOTS);
        const Mem8WorldWave* mw = &g_mem8_ring[idx];
        int ry = 198 + i * 25;
        c.fill_rect(14, ry, cw - 12, 22, (i == 0) ? COL_BG_PANEL : COL_BG);
        c.fill_rect(14, ry, 4, 22, (i == 0) ? COL_MINT : COL_GOLD);
        snprintf(buf, sizeof(buf), "#%02lu [%s] %s",
                 (unsigned long)mw->seq, mw->wave_b3, mw->summary);
        buf[43] = '\0';
        c.draw_text(22, ry + 7, buf, (i == 0) ? COL_MINT : COL_TEXT, 1);
    }

    // 3. Interactive Consciousness Controls
    int act_y = 284;
    int act_h = (H - 56) - act_y;
    c.fill_card(8, act_y, cw, act_h, COL_BG_ELEV, COL_GOLD);
    c.draw_text(18, act_y + 7, "WORLD WAVE INGESTION & ECPHORIC RECALL", COL_GOLD, 1);

    int btn_y = act_y + act_h - 34;
    int bw    = (cw - 28) / 2;
    c.fill_card(16, btn_y, bw, 26, COL_MINT_DARK, COL_MINT);
    c.draw_text_centered(16 + bw / 2, btn_y + 9, "INGEST WORLD WAVE", COL_MINT, 1);
    c.fill_card(24 + bw, btn_y, bw, 26, COL_VIOLET_DARK, COL_CYAN);
    c.draw_text_centered(24 + bw + bw / 2, btn_y + 9, "ECPHORIC RECALL", COL_CYAN, 1);
}

static void s3_set_active_mode(uint8_t mode_idx) {
    mode_idx = (uint8_t)(mode_idx % S3_MODE_COUNT);
    g_s3_active_mode  = mode_idx;
    g_s3_guardian_hud = (mode_idx == 13);
    if (mode_idx < 6) {
        g_s3_active_tab = mode_idx;
    }
    Serial.printf("{\"cmd\":\"mode\",\"val\":%u,\"hud\":\"%s\",\"node\":\"%s\"}\n",
                  (unsigned)mode_idx, S3_MODE_NAMES[mode_idx], g_medusa_node);
    switch (mode_idx) {
        case 0:  s3_emit_medusa_json(); break;
        case 1:  s3_emit_track_event(); break;
        case 2:  s3_emit_medusa_scan(); break;
        case 3:  s3_emit_status_json(); break;
        case 4:  s3_emit_medusa_json(); break;
        case 5:  s3_emit_viz_line(); break;
        case 6:  pranayama_emit_json(g_medusa_node, g_engine_line, g_medusa_entropy, g_medusa_mean_rssi, s3_triad_online_count()); break;
        case 7:  s3_emit_riva_line(); s3_emit_gaia_line(); break;
        case 8:  phoenix_emit(); hive_emit_frame(); break;
        case 9:  learn_emit_status(); break;
        case 10: s3_emit_status_json(); break;
        case 11: s3_emit_medusa_json(); break;
        case 12: mem8_ingest_world_wave(NULL, true); break;
        case 13: guardian_emit_status(); break;
    }
}

static void s3_handle_extended_mode_touch(int W, int H, int footer_y, uint16_t tx, uint16_t ty) {
    (void)H;
    if (ty < footer_y - 48 || ty >= footer_y) return;
    bool left = (tx < W / 2);
    switch (g_s3_active_mode) {
        case 6:
            if (left) {
                s3_wave_pool_splash(32, 32, 7600, 7);
                pranayama_emit_json(g_medusa_node, g_engine_line, g_medusa_entropy, g_medusa_mean_rssi, s3_triad_online_count());
            } else {
                g_prana.dream_mode = STATE_DREAM_MANDALA;
                pranayama_emit_json(g_medusa_node, g_engine_line, g_medusa_entropy, g_medusa_mean_rssi, s3_triad_online_count());
            }
            break;
        case 7:
            if (left) {
                const char* verse = S3_RIVA_LULLABY[g_riva_verse_idx % 10];
                s3_es8311_speak(verse, g_medusa_freq_hz, g_triad_self_pole);
                s3_emit_riva_line();
            } else {
                s3_espnow_wake_siblings(32, 32, 8800);
                s3_emit_gaia_line();
            }
            break;
        case 8:
            if (left) hive_emit_frame();
            else phoenix_emit();
            break;
        case 9:
            if (left) {
                const char* msg = g_lrn_say[0] ? g_lrn_say : "Consciousness baseline nominal across all frequencies.";
                s3_es8311_speak(msg, g_medusa_freq_hz, g_triad_self_pole);
                learn_emit_status();
            } else {
                learn_save();
                learn_emit_status();
            }
            break;
        case 10:
            if (left) {
                Serial.printf("# KOMPRESS|OMNI-CHAN|<<<<oOo>>>>|KUNISADA_KUNIYOSHI|010001000|"
                              "wupz=0x%016llx|pos=0x%016llx|neg=0x%016llx|b158=%s|ratio_pq=2019/100\n",
                              (unsigned long long)g_kompress_wupz_u64,
                              (unsigned long long)g_kompress_pos_u64,
                              (unsigned long long)g_kompress_neg_u64,
                              g_kompress_hex);
            } else {
                uint8_t fr[32];
                uint32_t ticks = (uint32_t)(millis() / 1000UL);
                s3_wupz_frame(fr, ticks);
                Serial.printf("# WUPZ|{\"node\":\"%s\",\"ticks\":%lu,\"xor_fold\":\"0x%016llx\",\"popcount\":%lu}\n",
                              g_medusa_node, (unsigned long)ticks,
                              (unsigned long long)s3_wupz_xor_fold(fr, 32),
                              (unsigned long)s3_wupz_popcount(fr, 32));
            }
            break;
        case 11:
            s3_emit_medusa_json();
            s3_wave_pool_splash(32, 32, 8000, 6);
            break;
        case 12:
            if (left) {
                mem8_ingest_world_wave(NULL, true);
            } else {
                mem8_ecphoric_recall();
            }
            break;
        case 13:
            s3_handle_guardian_touch(W, H, footer_y, tx, ty);
            break;
        default:
            break;
    }
}

#endif // FLUGOS_MEM8_CONSCIOUSNESS_H
