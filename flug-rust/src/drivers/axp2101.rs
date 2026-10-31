//! AXP2101 battery monitor. Leaves charge current/voltage configuration unchanged.
//! Register interpretation follows XPowersLib d6997586e68f65afd51baa775903df930db39821.
use super::i2c_bus::{self, SharedI2cBus};
use crate::{config, power_policy::BatteryReading};

pub fn init(bus: &SharedI2cBus) -> Result<(), &'static str> {
    let addr = config::AXP2101_I2C_ADDR;
    let id = i2c_bus::read_reg(bus, addr, 0x03).map_err(|_| "PMIC ID read failed")?;
    if id != 0x4a {
        return Err("Unexpected PMIC ID; no writes performed");
    }
    let adc = i2c_bus::read_reg(bus, addr, 0x30).map_err(|_| "PMIC ADC read failed")?;
    i2c_bus::write_reg(bus, addr, 0x30, adc | 1).map_err(|_| "PMIC ADC enable failed")?;
    Ok(())
}

pub fn read(bus: &SharedI2cBus) -> Result<BatteryReading, &'static str> {
    let addr = config::AXP2101_I2C_ADDR;
    let mut status = [0; 2];
    let mut voltage = [0; 2];
    i2c_bus::read_regs(bus, addr, 0x00, &mut status).map_err(|_| "PMIC status read failed")?;
    i2c_bus::read_regs(bus, addr, 0x34, &mut voltage).map_err(|_| "PMIC voltage read failed")?;
    let soc = i2c_bus::read_reg(bus, addr, 0xa4).map_err(|_| "PMIC gauge read failed")?;
    Ok(BatteryReading::from_registers(
        status[0], status[1], voltage[0], voltage[1], soc,
    ))
}
