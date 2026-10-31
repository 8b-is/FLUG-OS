//! # `config` — FLUG-OS Pure-Rust Hardware & Sovereign Constellation Configuration
//!
//! 100% integer constants (Zero Floats!).
//! Supports both Waveshare ESP32-S3-Touch-AMOLED-1.8 (`368×448` SH8601)
//! and ESP32-S3-Touch-AMOLED-2.06 (`410×502` CO5300, `--features board-206`).

pub const PIN_LCD_CS: i32 = 12;
pub const PIN_LCD_CLK: i32 = 11;
pub const PIN_LCD_D0: i32 = 4;
pub const PIN_LCD_D1: i32 = 5;
pub const PIN_LCD_D2: i32 = 6;
pub const PIN_LCD_D3: i32 = 7;

pub const BOARD_206: bool = cfg!(feature = "board-206");
pub const BOARD_NAME: &str = if BOARD_206 {
    "Waveshare AMOLED 2.06 (CO5300)"
} else {
    "Waveshare AMOLED 1.8 (SH8601)"
};
pub const PIN_LCD_RST: i32 = if BOARD_206 { 8 } else { -1 };
pub const PIN_SD_CS: i32 = if BOARD_206 { 17 } else { -1 };

pub const LCD_WIDTH: u16 = if BOARD_206 { 410 } else { 368 };
pub const LCD_HEIGHT: u16 = if BOARD_206 { 502 } else { 448 };

pub const PIN_I2S_MCLK: i32 = 16;
pub const PIN_I2S_BCLK: i32 = if BOARD_206 { 41 } else { 9 };
pub const PIN_I2S_LRCLK: i32 = 45;
pub const PIN_I2S_DOUT: i32 = if BOARD_206 { 40 } else { 8 };
pub const PIN_I2S_DIN: i32 = if BOARD_206 { 42 } else { 10 };
pub const ES8311_I2C_ADDR: u8 = 0x18;
pub const PIN_SPEAKER_EN: i32 = 46;

pub const TCA9554_I2C_ADDR: u8 = 0x20;
pub const QMI8658_I2C_ADDR: u8 = 0x6B;
pub const AXP2101_I2C_ADDR: u8 = 0x34;
pub const FT3168_I2C_ADDR: u8 = 0x38;

pub const PIN_SD_MOSI: i32 = 1;
pub const PIN_SD_SCLK: i32 = 2;
pub const PIN_SD_MISO: i32 = 3;
pub const SD_SPI_FREQ_INIT: i32 = 400_000;
pub const SD_SPI_FREQ_NORMAL: i32 = 20_000_000;
pub const SD_SECTOR_SIZE: usize = 512;
pub const M8_BLOCK_SIZE: usize = 4096;
pub const SECTORS_PER_M8_BLOCK: usize = M8_BLOCK_SIZE / SD_SECTOR_SIZE;
pub const TCA9554_EXIO7_BIT: u8 = 0x80;

/// Integer tilt axis sign (`-1` or `+1` — zero floats!)
pub const TILT_AXIS_SIGN: i8 = -1;
