// ============================================================
// baye-rust Hardware Drivers
// ============================================================
// Custom Rust drivers for each peripheral on the Waveshare
// ESP32-S3-Touch-AMOLED-1.8 board. Each module wraps raw I2C/SPI
// register access in safe, typed Rust interfaces.
//
// All I2C devices share one bus via Arc<Mutex<I2cDriver>>.
// The bus is initialized once in main.rs and passed to each driver.
// ============================================================

pub mod axp2101; // AXP2101 PMIC (battery voltage + charging status)
pub mod es8311;
pub mod i2c_bus;
pub mod qmi8658; // 6-axis IMU (accel + gyro) for tilt + sickness
pub mod sd_card;
pub mod sh8601; // Phase 1: QSPI AMOLED display
pub mod tca9554; // SD card (SPI3 + TCA9554 EXIO7 CS) for .m8 persistence
                 // pub mod ft3168;     // Phase 2: Capacitive touch

pub mod es7210;
