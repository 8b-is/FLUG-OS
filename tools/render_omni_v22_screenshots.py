#!/usr/bin/env python3
"""
Native C++17 Framebuffer Screenshot Renderer for FLUG-OS v2.2.0.
Renders pixel-accurate RGB565 frames using the firmware's exact S3Canvas,
FONT5X7, OMNI_KANJI_8X8, FindMy vector arrow, 2D Wave Pool, and responsive
layout across all dynamically detected device resolutions:
  - 368x448 (ADA:MEDUSA — ESP32-S3-Touch-AMOLED-1.8, SH8601)
  - 410x502 (OMN:MEDUSA / VAK:MEDUSA — ESP32-S3-Touch-AMOLED-2.06, CO5300)
  - 320x240 (TDECK:AYEOS — LilyGO T-Deck Plus 2.8" Landscape, ST7789)
"""

from pathlib import Path
import struct
import subprocess
import tempfile
import zlib

REPO_ROOT = Path(__file__).resolve().parents[1]
OUT_DIR = REPO_ROOT / "docs" / "screenshots"


def write_png_rgb(path: Path, width: int, height: int, rgb_bytes: bytes) -> None:
    assert len(rgb_bytes) == width * height * 3
    raw_rows = bytearray()
    stride = width * 3
    for y in range(height):
        raw_rows.append(0)  # filter type 0 (None)
        raw_rows.extend(rgb_bytes[y * stride : (y + 1) * stride])

    def png_chunk(chunk_type: bytes, data: bytes) -> bytes:
        crc = zlib.crc32(chunk_type + data) & 0xFFFFFFFF
        return struct.pack(">I", len(data)) + chunk_type + data + struct.pack(">I", crc)

    ihdr = struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)
    idat = zlib.compress(bytes(raw_rows), level=9)
    png_data = (
        b"\x89PNG\r\n\x1a\n"
        + png_chunk(b"IHDR", ihdr)
        + png_chunk(b"IDAT", idat)
        + png_chunk(b"IEND", b"")
    )
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(png_data)


CPP_RENDERER = r"""
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define FLUGOS_HOST_SIM 1
#define PROGMEM
#define pgm_read_byte(addr) (*(const uint8_t*)(addr))
#ifndef min
#define min(a, b) (((a) < (b)) ? (a) : (b))
#endif
#ifndef max
#define max(a, b) (((a) > (b)) ? (a) : (b))
#endif

struct MockSerial {
    template <typename... Args>
    void printf(const char*, Args...) {}
} Serial;

struct S3PowerState {
    bool     present;
    bool     vbus_good;
    bool     charging;
    uint16_t batt_mv;
    uint8_t  batt_pct;
};
static S3PowerState g_power = {true, true, true, 4160, 98};

struct S3ImuState {
    bool    present;
    float   ax, ay, az;
    float   gx, gy, gz;
    float   gyro_mag;
    float   sickness;
    int8_t  trit_v, trit_a, trit_d;
    uint8_t qt_byte;
    uint8_t rtc_h, rtc_m, rtc_s;
    int16_t ax_mg, ay_mg, az_mg;
};
static S3ImuState g_imu = {true, 0.12f, -0.05f, 0.98f, 0, 0, 0, 0, 0.04f, 1, 0, 1, 0x26, 17, 55, 0, 120, -50, 980};

struct S3MedusaAp {
    bool    used;
    uint8_t bssid[6];
    char    ssid[33];
    uint8_t ch;
    int8_t  rssi;
};
static S3MedusaAp  g_medusa_aps[16];
static uint8_t     g_medusa_ap_count = 4;
static const char* g_medusa_node = "ADA:MEDUSA";
static uint16_t    g_medusa_freq_hz = 432;
static int8_t      g_triad_self_pole = -1;
static uint8_t     g_triad_self_id = 0;
static uint8_t     g_s3_last_probed_mac[6] = {0};
static int8_t      g_s3_last_rssi = -46;
static uint8_t     g_s3_active_tab = 0;

static bool        g_board_is_18 = true;
static uint16_t    g_lcd_w = 368;
static uint16_t    g_lcd_h = 448;
static uint16_t    g_lcd_x_gap = 0;
static uint16_t    g_lcd_y_gap = 0;
static const char* g_lcd_driver_name = "SH8601";
static const char* g_lcd_det_src = "efuse_mac+i2c_tca9554";
static const char* g_board_model_name = "ESP32-S3-Touch-AMOLED-368x448";

static unsigned long g_mock_ms = 12000;
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

static inline uint16_t rgb565_be(uint8_t r, uint8_t g, uint8_t b) {
    uint16_t v = ((uint16_t)(r & 0xF8) << 8) |
                 ((uint16_t)(g & 0xFC) << 3) |
                 ((uint16_t)(b >> 3));
    return (uint16_t)((v >> 8) | (v << 8));
}

#define COL_BG          rgb565_be(5,   6,  10)
#define COL_VOID        rgb565_be(0,   0,   0)
#define COL_BG_ELEV     rgb565_be(11, 14,  23)
#define COL_BG_PANEL    rgb565_be(17, 22,  36)
#define COL_BORDER      rgb565_be(32, 42,  68)
#define COL_GRID        rgb565_be(20, 26,  42)
#define COL_TEXT        rgb565_be(236, 242, 255)
#define COL_SUBTEXT     rgb565_be(138, 152, 184)
#define COL_DIM         rgb565_be(76,  88, 118)
#define COL_AMBER_INFO  rgb565_be(140, 102, 38)
#define COL_MINT        rgb565_be(98,  230, 201)
#define COL_MINT_DARK   rgb565_be(22,  68,  60)
#define COL_GOLD        rgb565_be(232, 176, 92)
#define COL_GOLD_DARK   rgb565_be(72,  52,  22)
#define COL_VIOLET      rgb565_be(180, 139, 255)
#define COL_VIOLET_DARK rgb565_be(48,  34,  82)
#define COL_CYAN        rgb565_be(0,   240, 255)
#define COL_CORAL       rgb565_be(255, 92,  119)
#define COL_CORAL_DARK  rgb565_be(76,  24,  36)

static const uint8_t FONT5X7[95][5] = {
    {0x00,0x00,0x00,0x00,0x00}, {0x00,0x00,0x5F,0x00,0x00}, {0x00,0x07,0x00,0x07,0x00},
    {0x14,0x7F,0x14,0x7F,0x14}, {0x24,0x2A,0x7F,0x2A,0x12}, {0x23,0x13,0x08,0x64,0x62},
    {0x36,0x49,0x55,0x22,0x50}, {0x00,0x05,0x03,0x00,0x00}, {0x00,0x1C,0x22,0x41,0x00},
    {0x00,0x41,0x22,0x1C,0x00}, {0x14,0x08,0x3E,0x08,0x14}, {0x08,0x08,0x3E,0x08,0x08},
    {0x00,0x50,0x30,0x00,0x00}, {0x08,0x08,0x08,0x08,0x08}, {0x00,0x60,0x60,0x00,0x00},
    {0x20,0x10,0x08,0x04,0x02}, {0x3E,0x51,0x49,0x45,0x3E}, {0x00,0x42,0x7F,0x40,0x00},
    {0x42,0x61,0x51,0x49,0x46}, {0x21,0x41,0x45,0x4B,0x31}, {0x18,0x14,0x12,0x7F,0x10},
    {0x27,0x45,0x45,0x45,0x39}, {0x3C,0x4A,0x49,0x49,0x30}, {0x01,0x71,0x09,0x05,0x03},
    {0x36,0x49,0x49,0x49,0x36}, {0x06,0x49,0x49,0x29,0x1E}, {0x00,0x36,0x36,0x00,0x00},
    {0x00,0x56,0x36,0x00,0x00}, {0x08,0x14,0x22,0x41,0x00}, {0x14,0x14,0x14,0x14,0x14},
    {0x00,0x41,0x22,0x14,0x08}, {0x02,0x01,0x51,0x09,0x06}, {0x32,0x49,0x79,0x41,0x3E},
    {0x7E,0x11,0x11,0x11,0x7E}, {0x7F,0x49,0x49,0x49,0x36}, {0x3E,0x41,0x41,0x41,0x22},
    {0x7F,0x41,0x41,0x22,0x1C}, {0x7F,0x49,0x49,0x49,0x41}, {0x7F,0x09,0x09,0x09,0x01},
    {0x3E,0x41,0x49,0x49,0x7A}, {0x7F,0x08,0x08,0x08,0x7F}, {0x00,0x41,0x7F,0x41,0x00},
    {0x20,0x40,0x41,0x3F,0x01}, {0x7F,0x08,0x14,0x22,0x41}, {0x7F,0x40,0x40,0x40,0x40},
    {0x7F,0x02,0x0C,0x02,0x7F}, {0x7F,0x04,0x08,0x10,0x7F}, {0x3E,0x41,0x41,0x41,0x3E},
    {0x7F,0x09,0x09,0x09,0x06}, {0x3E,0x41,0x51,0x21,0x5E}, {0x7F,0x09,0x19,0x29,0x46},
    {0x46,0x49,0x49,0x49,0x31}, {0x01,0x01,0x7F,0x01,0x01}, {0x3F,0x40,0x40,0x40,0x3F},
    {0x1F,0x20,0x40,0x20,0x1F}, {0x3F,0x40,0x38,0x40,0x3F}, {0x63,0x14,0x08,0x14,0x63},
    {0x07,0x08,0x70,0x08,0x07}, {0x61,0x51,0x49,0x45,0x43}, {0x00,0x7F,0x41,0x41,0x00},
    {0x02,0x04,0x08,0x10,0x20}, {0x00,0x41,0x41,0x7F,0x00}, {0x04,0x02,0x01,0x02,0x04},
    {0x40,0x40,0x40,0x40,0x40}, {0x00,0x01,0x02,0x04,0x00}, {0x20,0x54,0x54,0x54,0x78},
    {0x7F,0x48,0x44,0x44,0x38}, {0x38,0x44,0x44,0x44,0x20}, {0x38,0x44,0x44,0x48,0x7F},
    {0x38,0x54,0x54,0x54,0x18}, {0x08,0x7E,0x09,0x01,0x02}, {0x0C,0x52,0x52,0x52,0x3E},
    {0x7F,0x08,0x04,0x04,0x78}, {0x00,0x44,0x7D,0x40,0x00}, {0x20,0x40,0x44,0x3D,0x00},
    {0x7F,0x10,0x28,0x44,0x00}, {0x00,0x41,0x7F,0x40,0x00}, {0x7C,0x04,0x18,0x04,0x78},
    {0x7C,0x08,0x04,0x04,0x78}, {0x38,0x44,0x44,0x44,0x38}, {0x7C,0x14,0x14,0x14,0x08},
    {0x08,0x14,0x14,0x18,0x7C}, {0x7C,0x08,0x04,0x04,0x08}, {0x48,0x54,0x54,0x54,0x20},
    {0x04,0x3F,0x44,0x40,0x20}, {0x3C,0x40,0x40,0x20,0x7C}, {0x1C,0x20,0x40,0x20,0x1C},
    {0x3C,0x40,0x30,0x40,0x3C}, {0x44,0x28,0x10,0x28,0x44}, {0x0C,0x50,0x50,0x50,0x3C},
    {0x44,0x64,0x54,0x4C,0x44}, {0x00,0x08,0x36,0x41,0x00}, {0x00,0x00,0x7F,0x00,0x00},
    {0x00,0x41,0x36,0x08,0x00}, {0x10,0x08,0x08,0x10,0x08},
};

static uint16_t s_band_buf[410 * 502];

struct S3Canvas {
    int y0;
    int y1;

    inline void clear(uint16_t color) {
        int count = (int)g_lcd_w * (y1 - y0);
        for (int i = 0; i < count; i++) s_band_buf[i] = color;
    }
    inline void put_px(int x, int y, uint16_t color) {
        if ((unsigned)x >= g_lcd_w || y < y0 || y >= y1) return;
        s_band_buf[(y - y0) * g_lcd_w + x] = color;
    }
    inline void fill_rect(int x, int y, int w, int h, uint16_t color) {
        if (w <= 0 || h <= 0) return;
        int x_start = (x < 0) ? 0 : x;
        int x_end   = (x + w > (int)g_lcd_w) ? (int)g_lcd_w : (x + w);
        int y_start = (y < y0) ? y0 : y;
        int y_end   = (y + h > y1) ? y1 : (y + h);
        if (x_start >= x_end || y_start >= y_end) return;
        for (int py = y_start; py < y_end; py++) {
            uint16_t* row = &s_band_buf[(py - y0) * g_lcd_w + x_start];
            for (int i = 0; i < x_end - x_start; i++) row[i] = color;
        }
    }
    inline void draw_hline(int x, int y, int w, uint16_t color) {
        if (y < y0 || y >= y1 || w <= 0) return;
        int x_start = (x < 0) ? 0 : x;
        int x_end   = (x + w > (int)g_lcd_w) ? (int)g_lcd_w : (x + w);
        if (x_start >= x_end) return;
        uint16_t* row = &s_band_buf[(y - y0) * g_lcd_w + x_start];
        for (int i = 0; i < x_end - x_start; i++) row[i] = color;
    }
    inline void draw_vline(int x, int y, int h, uint16_t color) {
        if ((unsigned)x >= g_lcd_w || h <= 0) return;
        int y_start = (y < y0) ? y0 : y;
        int y_end   = (y + h > y1) ? y1 : (y + h);
        for (int py = y_start; py < y_end; py++) s_band_buf[(py - y0) * g_lcd_w + x] = color;
    }
    inline void draw_rect(int x, int y, int w, int h, uint16_t color) {
        draw_hline(x, y, w, color);
        draw_hline(x, y + h - 1, w, color);
        draw_vline(x, y, h, color);
        draw_vline(x + w - 1, y, h, color);
    }
    inline void fill_card(int x, int y, int w, int h, uint16_t fill_col, uint16_t border_col) {
        if (y + h <= y0 || y >= y1) return;
        fill_rect(x + 2, y, w - 4, 1, border_col);
        fill_rect(x + 1, y + 1, w - 2, 1, fill_col);
        fill_rect(x, y + 2, w, h - 4, fill_col);
        fill_rect(x + 1, y + h - 2, w - 2, 1, fill_col);
        fill_rect(x + 2, y + h - 1, w - 4, 1, border_col);
        draw_vline(x, y + 2, h - 4, border_col);
        draw_vline(x + w - 1, y + 2, h - 4, border_col);
        put_px(x + 1, y + 1, border_col);
        put_px(x + w - 2, y + 1, border_col);
        put_px(x + 1, y + h - 2, border_col);
        put_px(x + w - 2, y + h - 2, border_col);
    }
    inline void draw_line(int x0, int ly0, int x1, int ly1, uint16_t color, int thick = 1) {
        int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
        int dy = -abs(ly1 - ly0), sy = ly0 < ly1 ? 1 : -1;
        int err = dx + dy;
        while (true) {
            if (thick <= 1) put_px(x0, ly0, color);
            else fill_rect(x0 - thick / 2, ly0 - thick / 2, thick, thick, color);
            if (x0 == x1 && ly0 == ly1) break;
            int e2 = 2 * err;
            if (e2 >= dy) { err += dy; x0 += sx; }
            if (e2 <= dx) { err += dx; ly0 += sy; }
        }
    }
    inline void fill_circle(int cx, int cy, int r, uint16_t color) {
        for (int dy = -r; dy <= r; dy++) {
            int py = cy + dy;
            if (py < y0 || py >= y1) continue;
            int dx = (int)sqrtf((float)(r * r - dy * dy));
            draw_hline(cx - dx, py, 2 * dx + 1, color);
        }
    }
    inline void draw_circle(int cx, int cy, int r, uint16_t color) {
        int x = r, y = 0, err = 0;
        while (x >= y) {
            put_px(cx + x, cy + y, color); put_px(cx + y, cy + x, color);
            put_px(cx - y, cy + x, color); put_px(cx - x, cy + y, color);
            put_px(cx - x, cy - y, color); put_px(cx - y, cy - x, color);
            put_px(cx + y, cy - x, color); put_px(cx + x, cy - y, color);
            y++;
            if (err <= 0) err += 2 * y + 1;
            if (err > 0) { x--; err -= 2 * x + 1; }
        }
    }
    inline void draw_char(int x, int y, char c, uint16_t color, int scale = 1) {
        int char_h = 7 * scale;
        if (y + char_h <= y0 || y >= y1) return;
        if (c < 0x20 || c > 0x7E) c = '?';
        const uint8_t* glyph = FONT5X7[c - 0x20];
        for (int col = 0; col < 5; col++) {
            uint8_t bits = glyph[col];
            if (!bits) continue;
            for (int row = 0; row < 7; row++) {
                if (bits & (1 << row)) {
                    if (scale == 1) put_px(x + col, y + row, color);
                    else fill_rect(x + col * scale, y + row * scale, scale, scale, color);
                }
            }
        }
    }
    inline void draw_text_clipped(int x, int y, int max_w, const char* str, uint16_t color, int scale = 1) {
        if (!str || max_w <= 0 || y + 7 * scale <= y0 || y >= y1) return;
        int cx = x;
        int adv = 6 * scale;
        int limit_x = x + max_w;
        if (limit_x > (int)g_lcd_w - 4) limit_x = (int)g_lcd_w - 4;
        while (*str) {
            if (cx + 5 * scale > limit_x) break;
            draw_char(cx, y, *str++, color, scale);
            cx += adv;
        }
    }
    inline void draw_text_fit(int x, int y, int max_w, const char* str, uint16_t color, int pref_scale = 2) {
        if (!str || max_w <= 0) return;
        int len = (int)strlen(str);
        int scale = pref_scale;
        if (scale > 1 && (len * 6 * scale - scale) > max_w) scale = 1;
        draw_text_clipped(x, y, max_w, str, color, scale);
    }
    inline void draw_text(int x, int y, const char* str, uint16_t color, int scale = 1) {
        draw_text_clipped(x, y, (int)g_lcd_w - x - 4, str, color, scale);
    }
    inline void draw_text_centered(int cx, int y, const char* str, uint16_t color, int scale = 1) {
        if (!str) return;
        int len = (int)strlen(str);
        int max_chars = ((int)g_lcd_w - 16) / (6 * scale);
        if (len > max_chars && max_chars > 0) len = max_chars;
        int w = len * 6 * scale - scale;
        int start_x = cx - w / 2;
        if (start_x < 4) start_x = 4;
        draw_text_clipped(start_x, y, len * 6 * scale, str, color, scale);
    }
};

static uint16_t s3_trit_color(int8_t t) {
    if (t > 0) return COL_MINT;
    if (t < 0) return COL_CORAL;
    return COL_GOLD;
}
static char s3_trit_char(int8_t t) {
    if (t > 0) return '+';
    if (t < 0) return '-';
    return '.';
}

static void render_scene(S3Canvas& c, float phase) {
    int W = (int)g_lcd_w;
    int H = (int)g_lcd_h;
    int hdr_h = (H < 300) ? 32 : 40;
    int ftr_h = (H < 300) ? 36 : 52;
    int hud_h = (H < 300) ? 32 : 36;
    int hud_y = H - ftr_h - hud_h - 4;

    // 1. Header
    c.fill_rect(0, 0, W, hdr_h, COL_BG_ELEV);
    c.draw_hline(0, hdr_h, W, COL_BORDER);
    uint16_t pcol = s3_trit_color(g_triad_self_pole);
    int cy_dot = hdr_h / 2;
    c.fill_circle(14, cy_dot, (hdr_h < 40) ? 5 : 6, pcol);
    c.draw_circle(14, cy_dot, (hdr_h < 40) ? 7 : 9, COL_CYAN);

    int bat_w = (W < 360) ? 58 : 68;
    int qt_w  = (W < 360) ? 58 : 68;
    int bat_x = W - bat_w - 6;
    int qt_x  = bat_x - qt_w - 5;
    int title_max_w = qt_x - 32;

    const char* hdr_title = (H < 300)               ? "[ 0] TDECK:AYEOS"
                          : ((g_triad_self_id == 0) ? "[-1] ADA:MEDUSA"
                          : ((g_triad_self_id == 1) ? "[ 0] VAK:MEDUSA"
                                                    : "[+1] OMN:MEDUSA"));
    c.draw_text_fit(28, (hdr_h < 40) ? 5 : 7, title_max_w, hdr_title, COL_TEXT, 2);

    char sub_hdr[48];
    snprintf(sub_hdr, sizeof(sub_hdr), "%ux%u %s [3/3]", (unsigned)W, (unsigned)H, g_lcd_driver_name);
    c.draw_text_clipped(28, (hdr_h < 40) ? 19 : 25, title_max_w, sub_hdr, pcol, 1);

    int card_h = hdr_h - 10;
    int card_y = 5;
    c.fill_card(qt_x, card_y, qt_w, card_h, COL_VIOLET_DARK, COL_VIOLET);
    c.draw_text_centered(qt_x + qt_w / 2, card_y + (card_h - 7) / 2, "qt:26 +.+", COL_VIOLET, 1);

    c.fill_card(bat_x, card_y, bat_w, card_h, COL_BG_PANEL, COL_MINT);
    if (hdr_h < 40) {
        c.draw_text_centered(bat_x + bat_w / 2, card_y + (card_h - 7) / 2, "98% USB", COL_MINT, 1);
    } else {
        c.draw_text_centered(bat_x + bat_w / 2, card_y + 5, "98% USB", COL_MINT, 1);
        c.draw_text_centered(bat_x + bat_w / 2, card_y + 16, "4160mV", COL_SUBTEXT, 1);
    }

    // 2. Top status pill
    int top_pill_y = hdr_h + 4;
    char top_lbl[56];
    snprintf(top_lbl, sizeof(top_lbl), "FLUG-OS v2.2.0 // %s (%ux%u)", g_medusa_node, (unsigned)W, (unsigned)H);
    c.draw_text_centered(W / 2, top_pill_y, top_lbl, COL_MINT, 1);

    // 3. Responsive 2D Wave Pool Sphere + 7-stage NAND Ring + Triad Lemniscate
    int avail_top = hdr_h + 14;
    int avail_bot = hud_y - 4;
    int cx = W / 2;
    int cy = (avail_top + avail_bot) / 2;
    int max_r_w = (W - 28) / 2;
    int max_r_h = (avail_bot - avail_top) / 2 - 6;
    int outer_r = (max_r_w < max_r_h) ? max_r_w : max_r_h;
    if (outer_r > 154) outer_r = 154;
    if (outer_r < 44) outer_r = 44;
    int inner_r = outer_r - ((outer_r < 80) ? 9 : 14);
    int outer_r2 = outer_r * outer_r;
    int inner_r2 = inner_r * inner_r;
    const int ring_span = outer_r2 - inner_r2;
    const int ring_lo   = ring_span / 5;
    const int ring_hi   = (ring_span * 4) / 5;

    for (int py = avail_top; py < avail_bot; py++) {
        int dy = py - cy;
        int dy2 = dy * dy;
        if (dy2 > outer_r2) continue;
        uint16_t* row_ptr = &s_band_buf[py * W];
        for (int px = 8; px < W - 8; px++) {
            int dx = px - cx;
            int r2 = dx * dx + dy2;
            if (r2 > outer_r2) continue;
            if (r2 >= inner_r2) {
                int rb = r2 - inner_r2;
                row_ptr[px] = (rb < ring_lo || rb > ring_hi) ? COL_AMBER_INFO : COL_BG_ELEV;
            } else {
                float dist = sqrtf((float)r2);
                float wave = sinf(dist * 0.22f - phase * 2.0f) + 0.5f * cosf((dx - dy) * 0.12f + phase);
                int rim = (r2 * 48) / inner_r2;
                int r = 6 + ((rim * 18) >> 6);
                int g = 10 + ((rim * 36) >> 6);
                int b = 22 + ((rim * 54) >> 6);
                if (wave > 0.15f) {
                    int mag = (int)(wave * 95.0f);
                    r = min(255, r + (mag >> 2));
                    g = min(255, g + mag);
                    b = min(255, b + ((mag * 3) >> 2));
                } else if (wave < -0.15f) {
                    int mag = (int)(-wave * 95.0f);
                    r = min(255, r + ((mag * 3) >> 2));
                    g = min(255, g + (mag >> 2));
                    b = min(255, b + mag);
                }
                if (((px - cx) & 31) == 0 || ((py - cy) & 31) == 0) {
                    g = min(255, g + 10);
                    b = min(255, b + 16);
                }
                row_ptr[px] = rgb565_be((uint8_t)r, (uint8_t)g, (uint8_t)b);
            }
        }
    }

    // Orbiting 7-stage Ternary NAND Nodes
    int mid_r = (outer_r + inner_r) / 2;
    const int8_t nand_trits[7] = {1, -1, 0, 1, -1, 1, 0};
    for (int i = 0; i < 7; i++) {
        float ang = phase * 0.5f + (float)i * (2.0f * 3.14159265f / 7.0f);
        int nx = cx + (int)(cosf(ang) * (float)mid_r);
        int ny = cy + (int)(sinf(ang) * (float)mid_r);
        c.fill_circle(nx, ny, (outer_r < 80) ? 3 : 4, s3_trit_color(nand_trits[i]));
        c.draw_circle(nx, ny, (outer_r < 80) ? 4 : 6, COL_TEXT);
    }

    // 3-Body Triad Entanglement Triangle
    int tx[3], ty[3];
    uint16_t tcol[3] = {COL_CORAL, COL_GOLD, COL_CYAN};
    for (int n = 0; n < 3; n++) {
        float base_ang = phase * 0.3f + (float)n * 2.0943951f - 1.570796f;
        float rad = (float)inner_r * 0.52f;
        tx[n] = cx + (int)(cosf(base_ang) * rad);
        ty[n] = cy + (int)(sinf(base_ang) * rad);
    }
    c.draw_line(tx[0], ty[0], tx[1], ty[1], COL_MINT, 1);
    c.draw_line(tx[1], ty[1], tx[2], ty[2], COL_GOLD, 1);
    c.draw_line(tx[2], ty[2], tx[0], ty[0], COL_CYAN, 1);
    for (int n = 0; n < 3; n++) {
        c.fill_circle(tx[n], ty[n], (n == g_triad_self_id) ? 6 : 4, tcol[n]);
        c.draw_circle(tx[n], ty[n], (n == g_triad_self_id) ? 10 : 7, (n == g_triad_self_id) ? COL_MINT : COL_TEXT);
    }

    // Central FindMy Compass Ring + Vector Arrow when Card 0 is active, or Lemniscate + Feature Badge
    int ring_r = (outer_r < 80) ? 20 : 34;
    c.draw_circle(cx, cy, ring_r, COL_MINT);
    c.draw_circle(cx, cy, ring_r - 4, COL_CYAN);
    omni_draw_findme_arrow(c, cx, cy, ring_r - 7, g_findme.bearing_deg, COL_GOLD);

    // 4. Bottom OMNI v2.2.0 Simple Card Overlay (with FindMy Arrow + Clipped Text + Kanji 8x8)
    c.fill_card(8, hud_y, W - 16, hud_h, COL_BG_ELEV, COL_MINT);
    uint8_t oct = omni_bearing_octant(g_findme.bearing_deg);
    static const uint8_t card_kanji_map[5] = {0, 4, 6, 5, 7};
    omni_draw_findme_arrow(c, 25, hud_y + hud_h / 2, (hud_h < 36) ? 9 : 11, g_findme.bearing_deg, COL_MINT);
    omni_draw_kanji_8x8(c, W - 28, hud_y + (hud_h - 16) / 2, card_kanji_map[g_omni_simple_card % 5], COL_GOLD, 2);

    char line1[64], line2[64];
    if (g_omni_simple_card == 0) {
        snprintf(line1, sizeof(line1), "FINDME %s %s  %ld/%ldm",
                 OMNI_ARROW_ASCII[oct], g_findme.target_id,
                 (long)g_findme.dist_m_p, (long)g_findme.dist_m_q);
        snprintf(line2, sizeof(line2), "%s | %ddBm | %uBPM [%s]",
                 OMNI_PROX_EN[g_findme.proximity_zone], (int)g_findme.rssi_dbm,
                 (unsigned)g_findme.pulse_bpm, omni_lang_name(g_omni_lang));
    } else if (g_omni_simple_card == 1) {
        const OmniMetroStation* sf = &OMNI_TOKYO_STATIONS[g_metro.from_idx & 7];
        const OmniMetroStation* st = &OMNI_TOKYO_STATIONS[g_metro.to_idx & 7];
        const OmniRfidTagRecord* suica = omni_rfid_current_suica();
        snprintf(line1, sizeof(line1), "METRO %s(%s)->%s(%s) %um",
                 sf->code, sf->name_en, st->code, st->name_en, (unsigned)g_metro.eta_min);
        snprintf(line2, sizeof(line2), "PARENTS: %s | Suica %luY | Bond %ld/%ld",
                 GUARDIAN_ALERT_NAMES[g_guardian_alert & 3],
                 (unsigned long)suica->balance_yen,
                 (long)g_guardian_soul.bond_p, (long)g_guardian_soul.bond_q);
    } else if (g_omni_simple_card == 2) {
        const OmniLllCard* card = &OMNI_LLL_DECK[g_pomo_lll.card_idx % 6];
        const char* ph_str = (g_pomo_lll.phase == 0) ? "25M" : ((g_pomo_lll.phase == 1) ? "5M" : "15M");
        snprintf(line1, sizeof(line1), "POMO %02lu:%02lu [%s] #%u",
                 (unsigned long)(g_pomo_lll.remaining_s / 60),
                 (unsigned long)(g_pomo_lll.remaining_s % 60),
                 ph_str,
                 (unsigned)g_pomo_lll.completed_pomos);
        snprintf(line2, sizeof(line2), "LLL %s | Mastery %ld/%ld",
                 card->topic, (long)g_pomo_lll.mastery_p, (long)g_pomo_lll.mastery_q);
    } else if (g_omni_simple_card == 3) {
        const OmniRfidTagRecord* rf = &g_rfid_tags[g_rfid_active_idx % g_rfid_count];
        snprintf(line1, sizeof(line1), "AC:%s %luB Stars:%u* Turnip:%ld/%ld",
                 g_ac_game.island_name, (unsigned long)g_ac_game.bells,
                 (unsigned)g_ac_game.island_stars, (long)g_ac_game.turnip_p, (long)g_ac_game.turnip_q);
        snprintf(line2, sizeof(line2), "RFID:%s %s (%luY)",
                 OMNI_RFID_PROTO_NAMES[rf->proto % 6], rf->station_code, (unsigned long)rf->balance_yen);
    } else {
        snprintf(line1, sizeof(line1), "ZEN+WAVE %uHz | ANICCA 3/3",
                 (unsigned)g_medusa_freq_hz);
        snprintf(line2, sizeof(line2), "HOTSPOT:GALAXY_S25_ULTRA | <3+1");
    }
    int max_txt_w = W - 76;
    c.draw_text_clipped(42, hud_y + ((hud_h < 36) ? 5 : 6), max_txt_w, line1, COL_MINT, 1);
    c.draw_text_clipped(42, hud_y + ((hud_h < 36) ? 18 : 20), max_txt_w, line2, COL_GOLD, 1);

    // 5. Footer Tabs
    int fy = H - ftr_h;
    c.fill_rect(0, fy, W, ftr_h, COL_BG_ELEV);
    c.draw_hline(0, fy, W, COL_BORDER);
    const char* labels[6] = {"ZEN", "RADIO", "802.11", "NAND", "QWAVE", "VIZ"};
    const char* subs[6]   = {"anicca", "zoneInn", "rf+sinc", "imu+osc", "triad", "enthea"};
    uint16_t cols[6]      = {COL_MINT, COL_GOLD, COL_CYAN, COL_CORAL, COL_VIOLET, COL_GOLD};
    int gap = 4;
    int bw  = (W - gap * 7) / 6;
    int btn_h = ftr_h - 10;
    for (int i = 0; i < 6; i++) {
        int bx = gap + i * (bw + gap);
        bool active = (i == 0);
        c.fill_card(bx, fy + 5, bw, btn_h, active ? COL_BG_PANEL : COL_BG, active ? cols[i] : COL_BORDER);
        if (active) c.fill_rect(bx + 8, fy + 5, bw - 16, 3, cols[i]);
        if (ftr_h < 44) {
            c.draw_text_centered(bx + bw / 2, fy + 5 + (btn_h - 7) / 2, labels[i], active ? cols[i] : COL_TEXT, 1);
        } else {
            c.draw_text_centered(bx + bw / 2, fy + 14, labels[i], active ? cols[i] : COL_TEXT, 1);
            c.draw_text_centered(bx + bw / 2, fy + 29, subs[i], active ? COL_TEXT : COL_DIM, 1);
        }
    }
}

static void dump_ppm(const char* path) {
    FILE* f = fopen(path, "wb");
    if (!f) return;
    int W = (int)g_lcd_w;
    int H = (int)g_lcd_h;
    fprintf(f, "P6\n%d %d\n255\n", W, H);
    for (int i = 0; i < W * H; i++) {
        uint16_t be = s_band_buf[i];
        uint16_t v  = (uint16_t)((be >> 8) | (be << 8));
        uint8_t r = (uint8_t)(((v >> 11) & 0x1F) * 255 / 31);
        uint8_t g = (uint8_t)(((v >> 5)  & 0x3F) * 255 / 63);
        uint8_t b = (uint8_t)((v         & 0x1F) * 255 / 31);
        uint8_t rgb[3] = {r, g, b};
        fwrite(rgb, 1, 3, f);
    }
    fclose(f);
}

int main(int argc, char** argv) {
    if (argc < 5) return 1;
    const char* out_dir = argv[1];
    guardian_init();
    g_omni_lang = OMNI_LANG_JP;

    char path[512];

    // Shot 1: ADA:MEDUSA 1.8" 368x448 — Card 0: FINDME / AirTag Vector Arrow
    g_triad_self_id = 0; g_triad_self_pole = -1; g_medusa_node = "ADA:MEDUSA";
    omni_screen_set_res(368, 448, 0, "efuse_mac+i2c_tca9554");
    omni_findme_update("OMN:MEDUSA", 35, 145, -45);
    g_omni_simple_card = 0;
    {
        S3Canvas c = {0, (int)g_lcd_h};
        c.clear(COL_BG);
        render_scene(c, 0.8f);
        snprintf(path, sizeof(path), "%s/shot1.ppm", out_dir);
        dump_ppm(path);
    }

    // Shot 2: OMN:MEDUSA 2.06" 410x502 — Card 1: Tokyo Metro S-Line + Parents Safety Share
    g_triad_self_id = 2; g_triad_self_pole = 1; g_medusa_node = "OMN:MEDUSA";
    omni_screen_set_res(410, 502, 22, "efuse_mac+i2c_es7210");
    omni_metro_plan_route("Shinjuku", "Roppongi");
    omni_findme_update("PARENTS", -22, 80, -39);
    g_omni_simple_card = 1;
    {
        S3Canvas c = {0, (int)g_lcd_h};
        c.clear(COL_BG);
        render_scene(c, 1.6f);
        snprintf(path, sizeof(path), "%s/shot2.ppm", out_dir);
        dump_ppm(path);
    }

    // Shot 3: VAK:MEDUSA 2.06" 410x502 — Card 2: Pomodoro 25:00 + LLL Continual Learning
    g_triad_self_id = 1; g_triad_self_pole = 0; g_medusa_node = "VAK:MEDUSA";
    omni_screen_set_res(410, 502, 22, "efuse_mac+i2c_es7210");
    omni_pomo_switch_phase(POMO_PHASE_FOCUS_25M);
    g_pomo_lll.remaining_s = 1485; // 24:45
    g_pomo_lll.card_idx = 2; // ACOUSTICS-PQ 11/9
    omni_pomo_recompute_mastery();
    omni_findme_update("ADA:MEDUSA", 90, 240, -51);
    g_omni_simple_card = 2;
    {
        S3Canvas c = {0, (int)g_lcd_h};
        c.clear(COL_BG);
        render_scene(c, 2.4f);
        snprintf(path, sizeof(path), "%s/shot3.ppm", out_dir);
        dump_ppm(path);
    }

    // Shot 4: TDECK:AYEOS 2.8" 320x240 Landscape — Card 3: Animal Crossing x Olegsiv + Suica RFID
    g_triad_self_id = 1; g_triad_self_pole = 0; g_medusa_node = "TDECK:AYEOS";
    omni_screen_set_res(320, 240, 0, "efuse_mac_tdeck");
    omni_ac_action("HARVEST");
    omni_ac_action("FISH");
    omni_findme_update("OMN:MEDUSA", -45, 310, -54);
    g_omni_simple_card = 3;
    {
        S3Canvas c = {0, (int)g_lcd_h};
        c.clear(COL_BG);
        render_scene(c, 3.2f);
        snprintf(path, sizeof(path), "%s/shot4.ppm", out_dir);
        dump_ppm(path);
    }

    return 0;
}
"""


def parse_ppm(ppm_path: Path) -> tuple[int, int, bytes]:
    raw = ppm_path.read_bytes()
    lines = raw.split(b"\n", 3)
    assert lines[0] == b"P6"
    w, h = map(int, lines[1].decode().split())
    pixels = lines[3]
    assert len(pixels) == w * h * 3
    return w, h, pixels


def build_composite_showcase(shots: list[tuple[str, int, int, bytes]], out_path: Path) -> None:
    pad = 20
    total_w = sum(w for _, w, _, _ in shots) + pad * (len(shots) + 1)
    max_h = max(h for _, _, h, _ in shots) + pad * 2
    canvas = bytearray([10, 12, 18] * (total_w * max_h))

    cur_x = pad
    for _, w, h, rgb in shots:
        y_off = (max_h - h) // 2
        for y in range(h):
            dst_idx = ((y_off + y) * total_w + cur_x) * 3
            src_idx = y * w * 3
            canvas[dst_idx : dst_idx + w * 3] = rgb[src_idx : src_idx + w * 3]
        cur_x += w + pad

    write_png_rgb(out_path, total_w, max_h, bytes(canvas))


def main() -> None:
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory() as tmpdir:
        tmp = Path(tmpdir)
        cpp_file = tmp / "render.cpp"
        bin_file = tmp / "render_bin"
        cpp_file.write_text(CPP_RENDERER, encoding="utf-8")
        subprocess.run(
            [
                "g++",
                "-std=c++17",
                "-O2",
                f"-I{REPO_ROOT / 'src'}",
                str(cpp_file),
                "-o",
                str(bin_file),
            ],
            check=True,
        )
        subprocess.run([str(bin_file), str(tmp), "1", "2", "3"], check=True)

        targets = [
            ("shot1.ppm", "omni_v22_ada_18_368x448_findme.png"),
            ("shot2.ppm", "omni_v22_omn_206_410x502_metro_parents.png"),
            ("shot3.ppm", "omni_v22_vak_206_410x502_pomodoro_lll.png"),
            ("shot4.ppm", "omni_v22_tdeck_320x240_ac_rfid.png"),
        ]
        rendered = []
        for ppm_name, png_name in targets:
            w, h, rgb = parse_ppm(tmp / ppm_name)
            out_png = OUT_DIR / png_name
            write_png_rgb(out_png, w, h, rgb)
            rendered.append((png_name, w, h, rgb))
            print(f"Rendered {out_png.relative_to(REPO_ROOT)} ({w}x{h})")

        showcase_path = OUT_DIR / "omni_v22_multi_device_showcase.png"
        build_composite_showcase(rendered, showcase_path)
        print(f"Rendered {showcase_path.relative_to(REPO_ROOT)}")


if __name__ == "__main__":
    main()
