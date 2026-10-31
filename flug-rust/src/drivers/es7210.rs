//! Waveshare 2.06 front-left/front-right microphones, standard stereo I2S.
//! Register sequence checked against Espressif esp-adf
//! daa4fbebaca3bfed4619f0001b3703f740e92a7b / esp_codec_dev/device/es7210.
//! Rear microphone/TDM remains outside this initial two-channel profile.
use super::i2c_bus::{self, SharedI2cBus};
use std::{thread, time::Duration};

pub fn init(bus: &SharedI2cBus) -> Result<(), &'static str> {
    const ADDR: u8 = 0x40;
    if !i2c_bus::probe(bus, ADDR) {
        return Err("ES7210 not found");
    }
    let w = |reg, value| i2c_bus::write_reg(bus, ADDR, reg, value).map_err(|_| "ES7210 write");
    let r = |reg| i2c_bus::read_reg(bus, ADDR, reg).map_err(|_| "ES7210 read");
    w(0x00, 0xff)?;
    thread::sleep(Duration::from_millis(10));
    for (reg, value) in [
        (0x00, 0x41),
        (0x01, 0x3f),
        (0x09, 0x30),
        (0x0a, 0x30),
        (0x23, 0x2a),
        (0x22, 0x0a),
        (0x20, 0x0a),
        (0x21, 0x2a),
        (0x08, 0x00), // slave, clocked by ESP32 I2S0
        (0x40, 0x43),
        (0x41, 0x70),
        (0x42, 0x70),
        (0x07, 0x20),
        (0x02, 0xc1),
        (0x11, 0x60), // 16-bit Philips standard
        (0x12, 0x00), // stereo, not TDM
        (0x01, 0x34), // enable mic1+2 clock domains
        (0x06, 0x00),
        (0x47, 0x08),
        (0x48, 0x08),
        (0x49, 0xff),
        (0x4a, 0xff),
        (0x4b, 0x00),
        (0x4c, 0xff),
        (0x43, 0x16),
        (0x44, 0x16), // mic1+2 enabled, 18 dB
        (0x45, 0x00),
        (0x46, 0x00),
        (0x40, 0x43),
        (0x00, 0x71),
        (0x00, 0x41),
    ] {
        w(reg, value)?;
    }
    thread::sleep(Duration::from_millis(50));
    for (reg, value) in [
        (0x11, 0x60),
        (0x12, 0),
        (0x43, 0x16),
        (0x44, 0x16),
        (0x4b, 0),
    ] {
        if r(reg)? != value {
            return Err("ES7210 readback mismatch");
        }
    }
    println!("[ES7210] Front L/R ADC configured: 48kHz, 16-bit stereo, 18dB; rear mic disabled");
    Ok(())
}
