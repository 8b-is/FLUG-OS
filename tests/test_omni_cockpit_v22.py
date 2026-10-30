import json
from pathlib import Path
import re
import subprocess
import tempfile
import unittest


REPO_ROOT = Path(__file__).resolve().parents[1]
OMNI_V22_H = REPO_ROOT / "src" / "omni_cockpit_v22.h"
S3_APP_H = REPO_ROOT / "src" / "s3_amoled_app.h"
NET_TIME_H = REPO_ROOT / "src" / "net_time.h"
VERSION_H = REPO_ROOT / "src" / "version.h"


class OmniCockpitV22Tests(unittest.TestCase):
    def test_zero_float_and_version_2_2_0(self):
        src_text = OMNI_V22_H.read_text(encoding="utf-8")
        code_lines = [
            line.split("//")[0]
            for line in src_text.splitlines()
            if not line.strip().startswith("//")
        ]
        code_only = "\n".join(code_lines)

        for tok in ("fl" + "oat", "dou" + "ble"):
            self.assertNotIn(tok, code_only)
        self.assertIsNone(re.search(r"\b\d+\.\d+f?\b", code_only))

        ver_text = VERSION_H.read_text(encoding="utf-8")
        self.assertIn('#define FLUGOS_VERSION_MAJOR  2', ver_text)
        self.assertIn('#define FLUGOS_VERSION_MINOR  2', ver_text)
        self.assertIn('#define FLUGOS_VERSION_STR    "2.2.0"', ver_text)
        self.assertIn('#define FLUGOS_HAS_OMNI_COCKPIT_V22 1', ver_text)

        app_text = S3_APP_H.read_text(encoding="utf-8")
        self.assertIn('#include "omni_cockpit_v22.h"', app_text)
        self.assertIn("omni_cockpit_handle_serial(line)", app_text)
        self.assertIn("omni_pomo_tick(now)", app_text)
        self.assertIn("net_hotspot_command(line + 7)", app_text)

        net_text = NET_TIME_H.read_text(encoding="utf-8")
        self.assertIn("GALAXY_S25_ULTRA", net_text)
        self.assertIn("GL_INET_ROUTER", net_text)
        self.assertIn("net_classify_hotspot_tier", net_text)
        self.assertIn("net_hotspot_command", net_text)

    def test_native_cpp_omni_cockpit_v22_subsystems(self):
        cpp_harness = r"""
#define FLUGOS_HOST_SIM 1
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
    uint16_t batt_mv = 4150;
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
static S3MedusaAp  g_medusa_aps[16];
static uint8_t     g_medusa_ap_count = 0;
static const char* g_medusa_node = "OMN:MEDUSA";
static uint16_t    g_medusa_freq_hz = 852;
static int8_t      g_triad_self_pole = 1;
static uint8_t     g_s3_last_probed_mac[6] = {0};
static int8_t      g_s3_last_rssi = -47;
static uint8_t     g_s3_active_tab = 0;
static bool        g_board_is_18 = true;
static uint16_t    g_lcd_w = 368;
static uint16_t    g_lcd_h = 448;
static uint16_t    g_lcd_x_gap = 0;
static uint16_t    g_lcd_y_gap = 0;
static const char* g_lcd_driver_name = "SH8601";
static const char* g_lcd_det_src = "efuse_mac+i2c_tca9554";
static const char* g_board_model_name = "ESP32-S3-Touch-AMOLED-368x448";

static unsigned long g_mock_ms = 1000;
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

static void s3_es8311_speak(const char*, uint16_t, int8_t) {}
static void s3_wave_pool_splash(uint8_t, uint8_t, int16_t, uint8_t) {}
static void s3_espnow_broadcast(uint8_t, uint8_t, int16_t) {}

#include "guardian_mesh.h"
#include "omni_cockpit_v22.h"

struct MockCanvas {
    int rects = 0;
    int lines = 0;
    void fill_rect(int, int, int, int, uint16_t) { rects++; }
    void draw_line(int, int, int, int, uint16_t, int = 1) { lines++; }
};

int main() {
    guardian_init();

    // 1. FindMy / AirTag vector navigation (including SE 135 deg "\\v" and NW -45 deg "^\\" backslash octants)
    omni_cockpit_handle_serial("FINDME ADA:MEDUSA 45 145");
    omni_cockpit_handle_serial("AIRTAG PARENTS -90 80");
    omni_cockpit_handle_serial("FINDME OMN:MEDUSA 135 320");
    omni_cockpit_handle_serial("FINDME TDECK:AYEOS -45 640");

    // 2. Multi-protocol RFID / FeliCa Suica / MIFARE / EM4100 + ring-buffer Suica preservation
    omni_cockpit_handle_serial("RFID FELICA 011401008899AABB 4580 S-06:Jimbocho");
    omni_cockpit_handle_serial("RFID MIFARE 04CAFEBABE8B15");
    for (int k = 0; k < 6; k++) {
        omni_cockpit_handle_serial("RFID EM4100 1A00998877");
    }

    // 3. Animal Crossing x Olegsiv Unity game
    omni_cockpit_handle_serial("GAME HARVEST");
    omni_cockpit_handle_serial("AC FISH");
    omni_cockpit_handle_serial("GAME TRADE");

    // 4. Pomodoro 25m / 5m + LLL continual learning + multi-second carry-over across phase boundary
    omni_cockpit_handle_serial("POMO START");
    omni_pomo_tick(1000);
    omni_pomo_tick(1506000); // 1505s elapsed -> transitions to 15m long break AND carries 5s into break (895s left)!
    omni_cockpit_handle_serial("LLL NEXT");

    // 5. Tokyo Metro + Google Maps route + Parents live share (cross-line, same-line direct, same-station, alert state)
    omni_cockpit_handle_serial("METRO ROUTE Shinjuku Roppongi");
    omni_cockpit_handle_serial("METRO ROUTE Shinjuku Jimbocho");
    omni_cockpit_handle_serial("METRO ROUTE Shinjuku Shinjuku");
    g_guardian_alert = ALERT_WATCH;
    omni_cockpit_handle_serial("PARENTS");

    // 6. Japanese Localization + Simplified UI + Dynamic Screen Resolution
    omni_cockpit_handle_serial("LANG JP");
    omni_cockpit_handle_serial("UI SIMPLE");
    omni_cockpit_handle_serial("CARD 2");
    omni_cockpit_handle_serial("RES ADA");
    omni_cockpit_handle_serial("RES OMN");
    omni_cockpit_handle_serial("RES TDECK");

    // 7. Canvas helpers (Kanji 8x8 + FindMy arrow)
    MockCanvas cv;
    omni_draw_kanji_8x8(cv, 10, 10, 4, 0xFFFF, 2);
    omni_draw_findme_arrow(cv, 50, 50, 12, 45, 0x07E0);
    ::printf("# CANVAS|{\"rects\":%d,\"lines\":%d}\n", cv.rects, cv.lines);
    return 0;
}
"""
        with tempfile.TemporaryDirectory() as tmpdir:
            cpp_path = Path(tmpdir) / "omni_v22_harness.cpp"
            bin_path = Path(tmpdir) / "omni_v22_bin"
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

        findme_lines = [json.loads(ln.split("|", 1)[1]) for ln in lines if ln.startswith("# FINDME|")]
        rfid_lines = [json.loads(ln.split("|", 1)[1]) for ln in lines if ln.startswith("# RFID|")]
        game_lines = [json.loads(ln.split("|", 1)[1]) for ln in lines if ln.startswith("# GAME|")]
        pomo_lines = [json.loads(ln.split("|", 1)[1]) for ln in lines if ln.startswith("# POMODORO|")]
        metro_lines = [json.loads(ln.split("|", 1)[1]) for ln in lines if ln.startswith("# METRO|")]
        parents_lines = [json.loads(ln.split("|", 1)[1]) for ln in lines if ln.startswith("# PARENTS|")]
        lang_lines = [json.loads(ln.split("|", 1)[1]) for ln in lines if ln.startswith("# LANG|")]
        ui_lines = [json.loads(ln.split("|", 1)[1]) for ln in lines if ln.startswith("# UI|")]
        res_lines = [json.loads(ln.split("|", 1)[1]) for ln in lines if ln.startswith("# RES|")]
        canvas_lines = [json.loads(ln.split("|", 1)[1]) for ln in lines if ln.startswith("# CANVAS|")]

        self.assertEqual(len(res_lines), 3)
        self.assertEqual((res_lines[0]["width"], res_lines[0]["height"], res_lines[0]["x_gap"]), (368, 448, 0))
        self.assertEqual(res_lines[0]["aspect_pq"], "23/28")
        self.assertEqual(res_lines[0]["driver"], "SH8601")
        self.assertEqual((res_lines[1]["width"], res_lines[1]["height"], res_lines[1]["x_gap"]), (410, 502, 22))
        self.assertEqual(res_lines[1]["aspect_pq"], "205/251")
        self.assertEqual(res_lines[1]["driver"], "CO5300")
        self.assertEqual((res_lines[2]["width"], res_lines[2]["height"], res_lines[2]["x_gap"]), (320, 240, 0))
        self.assertEqual(res_lines[2]["aspect_pq"], "4/3")
        self.assertEqual(res_lines[2]["driver"], "ST7789")

        # 1. Verify FindMy / AirTag arrows & reduced rational distance (including backslash JSON escaping)
        self.assertEqual(len(findme_lines), 4)
        self.assertEqual(findme_lines[0]["target"], "ADA:MEDUSA")
        self.assertEqual(findme_lines[0]["arrow"], "↗")
        self.assertEqual(findme_lines[0]["dist_m_pq"], "29/20")  # 145/100 = 29/20
        self.assertEqual(findme_lines[0]["proximity"], "NEARBY")
        self.assertEqual(findme_lines[1]["target"], "PARENTS")
        self.assertEqual(findme_lines[1]["arrow"], "←")
        self.assertEqual(findme_lines[1]["dist_m_pq"], "4/5")  # 80/100 = 4/5
        self.assertEqual(findme_lines[1]["proximity"], "HERE")
        self.assertEqual(findme_lines[2]["arrow_ascii"], "\\v")
        self.assertEqual(findme_lines[3]["arrow_ascii"], "^\\")

        # 2. Verify RFID / FeliCa Suica & MIFARE + Suica preservation across ring buffer wrap
        self.assertEqual(len(rfid_lines), 8)
        self.assertEqual(rfid_lines[0]["protocol"], "NFC-F/FeliCa-Suica")
        self.assertEqual(rfid_lines[0]["balance_yen"], 4580)
        self.assertEqual(rfid_lines[0]["station_code"], "S-06:Jimbocho")
        self.assertEqual(len(rfid_lines[0]["wave_b3"]), 9)
        self.assertEqual(rfid_lines[1]["protocol"], "ISO14443A/MIFARE")
        self.assertEqual(rfid_lines[1]["uid_hex"], "04CAFEBABE8B15")

        # 3. Verify Animal Crossing x Olegsiv Unity game
        self.assertEqual(len(game_lines), 3)
        last_game = game_lines[-1]
        self.assertEqual(last_game["engine"], "olegsiv-unity-animal-crossing")
        self.assertEqual(last_game["island"], "Sakura-Jima")
        self.assertEqual(last_game["bells"], 4320 + 300 + 528 + 852)
        self.assertEqual(last_game["sakura_petals"], 30)
        self.assertEqual(last_game["fish_caught"], 13)
        self.assertIn("/", last_game["turnip_ratio_pq"])

        # 4. Verify Pomodoro 25m -> 15m transition with 5s carry-over + LLL mastery
        self.assertEqual(len(pomo_lines), 2)
        self.assertEqual(pomo_lines[0]["phase"], "FOCUS 25:00")
        self.assertEqual(pomo_lines[0]["remaining_mmss"], "25:00")
        self.assertEqual(pomo_lines[1]["completed_pomos"], 4)
        self.assertEqual(pomo_lines[1]["phase"], "LONG BREAK 15:00")
        self.assertEqual(pomo_lines[1]["remaining_s"], 895)
        self.assertIn("/", pomo_lines[1]["lll"]["mastery_pq"])

        # 5. Verify Tokyo Metro + Google Maps E6 route + Parents live share (cross-line, direct same-line, same-station)
        self.assertEqual(len(metro_lines), 4)
        self.assertEqual(metro_lines[0]["from"]["code"], "S-01")
        self.assertEqual(metro_lines[0]["from"]["name_jp"], "新宿")
        self.assertEqual(metro_lines[0]["to"]["code"], "H-04")
        self.assertEqual(metro_lines[0]["to"]["name_jp"], "六本木")
        self.assertEqual(metro_lines[0]["transfer"]["code"], "S-06")
        self.assertEqual(metro_lines[0]["suica_balance_yen"], 4580)
        self.assertGreater(metro_lines[0]["eta_min"], 0)
        # Same-line direct route S-01 -> S-06
        self.assertEqual(metro_lines[1]["from"]["code"], "S-01")
        self.assertEqual(metro_lines[1]["to"]["code"], "S-06")
        self.assertEqual(metro_lines[1]["transfer"]["code"], "DIRECT")
        self.assertEqual(metro_lines[1]["stops"], 5)
        self.assertEqual(metro_lines[1]["eta_min"], 10)
        # Same-station route S-01 -> S-01
        self.assertEqual(metro_lines[2]["from"]["code"], "S-01")
        self.assertEqual(metro_lines[2]["to"]["code"], "S-01")
        self.assertEqual(metro_lines[2]["transfer"]["code"], "NONE")
        self.assertEqual(metro_lines[2]["stops"], 0)
        self.assertEqual(metro_lines[2]["eta_min"], 0)

        self.assertEqual(len(parents_lines), 4)
        self.assertEqual(parents_lines[0]["companion"], "Chiro-chan")
        self.assertEqual(parents_lines[0]["route_jp"], "新宿駅 → 六本木駅")
        self.assertEqual(parents_lines[0]["suica_yen"], 4580)
        self.assertTrue(parents_lines[0]["seal_fnv64"].startswith("0x"))
        self.assertEqual(parents_lines[3]["safety_alert"], "WATCH")
        self.assertTrue(parents_lines[3]["note"].endswith("WATCH"))

        # 6. Verify Japanese Localization + UI Simple mode + Canvas helpers
        self.assertEqual(lang_lines[0]["lang"], "JP")
        self.assertEqual(ui_lines[-1]["mode"], "SIMPLE")
        self.assertEqual(ui_lines[-1]["card"], 2)
        self.assertGreater(canvas_lines[0]["rects"], 10)
        self.assertEqual(canvas_lines[0]["lines"], 3)

    def test_hardware_res_guard_and_hotspot_tier_classifier(self):
        cpp_hw_test = r"""
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

struct MockPower { uint16_t batt_mv = 4150; } g_power;
struct MockImu { int16_t ax_mg = 0, ay_mg = 0, az_mg = 1000; } g_imu;
struct S3MedusaAp { bool used; uint8_t bssid[6]; char ssid[33]; uint8_t ch; int8_t rssi; };
static S3MedusaAp  g_medusa_aps[16];
static uint8_t     g_medusa_ap_count = 0;
static const char* g_medusa_node = "ADA:MEDUSA";
static uint16_t    g_medusa_freq_hz = 432;
static int8_t      g_triad_self_pole = -1;
static uint8_t     g_s3_last_probed_mac[6] = {0};
static int8_t      g_s3_last_rssi = -47;
static uint8_t     g_s3_active_tab = 0;
static bool        g_board_is_18 = true;
static uint16_t    g_lcd_w = 368;
static uint16_t    g_lcd_h = 448;
static uint16_t    g_hw_lcd_w = 368;
static uint16_t    g_hw_lcd_h = 448;
static uint16_t    g_lcd_x_gap = 0;
static uint16_t    g_lcd_y_gap = 0;
static const char* g_lcd_driver_name = "SH8601";
static const char* g_lcd_det_src = "efuse_mac+i2c_tca9554";
static const char* g_board_model_name = "ESP32-S3-Touch-AMOLED-368x448";

static unsigned long millis() { return 1000; }
extern "C" void rust_reduce_rational_pq(int32_t num, int32_t den, int32_t* out_p, int32_t* out_q) {
    if (den == 0) { *out_p = 0; *out_q = 1; return; }
    if (den < 0) { num = -num; den = -den; }
    if (num == 0) { *out_p = 0; *out_q = 1; return; }
    int32_t a = num < 0 ? -num : num, b = den;
    while (b != 0) { int32_t t = a % b; a = b; b = t; }
    *out_p = num / a; *out_q = den / a;
}
static void s3_es8311_speak(const char*, uint16_t, int8_t) {}
static void s3_wave_pool_splash(uint8_t, uint8_t, int16_t, uint8_t) {}
static void s3_espnow_broadcast(uint8_t, uint8_t, int16_t) {}
static int hive_hash9(const char* s, size_t n, char sig[10]) {
    (void)s; (void)n; memcpy(sig, "101T01101", 10); return 1;
}

#include "guardian_mesh.h"
#include "omni_cockpit_v22.h"

int main() {
    guardian_init();
    omni_pomo_tick(1000);
    // Attempt to set 410x502 on a 368x448 physical panel -> must reject and preserve SH8601 + 368x448!
    omni_cockpit_handle_serial("RES OMN");
    // Attempt to set 320x240 on a 368x448 physical panel -> allowed, but must preserve SH8601 driver and g_board_is_18!
    omni_cockpit_handle_serial("RES TDECK");
    ::printf("# BOARD18|%d\n", g_board_is_18 ? 1 : 0);
    return 0;
}
"""
        with tempfile.TemporaryDirectory() as tmpdir:
            cpp_path = Path(tmpdir) / "omni_hw_test.cpp"
            bin_path = Path(tmpdir) / "omni_hw_bin"
            cpp_path.write_text(cpp_hw_test, encoding="utf-8")
            subprocess.run(
                ["g++", "-std=c++17", "-Wall", "-Wextra", "-Werror", f"-I{REPO_ROOT / 'src'}", str(cpp_path), "-o", str(bin_path)],
                check=True,
            )
            proc = subprocess.run([str(bin_path)], capture_output=True, text=True, check=True)
            lines = [ln.strip() for ln in proc.stdout.splitlines() if ln.strip()]

        err_lines = [json.loads(ln.split("|", 1)[1]) for ln in lines if ln.startswith("# RES_ERR|")]
        res_lines = [json.loads(ln.split("|", 1)[1]) for ln in lines if ln.startswith("# RES|")]
        self.assertEqual(len(err_lines), 1)
        self.assertEqual(err_lines[0]["reason"], "exceeds_physical_panel")
        self.assertEqual((res_lines[0]["width"], res_lines[0]["height"]), (368, 448))
        self.assertEqual(res_lines[0]["driver"], "SH8601")
        self.assertEqual((res_lines[1]["width"], res_lines[1]["height"]), (320, 240))
        self.assertEqual(res_lines[1]["driver"], "SH8601")
        self.assertEqual(res_lines[1]["x_gap"], 0)


if __name__ == "__main__":
    unittest.main()
