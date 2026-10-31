// ============================================================
// QMI8658 6-Axis IMU Driver (I2C)
// ============================================================
// The QMI8658 provides 3-axis accelerometer and 3-axis gyroscope
// on the Waveshare ESP32-S3-Touch-AMOLED-1.8 board.
//
// We use it for two things:
//   1. Accelerometer gravity vector → tilt detection → slice selection
//      (tilting the device scrolls through 3D wave pool depth slices)
//   2. Gyroscope angular velocity → motion sickness detection
//      (shaking Baye makes the Aye sick!)
//
// Settings:
//   - Accel: ±8g, 30Hz ODR (matches our 30fps display rate)
//   - Gyro: ±2000 dps, 30Hz ODR
//
// Data is burst-read: 12 bytes starting at 0x35 gives us
// accel XYZ (6 bytes) + gyro XYZ (6 bytes) in one transaction.
//
// Ported from the Waveshare QMI8658 Arduino example + datasheet.
// ============================================================

use super::i2c_bus::{self, SharedI2cBus};
use crate::config;

// QMI8658 Register Addresses
const REG_WHO_AM_I: u8 = 0x00; // Device identification (should be 0x05)
const REG_CTRL1: u8 = 0x02; // SPI/I2C interface, sensor enable
const REG_CTRL2: u8 = 0x03; // Accelerometer: ODR + full-scale
const REG_CTRL3: u8 = 0x04; // Gyroscope: ODR + full-scale
const REG_CTRL5: u8 = 0x06; // Low-pass filter configuration
const REG_CTRL7: u8 = 0x08; // Enable accel + gyro
const REG_ACCEL_X_L: u8 = 0x35; // Start of 12-byte burst read

// Expected WHO_AM_I value for QMI8658
const EXPECTED_WHO_AM_I: u8 = 0x05;

/// Raw IMU reading: 3-axis accel (raw i16) + 3-axis gyro (raw i16).
///
/// Conversion factors (with our settings):
///   - Accel: raw / 4096 = g  (±8g range, 16-bit signed)
///   - Gyro:  raw / 16.4 = dps (±2000 dps range, 16-bit signed)
#[derive(Debug, Clone, Copy, Default)]
pub struct ImuReading {
    pub accel_x: i16,
    pub accel_y: i16,
    pub accel_z: i16,
    pub gyro_x: i16,
    pub gyro_y: i16,
    pub gyro_z: i16,
}

impl ImuReading {
    /// Calculate gyroscope angular velocity magnitude in degrees/sec (pure integer, zero floats!).
    /// Used for motion sickness detection: `isqrt_u32(gx^2 + gy^2 + gz^2) * 10 / 164`.
    pub fn gyro_magnitude_dps(&self) -> u16 {
        let gx = self.gyro_x as i32;
        let gy = self.gyro_y as i32;
        let gz = self.gyro_z as i32;
        let raw_mag_sq = (gx * gx + gy * gy + gz * gz).max(0) as u32;
        ((flug_core::triad_flock::isqrt_u32(raw_mag_sq) * 10) / 164) as u16
    }

    /// Calculate pitch angle from accelerometer gravity vector in integer degrees (`-90..=+90`).
    pub fn pitch_deg(&self) -> i16 {
        let ay = (self.accel_y as i32) * (config::TILT_AXIS_SIGN as i32);
        let az = self.accel_z as i32;
        flug_core::triad_flock::atan2_deg_i16(ay, az)
    }

    /// Calculate roll angle from accelerometer gravity vector in integer degrees (`-180..=+180`).
    pub fn roll_deg(&self) -> i16 {
        let ax = (self.accel_x as i32) * (config::TILT_AXIS_SIGN as i32);
        let az = self.accel_z as i32;
        flug_core::triad_flock::atan2_deg_i16(ax, az)
    }
}

/// Initialize the QMI8658 IMU.
///
/// Configures accelerometer (±8g, 30Hz) and gyroscope (±2000dps, 30Hz).
/// Returns Ok(()) if WHO_AM_I matches, Err otherwise.
pub fn init(bus: &SharedI2cBus) -> Result<(), &'static str> {
    println!("[IMU] Initializing QMI8658...");

    // Probe the device on I2C bus
    if !i2c_bus::probe(bus, config::QMI8658_I2C_ADDR) {
        return Err("QMI8658 not found on I2C bus!");
    }

    // Verify WHO_AM_I
    let who_am_i = i2c_bus::read_reg(bus, config::QMI8658_I2C_ADDR, REG_WHO_AM_I)
        .map_err(|_| "Failed to read QMI8658 WHO_AM_I")?;
    println!(
        "[IMU] WHO_AM_I: 0x{:02X} (expected 0x{:02X})",
        who_am_i, EXPECTED_WHO_AM_I
    );

    if who_am_i != EXPECTED_WHO_AM_I {
        return Err("QMI8658 WHO_AM_I mismatch!");
    }

    let addr = config::QMI8658_I2C_ADDR;
    let w = |reg: u8, val: u8| -> Result<(), &'static str> {
        i2c_bus::write_reg(bus, addr, reg, val).map_err(|_| "QMI8658 register write failed")
    };

    // CTRL1: I2C mode, auto-increment address, no big-endian swap
    w(REG_CTRL1, 0x40)?;

    // CTRL2: Accelerometer config
    // Bits [6:4] = Full-scale: 010 = ±8g
    // Bits [3:0] = ODR: 0011 = 30Hz (close to our 30fps display)
    w(REG_CTRL2, 0x23)?;

    // CTRL3: Gyroscope config
    // Bits [6:4] = Full-scale: 101 = ±2000 dps
    // Bits [3:0] = ODR: 0011 = 30Hz
    w(REG_CTRL3, 0x53)?;

    // CTRL5: Low-pass filter
    // Enable LPF for both accel and gyro, moderate bandwidth
    w(REG_CTRL5, 0x11)?;

    // CTRL7: Enable both sensors
    // Bit 0 = accel enable, Bit 1 = gyro enable
    w(REG_CTRL7, 0x03)?;

    // Give sensors time to start up and produce valid data
    std::thread::sleep(std::time::Duration::from_millis(30));

    println!("[IMU] QMI8658 ready: accel ±8g, gyro ±2000dps, 30Hz");
    Ok(())
}

/// Read all 6 axes in a single I2C burst transaction (12 bytes).
///
/// This is called once per frame (30fps). The burst read ensures
/// all axes are sampled at the same instant (no skew).
pub fn read(bus: &SharedI2cBus) -> Result<ImuReading, &'static str> {
    let mut buf = [0u8; 12];
    i2c_bus::read_regs(bus, config::QMI8658_I2C_ADDR, REG_ACCEL_X_L, &mut buf)
        .map_err(|_| "QMI8658 burst read failed")?;

    // Data is little-endian i16: [low, high] for each axis
    // Order: accel_x, accel_y, accel_z, gyro_x, gyro_y, gyro_z
    Ok(ImuReading {
        accel_x: i16::from_le_bytes([buf[0], buf[1]]),
        accel_y: i16::from_le_bytes([buf[2], buf[3]]),
        accel_z: i16::from_le_bytes([buf[4], buf[5]]),
        gyro_x: i16::from_le_bytes([buf[6], buf[7]]),
        gyro_y: i16::from_le_bytes([buf[8], buf[9]]),
        gyro_z: i16::from_le_bytes([buf[10], buf[11]]),
    })
}
