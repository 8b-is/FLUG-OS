// ============================================================
// SD Card SPI Driver (TCA9554 EXIO7 Chip Select)
// ============================================================
// Drives a 64GB TF/microSD card via SPI3_HOST on GPIO 1/2/3.
//
// CS is on EXIO7 of the TCA9554 I2C expander (bit 7) — NOT a
// real GPIO. This means we can't use ESP-IDF's sdspi_host, which
// expects a GPIO for CS. Instead we do raw SPI with manual CS
// toggling via I2C writes to the TCA9554 output register.
//
// Performance: ~10-15ms per 4KB block (dominated by I2C CS
// toggle overhead of ~200us per toggle, 2 per sector).
//
// SD SPI protocol reference:
//   - SD Simplified Specification Part 1 (Physical Layer)
//   - Section 7.2: SPI Mode
//
// Ported from: MiniMem8 SD card init (C++) + custom SPI path
// ============================================================

use esp_idf_sys::*;
use std::ptr;
use std::thread;
use std::time::Duration;

use super::i2c_bus::{self, SharedI2cBus};
use crate::config;

// ============================================================
// TCA9554 Registers (for CS toggle)
// ============================================================
const TCA9554_REG_OUTPUT: u8 = 0x01;
const TCA9554_REG_CONFIG: u8 = 0x03;

// ============================================================
// SD SPI Commands
// ============================================================
const CMD0_GO_IDLE: u8 = 0;
const CMD8_SEND_IF_COND: u8 = 8;
const CMD9_SEND_CSD: u8 = 9;
const CMD16_SET_BLOCKLEN: u8 = 16;
const CMD17_READ_SINGLE_BLOCK: u8 = 17;
const CMD24_WRITE_BLOCK: u8 = 24;
const CMD55_APP_CMD: u8 = 55;
const CMD58_READ_OCR: u8 = 58;
const ACMD41_SD_SEND_OP_COND: u8 = 41;

// R1 response bits
const R1_IDLE: u8 = 0x01;
const R1_ILLEGAL_CMD: u8 = 0x04;

// Data tokens
const DATA_START_TOKEN: u8 = 0xFE;
const DATA_ACCEPTED: u8 = 0x05;

/// SD card driver using SPI3_HOST with TCA9554 I2C chip select.
///
/// Provides 4KB block-level I/O for .m8 storage.
/// The DMA buffer lives in internal SRAM (required for DMA).
pub struct SdCard {
    /// SPI device handle on SPI3_HOST
    spi_handle: spi_device_handle_t,
    /// 512-byte DMA buffer in internal SRAM
    dma_buf: *mut u8,
    /// Shared I2C bus for TCA9554 CS toggling
    i2c_bus: SharedI2cBus<'static>,
    /// Cached TCA9554 output register state (we OR/AND EXIO7)
    tca_output_state: u8,
    /// Card capacity in 512-byte sectors
    sector_count: u64,
    /// True if card is SDHC/SDXC (block-addressed)
    is_sdhc: bool,
}

// Safety: SdCard is only used from the main task (Core 1)
unsafe impl Send for SdCard {}

impl SdCard {
    /// Initialize the SD card on SPI3_HOST with TCA9554 EXIO7 as CS.
    ///
    /// Full SD SPI init sequence:
    /// 1. Configure EXIO7 as output (CS), set HIGH
    /// 2. Init SPI bus at 400kHz (spec requirement)
    /// 3. Send 80+ clock pulses with CS HIGH (enter SPI mode)
    /// 4. CMD0 -> idle state
    /// 5. CMD8 -> detect SD v2
    /// 6. ACMD41 loop -> wait for card ready
    /// 7. CMD58 -> check SDHC/SDXC
    /// 8. CMD9 -> read capacity
    /// 9. Switch to 20MHz
    pub fn init(i2c_bus: SharedI2cBus<'static>) -> Result<Self, &'static str> {
        println!("[SD] Initializing SD card on SPI3...");

        // --- Step 1: Configure TCA9554 EXIO7 as output, set HIGH (CS deasserted) ---
        let tca_output_state = if config::PIN_SD_CS >= 0 {
            unsafe {
                if gpio_set_direction(config::PIN_SD_CS, gpio_mode_t_GPIO_MODE_OUTPUT) != ESP_OK
                    || gpio_set_level(config::PIN_SD_CS, 1) != ESP_OK
                {
                    return Err("SD GPIO chip select failed");
                }
            }
            0
        } else {
            // Read current config register, clear bit 7 (= output)
            let cfg = i2c_bus::read_reg(&i2c_bus, config::TCA9554_I2C_ADDR, TCA9554_REG_CONFIG)
                .map_err(|_| "SD: Failed to read TCA9554 config")?;
            let new_cfg = cfg & !config::TCA9554_EXIO7_BIT; // Clear bit 7 = output
            i2c_bus::write_reg(
                &i2c_bus,
                config::TCA9554_I2C_ADDR,
                TCA9554_REG_CONFIG,
                new_cfg,
            )
            .map_err(|_| "SD: Failed to configure TCA9554 EXIO7 as output")?;

            // Read current output state, set bit 7 HIGH (CS deasserted)
            let out = i2c_bus::read_reg(&i2c_bus, config::TCA9554_I2C_ADDR, TCA9554_REG_OUTPUT)
                .map_err(|_| "SD: Failed to read TCA9554 output")?;
            let state = out | config::TCA9554_EXIO7_BIT;
            i2c_bus::write_reg(
                &i2c_bus,
                config::TCA9554_I2C_ADDR,
                TCA9554_REG_OUTPUT,
                state,
            )
            .map_err(|_| "SD: Failed to set EXIO7 HIGH")?;

            println!("[SD] TCA9554 EXIO7 configured as CS (HIGH)");

            state
        };
        // --- Step 2: Initialize SPI bus on SPI3_HOST at 400kHz ---
        let bus_config = spi_bus_config_t {
            __bindgen_anon_1: spi_bus_config_t__bindgen_ty_1 {
                mosi_io_num: config::PIN_SD_MOSI,
            },
            __bindgen_anon_2: spi_bus_config_t__bindgen_ty_2 {
                miso_io_num: config::PIN_SD_MISO,
            },
            sclk_io_num: config::PIN_SD_SCLK,
            __bindgen_anon_3: spi_bus_config_t__bindgen_ty_3 { quadwp_io_num: -1 },
            __bindgen_anon_4: spi_bus_config_t__bindgen_ty_4 { quadhd_io_num: -1 },
            data4_io_num: -1,
            data5_io_num: -1,
            data6_io_num: -1,
            data7_io_num: -1,
            max_transfer_sz: config::SD_SECTOR_SIZE as i32 + 16,
            flags: SPICOMMON_BUSFLAG_MASTER | SPICOMMON_BUSFLAG_GPIO_PINS,
            isr_cpu_id: esp_intr_cpu_affinity_t_ESP_INTR_CPU_AFFINITY_AUTO,
            intr_flags: 0,
        };

        unsafe {
            let ret = spi_bus_initialize(
                spi_host_device_t_SPI3_HOST,
                &bus_config,
                spi_common_dma_t_SPI_DMA_CH_AUTO,
            );
            if ret != ESP_OK {
                return Err("SD: SPI3 bus init failed");
            }
        }

        // Add SPI device at 400kHz (init speed)
        let dev_config = spi_device_interface_config_t {
            command_bits: 0,
            address_bits: 0,
            dummy_bits: 0,
            mode: 0, // SPI Mode 0
            clock_source: soc_periph_spi_clk_src_t_SPI_CLK_SRC_DEFAULT,
            duty_cycle_pos: 0,
            cs_ena_pretrans: 0,
            cs_ena_posttrans: 0,
            clock_speed_hz: config::SD_SPI_FREQ_INIT,
            input_delay_ns: 0,
            spics_io_num: -1, // Manual CS via TCA9554
            flags: 0,         // Full-duplex for SD SPI
            queue_size: 1,
            pre_cb: None,
            post_cb: None,
        };

        let mut spi_handle: spi_device_handle_t = ptr::null_mut();
        unsafe {
            let ret = spi_bus_add_device(spi_host_device_t_SPI3_HOST, &dev_config, &mut spi_handle);
            if ret != ESP_OK {
                return Err("SD: SPI3 device add failed");
            }
            spi_device_acquire_bus(spi_handle, u32::MAX);
        }

        // Allocate 512-byte DMA buffer in internal SRAM
        let dma_buf = unsafe {
            heap_caps_aligned_alloc(4, config::SD_SECTOR_SIZE, MALLOC_CAP_DMA) as *mut u8
        };
        if dma_buf.is_null() {
            return Err("SD: DMA buffer allocation failed");
        }

        println!(
            "[SD] SPI3 bus ready (400kHz init, pins MOSI={} SCLK={} MISO={})",
            config::PIN_SD_MOSI,
            config::PIN_SD_SCLK,
            config::PIN_SD_MISO
        );

        let mut sd = SdCard {
            spi_handle,
            dma_buf,
            i2c_bus,
            tca_output_state,
            sector_count: 0,
            is_sdhc: false,
        };

        // --- Step 3: Send 80+ clock pulses with CS HIGH (enter SPI mode) ---
        // SD spec requires >=74 clock cycles with CS HIGH before first command
        sd.send_clocks(80)?;

        // --- Step 4: CMD0 (GO_IDLE_STATE) ---
        // Should return R1 = 0x01 (idle)
        let r1 = sd.send_cmd(CMD0_GO_IDLE, 0)?;
        if r1 != R1_IDLE {
            println!("[SD] CMD0 failed: R1=0x{:02X}", r1);
            return Err("SD: CMD0 failed (card not responding)");
        }
        println!("[SD] CMD0: card in idle state");

        // --- Step 5: CMD8 (SEND_IF_COND) -> detect SD v2 ---
        // Argument 0x1AA: voltage range 2.7-3.6V, check pattern 0xAA
        let r1 = sd.send_cmd(CMD8_SEND_IF_COND, 0x0000_01AA)?;
        let is_v2 = if r1 == R1_IDLE {
            // Read 4-byte R7 response
            let r7 = sd.read_r7()?;
            if (r7 & 0xFFF) != 0x1AA {
                return Err("SD: CMD8 check pattern mismatch");
            }
            println!("[SD] CMD8: SD v2 card detected");
            true
        } else if r1 == (R1_IDLE | R1_ILLEGAL_CMD) {
            println!("[SD] CMD8: SD v1 card (no SDHC)");
            false
        } else {
            return Err("SD: CMD8 unexpected response");
        };

        // --- Step 6: ACMD41 loop (SD_SEND_OP_COND) ---
        // HCS bit (bit 30) set for v2 cards to indicate we support SDHC
        let hcs_bit: u32 = if is_v2 { 0x4000_0000 } else { 0 };
        let mut retries = 500; // ~5 seconds at 10ms per retry
        loop {
            // CMD55 prefix (next command is application-specific)
            sd.send_cmd(CMD55_APP_CMD, 0)?;
            let r1 = sd.send_cmd(ACMD41_SD_SEND_OP_COND, hcs_bit)?;

            if r1 == 0x00 {
                println!("[SD] ACMD41: card ready");
                break;
            }
            if r1 != R1_IDLE {
                println!("[SD] ACMD41 unexpected: R1=0x{:02X}", r1);
                return Err("SD: ACMD41 failed");
            }

            retries -= 1;
            if retries == 0 {
                return Err("SD: ACMD41 timeout (card not initializing)");
            }
            thread::sleep(Duration::from_millis(10));
        }

        // --- Step 7: CMD58 (READ_OCR) -> check SDHC/SDXC ---
        let r1 = sd.send_cmd(CMD58_READ_OCR, 0)?;
        if r1 != 0x00 {
            return Err("SD: CMD58 failed");
        }
        let ocr = sd.read_r7()?; // OCR is also 4 bytes
        sd.is_sdhc = is_v2 && (ocr & 0x4000_0000) != 0;
        println!("[SD] CMD58: OCR=0x{:08X} SDHC={}", ocr, sd.is_sdhc);

        // For non-SDHC cards, set block length to 512
        if !sd.is_sdhc {
            let r1 = sd.send_cmd(CMD16_SET_BLOCKLEN, 512)?;
            if r1 != 0x00 {
                return Err("SD: CMD16 failed (can't set block length)");
            }
        }

        // --- Step 8: CMD9 (SEND_CSD) -> read card capacity ---
        sd.sector_count = sd.read_capacity()?;
        let size_gb = (sd.sector_count * config::SD_SECTOR_SIZE as u64) / (1024 * 1024 * 1024);
        let m8_blocks = sd.m8_block_count();
        println!(
            "[SD] Card capacity: {}GB ({} sectors, {} .m8 blocks)",
            size_gb, sd.sector_count, m8_blocks
        );

        // --- Step 9: Switch to 20MHz ---
        // Remove device at 400kHz, re-add at 20MHz
        unsafe {
            spi_device_release_bus(sd.spi_handle);
            spi_bus_remove_device(sd.spi_handle);
        }

        let fast_config = spi_device_interface_config_t {
            clock_speed_hz: config::SD_SPI_FREQ_NORMAL,
            ..dev_config
        };

        unsafe {
            let ret = spi_bus_add_device(
                spi_host_device_t_SPI3_HOST,
                &fast_config,
                &mut sd.spi_handle,
            );
            if ret != ESP_OK {
                return Err("SD: Failed to switch to 20MHz");
            }
            spi_device_acquire_bus(sd.spi_handle, u32::MAX);
        }

        println!("[SD] Switched to 20MHz. SD card ready!");
        Ok(sd)
    }

    /// Read-only, two-sector maximum inspection; no filesystem mount or writes.
    pub fn inspect_layout(&mut self) -> Result<(), &'static str> {
        let mut sector = [0u8; 512];
        self.read_probe_sector(0, &mut sector)?;
        println!(
            "[SD] Sector-zero header={:02x?}, signature={:02x?}",
            &sector[..16],
            &sector[510..]
        );
        let hint = crate::storage_probe::inspect(&sector, self.sector_count);
        println!(
            "[SD] Read-only layout hint: {:?}; capacity_bytes={}",
            hint,
            self.sector_count * 512
        );
        if let crate::storage_probe::Hint::PrimaryPartition { start, sectors } = hint {
            self.read_probe_sector(start, &mut sector)?;
            println!(
                "[SD] Primary partition hint: {:?}; writing disabled",
                crate::storage_probe::inspect(&sector, sectors)
            );
        }
        Ok(())
    }

    fn read_probe_sector(&mut self, index: u64, out: &mut [u8; 512]) -> Result<(), &'static str> {
        if index >= self.sector_count {
            return Err("Probe outside card capacity");
        }
        self.read_sector(index)?;
        unsafe {
            ptr::copy_nonoverlapping(self.dma_buf, out.as_mut_ptr(), 512);
        }
        Ok(())
    }

    /// Read one 4KB .m8 block (8 consecutive 512-byte sectors).
    ///
    /// `block_idx` is the .m8 block number (0 = header, 1+ = data).
    /// `buf` must be exactly 4096 bytes.
    pub fn read_m8_block(
        &mut self,
        block_idx: u64,
        buf: &mut [u8; config::M8_BLOCK_SIZE],
    ) -> Result<(), &'static str> {
        if block_idx >= self.m8_block_count() {
            return Err("Block outside card capacity");
        }
        let base_sector = block_idx * config::SECTORS_PER_M8_BLOCK as u64;

        for i in 0..config::SECTORS_PER_M8_BLOCK {
            let sector = base_sector + i as u64;
            self.read_sector(sector)?;

            // Copy from DMA buffer to output buffer
            let offset = i * config::SD_SECTOR_SIZE;
            unsafe {
                ptr::copy_nonoverlapping(
                    self.dma_buf,
                    buf.as_mut_ptr().add(offset),
                    config::SD_SECTOR_SIZE,
                );
            }
        }
        Ok(())
    }

    /// Write one 4KB .m8 block (8 consecutive 512-byte sectors).
    ///
    /// `block_idx` is the .m8 block number (0 = header, 1+ = data).
    /// `buf` must be exactly 4096 bytes.
    pub fn write_m8_block(
        &mut self,
        block_idx: u64,
        buf: &[u8; config::M8_BLOCK_SIZE],
    ) -> Result<(), &'static str> {
        if !config::ENABLE_RAW_SD_STORE {
            return Err("SD writes disabled during bring-up");
        }
        if block_idx >= self.m8_block_count() {
            return Err("Block outside card capacity");
        }
        let base_sector = block_idx * config::SECTORS_PER_M8_BLOCK as u64;

        for i in 0..config::SECTORS_PER_M8_BLOCK {
            let sector = base_sector + i as u64;

            // Copy from input buffer to DMA buffer
            let offset = i * config::SD_SECTOR_SIZE;
            unsafe {
                ptr::copy_nonoverlapping(
                    buf.as_ptr().add(offset),
                    self.dma_buf,
                    config::SD_SECTOR_SIZE,
                );
            }

            self.write_sector(sector)?;
        }
        Ok(())
    }

    /// Total capacity in 4KB .m8 blocks.
    pub fn m8_block_count(&self) -> u64 {
        self.sector_count / config::SECTORS_PER_M8_BLOCK as u64
    }

    // ============================================================
    // Low-Level SD SPI Operations
    // ============================================================

    /// Read a single 512-byte sector into the DMA buffer.
    fn read_sector(&mut self, sector: u64) -> Result<(), &'static str> {
        // SDHC uses block addressing, SDSC uses byte addressing
        let addr = if self.is_sdhc {
            sector as u32
        } else {
            (sector * 512) as u32
        };

        let r1 = self.send_cmd(CMD17_READ_SINGLE_BLOCK, addr)?;
        if r1 != 0x00 {
            return Err("SD: CMD17 read failed");
        }

        // Wait for data start token (0xFE)
        self.wait_for_token(DATA_START_TOKEN)?;

        // Read 512 bytes of data
        self.spi_recv(config::SD_SECTOR_SIZE)?;

        // Validate bytes before trusting filesystem signatures or writing.
        let expected_crc =
            u16::from(self.spi_transfer_byte(0xFF)) << 8 | u16::from(self.spi_transfer_byte(0xFF));
        let actual_crc = crate::storage_probe::crc16(unsafe {
            core::slice::from_raw_parts(self.dma_buf, config::SD_SECTOR_SIZE)
        });

        // Deassert CS + 8 clocks (SD spec: between commands)
        self.cs_high();
        self.spi_transfer_byte(0xFF);
        if expected_crc != actual_crc {
            return Err("SD read CRC mismatch; data not trusted");
        }

        Ok(())
    }

    /// Write a single 512-byte sector from the DMA buffer.
    fn write_sector(&mut self, sector: u64) -> Result<(), &'static str> {
        let addr = if self.is_sdhc {
            sector as u32
        } else {
            (sector * 512) as u32
        };

        let r1 = self.send_cmd(CMD24_WRITE_BLOCK, addr)?;
        if r1 != 0x00 {
            return Err("SD: CMD24 write failed");
        }

        // Send data start token
        self.spi_transfer_byte(DATA_START_TOKEN);

        // Send 512 bytes of data
        self.spi_send(config::SD_SECTOR_SIZE)?;

        let crc = crate::storage_probe::crc16(unsafe {
            core::slice::from_raw_parts(self.dma_buf, config::SD_SECTOR_SIZE)
        });
        self.spi_transfer_byte((crc >> 8) as u8);
        self.spi_transfer_byte(crc as u8);

        // Read data response token
        let response = self.spi_transfer_byte(0xFF);
        if (response & 0x1F) != DATA_ACCEPTED {
            return Err("SD: Write data not accepted");
        }

        // Wait for card to finish writing (busy = MISO LOW)
        self.wait_not_busy()?;

        // Deassert CS + 8 clocks (SD spec: between commands)
        self.cs_high();
        self.spi_transfer_byte(0xFF);

        Ok(())
    }

    /// Send an SD SPI command and return the R1 response byte.
    ///
    /// Protocol: CS LOW -> [0x40|cmd] [arg:4B] [CRC] -> wait R1 -> CS HIGH
    fn send_cmd(&mut self, cmd: u8, arg: u32) -> Result<u8, &'static str> {
        // For CMD0 and CMD8, we need correct CRC
        let crc = match cmd {
            CMD0_GO_IDLE => 0x95,      // Pre-computed CRC for CMD0(0)
            CMD8_SEND_IF_COND => 0x87, // Pre-computed CRC for CMD8(0x1AA)
            _ => 0x01,                 // Dummy CRC (ignored in SPI mode after init)
        };

        self.cs_low();

        // Send 6-byte command frame: [0x40|cmd, arg[3], arg[2], arg[1], arg[0], crc]
        self.spi_transfer_byte(0x40 | cmd);
        self.spi_transfer_byte((arg >> 24) as u8);
        self.spi_transfer_byte((arg >> 16) as u8);
        self.spi_transfer_byte((arg >> 8) as u8);
        self.spi_transfer_byte(arg as u8);
        self.spi_transfer_byte(crc);

        // Wait for response (MSB = 0 indicates valid R1)
        // SD spec says up to 8 bytes (NCR) of 0xFF before response
        for _ in 0..16 {
            let r = self.spi_transfer_byte(0xFF);
            if r & 0x80 == 0 {
                // Got valid R1 response
                // Don't deselect here - caller may need to read more data
                // (e.g., CMD8 R7, CMD17 data block)
                // Commands that have follow-up data keep CS asserted:
                // CMD17 (read data), CMD24 (write data), CMD9 (CSD),
                // CMD58 (OCR), CMD8 (R7 response).
                // All other commands deassert CS immediately.
                if cmd != CMD17_READ_SINGLE_BLOCK
                    && cmd != CMD24_WRITE_BLOCK
                    && cmd != CMD9_SEND_CSD
                    && cmd != CMD58_READ_OCR
                    && cmd != CMD8_SEND_IF_COND
                {
                    self.cs_high();
                    // Send 8 clocks after CS HIGH (SD spec)
                    self.spi_transfer_byte(0xFF);
                }
                return Ok(r);
            }
        }

        self.cs_high();
        self.spi_transfer_byte(0xFF);
        Err("SD: No response from card")
    }

    /// Read 4-byte response after CMD8 (R7) or CMD58 (OCR).
    fn read_r7(&mut self) -> Result<u32, &'static str> {
        let b3 = self.spi_transfer_byte(0xFF) as u32;
        let b2 = self.spi_transfer_byte(0xFF) as u32;
        let b1 = self.spi_transfer_byte(0xFF) as u32;
        let b0 = self.spi_transfer_byte(0xFF) as u32;

        self.cs_high();
        self.spi_transfer_byte(0xFF);

        Ok((b3 << 24) | (b2 << 16) | (b1 << 8) | b0)
    }

    /// Read card capacity from CSD register (CMD9).
    /// Returns total number of 512-byte sectors.
    fn read_capacity(&mut self) -> Result<u64, &'static str> {
        let r1 = self.send_cmd(CMD9_SEND_CSD, 0)?;
        if r1 != 0x00 {
            return Err("SD: CMD9 failed");
        }

        // Wait for data start token
        self.wait_for_token(DATA_START_TOKEN)?;

        // Read 16 bytes of CSD
        let mut csd = [0u8; 16];
        for byte in &mut csd {
            *byte = self.spi_transfer_byte(0xFF);
        }

        let expected_crc =
            u16::from(self.spi_transfer_byte(0xff)) << 8 | u16::from(self.spi_transfer_byte(0xff));
        self.cs_high();
        self.spi_transfer_byte(0xff);
        if crate::storage_probe::crc16(&csd) != expected_crc {
            return Err("SD capacity CRC mismatch");
        }

        // Parse CSD version
        let csd_ver = (csd[0] >> 6) & 0x03;

        let sectors = if csd_ver == 1 {
            // CSD v2.0 (SDHC/SDXC): C_SIZE is 22 bits in bytes 7-9
            let c_size = ((csd[7] & 0x3F) as u64) << 16 | (csd[8] as u64) << 8 | (csd[9] as u64);
            // Capacity = (C_SIZE + 1) * 512KB
            // In sectors: (C_SIZE + 1) * 1024
            (c_size + 1) * 1024
        } else {
            // CSD v1.0 (SDSC): compute from C_SIZE, C_SIZE_MULT, READ_BL_LEN
            let read_bl_len = (csd[5] & 0x0F) as u32;
            let c_size =
                (((csd[6] & 0x03) as u32) << 10) | ((csd[7] as u32) << 2) | ((csd[8] >> 6) as u32);
            let c_size_mult = (((csd[9] & 0x03) as u32) << 1) | ((csd[10] >> 7) as u32);
            let block_len = 1u64 << read_bl_len;
            let mult = 1u64 << (c_size_mult + 2);
            let capacity_bytes = (c_size as u64 + 1) * mult * block_len;
            capacity_bytes / 512
        };

        Ok(sectors)
    }

    /// Send clock pulses with CS HIGH (no data, just toggle SCLK).
    /// Used during initialization to put the card in SPI mode.
    fn send_clocks(&mut self, count: usize) -> Result<(), &'static str> {
        // CS must be HIGH during these clocks
        // Send 0xFF bytes (MOSI HIGH) — each byte = 8 clocks
        let bytes = count.div_ceil(8);
        for _ in 0..bytes {
            self.spi_transfer_byte(0xFF);
        }
        Ok(())
    }

    /// Wait for a specific token byte from the card (with timeout).
    fn wait_for_token(&mut self, token: u8) -> Result<(), &'static str> {
        for _ in 0..10000 {
            let r = self.spi_transfer_byte(0xFF);
            if r == token {
                return Ok(());
            }
            // 0xFF = card still thinking, anything else = error
            if r != 0xFF && r != token {
                return Err("SD: Unexpected token while waiting");
            }
        }
        Err("SD: Timeout waiting for data token")
    }

    /// Wait for the card to finish a write operation (busy = MISO LOW).
    fn wait_not_busy(&mut self) -> Result<(), &'static str> {
        for _ in 0..100_000 {
            let r = self.spi_transfer_byte(0xFF);
            if r == 0xFF {
                return Ok(()); // Card is ready
            }
        }
        Err("SD: Card busy timeout")
    }

    // ============================================================
    // SPI + CS Helpers
    // ============================================================

    /// Transfer one byte over SPI (full-duplex: send tx, return rx).
    ///
    /// Uses the inline 4-byte tx_data/rx_data buffers in the SPI
    /// transaction struct — does NOT touch the DMA buffer, so it's
    /// safe to call after spi_recv() without corrupting sector data.
    fn spi_transfer_byte(&mut self, tx: u8) -> u8 {
        unsafe {
            let mut tran: spi_transaction_t = std::mem::zeroed();
            tran.length = 8; // 1 byte = 8 bits
            tran.flags = SPI_TRANS_USE_TXDATA | SPI_TRANS_USE_RXDATA;
            tran.__bindgen_anon_1.tx_data[0] = tx;

            spi_device_polling_transmit(self.spi_handle, &mut tran);

            tran.__bindgen_anon_2.rx_data[0]
        }
    }

    /// Receive `len` bytes from SPI into the DMA buffer.
    /// Sends 0xFF on MOSI while reading.
    fn spi_recv(&mut self, len: usize) -> Result<(), &'static str> {
        // Fill DMA buffer with 0xFF (MOSI HIGH while reading)
        unsafe {
            ptr::write_bytes(self.dma_buf, 0xFF, len);
        }

        unsafe {
            let mut tran: spi_transaction_t = std::mem::zeroed();
            tran.length = len * 8;
            tran.__bindgen_anon_1.tx_buffer = self.dma_buf as *const _;
            tran.__bindgen_anon_2.rx_buffer = self.dma_buf as *mut _;

            let ret = spi_device_polling_transmit(self.spi_handle, &mut tran);
            if ret != ESP_OK {
                return Err("SD: SPI recv failed");
            }
        }
        Ok(())
    }

    /// Send `len` bytes from the DMA buffer over SPI.
    fn spi_send(&mut self, len: usize) -> Result<(), &'static str> {
        unsafe {
            let mut tran: spi_transaction_t = std::mem::zeroed();
            tran.length = len * 8;
            tran.__bindgen_anon_1.tx_buffer = self.dma_buf as *const _;
            tran.__bindgen_anon_2.rx_buffer = ptr::null_mut();

            let ret = spi_device_polling_transmit(self.spi_handle, &mut tran);
            if ret != ESP_OK {
                return Err("SD: SPI send failed");
            }
        }
        Ok(())
    }

    /// Assert CS (pull EXIO7 LOW via I2C write to TCA9554).
    fn cs_low(&mut self) {
        if config::PIN_SD_CS >= 0 {
            unsafe {
                gpio_set_level(config::PIN_SD_CS, 0);
            }
            return;
        }
        self.tca_output_state &= !config::TCA9554_EXIO7_BIT;
        let _ = i2c_bus::write_reg(
            &self.i2c_bus,
            config::TCA9554_I2C_ADDR,
            TCA9554_REG_OUTPUT,
            self.tca_output_state,
        );
    }

    /// Deassert CS (pull EXIO7 HIGH via I2C write to TCA9554).
    fn cs_high(&mut self) {
        if config::PIN_SD_CS >= 0 {
            unsafe {
                gpio_set_level(config::PIN_SD_CS, 1);
            }
            return;
        }
        self.tca_output_state |= config::TCA9554_EXIO7_BIT;
        let _ = i2c_bus::write_reg(
            &self.i2c_bus,
            config::TCA9554_I2C_ADDR,
            TCA9554_REG_OUTPUT,
            self.tca_output_state,
        );
    }
}

impl Drop for SdCard {
    fn drop(&mut self) {
        self.cs_high();
        unsafe {
            if !self.dma_buf.is_null() {
                heap_caps_free(self.dma_buf as *mut _);
            }
            spi_device_release_bus(self.spi_handle);
            spi_bus_remove_device(self.spi_handle);
            spi_bus_free(spi_host_device_t_SPI3_HOST);
        }
        println!("[SD] SPI3 bus freed");
    }
}
