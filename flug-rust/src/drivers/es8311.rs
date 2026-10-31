//! ES8311 control for the original Waveshare 1.8-inch board.
//! Register sequence/clock coefficients checked against Espressif esp-adf
//! d6e1ef5ccf29ca52dcb8332a3232505366755012, esp_codec_dev/device/es8311.
//! I2S clocks must already be running; 48 kHz, 256fs MCLK, 16-bit Philips.
use super::i2c_bus::{self, SharedI2cBus};
use crate::config;
use std::{thread, time::Duration};

pub fn init(bus: &SharedI2cBus) -> Result<(), &'static str> {
    if config::AUDIO_SAMPLE_RATE != 48_000 {
        return Err("Codec coefficients require 48 kHz / 12.288 MHz MCLK");
    }
    let addr = config::ES8311_I2C_ADDR;
    let r = |reg| i2c_bus::read_reg(bus, addr, reg).map_err(|_| "ES8311 read failed");
    if r(0xfd)? != 0x83 || r(0xfe)? != 0x11 {
        return Err("Unexpected ES8311 chip identity");
    }
    let w =
        |reg, value| i2c_bus::write_reg(bus, addr, reg, value).map_err(|_| "ES8311 write failed");
    w(0x00, 0x1f)?;
    thread::sleep(Duration::from_millis(20));
    w(0x00, 0x00)?;
    w(0x44, 0x08)?;
    w(0x44, 0x08)?;
    for (reg, value) in [
        (0x01, 0x30),
        (0x02, 0x00),
        (0x03, 0x10),
        (0x04, 0x10),
        (0x05, 0x00),
        (0x06, 0x03),
        (0x07, 0x00),
        (0x08, 0xff),
        (0x0b, 0x00),
        (0x0c, 0x00),
        (0x10, 0x1f),
        (0x11, 0x7f),
        (0x00, 0x80), // Codec slave, internal digital logic enabled.
        (0x01, 0x3f), // Enable ADC/DAC clocks from MCLK pin.
        (0x09, 0x0c),
        (0x0a, 0x0c), // 16-bit Philips, ADC and DAC enabled.
        (0x13, 0x10),
        (0x1b, 0x0a),
        (0x1c, 0x6a),
        (0x44, 0x58),
        (0x17, 0xbf), // ADC digital volume = 0 dB (NOT register 0x14).
        (0x0e, 0x02),
        (0x12, 0x00),
        (0x14, 0x1a), // Analog mic selected; digital-mic enable bit stays clear.
        (0x0d, 0x01),
        (0x15, 0x40),
        (0x16, 0x03), // 18 dB analog gain.
        (0x37, 0x08),
        (0x45, 0x00),
        (0x32, 0xbf), // DAC digital volume 0 dB; alert PCM is amplitude-limited.
    ] {
        w(reg, value)?;
    }
    thread::sleep(Duration::from_millis(50));
    for (reg, expected) in [
        (0x00, 0x80),
        (0x01, 0x3f),
        (0x09, 0x0c),
        (0x0a, 0x0c),
        (0x14, 0x1a),
        (0x16, 0x03),
    ] {
        if r(reg)? != expected {
            return Err("ES8311 configuration readback mismatch");
        }
    }
    println!("[ES8311] Verified 48kHz, 16-bit analog mic, 18dB gain, shared ADC/DAC clocks");
    Ok(())
}
