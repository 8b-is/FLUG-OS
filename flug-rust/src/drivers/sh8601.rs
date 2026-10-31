// ============================================================
// SH8601 QSPI AMOLED Display Driver
// ============================================================
// Drives the 368x448 rectangular AMOLED on the Waveshare
// ESP32-S3-Touch-AMOLED-1.8 board.
//
// The SH8601 uses a QSPI protocol where:
//   - Commands use instruction byte 0x02 on 1 line
//   - Pixel data uses instruction byte 0x32 on 4 lines (QIO)
//   - 24-bit address field carries the actual display command
//   - RGB565 pixel format (16 bits per pixel)
//
// Framebuffer lives in PSRAM (322KB) and is blitted to the
// display via DMA in chunks of 1024 pixels per transfer.
//
// Ported from: Arduino_ESP32QSPI + Arduino_SH8601 in Arduino_GFX
// ============================================================

use esp_idf_sys::*;
use std::ptr;
use std::thread;
use std::time::Duration;

use crate::config;

// ============================================================
// Display Constants
// ============================================================

pub const WIDTH: u16 = config::LCD_WIDTH;
pub const HEIGHT: u16 = config::LCD_HEIGHT;
const FB_SIZE: usize = WIDTH as usize * HEIGHT as usize;
const FB_BYTES: usize = FB_SIZE * 2; // RGB565 = 2 bytes/pixel

// SH8601 Command Definitions
const CMD_SWRESET: u8 = 0x01;
const CMD_SLPOUT: u8 = 0x11;
const CMD_NORON: u8 = 0x13;
const CMD_INVOFF: u8 = 0x20;
const CMD_DISPON: u8 = 0x29;
const CMD_CASET: u8 = 0x2A;
const CMD_PASET: u8 = 0x2B;
const CMD_RAMWR: u8 = 0x2C;
const CMD_PIXFMT: u8 = 0x3A;
const CMD_BRIGHTNESS: u8 = 0x51;
const CMD_CTRL_DISPLAY: u8 = 0x53;
const CMD_CE: u8 = 0x58;

// QSPI protocol: instruction bytes sent before the actual command
const QSPI_WRITE_CMD: u8 = 0x02; // Single-line write instruction
const QSPI_QUAD_WRITE_CMD: u8 = 0x32; // Quad-line write instruction
const QSPI_DATA_ADDR: u32 = 0x003C00; // Address for pixel data (RAM write continue)

// Transfer limits (4096-pixel QSPI DMA chunks @ 80 MHz -> 4x fewer transactions, 2x clock)
const MAX_PIXELS_PER_CHUNK: usize = 4096;
const DMA_BUF_SIZE: usize = MAX_PIXELS_PER_CHUNK * 2; // bytes
const SPI_FREQ_HZ: i32 = 80_000_000;

// ============================================================
// Display Struct
// ============================================================

pub struct Display {
    spi_handle: spi_device_handle_t,
    cs_pin: i32,
    // DMA-capable buffers for SPI transfers (SRAM, aligned)
    dma_buf: *mut u8,
    // PSRAM framebuffer (368 * 448 * 2 = 329,728 bytes)
    pub framebuffer: *mut u16,
}

// Safety: Display is only used from the main task (Core 1)
unsafe impl Send for Display {}

impl Display {
    /// Initialize the SH8601 QSPI AMOLED display.
    ///
    /// Sets up the SPI bus in quad mode, runs the SH8601 init sequence,
    /// and allocates the PSRAM framebuffer.
    pub fn init() -> Result<Self, &'static str> {
        println!(
            "[Display] Initializing SH8601 QSPI AMOLED ({}x{})...",
            WIDTH, HEIGHT
        );

        let cs_pin = config::PIN_LCD_CS;
        let clk_pin = config::PIN_LCD_CLK;
        let d0_pin = config::PIN_LCD_D0;
        let d1_pin = config::PIN_LCD_D1;
        let d2_pin = config::PIN_LCD_D2;
        let d3_pin = config::PIN_LCD_D3;

        // --- Configure CS pin as manual GPIO output ---
        unsafe {
            gpio_set_direction(cs_pin, gpio_mode_t_GPIO_MODE_OUTPUT);
            gpio_set_level(cs_pin, 1); // CS HIGH (deselected)
        }

        // --- Initialize SPI bus ---
        // QSPI uses all 4 data lines: MOSI=D0, MISO=D1, WP=D2, HD=D3
        let bus_config = spi_bus_config_t {
            __bindgen_anon_1: spi_bus_config_t__bindgen_ty_1 {
                mosi_io_num: d0_pin,
            },
            __bindgen_anon_2: spi_bus_config_t__bindgen_ty_2 {
                miso_io_num: d1_pin,
            },
            sclk_io_num: clk_pin,
            __bindgen_anon_3: spi_bus_config_t__bindgen_ty_3 {
                quadwp_io_num: d2_pin,
            },
            __bindgen_anon_4: spi_bus_config_t__bindgen_ty_4 {
                quadhd_io_num: d3_pin,
            },
            data4_io_num: -1,
            data5_io_num: -1,
            data6_io_num: -1,
            data7_io_num: -1,
            max_transfer_sz: (MAX_PIXELS_PER_CHUNK as i32 * 16) + 8,
            flags: SPICOMMON_BUSFLAG_MASTER | SPICOMMON_BUSFLAG_GPIO_PINS,
            isr_cpu_id: esp_intr_cpu_affinity_t_ESP_INTR_CPU_AFFINITY_AUTO,
            intr_flags: 0,
        };

        unsafe {
            let ret = spi_bus_initialize(
                spi_host_device_t_SPI2_HOST,
                &bus_config,
                spi_common_dma_t_SPI_DMA_CH_AUTO,
            );
            if ret != ESP_OK {
                println!("[Display] SPI bus init failed: {}", ret);
                return Err("SPI bus initialization failed");
            }
        }
        println!(
            "[Display] SPI bus initialized (40MHz QSPI, D0-D3 on pins {},{},{},{})",
            d0_pin, d1_pin, d2_pin, d3_pin
        );

        // --- Add SPI device ---
        // 8-bit command + 24-bit address, half-duplex, manual CS
        let dev_config = spi_device_interface_config_t {
            command_bits: 8,
            address_bits: 24,
            dummy_bits: 0,
            mode: 0, // SPI Mode 0
            clock_source: soc_periph_spi_clk_src_t_SPI_CLK_SRC_DEFAULT,
            duty_cycle_pos: 0,
            cs_ena_pretrans: 0,
            cs_ena_posttrans: 0,
            clock_speed_hz: SPI_FREQ_HZ,
            input_delay_ns: 0,
            spics_io_num: -1, // Manual CS control (not hardware CS)
            flags: SPI_DEVICE_HALFDUPLEX,
            queue_size: 1,
            pre_cb: None,
            post_cb: None,
        };

        let mut spi_handle: spi_device_handle_t = ptr::null_mut();
        unsafe {
            let ret = spi_bus_add_device(spi_host_device_t_SPI2_HOST, &dev_config, &mut spi_handle);
            if ret != ESP_OK {
                println!("[Display] SPI device add failed: {}", ret);
                return Err("SPI device add failed");
            }
            // Acquire the bus exclusively (not shared)
            spi_device_acquire_bus(spi_handle, u32::MAX);
        }

        // --- Allocate DMA buffer (must be in internal SRAM, DMA-capable) ---
        let dma_buf =
            unsafe { heap_caps_aligned_alloc(16, DMA_BUF_SIZE, MALLOC_CAP_DMA) as *mut u8 };
        if dma_buf.is_null() {
            return Err("DMA buffer allocation failed");
        }

        // --- Allocate PSRAM framebuffer ---
        let framebuffer = unsafe { heap_caps_malloc(FB_BYTES, MALLOC_CAP_SPIRAM) as *mut u16 };
        if framebuffer.is_null() {
            return Err("PSRAM framebuffer allocation failed (need 322KB)");
        }
        // Clear to black
        unsafe {
            ptr::write_bytes(framebuffer, 0, FB_SIZE);
        }

        println!(
            "[Display] PSRAM framebuffer: {}KB allocated",
            FB_BYTES / 1024
        );

        let mut display = Display {
            spi_handle,
            cs_pin,
            dma_buf,
            framebuffer,
        };

        // --- Run SH8601 initialization sequence ---
        display.init_panel()?;

        println!("[Display] SH8601 AMOLED ready!");
        Ok(display)
    }

    /// Run the SH8601 panel initialization sequence.
    /// Ported from Arduino_SH8601::tftInit() + sh8601_init_operations[].
    fn init_panel(&mut self) -> Result<(), &'static str> {
        if config::BOARD_206 {
            self.write_command_data(0x36, 0x00);
            self.write_command_data(CMD_PIXFMT, 0x55);
            self.write_command(CMD_SLPOUT);
            thread::sleep(Duration::from_millis(120));
            self.write_command_data(0xC4, 0x80);
            // TE output is unused by this synchronous renderer.
            self.write_command_data(0x35, 0x00);
            self.write_command_data(CMD_CTRL_DISPLAY, 0x20);
            thread::sleep(Duration::from_millis(10));
            self.write_command_data(0x63, 0xff);
            thread::sleep(Duration::from_millis(10));
            self.write_command(CMD_DISPON);
            thread::sleep(Duration::from_millis(20));
            self.set_brightness(30);
            return Ok(());
        }
        // Software reset (since RST pin is -1 / not connected)
        self.write_command(CMD_SWRESET);
        thread::sleep(Duration::from_millis(200));

        // Sleep out
        self.write_command(CMD_SLPOUT);
        thread::sleep(Duration::from_millis(120));

        // Normal display mode on
        self.write_command(CMD_NORON);

        // Inversion off
        self.write_command(CMD_INVOFF);

        // Pixel format: 0x05 = RGB565 (16 bits/pixel)
        self.write_command_data(CMD_PIXFMT, 0x05);

        // Display on
        self.write_command(CMD_DISPON);

        // Brightness control on + display dimming on
        self.write_command_data(CMD_CTRL_DISPLAY, 0x28);

        // Dim bring-up level until the PMIC policy establishes battery state.
        self.write_command_data(CMD_BRIGHTNESS, 30);

        // Contrast enhancement off
        self.write_command_data(CMD_CE, 0x00);

        thread::sleep(Duration::from_millis(10));

        Ok(())
    }

    /// Set display brightness (0-255).
    pub fn set_brightness(&self, brightness: u8) {
        self.write_command_data(CMD_BRIGHTNESS, brightness);
    }

    /// Set the address window for pixel writes.
    /// After this, pixel data can be streamed via write_pixels().
    fn set_addr_window(&self, x: u16, y: u16, w: u16, h: u16) {
        let x = x + if config::BOARD_206 { 22 } else { 0 };
        let x_end = x + w - 1;
        let y_end = y + h - 1;

        // Column address set (CASET)
        self.write_command_data32(CMD_CASET, x, x_end);

        // Page address set (PASET)
        self.write_command_data32(CMD_PASET, y, y_end);

        // Memory write start
        self.write_command(CMD_RAMWR);
    }

    /// Blit the entire PSRAM framebuffer to the AMOLED display.
    /// This is the main rendering call - once per frame at 30fps.
    pub fn blit(&self) {
        if self.framebuffer.is_null() {
            return;
        }

        // Set address window to full screen
        self.set_addr_window(0, 0, WIDTH, HEIGHT);

        // Transfer framebuffer in chunks via quad SPI DMA
        let total_pixels = FB_SIZE;
        let mut offset: usize = 0;
        let mut first_send = true;
        let rot = crate::config::rotate_180();

        while offset < total_pixels {
            let chunk = if total_pixels - offset > MAX_PIXELS_PER_CHUNK {
                MAX_PIXELS_PER_CHUNK
            } else {
                total_pixels - offset
            };

            // Copy pixels to DMA buffer with byte-swap (big-endian for display)
            unsafe {
                let dst = self.dma_buf as *mut u16;
                if rot {
                    for i in 0..chunk {
                        let pixel = *self.framebuffer.add(FB_SIZE - 1 - (offset + i));
                        *dst.add(i) = pixel.swap_bytes();
                    }
                } else {
                    for i in 0..chunk {
                        let pixel = *self.framebuffer.add(offset + i);
                        *dst.add(i) = pixel.swap_bytes();
                    }
                }
            }

            // Send chunk via quad SPI
            self.write_pixels_chunk(chunk, first_send);
            first_send = false;
            offset += chunk;
        }

        // Deselect after full transfer
        self.cs_high();
    }

    /// Fill the entire framebuffer with a solid RGB565 color.
    pub fn fill_color(&self, color: u16) {
        if self.framebuffer.is_null() {
            return;
        }
        unsafe {
            for i in 0..FB_SIZE {
                *self.framebuffer.add(i) = color;
            }
        }
    }

    /// Fill the framebuffer with a test gradient pattern.
    /// Creates a rainbow gradient to verify the display works.
    #[expect(dead_code, reason = "Retained for manual panel bring-up; normal rendering does not run the diagnostic")]
    pub fn fill_test_pattern(&self) {
        if self.framebuffer.is_null() {
            return;
        }
        unsafe {
            for y in 0..HEIGHT as usize {
                for x in 0..WIDTH as usize {
                    // Create a rainbow gradient based on position
                    let hue = ((x * 360) / WIDTH as usize) as u16;
                    let val = 255 - ((y * 200) / HEIGHT as usize) as u8;
                    let color = hsv_to_rgb565(hue, 255, val);
                    *self.framebuffer.add(y * WIDTH as usize + x) = color;
                }
            }
        }
    }

    // ============================================================
    // Low-level QSPI Transaction Helpers
    // ============================================================

    /// Send a command with no data (single-line mode).
    /// Protocol: [CS LOW] [0x02 cmd] [cmd_byte << 8 addr] [CS HIGH]
    fn write_command(&self, cmd: u8) {
        self.cs_low();
        unsafe {
            let mut tran: spi_transaction_ext_t = std::mem::zeroed();
            tran.base.flags = SPI_TRANS_MULTILINE_CMD | SPI_TRANS_MULTILINE_ADDR;
            tran.base.cmd = QSPI_WRITE_CMD as u16;
            tran.base.addr = (cmd as u64) << 8;
            tran.base.__bindgen_anon_1.tx_buffer = ptr::null();
            tran.base.length = 0;

            spi_device_polling_start(self.spi_handle, &mut tran.base, u32::MAX);
            spi_device_polling_end(self.spi_handle, u32::MAX);
        }
        self.cs_high();
    }

    /// Send a command with one byte of data (single-line mode).
    /// Protocol: [CS LOW] [0x02 cmd] [cmd_byte << 8 addr] [data_byte] [CS HIGH]
    fn write_command_data(&self, cmd: u8, data: u8) {
        self.cs_low();
        unsafe {
            let mut tran: spi_transaction_ext_t = std::mem::zeroed();
            tran.base.flags =
                SPI_TRANS_USE_TXDATA | SPI_TRANS_MULTILINE_CMD | SPI_TRANS_MULTILINE_ADDR;
            tran.base.cmd = QSPI_WRITE_CMD as u16;
            tran.base.addr = (cmd as u64) << 8;
            tran.base.__bindgen_anon_1.tx_data[0] = data;
            tran.base.length = 8;

            spi_device_polling_start(self.spi_handle, &mut tran.base, u32::MAX);
            spi_device_polling_end(self.spi_handle, u32::MAX);
        }
        self.cs_high();
    }

    /// Send a command with two 16-bit values (for CASET/PASET).
    fn write_command_data32(&self, cmd: u8, d1: u16, d2: u16) {
        self.cs_low();
        unsafe {
            let mut tran: spi_transaction_ext_t = std::mem::zeroed();
            tran.base.flags =
                SPI_TRANS_USE_TXDATA | SPI_TRANS_MULTILINE_CMD | SPI_TRANS_MULTILINE_ADDR;
            tran.base.cmd = QSPI_WRITE_CMD as u16;
            tran.base.addr = (cmd as u64) << 8;
            tran.base.__bindgen_anon_1.tx_data[0] = (d1 >> 8) as u8;
            tran.base.__bindgen_anon_1.tx_data[1] = (d1 & 0xFF) as u8;
            tran.base.__bindgen_anon_1.tx_data[2] = (d2 >> 8) as u8;
            tran.base.__bindgen_anon_1.tx_data[3] = (d2 & 0xFF) as u8;
            tran.base.length = 32;

            spi_device_polling_start(self.spi_handle, &mut tran.base, u32::MAX);
            spi_device_polling_end(self.spi_handle, u32::MAX);
        }
        self.cs_high();
    }

    /// Send a chunk of pixel data via quad SPI.
    /// First chunk includes the full command/address header.
    /// Subsequent chunks skip the header (continue mode).
    fn write_pixels_chunk(&self, pixel_count: usize, first: bool) {
        if first {
            self.cs_low();
        }

        unsafe {
            let mut tran: spi_transaction_ext_t = std::mem::zeroed();

            if first {
                // First chunk: include quad write command + address
                tran.base.flags = SPI_TRANS_MODE_QIO;
                tran.base.cmd = QSPI_QUAD_WRITE_CMD as u16;
                tran.base.addr = if config::BOARD_206 {
                    0x002C00
                } else {
                    QSPI_DATA_ADDR as u64
                };
            } else {
                // Continuation: skip command/address (variable length = 0)
                tran.base.flags = SPI_TRANS_MODE_QIO
                    | SPI_TRANS_VARIABLE_CMD
                    | SPI_TRANS_VARIABLE_ADDR
                    | SPI_TRANS_VARIABLE_DUMMY;
                tran.command_bits = 0;
                tran.address_bits = 0;
                tran.dummy_bits = 0;
            }

            tran.base.__bindgen_anon_1.tx_buffer = self.dma_buf as *const _;
            tran.base.length = pixel_count * 16; // bits

            spi_device_polling_start(self.spi_handle, &mut tran.base, u32::MAX);
            spi_device_polling_end(self.spi_handle, u32::MAX);
        }
    }

    // --- GPIO helpers for manual CS control ---

    #[inline(always)]
    fn cs_low(&self) {
        unsafe {
            gpio_set_level(self.cs_pin, 0);
        }
    }

    #[inline(always)]
    fn cs_high(&self) {
        unsafe {
            gpio_set_level(self.cs_pin, 1);
        }
    }
}

// ============================================================
// Color Utilities
// ============================================================

/// Convert HSV to RGB565 color.
/// Hue: 0-360, Saturation: 0-255, Value: 0-255.
/// Used throughout the system for emotional color mapping.
#[expect(dead_code, reason = "Used only by the retained manual panel test pattern")]
pub fn hsv_to_rgb565(hue: u16, sat: u8, val: u8) -> u16 {
    let hue = hue % 360;
    let region = (hue / 60) as u8;
    let remainder = ((hue - (region as u16 * 60)) * 255 / 60) as u8;

    let p = ((val as u16 * (255 - sat as u16)) >> 8) as u8;
    let q = ((val as u16 * (255 - ((sat as u16 * remainder as u16) >> 8))) >> 8) as u8;
    let t = ((val as u16 * (255 - ((sat as u16 * (255 - remainder as u16)) >> 8))) >> 8) as u8;

    let (r, g, b) = match region {
        0 => (val, t, p),
        1 => (q, val, p),
        2 => (p, val, t),
        3 => (p, q, val),
        4 => (t, p, val),
        _ => (val, p, q),
    };

    // Pack as RGB565: RRRRR GGGGGG BBBBB
    ((r as u16 & 0xF8) << 8) | ((g as u16 & 0xFC) << 3) | ((b as u16) >> 3)
}
