import json
from pathlib import Path
import re
import subprocess
import tempfile
import struct
import unittest


REPO_ROOT = Path(__file__).resolve().parents[1]
GUARDIAN_H = REPO_ROOT / "src" / "guardian_mesh.h"
S3_APP_H = REPO_ROOT / "src" / "s3_amoled_app.h"
VERSION_H = REPO_ROOT / "src" / "version.h"

BREADCRUMB_FMT = "<HbbII3I3bBHHiiIHHQQ"
FNV32_BASIS = 0x811C9DC5
FNV32_PRIME = 0x01000193
FNV64_BASIS = 0xCBF29CE484222325
FNV64_PRIME = 0x00000100000001B3
MASK32 = 0xFFFFFFFF
MASK64 = 0xFFFFFFFFFFFFFFFF


def fnv32_bytes(data: bytes) -> int:
    h = FNV32_BASIS
    for b in data:
        h = ((h ^ b) * FNV32_PRIME) & MASK32
    return h


def mac_fnv32(mac_str: str) -> int:
    raw = bytes(int(part, 16) for part in mac_str.split(":"))
    assert len(raw) == 6
    return fnv32_bytes(raw)


def fnv64_bytes(data: bytes) -> int:
    h = FNV64_BASIS
    for b in data:
        h = ((h ^ b) * FNV64_PRIME) & MASK64
    return h


def gcd_int(a: int, b: int) -> int:
    a = abs(a)
    b = abs(b)
    while b != 0:
        a, b = b, a % b
    return a if a != 0 else 1


def reduce_rational_pq(num: int, den: int) -> tuple[int, int]:
    if den == 0:
        return (0, 1)
    if den < 0:
        num = -num
        den = -den
    if num == 0:
        return (0, 1)
    g = gcd_int(num, den)
    return (num // g, den // g)


def pack_breadcrumb64(
    node_trit: int,
    alert_level: int,
    seq: int,
    timestamp_s: int,
    top_bssids: tuple[int, int, int],
    top_rssis: tuple[int, int, int],
    zone_count: int,
    imu_delta_mg: int,
    battery_mv: int,
    bond_p: int,
    bond_q: int,
    shadow_mac_fnv32: int,
    shadow_zone_mask: int,
    hop_count: int,
    prev_chain_fnv64: int,
) -> bytes:
    prefix = struct.pack(
        "<HbbII3I3bBHHiiIHHQ",
        0x4752,
        node_trit,
        alert_level,
        seq,
        timestamp_s,
        top_bssids[0],
        top_bssids[1],
        top_bssids[2],
        top_rssis[0],
        top_rssis[1],
        top_rssis[2],
        zone_count,
        imu_delta_mg,
        battery_mv,
        bond_p,
        bond_q,
        shadow_mac_fnv32,
        shadow_zone_mask,
        hop_count,
        prev_chain_fnv64,
    )
    assert len(prefix) == 56
    seal = fnv64_bytes(prefix)
    return prefix + struct.pack("<Q", seal)


class GuardianMeshTests(unittest.TestCase):
    def test_zero_fp_and_version_2_1_0(self):
        src_text = GUARDIAN_H.read_text(encoding="utf-8")
        code_lines = [
            line.split("//")[0]
            for line in src_text.splitlines()
            if not line.strip().startswith("//")
        ]
        code_only = "\n".join(code_lines)

        banned_tokens = ["fl" + "oat", "dou" + "ble"]
        for tok in banned_tokens:
            self.assertNotIn(tok, code_only)
        self.assertIsNone(re.search(r"\b\d+\.\d+f?\b", code_only))

        ver_text = VERSION_H.read_text(encoding="utf-8")
        self.assertIn('#define FLUGOS_VERSION_STR    "2.2.0"', ver_text)
        self.assertIn('#define FLUGOS_COMMIT         "omni-findme-rfid-ac-pomodoro-metro-jp"', ver_text)

        app_text = S3_APP_H.read_text(encoding="utf-8")
        self.assertIn('#include "pranayama.h"\n#include "guardian_mesh.h"\n#include "omni_cockpit_v22.h"', app_text)

    def test_breadcrumb64_binary_layout_and_fnv64_chain(self):
        self.assertEqual(struct.calcsize(BREADCRUMB_FMT), 64)

        stalker_hash = mac_fnv32("DE:AD:BE:EF:42:99")
        chain = FNV64_BASIS
        frames = []

        for z in range(3):
            alert = 2 if z == 2 else (1 if z == 1 else 0)
            mask = (1 << (z + 1)) - 1
            bp, bq = reduce_rational_pq((5 + z) * 3 + (3 + z) * 5 + 2 * 7 + 1 * 11, 100)
            pkt = pack_breadcrumb64(
                node_trit=-1,
                alert_level=alert,
                seq=z + 1,
                timestamp_s=1730000100 + z * 300,
                top_bssids=(0xA1000101 + z, 0xA1000102 + z, 0xA1000103 + z),
                top_rssis=(-48 - z, -55 - z, -63 - z),
                zone_count=z + 1,
                imu_delta_mg=120,
                battery_mv=4080,
                bond_p=bp,
                bond_q=bq,
                shadow_mac_fnv32=stalker_hash if z >= 1 else 0,
                shadow_zone_mask=mask if z >= 1 else 0,
                hop_count=z,
                prev_chain_fnv64=chain,
            )
            self.assertEqual(len(pkt), 64)
            unpacked = struct.unpack(BREADCRUMB_FMT, pkt)
            self.assertEqual(unpacked[0], 0x4752)
            self.assertEqual(unpacked[-2], chain)
            self.assertEqual(unpacked[-1], fnv64_bytes(pkt[:56]))
            chain = unpacked[-1]
            frames.append(pkt)

        self.assertEqual(len(frames), 3)

    def test_young_wave_partnership_rational_reduction(self):
        # Initial Chiro-chan state: care=4, syncs=2, duets=2, thoughts=1 -> 12+10+14+11 = 47/100
        self.assertEqual(reduce_rational_pq(4 * 3 + 2 * 5 + 2 * 7 + 1 * 11, 100), (47, 100))
        # Reducible bond score: care=5, syncs=3, duets=2, thoughts=1 -> 15+15+14+11 = 55/100 -> 11/20
        self.assertEqual(reduce_rational_pq(5 * 3 + 3 * 5 + 2 * 7 + 1 * 11, 100), (11, 20))
        # Sovereign-Guardian threshold: care=10, syncs=6, duets=4, thoughts=2 -> 30+30+28+22 = 110/100 -> 11/10
        self.assertEqual(reduce_rational_pq(10 * 3 + 6 * 5 + 4 * 7 + 2 * 11, 100), (11, 10))

    def test_native_cpp_guardian_mesh_simulation_and_commands(self):
        cpp_harness = r"""
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

struct MockSerial {
    template <typename... Args>
    void printf(const char* fmt, Args... args) {
        ::printf(fmt, args...);
    }
} Serial;

struct MockPower {
    uint16_t batt_mv = 4120;
} g_power;

struct MockImu {
    int16_t ax_mg = 0;
    int16_t ay_mg = 0;
    int16_t az_mg = 1000;
} g_imu;

struct S3MedusaAp {
    bool    used;
    uint8_t bssid[6];
    char    ssid[33];
    uint8_t ch;
    int8_t  rssi;
};
static S3MedusaAp g_medusa_aps[16];
static uint8_t    g_medusa_ap_count = 0;
static const char* g_medusa_node = "ADA:MEDUSA";
static uint16_t    g_medusa_freq_hz = 432;
static int8_t      g_triad_self_pole = -1;
static uint8_t     g_s3_last_probed_mac[6] = {0};
static int8_t      g_s3_last_rssi = -55;

static unsigned long g_mock_ms = 5000;
static unsigned long millis() { return g_mock_ms; }

extern "C" void rust_reduce_rational_pq(int32_t num, int32_t den, int32_t* out_p, int32_t* out_q) {
    if (den == 0) { *out_p = 0; *out_q = 1; return; }
    if (den < 0) { num = -num; den = -den; }
    if (num == 0) { *out_p = 0; *out_q = 1; return; }
    int32_t a = num < 0 ? -num : num;
    int32_t b = den;
    while (b != 0) { int32_t t = a % b; a = b; b = t; }
    *out_p = num / a;
    *out_q = den / a;
}

static int hive_hash9(const char* s, size_t n, char sig[10]) {
    int8_t t[9] = {0};
    for (size_t i = 0; i < n; i++) {
        int b = (uint8_t)s[i];
        int idx = (int)((i + b) % 9);
        t[idx] = (int8_t)((((t[idx] + (b % 3) - 1 + 4) % 3)) - 1);
    }
    int net = 0;
    for (int i = 0; i < 9; i++) {
        sig[i] = t[i] < 0 ? 'T' : (t[i] > 0 ? '1' : '0');
        net += t[i];
    }
    sig[9] = '\0';
    return net;
}

static int g_tts_calls = 0;
static void s3_es8311_speak(const char*, uint16_t, int8_t) { g_tts_calls++; }
static void s3_wave_pool_splash(uint8_t, uint8_t, int16_t, uint8_t) {}
static void s3_espnow_broadcast(uint8_t, uint8_t, int16_t) {}

#include "guardian_mesh.h"

int main() {
    guardian_handle_serial("GUARDIAN TRUST AA:BB:CC:DD:EE:11");
    guardian_handle_serial("GUARDIAN SIM_SHADOW");
    guardian_handle_serial("GUARDIAN THOUGHT Safe at Sakura Park with Chiro-chan!");
    return 0;
}
"""
        with tempfile.TemporaryDirectory() as tmpdir:
            cpp_path = Path(tmpdir) / "harness.cpp"
            bin_path = Path(tmpdir) / "harness_bin"
            cpp_path.write_text(cpp_harness, encoding="utf-8")
            subprocess.run(
                [
                    "g++",
                    "-std=c++17",
                    "-Wall",
                    "-Wextra",
                    "-Werror",
                    f"-I{REPO_ROOT / 'src'}",
                    str(cpp_path),
                    "-o",
                    str(bin_path),
                ],
                check=True,
            )
            proc = subprocess.run([str(bin_path)], capture_output=True, text=True, check=True)
            lines = [ln.strip() for ln in proc.stdout.splitlines() if ln.strip()]

        guardian_lines = [ln for ln in lines if ln.startswith("# GUARDIAN|")]
        thought_lines = [ln for ln in lines if ln.startswith("# THOUGHT|")]
        trust_lines = [ln for ln in lines if ln.startswith("# GUARDIAN_TRUST|")]

        self.assertEqual(len(trust_lines), 1)
        self.assertEqual(len(guardian_lines), 3)
        self.assertEqual(len(thought_lines), 2)

        prev_chain = FNV64_BASIS
        expected_stalker_fnv32 = f"0x{mac_fnv32('DE:AD:BE:EF:42:99'):08x}"

        for idx, gl in enumerate(guardian_lines):
            payload = json.loads(gl.split("|", 1)[1])
            self.assertEqual(payload["zone_idx"], idx)
            raw_bytes = bytes.fromhex(payload["breadcrumb64_hex"])
            self.assertEqual(len(raw_bytes), 64)
            unpacked = struct.unpack(BREADCRUMB_FMT, raw_bytes)
            self.assertEqual(unpacked[0], 0x4752)
            self.assertEqual(unpacked[-2], prev_chain)
            self.assertEqual(unpacked[-1], fnv64_bytes(raw_bytes[:56]))
            prev_chain = unpacked[-1]

        final_guardian = json.loads(guardian_lines[2].split("|", 1)[1])
        self.assertEqual(final_guardian["alert_level"], 2)
        self.assertEqual(final_guardian["alert"], "ALERT_SHADOW_PERSISTENCE")
        self.assertEqual(final_guardian["shadow_zones"], 3)
        self.assertEqual(final_guardian["shadow_zone_mask"], "0x0007")
        self.assertEqual(final_guardian["shadow_mac_fnv32"], expected_stalker_fnv32)
        self.assertEqual(final_guardian["trusted_flock"], 4)

        final_thought = json.loads(thought_lines[-1].split("|", 1)[1])
        self.assertEqual(final_thought["companion"], "Chiro-chan")
        self.assertEqual(len(final_thought["wave_b3"]), 9)
        self.assertTrue(set(final_thought["wave_b3"]).issubset({"T", "0", "1"}))
        self.assertIn("/", final_thought["bond_pq"])


if __name__ == "__main__":
    unittest.main()
