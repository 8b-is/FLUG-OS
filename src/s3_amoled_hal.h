/*
 * FLUG-OS — Waveshare ESP32-S3-Touch-AMOLED-1.8 & 2.06 Unified HAL
 * ULTRA-OMNI-ZEN Sovereign Constellation Hardware Target (vaked.dev × 8b-is)
 *
 * Runtime Auto-Detection via Shared I2C Bus (SDA=15, SCL=14 @ 400kHz):
 *   1. Waveshare ESP32-S3-Touch-AMOLED-1.8 (detected via TCA9554 @ 0x20):
 *      - SH8601 QSPI AMOLED: 368x448 (x_gap = 0, y_gap = 0)
 *      - LCD_CS=12, LCD_PCLK=11, LCD_D0=4, LCD_D1=5, LCD_D2=6, LCD_D3=7
 *      - TCA9554 I/O Expander @ 0x20 (pins 0..2 reset Display, Touch, Audio; EXIO7=SD CS)
 *      - FT3168 Touch @ 0x38 (TP_INT = GPIO 21)
 *      - ES8311 Audio Codec @ 0x18 (MCLK=16, BCLK=9, LRCLK=45, DOUT=8, DIN=10, PA=46)
 *      - QMI8658 6-Axis IMU @ 0x6B, AXP2101 PMIC @ 0x34, PCF85063 RTC @ 0x51
 *
 *   2. Waveshare ESP32-S3-Touch-AMOLED-2.06 (no TCA9554 @ 0x20):
 *      - SH8601 / CO5300 QSPI AMOLED: 410x502 (x_gap = 0x16 = 22)
 *      - LCD_CS=12, LCD_PCLK=11, LCD_D0..D3=4..7, LCD_RST=8, LCD_TE=13
 *      - FT3168 Touch @ 0x38 (TP_RST = GPIO 9, TP_INT = GPIO 38)
 *      - QMI8658 6-Axis IMU @ 0x6B, AXP2101 PMIC @ 0x34, PCF85063 RTC @ 0x51, ES8311 @ 0x18
 */

#ifndef FLUGOS_S3_AMOLED_HAL_H
#define FLUGOS_S3_AMOLED_HAL_H

#include <Arduino.h>
#include <Wire.h>
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "driver/i2s.h"
#include "esp_heap_caps.h"

// ============================================================
// Maximum / Dynamic Display Geometry & Pin Constants
// ============================================================
#define AMOLED_MAX_W        410
#define AMOLED_MAX_H        502
#define AMOLED_BAND_H       28     // Even band height (448 = 16*28; 502 = 17*28 + 26)
#define QSPI_CHUNK_PIXELS   1024   // Matches baye-rust 1024-pixel QSPI DMA chunk size

// Runtime-detected display geometry (defaults to 1.8" 368x448, updated at boot)
static bool     g_board_is_18 = true;
static uint16_t g_lcd_w       = 368;
static uint16_t g_lcd_h       = 448;
static uint16_t g_lcd_x_gap   = 0;

#define PIN_LCD_CS          12
#define PIN_LCD_PCLK        11
#define PIN_LCD_D0          4
#define PIN_LCD_D1          5
#define PIN_LCD_D2          6
#define PIN_LCD_D3          7
#define PIN_LCD_RST_206     8
#define PIN_LCD_TE          13

#define PIN_I2C_SCL         14
#define PIN_I2C_SDA         15
#define PIN_TP_RST_206      9
#define PIN_TP_INT_18       21
#define PIN_TP_INT_206      38
#define PIN_SD_CS_206       17
#define PIN_SPEAKER_EN      46
#define PIN_BOOT_BTN        0

// I2S Pinout: 1.8" uses BCLK=9/DOUT=8/DIN=10; 2.06" uses BCLK=41/DOUT=40/DIN=42 (GPIO 8/9 are LCD_RST/TP_RST!)
#define PIN_I2S_MCLK        16
#define PIN_I2S_LRCLK       45
#define PIN_I2S_BCLK_18     9
#define PIN_I2S_DOUT_18     8
#define PIN_I2S_DIN_18      10
#define PIN_I2S_BCLK_206    41
#define PIN_I2S_DOUT_206    40
#define PIN_I2S_DIN_206     42

#define I2C_ADDR_TCA9554    0x20
#define I2C_ADDR_AXP2101    0x34
#define I2C_ADDR_FT3168     0x38
#define I2C_ADDR_ES7210     0x40
#define I2C_ADDR_QMI8658    0x6B
#define I2C_ADDR_PCF85063   0x51
#define I2C_ADDR_ES8311     0x18

// ============================================================
// Constellation Ink Palette (RGB565 Big-Endian for SH8601 DMA)
// ============================================================
static inline uint16_t rgb565_be(uint8_t r, uint8_t g, uint8_t b) {
    uint16_t v = ((uint16_t)(r & 0xF8) << 8) |
                 ((uint16_t)(g & 0xFC) << 3) |
                 ((uint16_t)(b >> 3));
    return (uint16_t)((v >> 8) | (v << 8));
}

#define COL_BG          rgb565_be(5,   6,  10)   // #05060a Obsidian Ink
#define COL_VOID        rgb565_be(0,   0,   0)   // #000000 True AMOLED Black
#define COL_BG_ELEV     rgb565_be(11, 14,  23)   // #0b0e17 Elevated Card
#define COL_BG_PANEL    rgb565_be(17, 22,  36)   // #111624 Highlight Panel
#define COL_BORDER      rgb565_be(32, 42,  68)   // #202a44 Hairline Border
#define COL_GRID        rgb565_be(20, 26,  42)   // #141a2a Subtle Grid
#define COL_TEXT        rgb565_be(236, 242, 255) // #ecf2ff Crisp White
#define COL_SUBTEXT     rgb565_be(138, 152, 184) // #8a98b8 Muted Slate
#define COL_DIM         rgb565_be(76,  88, 118)  // #4c5876 Dim Label
#define COL_AMBER_INFO  rgb565_be(140, 102, 38)  // #8c6626 Orbiting Info Ring Amber
#define COL_MINT        rgb565_be(98,  230, 201) // #62e6c9 Sovereign Mint (+1)
#define COL_MINT_DARK   rgb565_be(22,  68,  60)  // #16443c Mint Fill
#define COL_GOLD        rgb565_be(232, 176, 92)  // #e8b05c Harmonic Gold (0)
#define COL_GOLD_DARK   rgb565_be(72,  52,  22)  // #483416 Gold Fill
#define COL_VIOLET      rgb565_be(180, 139, 255) // #b48bff QuantTern Violet
#define COL_VIOLET_DARK rgb565_be(48,  34,  82)  // #302252 Violet Fill
#define COL_CYAN        rgb565_be(0,   240, 255) // #00f0ff Spectral Cyan
#define COL_CORAL       rgb565_be(255, 92,  119) // #ff5c77 Subcritical Coral (-1)
#define COL_CORAL_DARK  rgb565_be(76,  24,  36)  // #4c1824 Coral Fill

// ============================================================
// I2C Helper Routines
// ============================================================
static bool s3_i2c_probe(uint8_t addr) {
    Wire.beginTransmission(addr);
    return (Wire.endTransmission() == 0);
}

static bool s3_i2c_write8(uint8_t addr, uint8_t reg, uint8_t val) {
    Wire.beginTransmission(addr);
    Wire.write(reg);
    Wire.write(val);
    return (Wire.endTransmission() == 0);
}

static bool s3_i2c_read_bytes(uint8_t addr, uint8_t reg, uint8_t* buf, size_t len) {
    Wire.beginTransmission(addr);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) return false;
    size_t got = Wire.requestFrom((int)addr, (int)len);
    if (got != len) return false;
    for (size_t i = 0; i < len; i++) {
        buf[i] = Wire.read();
    }
    return true;
}

static uint8_t s3_i2c_read8(uint8_t addr, uint8_t reg, uint8_t fallback = 0) {
    uint8_t v = fallback;
    s3_i2c_read_bytes(addr, reg, &v, 1);
    return v;
}

// ============================================================
// TCA9554 I/O Expander (0x20) vs 2.06" Direct Reset (GPIO 8 + GPIO 9)
// 1.8" toggles TCA9554 pins 0..2 LOW -> HIGH to reset SH8601, FT3168, ES8311.
// 2.06" (detected via FLUGOS_BOARD_206, ES7210 @ 0x40, or absent TCA9554 @ 0x20)
// toggles GPIO 8 (LCD_RST) + GPIO 9 (TP_RST) LOW 20ms -> HIGH 150ms (baye-rust parity).
// ============================================================
static const char* g_lcd_driver_name = "SH8601";
static bool        g_es7210_ok       = false;

static bool s3_tca9554_init_and_reset() {
#if defined(FLUGOS_BOARD_206)
    bool force_206 = true;
#else
    bool force_206 = false;
#endif
    bool has_es7210  = s3_i2c_probe(I2C_ADDR_ES7210);
    bool has_tca9554 = s3_i2c_probe(I2C_ADDR_TCA9554);

    if (force_206 || has_es7210 || !has_tca9554) {
        g_board_is_18     = false;
        g_lcd_w           = 410;
        g_lcd_h           = 502;
        g_lcd_x_gap       = 22; // 0x16 column offset for 410x502 CO5300 panel
        g_lcd_driver_name = "CO5300";
        // Waveshare 2.06" dedicated LCD_RST (GPIO 8) and TP_RST (GPIO 9) reset sequence (baye-rust tca9554.rs)
        gpio_set_direction((gpio_num_t)PIN_LCD_RST_206, GPIO_MODE_OUTPUT);
        gpio_set_direction((gpio_num_t)PIN_TP_RST_206, GPIO_MODE_OUTPUT);
        gpio_set_level((gpio_num_t)PIN_LCD_RST_206, 0);
        gpio_set_level((gpio_num_t)PIN_TP_RST_206, 0);
        delay(20);
        gpio_set_level((gpio_num_t)PIN_LCD_RST_206, 1);
        gpio_set_level((gpio_num_t)PIN_TP_RST_206, 1);
        delay(150);
        return false;
    }
    g_board_is_18     = true;
    g_lcd_w           = 368;
    g_lcd_h           = 448;
    g_lcd_x_gap       = 0;
    g_lcd_driver_name = "SH8601";
    // Set pins 0..2 as outputs (0 = output, 1 = input -> 0xF8 = 0b11111000)
    if (!s3_i2c_write8(I2C_ADDR_TCA9554, 0x03, 0xF8)) return false;
    // Assert reset (drive pins 0..2 LOW)
    s3_i2c_write8(I2C_ADDR_TCA9554, 0x01, 0x00);
    delay(20);
    // Release reset (drive pins 0..2 HIGH -> 0x07 = 0b00000111)
    s3_i2c_write8(I2C_ADDR_TCA9554, 0x01, 0x07);
    delay(50);
    return true;
}

// ============================================================
// ES7210 Dual-Mic ADC (0x40) — Present on ESP32-S3-Touch-AMOLED-2.06
// Exact register sequence & readback check from oxi-phoenix/baye-rust/src/drivers/es7210.rs
// ============================================================
static bool s3_es7210_init() {
    if (g_board_is_18 || !s3_i2c_probe(I2C_ADDR_ES7210)) {
        g_es7210_ok = false;
        return false;
    }
    s3_i2c_write8(I2C_ADDR_ES7210, 0x00, 0xFF);
    delay(10);
    static const uint8_t seq[][2] = {
        {0x00, 0x41}, {0x01, 0x3F}, {0x09, 0x30}, {0x0A, 0x30},
        {0x23, 0x2A}, {0x22, 0x0A}, {0x20, 0x0A}, {0x21, 0x2A},
        {0x08, 0x00}, {0x40, 0x43}, {0x41, 0x70}, {0x42, 0x70},
        {0x07, 0x20}, {0x02, 0xC1}, {0x11, 0x60}, {0x12, 0x00},
        {0x01, 0x34}, {0x06, 0x00}, {0x47, 0x08}, {0x48, 0x08},
        {0x49, 0xFF}, {0x4A, 0xFF}, {0x4B, 0x00}, {0x4C, 0xFF},
        {0x43, 0x16}, {0x44, 0x16}, {0x45, 0x00}, {0x46, 0x00},
        {0x40, 0x43}, {0x00, 0x71}, {0x00, 0x41}
    };
    for (size_t i = 0; i < sizeof(seq) / sizeof(seq[0]); i++) {
        s3_i2c_write8(I2C_ADDR_ES7210, seq[i][0], seq[i][1]);
    }
    delay(50);
    g_es7210_ok = (s3_i2c_read8(I2C_ADDR_ES7210, 0x11, 0) == 0x60) &&
                  (s3_i2c_read8(I2C_ADDR_ES7210, 0x43, 0) == 0x16);
    return g_es7210_ok;
}

// ============================================================
// ES8311 Audio Codec (0x18) — Full ADF Init Sequence (from baye-rust)
// ============================================================
static bool g_es8311_ok = false;

static bool s3_es8311_init() {
    if (!s3_i2c_probe(I2C_ADDR_ES8311)) {
        g_es8311_ok = false;
        return false;
    }
    s3_i2c_write8(I2C_ADDR_ES8311, 0x00, 0x1F);
    delay(20);
    s3_i2c_write8(I2C_ADDR_ES8311, 0x00, 0x00);
    delay(20);

    s3_i2c_write8(I2C_ADDR_ES8311, 0x44, 0x08);
    s3_i2c_write8(I2C_ADDR_ES8311, 0x44, 0x08);

    s3_i2c_write8(I2C_ADDR_ES8311, 0x01, 0x30);
    s3_i2c_write8(I2C_ADDR_ES8311, 0x02, 0x00);
    s3_i2c_write8(I2C_ADDR_ES8311, 0x03, 0x10);
    s3_i2c_write8(I2C_ADDR_ES8311, 0x04, 0x10);
    s3_i2c_write8(I2C_ADDR_ES8311, 0x05, 0x04);
    s3_i2c_write8(I2C_ADDR_ES8311, 0x06, 0x01);
    s3_i2c_write8(I2C_ADDR_ES8311, 0x07, 0x00);
    s3_i2c_write8(I2C_ADDR_ES8311, 0x08, 0xFF);

    s3_i2c_write8(I2C_ADDR_ES8311, 0x09, 0x00);
    s3_i2c_write8(I2C_ADDR_ES8311, 0x0A, 0x00);

    s3_i2c_write8(I2C_ADDR_ES8311, 0x0B, 0x00);
    s3_i2c_write8(I2C_ADDR_ES8311, 0x0C, 0x00);
    s3_i2c_write8(I2C_ADDR_ES8311, 0x10, 0x1F);
    s3_i2c_write8(I2C_ADDR_ES8311, 0x11, 0x7F);
    delay(50);

    s3_i2c_write8(I2C_ADDR_ES8311, 0x17, 0xBF);
    s3_i2c_write8(I2C_ADDR_ES8311, 0x0E, 0x02);
    s3_i2c_write8(I2C_ADDR_ES8311, 0x12, 0x00);
    s3_i2c_write8(I2C_ADDR_ES8311, 0x14, 0xBF);
    s3_i2c_write8(I2C_ADDR_ES8311, 0x0D, 0x01);
    s3_i2c_write8(I2C_ADDR_ES8311, 0x15, 0x40);
    s3_i2c_write8(I2C_ADDR_ES8311, 0x1B, 0x05);
    s3_i2c_write8(I2C_ADDR_ES8311, 0x1C, 0x6A);
    s3_i2c_write8(I2C_ADDR_ES8311, 0x37, 0x08);
    s3_i2c_write8(I2C_ADDR_ES8311, 0x45, 0x00);
    s3_i2c_write8(I2C_ADDR_ES8311, 0x44, 0x58);
    delay(30);
    s3_i2c_write8(I2C_ADDR_ES8311, 0x32, 0xD9); // 85% DAC volume

    pinMode(PIN_SPEAKER_EN, OUTPUT);
    digitalWrite(PIN_SPEAKER_EN, HIGH);
    g_es8311_ok = true;
    return true;
}

// ============================================================
// ES8311 On-Device Formant + {<3,<3,<3}+1 Harmonic Voice TTS
// Safe across both 1.8" (I2S MCLK=16,BCLK=9,WS=45,DOUT=8) and 2.06"
// ============================================================
static bool     g_i2s_tts_ready = false;
static uint32_t g_tts_utterances = 0;
static char     g_tts_last_text[64] = "{<3,<3,<3}+1 MEDUSA TRIAD ONLINE";

static void s3_es8311_init_i2s() {
    if (g_i2s_tts_ready) return;
    i2s_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.mode                 = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX);
    cfg.sample_rate          = 16000;
    cfg.bits_per_sample      = I2S_BITS_PER_SAMPLE_16BIT;
    cfg.channel_format       = I2S_CHANNEL_FMT_RIGHT_LEFT;
    cfg.communication_format = I2S_COMM_FORMAT_STAND_I2S;
    cfg.intr_alloc_flags     = ESP_INTR_FLAG_LEVEL1;
    cfg.dma_buf_count        = 4;
    cfg.dma_buf_len          = 256;
    cfg.use_apll             = false;
    cfg.tx_desc_auto_clear   = true;

    if (i2s_driver_install(I2S_NUM_0, &cfg, 0, NULL) == ESP_OK) {
        i2s_pin_config_t pins;
        memset(&pins, 0, sizeof(pins));
        pins.mck_io_num   = PIN_I2S_MCLK;
        pins.bck_io_num   = g_board_is_18 ? PIN_I2S_BCLK_18 : PIN_I2S_BCLK_206;
        pins.ws_io_num    = PIN_I2S_LRCLK;
        pins.data_out_num = g_board_is_18 ? PIN_I2S_DOUT_18 : PIN_I2S_DOUT_206;
        pins.data_in_num  = I2S_PIN_NO_CHANGE;
        if (i2s_set_pin(I2S_NUM_0, &pins) == ESP_OK) {
            g_i2s_tts_ready = true;
        }
    }
}

static void s3_es8311_speak(const char* text, uint16_t carrier_hz, int8_t trit) {
    if (!text || !text[0]) return;
    strncpy(g_tts_last_text, text, sizeof(g_tts_last_text) - 1);
    g_tts_last_text[sizeof(g_tts_last_text) - 1] = '\0';
    g_tts_utterances++;

    if (!g_es8311_ok) return;
    digitalWrite(PIN_SPEAKER_EN, HIGH);

    s3_es8311_init_i2s();
    if (g_i2s_tts_ready) {
        int16_t stereo_buf[256]; // 128 stereo frames (8ms @ 16kHz)
        float phase0 = 0.0f, phase1 = 0.0f, phase2 = 0.0f;
        // Pre-chime: {<3,<3,<3}+1 triad chord (432 / 528 / 852 / 963 Hz)
        const float chord[4] = {432.0f, 528.0f, 852.0f, 963.0f};
        for (int n = 0; n < 4; n++) {
            float f = chord[n];
            for (int blk = 0; blk < 5; blk++) {
                for (int i = 0; i < 128; i++) {
                    phase0 += (6.2831853f * f) / 16000.0f;
                    if (phase0 > 6.2831853f) phase0 -= 6.2831853f;
                    int16_t s = (int16_t)(sinf(phase0) * 5200.0f);
                    stereo_buf[i * 2]     = s;
                    stereo_buf[i * 2 + 1] = s;
                }
                size_t written = 0;
                i2s_write(I2S_NUM_0, stereo_buf, sizeof(stereo_buf), &written, 20);
            }
        }
        // Formant synthesis over up to 48 characters
        float f0 = (float)((carrier_hz >= 200 && carrier_hz <= 1200) ? carrier_hz : 432) * 0.35f;
        if (trit > 0) f0 *= 1.12f;
        else if (trit < 0) f0 *= 0.92f;
        size_t len = strlen(text);
        if (len > 48) len = 48;
        for (size_t idx = 0; idx < len; idx++) {
            char c = text[idx];
            if (c >= 'A' && c <= 'Z') c = (char)(c + 32);
            float f1 = 500.0f, f2 = 1500.0f, amp = 4800.0f;
            if (c == 'a')      { f1 = 730.0f; f2 = 1090.0f; }
            else if (c == 'e') { f1 = 530.0f; f2 = 1840.0f; }
            else if (c == 'i') { f1 = 270.0f; f2 = 2290.0f; }
            else if (c == 'o') { f1 = 570.0f; f2 = 840.0f;  }
            else if (c == 'u') { f1 = 300.0f; f2 = 870.0f;  }
            else if (c == ' ' || c == '.' || c == ',') { amp = 0.0f; }
            else {
                f1 = 420.0f + (float)((uint8_t)c & 0x0F) * 24.0f;
                f2 = 1320.0f + (float)(((uint8_t)c >> 2) & 0x0F) * 45.0f;
                amp = 3200.0f;
            }
            for (int blk = 0; blk < 3; blk++) {
                for (int i = 0; i < 128; i++) {
                    phase0 += (6.2831853f * f0) / 16000.0f;
                    phase1 += (6.2831853f * f1) / 16000.0f;
                    phase2 += (6.2831853f * f2) / 16000.0f;
                    if (phase0 > 6.2831853f) phase0 -= 6.2831853f;
                    if (phase1 > 6.2831853f) phase1 -= 6.2831853f;
                    if (phase2 > 6.2831853f) phase2 -= 6.2831853f;
                    float v = 0.45f * sinf(phase0) + 0.35f * sinf(phase1) + 0.20f * sinf(phase2);
                    int16_t s = (int16_t)(v * amp);
                    stereo_buf[i * 2]     = s;
                    stereo_buf[i * 2 + 1] = s;
                }
                size_t written = 0;
                i2s_write(I2S_NUM_0, stereo_buf, sizeof(stereo_buf), &written, 20);
            }
        }
    }
    digitalWrite(PIN_SPEAKER_EN, LOW);
}

// ============================================================
// AXP2101 Power Management IC (0x34)
// Enables 3.3V rails for AMOLED, Touch, Audio & IMU + reads Battery
// ============================================================
typedef struct {
    bool     present;
    bool     vbus_good;
    bool     charging;
    uint16_t batt_mv;
    uint8_t  batt_pct;
} S3PowerState;

static S3PowerState g_power = {false, true, false, 4120, 100};

static bool s3_axp2101_init() {
    uint8_t chip_id = 0;
    if (!s3_i2c_read_bytes(I2C_ADDR_AXP2101, 0x03, &chip_id, 1)) {
        g_power.present = false;
        return false;
    }
    g_power.present = true;

    s3_i2c_write8(I2C_ADDR_AXP2101, 0x22, 0b00000110);
    s3_i2c_write8(I2C_ADDR_AXP2101, 0x27, 0x10);

    // Enable DCDC1 @ 3.3V while preserving any other active DCDC rails
    uint8_t dcdc_en = s3_i2c_read8(I2C_ADDR_AXP2101, 0x80, 0x01);
    s3_i2c_write8(I2C_ADDR_AXP2101, 0x80, dcdc_en | 0x01);
    s3_i2c_write8(I2C_ADDR_AXP2101, 0x82, (3300 - 1500) / 100);

    // Set ALDO1, ALDO2, ALDO3, ALDO4 = 3.3V (powers AMOLED, Touch, Audio & IMU on 1.8" & 2.06")
    uint8_t aldo_3v3 = (3300 - 500) / 100; // 0x1C
    s3_i2c_write8(I2C_ADDR_AXP2101, 0x92, aldo_3v3);
    s3_i2c_write8(I2C_ADDR_AXP2101, 0x93, aldo_3v3);
    s3_i2c_write8(I2C_ADDR_AXP2101, 0x94, aldo_3v3);
    s3_i2c_write8(I2C_ADDR_AXP2101, 0x95, aldo_3v3);

    uint8_t ldo_en = s3_i2c_read8(I2C_ADDR_AXP2101, 0x90, 0x00);
    s3_i2c_write8(I2C_ADDR_AXP2101, 0x90, ldo_en | 0x3F);

    // Enable battery voltage & system ADC channels (0x30 = 0x3F)
    s3_i2c_write8(I2C_ADDR_AXP2101, 0x30, 0x3F);
    s3_i2c_write8(I2C_ADDR_AXP2101, 0x62, 0x08);
    s3_i2c_write8(I2C_ADDR_AXP2101, 0x64, 0x03);
    return true;
}

static void s3_axp2101_poll() {
    if (!g_power.present) return;
    uint8_t st0 = s3_i2c_read8(I2C_ADDR_AXP2101, 0x00, 0x20);
    uint8_t st1 = s3_i2c_read8(I2C_ADDR_AXP2101, 0x01, 0x00);
    g_power.vbus_good = (st0 & (1 << 5)) != 0 || (st0 & (1 << 0)) != 0;
    g_power.charging  = ((st1 >> 5) & 0x03) == 0x01;

    uint8_t vbuf[2] = {0};
    if (s3_i2c_read_bytes(I2C_ADDR_AXP2101, 0x34, vbuf, 2)) {
        uint16_t mv = ((uint16_t)(vbuf[0] & 0x3F) << 8) | vbuf[1];
        if (mv >= 2500 && mv <= 4500) {
            g_power.batt_mv = mv;
        }
    }
    uint8_t pct = s3_i2c_read8(I2C_ADDR_AXP2101, 0xA4, 100);
    if (pct > 0 && pct <= 100) {
        g_power.batt_pct = pct;
    } else if (g_power.vbus_good) {
        g_power.batt_pct = 100;
    }
}

// ============================================================
// FT3168 Capacitive Touch Controller (0x38)
// ============================================================
typedef struct {
    bool     touched;
    bool     just_pressed;
    uint16_t x;
    uint16_t y;
    int16_t  drag_dx;
    int16_t  drag_dy;
} S3TouchState;

static S3TouchState g_touch = {false, false, 0, 0, 0, 0};
static uint16_t s_touch_start_x = 0;
static uint16_t s_touch_start_y = 0;
static unsigned long s_last_tap_ms = 0;

static void s3_touch_init() {
    if (g_board_is_18) {
        pinMode(PIN_TP_INT_18, INPUT_PULLUP);
    } else {
        pinMode(PIN_TP_RST_206, OUTPUT);
        pinMode(PIN_TP_INT_206, INPUT_PULLUP);
        digitalWrite(PIN_TP_RST_206, LOW);
        delay(15);
        digitalWrite(PIN_TP_RST_206, HIGH);
        delay(50);
    }
}

static void s3_touch_poll() {
    uint8_t buf[5] = {0};
    g_touch.just_pressed = false;
    if (!s3_i2c_read_bytes(I2C_ADDR_FT3168, 0x02, buf, 5)) {
        g_touch.touched = false;
        return;
    }
    uint8_t count = buf[0] & 0x0F;
    if (count == 0 || count > 2) {
        g_touch.touched = false;
        return;
    }
    uint16_t tx = ((uint16_t)(buf[1] & 0x0F) << 8) | buf[2];
    uint16_t ty = ((uint16_t)(buf[3] & 0x0F) << 8) | buf[4];
    if (tx >= g_lcd_w) tx = g_lcd_w - 1;
    if (ty >= g_lcd_h) ty = g_lcd_h - 1;

    if (!g_touch.touched) {
        s_touch_start_x = tx;
        s_touch_start_y = ty;
        if (millis() - s_last_tap_ms > 140) {
            g_touch.just_pressed = true;
            s_last_tap_ms = millis();
        }
    }
    g_touch.touched = true;
    g_touch.x = tx;
    g_touch.y = ty;
    g_touch.drag_dx = (int16_t)tx - (int16_t)s_touch_start_x;
    g_touch.drag_dy = (int16_t)ty - (int16_t)s_touch_start_y;
}

// ============================================================
// QMI8658 6-Axis IMU (0x6B) + PCF85063 RTC (0x51)
// Drives QuantTern {-1, 0, +1} VAD affect trits, Sickness & Tilt
// ============================================================
typedef struct {
    bool    present;
    float   ax, ay, az;      // g
    float   gx, gy, gz;      // deg/s
    float   gyro_mag;        // total angular speed (deg/s)
    float   sickness;        // 0.0 (zen calm) .. 1.0 (chaotic motion)
    int8_t  trit_v;          // Valence trit {-1, 0, +1}
    int8_t  trit_a;          // Arousal trit {-1, 0, +1}
    int8_t  trit_d;          // Dominance trit {-1, 0, +1}
    uint8_t qt_byte;         // 6-bit packed QuantTern affect byte
    uint8_t rtc_h, rtc_m, rtc_s;
} S3ImuState;

static S3ImuState g_imu = {
    false, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
    1, 0, 1, 0x26, 6, 58, 0
};

static inline uint8_t s3_trit_bits(int8_t t) {
    if (t > 0) return 0b10;
    if (t < 0) return 0b01;
    return 0b00;
}

static bool s3_imu_init() {
    uint8_t who = s3_i2c_read8(I2C_ADDR_QMI8658, 0x00, 0x00);
    if (who != 0x05) {
        g_imu.present = false;
        return false;
    }
    g_imu.present = true;
    // CTRL1: address auto-increment (0x40) | little-endian (0x60)
    s3_i2c_write8(I2C_ADDR_QMI8658, 0x02, 0x60);
    // CTRL2: Accel +-8g, 250Hz
    s3_i2c_write8(I2C_ADDR_QMI8658, 0x03, 0x23);
    // CTRL3: Gyro +-512dps, 250Hz
    s3_i2c_write8(I2C_ADDR_QMI8658, 0x04, 0x53);
    // CTRL7: Enable Accel + Gyro
    s3_i2c_write8(I2C_ADDR_QMI8658, 0x08, 0x03);
    return true;
}

static uint8_t bcd2dec(uint8_t v) {
    return (v >> 4) * 10 + (v & 0x0F);
}

static void s3_imu_poll() {
    if (g_imu.present) {
        uint8_t raw[12] = {0};
        if (s3_i2c_read_bytes(I2C_ADDR_QMI8658, 0x35, raw, 12)) {
            int16_t rax = (int16_t)((raw[1] << 8) | raw[0]);
            int16_t ray = (int16_t)((raw[3] << 8) | raw[2]);
            int16_t raz = (int16_t)((raw[5] << 8) | raw[4]);
            int16_t rgx = (int16_t)((raw[7] << 8) | raw[6]);
            int16_t rgy = (int16_t)((raw[9] << 8) | raw[8]);
            int16_t rgz = (int16_t)((raw[11] << 8) | raw[10]);

            g_imu.ax = g_imu.ax * 0.7f + ((float)rax / 4096.0f) * 0.3f;
            g_imu.ay = g_imu.ay * 0.7f + ((float)ray / 4096.0f) * 0.3f;
            g_imu.az = g_imu.az * 0.7f + ((float)raz / 4096.0f) * 0.3f;
            g_imu.gx = (float)rgx / 64.0f;
            g_imu.gy = (float)rgy / 64.0f;
            g_imu.gz = (float)rgz / 64.0f;

            float gmag = sqrtf(g_imu.gx * g_imu.gx + g_imu.gy * g_imu.gy + g_imu.gz * g_imu.gz);
            g_imu.gyro_mag = gmag;

            // Motion sickness / zen coherence (matches baye-rust thresholds)
            if (gmag > 80.0f) {
                g_imu.sickness += 0.035f;
                if (g_imu.sickness > 1.0f) g_imu.sickness = 1.0f;
            } else if (gmag < 26.0f) {
                g_imu.sickness -= 0.025f;
                if (g_imu.sickness < 0.0f) g_imu.sickness = 0.0f;
            }

            g_imu.trit_v = (g_imu.ax > 0.18f) ? +1 : ((g_imu.ax < -0.18f) ? -1 : 0);
            g_imu.trit_a = (gmag > 35.0f) ? +1 : ((gmag < 8.0f) ? -1 : 0);
            g_imu.trit_d = (g_imu.az > 0.35f) ? +1 : ((g_imu.az < -0.35f) ? -1 : 0);
            g_imu.qt_byte = (uint8_t)((s3_trit_bits(g_imu.trit_v) << 4) |
                                      (s3_trit_bits(g_imu.trit_a) << 2) |
                                       s3_trit_bits(g_imu.trit_d));
        }
    }
    uint8_t rbuf[3] = {0};
    if (s3_i2c_read_bytes(I2C_ADDR_PCF85063, 0x04, rbuf, 3)) {
        g_imu.rtc_s = bcd2dec(rbuf[0] & 0x7F);
        g_imu.rtc_m = bcd2dec(rbuf[1] & 0x7F);
        g_imu.rtc_h = bcd2dec(rbuf[2] & 0x3F);
    }
}

// ============================================================
// SH8601 / CO5300 QSPI AMOLED Driver (baye-rust + Waveshare BSP parity)
// Manual CS (GPIO 12) + 1024-pixel QSPI DMA chunk streaming
// ============================================================
static spi_device_handle_t s_amoled_spi = NULL;
static uint16_t* s_band_buf = NULL;
static bool      s_first_chunk = true;

static inline void sh8601_cs_low() {
    gpio_set_level((gpio_num_t)PIN_LCD_CS, 0);
}

static inline void sh8601_cs_high() {
    gpio_set_level((gpio_num_t)PIN_LCD_CS, 1);
}

static void sh8601_write_cmd(uint8_t cmd) {
    if (!s_amoled_spi) return;
    sh8601_cs_low();
    spi_transaction_ext_t tran;
    memset(&tran, 0, sizeof(tran));
    tran.base.flags = SPI_TRANS_MULTILINE_CMD | SPI_TRANS_MULTILINE_ADDR;
    tran.base.cmd   = 0x02;
    tran.base.addr  = ((uint64_t)cmd) << 8;
    tran.base.length = 0;
    spi_device_polling_transmit(s_amoled_spi, &tran.base);
    sh8601_cs_high();
}

static void sh8601_write_cmd_data(uint8_t cmd, uint8_t data) {
    if (!s_amoled_spi) return;
    sh8601_cs_low();
    spi_transaction_ext_t tran;
    memset(&tran, 0, sizeof(tran));
    tran.base.flags = SPI_TRANS_USE_TXDATA | SPI_TRANS_MULTILINE_CMD | SPI_TRANS_MULTILINE_ADDR;
    tran.base.cmd   = 0x02;
    tran.base.addr  = ((uint64_t)cmd) << 8;
    tran.base.tx_data[0] = data;
    tran.base.length = 8;
    spi_device_polling_transmit(s_amoled_spi, &tran.base);
    sh8601_cs_high();
}

static void sh8601_write_cmd_bytes(uint8_t cmd, const uint8_t* data, size_t len) {
    if (!s_amoled_spi) return;
    if (len == 0) {
        sh8601_write_cmd(cmd);
        return;
    }
    if (len == 1) {
        sh8601_write_cmd_data(cmd, data[0]);
        return;
    }
    sh8601_cs_low();
    spi_transaction_ext_t tran;
    memset(&tran, 0, sizeof(tran));
    tran.base.flags = SPI_TRANS_USE_TXDATA | SPI_TRANS_MULTILINE_CMD | SPI_TRANS_MULTILINE_ADDR;
    tran.base.cmd   = 0x02;
    tran.base.addr  = ((uint64_t)cmd) << 8;
    for (size_t i = 0; i < len && i < 4; i++) {
        tran.base.tx_data[i] = data[i];
    }
    tran.base.length = len * 8;
    spi_device_polling_transmit(s_amoled_spi, &tran.base);
    sh8601_cs_high();
}

static void sh8601_write_cmd_data32(uint8_t cmd, uint16_t d1, uint16_t d2) {
    uint8_t buf[4] = {
        (uint8_t)(d1 >> 8), (uint8_t)(d1 & 0xFF),
        (uint8_t)(d2 >> 8), (uint8_t)(d2 & 0xFF)
    };
    sh8601_write_cmd_bytes(cmd, buf, 4);
}

static void s3_amoled_set_brightness(uint8_t level) {
    sh8601_write_cmd_data(0x51, level);
}

static bool s3_amoled_init() {
    // Configure CS as manual GPIO output (matches baye-rust sh8601.rs)
    gpio_set_direction((gpio_num_t)PIN_LCD_CS, GPIO_MODE_OUTPUT);
    sh8601_cs_high();

    if (!g_board_is_18) {
        // 2.06" board has direct LCD_RST on GPIO 8
        pinMode(PIN_LCD_RST_206, OUTPUT);
        pinMode(PIN_LCD_TE, INPUT);
        digitalWrite(PIN_LCD_RST_206, HIGH);
        delay(10);
        digitalWrite(PIN_LCD_RST_206, LOW);
        delay(20);
        digitalWrite(PIN_LCD_RST_206, HIGH);
        delay(150);
    }

    spi_bus_config_t buscfg;
    memset(&buscfg, 0, sizeof(buscfg));
    buscfg.sclk_io_num     = PIN_LCD_PCLK;
    buscfg.data0_io_num    = PIN_LCD_D0;
    buscfg.data1_io_num    = PIN_LCD_D1;
    buscfg.data2_io_num    = PIN_LCD_D2;
    buscfg.data3_io_num    = PIN_LCD_D3;
    buscfg.data4_io_num    = -1;
    buscfg.data5_io_num    = -1;
    buscfg.data6_io_num    = -1;
    buscfg.data7_io_num    = -1;
    buscfg.max_transfer_sz = (AMOLED_MAX_W * AMOLED_BAND_H * 2) + 64;
    buscfg.flags           = SPICOMMON_BUSFLAG_MASTER | SPICOMMON_BUSFLAG_GPIO_PINS;

    if (spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO) != ESP_OK) {
        return false;
    }

    spi_device_interface_config_t devcfg;
    memset(&devcfg, 0, sizeof(devcfg));
    devcfg.command_bits   = 8;
    devcfg.address_bits   = 24;
    devcfg.dummy_bits     = 0;
    devcfg.mode           = 0;
    devcfg.clock_speed_hz = 40 * 1000 * 1000;
    devcfg.spics_io_num   = -1; // Manual CS control on GPIO 12
    devcfg.flags          = SPI_DEVICE_HALFDUPLEX;
    devcfg.queue_size     = 1;

    if (spi_bus_add_device(SPI2_HOST, &devcfg, &s_amoled_spi) != ESP_OK) {
        return false;
    }
    spi_device_acquire_bus(s_amoled_spi, portMAX_DELAY);

    // Allocate DMA-capable band buffer in internal SRAM
    size_t band_bytes = (size_t)AMOLED_MAX_W * (size_t)AMOLED_BAND_H * sizeof(uint16_t);
    s_band_buf = (uint16_t*)heap_caps_aligned_alloc(16, band_bytes, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    if (!s_band_buf) {
        s_band_buf = (uint16_t*)heap_caps_malloc(band_bytes, MALLOC_CAP_DMA);
    }

    if (g_board_is_18) {
        // Exact Waveshare ESP32-S3-Touch-AMOLED-1.8 SH8601 init sequence (from baye-rust sh8601.rs)
        sh8601_write_cmd(0x01); // SWRESET
        delay(200);
        sh8601_write_cmd(0x11); // SLPOUT
        delay(120);
        sh8601_write_cmd(0x13); // NORON
        sh8601_write_cmd(0x20); // INVOFF
        sh8601_write_cmd_data(0x3A, 0x05); // PIXFMT = RGB565 (SH8601 uses 0x05)
        sh8601_write_cmd(0x29); // DISPON
        sh8601_write_cmd_data(0x53, 0x28); // CTRL_DISPLAY
        sh8601_write_cmd_data(0x51, 0x58); // Soft-start brightness (34%) for USB-C hub enumeration safety
        sh8601_write_cmd_data(0x58, 0x00); // CE off
        delay(15);
    } else {
        // Exact Waveshare ESP32-S3-Touch-AMOLED-2.06 CO5300 init sequence (from baye-rust sh8601.rs)
        sh8601_write_cmd_data(0x36, 0x00); // MADCTL
        sh8601_write_cmd_data(0x3A, 0x55); // PIXFMT = RGB565 (CO5300 uses 0x55)
        sh8601_write_cmd(0x11);            // SLPOUT
        delay(120);
        sh8601_write_cmd_data(0xC4, 0x80); // CO5300 SPI Mode Control: enable Quad-SPI (QIO) pixel path
        sh8601_write_cmd_data(0x35, 0x00); // TEON
        sh8601_write_cmd_data(0x53, 0x20); // WRCTRLD
        delay(10);
        sh8601_write_cmd_data(0x63, 0xFF); // WRHBMD (CO5300 HBM brightness)
        delay(10);
        sh8601_write_cmd(0x29);            // DISPON
        delay(20);
        sh8601_write_cmd_data(0x51, 0x58); // Soft-start brightness (34%) for USB-C hub enumeration safety
    }

    return (s_band_buf != NULL);
}

static void s3_amoled_begin_frame() {
    if (!s_amoled_spi || !s_band_buf) return;
    // Both 1.8" SH8601 (x_gap=0) and 2.06" CO5300 (x_gap=22) set full-frame CASET/PASET/RAMWR
    // before streaming 1024-pixel QSPI chunks (matches baye-rust/src/drivers/sh8601.rs::set_addr_window)
    uint16_t x0 = g_lcd_x_gap;
    uint16_t x1 = g_lcd_x_gap + g_lcd_w - 1;
    uint16_t y0 = 0;
    uint16_t y1 = g_lcd_h - 1;
    sh8601_write_cmd_data32(0x2A, x0, x1);
    sh8601_write_cmd_data32(0x2B, y0, y1);
    sh8601_write_cmd(0x2C); // RAMWR
    s_first_chunk = true;
}

static void s3_amoled_push_band(int y0, int h) {
    (void)y0;
    if (!s_amoled_spi || !s_band_buf || h <= 0) return;

    // 1024-pixel chunked QSPI DMA streaming (exact baye-rust sh8601.rs parity for both SH8601 & CO5300):
    // First chunk of frame sends 0x32 QIO write with addr 0x003C00 (1.8" SH8601) or 0x002C00 (2.06" CO5300);
    // subsequent chunks stream raw 4-line QIO pixel data while CS stays low.
    size_t total_pixels = (size_t)g_lcd_w * (size_t)h;
    size_t offset = 0;

    while (offset < total_pixels) {
        size_t chunk = total_pixels - offset;
        if (chunk > QSPI_CHUNK_PIXELS) chunk = QSPI_CHUNK_PIXELS;

        if (s_first_chunk) {
            sh8601_cs_low();
        }

        spi_transaction_ext_t tran;
        memset(&tran, 0, sizeof(tran));
        if (s_first_chunk) {
            tran.base.flags = SPI_TRANS_MODE_QIO;
            tran.base.cmd   = 0x32;
            tran.base.addr  = g_board_is_18 ? 0x003C00 : 0x002C00;
            s_first_chunk   = false;
        } else {
            tran.base.flags   = SPI_TRANS_MODE_QIO |
                                SPI_TRANS_VARIABLE_CMD |
                                SPI_TRANS_VARIABLE_ADDR |
                                SPI_TRANS_VARIABLE_DUMMY;
            tran.command_bits = 0;
            tran.address_bits = 0;
            tran.dummy_bits   = 0;
        }
        tran.base.tx_buffer = &s_band_buf[offset];
        tran.base.length    = chunk * 16; // bits
        spi_device_polling_transmit(s_amoled_spi, &tran.base);

        offset += chunk;
    }
}

static void s3_amoled_end_frame() {
    sh8601_cs_high();
}

// ============================================================
// Compact 5x7 ASCII Font (0x20 .. 0x7E) for Constellation UI
// ============================================================
static const uint8_t FONT5X7[95][5] PROGMEM = {
    {0x00,0x00,0x00,0x00,0x00}, // ' '
    {0x00,0x00,0x5F,0x00,0x00}, // '!'
    {0x00,0x07,0x00,0x07,0x00}, // '"'
    {0x14,0x7F,0x14,0x7F,0x14}, // '#'
    {0x24,0x2A,0x7F,0x2A,0x12}, // '$'
    {0x23,0x13,0x08,0x64,0x62}, // '%'
    {0x36,0x49,0x55,0x22,0x50}, // '&'
    {0x00,0x05,0x03,0x00,0x00}, // '''
    {0x00,0x1C,0x22,0x41,0x00}, // '('
    {0x00,0x41,0x22,0x1C,0x00}, // ')'
    {0x14,0x08,0x3E,0x08,0x14}, // '*'
    {0x08,0x08,0x3E,0x08,0x08}, // '+'
    {0x00,0x50,0x30,0x00,0x00}, // ','
    {0x08,0x08,0x08,0x08,0x08}, // '-'
    {0x00,0x60,0x60,0x00,0x00}, // '.'
    {0x20,0x10,0x08,0x04,0x02}, // '/'
    {0x3E,0x51,0x49,0x45,0x3E}, // '0'
    {0x00,0x42,0x7F,0x40,0x00}, // '1'
    {0x42,0x61,0x51,0x49,0x46}, // '2'
    {0x21,0x41,0x45,0x4B,0x31}, // '3'
    {0x18,0x14,0x12,0x7F,0x10}, // '4'
    {0x27,0x45,0x45,0x45,0x39}, // '5'
    {0x3C,0x4A,0x49,0x49,0x30}, // '6'
    {0x01,0x71,0x09,0x05,0x03}, // '7'
    {0x36,0x49,0x49,0x49,0x36}, // '8'
    {0x06,0x49,0x49,0x29,0x1E}, // '9'
    {0x00,0x36,0x36,0x00,0x00}, // ':'
    {0x00,0x56,0x36,0x00,0x00}, // ';'
    {0x08,0x14,0x22,0x41,0x00}, // '<'
    {0x14,0x14,0x14,0x14,0x14}, // '='
    {0x00,0x41,0x22,0x14,0x08}, // '>'
    {0x02,0x01,0x51,0x09,0x06}, // '?'
    {0x32,0x49,0x79,0x41,0x3E}, // '@'
    {0x7E,0x11,0x11,0x11,0x7E}, // 'A'
    {0x7F,0x49,0x49,0x49,0x36}, // 'B'
    {0x3E,0x41,0x41,0x41,0x22}, // 'C'
    {0x7F,0x41,0x41,0x22,0x1C}, // 'D'
    {0x7F,0x49,0x49,0x49,0x41}, // 'E'
    {0x7F,0x09,0x09,0x09,0x01}, // 'F'
    {0x3E,0x41,0x49,0x49,0x7A}, // 'G'
    {0x7F,0x08,0x08,0x08,0x7F}, // 'H'
    {0x00,0x41,0x7F,0x41,0x00}, // 'I'
    {0x20,0x40,0x41,0x3F,0x01}, // 'J'
    {0x7F,0x08,0x14,0x22,0x41}, // 'K'
    {0x7F,0x40,0x40,0x40,0x40}, // 'L'
    {0x7F,0x02,0x0C,0x02,0x7F}, // 'M'
    {0x7F,0x04,0x08,0x10,0x7F}, // 'N'
    {0x3E,0x41,0x41,0x41,0x3E}, // 'O'
    {0x7F,0x09,0x09,0x09,0x06}, // 'P'
    {0x3E,0x41,0x51,0x21,0x5E}, // 'Q'
    {0x7F,0x09,0x19,0x29,0x46}, // 'R'
    {0x46,0x49,0x49,0x49,0x31}, // 'S'
    {0x01,0x01,0x7F,0x01,0x01}, // 'T'
    {0x3F,0x40,0x40,0x40,0x3F}, // 'U'
    {0x1F,0x20,0x40,0x20,0x1F}, // 'V'
    {0x3F,0x40,0x38,0x40,0x3F}, // 'W'
    {0x63,0x14,0x08,0x14,0x63}, // 'X'
    {0x07,0x08,0x70,0x08,0x07}, // 'Y'
    {0x61,0x51,0x49,0x45,0x43}, // 'Z'
    {0x00,0x7F,0x41,0x41,0x00}, // '['
    {0x02,0x04,0x08,0x10,0x20}, // '\'
    {0x00,0x41,0x41,0x7F,0x00}, // ']'
    {0x04,0x02,0x01,0x02,0x04}, // '^'
    {0x40,0x40,0x40,0x40,0x40}, // '_'
    {0x00,0x01,0x02,0x04,0x00}, // '`'
    {0x20,0x54,0x54,0x54,0x78}, // 'a'
    {0x7F,0x48,0x44,0x44,0x38}, // 'b'
    {0x38,0x44,0x44,0x44,0x20}, // 'c'
    {0x38,0x44,0x44,0x48,0x7F}, // 'd'
    {0x38,0x54,0x54,0x54,0x18}, // 'e'
    {0x08,0x7E,0x09,0x01,0x02}, // 'f'
    {0x0C,0x52,0x52,0x52,0x3E}, // 'g'
    {0x7F,0x08,0x04,0x04,0x78}, // 'h'
    {0x00,0x44,0x7D,0x40,0x00}, // 'i'
    {0x20,0x40,0x44,0x3D,0x00}, // 'j'
    {0x7F,0x10,0x28,0x44,0x00}, // 'k'
    {0x00,0x41,0x7F,0x40,0x00}, // 'l'
    {0x7C,0x04,0x18,0x04,0x78}, // 'm'
    {0x7C,0x08,0x04,0x04,0x78}, // 'n'
    {0x38,0x44,0x44,0x44,0x38}, // 'o'
    {0x7C,0x14,0x14,0x14,0x08}, // 'p'
    {0x08,0x14,0x14,0x18,0x7C}, // 'q'
    {0x7C,0x08,0x04,0x04,0x08}, // 'r'
    {0x48,0x54,0x54,0x54,0x20}, // 's'
    {0x04,0x3F,0x44,0x40,0x20}, // 't'
    {0x3C,0x40,0x40,0x20,0x7C}, // 'u'
    {0x1C,0x20,0x40,0x20,0x1C}, // 'v'
    {0x3C,0x40,0x30,0x40,0x3C}, // 'w'
    {0x44,0x28,0x10,0x28,0x44}, // 'x'
    {0x0C,0x50,0x50,0x50,0x3C}, // 'y'
    {0x44,0x64,0x54,0x4C,0x44}, // 'z'
    {0x00,0x08,0x36,0x41,0x00}, // '{'
    {0x00,0x00,0x7F,0x00,0x00}, // '|'
    {0x00,0x41,0x36,0x08,0x00}, // '}'
    {0x10,0x08,0x08,0x10,0x08}, // '~'
};

// ============================================================
// Band-Clipped True-Color 2D Rasterizer (S3Canvas)
// Responsive across 368x448 (1.8") and 410x502 (2.06")
// ============================================================
struct S3Canvas {
    int y0;
    int y1;

    inline void clear(uint16_t color) {
        int count = (int)g_lcd_w * (y1 - y0);
        for (int i = 0; i < count; i++) {
            s_band_buf[i] = color;
        }
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
            int span = x_end - x_start;
            for (int i = 0; i < span; i++) row[i] = color;
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
        for (int py = y_start; py < y_end; py++) {
            s_band_buf[(py - y0) * g_lcd_w + x] = color;
        }
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
        int min_y = (ly0 < ly1 ? ly0 : ly1) - thick;
        int max_y = (ly0 > ly1 ? ly0 : ly1) + thick;
        if (max_y < y0 || min_y >= y1) return;

        int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
        int dy = -abs(ly1 - ly0), sy = ly0 < ly1 ? 1 : -1;
        int err = dx + dy;
        while (true) {
            if (thick <= 1) {
                put_px(x0, ly0, color);
            } else {
                fill_rect(x0 - thick / 2, ly0 - thick / 2, thick, thick, color);
            }
            if (x0 == x1 && ly0 == ly1) break;
            int e2 = 2 * err;
            if (e2 >= dy) { err += dy; x0 += sx; }
            if (e2 <= dx) { err += dx; ly0 += sy; }
        }
    }

    inline void fill_circle(int cx, int cy, int r, uint16_t color) {
        if (cy + r < y0 || cy - r >= y1) return;
        for (int dy = -r; dy <= r; dy++) {
            int py = cy + dy;
            if (py < y0 || py >= y1) continue;
            int dx = (int)sqrtf((float)(r * r - dy * dy));
            draw_hline(cx - dx, py, 2 * dx + 1, color);
        }
    }

    inline void draw_circle(int cx, int cy, int r, uint16_t color) {
        if (cy + r < y0 || cy - r >= y1) return;
        int x = r, y = 0, err = 0;
        while (x >= y) {
            put_px(cx + x, cy + y, color);
            put_px(cx + y, cy + x, color);
            put_px(cx - y, cy + x, color);
            put_px(cx - x, cy + y, color);
            put_px(cx - x, cy - y, color);
            put_px(cx - y, cy - x, color);
            put_px(cx + y, cy - x, color);
            put_px(cx + x, cy - y, color);
            y++;
            if (err <= 0) {
                err += 2 * y + 1;
            }
            if (err > 0) {
                x--;
                err -= 2 * x + 1;
            }
        }
    }

    inline void draw_char(int x, int y, char c, uint16_t color, int scale = 1) {
        int char_h = 7 * scale;
        if (y + char_h <= y0 || y >= y1) return;
        if (c < 0x20 || c > 0x7E) c = '?';
        const uint8_t* glyph = FONT5X7[c - 0x20];
        for (int col = 0; col < 5; col++) {
            uint8_t bits = pgm_read_byte(&glyph[col]);
            if (!bits) continue;
            for (int row = 0; row < 7; row++) {
                if (bits & (1 << row)) {
                    if (scale == 1) {
                        put_px(x + col, y + row, color);
                    } else {
                        fill_rect(x + col * scale, y + row * scale, scale, scale, color);
                    }
                }
            }
        }
    }

    inline void draw_text(int x, int y, const char* str, uint16_t color, int scale = 1) {
        if (!str || y + 7 * scale <= y0 || y >= y1) return;
        int cx = x;
        int adv = 6 * scale;
        while (*str) {
            draw_char(cx, y, *str++, color, scale);
            cx += adv;
        }
    }

    inline void draw_text_centered(int cx, int y, const char* str, uint16_t color, int scale = 1) {
        if (!str) return;
        int len = (int)strlen(str);
        int w = len * 6 * scale - scale;
        draw_text(cx - w / 2, y, str, color, scale);
    }

    inline void draw_text_right(int rx, int y, const char* str, uint16_t color, int scale = 1) {
        if (!str) return;
        int len = (int)strlen(str);
        int w = len * 6 * scale - scale;
        draw_text(rx - w, y, str, color, scale);
    }
};

#endif /* FLUGOS_S3_AMOLED_HAL_H */
