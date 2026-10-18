/*
 * FLUG-OS — Waveshare ESP32-S3-Touch-AMOLED-2.06 Hardware Abstraction Layer
 * Sovereign Constellation Hardware Target (vaked.dev × 8b-is)
 *
 * Exact Board Pinout (verified from factory firmware & Waveshare BSP):
 *   Display: SH8601 / CO5300 2.06" 410x502 QSPI AMOLED (x_gap = 0x16 = 22)
 *     LCD_CS   = GPIO 12
 *     LCD_PCLK = GPIO 11
 *     LCD_D0   = GPIO 4
 *     LCD_D1   = GPIO 5
 *     LCD_D2   = GPIO 6
 *     LCD_D3   = GPIO 7
 *     LCD_RST  = GPIO 8
 *     LCD_TE   = GPIO 13
 *   Shared I2C Bus (I2C_NUM_0):
 *     I2C_SCL  = GPIO 14
 *     I2C_SDA  = GPIO 15
 *     AXP2101 PMIC  @ 0x34 (ALDO1..4 3.3V display/peripheral power rails)
 *     FT3168 Touch  @ 0x38 (TP_RST = GPIO 9, TP_INT = GPIO 38)
 *     QMI8658 IMU   @ 0x6B (6-axis Accel + Gyro)
 *     PCF85063 RTC  @ 0x51
 *     ES8311 Codec  @ 0x18
 *   Buttons:
 *     BOOT_BTN = GPIO 0 (active low)
 */

#ifndef FLUGOS_S3_AMOLED_HAL_H
#define FLUGOS_S3_AMOLED_HAL_H

#include <Arduino.h>
#include <Wire.h>
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_heap_caps.h"

// ============================================================
// Display & Pin Constants
// ============================================================
#define AMOLED_W            410
#define AMOLED_H            502
#define AMOLED_X_GAP        0x16   // 22-column internal GRAM offset on 2.06" CO5300/SH8601
#define AMOLED_Y_GAP        0x00
#define AMOLED_BAND_H       26     // Even height (y0 even, y1-1 odd) -> 21,320 bytes DMA RAM

#define PIN_LCD_CS          12
#define PIN_LCD_PCLK        11
#define PIN_LCD_D0          4
#define PIN_LCD_D1          5
#define PIN_LCD_D2          6
#define PIN_LCD_D3          7
#define PIN_LCD_RST         8
#define PIN_LCD_TE          13

#define PIN_I2C_SCL         14
#define PIN_I2C_SDA         15
#define PIN_TP_RST          9
#define PIN_TP_INT          38
#define PIN_BOOT_BTN        0

#define I2C_ADDR_AXP2101    0x34
#define I2C_ADDR_FT3168     0x38
#define I2C_ADDR_QMI8658    0x6B
#define I2C_ADDR_PCF85063   0x51

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
#define COL_BG_ELEV     rgb565_be(11, 14,  23)   // #0b0e17 Elevated Card
#define COL_BG_PANEL    rgb565_be(17, 22,  36)   // #111624 Highlight Panel
#define COL_BORDER      rgb565_be(32, 42,  68)   // #202a44 Hairline Border
#define COL_GRID        rgb565_be(20, 26,  42)   // #141a2a Subtle Grid
#define COL_TEXT        rgb565_be(236, 242, 255) // #ecf2ff Crisp White
#define COL_SUBTEXT     rgb565_be(138, 152, 184) // #8a98b8 Muted Slate
#define COL_DIM         rgb565_be(76,  88, 118)  // #4c5876 Dim Label
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

static S3PowerState g_power = {false, true, false, 4120, 95};

static bool s3_axp2101_init() {
    uint8_t chip_id = 0;
    if (!s3_i2c_read_bytes(I2C_ADDR_AXP2101, 0x03, &chip_id, 1)) {
        g_power.present = false;
        return false;
    }
    g_power.present = true;

    // Power-off & key hold config (from Waveshare 2.06 BSP)
    s3_i2c_write8(I2C_ADDR_AXP2101, 0x22, 0b00000110);
    s3_i2c_write8(I2C_ADDR_AXP2101, 0x27, 0x10);

    // Enable DCDC1 @ 3.3V
    s3_i2c_write8(I2C_ADDR_AXP2101, 0x80, 0x01);
    s3_i2c_write8(I2C_ADDR_AXP2101, 0x82, (3300 - 1500) / 100);

    // Set ALDO1, ALDO2, ALDO3, ALDO4 = 3.3V (step = 100mV from 500mV)
    uint8_t aldo_3v3 = (3300 - 500) / 100; // 0x1C
    s3_i2c_write8(I2C_ADDR_AXP2101, 0x92, aldo_3v3);
    s3_i2c_write8(I2C_ADDR_AXP2101, 0x93, aldo_3v3);
    s3_i2c_write8(I2C_ADDR_AXP2101, 0x94, aldo_3v3);
    s3_i2c_write8(I2C_ADDR_AXP2101, 0x95, aldo_3v3);

    // Enable ALDO1..4 & BLDO1..2
    uint8_t ldo_en = s3_i2c_read8(I2C_ADDR_AXP2101, 0x90, 0x00);
    s3_i2c_write8(I2C_ADDR_AXP2101, 0x90, ldo_en | 0x3F);

    // Enable battery voltage & system ADC channels
    s3_i2c_write8(I2C_ADDR_AXP2101, 0x30, 0x3F);
    // Charger current 200mA, CV 4.2V
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
        uint16_t mv = ((uint16_t)(vbuf[0] & 0x1F) << 8) | vbuf[1];
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
    pinMode(PIN_TP_RST, OUTPUT);
    pinMode(PIN_TP_INT, INPUT_PULLUP);
    digitalWrite(PIN_TP_RST, LOW);
    delay(15);
    digitalWrite(PIN_TP_RST, HIGH);
    delay(50);
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
    if (tx >= AMOLED_W) tx = AMOLED_W - 1;
    if (ty >= AMOLED_H) ty = AMOLED_H - 1;

    if (!g_touch.touched) {
        s_touch_start_x = tx;
        s_touch_start_y = ty;
        if (millis() - s_last_tap_ms > 160) {
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
// Drives QuantTern {-1, 0, +1} VAD affect trits & Lissajous tilt
// ============================================================
typedef struct {
    bool    present;
    float   ax, ay, az;      // g
    float   gx, gy, gz;      // deg/s
    int8_t  trit_v;          // Valence trit {-1, 0, +1}
    int8_t  trit_a;          // Arousal trit {-1, 0, +1}
    int8_t  trit_d;          // Dominance trit {-1, 0, +1}
    uint8_t qt_byte;         // 6-bit packed QuantTern affect byte
    uint8_t rtc_h, rtc_m, rtc_s;
} S3ImuState;

static S3ImuState g_imu = {false, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1, 0, 1, 0x26, 4, 20, 0};

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
    // CTRL1: address auto-increment + little-endian serial read
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

            // +-8g -> 4096 LSB/g; +-512dps -> 64 LSB/dps
            g_imu.ax = g_imu.ax * 0.7f + ((float)rax / 4096.0f) * 0.3f;
            g_imu.ay = g_imu.ay * 0.7f + ((float)ray / 4096.0f) * 0.3f;
            g_imu.az = g_imu.az * 0.7f + ((float)raz / 4096.0f) * 0.3f;
            g_imu.gx = (float)rgx / 64.0f;
            g_imu.gy = (float)rgy / 64.0f;
            g_imu.gz = (float)rgz / 64.0f;

            float gyro_mag = fabsf(g_imu.gx) + fabsf(g_imu.gy) + fabsf(g_imu.gz);
            g_imu.trit_v = (g_imu.ax > 0.18f) ? +1 : ((g_imu.ax < -0.18f) ? -1 : 0);
            g_imu.trit_a = (gyro_mag > 35.0f) ? +1 : ((gyro_mag < 8.0f) ? -1 : 0);
            g_imu.trit_d = (g_imu.az > 0.35f) ? +1 : ((g_imu.az < -0.35f) ? -1 : 0);
            g_imu.qt_byte = (uint8_t)((s3_trit_bits(g_imu.trit_v) << 4) |
                                      (s3_trit_bits(g_imu.trit_a) << 2) |
                                       s3_trit_bits(g_imu.trit_d));
        }
    }
    // Read PCF85063 RTC time registers 0x04..0x06
    uint8_t rbuf[3] = {0};
    if (s3_i2c_read_bytes(I2C_ADDR_PCF85063, 0x04, rbuf, 3)) {
        g_imu.rtc_s = bcd2dec(rbuf[0] & 0x7F);
        g_imu.rtc_m = bcd2dec(rbuf[1] & 0x7F);
        g_imu.rtc_h = bcd2dec(rbuf[2] & 0x3F);
    }
}

// ============================================================
// SH8601 / CO5300 QSPI AMOLED Driver (410x502, SPI2_HOST QSPI DMA)
// ============================================================
static spi_device_handle_t s_amoled_spi = NULL;
static uint16_t* s_band_buf = NULL;

static void sh8601_tx_param(uint8_t cmd, const uint8_t* data, size_t len) {
    if (!s_amoled_spi) return;
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.flags = 0; // 1-line SPI for command, address, and parameter bytes
    t.cmd   = 0x02;
    t.addr  = ((uint32_t)cmd) << 8;
    t.length = len * 8;
    t.tx_buffer = (len > 0) ? data : NULL;
    spi_device_polling_transmit(s_amoled_spi, &t);
}

static void sh8601_tx_color(const void* pixels, size_t byte_len) {
    if (!s_amoled_spi || byte_len == 0) return;
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    // 1-line SPI for 0x32002C00 command header, 4-line QSPI (QIO) for pixel payload
    t.flags     = SPI_TRANS_MODE_QIO;
    t.cmd       = 0x32;
    t.addr      = 0x002C00;
    t.length    = byte_len * 8;
    t.tx_buffer = pixels;
    spi_device_polling_transmit(s_amoled_spi, &t);
}

static void s3_amoled_set_brightness(uint8_t level) {
    sh8601_tx_param(0x51, &level, 1);
}

static bool s3_amoled_init() {
    pinMode(PIN_LCD_RST, OUTPUT);
    pinMode(PIN_LCD_TE, INPUT);

    // Hardware reset pulse
    digitalWrite(PIN_LCD_RST, HIGH);
    delay(10);
    digitalWrite(PIN_LCD_RST, LOW);
    delay(20);
    digitalWrite(PIN_LCD_RST, HIGH);
    delay(150);

    // Initialize QSPI bus on SPI2_HOST
    spi_bus_config_t buscfg;
    memset(&buscfg, 0, sizeof(buscfg));
    buscfg.sclk_io_num     = PIN_LCD_PCLK;
    buscfg.data0_io_num    = PIN_LCD_D0;
    buscfg.data1_io_num    = PIN_LCD_D1;
    buscfg.data2_io_num    = PIN_LCD_D2;
    buscfg.data3_io_num    = PIN_LCD_D3;
    buscfg.max_transfer_sz = AMOLED_W * AMOLED_BAND_H * 2 + 64;
    buscfg.flags           = SPICOMMON_BUSFLAG_MASTER | SPICOMMON_BUSFLAG_QUAD;

    if (spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO) != ESP_OK) {
        return false;
    }

    spi_device_interface_config_t devcfg;
    memset(&devcfg, 0, sizeof(devcfg));
    devcfg.command_bits   = 8;
    devcfg.address_bits   = 24;
    devcfg.mode           = 0;
    devcfg.clock_speed_hz = 40 * 1000 * 1000; // 40 MHz QSPI = 20 MB/s
    devcfg.spics_io_num   = PIN_LCD_CS;
    devcfg.queue_size     = 4;
    devcfg.flags          = SPI_DEVICE_HALFDUPLEX;

    if (spi_bus_add_device(SPI2_HOST, &devcfg, &s_amoled_spi) != ESP_OK) {
        return false;
    }

    // Allocate DMA-capable band buffer in internal SRAM (21,320 bytes)
    s_band_buf = (uint16_t*)heap_caps_malloc(AMOLED_W * AMOLED_BAND_H * sizeof(uint16_t),
                                             MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    if (!s_band_buf) {
        s_band_buf = (uint16_t*)malloc(AMOLED_W * AMOLED_BAND_H * sizeof(uint16_t));
    }

    // Exact Waveshare ESP32-S3-Touch-AMOLED-2.06 SH8601/CO5300 init sequence
    const uint8_t madctl = 0x00;
    const uint8_t colmod = 0x55; // 16-bit RGB565
    sh8601_tx_param(0x36, &madctl, 1);
    sh8601_tx_param(0x3A, &colmod, 1);

    sh8601_tx_param(0x11, NULL, 0); // Sleep Out
    delay(120);

    const uint8_t c4[] = {0x80};
    sh8601_tx_param(0xC4, c4, 1);

    const uint8_t te_line[] = {0x01, 0xD1};
    sh8601_tx_param(0x44, te_line, 2);

    const uint8_t te_on[] = {0x00};
    sh8601_tx_param(0x35, te_on, 1);

    const uint8_t ctrl_d[] = {0x20};
    sh8601_tx_param(0x53, ctrl_d, 1);
    delay(10);

    const uint8_t hbm[] = {0xFF};
    sh8601_tx_param(0x63, hbm, 1);
    delay(10);

    const uint8_t dim0[] = {0x00};
    sh8601_tx_param(0x51, dim0, 1);
    delay(10);

    sh8601_tx_param(0x29, NULL, 0); // Display ON
    delay(20);

    const uint8_t full_bright[] = {0xFF};
    sh8601_tx_param(0x51, full_bright, 1);

    return (s_band_buf != NULL);
}

static void s3_amoled_push_band(int y0, int h) {
    if (!s_amoled_spi || !s_band_buf || h <= 0) return;
    uint16_t xs = AMOLED_X_GAP;
    uint16_t xe = AMOLED_X_GAP + AMOLED_W - 1;
    uint16_t ys = (uint16_t)(AMOLED_Y_GAP + y0);
    uint16_t ye = (uint16_t)(AMOLED_Y_GAP + y0 + h - 1);

    uint8_t caset[4] = {(uint8_t)(xs >> 8), (uint8_t)(xs & 0xFF),
                        (uint8_t)(xe >> 8), (uint8_t)(xe & 0xFF)};
    uint8_t raset[4] = {(uint8_t)(ys >> 8), (uint8_t)(ys & 0xFF),
                        (uint8_t)(ye >> 8), (uint8_t)(ye & 0xFF)};
    sh8601_tx_param(0x2A, caset, 4);
    sh8601_tx_param(0x2B, raset, 4);
    sh8601_tx_color(s_band_buf, (size_t)AMOLED_W * (size_t)h * 2);
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
// Renders full 410x502 scene in 26-row horizontal DMA bands
// ============================================================
struct S3Canvas {
    int y0;
    int y1;

    inline void clear(uint16_t color) {
        int count = AMOLED_W * (y1 - y0);
        for (int i = 0; i < count; i++) {
            s_band_buf[i] = color;
        }
    }

    inline void put_px(int x, int y, uint16_t color) {
        if ((unsigned)x >= AMOLED_W || y < y0 || y >= y1) return;
        s_band_buf[(y - y0) * AMOLED_W + x] = color;
    }

    inline void fill_rect(int x, int y, int w, int h, uint16_t color) {
        if (w <= 0 || h <= 0) return;
        int x_start = (x < 0) ? 0 : x;
        int x_end   = (x + w > AMOLED_W) ? AMOLED_W : (x + w);
        int y_start = (y < y0) ? y0 : y;
        int y_end   = (y + h > y1) ? y1 : (y + h);
        if (x_start >= x_end || y_start >= y_end) return;

        for (int py = y_start; py < y_end; py++) {
            uint16_t* row = &s_band_buf[(py - y0) * AMOLED_W + x_start];
            int span = x_end - x_start;
            for (int i = 0; i < span; i++) row[i] = color;
        }
    }

    inline void draw_hline(int x, int y, int w, uint16_t color) {
        if (y < y0 || y >= y1 || w <= 0) return;
        int x_start = (x < 0) ? 0 : x;
        int x_end   = (x + w > AMOLED_W) ? AMOLED_W : (x + w);
        if (x_start >= x_end) return;
        uint16_t* row = &s_band_buf[(y - y0) * AMOLED_W + x_start];
        for (int i = 0; i < x_end - x_start; i++) row[i] = color;
    }

    inline void draw_vline(int x, int y, int h, uint16_t color) {
        if ((unsigned)x >= AMOLED_W || h <= 0) return;
        int y_start = (y < y0) ? y0 : y;
        int y_end   = (y + h > y1) ? y1 : (y + h);
        for (int py = y_start; py < y_end; py++) {
            s_band_buf[(py - y0) * AMOLED_W + x] = color;
        }
    }

    inline void draw_rect(int x, int y, int w, int h, uint16_t color) {
        draw_hline(x, y, w, color);
        draw_hline(x, y + h - 1, w, color);
        draw_vline(x, y, h, color);
        draw_vline(x + w - 1, y, h, color);
    }

    // Rounded card with 4px corner bevels
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
