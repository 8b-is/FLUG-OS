// ============================================================
// Shared I2C Bus Abstraction
// ============================================================
// All peripherals on this board share ONE I2C bus:
//   FT3168 (0x38), ES8311 (0x18), QMI8658 (0x6B),
//   PCF85063 (0x51), AXP2101 (0x34), TCA9554 (0x20)
//
// We wrap the I2C driver in Arc<Mutex<>> so multiple modules
// (and potentially multiple tasks) can safely share it.
//
// In practice, contention is near-zero: ES8311 only uses I2C
// during init (audio flows via I2S DMA after that), and touch
// polling happens only on Core 1.
// ============================================================

use esp_idf_hal::gpio::AnyIOPin;
use esp_idf_hal::i2c::{I2cConfig, I2cDriver};
use esp_idf_hal::prelude::*;
use std::sync::{Arc, Mutex};

use crate::config;

// A missing peripheral must not stall battery policy and the main loop forever.
const IO_TIMEOUT: esp_idf_sys::TickType_t =
    esp_idf_hal::delay::TickType::new_millis(100).ticks();

/// The shared I2C bus type. Clone the Arc to share across modules.
pub type SharedI2cBus<'a> = Arc<Mutex<I2cDriver<'a>>>;

/// Initialize the I2C bus with our board's pin configuration.
/// Returns a shared, thread-safe reference to the I2C driver.
///
/// # Safety
/// Must be called exactly once during boot, before any I2C device access.
pub fn init(
    sda: AnyIOPin,
    scl: AnyIOPin,
    i2c: esp_idf_hal::i2c::I2C0,
) -> Result<SharedI2cBus<'static>, esp_idf_hal::i2c::I2cError> {
    let i2c_config = I2cConfig::new().baudrate(config::I2C_FREQ_HZ.Hz());

    let driver = I2cDriver::new(i2c, sda, scl, &i2c_config)?;

    Ok(Arc::new(Mutex::new(driver)))
}

/// Write a single register on an I2C device.
/// This is the bread-and-butter operation for all our peripherals.
pub fn write_reg(
    bus: &SharedI2cBus,
    addr: u8,
    reg: u8,
    value: u8,
) -> Result<(), esp_idf_hal::i2c::I2cError> {
    let mut i2c = bus.lock().expect("I2C bus lock poisoned");
    i2c.write(addr, &[reg, value], IO_TIMEOUT)?;
    Ok(())
}

/// Read a single register from an I2C device.
pub fn read_reg(bus: &SharedI2cBus, addr: u8, reg: u8) -> Result<u8, esp_idf_hal::i2c::I2cError> {
    let mut i2c = bus.lock().expect("I2C bus lock poisoned");
    let mut buf = [0u8; 1];
    i2c.write_read(addr, &[reg], &mut buf, IO_TIMEOUT)?;
    Ok(buf[0])
}

/// Read multiple registers from an I2C device (burst read).
pub fn read_regs(
    bus: &SharedI2cBus,
    addr: u8,
    reg: u8,
    buf: &mut [u8],
) -> Result<(), esp_idf_hal::i2c::I2cError> {
    let mut i2c = bus.lock().expect("I2C bus lock poisoned");
    i2c.write_read(addr, &[reg], buf, IO_TIMEOUT)?;
    Ok(())
}

/// Check if a device is present on the I2C bus by attempting a write.
/// Returns true if the device ACKs.
pub fn probe(bus: &SharedI2cBus, addr: u8) -> bool {
    let mut i2c = bus.lock().expect("I2C bus lock poisoned");
    i2c.write(addr, &[], IO_TIMEOUT).is_ok()
}
