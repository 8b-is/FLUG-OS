//! # `tdeck-wave-core` — 100% `#![no_std]` Rust Cymatic Wave-Memory OS Kernel
//!
//! Target: `xtensa-esp32s3-none-elf` (LilyGO T-Deck `ESP32-S3FN16R8`)
//!
//! Everything runs in `#![no_std]` Rust with **Zero Floating-Point Math** (`0` `f32`/`f64`)
//! and **Zero Heap Allocations**:
//! - **2D Integer Cymatic Wave Tank (`64 × 48` cells = `320 × 240` ST7789 GRAM)**:
//!   Letters, numbers, RF spectrum bars, `LINOSV_SEED` ternary lattices, and
//!   Anicca breathing spheres are injected as **Standing-Wave Nodal Attractors**
//!   that crystallize out of the rippling water while radiating concentric waves.
//! - **8-Slot Resonant Wave Engram Ring Buffer**:
//!   Remembers words, numbers, and shapes by their standing-wave nodal pattern
//!   and resonant frequency (`432 Hz`, `528 Hz`, `864 Hz`).
//! - **432 Hz Just-Intonation Integer Triangle Synthesizer**:
//!   Generates phase-accurate 16-bit PCM audio buffers in pure integer Rust.
//! - **Differential ST7789 GRAM Strip Renderer**:
//!   Tracks each cell's quantized color tier in ST7789 GRAM and only builds
//!   `320 × 5` RGB565 DMA strips for rows that actually changed.

#![cfg_attr(target_os = "none", no_std)]

pub mod amoled_hud;
pub mod marine_sense;
pub mod personal_jesus;
pub mod qwave_liquid;
pub mod triad_flock;

#[cfg(target_os = "none")]
#[panic_handler]
fn panic(_info: &core::panic::PanicInfo) -> ! {
    loop {}
}

pub const WAVE_W: usize = 64;
pub const WAVE_H: usize = 48;

/// Canonical ayeOS `LINOSV_SEED` SHA-256 prefix (`67dc0028cfe67ab79a3d1042e91f58c4`)
pub const LINOSV_HASH16: [u8; 16] = [
    0x67, 0xdc, 0x00, 0x28, 0xcf, 0xe6, 0x7a, 0xb7,
    0x9a, 0x3d, 0x10, 0x42, 0xe9, 0x1f, 0x58, 0xc4,
];

/// Quarter-wave integer sine LUT (`Q8` scale: `0..=256` over `0..=64` steps)
pub const SIN_Q8: [i16; 65] = [
      0,   6,  13,  19,  25,  31,  38,  44,  50,  56,  62,  68,  74,  80,  86,  92,
     98, 104, 109, 115, 121, 126, 132, 137, 142, 147, 152, 157, 162, 167, 172, 177,
    181, 185, 190, 194, 198, 202, 206, 209, 213, 216, 220, 223, 226, 229, 231, 234,
    237, 239, 241, 243, 245, 247, 248, 250, 251, 252, 253, 254, 255, 255, 256, 256, 256,
];

/// Integer `log2(x)` in `Q10` (`1024 == 1.0 bit`) for `x in 0..=64`
pub const LOG2_Q10: [u16; 65] = [
       0,    0, 1024, 1623, 2048, 2378, 2647, 2875, 3072, 3246, 3402, 3542, 3671, 3789, 3899, 4001,
    4096, 4186, 4270, 4350, 4426, 4498, 4566, 4632, 4695, 4755, 4813, 4869, 4923, 4975, 5025, 5073,
    5120, 5166, 5210, 5253, 5294, 5335, 5374, 5413, 5450, 5486, 5522, 5557, 5590, 5623, 5656, 5687,
    5718, 5748, 5778, 5807, 5835, 5863, 5891, 5918, 5944, 5970, 5995, 6020, 6045, 6069, 6093, 6116, 6144,
];

/// Pre-byte-swapped 16-tier bioluminescent RGB565 wave palette for direct ST7789 DMA
pub const PALETTE_SW: [u16; 16] = [
    0x0821u16.swap_bytes(), // 0: Deepest Trough (#04050A)
    0x0842u16.swap_bytes(), // 1: Resting Water (#080914)
    0x10A5u16.swap_bytes(), // 2: Subtle Ripple (#10152A)
    0x194Au16.swap_bytes(), // 3: Rising Wave (#182952)
    0x0333u16.swap_bytes(), // 4: Mid Cyan Wave
    0x059Bu16.swap_bytes(), // 5: Bright Cyan Wave
    0x07FFu16.swap_bytes(), // 6: VAK Territory Cyan Crest (528 Hz)
    0x7FFFu16.swap_bytes(), // 7: Bioluminescent White-Cyan Peak
    0xFBB7u16.swap_bytes(), // 8: Standing-Wave Node: ADA Rose-Gold (432 Hz)
    0xFEA0u16.swap_bytes(), // 9: Standing-Wave Node: Sovereign Gold Core
    0xFFFFu16.swap_bytes(), // 10: Standing-Wave Node: Incandescent Peak
    0x27E8u16.swap_bytes(), // 11: Standing-Wave Node: Emerald Harmonic
    0xB4DFu16.swap_bytes(), // 12: Engram Beacon Idle (OMN Violet)
    0x07FFu16.swap_bytes(), // 13: Engram Beacon Active (Cyan)
    0xFBB7u16.swap_bytes(), // 14: Engram Beacon Typing (Rose)
    0xFEA0u16.swap_bytes(), // 15: Engram Beacon Selected (Gold)
];

/// 5x7 Wave-Node Glyph Masks (`0x20..=0x5F`)
pub const GLYPH5X7: [[u8; 5]; 64] = [
    [0x00,0x00,0x00,0x00,0x00],[0x00,0x00,0x5F,0x00,0x00],[0x00,0x07,0x00,0x07,0x00],[0x14,0x7F,0x14,0x7F,0x14],
    [0x24,0x2A,0x7F,0x2A,0x12],[0x23,0x13,0x08,0x64,0x62],[0x36,0x49,0x55,0x22,0x50],[0x00,0x05,0x03,0x00,0x00],
    [0x00,0x1C,0x22,0x41,0x00],[0x00,0x41,0x22,0x1C,0x00],[0x14,0x08,0x3E,0x08,0x14],[0x08,0x08,0x3E,0x08,0x08],
    [0x00,0x50,0x30,0x00,0x00],[0x08,0x08,0x08,0x08,0x08],[0x00,0x60,0x60,0x00,0x00],[0x20,0x10,0x08,0x04,0x02],
    [0x3E,0x51,0x49,0x45,0x3E],[0x00,0x42,0x7F,0x40,0x00],[0x42,0x61,0x51,0x49,0x46],[0x21,0x41,0x45,0x4B,0x31],
    [0x18,0x14,0x12,0x7F,0x10],[0x27,0x45,0x45,0x45,0x39],[0x3C,0x4A,0x49,0x49,0x30],[0x01,0x71,0x09,0x05,0x03],
    [0x36,0x49,0x49,0x49,0x36],[0x06,0x49,0x49,0x29,0x1E],[0x00,0x36,0x36,0x00,0x00],[0x00,0x56,0x36,0x00,0x00],
    [0x08,0x14,0x22,0x41,0x00],[0x14,0x14,0x14,0x14,0x14],[0x00,0x41,0x22,0x14,0x08],[0x02,0x01,0x51,0x09,0x06],
    [0x32,0x49,0x79,0x41,0x3E],[0x7E,0x11,0x11,0x11,0x7E],[0x7F,0x49,0x49,0x49,0x36],[0x3E,0x41,0x41,0x41,0x22],
    [0x7F,0x41,0x41,0x22,0x1C],[0x7F,0x49,0x49,0x49,0x41],[0x7F,0x09,0x09,0x09,0x01],[0x3E,0x41,0x49,0x49,0x7A],
    [0x7F,0x08,0x08,0x08,0x7F],[0x00,0x41,0x7F,0x41,0x00],[0x20,0x40,0x41,0x3F,0x01],[0x7F,0x08,0x14,0x22,0x41],
    [0x7F,0x40,0x40,0x40,0x40],[0x7F,0x02,0x0C,0x02,0x7F],[0x7F,0x04,0x08,0x10,0x7F],[0x3E,0x41,0x41,0x41,0x3E],
    [0x7F,0x09,0x09,0x09,0x06],[0x3E,0x41,0x51,0x21,0x5E],[0x7F,0x09,0x19,0x29,0x46],[0x46,0x49,0x49,0x49,0x31],
    [0x01,0x01,0x7F,0x01,0x01],[0x3F,0x40,0x40,0x40,0x3F],[0x1F,0x20,0x40,0x20,0x1F],[0x3F,0x40,0x38,0x40,0x3F],
    [0x63,0x14,0x08,0x14,0x63],[0x07,0x08,0x70,0x08,0x07],[0x61,0x51,0x49,0x45,0x43],[0x00,0x7F,0x41,0x41,0x00],
    [0x02,0x04,0x08,0x10,0x20],[0x00,0x41,0x41,0x7F,0x00],[0x04,0x02,0x01,0x02,0x04],[0x40,0x40,0x40,0x40,0x40],
];

#[inline]
pub const fn fast_sin_q8(angle: u8) -> i16 {
    let q = angle >> 6;
    let idx = (angle & 63) as usize;
    match q {
        0 => SIN_Q8[idx],
        1 => SIN_Q8[64 - idx],
        2 => -SIN_Q8[idx],
        _ => -SIN_Q8[64 - idx],
    }
}

#[inline]
pub const fn fast_cos_q8(angle: u8) -> i16 {
    fast_sin_q8(angle.wrapping_add(64))
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct Xoshiro128 {
    pub s: [u32; 4],
}

impl Xoshiro128 {
    pub const fn new(seed16: &[u8; 16]) -> Self {
        let mut s = [0u32; 4];
        let mut i = 0;
        while i < 4 {
            let v = (seed16[i * 4] as u32)
                | ((seed16[i * 4 + 1] as u32) << 8)
                | ((seed16[i * 4 + 2] as u32) << 16)
                | ((seed16[i * 4 + 3] as u32) << 24);
            s[i] = if v == 0 {
                ((i as u32) + 1).wrapping_mul(2654435761)
            } else {
                v
            };
            i += 1;
        }
        Self { s }
    }

    #[inline]
    pub fn next_u32(&mut self) -> u32 {
        let r = self.s[1].wrapping_mul(5);
        let result = r.rotate_left(7).wrapping_mul(9);
        let t = self.s[1] << 9;
        self.s[2] ^= self.s[0];
        self.s[3] ^= self.s[1];
        self.s[1] ^= self.s[2];
        self.s[0] ^= self.s[3];
        self.s[2] ^= t;
        self.s[3] = self.s[3].rotate_left(11);
        result
    }
}

/// Maps physical QWERTY key to 432 Hz Just-Intonation frequency in Hz (pure integer)
#[no_mangle]
pub extern "C" fn rust_synth_note_for_key(c: u8) -> u16 {
    let lower = if c >= b'A' && c <= b'Z' { c + 32 } else { c };
    match lower {
        b'q' => 216,
        b'w' => 243,
        b'e' => 270,
        b'r' => 288,
        b't' => 324,
        b'y' => 360,
        b'u' => 405,
        b'a' => 432,
        b's' => 486,
        b'd' => 528,
        b'f' => 576,
        b'g' => 648,
        b'h' => 720,
        b'j' => 864,
        b'k' => 972,
        b'l' => 1056,
        _ => 432,
    }
}

/// Fills a 16-bit PCM triangle wave chunk with smooth attack/decay envelope in pure integer Rust
#[no_mangle]
pub extern "C" fn rust_synth_fill_triangle_i16(
    buf: *mut i16,
    chunk_len: usize,
    sample_offset: u32,
    total_samples: u32,
    hz: u16,
    amp: i16,
) {
    if buf.is_null() || hz == 0 || total_samples == 0 {
        return;
    }
    let sr: u32 = 24000;
    let period = (sr / (hz as u32)).max(2);
    let half = period / 2;
    let amp32 = amp as i32;
    let period32 = period as i32;

    let mut i = 0usize;
    while i < chunk_len {
        let pos = sample_offset + (i as u32);
        let p = pos % period;
        let mut tri = if p < half {
            ((p as i32) * 4 * amp32 / period32) - amp32
        } else {
            (((period - p) as i32) * 4 * amp32 / period32) - amp32
        };
        if pos < 240 {
            tri = (tri * (pos as i32)) / 240;
        }
        let rem = total_samples.saturating_sub(pos);
        if rem < 240 {
            tri = (tri * (rem as i32)) / 240;
        }
        unsafe {
            *buf.add(i) = tri as i16;
        }
        i += 1;
    }
}

/// Computes 100% integer Shannon entropy $H(X)$ in milli-bits (`1000 == 1.000 bit`)
#[no_mangle]
pub extern "C" fn rust_shannon_entropy_milli(ch_counts: *const i32, total_aps: i32) -> u32 {
    if ch_counts.is_null() || total_aps <= 0 {
        return 0;
    }
    let n = if total_aps > 64 { 64 } else { total_aps as usize };
    let log_n_q10 = LOG2_Q10[n] as u32;
    let mut weighted_sum_q10: u32 = 0;
    let mut c = 1usize;
    while c <= 14 {
        let cnt = unsafe { *ch_counts.add(c) };
        if cnt > 0 {
            let cc = if cnt > 64 { 64 } else { cnt as usize };
            weighted_sum_q10 = weighted_sum_q10.wrapping_add((cnt as u32) * (LOG2_Q10[cc] as u32));
        }
        c += 1;
    }
    let mean_q10 = weighted_sum_q10 / (total_aps as u32);
    let h_q10 = log_n_q10.saturating_sub(mean_q10);
    (h_q10 * 1000) >> 10
}

/// Computes the 9-trit balanced ternary signature (`+`, `-`, `0`) from 14-channel counts
#[no_mangle]
pub extern "C" fn rust_compute_trit_vec(ch_counts: *const i32, out_trits10: *mut u8) {
    if ch_counts.is_null() || out_trits10.is_null() {
        return;
    }
    let mut t = 0usize;
    while t < 9 {
        let c1 = unsafe { *ch_counts.add((t % 13) + 1) };
        let c2 = unsafe { *ch_counts.add(((t + 4) % 13) + 1) };
        let ch = if c1 > c2 {
            b'+'
        } else if c1 < c2 {
            b'-'
        } else {
            b'0'
        };
        unsafe {
            *out_trits10.add(t) = ch;
        }
        t += 1;
    }
    unsafe {
        *out_trits10.add(9) = 0;
    }
}

/// Stamps a single ASCII character (`0x20..=0x7E`) as a standing-wave nodal attractor
#[no_mangle]
pub extern "C" fn rust_stamp_char_attractor(
    attractor: *mut u8,
    cur_wave: *mut i16,
    gx0: i32,
    gy0: i32,
    ch: u8,
    attr_type: u8,
    pluck: bool,
) {
    if attractor.is_null() || cur_wave.is_null() {
        return;
    }
    let up = if ch >= b'a' && ch <= b'z' { ch - 32 } else { ch };
    if up < 0x20 || up > 0x5F {
        return;
    }
    let glyph = &GLYPH5X7[(up - 0x20) as usize];
    let mut col = 0usize;
    while col < 5 {
        let bits = glyph[col];
        let mut row = 0usize;
        while row < 7 {
            if (bits & (1 << row)) != 0 {
                let gx = gx0 + (col as i32);
                let gy = gy0 + (row as i32);
                if gx >= 1 && gx < (WAVE_W as i32 - 1) && gy >= 2 && gy < (WAVE_H as i32 - 2) {
                    let idx = (gy as usize) * WAVE_W + (gx as usize);
                    unsafe {
                        *attractor.add(idx) = attr_type;
                        if pluck {
                            *cur_wave.add(idx) = 900;
                        }
                    }
                }
            }
            row += 1;
        }
        col += 1;
    }
}

/// Stamps a null-terminated C string centered horizontally at `gy0` as standing-wave nodes
#[no_mangle]
pub extern "C" fn rust_stamp_str_attractor(
    attractor: *mut u8,
    cur_wave: *mut i16,
    gy0: i32,
    s: *const u8,
    attr_type: u8,
    pluck: bool,
) {
    if attractor.is_null() || cur_wave.is_null() || s.is_null() {
        return;
    }
    let mut len = 0usize;
    unsafe {
        while len < 10 && *s.add(len) != 0 {
            len += 1;
        }
    }
    if len == 0 {
        return;
    }
    let total_w = (len * 6) - 1;
    let gx0 = ((WAVE_W.saturating_sub(total_w)) / 2).max(1) as i32;
    let mut i = 0usize;
    while i < len {
        let ch = unsafe { *s.add(i) };
        rust_stamp_char_attractor(
            attractor,
            cur_wave,
            gx0 + (i as i32) * 6,
            gy0,
            ch,
            attr_type,
            pluck,
        );
        i += 1;
    }
}

/// Steps the 2D Integer Cymatic Wave Tank (`64 × 48`) by one timestep and returns
/// a 64-bit bitmask of which rows (`0..48`) changed their quantized color tier in ST7789 GRAM!
#[no_mangle]
pub extern "C" fn rust_step_cymatic_wave_tank(
    cur: *mut i16,
    nxt: *mut i16,
    attractor: *const u8,
    gram_tier: *mut u8,
    tick: u32,
) -> u64 {
    if cur.is_null() || nxt.is_null() || attractor.is_null() || gram_tier.is_null() {
        return 0;
    }
    let phase = (tick.wrapping_mul(7)) as u8;
    let drive = fast_sin_q8(phase) as i32;

    // 1. Propagate 2D Integer Wave Equation across interior cells
    let mut y = 1usize;
    while y < WAVE_H - 1 {
        let mut x = 1usize;
        while x < WAVE_W - 1 {
            let idx = y * WAVE_W + x;
            unsafe {
                let neighbors = (*cur.add((y - 1) * WAVE_W + x) as i32)
                    + (*cur.add((y + 1) * WAVE_W + x) as i32)
                    + (*cur.add(y * WAVE_W + (x - 1)) as i32)
                    + (*cur.add(y * WAVE_W + (x + 1)) as i32);
                let mut val = (neighbors >> 1) - (*nxt.add(idx) as i32);
                val -= val >> 4; // 1/16 natural water damping

                let attr = *attractor.add(idx);
                if attr == 1 {
                    val = 520 + (drive >> 1);
                } else if attr == 2 {
                    val = 420 - (drive >> 1);
                } else if attr == 3 {
                    val = 760;
                }

                if val > 1024 {
                    val = 1024;
                } else if val < -1024 {
                    val = -1024;
                }
                *nxt.add(idx) = val as i16;
            }
            x += 1;
        }
        y += 1;
    }

    // 2. Quantize wave displacement to ST7789 GRAM color tiers & build dirty-row bitmask
    let mut dirty_rows_mask: u64 = 0;
    let mut gy = 0usize;
    while gy < WAVE_H {
        let mut row_dirty = false;
        let mut gx = 0usize;
        while gx < WAVE_W {
            let idx = gy * WAVE_W + gx;
            unsafe {
                let attr = *attractor.add(idx);
                let u = *nxt.add(idx) as i32;
                let tier: u8 = if attr == 3 {
                    15
                } else if attr == 1 {
                    if u > 580 {
                        10
                    } else if u > 460 {
                        9
                    } else {
                        6
                    }
                } else if attr == 2 {
                    if u > 440 { 8 } else { 9 }
                } else {
                    let mapped = (u + 320) >> 6;
                    if mapped < 0 {
                        0
                    } else if mapped > 7 {
                        7
                    } else {
                        mapped as u8
                    }
                };
                if *gram_tier.add(idx) != tier {
                    *gram_tier.add(idx) = tier;
                    row_dirty = true;
                }
            }
            gx += 1;
        }
        if row_dirty {
            dirty_rows_mask |= 1u64 << gy;
        }
        gy += 1;
    }
    dirty_rows_mask
}

/// Builds a `320 × 5` (`1,600` pixel) pre-byte-swapped RGB565 scan-strip from `gram_tier` row `gy`
#[no_mangle]
pub extern "C" fn rust_build_gram_strip_320x5(
    gram_tier: *const u8,
    gy: usize,
    strip_out: *mut u16,
) {
    if gram_tier.is_null() || strip_out.is_null() || gy >= WAVE_H {
        return;
    }
    let row_base = gy * WAVE_W;
    let mut gx = 0usize;
    while gx < WAVE_W {
        let tier = unsafe { (*gram_tier.add(row_base + gx) & 15) as usize };
        let sw_col = PALETTE_SW[tier];
        let px0 = gx * 5;
        unsafe {
            *strip_out.add(px0) = sw_col;
            *strip_out.add(px0 + 1) = sw_col;
            *strip_out.add(px0 + 2) = sw_col;
            *strip_out.add(px0 + 3) = sw_col;
            *strip_out.add(px0 + 4) = sw_col;
        }
        gx += 1;
    }
    // Replicate row 0 into rows 1..=4 of the 320x5 strip
    let mut r = 1usize;
    while r < 5 {
        let dst_base = r * 320;
        let mut x = 0usize;
        while x < 320 {
            unsafe {
                *strip_out.add(dst_base + x) = *strip_out.add(x);
            }
            x += 1;
        }
        r += 1;
    }
}

/// Real-time `#![no_std]` Rust Qwave Stereo Acoustic Telemetry (`ES7210` Dual-Mic DSP)
#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct QwaveFrameMetrics {
    pub peak_l: u16,
    pub peak_r: u16,
    pub mav_l: u16,
    pub mav_r: u16,
    pub f0_hz: u16,
    pub azimuth_deg: i16,
    pub distance_cm: u16,
    pub salience_pct: u8,
    pub vad_state: u8, // 0 = QUIET, 1 = AMBIENT, 2 = DIRECT!
    pub wave_hash16: u16,
}

/// Analyzes an interleaved stereo 16-bit PCM frame (`[L0, R0, L1, R1, ...]`) from the
/// LilyGO T-Deck's `ES7210` dual microphones in 100% integer `#![no_std]` Rust.
#[no_mangle]
pub extern "C" fn rust_qwave_analyze_stereo_i16(
    stereo_pcm: *const i16,
    num_frames: usize,
    sample_rate: u32,
    out: *mut QwaveFrameMetrics,
) {
    if stereo_pcm.is_null() || out.is_null() || num_frames < 16 || sample_rate == 0 {
        return;
    }
    // 1. Compute DC offset per channel
    let mut sum_l: i64 = 0;
    let mut sum_r: i64 = 0;
    let mut i = 0usize;
    while i < num_frames {
        unsafe {
            sum_l += *stereo_pcm.add(i * 2) as i64;
            sum_r += *stereo_pcm.add(i * 2 + 1) as i64;
        }
        i += 1;
    }
    let dc_l = (sum_l / (num_frames as i64)) as i32;
    let dc_r = (sum_r / (num_frames as i64)) as i32;

    // 2. Peak, Mean-Absolute-Value (MAV), Zero-Crossing Pitch F0, MarineSense O(1) Salience, and FNV-1a Wave Hash
    let mut marine = marine_sense_core::MarineEngine::new();
    let mut marine_salience_peak: i32 = 0;
    let mut peak_l: i32 = 0;
    let mut peak_r: i32 = 0;
    let mut abs_sum_l: u64 = 0;
    let mut abs_sum_r: u64 = 0;
    let mut zc_count: u32 = 0;
    let mut prev_sign: i32 = 0;
    let mut hash: u32 = 0x811c_9dc5;

    i = 0;
    while i < num_frames {
        let (raw_l, raw_r) = unsafe {
            (
                *stereo_pcm.add(i * 2) as i32,
                *stereo_pcm.add(i * 2 + 1) as i32,
            )
        };
        let l = raw_l - dc_l;
        let r = raw_r - dc_r;
        let al = l.abs();
        let ar = r.abs();
        if al > peak_l {
            peak_l = al;
        }
        if ar > peak_r {
            peak_r = ar;
        }
        abs_sum_l += al as u64;
        abs_sum_r += ar as u64;

        // Hysteresis zero-crossing detector + MarineSense O(1) Salience on combined signal
        let mono = (l + r) >> 1;
        let m_sal = marine.process_sample(mono);
        if m_sal > marine_salience_peak {
            marine_salience_peak = m_sal;
        }
        if mono > 90 {
            if prev_sign < 0 {
                zc_count += 1;
            }
            prev_sign = 1;
        } else if mono < -90 {
            if prev_sign > 0 {
                zc_count += 1;
            }
            prev_sign = -1;
        }

        hash ^= ((mono >> 4) & 0xFF) as u32;
        hash = hash.wrapping_mul(0x0100_0193);
        i += 1;
    }

    let mav_l = (abs_sum_l / (num_frames as u64)).min(32767) as u16;
    let mav_r = (abs_sum_r / (num_frames as u64)).min(32767) as u16;
    let mav_avg = ((mav_l as u32) + (mav_r as u32)) >> 1;

    // 3. Stereo Cross-Correlation ITD (-4..=+4 sample lag) + ILD Azimuth
    let mut best_lag: i32 = 0;
    let mut best_corr: i64 = i64::MIN;
    let mut lag: i32 = -4;
    while lag <= 4 {
        let mut corr: i64 = 0;
        let mut k = 8usize;
        while k + 8 < num_frames {
            let rk = ((k as i32) + lag) as usize;
            let (l, r) = unsafe {
                (
                    (*stereo_pcm.add(k * 2) as i32 - dc_l) as i64,
                    (*stereo_pcm.add(rk * 2 + 1) as i32 - dc_r) as i64,
                )
            };
            corr += (l * r) >> 6;
            k += 2;
        }
        if corr > best_corr {
            best_corr = corr;
            best_lag = lag;
        }
        lag += 1;
    }

    let ild_deg = if (mav_l as i32 + mav_r as i32) > 40 {
        (((mav_r as i32 - mav_l as i32) * 45) / (mav_l as i32 + mav_r as i32 + 1)).clamp(-60, 60)
    } else {
        0
    };
    let azimuth_deg = ((best_lag * 12) + ild_deg).clamp(-90, 90) as i16;

    // 4. Pitch F0 (Hz), Distance (cm), Salience (%), and VAD state
    let raw_f0 = ((zc_count * sample_rate) / ((num_frames as u32) * 2)) as u16;
    let f0_hz = if mav_avg < 25 {
        148
    } else {
        raw_f0.clamp(75, 1800)
    };

    let distance_cm = if mav_avg > 2000 {
        45
    } else if mav_avg > 800 {
        95
    } else if mav_avg > 250 {
        145
    } else if mav_avg > 80 {
        195
    } else {
        260
    };

    let marine_pct = ((marine_salience_peak.max(0) as u32 * 100) >> 16).min(99) as u8;
    let salience_pct = if mav_avg >= 1200 {
        96.max(marine_pct)
    } else if mav_avg >= 400 {
        (85 + ((mav_avg - 400) / 80) as u8).max(marine_pct)
    } else if mav_avg >= 60 {
        (50 + ((mav_avg - 60) / 10) as u8).max(marine_pct)
    } else {
        ((20 + (mav_avg / 3)).min(49) as u8).max(marine_pct)
    };

    let vad_state = if mav_avg >= 180 {
        2 // DIRECT!
    } else if mav_avg >= 45 {
        1 // AMBIENT
    } else {
        0 // QUIET
    };

    unsafe {
        *out = QwaveFrameMetrics {
            peak_l: peak_l.min(32767) as u16,
            peak_r: peak_r.min(32767) as u16,
            mav_l,
            mav_r,
            f0_hz,
            azimuth_deg,
            distance_cm,
            salience_pct,
            vad_state,
            wave_hash16: ((hash >> 16) ^ (hash & 0xFFFF)) as u16,
        };
    }
}

/// Full 95-glyph 5x7 ASCII Font (`0x20..=0x7E`, including lowercase & punctuation) in `#![no_std]` Rust
pub const FONT5X7_95: [[u8; 5]; 95] = [
    [0x00,0x00,0x00,0x00,0x00],[0x00,0x00,0x5F,0x00,0x00],[0x00,0x07,0x00,0x07,0x00],[0x14,0x7F,0x14,0x7F,0x14],
    [0x24,0x2A,0x7F,0x2A,0x12],[0x23,0x13,0x08,0x64,0x62],[0x36,0x49,0x55,0x22,0x50],[0x00,0x05,0x03,0x00,0x00],
    [0x00,0x1C,0x22,0x41,0x00],[0x00,0x41,0x22,0x1C,0x00],[0x14,0x08,0x3E,0x08,0x14],[0x08,0x08,0x3E,0x08,0x08],
    [0x00,0x50,0x30,0x00,0x00],[0x08,0x08,0x08,0x08,0x08],[0x00,0x60,0x60,0x00,0x00],[0x20,0x10,0x08,0x04,0x02],
    [0x3E,0x51,0x49,0x45,0x3E],[0x00,0x42,0x7F,0x40,0x00],[0x42,0x61,0x51,0x49,0x46],[0x21,0x41,0x45,0x4B,0x31],
    [0x18,0x14,0x12,0x7F,0x10],[0x27,0x45,0x45,0x45,0x39],[0x3C,0x4A,0x49,0x49,0x30],[0x01,0x71,0x09,0x05,0x03],
    [0x36,0x49,0x49,0x49,0x36],[0x06,0x49,0x49,0x29,0x1E],[0x00,0x36,0x36,0x00,0x00],[0x00,0x56,0x36,0x00,0x00],
    [0x08,0x14,0x22,0x41,0x00],[0x14,0x14,0x14,0x14,0x14],[0x00,0x41,0x22,0x14,0x08],[0x02,0x01,0x51,0x09,0x06],
    [0x32,0x49,0x79,0x41,0x3E],[0x7E,0x11,0x11,0x11,0x7E],[0x7F,0x49,0x49,0x49,0x36],[0x3E,0x41,0x41,0x41,0x22],
    [0x7F,0x41,0x41,0x22,0x1C],[0x7F,0x49,0x49,0x49,0x41],[0x7F,0x09,0x09,0x09,0x01],[0x3E,0x41,0x49,0x49,0x7A],
    [0x7F,0x08,0x08,0x08,0x7F],[0x00,0x41,0x7F,0x41,0x00],[0x20,0x40,0x41,0x3F,0x01],[0x7F,0x08,0x14,0x22,0x41],
    [0x7F,0x40,0x40,0x40,0x40],[0x7F,0x02,0x0C,0x02,0x7F],[0x7F,0x04,0x08,0x10,0x7F],[0x3E,0x41,0x41,0x41,0x3E],
    [0x7F,0x09,0x09,0x09,0x06],[0x3E,0x41,0x51,0x21,0x5E],[0x7F,0x09,0x19,0x29,0x46],[0x46,0x49,0x49,0x49,0x31],
    [0x01,0x01,0x7F,0x01,0x01],[0x3F,0x40,0x40,0x40,0x3F],[0x1F,0x20,0x40,0x20,0x1F],[0x3F,0x40,0x38,0x40,0x3F],
    [0x63,0x14,0x08,0x14,0x63],[0x07,0x08,0x70,0x08,0x07],[0x61,0x51,0x49,0x45,0x43],[0x00,0x7F,0x41,0x41,0x00],
    [0x02,0x04,0x08,0x10,0x20],[0x00,0x41,0x41,0x7F,0x00],[0x04,0x02,0x01,0x02,0x04],[0x40,0x40,0x40,0x40,0x40],
    [0x00,0x01,0x02,0x04,0x00],[0x20,0x54,0x54,0x54,0x78],[0x7F,0x48,0x44,0x44,0x38],[0x38,0x44,0x44,0x44,0x20],
    [0x38,0x44,0x44,0x48,0x7F],[0x38,0x54,0x54,0x54,0x18],[0x08,0x7E,0x09,0x01,0x02],[0x0C,0x52,0x52,0x52,0x3E],
    [0x7F,0x08,0x04,0x04,0x78],[0x00,0x44,0x7D,0x40,0x00],[0x20,0x40,0x44,0x3D,0x00],[0x7F,0x10,0x28,0x44,0x00],
    [0x00,0x41,0x7F,0x40,0x00],[0x7C,0x04,0x18,0x04,0x78],[0x7C,0x08,0x04,0x04,0x78],[0x38,0x44,0x44,0x44,0x38],
    [0x7C,0x14,0x14,0x14,0x08],[0x08,0x14,0x14,0x18,0x7C],[0x7C,0x08,0x04,0x04,0x08],[0x48,0x54,0x54,0x54,0x20],
    [0x04,0x3F,0x44,0x40,0x20],[0x3C,0x40,0x40,0x20,0x7C],[0x1C,0x20,0x40,0x20,0x1C],[0x3C,0x40,0x30,0x40,0x3C],
    [0x44,0x28,0x10,0x28,0x44],[0x0C,0x50,0x50,0x50,0x3C],[0x44,0x64,0x54,0x4C,0x44],[0x00,0x08,0x36,0x41,0x00],
    [0x00,0x00,0x7F,0x00,0x00],[0x00,0x41,0x36,0x08,0x00],[0x10,0x08,0x08,0x10,0x08],
];

/// Renders a single `6 × 8` RGB565 character cell (byte-swapped for direct ST7789 DMA) in `#![no_std]` Rust
#[no_mangle]
pub extern "C" fn rust_render_char_cell_6x8(
    c: u8,
    fg: u16,
    bg: u16,
    out_cell48: *mut u16,
) {
    if out_cell48.is_null() {
        return;
    }
    let ch = if c >= 0x20 && c <= 0x7E { c } else { b'?' };
    let glyph = &FONT5X7_95[(ch - 0x20) as usize];
    let fg_sw = fg.swap_bytes();
    let bg_sw = bg.swap_bytes();

    let mut row = 0usize;
    while row < 8 {
        let mut col = 0usize;
        while col < 6 {
            let on = if col < 5 && row < 7 {
                (glyph[col] & (1 << row)) != 0
            } else {
                false
            };
            unsafe {
                *out_cell48.add(row * 6 + col) = if on { fg_sw } else { bg_sw };
            }
            col += 1;
        }
        row += 1;
    }
}

/// 100% Zero-Allocation `#![no_std]` Rust JSON String Field Extractor.
/// Parses `"<key>" : "<value>"` (regardless of whitespace around `:`), unescapes `\"`,
/// writes null-terminated bytes to `out_ptr`, and returns the byte length.
#[no_mangle]
pub extern "C" fn rust_extract_json_field(
    json_ptr: *const u8,
    json_len: usize,
    key_ptr: *const u8,
    key_len: usize,
    out_ptr: *mut u8,
    out_cap: usize,
) -> usize {
    if json_ptr.is_null() || key_ptr.is_null() || out_ptr.is_null() || out_cap == 0 {
        return 0;
    }
    unsafe {
        *out_ptr = 0;
    }
    if json_len < key_len + 4 {
        return 0;
    }
    let json = unsafe { core::slice::from_raw_parts(json_ptr, json_len) };
    let key = unsafe { core::slice::from_raw_parts(key_ptr, key_len) };

    // Locate `"key"`
    let mut pos = 0usize;
    let mut found_after_key = None;
    while pos + key_len + 2 <= json_len {
        if json[pos] == b'"' && json[pos + key_len + 1] == b'"' {
            let mut m = true;
            let mut k = 0usize;
            while k < key_len {
                if json[pos + 1 + k] != key[k] {
                    m = false;
                    break;
                }
                k += 1;
            }
            if m {
                found_after_key = Some(pos + key_len + 2);
                break;
            }
        }
        pos += 1;
    }

    let mut i = match found_after_key {
        Some(idx) => idx,
        None => return 0,
    };

    // Find ':'
    while i < json_len && json[i] != b':' {
        i += 1;
    }
    if i >= json_len {
        return 0;
    }
    i += 1; // skip ':'

    // Find opening '"'
    while i < json_len && (json[i] == b' ' || json[i] == b'\t' || json[i] == b'\r' || json[i] == b'\n') {
        i += 1;
    }
    if i >= json_len || json[i] != b'"' {
        return 0;
    }
    i += 1; // skip opening '"'

    let mut written = 0usize;
    let max_write = out_cap - 1;
    while i < json_len && written < max_write {
        let b = json[i];
        if b == b'\\' && i + 1 < json_len {
            i += 1;
            unsafe {
                *out_ptr.add(written) = json[i];
            }
            written += 1;
        } else if b == b'"' {
            break;
        } else {
            unsafe {
                *out_ptr.add(written) = b;
            }
            written += 1;
        }
        i += 1;
    }
    unsafe {
        *out_ptr.add(written) = 0;
    }
    written
}

/// Canonical `p/q` Rational reduction via `marine_sense_core::Rational::new(num, den)` (Euclidean GCD, zero floats!).
#[no_mangle]
pub extern "C" fn rust_reduce_rational_pq(
    num: i32,
    den: i32,
    out_p: *mut i32,
    out_q: *mut i32,
) {
    let r = marine_sense_core::Rational::new(num, den);
    if !out_p.is_null() {
        unsafe {
            *out_p = r.num;
        }
    }
    if !out_q.is_null() {
        unsafe {
            *out_q = r.den;
        }
    }
}

/// Builds a 18-byte `FLUG-OS` `S3TriadCapsule` (`magic = "TRIA"`) in `#![no_std]` Rust
/// so the T-Deck ("THE SUN") can wake and ripple all 3 AMOLED watches (`ADA`, `VAK`, `OMN`).
#[no_mangle]
pub extern "C" fn rust_build_tria_capsule(
    out_buf18: *mut u8,
    node_id: u8,
    trit_pole: i8,
    track_idx: u8,
    qt_byte: u8,
    ax_i8: i8,
    ay_i8: i8,
    nand_trit: i8,
    splash_x: u8,
    splash_y: u8,
    splash_amp: i16,
    seq: u32,
) {
    if out_buf18.is_null() {
        return;
    }
    let amp_le = splash_amp.to_le_bytes();
    let seq_le = seq.to_le_bytes();
    let raw: [u8; 18] = [
        b'T',
        b'R',
        b'I',
        b'A',
        node_id,
        trit_pole as u8,
        track_idx,
        qt_byte,
        ax_i8 as u8,
        ay_i8 as u8,
        nand_trit as u8,
        splash_x.min(63),
        splash_y.min(47),
        amp_le[0],
        amp_le[1],
        seq_le[0],
        seq_le[1],
        seq_le[2],
    ];
    let mut i = 0usize;
    while i < 18 {
        unsafe {
            *out_buf18.add(i) = raw[i];
        }
        i += 1;
    }
}

/// Builds a 64-byte `baye-rust` `BayeCapsule` (`magic = "BC"`, `version = 1`, `Splash = 0x03`)
/// in `#![no_std]` Rust so `baye-rust` watches also receive the Sun's and sibling watches' wave impulses!
#[no_mangle]
pub extern "C" fn rust_build_baye_splash_capsule(
    out_buf64: *mut u8,
    src_mac6: *const u8,
    seq: u8,
    uptime_ms: u32,
    energy_u16: u16,
    f0_hz: u16,
    salience_pct: u8,
) {
    if out_buf64.is_null() || src_mac6.is_null() {
        return;
    }
    let mut i = 0usize;
    while i < 64 {
        unsafe {
            *out_buf64.add(i) = 0;
        }
        i += 1;
    }
    let ts = uptime_ms.to_le_bytes();
    // baye-rust SplashInfo layout at payload[0..7] (bytes 16..23):
    //   [0..2] x: u16 LE (center of 128x128 grid = 64, also > 0 for tdeck-omni energy check)
    //   [2..4] y: u16 LE (center of 128x128 grid = 64)
    //   [4..6] amplitude: i16 LE
    //   [6]    radius: u8 (6)
    let x_le = 64u16.to_le_bytes();
    let y_le = 64u16.to_le_bytes();
    let amp_i16 = (energy_u16.clamp(600, 14000)) as i16;
    let amp_le = amp_i16.to_le_bytes();
    let en = energy_u16.to_le_bytes();
    let f0 = f0_hz.to_le_bytes();
    unsafe {
        *out_buf64.add(0) = 0x42; // 'B'
        *out_buf64.add(1) = 0x43; // 'C'
        *out_buf64.add(2) = 1;    // CAPSULE_VERSION = 1
        *out_buf64.add(3) = 0x03; // CapsuleKind::Splash
        let mut m = 0usize;
        while m < 6 {
            *out_buf64.add(4 + m) = *src_mac6.add(m);
            m += 1;
        }
        *out_buf64.add(10) = 2;   // TTL
        *out_buf64.add(11) = seq;
        *out_buf64.add(12) = ts[0];
        *out_buf64.add(13) = ts[1];
        *out_buf64.add(14) = ts[2];
        *out_buf64.add(15) = ts[3];
        // SplashInfo payload[0..7] -> bytes 16..23
        *out_buf64.add(16) = x_le[0];
        *out_buf64.add(17) = x_le[1];
        *out_buf64.add(18) = y_le[0];
        *out_buf64.add(19) = y_le[1];
        *out_buf64.add(20) = amp_le[0];
        *out_buf64.add(21) = amp_le[1];
        *out_buf64.add(22) = 6; // radius
        // Extended telemetry at bytes 24..29
        *out_buf64.add(24) = en[0];
        *out_buf64.add(25) = en[1];
        *out_buf64.add(26) = f0[0];
        *out_buf64.add(27) = f0[1];
        *out_buf64.add(28) = salience_pct;
    }
}

/// `#![no_std]` Rust 802.11 Management IE Parser (Beacons `fsub=8`, Probe Responses `fsub=5`,
/// AND Directed Probe Requests `fsub=4` where nearby devices ask for specific Wi-Fi points!).
/// Extracts:
///   - `out_ssid33`: null-terminated printable SSID (up to 32 bytes)
///   - `out_vendor_oui`: 24-bit Vendor OUI from IE Tag 221 (e.g. `0x0017F2` Apple, `0x0050F2` Microsoft)
///   - `out_phy_caps`: bitmask (`bit0` = HT/Wi-Fi 4 Tag 45, `bit1` = VHT/Wi-Fi 5 Tag 191, `bit2` = HE/Wi-Fi 6 Tag 255)
/// Returns the extracted SSID length (`0` if broadcast/wildcard probe or non-printable).
#[no_mangle]
pub extern "C" fn rust_parse_80211_probe_ie(
    payload: *const u8,
    len: usize,
    fsub: u8,
    out_ssid33: *mut u8,
    out_vendor_oui: *mut u32,
    out_phy_caps: *mut u8,
) -> usize {
    if payload.is_null() || out_ssid33.is_null() {
        return 0;
    }
    unsafe {
        *out_ssid33 = 0;
        if !out_vendor_oui.is_null() {
            *out_vendor_oui = 0;
        }
        if !out_phy_caps.is_null() {
            *out_phy_caps = 0;
        }
    }
    // Probe Request (fsub == 4) starts IEs at byte 24; Beacon (8) / Probe Resp (5) starts at byte 36
    let mut off: usize = if fsub == 4 {
        24
    } else if fsub == 8 || fsub == 5 {
        36
    } else {
        return 0;
    };
    if len <= off + 2 {
        return 0;
    }
    let bytes = unsafe { core::slice::from_raw_parts(payload, len) };
    let max_off = len.min(240);
    let mut ssid_len: usize = 0;
    let mut vendor_oui: u32 = 0;
    let mut phy_caps: u8 = 0;

    while off + 2 <= max_off {
        let tag = bytes[off];
        let tlen = bytes[off + 1] as usize;
        if off + 2 + tlen > len {
            break;
        }
        if tag == 0 && ssid_len == 0 && tlen > 0 && tlen <= 32 {
            let mut printable = true;
            let mut i = 0usize;
            while i < tlen {
                let b = bytes[off + 2 + i];
                if !(0x20..=0x7E).contains(&b) {
                    printable = false;
                    break;
                }
                i += 1;
            }
            if printable {
                let mut k = 0usize;
                while k < tlen {
                    let mut ch = bytes[off + 2 + k];
                    if ch == b'|' || ch == b'\\' || ch == b'"' {
                        ch = b'_';
                    }
                    unsafe {
                        *out_ssid33.add(k) = ch;
                    }
                    k += 1;
                }
                unsafe {
                    *out_ssid33.add(tlen) = 0;
                }
                ssid_len = tlen;
            }
        } else if tag == 45 {
            phy_caps |= 0x01; // HT (802.11n / Wi-Fi 4)
        } else if tag == 191 {
            phy_caps |= 0x02; // VHT (802.11ac / Wi-Fi 5)
        } else if tag == 255 {
            phy_caps |= 0x04; // HE (802.11ax / Wi-Fi 6)
        } else if tag == 221 && tlen >= 3 && vendor_oui == 0 {
            vendor_oui = ((bytes[off + 2] as u32) << 16)
                | ((bytes[off + 3] as u32) << 8)
                | (bytes[off + 4] as u32);
        }
        off += 2 + tlen;
    }

    unsafe {
        if !out_vendor_oui.is_null() {
            *out_vendor_oui = vendor_oui;
        }
        if !out_phy_caps.is_null() {
            *out_phy_caps = phy_caps;
        }
    }
    ssid_len
}

/// 32-slot `#![no_std]` MAC-Independent 802.11 IE Genome Signature Table.
/// Identifies unique physical radio/driver signatures (`sig_hash`) even when a device
/// randomizes or rotates its 48-bit MAC address (`addr2`).
#[derive(Copy, Clone)]
struct ProbeDeviceSig {
    used: bool,
    sig_hash: u32,
    last_mac: [u8; 6],
    last_seq: u16,
    last_rssi: i8,
    mac_rotations: u16,
    probe_count: u16,
    vendor_oui: u32,
    phy_caps: u8,
}

impl ProbeDeviceSig {
    const fn empty() -> Self {
        Self {
            used: false,
            sig_hash: 0,
            last_mac: [0; 6],
            last_seq: 0,
            last_rssi: -100,
            mac_rotations: 0,
            probe_count: 0,
            vendor_oui: 0,
            phy_caps: 0,
        }
    }
}

static mut SIG_TABLE: [ProbeDeviceSig; 32] = [ProbeDeviceSig::empty(); 32];
static mut SIG_TOTAL_ROTATIONS: u32 = 0;

#[inline]
fn fnv1a_byte(h: u32, b: u8) -> u32 {
    (h ^ (b as u32)).wrapping_mul(0x0100_0193)
}

/// `#![no_std]` Rust MAC-Independent 802.11 Probe Request IE Genome Fingerprinter (`fsub == 4`).
/// Computes a 32-bit FNV-1a signature (`out_sig_hash`) over the invariant Information Element (IE)
/// layout and radio capability bitmasks (ordered Tag IDs, Supported Rates `1/50`, HT Capabilities `45`,
/// Extended Capabilities `127`, VHT `191`, HE `255`, and Vendor OUI + Subtype `221` while skipping
/// per-burst random vendor tail nonces and skipping the MAC address).
/// Tracks unique signatures in `SIG_TABLE` and counts MAC rotations (`out_total_rotations`) whenever
/// the same physical signature appears with a rotated MAC address!
#[no_mangle]
pub extern "C" fn rust_fingerprint_80211_probe(
    payload: *const u8,
    len: usize,
    rssi: i8,
    out_sig_hash: *mut u32,
    out_unique_sigs: *mut u16,
    out_total_rotations: *mut u32,
    out_sig_rotations: *mut u16,
) -> u8 {
    if payload.is_null() || len <= 26 {
        return 0;
    }
    let bytes = unsafe { core::slice::from_raw_parts(payload, len) };
    let fctl = bytes[0];
    let ftype = (fctl >> 2) & 0x03;
    let fsub = (fctl >> 4) & 0x0F;
    if ftype != 0 || fsub != 4 {
        return 0;
    }

    // Source MAC (addr2 = bytes[10..16]) & 12-bit 802.11 sequence number (bytes[22..24] >> 4)
    let mac = [
        bytes[10], bytes[11], bytes[12], bytes[13], bytes[14], bytes[15],
    ];
    let seq_ctrl = (bytes[22] as u16) | ((bytes[23] as u16) << 8);
    let seq_num = seq_ctrl >> 4;
    let is_random_mac = (mac[0] & 0x02) != 0;

    let mut h: u32 = 0x811C_9DC5;
    let mut off: usize = 24;
    let max_off = len.min(240);
    let mut phy_caps: u8 = if is_random_mac { 0x08 } else { 0x00 };
    let mut vendor_oui: u32 = if !is_random_mac {
        ((mac[0] as u32) << 16) | ((mac[1] as u32) << 8) | (mac[2] as u32)
    } else {
        0
    };
    let mut tag_count: u8 = 0;

    while off + 2 <= max_off {
        let tag = bytes[off];
        let tlen = bytes[off + 1] as usize;
        if off + 2 + tlen > len {
            break;
        }
        tag_count = tag_count.saturating_add(1);

        // Fold structural tag ID and length (skip Tag 0 SSID length from structural genome
        // so wildcard and directed probes from the same radio share the same base structure,
        // and skip Tag 3 DS channel parameter which varies per channel hop).
        if tag != 0 && tag != 3 {
            h = fnv1a_byte(h, tag);
            h = fnv1a_byte(h, tlen as u8);
        }

        match tag {
            1 | 50 => {
                // Supported Rates & Extended Supported Rates (invariant per chipset/driver)
                let mut i = 0usize;
                while i < tlen {
                    h = fnv1a_byte(h, bytes[off + 2 + i]);
                    i += 1;
                }
            }
            45 => {
                // HT Capabilities (Wi-Fi 4): 2B CapInfo + 1B A-MPDU + 4B Rx MCS bitmask
                phy_caps |= 0x01;
                let n = tlen.min(7);
                let mut i = 0usize;
                while i < n {
                    h = fnv1a_byte(h, bytes[off + 2 + i]);
                    i += 1;
                }
                if tlen >= 5 && bytes[off + 2 + 4] != 0 {
                    phy_caps |= 0x10; // 2x2 MIMO spatial stream bit
                }
            }
            70 | 107 | 127 => {
                // RM Enabled (70), Interworking (107), Extended Capabilities (127)
                let n = tlen.min(8);
                let mut i = 0usize;
                while i < n {
                    h = fnv1a_byte(h, bytes[off + 2 + i]);
                    i += 1;
                }
            }
            191 => {
                // VHT Capabilities (Wi-Fi 5)
                phy_caps |= 0x02;
                let n = tlen.min(8);
                let mut i = 0usize;
                while i < n {
                    h = fnv1a_byte(h, bytes[off + 2 + i]);
                    i += 1;
                }
            }
            255 => {
                // Extension Element (HE Wi-Fi 6 = Ext Tag 35, EHT Wi-Fi 7 = Ext Tag 108)
                phy_caps |= 0x04;
                let n = tlen.min(7);
                let mut i = 0usize;
                while i < n {
                    h = fnv1a_byte(h, bytes[off + 2 + i]);
                    i += 1;
                }
            }
            221 => {
                // Vendor-Specific IE: fold ONLY the 3-byte OUI + 1-byte OUI Type (skip random tail nonces!)
                if tlen >= 3 {
                    let oui = ((bytes[off + 2] as u32) << 16)
                        | ((bytes[off + 3] as u32) << 8)
                        | (bytes[off + 4] as u32);
                    if vendor_oui == 0 {
                        vendor_oui = oui;
                    }
                    h = fnv1a_byte(h, bytes[off + 2]);
                    h = fnv1a_byte(h, bytes[off + 3]);
                    h = fnv1a_byte(h, bytes[off + 4]);
                    if tlen >= 4 {
                        h = fnv1a_byte(h, bytes[off + 5]);
                    }
                }
            }
            _ => {}
        }
        off += 2 + tlen;
    }

    if tag_count == 0 {
        return 0;
    }

    let mut unique_cnt: u16 = 0;
    let mut sig_rots: u16 = 0;
    let total_rots: u32;

    unsafe {
        let mut slot: usize = 32;
        let mut empty_slot: usize = 32;
        let mut min_cnt: u16 = u16::MAX;
        let mut evict_slot: usize = 0;

        let mut i = 0usize;
        while i < 32 {
            if SIG_TABLE[i].used {
                if SIG_TABLE[i].sig_hash == h {
                    slot = i;
                }
                if SIG_TABLE[i].probe_count < min_cnt {
                    min_cnt = SIG_TABLE[i].probe_count;
                    evict_slot = i;
                }
            } else if empty_slot == 32 {
                empty_slot = i;
            }
            i += 1;
        }

        if slot == 32 {
            slot = if empty_slot < 32 {
                empty_slot
            } else {
                evict_slot
            };
            SIG_TABLE[slot] = ProbeDeviceSig {
                used: true,
                sig_hash: h,
                last_mac: mac,
                last_seq: seq_num,
                last_rssi: rssi,
                mac_rotations: 0,
                probe_count: 1,
                vendor_oui,
                phy_caps,
            };
        } else {
            let entry = &mut SIG_TABLE[slot];
            entry.probe_count = entry.probe_count.saturating_add(1);
            if entry.last_mac != mac {
                let rssi_diff = (rssi as i16 - entry.last_rssi as i16).abs();
                let seq_delta = seq_num.wrapping_sub(entry.last_seq) & 0x0FFF;
                // Same IE genome + rotated MAC within proximity envelope or monotonic sequence window
                if rssi_diff <= 14 || seq_delta <= 64 {
                    entry.mac_rotations = entry.mac_rotations.saturating_add(1);
                    SIG_TOTAL_ROTATIONS = SIG_TOTAL_ROTATIONS.wrapping_add(1);
                }
                entry.last_mac = mac;
            }
            entry.last_seq = seq_num;
            entry.last_rssi = rssi;
            if vendor_oui != 0 {
                entry.vendor_oui = vendor_oui;
            }
            entry.phy_caps |= phy_caps;
            sig_rots = entry.mac_rotations;
        }

        let mut k = 0usize;
        while k < 32 {
            if SIG_TABLE[k].used {
                unique_cnt += 1;
            }
            k += 1;
        }
        total_rots = SIG_TOTAL_ROTATIONS;

        if !out_sig_hash.is_null() {
            *out_sig_hash = h;
        }
        if !out_unique_sigs.is_null() {
            *out_unique_sigs = unique_cnt;
        }
        if !out_total_rotations.is_null() {
            *out_total_rotations = total_rots;
        }
        if !out_sig_rotations.is_null() {
            *out_sig_rotations = sig_rots;
        }
    }

    phy_caps
}

/// `#![no_std]` Rust 64x64 2D Integer Wave Equation + 7-Stage Ternary NAND Ring Stepper
/// for the Waveshare AMOLED Watches (`FLUG-OS`), eliminating C++ inner loops and floats.
#[no_mangle]
pub extern "C" fn rust_step_watch_wave_pool_64(
    u_curr: *mut i16,
    u_prev: *mut i16,
    damp: i32,
    nand_ring7: *mut i8,
    combined_trit: i8,
) {
    if !nand_ring7.is_null() {
        unsafe {
            let r6 = *nand_ring7.add(6);
            let mn0 = if r6 < combined_trit { r6 } else { combined_trit };
            let next0 = -mn0;
            let mut i = 6usize;
            while i >= 1 {
                let r_prev = *nand_ring7.add(i - 1);
                let fb = if i == 3 {
                    *nand_ring7
                } else {
                    *nand_ring7.add((i + 5) % 7)
                };
                let mn = if r_prev < fb { r_prev } else { fb };
                *nand_ring7.add(i) = -mn;
                i -= 1;
            }
            *nand_ring7 = next0;
        }
    }

    if u_curr.is_null() || u_prev.is_null() {
        return;
    }
    let d = damp.clamp(240, 254);
    unsafe {
        let mut y = 1usize;
        while y < 63 {
            let row_up = (y - 1) * 64;
            let row_c = y * 64;
            let row_dn = (y + 1) * 64;
            let mut x = 1usize;
            while x < 63 {
                let idx = row_c + x;
                let center = *u_curr.add(idx) as i32;
                let prev = *u_prev.add(idx) as i32;
                let lap = (*u_curr.add(row_up + x) as i32)
                    + (*u_curr.add(row_dn + x) as i32)
                    + (*u_curr.add(idx - 1) as i32)
                    + (*u_curr.add(idx + 1) as i32)
                    - (center << 2);
                let next = (center << 1) - prev + (lap >> 2);
                let damped = ((next * d) >> 8).clamp(-16384, 16384) as i16;
                *u_prev.add(idx) = damped;
                x += 1;
            }
            y += 1;
        }
        let mut i = 0usize;
        while i < 64 {
            *u_prev.add(i) = 0;
            *u_prev.add(63 * 64 + i) = 0;
            *u_prev.add(i * 64) = 0;
            *u_prev.add(i * 64 + 63) = 0;
            i += 1;
        }
        let mut k = 0usize;
        while k < 4096 {
            let tmp = *u_curr.add(k);
            *u_curr.add(k) = *u_prev.add(k);
            *u_prev.add(k) = tmp;
            k += 1;
        }
    }
}

/// `#![no_std]` Rust Autonomous Moment Director:
/// Decides which of the 5 views (`0..=4`) fits the current acoustic & user-interaction moment!
#[no_mangle]
pub extern "C" fn rust_autonomous_moment_director(
    cur_view: i32,
    vad_state: u8,
    direct_streak: *mut u8,
    tour_idx: *mut u8,
    input_len: i32,
    elapsed_user_ms: u32,
    elapsed_switch_ms: u32,
) -> i32 {
    if direct_streak.is_null() || tour_idx.is_null() {
        return cur_view;
    }
    let streak = unsafe {
        if vad_state == 2 {
            let s = (*direct_streak).saturating_add(1).min(50);
            *direct_streak = s;
            s
        } else {
            *direct_streak = 0;
            0
        }
    };

    // 1. Voice Wake Moment: If user speaks out loud (DIRECT! >= 6 frames) while not typing -> View 0 (1:QWAVE+WAVE)
    if streak >= 6 && cur_view != 0 && input_len == 0 && elapsed_user_ms >= 10000 && elapsed_switch_ms >= 8000 {
        return 0;
    }

    // 2. Ambient Meditative Tour: When quiet & idle for >= 16s, glide through views every 14s
    if input_len == 0 && elapsed_user_ms >= 16000 && elapsed_switch_ms >= 14000 {
        const TOUR_VIEWS: [i32; 5] = [4, 1, 2, 3, 0];
        unsafe {
            let next_i = ((*tour_idx) + 1) % 5;
            *tour_idx = next_i;
            return TOUR_VIEWS[next_i as usize];
        }
    }

    cur_view
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_integer_shannon_entropy_and_wave_tank() {
        let mut counts = [0i32; 15];
        counts[1] = 4;
        counts[6] = 4;
        let h = rust_shannon_entropy_milli(counts.as_ptr(), 8);
        assert_eq!(h, 1000); // Exactly 1.000 bit for 2 equal channels!

        let mut cur = [0i16; WAVE_W * WAVE_H];
        let mut nxt = [0i16; WAVE_W * WAVE_H];
        let mut attr = [0u8; WAVE_W * WAVE_H];
        let mut tiers = [0xFFu8; WAVE_W * WAVE_H];
        rust_stamp_str_attractor(
            attr.as_mut_ptr(),
            cur.as_mut_ptr(),
            13,
            b"AYEOS\0".as_ptr(),
            1,
            true,
        );
        let mask = rust_step_cymatic_wave_tank(
            cur.as_mut_ptr(),
            nxt.as_mut_ptr(),
            attr.as_ptr(),
            tiers.as_mut_ptr(),
            1,
        );
        assert_eq!(mask, (1u64 << WAVE_H) - 1);
    }

    #[test]
    fn test_marine_sense_7_jellyfish_and_exact_pq_viscosity() {
        let mut pool = marine_sense::MarineSensePool::new();
        // At default damping = 248: viscosity = (256 - 248)*100 / 256 = 800/256 = 25/8 (312 centi)
        assert_eq!(pool.summary.viscosity_p, 25);
        assert_eq!(pool.summary.viscosity_q, 8);
        assert_eq!(pool.summary.viscosity_centi, 312);

        // Splash near Alpha (16, 16) and Gamma (32, 11) and step 16 frames
        pool.splash(16, 16, 4800, 5);
        pool.splash(32, 11, 4800, 5);
        for t in 0..16u8 {
            pool.step(t.wrapping_mul(11), 1);
        }
        assert!(pool.splashes >= 2);
        assert!(pool.summary.viscosity_q > 0);
        assert!(pool.summary.reynolds_q > 0);
        assert!(pool.summary.reynolds_u16 >= 100);
    }

    #[test]
    fn test_personal_jesus_hotspot_twif_and_memnet_capsules() {
        let mut pj = personal_jesus::PersonalJesusHotspot::new();
        pj.tick(0);
        assert_eq!(pj.faith_trit, -1);
        assert_eq!(pj.faith_freq_hz(), 432);
        pj.tick(32);
        assert_eq!(pj.faith_trit, 0);
        assert_eq!(pj.faith_freq_hz(), 528);
        pj.tick(64);
        assert_eq!(pj.faith_trit, 1);
        assert_eq!(pj.faith_freq_hz(), 852);

        let mut twif = [0u8; personal_jesus::TWIF_CAPSULE_LEN];
        pj.build_twif_capsule(2, 42, &mut twif);
        assert!(personal_jesus::verify_twif_capsule(&twif));
        assert_eq!(&twif[0..4], b"TWIF");
        assert_eq!(&twif[7..24], b"PERSONAL-JESUS-8B");

        let mut mn = [0u8; personal_jesus::MEMNET_CAPSULE_LEN];
        personal_jesus::encode_memnet_capsule64(
            &mut mn,
            1,
            0,
            3,
            77,
            0x8B15_0528_0432_0852,
            40,
            180,
            220,
            88,
            528,
            12000,
            312,
            380,
            2,
            -48,
            1,
            4,
            b"PERSONAL-JESUS-",
        );
        assert!(personal_jesus::verify_memnet_capsule64(&mn));

        // Verify Anicca Learn-Share-Forget Wi-Fi Pool + 15-char truncation guard
        let mut wifi_pool = personal_jesus::AniccaWifiPool::new();
        wifi_pool.learn(b"PERSONAL-JESUS-8B", b"8b-is-faith!", -42, true, false);
        assert_eq!(wifi_pool.active_count(), 1);
        // Attempting to overwrite with a 15-char truncated hint should be ignored
        wifi_pool.learn(b"PERSONAL-JESUS-", b"", -60, false, false);
        assert_eq!(wifi_pool.active_count(), 1);
        assert_eq!(wifi_pool.slots[0].ssid_len(), 17);
    }

    #[test]
    fn test_triad_flock_integer_synth_imu_and_amoled_hud() {
        let mut stereo = [0i16; 256];
        let mut lfsr = 0xACE1_8B15u32;
        triad_flock::rust_flug_synth_crate_block_i16(
            stereo.as_mut_ptr(),
            0,
            0,
            0,
            &mut lfsr,
        );
        assert!(stereo.iter().any(|&s| s != 0));

        let mut imu = triad_flock::FlugImuIntegerState::default();
        triad_flock::rust_flug_update_imu_integer(&mut imu, 0, 2048, 3547, 640, -320, 128);
        assert!(imu.gyro_mag_dps > 0);
        assert!(imu.pitch_deg > 10 && imu.pitch_deg < 45);

        let phx = triad_flock::PhoenixHiveState::compute(0b0101, -52, -58, -49);
        assert_eq!(phx.missing_node_id, 1); // VAK reconstructed by ADA + OMN
        assert_eq!(phx.reconstructed_trit, 0);
        assert_eq!(phx.reconstructed_freq_hz, 528);

        // Render full 368x448 AMOLED frame in pure Rust
        let mut fb = vec![0u16; 368 * 448];
        let pool = marine_sense::MarineSensePool::new();
        let pj = personal_jesus::PersonalJesusHotspot::new();
        amoled_hud::render_flug_amoled_frame(
            &mut fb,
            368,
            448,
            0,
            0,
            12,
            30_000,
            &pool.u_curr,
            &pool.nodes,
            &pool.summary,
            &pj,
        );
        assert!(fb.iter().any(|&px| px == amoled_hud::COL_ROSE));
        assert!(fb.iter().any(|&px| px == amoled_hud::COL_MINT));
    }

    #[test]
    fn test_qwave_markdown_liquid_rust_asr_tts_and_50mp_cam() {
        let mut asr = qwave_liquid::LiquidRustAsrEngine::new();
        let mut pcm = [0i16; 256];
        qwave_liquid::LiquidRustVoiceSynth::synthesize_voice_pcm_i16(
            b"AYE SHOW QWAVE MARKDOWN",
            432,
            16000,
            0,
            &mut pcm,
        );
        assert!(pcm.iter().any(|&s| s != 0));

        asr.process_pcm_frame(&pcm, 16000, 32);
        assert_eq!(asr.vad_active, 1);
        assert!(asr.salience_pct >= 68);
        assert_eq!(asr.current_phrase(), b"PERSONAL JESUS HOTSPOT ON");

        let mut cam = qwave_liquid::HighEndCameraV1::new();
        cam.step_optical_field(0x50E0_13B1, 7);
        assert!(cam.mean_luma > 0);

        let mut fb = vec![0u16; 368 * 448];
        let md: [&[u8]; 6] = [
            b"# QWAVE + LIQUID-RUST",
            b"> [!TIP] 50MP CAM + DUAL-MIC ASR LIVE",
            b"- [x] Liquid CfC Speech ASR",
            b"```rust",
            b"fn aye_voice(f0: u16) {}",
            b"```",
        ];
        qwave_liquid::render_qwave_markdown_rgb565(&mut fb, 368, 448, 8, 8, 352, 160, &md);
        assert!(fb.iter().any(|&px| px == amoled_hud::COL_GOLD));
    }
}
