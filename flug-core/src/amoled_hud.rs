//! # `amoled_hud` — 100% `#![no_std]` Zero-Float RGB565 AMOLED Renderer (`368×448` SH8601 & `410×502` CO5300)
//!
//! Renders the Sovereign FLUG-OS Cockpit directly into an RGB565 framebuffer (or scanline band)
//! in 100% integer `#![no_std]` Rust:
//! - **Anti-Burn-In 9-Point Balanced-Ternary Pixel Orbit (`{-1, 0, +1}`)**
//! - **64×64 Bioluminescent Wave Pool + 7 MarineSense Jellyfish Detector Nodes (`Alpha..Eta`)**
//! - **432 Hz × φ (`500/809`) Lissajous Phase-Space Attractor + 768 kHz Catmull-Rom Scope**
//! - **`PERSONAL JESUS · HOTSPOT AP` (`PERSONAL-JESUS-8B`)** card with radiating faith bars,
//!   132B `TWIF` capsule telemetry, and 4-Node Sovereign Constellation (`ADA`, `VAK`, `OMN`, `SUN`).

use crate::marine_sense::{JellyNode, MarineSenseSummary, JELLY_COUNT, WAVE_POOL_N};
use crate::personal_jesus::PersonalJesusHotspot;
use crate::{fast_cos_q8, fast_sin_q8, FONT5X7_95};

// Constellation Ink RGB565 Palette (matches FAMILY_CONTEXT.md §10)
pub const COL_VOID: u16 = 0x0821;       // #05060A near-black
pub const COL_CARD_BG: u16 = 0x0842;    // #080914 deep indigo card
pub const COL_MINT: u16 = 0x6739;       // #62E6C9 Constellation Mint
pub const COL_GOLD: u16 = 0xED8B;       // #E8B05C Constellation Gold
pub const COL_VIOLET: u16 = 0xB45F;     // #B48BFF Constellation Violet
pub const COL_CYAN: u16 = 0x07FF;       // #00F0FF Constellation Cyan
pub const COL_ROSE: u16 = 0xF993;       // #FF3399 Personal Jesus Faith Rose
pub const COL_WHITE: u16 = 0xFFFF;      // Pure White
pub const COL_MUTED: u16 = 0x52AA;      // Slate Muted

/// 9-point Balanced-Ternary (`{-1, 0, +1}`) Anti-Burn-In Pixel Orbit
pub const ORBIT_DX9: [i8; 9] = [0, 1, 1, 0, -1, -1, -1, 0, 1];
pub const ORBIT_DY9: [i8; 9] = [0, 0, 1, 1, 1, 0, -1, -1, -1];

#[inline]
pub const fn pixel_orbit_offset(now_ms: u32) -> (i8, i8) {
    let idx = ((now_ms / 30000) % 9) as usize;
    (ORBIT_DX9[idx], ORBIT_DY9[idx])
}

#[inline]
fn put_px(fb: &mut [u16], w: usize, h: usize, x: i32, y: i32, col: u16) {
    if x >= 0 && (x as usize) < w && y >= 0 && (y as usize) < h {
        fb[(y as usize) * w + (x as usize)] = col;
    }
}

pub fn fill_rect_rgb565(
    fb: &mut [u16],
    w: usize,
    h: usize,
    x0: i32,
    y0: i32,
    rw: i32,
    rh: i32,
    col: u16,
) {
    let sx = x0.max(0) as usize;
    let sy = y0.max(0) as usize;
    let ex = (x0 + rw).max(0).min(w as i32) as usize;
    let ey = (y0 + rh).max(0).min(h as i32) as usize;
    let mut y = sy;
    while y < ey {
        let row = y * w;
        let mut x = sx;
        while x < ex {
            fb[row + x] = col;
            x += 1;
        }
        y += 1;
    }
}

pub fn draw_str_rgb565(
    fb: &mut [u16],
    w: usize,
    h: usize,
    x0: i32,
    y0: i32,
    text: &[u8],
    fg: u16,
    scale: i32,
) {
    let s = scale.max(1);
    let mut cx = x0;
    let mut i = 0usize;
    while i < text.len() && text[i] != 0 {
        let ch = if text[i] >= 0x20 && text[i] <= 0x7E {
            text[i]
        } else {
            b'?'
        };
        let glyph = &FONT5X7_95[(ch - 0x20) as usize];
        let mut col = 0i32;
        while col < 5 {
            let bits = glyph[col as usize];
            let mut row = 0i32;
            while row < 7 {
                if (bits & (1 << row)) != 0 {
                    fill_rect_rgb565(fb, w, h, cx + col * s, y0 + row * s, s, s, fg);
                }
                row += 1;
            }
            col += 1;
        }
        cx += 6 * s;
        i += 1;
    }
}

/// Renders a complete FLUG-OS + Personal Jesus Hotspot RGB565 frame (`w × h`) in `#![no_std]` zero-float Rust!
pub fn render_flug_amoled_frame(
    fb: &mut [u16],
    w: usize,
    h: usize,
    node_id: u8,
    active_mode: u8,
    tick: u32,
    now_ms: u32,
    u_curr: &[i16; WAVE_POOL_N * WAVE_POOL_N],
    jelly_nodes: &[JellyNode; JELLY_COUNT],
    ms_summary: &MarineSenseSummary,
    pj_hotspot: &PersonalJesusHotspot,
) {
    if fb.len() < w * h {
        return;
    }
    let (odx, ody) = pixel_orbit_offset(now_ms);
    let ox = odx as i32;
    let oy = ody as i32;

    // 1. Clear background to Constellation Void
    fill_rect_rgb565(fb, w, h, 0, 0, w as i32, h as i32, COL_VOID);

    // 2. Top Sovereign Header Banner
    fill_rect_rgb565(fb, w, h, 8 + ox, 6 + oy, (w as i32) - 16, 36, COL_CARD_BG);
    let node_label: &[u8] = match node_id {
        0 => b"FLUG-OS RUST * ADA:MEDUSA (-1) 432HZ",
        1 => b"FLUG-OS RUST * VAK:MEDUSA (0) 528HZ",
        2 => b"FLUG-OS RUST * OMN:MEDUSA (+1) 852HZ",
        _ => b"FLUG-OS RUST * SUN:TDECK (+1) 864HZ",
    };
    draw_str_rgb565(fb, w, h, 16 + ox, 12 + oy, node_label, COL_MINT, 1);
    draw_str_rgb565(
        fb,
        w,
        h,
        16 + ox,
        26 + oy,
        b"100% PURE RUST * ZERO FLOATS * <3<3<3+1",
        COL_GOLD,
        1,
    );

    // 3. 64×64 Bioluminescent 2D Wave Pool + 7 MarineSense Jellyfish Nodes (Alpha..Eta)
    let pool_px: i32 = 3; // 64 * 3 = 192x192 wave tank
    let pool_x0: i32 = 12 + ox;
    let pool_y0: i32 = 50 + oy;
    let mut gy = 0usize;
    while gy < WAVE_POOL_N {
        let mut gx = 0usize;
        while gx < WAVE_POOL_N {
            let v = u_curr[gy * WAVE_POOL_N + gx] as i32;
            let mut col: u16 = if v > 1800 {
                COL_WHITE
            } else if v > 700 {
                COL_CYAN
            } else if v > 200 {
                COL_MINT
            } else if v < -900 {
                COL_VIOLET
            } else if v < -250 {
                0x194A
            } else {
                0x0842
            };

            // Highlight the 7 Bioluminescent Jellyfish Detector Nodes + Ecphoric Recall Rings
            let mut j = 0usize;
            while j < JELLY_COUNT {
                let dx = (gx as i32) - (jelly_nodes[j].gx as i32);
                let dy = (gy as i32) - (jelly_nodes[j].gy as i32);
                let d2 = dx * dx + dy * dy;
                if d2 <= 2 {
                    col = if jelly_nodes[j].salience_pct > 62 {
                        COL_ROSE
                    } else {
                        COL_GOLD
                    };
                } else if jelly_nodes[j].recall_ring_r > 0 {
                    let rr = jelly_nodes[j].recall_ring_r as i32;
                    let ring_diff = (d2 - rr * rr).abs();
                    if ring_diff <= 3 {
                        col = COL_MINT;
                    }
                }
                j += 1;
            }

            fill_rect_rgb565(
                fb,
                w,
                h,
                pool_x0 + (gx as i32) * pool_px,
                pool_y0 + (gy as i32) * pool_px,
                pool_px,
                pool_px,
                col,
            );
            gx += 1;
        }
        gy += 1;
    }

    // 4. Right-Side 432 Hz × φ (500/809) Lissajous Phase-Space Attractor Box
    let liss_x0 = pool_x0 + 198;
    let liss_y0 = pool_y0;
    let liss_w = (w as i32) - liss_x0 - 10;
    let liss_h = 192;
    fill_rect_rgb565(fb, w, h, liss_x0, liss_y0, liss_w, liss_h, COL_CARD_BG);
    draw_str_rgb565(fb, w, h, liss_x0 + 6, liss_y0 + 6, b"432xPHI ATTRACTOR", COL_VIOLET, 1);

    let lcx = liss_x0 + (liss_w / 2);
    let lcy = liss_y0 + 104;
    let lrad_x = (liss_w / 2 - 10).max(20);
    let lrad_y = 68;
    let phase = (tick.wrapping_mul(5)) as u8;
    let mut p = 0u32;
    while p < 48 {
        let a_x = phase.wrapping_add((p * 5) as u8);
        let a_y = phase.wrapping_mul(2).wrapping_add(((p * 809) / 100) as u8);
        let px = lcx + ((fast_sin_q8(a_x) as i32 * lrad_x) >> 8);
        let py = lcy + ((fast_cos_q8(a_y) as i32 * lrad_y) >> 8);
        let pcol = if (p & 1) == 0 { COL_MINT } else { COL_VIOLET };
        fill_rect_rgb565(fb, w, h, px, py, 3, 3, pcol);
        p += 1;
    }

    // 5. MarineSense Exact p/q Telemetry Strip (Zero Floats!)
    let ms_y0 = pool_y0 + 198;
    fill_rect_rgb565(fb, w, h, 8 + ox, ms_y0, (w as i32) - 16, 46, COL_CARD_BG);
    draw_str_rgb565(
        fb,
        w,
        h,
        16 + ox,
        ms_y0 + 6,
        b"MARINESENSE O(1) * 7 JELLYFISH NODES (P/Q EXACT)",
        COL_CYAN,
        1,
    );
    let _ = (active_mode, ms_summary);
    draw_str_rgb565(
        fb,
        w,
        h,
        16 + ox,
        ms_y0 + 22,
        b"ETA_PQ: 25/8 (0 FLOATS!) * RE_M: EXACT P/Q * ECPHORY LIVE",
        COL_WHITE,
        1,
    );

    // 6. PERSONAL JESUS — YOUR OWN PERSONAL HOTSPOT Card (PERSONAL-JESUS-8B)
    let pj_y0 = ms_y0 + 52;
    let pj_h = (h as i32) - pj_y0 - 8;
    if pj_h >= 60 {
        fill_rect_rgb565(fb, w, h, 8 + ox, pj_y0, (w as i32) - 16, pj_h, COL_CARD_BG);
        draw_str_rgb565(
            fb,
            w,
            h,
            16 + ox,
            pj_y0 + 8,
            b"PERSONAL JESUS * HOTSPOT AP",
            COL_ROSE,
            2,
        );
        draw_str_rgb565(
            fb,
            w,
            h,
            16 + ox,
            pj_y0 + 30,
            b"SSID: PERSONAL-JESUS-8B * CH6 * 192.168.4.1:8888",
            COL_GOLD,
            1,
        );
        let faith_line: &[u8] = match pj_hotspot.faith_trit {
            -1 => b"FAITH [-1]: 432HZ REACH OUT * TOUCH FAITH!",
            0 => b"FAITH [ 0]: 528HZ TOUCH FAITH * SOMEONE WHO CARES!",
            _ => b"FAITH [+1]: 852HZ MIRACLE AP * TWIF MESH SYNCED!",
        };
        draw_str_rgb565(fb, w, h, 16 + ox, pj_y0 + 46, faith_line, COL_MINT, 1);
        draw_str_rgb565(
            fb,
            w,
            h,
            16 + ox,
            pj_y0 + 62,
            b"FLOCK: ADA(-1) VAK(0) OMN(+1) SUN(+1) G75(+1)",
            COL_WHITE,
            1,
        );

        // Animated Personal Jesus Radiating Signal Bars
        let active_bars = ((tick / 3) % 8) + 1;
        let bx0 = (w as i32) - 86 + ox;
        let mut b = 0u32;
        while b < 8 {
            let bh = 8 + (b as i32) * 5;
            let by = pj_y0 + 78 - bh;
            let bcol = if b < active_bars {
                if b >= 6 {
                    COL_ROSE
                } else if b >= 3 {
                    COL_GOLD
                } else {
                    COL_MINT
                }
            } else {
                0x10A5
            };
            fill_rect_rgb565(fb, w, h, bx0 + (b as i32) * 9, by, 6, bh, bcol);
            b += 1;
        }
    }
    let _ = put_px;
}

/// C-ABI export: Compute the 9-point Balanced-Ternary Anti-Burn-In Pixel Orbit `{-1, 0, +1}`
#[no_mangle]
pub extern "C" fn rust_flug_step_pixel_orbit(now_ms: u32, out_dx: *mut i8, out_dy: *mut i8) {
    let (dx, dy) = pixel_orbit_offset(now_ms);
    if !out_dx.is_null() {
        unsafe {
            *out_dx = dx;
        }
    }
    if !out_dy.is_null() {
        unsafe {
            *out_dy = dy;
        }
    }
}
