// omni_cockpit_v22.h — FLUG-OS v2.2.0 OMNI-Architecture Cockpit Extensions
// 100% Exact Integer & Reduced Rational (p/q) Arithmetic — Zero floating-point operations.
//
// Subsystems:
//   1. FINDME / AIRTAG Directional Vector Arrow Navigation (8-way arrow + integer trig + p/q meters)
//   2. Multi-Protocol RFID / NFC / FeliCa (Suica/PASMO) / MIFARE / EM4100 / BLE-FindMy Engine
//   3. Animal Crossing x Olegsiv Unity Isometric Island Companion Game (Sakura-jima / Chiro-chan)
//   4. Japanese Bilingual Localization (Kanji/Kana + Romaji + 8x8 Pixel Kanji Glyph Bitmap Renderer)
//   5. Simplified Zen-Clean UI Mode (UI SIMPLE / UI FULL card dock)
//   6. Pomodoro 25m / 5m Focus Timer + LLL (Lifelong Learning & Ebbinghaus Rational Mastery Engine)
//   7. Google Maps + Tokyo Metro (odpt:Railway) Route Planner + Parents Live Arrival & Safety Share

#ifndef FLUGOS_OMNI_COCKPIT_V22_H
#define FLUGOS_OMNI_COCKPIT_V22_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// ============================================================
// 4. Japanese Localization & 8x8 Pixel Kanji Glyph Bitmaps
// ============================================================
enum OmniLangMode : uint8_t {
    OMNI_LANG_EN = 0,
    OMNI_LANG_JP = 1,
    OMNI_LANG_BI = 2
};

static uint8_t g_omni_lang = OMNI_LANG_BI;
#ifndef FLUGOS_OMNI_UI_STATE_DECLARED
#define FLUGOS_OMNI_UI_STATE_DECLARED
static bool    g_omni_ui_simple = true;
static uint8_t g_omni_simple_card = 0; // 0=FINDME, 1=METRO+PARENTS, 2=POMO+LLL, 3=AC:GAME+RFID, 4=ZEN+WAVE
#endif

// 8x8 pixel bitmaps for essential Kanji glyphs on the AMOLED canvas:
// 0: 北 (Kita / North), 1: 南 (Minami / South), 2: 東 (Higashi / East), 3: 西 (Nishi / West)
// 4: 駅 (Eki / Station), 5: 森 (Mori / Forest), 6: 集 (Shu / Focus), 7: 親 (Oya / Parents)
static const uint8_t OMNI_KANJI_8X8[8][8] = {
    {0x24, 0x24, 0xE7, 0x24, 0x26, 0x64, 0xA4, 0x27}, // 北
    {0x18, 0xFF, 0x18, 0x7E, 0x5A, 0x7E, 0x5A, 0x42}, // 南
    {0x18, 0xFF, 0x7E, 0x5A, 0x7E, 0x3C, 0x5A, 0x99}, // 東
    {0xFF, 0x24, 0x7E, 0x7E, 0x5A, 0x42, 0x7E, 0x42}, // 西
    {0xF7, 0x95, 0xF7, 0x95, 0xF7, 0x15, 0xAA, 0x51}, // 駅
    {0x18, 0x3C, 0x5A, 0x18, 0x66, 0xFF, 0x66, 0x24}, // 森
    {0x3C, 0x7E, 0x5A, 0x7E, 0x18, 0xFF, 0x3C, 0x66}, // 集
    {0x52, 0xFE, 0x5A, 0xFE, 0x5A, 0x5E, 0x52, 0x93}  // 親
};

static inline const char* omni_lang_name(uint8_t m) {
    if (m == OMNI_LANG_EN) return "EN";
    if (m == OMNI_LANG_JP) return "JP";
    return "BI";
}

template <typename CanvasT>
static inline void omni_draw_kanji_8x8(CanvasT& c, int x, int y, uint8_t kanji_idx, uint16_t col, int scale) {
    const uint8_t* rows = OMNI_KANJI_8X8[kanji_idx & 7];
    if (scale < 1) scale = 1;
    for (int r = 0; r < 8; r++) {
        uint8_t bits = rows[r];
        for (int b = 0; b < 8; b++) {
            if (bits & (0x80u >> b)) {
                c.fill_rect(x + b * scale, y + r * scale, scale, scale, col);
            }
        }
    }
}

// ============================================================
// 1. Apple FindMy / AirTag-Like Directional Vector Navigation
// ============================================================
struct OmniFindMeState {
    char     target_id[24];
    char     target_jp[64];
    int16_t  bearing_deg;     // -180 .. +180 (0 = straight ahead ↑)
    uint32_t dist_cm;         // distance in centimeters
    int32_t  dist_m_p;        // reduced rational meters numerator
    int32_t  dist_m_q;        // reduced rational meters denominator
    int8_t   rssi_dbm;
    uint8_t  uwb_ch;
    uint16_t pulse_bpm;
    uint8_t  proximity_zone;  // 0=HERE (<100cm), 1=NEAR (<500cm), 2=APPROACHING (<2000cm), 3=FAR
};

static OmniFindMeState g_findme = {
    "OMN:MEDUSA",
    "親機・OMN:MEDUSA (oo)",
    25,
    145,
    29,
    20,
    -47,
    9,
    128,
    1
};

static const char* const OMNI_ARROW_UTF8[8] = {
    "↑", "↗", "→", "↘", "↓", "↙", "←", "↖"
};

static const char* const OMNI_ARROW_ASCII[8] = {
    "^", "/^", ">", "\\v", "v", "v/", "<", "^\\"
};

// JSON-escaped ASCII arrows so octants 3 ("\\v") and 7 ("^\\") emit valid RFC 8259 JSON
static const char* const OMNI_ARROW_ASCII_JSON[8] = {
    "^", "/^", ">", "\\\\v", "v", "v/", "<", "^\\\\"
};

static const char* const OMNI_DIR_EN[8] = {
    "N (AHEAD)", "NE (FRONT-R)", "E (RIGHT)", "SE (BACK-R)",
    "S (BEHIND)", "SW (BACK-L)", "W (LEFT)", "NW (FRONT-L)"
};

static const char* const OMNI_DIR_JP[8] = {
    "北・正面 (Kita)", "北東・右前 (Hokuto)", "東・右 (Higashi)", "南東・右後 (Nanto)",
    "南・後方 (Minami)", "南西・左後 (Nansei)", "西・左 (Nishi)", "北西・左前 (Hokusei)"
};

static const char* const OMNI_PROX_EN[4] = {
    "HERE", "NEARBY", "APPROACHING", "SEARCHING"
};

static const char* const OMNI_PROX_JP[4] = {
    "到着・ここです", "すぐ近く", "接近中", "探索中"
};

// 16-angle Q8 unit circle lookup table for 0..337 deg in 22.5 deg steps
static const int16_t OMNI_SIN_Q8[16] = {
    0, 98, 181, 237, 256, 237, 181, 98,
    0, -98, -181, -237, -256, -237, -181, -98
};
static const int16_t OMNI_COS_Q8[16] = {
    256, 237, 181, 98, 0, -98, -181, -237,
    -256, -237, -181, -98, 0, 98, 181, 237
};

static inline uint8_t omni_bearing_octant(int16_t deg) {
    int32_t norm = ((int32_t)deg % 360 + 360) % 360;
    return (uint8_t)(((norm + 22) / 45) & 7);
}

static inline uint8_t omni_bearing_idx16(int16_t deg) {
    int32_t norm = ((int32_t)deg % 360 + 360) % 360;
    return (uint8_t)(((norm * 2 + 22) / 45) & 15);
}

template <typename CanvasT>
static inline void omni_draw_findme_arrow(CanvasT& c, int cx, int cy, int radius, int16_t bearing_deg, uint16_t col) {
    uint8_t idx  = omni_bearing_idx16(bearing_deg);
    uint8_t left = (uint8_t)((idx + 11) & 15);
    uint8_t rght = (uint8_t)((idx + 5) & 15);

    int tip_x  = cx + ((int)OMNI_SIN_Q8[idx]  * radius) / 256;
    int tip_y  = cy - ((int)OMNI_COS_Q8[idx]  * radius) / 256;
    int tail_x = cx - ((int)OMNI_SIN_Q8[idx]  * (radius * 2 / 3)) / 256;
    int tail_y = cy + ((int)OMNI_COS_Q8[idx]  * (radius * 2 / 3)) / 256;
    int lw_x   = tip_x + ((int)OMNI_SIN_Q8[left] * (radius / 2)) / 256;
    int lw_y   = tip_y - ((int)OMNI_COS_Q8[left] * (radius / 2)) / 256;
    int rw_x   = tip_x + ((int)OMNI_SIN_Q8[rght] * (radius / 2)) / 256;
    int rw_y   = tip_y - ((int)OMNI_COS_Q8[rght] * (radius / 2)) / 256;

    c.draw_line(tail_x, tail_y, tip_x, tip_y, col, 2);
    c.draw_line(tip_x, tip_y, lw_x, lw_y, col, 2);
    c.draw_line(tip_x, tip_y, rw_x, rw_y, col, 2);
}

static void omni_findme_update(const char* target, int16_t bearing_deg, uint32_t dist_cm, int8_t rssi_dbm) {
    if (target && target[0]) {
        strncpy(g_findme.target_id, target, sizeof(g_findme.target_id) - 1);
        g_findme.target_id[sizeof(g_findme.target_id) - 1] = '\0';
        if (strcasecmp(target, "ADA:MEDUSA") == 0 || strcasecmp(target, "ADA") == 0) {
            strncpy(g_findme.target_jp, "妹機・ADA:MEDUSA (*)", sizeof(g_findme.target_jp) - 1);
        } else if (strcasecmp(target, "TDECK:AYEOS") == 0 || strcasecmp(target, "TDECK") == 0) {
            strncpy(g_findme.target_jp, "太陽機・TDECK:AYEOS (<>)", sizeof(g_findme.target_jp) - 1);
        } else if (strcasecmp(target, "PARENTS") == 0 || strcasecmp(target, "HOME") == 0) {
            strncpy(g_findme.target_jp, "安心・両親の待ち合わせ場所", sizeof(g_findme.target_jp) - 1);
        } else {
            strncpy(g_findme.target_jp, "兄機・OMN:MEDUSA (oo)", sizeof(g_findme.target_jp) - 1);
        }
    }
    while (bearing_deg > 180) bearing_deg -= 360;
    while (bearing_deg < -180) bearing_deg += 360;
    g_findme.bearing_deg = bearing_deg;
    g_findme.dist_cm     = dist_cm;
    g_findme.rssi_dbm    = rssi_dbm;
    rust_reduce_rational_pq((int32_t)dist_cm, 100, &g_findme.dist_m_p, &g_findme.dist_m_q);

    if (dist_cm < 100) {
        g_findme.proximity_zone = 0;
        g_findme.pulse_bpm      = 180;
    } else if (dist_cm < 500) {
        g_findme.proximity_zone = 1;
        g_findme.pulse_bpm      = 132;
    } else if (dist_cm < 2000) {
        g_findme.proximity_zone = 2;
        g_findme.pulse_bpm      = 96;
    } else {
        g_findme.proximity_zone = 3;
        g_findme.pulse_bpm      = 60;
    }
}

static void omni_findme_emit_json() {
    uint8_t oct = omni_bearing_octant(g_findme.bearing_deg);
    Serial.printf(
        "# FINDME|{\"node\":\"%s\",\"target\":\"%s\",\"target_jp\":\"%s\","
        "\"arrow\":\"%s\",\"arrow_ascii\":\"%s\",\"bearing_deg\":%d,"
        "\"dir_en\":\"%s\",\"dir_jp\":\"%s\","
        "\"dist_cm\":%lu,\"dist_m_pq\":\"%ld/%ld\","
        "\"rssi_dbm\":%d,\"uwb_ch\":%u,\"pulse_bpm\":%u,"
        "\"proximity\":\"%s\",\"proximity_jp\":\"%s\",\"lang\":\"%s\"}\n",
        g_medusa_node,
        g_findme.target_id,
        g_findme.target_jp,
        OMNI_ARROW_UTF8[oct],
        OMNI_ARROW_ASCII_JSON[oct],
        (int)g_findme.bearing_deg,
        OMNI_DIR_EN[oct],
        OMNI_DIR_JP[oct],
        (unsigned long)g_findme.dist_cm,
        (long)g_findme.dist_m_p,
        (long)g_findme.dist_m_q,
        (int)g_findme.rssi_dbm,
        (unsigned)g_findme.uwb_ch,
        (unsigned)g_findme.pulse_bpm,
        OMNI_PROX_EN[g_findme.proximity_zone],
        OMNI_PROX_JP[g_findme.proximity_zone],
        omni_lang_name(g_omni_lang)
    );
}

// ============================================================
// 2. Multi-Protocol RFID / NFC / FeliCa (Suica/PASMO) / MIFARE
// ============================================================
enum OmniRfidProtocol : uint8_t {
    RFID_PROTO_FELICA_SUICA    = 0, // JIS X 6319-4 (13.56 MHz NFC-F, Sony FeliCa / Suica / PASMO)
    RFID_PROTO_MIFARE_14443A   = 1, // ISO/IEC 14443-A (13.56 MHz NFC-A, MIFARE Classic / Ultralight)
    RFID_PROTO_ISO15693_NFCV   = 2, // ISO/IEC 15693 (13.56 MHz NFC-V Vicinity Card)
    RFID_PROTO_EM4100_LF125K   = 3, // 125 kHz LF RFID Manchester Tag
    RFID_PROTO_APPLE_FINDMY    = 4, // BLE 0x004C Continuity / FindMy Proximity Tag
    RFID_PROTO_LORA_SUBGHZ_JP  = 5  // 920.6 MHz ARIB STD-T108 LoRa SX1262 Tag
};

static const char* const OMNI_RFID_PROTO_NAMES[6] = {
    "NFC-F/FeliCa-Suica",
    "ISO14443A/MIFARE",
    "ISO15693/NFC-V",
    "LF-125kHz/EM4100",
    "BLE/Apple-FindMy",
    "SubGHz-920MHz/LoRa-JP"
};

struct OmniRfidTagRecord {
    uint8_t  proto;
    char     uid_hex[24];
    char     label[32];
    char     label_jp[64];
    uint16_t sys_code;
    uint32_t balance_yen;
    char     station_code[16];
    uint32_t fnv32_hash;
    char     wave_b3[10];
    int8_t   rssi_dbm;
};

#define OMNI_RFID_CAP 6
static OmniRfidTagRecord g_rfid_tags[OMNI_RFID_CAP] = {
    {
        RFID_PROTO_FELICA_SUICA,
        "0114010042198B15",
        "Suica Transit IC",
        "Suica・交通系ICカード (新宿駅)",
        0x0003,
        3420,
        "S-01:Shinjuku",
        0x9A4C2E11u,
        "101T01101",
        -32
    },
    {
        RFID_PROTO_MIFARE_14443A,
        "04A28B15C94E80",
        "Sovereign Studio Keycard",
        "8b-is・スタジオ入館証",
        0x0008,
        0,
        "GATE-8B",
        0x4D2188C3u,
        "11010T101",
        -38
    },
    {
        RFID_PROTO_EM4100_LF125K,
        "1A00432528",
        "Sakura School Badge",
        "桜学園・登下校タグ",
        0x0125,
        0,
        "ZONE-0",
        0x7F11B402u,
        "0110110T1",
        -41
    },
    {
        RFID_PROTO_APPLE_FINDMY,
        "4C001219108B15",
        "Chiro AirTag Backpack",
        "ちろちゃん・ランドセルタグ",
        0x004C,
        0,
        "S-LINE",
        0xE482019Au,
        "10T110101",
        -47
    }
};
static uint8_t  g_rfid_count = 4;
static uint8_t  g_rfid_active_idx = 0;
static uint8_t  g_rfid_suica_idx = 0;
static uint32_t g_rfid_scan_seq = 1;

static inline const OmniRfidTagRecord* omni_rfid_current_suica() {
    uint8_t idx = (g_rfid_suica_idx < g_rfid_count) ? g_rfid_suica_idx : 0;
    if (g_rfid_tags[idx].proto == RFID_PROTO_FELICA_SUICA) {
        return &g_rfid_tags[idx];
    }
    for (uint8_t i = 0; i < g_rfid_count; i++) {
        if (g_rfid_tags[i].proto == RFID_PROTO_FELICA_SUICA) {
            return &g_rfid_tags[i];
        }
    }
    return &g_rfid_tags[0];
}

static uint32_t omni_fnv32_str(const char* s) {
    uint32_t h = 0x811C9DC5u;
    while (s && *s) {
        h ^= (uint8_t)(*s++);
        h *= 0x01000193u;
    }
    return h;
}

// Self-contained 9-trit balanced-ternary hash so omni_cockpit_v22.h does not depend on phoenix_hive.h include order
static void omni_trit_hash9(const char* s, size_t n, char sig[10]) {
    int8_t t[9] = {0};
    for (size_t i = 0; i < n; i++) {
        int b = (uint8_t)s[i];
        int idx = (int)((i + (size_t)b) % 9u);
        t[idx] = (int8_t)((((t[idx] + (b % 3) - 1 + 4) % 3)) - 1);
    }
    for (int i = 0; i < 9; i++) {
        sig[i] = t[i] < 0 ? 'T' : (t[i] > 0 ? '1' : '0');
    }
    sig[9] = '\0';
}

static void omni_rfid_ingest(uint8_t proto, const char* uid_hex, const char* label, const char* label_jp,
                             uint16_t sys_code, uint32_t balance_yen, const char* station_code) {
    uint8_t norm_proto = (uint8_t)(proto % 6);
    uint8_t idx = 0;
    if (g_rfid_count < OMNI_RFID_CAP) {
        idx = g_rfid_count++;
    } else {
        idx = (uint8_t)(g_rfid_scan_seq % OMNI_RFID_CAP);
        // Never overwrite the active FeliCa/Suica transit card slot with a non-Suica tag
        if (norm_proto != RFID_PROTO_FELICA_SUICA && idx == g_rfid_suica_idx) {
            idx = (uint8_t)((idx + 1) % OMNI_RFID_CAP);
        }
    }
    OmniRfidTagRecord* r = &g_rfid_tags[idx];
    r->proto = norm_proto;
    strncpy(r->uid_hex, uid_hex ? uid_hex : "0114000000000000", sizeof(r->uid_hex) - 1);
    r->uid_hex[sizeof(r->uid_hex) - 1] = '\0';
    strncpy(r->label, label ? label : "RFID Tag", sizeof(r->label) - 1);
    r->label[sizeof(r->label) - 1] = '\0';
    strncpy(r->label_jp, label_jp ? label_jp : "ICタグ", sizeof(r->label_jp) - 1);
    r->label_jp[sizeof(r->label_jp) - 1] = '\0';
    r->sys_code = sys_code;
    r->balance_yen = balance_yen;
    strncpy(r->station_code, station_code ? station_code : "S-01:Shinjuku", sizeof(r->station_code) - 1);
    r->station_code[sizeof(r->station_code) - 1] = '\0';
    r->fnv32_hash = omni_fnv32_str(r->uid_hex);
    omni_trit_hash9(r->uid_hex, strlen(r->uid_hex), r->wave_b3);
    r->rssi_dbm = -30;
    g_rfid_active_idx = idx;
    if (norm_proto == RFID_PROTO_FELICA_SUICA) {
        g_rfid_suica_idx = idx;
    }
    g_rfid_scan_seq++;
}

static void omni_rfid_emit_json() {
    const OmniRfidTagRecord* r = &g_rfid_tags[g_rfid_active_idx % g_rfid_count];
    Serial.printf(
        "# RFID|{\"node\":\"%s\",\"seq\":%lu,\"tag_count\":%u,\"active_idx\":%u,"
        "\"protocol\":\"%s\",\"uid_hex\":\"%s\",\"label\":\"%s\",\"label_jp\":\"%s\","
        "\"sys_code\":\"0x%04x\",\"balance_yen\":%lu,\"station_code\":\"%s\","
        "\"fnv32\":\"0x%08lx\",\"wave_b3\":\"%s\",\"rssi_dbm\":%d,"
        "\"supported_protocols\":[\"NFC-F/FeliCa-Suica\",\"ISO14443A/MIFARE\",\"ISO15693/NFC-V\",\"LF-125kHz/EM4100\",\"BLE/Apple-FindMy\",\"SubGHz-920MHz/LoRa-JP\"]}\n",
        g_medusa_node,
        (unsigned long)g_rfid_scan_seq,
        (unsigned)g_rfid_count,
        (unsigned)g_rfid_active_idx,
        OMNI_RFID_PROTO_NAMES[r->proto % 6],
        r->uid_hex,
        r->label,
        r->label_jp,
        (unsigned)r->sys_code,
        (unsigned long)r->balance_yen,
        r->station_code,
        (unsigned long)r->fnv32_hash,
        r->wave_b3,
        (int)r->rssi_dbm
    );
}

// ============================================================
// 3. Animal Crossing x Olegsiv Unity Isometric Island Game
// ============================================================
struct OmniAcOlegsivGame {
    char     island_name[24];
    char     island_jp[48];
    char     villager_lead[24];
    char     villager_jp[48];
    uint32_t bells;
    uint32_t nook_miles;
    uint16_t sakura_petals;
    uint16_t fish_caught;
    uint16_t trees_planted;
    uint8_t  island_stars;      // 1..5 star island rating
    int32_t  turnip_p;          // Turnip Stalk Market ratio p/q
    int32_t  turnip_q;
    uint8_t  player_tile_x;     // 0..7 isometric grid X
    uint8_t  player_tile_y;     // 0..7 isometric grid Y
    char     last_event[96];
    char     last_event_jp[96];
};

static OmniAcOlegsivGame g_ac_game = {
    "Sakura-Jima",
    "桜島・どうぶつの森",
    "Chiro & Tanuki-Oleg",
    "ちろちゃん＆たぬきオレグ",
    4320,
    1850,
    27,
    12,
    18,
    5,
    135,
    100,
    3,
    4,
    "Chiro-chan planted a 432Hz Sakura tree by the RIVA river!",
    "ちろちゃんがRIVA川のほとりに432Hzの桜を植えました！"
};

static void omni_ac_recompute_turnips() {
    int32_t raw_num = (int32_t)(108 + (g_ac_game.sakura_petals * 3) + (g_ac_game.fish_caught * 5));
    rust_reduce_rational_pq(raw_num, 100, &g_ac_game.turnip_p, &g_ac_game.turnip_q);
}

static void omni_ac_action(const char* act) {
    if (!act || strcasecmp(act, "STEP") == 0 || strcasecmp(act, "WALK") == 0) {
        g_ac_game.player_tile_x = (uint8_t)((g_ac_game.player_tile_x + 1) & 7);
        g_ac_game.player_tile_y = (uint8_t)((g_ac_game.player_tile_y + 3) & 7);
        g_ac_game.bells += 50;
        strncpy(g_ac_game.last_event, "Strolled across the RIVA wooden bridge with Chiro-chan", sizeof(g_ac_game.last_event) - 1);
        strncpy(g_ac_game.last_event_jp, "ちろちゃんとRIVA木の橋をお散歩しました", sizeof(g_ac_game.last_event_jp) - 1);
    } else if (strcasecmp(act, "HARVEST") == 0 || strcasecmp(act, "SAKURA") == 0) {
        g_ac_game.sakura_petals += 3;
        g_ac_game.bells += 300;
        g_ac_game.nook_miles += 50;
        strncpy(g_ac_game.last_event, "Harvested 3 glowing Sakura blossoms (+300 Bells)", sizeof(g_ac_game.last_event) - 1);
        strncpy(g_ac_game.last_event_jp, "光る桜の花びらを3枚収穫！(+300ベル)", sizeof(g_ac_game.last_event_jp) - 1);
    } else if (strcasecmp(act, "FISH") == 0) {
        g_ac_game.fish_caught += 1;
        g_ac_game.bells += 528;
        g_ac_game.nook_miles += 100;
        strncpy(g_ac_game.last_event, "Caught a 528Hz Bioluminescent Tai Sea Bream (+528 Bells)", sizeof(g_ac_game.last_event) - 1);
        strncpy(g_ac_game.last_event_jp, "528Hzの発光めで鯛を釣り上げた！(+528ベル)", sizeof(g_ac_game.last_event_jp) - 1);
    } else if (strcasecmp(act, "PLANT") == 0) {
        g_ac_game.trees_planted += 1;
        g_ac_game.nook_miles += 150;
        if (g_ac_game.island_stars < 5) g_ac_game.island_stars++;
        strncpy(g_ac_game.last_event, "Planted a Golden Cedar in Olegsiv Unity Isometric Valley", sizeof(g_ac_game.last_event) - 1);
        strncpy(g_ac_game.last_event_jp, "オレグシブの谷に黄金杉を植樹しました", sizeof(g_ac_game.last_event_jp) - 1);
    } else if (strcasecmp(act, "TRADE") == 0 || strcasecmp(act, "GIFT") == 0) {
        g_ac_game.bells += 852;
        g_ac_game.nook_miles += 200;
        g_guardian_soul.care_touches++;
        guardian_update_companion_soul();
        strncpy(g_ac_game.last_event, "Shared Turnip & Matcha gift with sibling island (+852 Bells)", sizeof(g_ac_game.last_event) - 1);
        strncpy(g_ac_game.last_event_jp, "姉妹島へカブと抹茶のお裾分け！(+852ベル)", sizeof(g_ac_game.last_event_jp) - 1);
    }
    omni_ac_recompute_turnips();
}

static void omni_ac_emit_json() {
    omni_ac_recompute_turnips();
    Serial.printf(
        "# GAME|{\"engine\":\"olegsiv-unity-animal-crossing\",\"node\":\"%s\","
        "\"island\":\"%s\",\"island_jp\":\"%s\","
        "\"villagers\":\"%s\",\"villagers_jp\":\"%s\","
        "\"bells\":%lu,\"nook_miles\":%lu,\"sakura_petals\":%u,\"fish_caught\":%u,\"trees_planted\":%u,"
        "\"island_stars\":%u,\"turnip_ratio_pq\":\"%ld/%ld\","
        "\"tile_xy\":[%u,%u],\"event\":\"%s\",\"event_jp\":\"%s\"}\n",
        g_medusa_node,
        g_ac_game.island_name,
        g_ac_game.island_jp,
        g_ac_game.villager_lead,
        g_ac_game.villager_jp,
        (unsigned long)g_ac_game.bells,
        (unsigned long)g_ac_game.nook_miles,
        (unsigned)g_ac_game.sakura_petals,
        (unsigned)g_ac_game.fish_caught,
        (unsigned)g_ac_game.trees_planted,
        (unsigned)g_ac_game.island_stars,
        (long)g_ac_game.turnip_p,
        (long)g_ac_game.turnip_q,
        (unsigned)g_ac_game.player_tile_x,
        (unsigned)g_ac_game.player_tile_y,
        g_ac_game.last_event,
        g_ac_game.last_event_jp
    );
}

// ============================================================
// 6. Pomodoro (25m Focus / 5m Break) + LLL (Lifelong Learning)
// ============================================================
enum OmniPomoPhase : uint8_t {
    POMO_PHASE_FOCUS_25M      = 0, // 1500s (25m) Focus
    POMO_PHASE_BREAK_5M       = 1, // 300s  (5m)  Short Break
    POMO_PHASE_LONG_BREAK_15M = 2  // 900s  (15m) Long Break (every 4 pomodoros)
};

static const char* const OMNI_POMO_EN[3] = {
    "FOCUS 25:00",
    "BREAK 05:00",
    "LONG BREAK 15:00"
};

static const char* const OMNI_POMO_JP[3] = {
    "集中・ポモドーロ (25分)",
    "休憩・深呼吸 (5分)",
    "大休憩・リフレッシュ (15分)"
};

struct OmniLllCard {
    const char* topic;
    const char* front_jp;
    const char* back_en;
    const char* formula;
};

static const OmniLllCard OMNI_LLL_DECK[6] = {
    {"JP-TRANSIT", "新宿駅 ・ 乗り換え (Shinjuku Norikae)", "Shinjuku Station Transfer (S-Line / Marunouchi)", "odpt:Station:Toei.Shinjuku"},
    {"TERNARY-MATH", "均衡三進法 {-1, 0, +1}", "Balanced Ternary Gates: AND=min(a,b), OR=max(a,b), NAND=-min(a,b)", "Net = (+1) + 0 + 0 + (+1) = +2"},
    {"ACOUSTICS-PQ", "純正律 432Hz : 528Hz", "Exact Reduced Rational Interval: 528 / 432 = 11 / 9", "gcd(528, 432) = 48 => 11/9"},
    {"FELICA-NFC", "Suica / FeliCa JIS X 6319-4", "13560 kHz Contactless IC (System 0x0003, Service 0x090F)", "IDm 8B + PMm 8B -> <100ms Tap"},
    {"MARINESENSE", "海洋音響 O(1) クラゲ検出", "Salience S = w_e*E + w_j/(1+J) + w_h*H across 7 Wave Nodes", "c_sound = 15034/10 m/s"},
    {"GUARDIAN-64B", "ちろちゃん・64B 暗号パンくず", "Chained FNV-1a 64-Byte Safety Breadcrumb + Shadow MAC Detector", "prev_chain_fnv64 -> seal_fnv64"}
};

struct OmniPomodoroLllState {
    uint8_t  phase;             // 0=FOCUS(25m), 1=BREAK(5m), 2=LONG_BREAK(15m)
    bool     running;
    uint32_t remaining_s;       // seconds remaining in current phase
    uint32_t total_phase_s;     // 1500, 300, or 900
    uint16_t completed_pomos;
    uint32_t last_tick_ms;
    // LLL (Lifelong Learning & Continual On-Device Learning)
    uint8_t  card_idx;
    uint32_t cards_reviewed;
    uint16_t lll_streak;
    int32_t  mastery_p;
    int32_t  mastery_q;
};

static OmniPomodoroLllState g_pomo_lll = {
    POMO_PHASE_FOCUS_25M,
    true,
    1500,
    1500,
    3,
    0,
    0,
    18,
    7,
    23,
    25
};

static void omni_pomo_recompute_mastery() {
    int32_t num = (int32_t)(70 + g_pomo_lll.cards_reviewed * 2 + g_pomo_lll.lll_streak * 3);
    int32_t den = (int32_t)(100 + g_pomo_lll.cards_reviewed * 2);
    rust_reduce_rational_pq(num, den, &g_pomo_lll.mastery_p, &g_pomo_lll.mastery_q);
}

static void omni_pomo_switch_phase(uint8_t next_phase) {
    g_pomo_lll.phase = (uint8_t)(next_phase % 3);
    if (g_pomo_lll.phase == POMO_PHASE_FOCUS_25M) {
        g_pomo_lll.total_phase_s = 1500;
    } else if (g_pomo_lll.phase == POMO_PHASE_BREAK_5M) {
        g_pomo_lll.total_phase_s = 300;
    } else {
        g_pomo_lll.total_phase_s = 900;
    }
    g_pomo_lll.remaining_s = g_pomo_lll.total_phase_s;
}

static void omni_pomo_tick(unsigned long now_ms) {
    if (!g_pomo_lll.running) {
        g_pomo_lll.last_tick_ms = now_ms;
        return;
    }
    if (g_pomo_lll.last_tick_ms == 0) {
        g_pomo_lll.last_tick_ms = now_ms;
        return;
    }
    unsigned long elapsed_ms = now_ms - g_pomo_lll.last_tick_ms;
    if (elapsed_ms >= 1000UL) {
        uint32_t secs = (uint32_t)(elapsed_ms / 1000UL);
        g_pomo_lll.last_tick_ms += secs * 1000UL;
        while (secs > 0 && g_pomo_lll.running) {
            if (secs < g_pomo_lll.remaining_s) {
                g_pomo_lll.remaining_s -= secs;
                secs = 0;
            } else {
                secs -= g_pomo_lll.remaining_s;
                if (g_pomo_lll.phase == POMO_PHASE_FOCUS_25M) {
                    g_pomo_lll.completed_pomos++;
                    g_pomo_lll.cards_reviewed++;
                    g_pomo_lll.card_idx = (uint8_t)((g_pomo_lll.card_idx + 1) % 6);
                    omni_pomo_recompute_mastery();
                    if ((g_pomo_lll.completed_pomos % 4) == 0) {
                        omni_pomo_switch_phase(POMO_PHASE_LONG_BREAK_15M);
                    } else {
                        omni_pomo_switch_phase(POMO_PHASE_BREAK_5M);
                    }
                } else {
                    omni_pomo_switch_phase(POMO_PHASE_FOCUS_25M);
                }
            }
        }
    }
}

static void omni_pomo_lll_emit_json() {
    omni_pomo_recompute_mastery();
    const OmniLllCard* card = &OMNI_LLL_DECK[g_pomo_lll.card_idx % 6];
    int32_t prog_p = 0, prog_q = 1;
    uint32_t done_s = g_pomo_lll.total_phase_s - g_pomo_lll.remaining_s;
    rust_reduce_rational_pq((int32_t)done_s, (int32_t)g_pomo_lll.total_phase_s, &prog_p, &prog_q);

    Serial.printf(
        "# POMODORO|{\"node\":\"%s\",\"phase\":\"%s\",\"phase_jp\":\"%s\","
        "\"running\":%d,\"remaining_s\":%lu,\"remaining_mmss\":\"%02lu:%02lu\",\"total_s\":%lu,"
        "\"progress_pq\":\"%ld/%ld\",\"completed_pomos\":%u,"
        "\"lll\":{\"card_idx\":%u,\"topic\":\"%s\",\"front_jp\":\"%s\",\"back_en\":\"%s\",\"formula\":\"%s\","
        "\"cards_reviewed\":%lu,\"streak\":%u,\"mastery_pq\":\"%ld/%ld\"}}\n",
        g_medusa_node,
        OMNI_POMO_EN[g_pomo_lll.phase % 3],
        OMNI_POMO_JP[g_pomo_lll.phase % 3],
        g_pomo_lll.running ? 1 : 0,
        (unsigned long)g_pomo_lll.remaining_s,
        (unsigned long)(g_pomo_lll.remaining_s / 60),
        (unsigned long)(g_pomo_lll.remaining_s % 60),
        (unsigned long)g_pomo_lll.total_phase_s,
        (long)prog_p,
        (long)prog_q,
        (unsigned)g_pomo_lll.completed_pomos,
        (unsigned)g_pomo_lll.card_idx,
        card->topic,
        card->front_jp,
        card->back_en,
        card->formula,
        (unsigned long)g_pomo_lll.cards_reviewed,
        (unsigned)g_pomo_lll.lll_streak,
        (long)g_pomo_lll.mastery_p,
        (long)g_pomo_lll.mastery_q
    );
}

// ============================================================
// 7. Google Maps + Tokyo Metro (odpt:Railway) Route + Parents
// ============================================================
struct OmniMetroStation {
    const char* code;
    const char* name_en;
    const char* name_jp;
    const char* line_code;
    const char* line_name;
    uint8_t     station_num;
    int32_t     lat_e6;  // integer micro-degrees (deg * 1000000)
    int32_t     lon_e6;
};

static const OmniMetroStation OMNI_TOKYO_STATIONS[8] = {
    {"S-01", "Shinjuku",   "新宿",   "S", "Toei Shinjuku S-Line",     1,  35689500, 139700400},
    {"S-04", "Ichigaya",   "市ヶ谷", "S", "Toei Shinjuku S-Line",     4,  35691200, 139735600},
    {"S-06", "Jimbocho",   "神保町", "S", "Toei Shinjuku / Hanzomon", 6,  35695900, 139757600},
    {"M-17", "Tokyo",      "東京",   "M", "Tokyo Metro Marunouchi",   17, 35681200, 139767100},
    {"G-01", "Shibuya",    "渋谷",   "G", "Tokyo Metro Ginza Line",   1,  35658000, 139701600},
    {"G-09", "Ginza",      "銀座",   "G", "Tokyo Metro Ginza Line",   9,  35671200, 139765000},
    {"H-04", "Roppongi",   "六本木", "H", "Tokyo Metro Hibiya Line",  4,  35662800, 139731400},
    {"H-15", "Akihabara",  "秋葉原", "H", "Tokyo Metro Hibiya Line",  15, 35698400, 139773100}
};

struct OmniMetroRouteState {
    uint8_t  from_idx;
    uint8_t  to_idx;
    uint8_t  transfer_idx;
    uint16_t eta_min;
    uint16_t ic_fare_yen;
    uint8_t  stops_count;
    char     platform_info[64];
    char     parents_note[96];
    uint32_t parents_notify_seq;
};

static OmniMetroRouteState g_metro = {
    0, // S-01 Shinjuku
    3, // M-17 Tokyo
    2, // S-06 Jimbocho / Otemachi transfer
    14,
    210,
    5,
    "Car 4 · Door 2 (Elev/Stroller Friendly)",
    "On the Tokyo S-Line heading to Tokyo Station — SAFE with Chiro-chan!",
    1
};

static int omni_find_station_idx(const char* query) {
    if (!query || !query[0]) return -1;
    for (int i = 0; i < 8; i++) {
        if (strcasecmp(query, OMNI_TOKYO_STATIONS[i].code) == 0 ||
            strcasecmp(query, OMNI_TOKYO_STATIONS[i].name_en) == 0 ||
            strncasecmp(query, OMNI_TOKYO_STATIONS[i].name_en, strlen(query)) == 0) {
            return i;
        }
    }
    return -1;
}

static void omni_metro_refresh_parents_note() {
    const char* alert_str = GUARDIAN_ALERT_NAMES[g_guardian_alert & 3];
    snprintf(g_metro.parents_note, sizeof(g_metro.parents_note),
             "En route %s -> %s (%u min ETA) · Suica IC %u JPY · %s",
             OMNI_TOKYO_STATIONS[g_metro.from_idx & 7].name_en,
             OMNI_TOKYO_STATIONS[g_metro.to_idx & 7].name_en,
             (unsigned)g_metro.eta_min,
             (unsigned)g_metro.ic_fare_yen,
             alert_str);
}

static void omni_metro_plan_route(const char* from_q, const char* to_q) {
    int fi = omni_find_station_idx(from_q);
    int ti = omni_find_station_idx(to_q);
    if (fi >= 0) g_metro.from_idx = (uint8_t)fi;
    if (ti >= 0) g_metro.to_idx   = (uint8_t)ti;

    const OmniMetroStation* st_from = &OMNI_TOKYO_STATIONS[g_metro.from_idx & 7];
    const OmniMetroStation* st_to   = &OMNI_TOKYO_STATIONS[g_metro.to_idx & 7];

    if (g_metro.from_idx == g_metro.to_idx) {
        g_metro.stops_count  = 0;
        g_metro.eta_min      = 0;
        g_metro.ic_fare_yen  = 0;
        g_metro.transfer_idx = g_metro.from_idx;
    } else if (strcmp(st_from->line_code, st_to->line_code) == 0) {
        // Direct same-line journey (no transfer required)
        int sdiff = (int)st_to->station_num - (int)st_from->station_num;
        if (sdiff < 0) sdiff = -sdiff;
        if (sdiff == 0) sdiff = 1;
        g_metro.stops_count  = (uint8_t)sdiff;
        g_metro.eta_min      = (uint16_t)(sdiff * 2);
        g_metro.ic_fare_yen  = (uint16_t)((sdiff <= 3) ? 178 : 220);
        g_metro.transfer_idx = g_metro.to_idx;
    } else {
        // Cross-line interchange journey via S-06 Jimbocho / Otemachi
        int diff = (int)g_metro.to_idx - (int)g_metro.from_idx;
        if (diff < 0) diff = -diff;
        g_metro.stops_count  = (uint8_t)(diff + 2);
        g_metro.eta_min      = (uint16_t)(diff * 3 + 5);
        g_metro.ic_fare_yen  = (uint16_t)(178 + diff * 16);
        g_metro.transfer_idx = 2; // S-06 Jimbocho hub
    }
    omni_metro_refresh_parents_note();
}

static void omni_metro_emit_json() {
    const OmniMetroStation* st_from = &OMNI_TOKYO_STATIONS[g_metro.from_idx & 7];
    const OmniMetroStation* st_to   = &OMNI_TOKYO_STATIONS[g_metro.to_idx & 7];
    const OmniMetroStation* st_xfer = &OMNI_TOKYO_STATIONS[g_metro.transfer_idx & 7];
    const OmniRfidTagRecord* suica  = omni_rfid_current_suica();
    uint8_t oct = omni_bearing_octant(g_findme.bearing_deg);
    bool is_same   = (g_metro.from_idx == g_metro.to_idx);
    bool is_direct = (!is_same && strcmp(st_from->line_code, st_to->line_code) == 0);
    const char* xfer_code = is_same ? "NONE" : (is_direct ? "DIRECT" : st_xfer->code);
    const char* xfer_en   = is_same ? "Already at Destination" : (is_direct ? "Direct (No Transfer)" : st_xfer->name_en);
    const char* xfer_jp   = is_same ? "到着済み" : (is_direct ? "直通・乗換なし" : st_xfer->name_jp);

    Serial.printf(
        "# METRO|{\"node\":\"%s\",\"api\":\"odpt:Railway+GoogleMaps-E6\","
        "\"from\":{\"code\":\"%s\",\"name\":\"%s\",\"name_jp\":\"%s\",\"line\":\"%s\",\"lat_e6\":%ld,\"lon_e6\":%ld},"
        "\"to\":{\"code\":\"%s\",\"name\":\"%s\",\"name_jp\":\"%s\",\"line\":\"%s\",\"lat_e6\":%ld,\"lon_e6\":%ld},"
        "\"transfer\":{\"code\":\"%s\",\"name\":\"%s\",\"name_jp\":\"%s\"},"
        "\"eta_min\":%u,\"stops\":%u,\"ic_fare_yen\":%u,\"suica_balance_yen\":%lu,"
        "\"platform\":\"%s\",\"findme_arrow\":\"%s\",\"findme_dist_m_pq\":\"%ld/%ld\","
        "\"gmaps_query\":\"%ld,%ld\"}\n",
        g_medusa_node,
        st_from->code, st_from->name_en, st_from->name_jp, st_from->line_name, (long)st_from->lat_e6, (long)st_from->lon_e6,
        st_to->code, st_to->name_en, st_to->name_jp, st_to->line_name, (long)st_to->lat_e6, (long)st_to->lon_e6,
        xfer_code, xfer_en, xfer_jp,
        (unsigned)g_metro.eta_min,
        (unsigned)g_metro.stops_count,
        (unsigned)g_metro.ic_fare_yen,
        (unsigned long)suica->balance_yen,
        g_metro.platform_info,
        OMNI_ARROW_UTF8[oct],
        (long)g_findme.dist_m_p,
        (long)g_findme.dist_m_q,
        (long)st_from->lat_e6,
        (long)st_from->lon_e6
    );
}

static void omni_parents_emit_json() {
    omni_metro_refresh_parents_note();
    const OmniMetroStation* st_from = &OMNI_TOKYO_STATIONS[g_metro.from_idx & 7];
    const OmniMetroStation* st_to   = &OMNI_TOKYO_STATIONS[g_metro.to_idx & 7];
    const OmniRfidTagRecord* suica  = omni_rfid_current_suica();
    uint8_t oct = omni_bearing_octant(g_findme.bearing_deg);

    Serial.printf(
        "# PARENTS|{\"node\":\"%s\",\"seq\":%lu,\"companion\":\"Chiro-chan\","
        "\"safety_alert\":\"%s\",\"bond_pq\":\"%ld/%ld\",\"battery_mv\":%u,"
        "\"route_en\":\"%s (%s) -> %s (%s)\",\"route_jp\":\"%s駅 → %s駅\","
        "\"eta_min\":%u,\"suica_yen\":%lu,\"last_ic_gate\":\"%s\","
        "\"findme_arrow\":\"%s\",\"findme_dist_m_pq\":\"%ld/%ld\","
        "\"lat_e6\":%ld,\"lon_e6\":%ld,\"seal_fnv64\":\"0x%016llx\","
        "\"note\":\"%s\"}\n",
        g_medusa_node,
        (unsigned long)g_metro.parents_notify_seq++,
        GUARDIAN_ALERT_NAMES[g_guardian_alert & 3],
        (long)g_guardian_soul.bond_p,
        (long)g_guardian_soul.bond_q,
        (unsigned)g_power.batt_mv,
        st_from->name_en, st_from->code, st_to->name_en, st_to->code,
        st_from->name_jp, st_to->name_jp,
        (unsigned)g_metro.eta_min,
        (unsigned long)suica->balance_yen,
        suica->station_code,
        OMNI_ARROW_UTF8[oct],
        (long)g_findme.dist_m_p,
        (long)g_findme.dist_m_q,
        (long)st_from->lat_e6,
        (long)st_from->lon_e6,
        (unsigned long long)g_guardian_chain_fnv64,
        g_metro.parents_note
    );
}

static void omni_screen_emit_json() {
    int32_t asp_p = 0, asp_q = 1, scl_p = 0, scl_q = 1;
    rust_reduce_rational_pq((int32_t)g_lcd_w, (int32_t)g_lcd_h, &asp_p, &asp_q);
    rust_reduce_rational_pq((int32_t)g_lcd_w, 368, &scl_p, &scl_q);

    int W = (int)g_lcd_w;
    int H = (int)g_lcd_h;
    int hdr_h = (H < 300) ? 32 : 40;
    int ftr_h = (H < 300) ? 36 : 52;
    int hud_h = (H < 300) ? 32 : 36;
    int hud_y = H - ftr_h - hud_h - 4;
    int avail_top = hdr_h + 14;
    int avail_bot = hud_y - 4;
    int max_r_w = (W - 28) / 2;
    int max_r_h = (avail_bot - avail_top) / 2 - 6;
    int outer_r = (max_r_w < max_r_h) ? max_r_w : max_r_h;
    if (outer_r > 154) outer_r = 154;
    if (outer_r < 44) outer_r = 44;

    Serial.printf(
        "# RES|{\"node\":\"%s\",\"board\":\"%s\",\"driver\":\"%s\",\"det_src\":\"%s\","
        "\"width\":%u,\"height\":%u,\"x_gap\":%u,\"y_gap\":%u,"
        "\"aspect_pq\":\"%ld/%ld\",\"scale_pq\":\"%ld/%ld\","
        "\"hdr_h\":%d,\"ftr_h\":%d,\"hud_y\":%d,\"zen_outer_r\":%d,\"fit_ok\":1}\n",
        g_medusa_node,
        g_board_model_name,
        g_lcd_driver_name,
        g_lcd_det_src,
        (unsigned)g_lcd_w,
        (unsigned)g_lcd_h,
        (unsigned)g_lcd_x_gap,
        (unsigned)g_lcd_y_gap,
        (long)asp_p,
        (long)asp_q,
        (long)scl_p,
        (long)scl_q,
        hdr_h,
        ftr_h,
        hud_y,
        outer_r
    );
}

static void omni_screen_set_res(uint16_t w, uint16_t h, uint16_t x_gap, const char* src) {
    if (w < 120 || w > 410 || h < 120 || h > 502) return;
#ifndef FLUGOS_HOST_SIM
    if (w > g_hw_lcd_w || h > g_hw_lcd_h) {
        Serial.printf("# RES_ERR|{\"reason\":\"exceeds_physical_panel\",\"req_w\":%u,\"req_h\":%u,\"hw_w\":%u,\"hw_h\":%u}\n",
                      (unsigned)w, (unsigned)h, (unsigned)g_hw_lcd_w, (unsigned)g_hw_lcd_h);
        return;
    }
    (void)x_gap;
#else
    g_lcd_x_gap = x_gap;
    g_lcd_y_gap = 0;
    if (w == 320 && h == 240) {
        g_board_is_18      = false;
        g_lcd_driver_name  = "ST7789";
        g_board_model_name = "LilyGO-T-Deck-Plus-320x240";
    } else if (w >= 400) {
        g_board_is_18      = false;
        g_lcd_driver_name  = "CO5300";
        g_board_model_name = "ESP32-S3-Touch-AMOLED-410x502";
    } else {
        g_board_is_18      = true;
        g_lcd_driver_name  = "SH8601";
        g_board_model_name = "ESP32-S3-Touch-AMOLED-368x448";
    }
#endif
    g_lcd_w       = w;
    g_lcd_h       = h;
    g_lcd_det_src = src ? src : "runtime_serial";
}

// ============================================================
// 8. Audio8-TTS/STT (600M DualAR) x DeepSeek Engram BFP-64 x
//    ALEXIAI GAIA Fold x AURORA Voice-Shell Engine (Zero-Float p/q)
// ============================================================
#define OMNI_AUDIO8_DIM               896u    // 14 * 64
#define OMNI_AUDIO8_HEAD_DIM          64u     // 1 * 64
#define OMNI_AUDIO8_KV_DIM            128u    // 2 * 64
#define OMNI_AUDIO8_INTER             4864u   // 76 * 64
#define OMNI_AUDIO8_CODEC_INTER       1216u   // 19 * 64
#define OMNI_AUDIO8_CODEBOOK_SIZE     4096u   // 64 * 64
#define OMNI_AUDIO8_NUM_CODEBOOKS     10u
#define OMNI_AUDIO8_SEMANTIC_BEGIN    151678u
#define OMNI_AUDIO8_SEMANTIC_END      155773u // 4,096 semantic tokens = 64 * 64
#define OMNI_AUDIO8_SAMPLE_RATE_HZ    44100u
#define OMNI_AUDIO8_I2S_RATE_HZ       16000u
#define OMNI_AUDIO8_FRAME_SAMPLES     2048u   // 32 * 64 (512/11025 s = 46,439 us)
#define OMNI_AUDIO8_FRAME_BUDGET_US   46439u
#define OMNI_AUDIO8_TILE_EXEC_US      18u     // 18 us per 64x64 bit-sliced tile on ESP32-S3
#define OMNI_ENGRAM_PRIME_0           1000003u

#ifndef FLUGOS_WAVE_CORE_VOICE_MESH_FFI
#define FLUGOS_WAVE_CORE_VOICE_MESH_FFI
typedef struct {
    int32_t predictor;
    int32_t index;
} FlugAdpcmState;

typedef struct {
    int32_t y1;
    int32_t y2;
} FlugQ15Biquad;

extern "C" {
    void rust_flugos_adpcm_encode_frame(const int16_t* pcm_in, uint8_t* adpcm_out, FlugAdpcmState* st);
    void rust_flugos_adpcm_decode_frame(const uint8_t* adpcm_in, int16_t* pcm_out, FlugAdpcmState* st);
    uint32_t rust_flugos_simd_rms_and_zcr(const int16_t* pcm_in);
    int32_t rust_flugos_pie_int8_dot_16x(const int8_t* weights_16b_aligned, const int8_t* activations_16b_aligned, size_t blocks_of_16);
    void rust_flugos_render_q15_phoneme_chunk(uint8_t ch, FlugQ15Biquad* r1, FlugQ15Biquad* r2, uint32_t* phase_q16, uint32_t* lfsr, int16_t* out_dma);
}
#endif

struct OmniVoiceEngineState {
    uint8_t  tmv_mode;            // 0x80=RAW, 0x81=SHIELDED, 0x82=AUDIO8_TTS, 0x83=AUDIO8_STT
    uint8_t  active_page;         // Ping-Pong matrix bank (0 or 1)
    uint16_t seq;
    uint32_t ebpf_verdict;        // 0x7C24xxxx (SEAL_OK) or 0xDEADxxxx (FISSION_TRAP)
    uint16_t continuity_seal;     // 0x7C24 or 0xDEAD
    uint16_t l1_spectral_norm;
    uint8_t  boundary_breaches;   // Breaches across Megaira dipole gates {9, 31, 41, 63}
    uint32_t semantic_token_id;   // 151678 .. 155773
    uint16_t codebook_idx;        // 0 .. 4095
    uint32_t engram_bucket;       // Lemire p/q prime bucket
    int8_t   engram_bfp64_exp;    // Shared dyadic exponent for 64-dim BFP-64 head
    int32_t  engram_proj_q16;     // Zero-quantization i16->i64->Q16 projection
    int32_t  aurora_fade_p;       // Smoothstep 3t^2 - 2t^3 rational numerator
    int32_t  aurora_fade_q;       // Smoothstep rational denominator
    int32_t  gaia_fold_p;         // ALEXIAI GAIA resonance fold numerator (1618/1000 -> 809/500)
    int32_t  gaia_fold_q;         // ALEXIAI GAIA resonance fold denominator
    bool     transcriber_muted;   // Muted while ES8311 PA is speaking (prevents echo loop)
    bool     bot_speaking;
    bool     interrupted;
    uint16_t total_chunks;
    uint16_t spoken_chunks;
    char     last_stt_text[64];
    char     full_bot_msg[96];
    char     last_bot_msg[96];    // Truncated via get_message_up_to() on interrupt
    char     wave_b3[10];
};

static OmniVoiceEngineState g_voice_ai = {
    0x82u,
    0u,
    1u,
    0x7C240E10u,
    0x7C24u,
    3600u,
    0u,
    151678u,
    0u,
    0u,
    -3,
    65536,
    1,
    1,
    809,
    500,
    false,
    false,
    false,
    8u,
    8u,
    "KONNICHIWA SHINJUKU OMNI",
    "KONNICHIWA CHIRO GUARDIAN SPEAKING ON ESP32 S3",
    "KONNICHIWA CHIRO GUARDIAN SPEAKING ON ESP32 S3",
    "101T01101"
};

// Lemire p/q (q = 2^64) reciprocal prime bucket lookup (100% integer, matches 8b-is/Engram)
static inline uint32_t omni_engram_lemire_bucket(uint32_t ngram_hash, uint32_t prime) {
    if (prime == 0u) return 0u;
    uint64_t recip = (0xFFFFFFFFFFFFFFFFULL / (uint64_t)prime) + 1ULL;
    uint64_t prod_lo = (uint64_t)ngram_hash * recip;
    uint32_t bucket = (uint32_t)(((unsigned __int128)prod_lo * (unsigned __int128)prime) >> 64);
    return bucket;
}

// Aurora-Voice-Shell exact integer smoothstep polynomial: 3t^2 - 2t^3 over SCALE = 1000
static inline uint32_t omni_aurora_smoothstep_permille(uint32_t t_permille) {
    uint64_t t = (t_permille > 1000u) ? 1000ULL : (uint64_t)t_permille;
    uint64_t s = 1000ULL;
    return (uint32_t)((t * t * (3ULL * s - 2ULL * t)) / (s * s));
}

// Computes partial message spoken up to (spoken_chunks / total_chunks) for barge-in interrupt
static inline void omni_voice_get_message_up_to(
    const char* full_msg,
    uint16_t spoken_chunks,
    uint16_t total_chunks,
    char* out_buf,
    size_t out_cap
) {
    if (!full_msg || !out_buf || out_cap == 0) return;
    size_t len = strlen(full_msg);
    if (total_chunks == 0 || spoken_chunks >= total_chunks) {
        strncpy(out_buf, full_msg, out_cap - 1);
        out_buf[out_cap - 1] = '\0';
        return;
    }
    size_t cut = (len * (size_t)spoken_chunks) / (size_t)total_chunks;
    while (cut < len && full_msg[cut] != ' ') {
        cut++;
    }
    if (cut >= out_cap) cut = out_cap - 1;
    memcpy(out_buf, full_msg, cut);
    out_buf[cut] = '\0';
}

// Runs 64x64 Balanced-Ternary Contraction (0xF3 ONESHOT_TMV) + Engram BFP-64 + eBPF R0 Verdict
static void omni_audio8_exec_tmv(uint8_t mode, const char* text_in) {
    const uint64_t shaw_neg  = 0x000001FF000001FFULL; // 18 Human (-1) trits
    const uint64_t shaw_pos  = 0x7FFFFC007FFFFC00ULL; // 42 Xeno (+1) trits
    const uint64_t shaw_zero = 0x8000020080000200ULL; // 4 Megaira Dipole Gates @ {9, 31, 41, 63}

    g_voice_ai.tmv_mode = mode;
    g_voice_ai.seq++;

    if (text_in && text_in[0]) {
        if (mode == 0x83u) {
            strncpy(g_voice_ai.last_stt_text, text_in, sizeof(g_voice_ai.last_stt_text) - 1);
            g_voice_ai.last_stt_text[sizeof(g_voice_ai.last_stt_text) - 1] = '\0';
        } else {
            strncpy(g_voice_ai.full_bot_msg, text_in, sizeof(g_voice_ai.full_bot_msg) - 1);
            g_voice_ai.full_bot_msg[sizeof(g_voice_ai.full_bot_msg) - 1] = '\0';
            strncpy(g_voice_ai.last_bot_msg, text_in, sizeof(g_voice_ai.last_bot_msg) - 1);
            g_voice_ai.last_bot_msg[sizeof(g_voice_ai.last_bot_msg) - 1] = '\0';
        }
    }

    const char* hash_src = (mode == 0x83u) ? g_voice_ai.last_stt_text : g_voice_ai.full_bot_msg;
    uint32_t fnv = omni_fnv32_str(hash_src);
    omni_trit_hash9(hash_src, strlen(hash_src), g_voice_ai.wave_b3);

    // 1. DeepSeek Engram BFP-64 Lookup (64 x i8 mantissas + 1 x i8 shared exponent)
    g_voice_ai.engram_bucket    = omni_engram_lemire_bucket(fnv, OMNI_ENGRAM_PRIME_0);
    g_voice_ai.engram_bfp64_exp = -2;
    int32_t bfp_acc_i16 = 0;
    for (unsigned int i = 0; i < 64u; i++) {
        int8_t mant = (int8_t)(((int32_t)((fnv >> (i & 15u)) & 0x3Fu)) - 31);
        int8_t trit = ((shaw_pos >> i) & 1ULL) ? 1 : (((shaw_neg >> i) & 1ULL) ? -1 : 0);
        bfp_acc_i16 += (int32_t)mant * (int32_t)trit;
    }
    g_voice_ai.engram_proj_q16 = bfp_acc_i16 << 6;

    // 2. 64x64 Bit-Sliced Ternary Contraction (0xF3 ONESHOT_TMV)
    //    Mode 0x80 leaves dipole gates unmasked (adversarial page 1 -> 4 breaches -> 0xDEAD0F00)
    //    Modes 0x81, 0x82, 0x83 enforce the Zero-Signed Boundary Gate Firewall -> 0x7C240E10
    uint16_t l1_sum = 0;
    uint8_t  breaches = 0;
    for (unsigned int r = 0; r < 64u; r++) {
        uint64_t r_bit = 1ULL << r;
        bool is_gate = (shaw_zero & r_bit) != 0ULL;
        uint64_t wn = 0ULL;
        uint64_t wp = 0ULL;
        if (is_gate) {
            if (mode == 0x80u) {
                wn = shaw_neg;
                wp = shaw_pos;
                breaches++;
            }
        } else if ((shaw_neg & r_bit) != 0ULL) {
            wn = shaw_pos;
            wp = shaw_neg;
        } else {
            wn = shaw_neg;
            wp = shaw_pos;
        }
        uint64_t prod_pos = (wp & shaw_pos) | (wn & shaw_neg);
        uint64_t prod_neg = (wp & shaw_neg) | (wn & shaw_pos);
        int32_t  y_r = (int32_t)__builtin_popcountll(prod_pos) - (int32_t)__builtin_popcountll(prod_neg);
        l1_sum = (uint16_t)(l1_sum + (uint16_t)(y_r >= 0 ? y_r : -y_r));
    }

    g_voice_ai.active_page       = (mode == 0x80u) ? 1u : 0u;
    g_voice_ai.boundary_breaches = breaches;
    g_voice_ai.l1_spectral_norm  = l1_sum;
    g_voice_ai.continuity_seal   = (breaches == 0u) ? 0x7C24u : 0xDEADu;
    g_voice_ai.ebpf_verdict      = ((uint32_t)g_voice_ai.continuity_seal << 16) | (uint32_t)l1_sum;

    // 3. Audio8 Codebook & Semantic Token Selection (4,096 = 64 x 64)
    g_voice_ai.codebook_idx      = (uint16_t)((fnv ^ (uint32_t)l1_sum) % OMNI_AUDIO8_CODEBOOK_SIZE);
    g_voice_ai.semantic_token_id = OMNI_AUDIO8_SEMANTIC_BEGIN + (uint32_t)g_voice_ai.codebook_idx;

    // 4. Worker Pipeline State: Mute Transcriber during TTS synthesis to prevent echo feedback
    if (mode == 0x82u) {
        g_voice_ai.bot_speaking      = true;
        g_voice_ai.transcriber_muted = true;
        g_voice_ai.interrupted       = false;
        g_voice_ai.total_chunks      = 8u;
        g_voice_ai.spoken_chunks     = 8u;
        uint32_t fade_pm = omni_aurora_smoothstep_permille(1000u);
        rust_reduce_rational_pq((int32_t)fade_pm, 1000, &g_voice_ai.aurora_fade_p, &g_voice_ai.aurora_fade_q);
        s3_es8311_speak(g_voice_ai.full_bot_msg, g_medusa_freq_hz, g_triad_self_pole);
    } else if (mode == 0x83u) {
        g_voice_ai.bot_speaking      = false;
        g_voice_ai.transcriber_muted = false;
    }
}

// Triggers broadcast_interrupt() on mid-sentence user barge-in
static void omni_voice_broadcast_interrupt(uint16_t chunks_played) {
    if (chunks_played >= g_voice_ai.total_chunks) {
        chunks_played = (g_voice_ai.total_chunks > 1u) ? (uint16_t)(g_voice_ai.total_chunks / 2u) : 1u;
    }
    g_voice_ai.spoken_chunks     = chunks_played;
    g_voice_ai.interrupted       = true;
    g_voice_ai.bot_speaking      = false;
    g_voice_ai.transcriber_muted = false; // Unmute ES7210 mic immediately on user interrupt

    omni_voice_get_message_up_to(
        g_voice_ai.full_bot_msg,
        g_voice_ai.spoken_chunks,
        g_voice_ai.total_chunks,
        g_voice_ai.last_bot_msg,
        sizeof(g_voice_ai.last_bot_msg)
    );

    uint32_t t_permille = ((uint32_t)g_voice_ai.spoken_chunks * 1000u) /
                          (uint32_t)(g_voice_ai.total_chunks ? g_voice_ai.total_chunks : 1u);
    uint32_t fade_pm = omni_aurora_smoothstep_permille(t_permille);
    rust_reduce_rational_pq((int32_t)fade_pm, 1000, &g_voice_ai.aurora_fade_p, &g_voice_ai.aurora_fade_q);
}

static void omni_audio8_emit_json() {
    uint32_t tile_14_us = 14u * OMNI_AUDIO8_TILE_EXEC_US; // 252 us for all 14 tiles (dim=896)
    int32_t rt_p = 0, rt_q = 1;
    rust_reduce_rational_pq((int32_t)OMNI_AUDIO8_FRAME_BUDGET_US, (int32_t)tile_14_us, &rt_p, &rt_q);

    Serial.printf(
        "# AUDIO8|{\"node\":\"%s\",\"model\":\"Edge0/Audio8-TTS-Realistic-600M\",\"arch\":\"ArkttsModel-DualAR\","
        "\"mode\":\"0x%02x\",\"page\":%u,\"seq\":%u,"
        "\"dim\":%u,\"dim_tiles\":\"14x64\",\"inter\":%u,\"inter_tiles\":\"76x64\","
        "\"codebook_size\":%u,\"codebooks\":%u,\"codebook_idx\":%u,\"semantic_id\":%lu,"
        "\"l1_norm\":%u,\"breaches\":%u,\"seal\":\"0x%04x\",\"ebpf_verdict\":\"0x%08lx\","
        "\"engram_bucket\":%lu,\"engram_bfp64_exp\":%d,\"engram_proj_q16\":%ld,"
        "\"tile_exec_us\":%u,\"layer_14tile_us\":%lu,\"frame_budget_us\":%lu,"
        "\"realtime_headroom_pq\":\"%ld/%ld\",\"zero_padding_64x64\":true,\"wave_b3\":\"%s\"}\n",
        g_medusa_node,
        (unsigned)g_voice_ai.tmv_mode,
        (unsigned)g_voice_ai.active_page,
        (unsigned)g_voice_ai.seq,
        (unsigned)OMNI_AUDIO8_DIM,
        (unsigned)OMNI_AUDIO8_INTER,
        (unsigned)OMNI_AUDIO8_CODEBOOK_SIZE,
        (unsigned)OMNI_AUDIO8_NUM_CODEBOOKS,
        (unsigned)g_voice_ai.codebook_idx,
        (unsigned long)g_voice_ai.semantic_token_id,
        (unsigned)g_voice_ai.l1_spectral_norm,
        (unsigned)g_voice_ai.boundary_breaches,
        (unsigned)g_voice_ai.continuity_seal,
        (unsigned long)g_voice_ai.ebpf_verdict,
        (unsigned long)g_voice_ai.engram_bucket,
        (int)g_voice_ai.engram_bfp64_exp,
        (long)g_voice_ai.engram_proj_q16,
        (unsigned)OMNI_AUDIO8_TILE_EXEC_US,
        (unsigned long)tile_14_us,
        (unsigned long)OMNI_AUDIO8_FRAME_BUDGET_US,
        (long)rt_p,
        (long)rt_q,
        g_voice_ai.wave_b3
    );

    Serial.printf(
        "# VOICE_AI|{\"node\":\"%s\",\"pipeline\":\"Transcriber->Agent->Synthesizer->ES8311\","
        "\"codec_dac\":\"ES8311@0x18\",\"codec_adc\":\"ES7210@0x40\",\"i2s_rate_hz\":%u,"
        "\"transcriber_muted\":%s,\"bot_speaking\":%s,\"interrupted\":%s,"
        "\"spoken_chunks\":%u,\"total_chunks\":%u,"
        "\"aurora_fade_pq\":\"%ld/%ld\",\"gaia_fold_pq\":\"%ld/%ld\","
        "\"last_stt\":\"%s\",\"last_bot_msg\":\"%s\",\"full_bot_msg\":\"%s\"}\n",
        g_medusa_node,
        (unsigned)OMNI_AUDIO8_I2S_RATE_HZ,
        g_voice_ai.transcriber_muted ? "true" : "false",
        g_voice_ai.bot_speaking ? "true" : "false",
        g_voice_ai.interrupted ? "true" : "false",
        (unsigned)g_voice_ai.spoken_chunks,
        (unsigned)g_voice_ai.total_chunks,
        (long)g_voice_ai.aurora_fade_p,
        (long)g_voice_ai.aurora_fade_q,
        (long)g_voice_ai.gaia_fold_p,
        (long)g_voice_ai.gaia_fold_q,
        g_voice_ai.last_stt_text,
        g_voice_ai.last_bot_msg,
        g_voice_ai.full_bot_msg
    );
}

static void omni_emit_full_status_json() {
    omni_screen_emit_json();
    omni_findme_emit_json();
    omni_rfid_emit_json();
    omni_ac_emit_json();
    omni_pomo_lll_emit_json();
    omni_metro_emit_json();
    omni_parents_emit_json();
    omni_audio8_emit_json();
}

// ============================================================
// Serial Command Dispatcher for v2.2.0 OMNI Subsystems
// ============================================================
static bool omni_cockpit_handle_serial(const char* line) {
    if (!line || !line[0]) return false;

    if (strcasecmp(line, "FINDME") == 0 || strcasecmp(line, "AIRTAG") == 0 ||
        strncasecmp(line, "FINDME ", 7) == 0 || strncasecmp(line, "AIRTAG ", 7) == 0) {
        const char* arg = (strlen(line) > 7) ? (line + 7) : "";
        while (*arg == ' ') arg++;
        if (*arg) {
            char target[24] = {0};
            int deg = 0;
            unsigned long cm = 145;
            if (sscanf(arg, "%23s %d %lu", target, &deg, &cm) >= 1) {
                omni_findme_update(target, (int16_t)deg, (uint32_t)cm, -45);
            }
        }
        g_omni_simple_card = 0;
        omni_findme_emit_json();
        return true;
    }

    if (strcasecmp(line, "RFID") == 0 || strcasecmp(line, "NFC") == 0 ||
        strcasecmp(line, "SUICA") == 0 || strcasecmp(line, "FELICA") == 0 ||
        strncasecmp(line, "RFID ", 5) == 0) {
        const char* arg = (strncasecmp(line, "RFID ", 5) == 0) ? (line + 5) : "";
        while (*arg == ' ') arg++;
        if (strncasecmp(arg, "FELICA ", 7) == 0) {
            char uid[24] = {0};
            unsigned long yen = 3420;
            char st[16] = "S-01:Shinjuku";
            if (sscanf(arg + 7, "%23s %lu %15s", uid, &yen, st) >= 1) {
                omni_rfid_ingest(RFID_PROTO_FELICA_SUICA, uid, "Suica Transit IC", "Suica・交通系IC", 0x0003, (uint32_t)yen, st);
            }
        } else if (strncasecmp(arg, "MIFARE ", 7) == 0) {
            char uid[24] = {0};
            if (sscanf(arg + 7, "%23s", uid) == 1) {
                omni_rfid_ingest(RFID_PROTO_MIFARE_14443A, uid, "MIFARE Keycard", "MIFARE・カード", 0x0008, 0, "GATE-8B");
            }
        } else if (strncasecmp(arg, "EM4100 ", 7) == 0) {
            char uid[24] = {0};
            if (sscanf(arg + 7, "%23s", uid) == 1) {
                omni_rfid_ingest(RFID_PROTO_EM4100_LF125K, uid, "EM4100 125kHz Tag", "EM4100・125kHzタグ", 0x0125, 0, "ZONE-0");
            }
        } else if (strcasecmp(arg, "NEXT") == 0 || strcasecmp(arg, "SCAN") == 0) {
            g_rfid_active_idx = (uint8_t)((g_rfid_active_idx + 1) % g_rfid_count);
            g_rfid_scan_seq++;
        }
        g_omni_simple_card = 3;
        omni_rfid_emit_json();
        return true;
    }

    if (strcasecmp(line, "GAME") == 0 || strcasecmp(line, "AC") == 0 ||
        strcasecmp(line, "OLEGSIV") == 0 || strcasecmp(line, "ANIMALCROSSING") == 0 ||
        strncasecmp(line, "GAME ", 5) == 0 || strncasecmp(line, "AC ", 3) == 0) {
        const char* arg = "";
        if (strncasecmp(line, "GAME ", 5) == 0) arg = line + 5;
        else if (strncasecmp(line, "AC ", 3) == 0) arg = line + 3;
        while (*arg == ' ') arg++;
        if (*arg && strcasecmp(arg, "STATUS") != 0) {
            omni_ac_action(arg);
        }
        g_omni_simple_card = 3;
        omni_ac_emit_json();
        return true;
    }

    if (strcasecmp(line, "POMODORO") == 0 || strcasecmp(line, "POMO") == 0 ||
        strcasecmp(line, "LLL") == 0 || strncasecmp(line, "POMO ", 5) == 0 ||
        strncasecmp(line, "LLL ", 4) == 0) {
        const char* arg = "";
        if (strncasecmp(line, "POMO ", 5) == 0) arg = line + 5;
        else if (strncasecmp(line, "LLL ", 4) == 0) arg = line + 4;
        while (*arg == ' ') arg++;

        if (strcasecmp(arg, "START") == 0 || strcasecmp(arg, "25") == 0) {
            g_pomo_lll.running = true;
            omni_pomo_switch_phase(POMO_PHASE_FOCUS_25M);
        } else if (strcasecmp(arg, "BREAK") == 0 || strcasecmp(arg, "5") == 0) {
            g_pomo_lll.running = true;
            omni_pomo_switch_phase(POMO_PHASE_BREAK_5M);
        } else if (strcasecmp(arg, "PAUSE") == 0 || strcasecmp(arg, "STOP") == 0) {
            g_pomo_lll.running = false;
        } else if (strcasecmp(arg, "NEXT") == 0 || strcasecmp(arg, "CARD") == 0) {
            g_pomo_lll.card_idx = (uint8_t)((g_pomo_lll.card_idx + 1) % 6);
            g_pomo_lll.cards_reviewed++;
            g_pomo_lll.lll_streak++;
        }
        g_omni_simple_card = 2;
        omni_pomo_lll_emit_json();
        return true;
    }

    if (strcasecmp(line, "METRO") == 0 || strcasecmp(line, "MAPS") == 0 ||
        strcasecmp(line, "PARENTS") == 0 || strncasecmp(line, "METRO ", 6) == 0 ||
        strncasecmp(line, "PARENTS ", 8) == 0) {
        if (strncasecmp(line, "METRO ", 6) == 0) {
            const char* arg = line + 6;
            while (*arg == ' ') arg++;
            if (strncasecmp(arg, "ROUTE ", 6) == 0) {
                char fbuf[24] = {0}, tbuf[24] = {0};
                if (sscanf(arg + 6, "%23s %23s", fbuf, tbuf) >= 2) {
                    omni_metro_plan_route(fbuf, tbuf);
                }
            }
        }
        g_omni_simple_card = 1;
        omni_metro_emit_json();
        omni_parents_emit_json();
        return true;
    }

    if (strcasecmp(line, "JP") == 0 || strcasecmp(line, "JAPANESE") == 0 ||
        strncasecmp(line, "LANG ", 5) == 0) {
        if (strncasecmp(line, "LANG ", 5) == 0) {
            const char* a = line + 5;
            while (*a == ' ') a++;
            if (strcasecmp(a, "EN") == 0) g_omni_lang = OMNI_LANG_EN;
            else if (strcasecmp(a, "JP") == 0) g_omni_lang = OMNI_LANG_JP;
            else g_omni_lang = OMNI_LANG_BI;
        } else {
            g_omni_lang = (g_omni_lang == OMNI_LANG_JP) ? OMNI_LANG_BI : OMNI_LANG_JP;
        }
        Serial.printf("# LANG|{\"node\":\"%s\",\"lang\":\"%s\",\"ui_simple\":%d,\"card\":%u}\n",
                      g_medusa_node, omni_lang_name(g_omni_lang), g_omni_ui_simple ? 1 : 0, (unsigned)g_omni_simple_card);
        return true;
    }

    if (strcasecmp(line, "UI SIMPLE") == 0 || strcasecmp(line, "SIMPLE") == 0) {
        g_omni_ui_simple = true;
        Serial.printf("# UI|{\"node\":\"%s\",\"mode\":\"SIMPLE\",\"lang\":\"%s\",\"card\":%u}\n",
                      g_medusa_node, omni_lang_name(g_omni_lang), (unsigned)g_omni_simple_card);
        return true;
    }

    if (strcasecmp(line, "UI FULL") == 0 || strcasecmp(line, "EXPERT") == 0) {
        g_omni_ui_simple = false;
        Serial.printf("# UI|{\"node\":\"%s\",\"mode\":\"FULL\",\"lang\":\"%s\",\"tab\":%u}\n",
                      g_medusa_node, omni_lang_name(g_omni_lang), (unsigned)g_s3_active_tab);
        return true;
    }

    if (strncasecmp(line, "CARD ", 5) == 0) {
        int c = atoi(line + 5);
        if (c >= 0 && c <= 4) g_omni_simple_card = (uint8_t)c;
        g_omni_ui_simple = true;
        Serial.printf("# UI|{\"node\":\"%s\",\"mode\":\"SIMPLE\",\"card\":%u}\n",
                      g_medusa_node, (unsigned)g_omni_simple_card);
        return true;
    }

    if (strcasecmp(line, "RES") == 0 || strcasecmp(line, "SCREEN") == 0 ||
        strncasecmp(line, "RES ", 4) == 0 || strncasecmp(line, "SCREEN ", 7) == 0) {
        const char* arg = "";
        if (strncasecmp(line, "RES ", 4) == 0) arg = line + 4;
        else if (strncasecmp(line, "SCREEN ", 7) == 0) arg = line + 7;
        while (*arg == ' ') arg++;
        if (*arg) {
            if (strcasecmp(arg, "18") == 0 || strcasecmp(arg, "368") == 0 || strcasecmp(arg, "ADA") == 0) {
                omni_screen_set_res(368, 448, 0, "preset_18_368x448");
            } else if (strcasecmp(arg, "206") == 0 || strcasecmp(arg, "410") == 0 || strcasecmp(arg, "OMN") == 0 || strcasecmp(arg, "VAK") == 0) {
                omni_screen_set_res(410, 502, 22, "preset_206_410x502");
            } else if (strcasecmp(arg, "TDECK") == 0) {
                omni_screen_set_res(320, 240, 0, "preset_tdeck_320x240");
            } else {
                unsigned int w = 0, h = 0, xg = 0;
                int n = sscanf(arg, "%u %u %u", &w, &h, &xg);
                if (n >= 2) {
                    uint16_t gap = (n >= 3) ? (uint16_t)xg : ((w == 410) ? 22 : 0);
                    omni_screen_set_res((uint16_t)w, (uint16_t)h, gap, "runtime_custom");
                }
            }
        }
        omni_screen_emit_json();
        return true;
    }

    if (strcasecmp(line, "AUDIO8") == 0 || strcasecmp(line, "VOICE") == 0 ||
        strcasecmp(line, "TTS") == 0 || strcasecmp(line, "SPEAK") == 0 ||
        strncasecmp(line, "AUDIO8 ", 7) == 0 || strncasecmp(line, "VOICE ", 6) == 0 ||
        strncasecmp(line, "TTS ", 4) == 0 || strncasecmp(line, "SPEAK ", 6) == 0) {
        const char* arg = "";
        if (strncasecmp(line, "AUDIO8 ", 7) == 0) arg = line + 7;
        else if (strncasecmp(line, "VOICE ", 6) == 0) arg = line + 6;
        else if (strncasecmp(line, "SPEAK ", 6) == 0) arg = line + 6;
        else if (strncasecmp(line, "TTS ", 4) == 0) arg = line + 4;
        while (*arg == ' ') arg++;

        if (strncasecmp(arg, "INTERRUPT", 9) == 0) {
            const char* sub = arg + 9;
            while (*sub == ' ') sub++;
            uint16_t chunks = (*sub) ? (uint16_t)atoi(sub) : 4u;
            omni_voice_broadcast_interrupt(chunks);
        } else if (strncasecmp(arg, "STT ", 4) == 0) {
            omni_audio8_exec_tmv(0x83u, arg + 4);
        } else if (*arg) {
            omni_audio8_exec_tmv(0x82u, arg);
        } else {
            omni_audio8_exec_tmv(0x82u, NULL);
        }
        g_omni_simple_card = 4;
        omni_audio8_emit_json();
        return true;
    }

    if (strcasecmp(line, "STT") == 0 || strncasecmp(line, "STT ", 4) == 0) {
        const char* arg = (strncasecmp(line, "STT ", 4) == 0) ? (line + 4) : "";
        while (*arg == ' ') arg++;
        omni_audio8_exec_tmv(0x83u, *arg ? arg : NULL);
        g_omni_simple_card = 4;
        omni_audio8_emit_json();
        return true;
    }

    if (strcasecmp(line, "INTERRUPT") == 0 || strncasecmp(line, "INTERRUPT ", 10) == 0) {
        const char* arg = (strncasecmp(line, "INTERRUPT ", 10) == 0) ? (line + 10) : "";
        while (*arg == ' ') arg++;
        uint16_t chunks = (*arg) ? (uint16_t)atoi(arg) : 4u;
        omni_voice_broadcast_interrupt(chunks);
        g_omni_simple_card = 4;
        omni_audio8_emit_json();
        return true;
    }

    if (strcasecmp(line, "TMV") == 0 || strncasecmp(line, "TMV ", 4) == 0) {
        const char* arg = (strncasecmp(line, "TMV ", 4) == 0) ? (line + 4) : "";
        while (*arg == ' ') arg++;
        uint8_t mode = 0x81u;
        if (strncasecmp(arg, "0x80", 4) == 0 || strcasecmp(arg, "RAW") == 0) mode = 0x80u;
        else if (strncasecmp(arg, "0x81", 4) == 0 || strcasecmp(arg, "SHIELD") == 0) mode = 0x81u;
        else if (strncasecmp(arg, "0x82", 4) == 0 || strcasecmp(arg, "TTS") == 0) mode = 0x82u;
        else if (strncasecmp(arg, "0x83", 4) == 0 || strcasecmp(arg, "STT") == 0) mode = 0x83u;
        omni_audio8_exec_tmv(mode, NULL);
        g_omni_simple_card = 4;
        omni_audio8_emit_json();
        return true;
    }

    if (strcasecmp(line, "OMNI22") == 0 || strcasecmp(line, "COCKPIT") == 0) {
        omni_emit_full_status_json();
        return true;
    }

    return false;
}

#endif // FLUGOS_OMNI_COCKPIT_V22_H
