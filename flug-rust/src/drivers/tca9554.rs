// ============================================================
// TCA9554 I/O Expander Driver
// ============================================================
// The TCA9554 (I2C @ 0x20) controls reset lines for various
// peripherals on this board. We MUST toggle these during boot
// to properly reset the touch controller, display, and audio.
//
// Expander pins 0, 1, 2 are used as reset outputs.
// Sequence: drive LOW (reset), wait 20ms, drive HIGH (enable).
//
// Without this, touch and possibly audio won't work!
//
// Ported from: MiniMem8/src/main.cpp::init_io_expander()
// ============================================================

use std::thread;
use std::time::Duration;

use super::i2c_bus::{self, SharedI2cBus};
use crate::config;

/// TCA9554 register addresses
const REG_OUTPUT: u8 = 0x01; // Output port register
const REG_CONFIG: u8 = 0x03; // Configuration register (0 = output, 1 = input)

/// Initialize the TCA9554 I/O expander and reset all peripherals.
///
/// This toggles pins 0-2 LOW then HIGH to reset:
///   - Touch controller (FT3168)
///   - Display controller (SH8601)
///   - Any other peripheral on the reset bus
///
/// Returns Ok(()) on success, or an error if the device isn't found.
pub fn init_and_reset(bus: &SharedI2cBus) -> Result<(), &'static str> {
    if config::BOARD_206 {
        // Waveshare 2.06 has dedicated LCD and touch reset GPIOs, no expander.
        for pin in [config::PIN_LCD_RST, 9] {
            unsafe {
                esp_idf_sys::gpio_set_direction(pin, esp_idf_sys::gpio_mode_t_GPIO_MODE_OUTPUT);
                esp_idf_sys::gpio_set_level(pin, 0);
            }
        }
        thread::sleep(Duration::from_millis(20));
        for pin in [config::PIN_LCD_RST, 9] {
            unsafe {
                esp_idf_sys::gpio_set_level(pin, 1);
            }
        }
        thread::sleep(Duration::from_millis(150));
        return Ok(());
    }
    println!("[Boot] Initializing TCA9554 I/O expander...");

    // Check if the TCA9554 is present on the bus
    if !i2c_bus::probe(bus, config::TCA9554_I2C_ADDR) {
        return Err("TCA9554 not found! Peripherals may not reset properly.");
    }

    // Set pins 0-2 as outputs (0 = output in config register)
    // Pins 3-7 remain as inputs (1)
    // 0xF8 = 1111_1000 -> pins 0,1,2 = output, pins 3-7 = input
    i2c_bus::write_reg(bus, config::TCA9554_I2C_ADDR, REG_CONFIG, 0xF8)
        .map_err(|_| "Failed to configure TCA9554 pin directions")?;

    // Drive pins 0-2 LOW (assert reset on all peripherals)
    i2c_bus::write_reg(bus, config::TCA9554_I2C_ADDR, REG_OUTPUT, 0x00)
        .map_err(|_| "Failed to assert TCA9554 reset")?;

    // Hold reset for 20ms
    thread::sleep(Duration::from_millis(20));

    // Drive pins 0-2 HIGH (release reset, enable peripherals)
    // 0x07 = 0000_0111 -> pins 0,1,2 HIGH
    i2c_bus::write_reg(bus, config::TCA9554_I2C_ADDR, REG_OUTPUT, 0x07)
        .map_err(|_| "Failed to release TCA9554 reset")?;

    // Give peripherals time to wake up after reset
    thread::sleep(Duration::from_millis(50));

    println!("[Boot] TCA9554 ready - peripherals reset and enabled.");
    Ok(())
}
