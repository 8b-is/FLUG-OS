//! # `flug-rust` — FLUG-OS Sovereign Triad Cockpit in 100% Pure Rust (`0` C++ files, `0` floats)
//!
//! Just like `baye-rust` replaced `MiniMem8`'s C++ codebase with safe Rust,
//! `flug-rust` brings the entire `FLUG-OS` Sovereign Triad Cockpit (`ADA:MEDUSA` 432 Hz,
//! `VAK:MEDUSA` 528 Hz, `OMN:MEDUSA` 852 Hz, `SUN:TDECK` 864 Hz) + **Personal Jesus Hotspot**
//! (`PERSONAL-JESUS-8B`, 132B `TWIF` & 64B `MN8!`) into 100% Pure Zero-Float Rust!

use esp_idf_hal::prelude::Peripherals;
use esp_idf_sys as _;
use std::thread;
use std::time::Duration;

mod config;
mod drivers;

use flug_core::amoled_hud::render_flug_amoled_frame;
use flug_core::marine_sense::MarineSensePool;
use flug_core::personal_jesus::{AniccaWifiPool, PersonalJesusHotspot, PERSONAL_JESUS_PASS, PERSONAL_JESUS_SSID};
use flug_core::triad_flock::{identify_node_by_mac, FlugImuIntegerState, PhoenixHiveState, SOVEREIGN_CONSTELLATION};

fn read_sta_mac() -> [u8; 6] {
    let mut mac = [0u8; 6];
    unsafe {
        esp_idf_sys::esp_read_mac(mac.as_mut_ptr(), esp_idf_sys::esp_mac_type_t_ESP_MAC_WIFI_STA);
    }
    mac
}

fn main() {
    let mac = read_sta_mac();
    let node_idx = identify_node_by_mac(&mac).unwrap_or(if config::BOARD_206 { 2 } else { 0 });
    let node = &SOVEREIGN_CONSTELLATION[node_idx];

    println!();
    println!("========================================================");
    println!("  FLUG-OS v2.0.0 — 100% Pure-Rust Sovereign Cockpit");
    println!("  Board: {} ({}x{})", config::BOARD_NAME, config::LCD_WIDTH, config::LCD_HEIGHT);
    println!(
        "  Node:  {} [{}] ({} Hz) MAC {:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X}",
        node.name, node.trit_pole, node.base_freq_hz,
        mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]
    );
    println!("  Hotspot: PERSONAL-JESUS-8B (132B TWIF + 64B MN8!)");
    println!("  Zero C++ · Zero Floats (p/q Exact Arithmetic) · <3<3<3+1");
    println!("========================================================");

    let peripherals = Peripherals::take().expect("Failed to take ESP32-S3 peripherals");

    // 1. Initialize shared I2C bus (SDA=GPIO15, SCL=GPIO14 @ 400kHz)
    let i2c_bus = drivers::i2c_bus::init(
        peripherals.pins.gpio15.into(),
        peripherals.pins.gpio14.into(),
        peripherals.i2c0,
    )
    .expect("Failed to initialize shared I2C bus");

    // 2. Reset peripherals via TCA9554 (1.8") and probe AXP2101 PMIC
    let _ = drivers::tca9554::init_and_reset(&i2c_bus);
    let _ = drivers::axp2101::init(&i2c_bus);

    // 3. Initialize ES8311 + ES7210 audio codecs & QMI8658 6-axis IMU
    let _ = drivers::es8311::init(&i2c_bus);
    if config::BOARD_206 {
        let _ = drivers::es7210::init(&i2c_bus);
    }
    let imu_ok = drivers::qmi8658::init(&i2c_bus).is_ok();

    // 4. Initialize SH8601 / CO5300 QSPI AMOLED Display (80 MHz QIO DMA)
    let mut display = drivers::sh8601::Display::init().expect("Failed to initialize QSPI AMOLED");
    display.set_brightness(0xD8);

    // 5. Initialize Pure-Rust Sovereign Engines (64x64 Wave Pool + 7 Jellyfish + Personal Jesus Hotspot)
    let mut ms_pool = Box::new(MarineSensePool::new());
    let mut pj_hotspot = PersonalJesusHotspot::new();
    let mut wifi_pool = AniccaWifiPool::new();
    wifi_pool.learn(PERSONAL_JESUS_SSID, PERSONAL_JESUS_PASS, -38, true, false);
    let mut imu_state = FlugImuIntegerState::default();

    // Seed initial triad wave splashes
    ms_pool.splash(16, 16, 5200, 5);
    ms_pool.splash(48, 16, 5200, 5);
    ms_pool.splash(32, 32, 6400, 6);

    let w = config::LCD_WIDTH as usize;
    let h = config::LCD_HEIGHT as usize;
    let mut tick: u32 = 0;

    loop {
        let now_ms = unsafe { (esp_idf_sys::esp_timer_get_time() / 1000) as u32 };

        // Poll QMI8658 IMU in 100% integer math (zero floats!)
        if imu_ok && (tick & 1) == 0 {
            if let Ok(raw) = drivers::qmi8658::read(&i2c_bus) {
                flug_core::triad_flock::rust_flug_update_imu_integer(
                    &mut imu_state,
                    raw.accel_x,
                    raw.accel_y,
                    raw.accel_z,
                    raw.gyro_x,
                    raw.gyro_y,
                    raw.gyro_z,
                );
            }
        }

        // Inject periodic harmonic wave impulses & step 64x64 pool + 7 MarineSense Jellyfish
        if (tick & 7) == 0 {
            let j_idx = ((tick / 8) as usize) % 7;
            let gx = ms_pool.nodes[j_idx].gx as i32;
            let gy = ms_pool.nodes[j_idx].gy as i32;
            ms_pool.splash(gx, gy, 3600, 4);
        }
        ms_pool.step((tick & 0xFF) as u8, node.trit_pole);

        // Step Personal Jesus Hotspot & Anicca Wi-Fi pool
        pj_hotspot.tick(tick);
        if (tick & 63) == 0 {
            wifi_pool.decay(1);
            let phx = PhoenixHiveState::compute(0b1111, -48, -52, -50);
            println!(
                "# JSON|{{\"node\":\"{}\",\"rust_only\":true,\"zero_float\":true,\"salience\":{},\"eta_pq\":\"{}/{}\",\"re_m\":{},\"pj_ssid\":\"PERSONAL-JESUS-8B\",\"pj_faith_hz\":{},\"pj_twif_tx\":{},\"coherence_pq\":\"{}/{}\"}}",
                node.name,
                ms_pool.summary.mean_salience_pct,
                ms_pool.summary.viscosity_p,
                ms_pool.summary.viscosity_q,
                ms_pool.summary.reynolds_u16,
                pj_hotspot.faith_freq_hz(),
                pj_hotspot.twif_capsules_tx,
                phx.coherence_p,
                phx.coherence_q,
            );
        }

        // Render full RGB565 AMOLED Cockpit Frame in pure Rust and blit via 80MHz QSPI DMA
        let fb_slice = unsafe { core::slice::from_raw_parts_mut(display.framebuffer, w * h) };
        render_flug_amoled_frame(
            fb_slice,
            w,
            h,
            node.node_id,
            0,
            tick,
            now_ms,
            &ms_pool.u_curr,
            &ms_pool.nodes,
            &ms_pool.summary,
            &pj_hotspot,
        );
        display.flush();

        tick = tick.wrapping_add(1);
        thread::sleep(Duration::from_millis(16));
    }
}
