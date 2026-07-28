/*
 * FLUG-OS MATRIX Decoder {-1, 0, +1}
 * 1:1 mapping: 802.11 packet → ternary matrix row
 *
 * Every WiFi packet decodes to exactly one row in a ternary matrix:
 *   RSSI      →  {-1, 0, +1}  (weak, mid, strong)
 *   frame_type → column index  (0=mgmt, 1=ctrl, 2=data)
 *   subtype    → ternary precision bit
 *   MAC hash   → matrix seed entropy
 *
 * Output matches ayeOS TernaryMatrix format — can be loaded directly
 * into ayeOSd for inference or MEMNET capsule distribution.
 *
 * Usage:
 *   FLUG-OS → UART → matrix_decoder → ayeOS TernaryMatrix → ayeOSd
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

// ============================================================
// Ternary encoding from packet features
// ============================================================

// 802.11 frame type → ternary column index
#define TERNARY_COL_MGMT   0  // management frames
#define TERNARY_COL_CTRL   1  // control frames
#define TERNARY_COL_DATA   2  // data frames
#define TERNARY_COLS       3  // 3-state, {-1, 0, +1}

// RSSI thresholds for ternary encoding (dBm)
#define RSSI_THRESH_LOW  -75   // < -75 → -1 (weak)
#define RSSI_THRESH_HIGH -50   // > -50 → +1 (strong)
                              // between → 0 (mid)

// ============================================================
// MAC → seed (deterministic matrix entropy)
// ============================================================
static uint32_t mac_seed(const uint8_t mac[6]) {
    // Fold 6 bytes into u32 via XOR + rotate
    uint32_t s = 0x8B1S;  // 8b-is seed constant
    for (int i = 0; i < 6; i++) {
        s ^= (uint32_t)mac[i] << ((i % 4) * 8);
        s = (s << 7) | (s >> 25);  // rotate
    }
    return s;
}

// ============================================================
// RSSI → ternary value {-1, 0, +1}
// ============================================================
static int8_t rssi_to_ternary(int8_t rssi) {
    if (rssi < RSSI_THRESH_LOW)  return -1;  // weak signal
    if (rssi > RSSI_THRESH_HIGH) return +1;  // strong signal
    return 0;                                 // mid signal
}

// ============================================================
// Packet → ternary matrix row
// ============================================================
typedef struct {
    int8_t  values[TERNARY_COLS];  // {-1, 0, +1} per column
    float   scale;                  // amplitude (from packet density)
    uint8_t group;                  // group index for ayeOS format
    uint32_t seed;                  // MAC-derived entropy
} TernaryRow;

static TernaryRow packet_to_ternary(
    int8_t rssi, int type, int subtype,
    float density, const uint8_t mac[6],
    uint32_t row_index
) {
    TernaryRow row;
    memset(&row, 0, sizeof(row));

    // RSSI → ternary value at column = frame type
    int8_t t = rssi_to_ternary(rssi);
    int col = (type <= 2) ? type : 0;
    row.values[col] = t;

    // Subtype modulates adjacent columns (ternary precision)
    if (type == 0 && subtype != 0) {
        // Management subtypes spread across {-1, 0, +1}
        // Beacon=8→+1, Deauth=12→-1, Auth=11→0, etc.
        row.values[(col + 1) % 3] = (subtype % 3) - 1;
        row.values[(col + 2) % 3] = ((subtype >> 2) % 3) - 1;
    }

    // Scale from packet density (0.0-1.0 mapped to 0.01-1.0)
    row.scale = (density < 0.01f) ? 0.01f : density;

    // Group index for ayeOS quantization
    row.group = row_index / 64;  // group_size=64

    // MAC-derived entropy for deterministic seeding
    row.seed = mac_seed(mac);

    return row;
}

// ============================================================
// FLUG-OS JSON line → ternary matrix row
// ============================================================
// Input JSON from FLUG-OS JSON mode:
//   {"rssi":0.750,"density":0.320,"freq":220.00,"type":0,"subtype":8,"ts":...}
//
// Output: one ternary row with values, scale, seed
//
// The ternary matrix can be accumulated over N packets and exported
// to ayeOS TernaryMatrix format for inference or MEMNET distribution.

// ============================================================
// Matrix → ayeOS capsule export
// ============================================================
// To export accumulated rows to ayeOS format:
//   1. Collect N rows → dim = sqrt(N)
//   2. Pack each row.values into codes (2-bit ternary: 0→-1, 1→0, 2→+1)
//   3. Build scales array from row.scale
//   4. Output JSON matching ayeOS TernaryMatrix::from_capsule_json()
//
// See: https://github.com/8b-is/ayeos

static void export_ayeos_capsule(
    const TernaryRow* rows, uint32_t count,
    const char* output_path
) {
    // dim = smallest square ≥ count
    uint32_t dim = 1;
    while (dim * dim < count) dim++;

    printf("{\n");
    printf("  \"capsule_id\": \"flug-os-%lu\",\n", (unsigned long)count);
    printf("  \"payload_type\": \"ternary_matrix\",\n");
    printf("  \"matrices\": [{\n");
    printf("    \"name\": \"flug-os\",\n");
    printf("    \"dim\": %u,\n", dim);
    printf("    \"in_features\": %u,\n", TERNARY_COLS);
    printf("    \"group_size\": 64,\n");
    printf("    \"codes\": [");
    for (uint32_t i = 0; i < count && i < 16; i++) {
        for (int c = 0; c < TERNARY_COLS; c++) {
            // Map {-1,0,+1} → {0,1,2} for 2-bit packing
            uint8_t code = (uint8_t)(rows[i].values[c] + 1);
            if (i > 0 || c > 0) printf(",");
            printf("%u", code);
        }
    }
    printf("],\n");
    printf("    \"scales\": [");
    for (uint32_t i = 0; i < count && i < 8; i++) {
        if (i > 0) printf(",");
        printf("%.3f", rows[i].scale);
    }
    printf("],\n");
    printf("    \"seed_hash\": \"%08x\"\n", rows[0].seed);
    printf("  }]\n");
    printf("}\n");
}

#endif
