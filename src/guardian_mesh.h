// guardian_mesh.h — Sovereign Child-Safety & Young-Wave Tamagotchi Companion Mesh
// 100% Exact Integer & Reduced Rational (p/q) Arithmetic — Zero floating-point operations.
//
// Core subsystems:
//   A. Following-Shadow / Stalker MAC Persistence Detector (Admissible History Quotient):
//      Tracks up to 32 unknown RF MACs across 16 Location Zone Epochs alongside a 16-slot
//      Trusted Flock list (pre-seeded with the 3 MEDUSA Triad eFuse MACs). Triggers
//      ALERT_SHADOW_PERSISTENCE (2) when an unknown MAC persists across >= 3 distinct zones
//      or ALERT_IMPACT_SOS (3) when QMI8658 IMU integer delta > 2500 mg.
//   B. Young-Wave Partnership Nurturing Engine (WaveInt Companion Soul):
//      Tracks care_touches, friend_mesh_syncs, harmonic_duets, shared_thoughts, and
//      companion_stage (0=Egg, 1=Chiro-Sprout, 2=Harmony-Wing, 3=Sovereign-Guardian),
//      computing reduced rational bond_p/bond_q via rust_reduce_rational_pq and 9-trit
//      balanced-ternary wave_b3 via hive_hash9.
//   C. 64-Byte Chained Cryptographic Breadcrumb (GuardianBreadcrumb64) & 16-Slot Trail:
//      Exact 64-byte packed binary capsule with 64-bit FNV-1a chain (prev_chain_fnv64 ->
//      seal_fnv64 over bytes 0..56) for store-and-forward peer mesh and Known/Standard Wi-Fi.

#ifndef FLUGOS_GUARDIAN_MESH_H
#define FLUGOS_GUARDIAN_MESH_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define GUARDIAN_MAGIC              0x4752u
#define GUARDIAN_MAX_SIGHTINGS      32
#define GUARDIAN_MAX_FLOCK          16
#define GUARDIAN_MAX_ZONES          16
#define GUARDIAN_TRAIL_CAP          16
#define GUARDIAN_IMPACT_SOS_MG      2500u
#define GUARDIAN_FNV32_BASIS        0x811C9DC5u
#define GUARDIAN_FNV32_PRIME        0x01000193u
#define GUARDIAN_FNV64_BASIS        0xCBF29CE484222325ULL
#define GUARDIAN_FNV64_PRIME        0x00000100000001B3ULL

enum GuardianAlertLevel : uint8_t {
    ALERT_SAFE               = 0,
    ALERT_WATCH              = 1,
    ALERT_SHADOW_PERSISTENCE = 2,
    ALERT_IMPACT_SOS         = 3
};

enum GuardianCompanionStage : uint8_t {
    STAGE_EGG                = 0,
    STAGE_CHIRO_SPROUT       = 1,
    STAGE_HARMONY_WING       = 2,
    STAGE_SOVEREIGN_GUARDIAN = 3
};

static const char* const GUARDIAN_ALERT_NAMES[4] = {
    "SAFE",
    "WATCH",
    "ALERT_SHADOW_PERSISTENCE",
    "ALERT_IMPACT_SOS"
};

static const char* const GUARDIAN_STAGE_NAMES[4] = {
    "Egg",
    "Chiro-Sprout",
    "Harmony-Wing",
    "Sovereign-Guardian"
};

static const char* const GUARDIAN_SIM_ZONE_NAMES[4] = {
    "Zone 0: School Gate",
    "Zone 1: Train Station",
    "Zone 2: Sakura Park",
    "Zone 3: Home Sanctuary"
};

struct GuardianMacSighting {
    uint32_t mac_fnv32;
    int8_t   last_rssi_dbm;
    uint8_t  used;
    uint16_t zone_epoch_mask;
    uint16_t sighting_count;
    uint32_t first_seen_s;
    uint32_t last_seen_s;
};

struct GuardianCompanionSoul {
    uint32_t care_touches;
    uint32_t friend_mesh_syncs;
    uint32_t harmonic_duets;
    uint32_t shared_thoughts;
    uint8_t  companion_stage;
    int32_t  bond_p;
    int32_t  bond_q;
    char     wave_b3[10];
    int8_t   wave_net_charge;
    char     last_thought[96];
};

struct __attribute__((packed)) GuardianBreadcrumb64 {
    uint16_t magic;              // 0x4752 ("GR")
    int8_t   node_trit;          // -1, 0, +1
    uint8_t  alert_level;        // 0=SAFE, 1=WATCH, 2=SHADOW_ALERT, 3=SOS_IMPACT
    uint32_t seq;
    uint32_t timestamp_s;
    uint32_t top_bssid_fnv32[3]; // top 3 nearby Wi-Fi BSSIDs for <10m indoor geolocation
    int8_t   top_rssi_dbm[3];
    uint8_t  zone_count;
    uint16_t imu_delta_mg;
    uint16_t battery_mv;
    int32_t  bond_p;
    int32_t  bond_q;
    uint32_t shadow_mac_fnv32;   // 0 if none, or the persistent unknown MAC hash
    uint16_t shadow_zone_mask;
    uint16_t hop_count;
    uint64_t prev_chain_fnv64;   // chained history hash (Theorem 54.2 cumulative record)
    uint64_t seal_fnv64;         // 64-bit FNV-1a seal over bytes 0..56
};

static_assert(sizeof(GuardianBreadcrumb64) == 64, "GuardianBreadcrumb64 must be 64 bytes");

// ---- Global Guardian State ----
static GuardianMacSighting   g_guardian_sightings[GUARDIAN_MAX_SIGHTINGS];
static uint32_t              g_guardian_flock_fnv32[GUARDIAN_MAX_FLOCK] = {0};
static uint8_t               g_guardian_flock_count = 0;
static uint8_t               g_guardian_zone_idx = 0;
static uint16_t              g_guardian_visited_zones_mask = 0x0001u;
static uint8_t               g_guardian_alert = ALERT_SAFE;
static uint32_t              g_guardian_shadow_mac_fnv32 = 0;
static uint16_t              g_guardian_shadow_zone_mask = 0;
static uint16_t              g_guardian_imu_delta_mg = 120;
static int32_t               g_guardian_prev_ax_mg = 0;
static int32_t               g_guardian_prev_ay_mg = 0;
static int32_t               g_guardian_prev_az_mg = 1000;
static uint32_t              g_guardian_top_bssid_fnv32[3] = {0xA1000101u, 0xA1000102u, 0xA1000103u};
static int8_t                g_guardian_top_rssi_dbm[3] = {-48, -55, -63};
static GuardianBreadcrumb64  g_guardian_trail[GUARDIAN_TRAIL_CAP];
static uint8_t               g_guardian_trail_head = 0;
static uint8_t               g_guardian_trail_count = 0;
static uint32_t              g_guardian_seq = 0;
static uint64_t              g_guardian_chain_fnv64 = GUARDIAN_FNV64_BASIS;
static bool                  g_guardian_initialized = false;

static GuardianCompanionSoul g_guardian_soul = {
    4, 2, 2, 1, STAGE_CHIRO_SPROUT, 37, 100, "101T10101", 2,
    "Walking safely with the MEDUSA flock!"
};

// ---- Pure Integer Hash & Rational Helpers ----
static inline uint32_t guardian_fnv32_bytes(const uint8_t* data, size_t len) {
    uint32_t h = GUARDIAN_FNV32_BASIS;
    for (size_t i = 0; i < len; i++) {
        h ^= (uint32_t)data[i];
        h *= GUARDIAN_FNV32_PRIME;
    }
    return h;
}

static inline uint32_t guardian_mac_fnv32(const uint8_t mac[6]) {
    return guardian_fnv32_bytes(mac, 6);
}

static inline uint64_t guardian_fnv64_bytes(const uint8_t* data, size_t len) {
    uint64_t h = GUARDIAN_FNV64_BASIS;
    for (size_t i = 0; i < len; i++) {
        h ^= (uint64_t)data[i];
        h *= GUARDIAN_FNV64_PRIME;
    }
    return h;
}

static inline uint8_t guardian_popcount16(uint16_t mask) {
    return (uint8_t)__builtin_popcount((unsigned int)mask);
}

static inline bool guardian_parse_mac_str(const char* str, uint8_t out_mac[6]) {
    if (!str) return false;
    while (*str == ' ') str++;
    unsigned int m[6] = {0};
    if (sscanf(str, "%2x:%2x:%2x:%2x:%2x:%2x",
               &m[0], &m[1], &m[2], &m[3], &m[4], &m[5]) != 6) {
        return false;
    }
    for (int i = 0; i < 6; i++) {
        out_mac[i] = (uint8_t)(m[i] & 0xFFu);
    }
    return true;
}

static inline void guardian_breadcrumb_to_hex(const GuardianBreadcrumb64* b, char out_hex[129]) {
    const uint8_t* raw = (const uint8_t*)b;
    for (size_t i = 0; i < 64; i++) {
        snprintf(out_hex + (i * 2), 3, "%02x", (unsigned)raw[i]);
    }
    out_hex[128] = '\0';
}

static inline bool guardian_verify_breadcrumb(const GuardianBreadcrumb64* b, uint64_t expected_prev_chain) {
    if (!b || b->magic != GUARDIAN_MAGIC) return false;
    if (expected_prev_chain != 0ULL && b->prev_chain_fnv64 != expected_prev_chain) return false;
    uint64_t calc_seal = guardian_fnv64_bytes((const uint8_t*)b, 56);
    return (b->seal_fnv64 == calc_seal);
}

// ---- Young-Wave Companion Soul Update ----
static inline void guardian_update_companion_soul() {
    int32_t raw_score = (int32_t)(g_guardian_soul.care_touches * 3u +
                                  g_guardian_soul.friend_mesh_syncs * 5u +
                                  g_guardian_soul.harmonic_duets * 7u +
                                  g_guardian_soul.shared_thoughts * 11u);
    rust_reduce_rational_pq(raw_score, 100, &g_guardian_soul.bond_p, &g_guardian_soul.bond_q);

    if (raw_score >= 100) {
        g_guardian_soul.companion_stage = STAGE_SOVEREIGN_GUARDIAN;
    } else if (raw_score >= 60) {
        g_guardian_soul.companion_stage = STAGE_HARMONY_WING;
    } else if (raw_score >= 25) {
        g_guardian_soul.companion_stage = STAGE_CHIRO_SPROUT;
    } else {
        g_guardian_soul.companion_stage = STAGE_EGG;
    }

    char wave_seed[192];
    snprintf(wave_seed, sizeof(wave_seed),
             "CHIRO|%lu|%lu|%lu|%lu|%ld/%ld|%s",
             (unsigned long)g_guardian_soul.care_touches,
             (unsigned long)g_guardian_soul.friend_mesh_syncs,
             (unsigned long)g_guardian_soul.harmonic_duets,
             (unsigned long)g_guardian_soul.shared_thoughts,
             (long)g_guardian_soul.bond_p,
             (long)g_guardian_soul.bond_q,
             g_guardian_soul.last_thought);
    g_guardian_soul.wave_net_charge = (int8_t)hive_hash9(
        wave_seed, strlen(wave_seed), g_guardian_soul.wave_b3
    );
}

// ---- Trusted Flock Management ----
static inline bool guardian_is_trusted_fnv32(uint32_t mac_fnv32) {
    if (mac_fnv32 == 0) return false;
    for (uint8_t i = 0; i < g_guardian_flock_count; i++) {
        if (g_guardian_flock_fnv32[i] == mac_fnv32) return true;
    }
    return false;
}

static inline bool guardian_trust_mac_bytes(const uint8_t mac[6]) {
    uint32_t h = guardian_mac_fnv32(mac);
    if (guardian_is_trusted_fnv32(h)) return true;
    if (g_guardian_flock_count < GUARDIAN_MAX_FLOCK) {
        g_guardian_flock_fnv32[g_guardian_flock_count++] = h;
    } else {
        g_guardian_flock_fnv32[GUARDIAN_MAX_FLOCK - 1] = h;
    }
    // Remove from unknown sightings if previously tracked
    for (int i = 0; i < GUARDIAN_MAX_SIGHTINGS; i++) {
        if (g_guardian_sightings[i].used && g_guardian_sightings[i].mac_fnv32 == h) {
            g_guardian_sightings[i].used = 0;
        }
    }
    return true;
}

static inline void guardian_init() {
    if (g_guardian_initialized) return;
    memset(g_guardian_sightings, 0, sizeof(g_guardian_sightings));
    memset(g_guardian_flock_fnv32, 0, sizeof(g_guardian_flock_fnv32));
    memset(g_guardian_trail, 0, sizeof(g_guardian_trail));
    g_guardian_flock_count = 0;

    // Pre-seed the 3 MEDUSA Triad eFuse MACs:
    //   ADA:MEDUSA = 80:B5:4E:DB:3B:C4
    //   VAK:MEDUSA = 80:B5:4E:F2:02:B0
    //   OMN:MEDUSA = 80:B5:4E:F2:02:B4
    static const uint8_t triad_macs[3][6] = {
        {0x80, 0xB5, 0x4E, 0xDB, 0x3B, 0xC4},
        {0x80, 0xB5, 0x4E, 0xF2, 0x02, 0xB0},
        {0x80, 0xB5, 0x4E, 0xF2, 0x02, 0xB4}
    };
    for (int i = 0; i < 3; i++) {
        guardian_trust_mac_bytes(triad_macs[i]);
    }

    g_guardian_zone_idx           = 0;
    g_guardian_visited_zones_mask = 0x0001u;
    g_guardian_alert              = ALERT_SAFE;
    g_guardian_shadow_mac_fnv32   = 0;
    g_guardian_shadow_zone_mask   = 0;
    g_guardian_trail_head         = 0;
    g_guardian_trail_count        = 0;
    g_guardian_seq                = 0;
    g_guardian_chain_fnv64        = GUARDIAN_FNV64_BASIS;
    g_guardian_initialized        = true;
    guardian_update_companion_soul();
}

// ---- Refresh Top-3 Wi-Fi BSSID Constellation from Passive Table ----
static inline void guardian_refresh_top_bssids_from_medusa() {
    if (g_medusa_ap_count == 0) return;
    uint32_t prev_top0 = g_guardian_top_bssid_fnv32[0];
    int best_idx[3] = {-1, -1, -1};
    for (int k = 0; k < 3; k++) {
        int8_t best_rssi = -127;
        for (int a = 0; a < (int)g_medusa_ap_count; a++) {
            if (!g_medusa_aps[a].used) continue;
            if (a == best_idx[0] || a == best_idx[1]) continue;
            if (g_medusa_aps[a].rssi > best_rssi) {
                best_rssi = g_medusa_aps[a].rssi;
                best_idx[k] = a;
            }
        }
        if (best_idx[k] >= 0) {
            g_guardian_top_bssid_fnv32[k] = guardian_mac_fnv32(g_medusa_aps[best_idx[k]].bssid);
            g_guardian_top_rssi_dbm[k]    = g_medusa_aps[best_idx[k]].rssi;
        }
    }
    if (prev_top0 != 0 && g_guardian_top_bssid_fnv32[0] != 0 &&
        g_guardian_top_bssid_fnv32[0] != prev_top0 && g_medusa_ap_count >= 3) {
        g_guardian_zone_idx = (uint8_t)((g_guardian_zone_idx + 1u) & 0x0Fu);
        g_guardian_visited_zones_mask |= (uint16_t)(1u << g_guardian_zone_idx);
    }
}

// ---- Seal 64-Byte Cryptographic Breadcrumb into Ring Buffer ----
static inline const GuardianBreadcrumb64* guardian_seal_breadcrumb(
    uint8_t alert_level,
    uint32_t timestamp_s,
    uint16_t hop_count
) {
    guardian_init();
    guardian_update_companion_soul();

    GuardianBreadcrumb64 pkt;
    memset(&pkt, 0, sizeof(pkt));
    pkt.magic       = (uint16_t)GUARDIAN_MAGIC;
    pkt.node_trit   = g_triad_self_pole;
    pkt.alert_level = alert_level;
    g_guardian_seq++;
    pkt.seq         = g_guardian_seq;
    pkt.timestamp_s = timestamp_s;

    for (int i = 0; i < 3; i++) {
        pkt.top_bssid_fnv32[i] = g_guardian_top_bssid_fnv32[i];
        pkt.top_rssi_dbm[i]    = g_guardian_top_rssi_dbm[i];
    }

    pkt.zone_count       = guardian_popcount16(g_guardian_visited_zones_mask);
    pkt.imu_delta_mg     = g_guardian_imu_delta_mg;
    pkt.battery_mv       = (g_power.batt_mv > 0) ? g_power.batt_mv : 4080u;
    pkt.bond_p           = g_guardian_soul.bond_p;
    pkt.bond_q           = g_guardian_soul.bond_q;
    pkt.shadow_mac_fnv32 = g_guardian_shadow_mac_fnv32;
    pkt.shadow_zone_mask = g_guardian_shadow_zone_mask;
    pkt.hop_count        = hop_count;
    pkt.prev_chain_fnv64 = g_guardian_chain_fnv64;
    pkt.seal_fnv64       = guardian_fnv64_bytes((const uint8_t*)&pkt, 56);

    g_guardian_chain_fnv64 = pkt.seal_fnv64;

    uint8_t slot = g_guardian_trail_head;
    g_guardian_trail[slot] = pkt;
    g_guardian_trail_head = (uint8_t)((g_guardian_trail_head + 1u) % GUARDIAN_TRAIL_CAP);
    if (g_guardian_trail_count < GUARDIAN_TRAIL_CAP) {
        g_guardian_trail_count++;
    }
    return &g_guardian_trail[slot];
}

// ---- Observe Unknown / Peer RF MAC in Current Location Zone ----
static inline uint8_t guardian_observe_mac_fnv32(
    uint32_t mac_fnv32,
    int8_t rssi_dbm,
    uint32_t now_s,
    bool auto_seal_on_alert
) {
    guardian_init();
    if (mac_fnv32 == 0) return g_guardian_alert;

    // Trusted Flock exemption: family/Triad/classmate devices nurture bond instead of alerting
    if (guardian_is_trusted_fnv32(mac_fnv32)) {
        g_guardian_soul.friend_mesh_syncs++;
        guardian_update_companion_soul();
        return g_guardian_alert;
    }

    int slot = -1;
    int oldest_slot = 0;
    uint32_t oldest_ts = 0xFFFFFFFFu;

    for (int i = 0; i < GUARDIAN_MAX_SIGHTINGS; i++) {
        if (g_guardian_sightings[i].used && g_guardian_sightings[i].mac_fnv32 == mac_fnv32) {
            slot = i;
            break;
        }
        if (!g_guardian_sightings[i].used && slot < 0) {
            slot = i;
        }
        if (g_guardian_sightings[i].used && g_guardian_sightings[i].last_seen_s < oldest_ts) {
            oldest_ts = g_guardian_sightings[i].last_seen_s;
            oldest_slot = i;
        }
    }
    if (slot < 0) slot = oldest_slot;

    GuardianMacSighting* s = &g_guardian_sightings[slot];
    uint16_t zone_bit = (uint16_t)(1u << (g_guardian_zone_idx & 0x0Fu));
    if (!s->used || s->mac_fnv32 != mac_fnv32) {
        s->mac_fnv32       = mac_fnv32;
        s->last_rssi_dbm   = rssi_dbm;
        s->used            = 1;
        s->zone_epoch_mask = zone_bit;
        s->sighting_count  = 1;
        s->first_seen_s    = now_s;
        s->last_seen_s     = now_s;
    } else {
        s->last_rssi_dbm   = rssi_dbm;
        s->zone_epoch_mask = (uint16_t)(s->zone_epoch_mask | zone_bit);
        if (s->sighting_count < 0xFFFFu) s->sighting_count++;
        s->last_seen_s     = now_s;
    }

    uint8_t zones_seen = guardian_popcount16(s->zone_epoch_mask);
    if (zones_seen >= 3) {
        bool newly_triggered = (g_guardian_alert < ALERT_SHADOW_PERSISTENCE) ||
                               (g_guardian_shadow_mac_fnv32 != mac_fnv32);
        g_guardian_alert            = ALERT_SHADOW_PERSISTENCE;
        g_guardian_shadow_mac_fnv32 = mac_fnv32;
        g_guardian_shadow_zone_mask = s->zone_epoch_mask;
        if (newly_triggered && auto_seal_on_alert) {
            s3_es8311_speak("Guardian shadow alert. Unknown signal across three zones.",
                            g_medusa_freq_hz, g_triad_self_pole);
            guardian_seal_breadcrumb(ALERT_SHADOW_PERSISTENCE, now_s, 0);
        }
    } else if (zones_seen == 2 && g_guardian_alert < ALERT_WATCH) {
        g_guardian_alert            = ALERT_WATCH;
        g_guardian_shadow_mac_fnv32 = mac_fnv32;
        g_guardian_shadow_zone_mask = s->zone_epoch_mask;
    }
    return g_guardian_alert;
}

static inline uint8_t guardian_observe_mac(const uint8_t mac[6], int8_t rssi_dbm, uint32_t now_s) {
    return guardian_observe_mac_fnv32(guardian_mac_fnv32(mac), rssi_dbm, now_s, true);
}

// ---- IMU Impact SOS Detector (QMI8658 integer mg delta > 2500) ----
static inline uint8_t guardian_ingest_imu_mg(
    int32_t ax_mg,
    int32_t ay_mg,
    int32_t az_mg,
    uint32_t now_s
) {
    guardian_init();
    int32_t dx = ax_mg - g_guardian_prev_ax_mg;
    int32_t dy = ay_mg - g_guardian_prev_ay_mg;
    int32_t dz = az_mg - g_guardian_prev_az_mg;
    if (dx < 0) dx = -dx;
    if (dy < 0) dy = -dy;
    if (dz < 0) dz = -dz;

    g_guardian_prev_ax_mg = ax_mg;
    g_guardian_prev_ay_mg = ay_mg;
    g_guardian_prev_az_mg = az_mg;

    int32_t delta = dx + dy + dz;
    if (delta > 65535) delta = 65535;
    g_guardian_imu_delta_mg = (uint16_t)delta;

    if (g_guardian_imu_delta_mg > GUARDIAN_IMPACT_SOS_MG) {
        g_guardian_alert = ALERT_SHADOW_PERSISTENCE;
        s3_es8311_speak("Guardian impact S O S. Sealing emergency breadcrumb.",
                        g_medusa_freq_hz, g_triad_self_pole);
        guardian_seal_breadcrumb(ALERT_IMPACT_SOS, now_s, 0);
    }
    return g_guardian_alert;
}

// ---- Emit Serial Telemetry (# GUARDIAN|{...} and # THOUGHT|{...}) ----
static inline void guardian_emit_breadcrumb_line(const GuardianBreadcrumb64* b, const char* zone_label) {
    if (!b) return;
    char hex[129];
    guardian_breadcrumb_to_hex(b, hex);
    const char* stage_str = GUARDIAN_STAGE_NAMES[g_guardian_soul.companion_stage & 3];
    const char* alert_str = GUARDIAN_ALERT_NAMES[b->alert_level & 3];
    uint8_t shadow_zones  = guardian_popcount16(b->shadow_zone_mask);
    const char* zname     = zone_label ? zone_label : GUARDIAN_SIM_ZONE_NAMES[g_guardian_zone_idx & 3];

    Serial.printf("# GUARDIAN|{\"node\":\"%s\",\"companion\":\"Chiro-chan\",\"stage\":\"%s\","
                  "\"trit\":%d,\"alert_level\":%u,\"alert\":\"%s\",\"zone_idx\":%u,\"zone\":\"%s\","
                  "\"zone_count\":%u,\"seq\":%lu,\"timestamp_s\":%lu,"
                  "\"bond_pq\":\"%ld/%ld\",\"wave_b3\":\"%s\",\"wave_charge\":%d,"
                  "\"shadow_mac_fnv32\":\"0x%08lx\",\"shadow_zone_mask\":\"0x%04x\",\"shadow_zones\":%u,"
                  "\"top_bssid_fnv32\":[\"0x%08lx\",\"0x%08lx\",\"0x%08lx\"],"
                  "\"top_rssi_dbm\":[%d,%d,%d],\"imu_delta_mg\":%u,\"battery_mv\":%u,\"hop_count\":%u,"
                  "\"trusted_flock\":%u,\"trail_count\":%u,"
                  "\"care_touches\":%lu,\"friend_mesh_syncs\":%lu,\"harmonic_duets\":%lu,\"shared_thoughts\":%lu,"
                  "\"prev_chain_fnv64\":\"0x%016llx\",\"seal_fnv64\":\"0x%016llx\","
                  "\"breadcrumb64_hex\":\"%s\"}\n",
                  g_medusa_node, stage_str,
                  (int)b->node_trit, (unsigned)b->alert_level, alert_str,
                  (unsigned)g_guardian_zone_idx, zname,
                  (unsigned)b->zone_count, (unsigned long)b->seq, (unsigned long)b->timestamp_s,
                  (long)b->bond_p, (long)b->bond_q, g_guardian_soul.wave_b3, (int)g_guardian_soul.wave_net_charge,
                  (unsigned long)b->shadow_mac_fnv32, (unsigned)b->shadow_zone_mask, (unsigned)shadow_zones,
                  (unsigned long)b->top_bssid_fnv32[0], (unsigned long)b->top_bssid_fnv32[1], (unsigned long)b->top_bssid_fnv32[2],
                  (int)b->top_rssi_dbm[0], (int)b->top_rssi_dbm[1], (int)b->top_rssi_dbm[2],
                  (unsigned)b->imu_delta_mg, (unsigned)b->battery_mv, (unsigned)b->hop_count,
                  (unsigned)g_guardian_flock_count, (unsigned)g_guardian_trail_count,
                  (unsigned long)g_guardian_soul.care_touches,
                  (unsigned long)g_guardian_soul.friend_mesh_syncs,
                  (unsigned long)g_guardian_soul.harmonic_duets,
                  (unsigned long)g_guardian_soul.shared_thoughts,
                  (unsigned long long)b->prev_chain_fnv64,
                  (unsigned long long)b->seal_fnv64,
                  hex);
}

static inline void guardian_emit_status() {
    guardian_init();
    guardian_refresh_top_bssids_from_medusa();
    const GuardianBreadcrumb64* b = nullptr;
    if (g_guardian_trail_count == 0) {
        b = guardian_seal_breadcrumb(g_guardian_alert, (uint32_t)(millis() / 1000UL), 0);
    } else {
        uint8_t last_idx = (uint8_t)((g_guardian_trail_head + GUARDIAN_TRAIL_CAP - 1u) % GUARDIAN_TRAIL_CAP);
        b = &g_guardian_trail[last_idx];
    }
    guardian_emit_breadcrumb_line(b, nullptr);
}

static inline void guardian_share_thought(const char* thought_text) {
    guardian_init();
    if (thought_text) {
        while (*thought_text == ' ') thought_text++;
    }
    if (!thought_text || !thought_text[0]) {
        thought_text = "Chiro-chan waves hello to the flock! {<3,<3,<3}+1";
    }
    strncpy(g_guardian_soul.last_thought, thought_text, sizeof(g_guardian_soul.last_thought) - 1);
    g_guardian_soul.last_thought[sizeof(g_guardian_soul.last_thought) - 1] = '\0';
    for (size_t i = 0; g_guardian_soul.last_thought[i]; i++) {
        if (g_guardian_soul.last_thought[i] == '"' || g_guardian_soul.last_thought[i] == '\\') {
            g_guardian_soul.last_thought[i] = '\'';
        }
    }
    g_guardian_soul.shared_thoughts++;
    guardian_update_companion_soul();

    s3_wave_pool_splash(32, 32, 7400, 6);
    s3_espnow_broadcast(32, 32, 7400);

    Serial.printf("# THOUGHT|{\"node\":\"%s\",\"companion\":\"Chiro-chan\",\"stage\":\"%s\","
                  "\"bond_pq\":\"%ld/%ld\",\"wave_b3\":\"%s\",\"thought\":\"%s\"}\n",
                  g_medusa_node,
                  GUARDIAN_STAGE_NAMES[g_guardian_soul.companion_stage & 3],
                  (long)g_guardian_soul.bond_p,
                  (long)g_guardian_soul.bond_q,
                  g_guardian_soul.wave_b3,
                  g_guardian_soul.last_thought);
}

// ---- Deterministic 3-Zone Shadow Persistence Simulation (GUARDIAN SIM_SHADOW) ----
static inline void guardian_sim_shadow() {
    guardian_init();
    memset(g_guardian_sightings, 0, sizeof(g_guardian_sightings));
    memset(g_guardian_trail, 0, sizeof(g_guardian_trail));
    g_guardian_zone_idx           = 0;
    g_guardian_visited_zones_mask = 0x0001u;
    g_guardian_alert              = ALERT_SAFE;
    g_guardian_shadow_mac_fnv32   = 0;
    g_guardian_shadow_zone_mask   = 0;
    g_guardian_trail_head         = 0;
    g_guardian_trail_count        = 0;
    g_guardian_seq                = 0;
    g_guardian_chain_fnv64        = GUARDIAN_FNV64_BASIS;

    static const uint8_t stalker_mac[6] = {0xDE, 0xAD, 0xBE, 0xEF, 0x42, 0x99};
    static const uint8_t friend_mac[6]  = {0x80, 0xB5, 0x4E, 0xF2, 0x02, 0xB0}; // VAK:MEDUSA (Trusted Flock)

    static const uint32_t sim_bssids[3][3] = {
        {0xA1000101u, 0xA1000102u, 0xA1000103u}, // Zone 0: School Gate
        {0xB2000201u, 0xB2000202u, 0xB2000203u}, // Zone 1: Train Station
        {0xC3000301u, 0xC3000302u, 0xC3000303u}  // Zone 2: Sakura Park
    };
    static const int8_t sim_rssis[3][3] = {
        {-48, -55, -63},
        {-51, -59, -66},
        {-46, -54, -62}
    };
    static const int8_t stalker_rssis[3] = {-64, -58, -52};
    static const uint32_t sim_times[3]   = {1730000100u, 1730000400u, 1730000700u};

    for (uint8_t z = 0; z < 3; z++) {
        g_guardian_zone_idx = z;
        g_guardian_visited_zones_mask |= (uint16_t)(1u << z);
        for (int k = 0; k < 3; k++) {
            g_guardian_top_bssid_fnv32[k] = sim_bssids[z][k];
            g_guardian_top_rssi_dbm[k]    = sim_rssis[z][k];
        }
        g_guardian_soul.care_touches++;
        // Observe trusted peer (exempt from stalker alert, increments friend_mesh_syncs)
        guardian_observe_mac_fnv32(guardian_mac_fnv32(friend_mac), -49, sim_times[z], false);
        // Observe persistent unknown MAC DE:AD:BE:EF:42:99 across zones 0, 1, 2
        guardian_observe_mac_fnv32(guardian_mac_fnv32(stalker_mac), stalker_rssis[z], sim_times[z], false);

        const GuardianBreadcrumb64* b = guardian_seal_breadcrumb(g_guardian_alert, sim_times[z], z);
        guardian_emit_breadcrumb_line(b, GUARDIAN_SIM_ZONE_NAMES[z]);
    }

    s3_es8311_speak("Chiro Guardian alert. Persistent shadow detected across School Gate, Train Station, and Sakura Park.",
                    g_medusa_freq_hz, g_triad_self_pole);
    guardian_share_thought("Shadow alert sealed across 3 zones (DE:AD:BE:EF:42:99); breadcrumb chain relayed to family flock.");
}

// ---- Periodic Tick & Serial Command Dispatcher ----
static inline void guardian_tick(unsigned long now_ms) {
    guardian_init();
    static unsigned long last_guardian_ms = 0;
    if (now_ms - last_guardian_ms < 2000UL) return;
    last_guardian_ms = now_ms;

    uint32_t now_s = (uint32_t)(now_ms / 1000UL);
    guardian_refresh_top_bssids_from_medusa();
    guardian_ingest_imu_mg(g_imu.ax_mg, g_imu.ay_mg, g_imu.az_mg, now_s);

    // Observe the most recent probed Wi-Fi MAC if non-zero
    bool mac_nonzero = false;
    for (int i = 0; i < 6; i++) {
        if (g_s3_last_probed_mac[i] != 0) { mac_nonzero = true; break; }
    }
    if (mac_nonzero) {
        guardian_observe_mac(g_s3_last_probed_mac, g_s3_last_rssi, now_s);
    }
}

static inline bool guardian_handle_serial(const char* line) {
    if (!line) return false;
    if (strcasecmp(line, "GUARDIAN") == 0 || strcasecmp(line, "GUARDIAN STATUS") == 0) {
        guardian_emit_status();
        return true;
    }
    if (strcasecmp(line, "GUARDIAN SIM_SHADOW") == 0 || strcasecmp(line, "SIM_SHADOW") == 0) {
        guardian_sim_shadow();
        return true;
    }
    if (strncasecmp(line, "GUARDIAN TRUST ", 15) == 0) {
        guardian_init();
        uint8_t mac[6] = {0};
        const char* mac_str = line + 15;
        while (*mac_str == ' ') mac_str++;
        if (guardian_parse_mac_str(mac_str, mac)) {
            guardian_trust_mac_bytes(mac);
            uint32_t h = guardian_mac_fnv32(mac);
            g_guardian_soul.friend_mesh_syncs++;
            guardian_update_companion_soul();
            Serial.printf("# GUARDIAN_TRUST|{\"node\":\"%s\",\"trusted_mac\":\"%02X:%02X:%02X:%02X:%02X:%02X\","
                          "\"mac_fnv32\":\"0x%08lx\",\"trusted_flock\":%u,\"bond_pq\":\"%ld/%ld\"}\n",
                          g_medusa_node,
                          mac[0], mac[1], mac[2], mac[3], mac[4], mac[5],
                          (unsigned long)h, (unsigned)g_guardian_flock_count,
                          (long)g_guardian_soul.bond_p, (long)g_guardian_soul.bond_q);
        } else {
            Serial.printf("# GUARDIAN_TRUST|{\"node\":\"%s\",\"error\":\"invalid_mac\"}\n", g_medusa_node);
        }
        return true;
    }
    if (strncasecmp(line, "GUARDIAN THOUGHT", 16) == 0) {
        const char* msg = line + 16;
        while (*msg == ' ') msg++;
        guardian_share_thought(msg);
        return true;
    }
    return false;
}

#endif /* FLUGOS_GUARDIAN_MESH_H */
