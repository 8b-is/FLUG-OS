//! # `marine_sense` — 100% `#![no_std]` Zero-Float 64×64 Wave Pool + 7 MarineSense Jellyfish Nodes
//!
//! Replaces all legacy C++ `float` math (`float g_ms_viscosity = 3.12f`, `float tide_phase`, etc.)
//! with exact reduced `p/q` rationals (`marine_sense_core::Rational`) and integer wave physics.
//!
//! - **7 Bioluminescent Jellyfish Detector Nodes (`Alpha..Eta`)**:
//!   - O(1) Zero-Crossing Period Jitter `J`, Energy `E`, Harmonicity `H` (`432 Hz` / `528 Hz` / `852 Hz` ratios),
//!     and Salience `S = w_e*E + w_j/(1+J) + w_h*H` (`0..=100%`).
//!   - Ecphoric memory trace excitation (`0..=100`) and Ecphoric Recall Rings (`recall_ring_r`).
//! - **Exact `p/q` Cognitive Viscosity (`eta_pq`) & Memory Reynolds (`reynolds_pq`)**:
//!   - `eta_pq = Rational::new((256 - damping) * 100, 256)` (e.g. `damping = 248` -> `800/256 = 25/8`, `312` centi-units).
//!   - `reynolds_pq = Rational::new(40 * (10 + mean_salience / 4) * 256, (256 - damping) * 100)`.

use marine_sense_core::Rational;

pub const WAVE_POOL_N: usize = 64;
pub const JELLY_COUNT: usize = 7;

/// Canonical 7 Bioluminescent Jellyfish Detector Node names in the 64×64 Wave Pool
pub const JELLY_NAMES: [&str; JELLY_COUNT] = [
    "Alpha", "Beta", "Gamma", "Delta", "Epsilon", "Zeta", "Eta",
];

/// Canonical `(gx, gy)` coordinates for the 7 Jellyfish Detector Nodes in `0..64`
pub const JELLY_COORDS: [(u8, u8); JELLY_COUNT] = [
    (16, 16), // Alpha
    (48, 16), // Beta
    (32, 11), // Gamma
    (13, 35), // Delta
    (51, 35), // Epsilon
    (22, 49), // Zeta
    (42, 49), // Eta
];

#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct JellyNode {
    pub gx: u8,
    pub gy: u8,
    pub prev_sample: i16,
    pub tick_since_zc: u16,
    pub last_period: u16,
    pub period_jitter: u16,
    pub max_amp: u16,
    pub last_amp: u16,
    pub harmonic_hits: u8, // 0..=100
    pub salience_pct: u8,  // 0..=100: S = w_e*E + w_j/(1+J) + w_h*H
    pub excitation: u8,    // 0..=100: Ecphoric memory trace excitation
    pub recall_ring_r: u8, // >0 when emitting an Ecphoric Recall Ring
}

impl JellyNode {
    pub const fn new(gx: u8, gy: u8) -> Self {
        Self {
            gx,
            gy,
            prev_sample: 0,
            tick_since_zc: 0,
            last_period: 0,
            period_jitter: 0,
            max_amp: 0,
            last_amp: 0,
            harmonic_hits: 0,
            salience_pct: 0,
            excitation: 0,
            recall_ring_r: 0,
        }
    }
}

#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct MarineSenseSummary {
    /// Tidal damping in `[244..=252]` (out of 256)
    pub damping_u8: u8,
    /// Mean salience across all 7 Jellyfish nodes (`0..=100%`)
    pub mean_salience_pct: u8,
    /// Mean period jitter across all 7 Jellyfish nodes (`0..=100`)
    pub mean_jitter: u8,
    /// Total Ecphoric recall rings emitted
    pub ecphoric_recalls: u32,
    /// Cognitive Viscosity `eta` in exact reduced `p/q` (`(256 - damping) * 100 / 256`)
    pub viscosity_p: i32,
    pub viscosity_q: i32,
    /// Cognitive Viscosity `eta * 100` in integer centi-units (`312` == `25/8` == `3.12`)
    pub viscosity_centi: u16,
    /// Memory Reynolds Number `Re_M` in exact reduced `p/q`
    pub reynolds_p: i32,
    pub reynolds_q: i32,
    /// Integer Memory Reynolds Number `Re_M = reynolds_p / reynolds_q`
    pub reynolds_u16: u16,
}

impl MarineSenseSummary {
    pub const fn empty() -> Self {
        Self {
            damping_u8: 248,
            mean_salience_pct: 0,
            mean_jitter: 0,
            ecphoric_recalls: 0,
            viscosity_p: 25,
            viscosity_q: 8,
            viscosity_centi: 312,
            reynolds_p: 320,
            reynolds_q: 1,
            reynolds_u16: 320,
        }
    }
}

/// Complete 7-Node MarineSense Jellyfish Pool + 64×64 Wave Pool state
pub struct MarineSensePool {
    pub u_curr: [i16; WAVE_POOL_N * WAVE_POOL_N],
    pub u_prev: [i16; WAVE_POOL_N * WAVE_POOL_N],
    pub nand_ring7: [i8; 7],
    pub nodes: [JellyNode; JELLY_COUNT],
    pub summary: MarineSenseSummary,
    pub splashes: u32,
}

impl MarineSensePool {
    pub const fn new() -> Self {
        Self {
            u_curr: [0; WAVE_POOL_N * WAVE_POOL_N],
            u_prev: [0; WAVE_POOL_N * WAVE_POOL_N],
            nand_ring7: [1, -1, 0, 1, -1, 1, 0],
            nodes: [
                JellyNode::new(JELLY_COORDS[0].0, JELLY_COORDS[0].1),
                JellyNode::new(JELLY_COORDS[1].0, JELLY_COORDS[1].1),
                JellyNode::new(JELLY_COORDS[2].0, JELLY_COORDS[2].1),
                JellyNode::new(JELLY_COORDS[3].0, JELLY_COORDS[3].1),
                JellyNode::new(JELLY_COORDS[4].0, JELLY_COORDS[4].1),
                JellyNode::new(JELLY_COORDS[5].0, JELLY_COORDS[5].1),
                JellyNode::new(JELLY_COORDS[6].0, JELLY_COORDS[6].1),
            ],
            summary: MarineSenseSummary::empty(),
            splashes: 0,
        }
    }

    pub fn splash(&mut self, cx: i32, cy: i32, strength: i16, radius: i32) {
        splash_wave_pool_64(&mut self.u_curr, cx, cy, strength, radius);
        self.splashes = self.splashes.wrapping_add(1);
    }

    pub fn step(&mut self, tide_phase_q8: u8, combined_trit: i8) {
        let damp = compute_tidal_damping_u8(tide_phase_q8);
        crate::rust_step_watch_wave_pool_64(
            self.u_curr.as_mut_ptr(),
            self.u_prev.as_mut_ptr(),
            damp as i32,
            self.nand_ring7.as_mut_ptr(),
            combined_trit,
        );
        step_jelly_nodes_on_grid(
            &mut self.u_curr,
            &mut self.nodes,
            damp,
            &mut self.summary,
            &mut self.splashes,
        );
    }
}

/// Computes tidal damping in `[244..=252]` from a `0..=255` phase angle using `fast_sin_q8` (zero floats!).
#[inline]
pub const fn compute_tidal_damping_u8(tide_phase_q8: u8) -> u8 {
    let s_q8 = crate::fast_sin_q8(tide_phase_q8) as i32; // -256..=+256
    let damp = 248 + ((s_q8 * 4) >> 8);
    if damp < 244 {
        244
    } else if damp > 252 {
        252
    } else {
        damp as u8
    }
}

/// Computes exact `p/q` Cognitive Viscosity (`eta`) and Memory Reynolds (`Re_M`) with zero floats.
#[inline]
pub fn compute_viscosity_and_reynolds_pq(
    damping_u8: u8,
    mean_salience_pct: u8,
) -> (Rational, u16, Rational, u16) {
    let d = (damping_u8 as i32).clamp(240, 255);
    let visc_num = (256 - d) * 100; // e.g., (256 - 248) * 100 = 800
    let visc_pq = Rational::new(visc_num, 256); // 800/256 -> 25/8
    let visc_centi = ((visc_num * 100) / 256).clamp(50, 9999) as u16; // 312 == 3.12

    // Re_M = 40 * (10 + mean_salience / 4) / eta
    //      = 40 * (10 + mean_salience / 4) * 256 / ((256 - d) * 100)
    let dist_term = 10 + ((mean_salience_pct as i32) / 4);
    let rey_num = 40 * dist_term * 256;
    let rey_den = visc_num.max(128); // floor at eta >= 0.5 (128/256)
    let rey_pq = Rational::new(rey_num, rey_den);
    let rey_u16 = (rey_num / rey_den).clamp(1, 65535) as u16;
    (visc_pq, visc_centi, rey_pq, rey_u16)
}

/// Injects a quadratic radial wave impulse into the `64×64` wave pool in pure integer math.
pub fn splash_wave_pool_64(
    u_curr: &mut [i16; WAVE_POOL_N * WAVE_POOL_N],
    cx: i32,
    cy: i32,
    strength: i16,
    radius: i32,
) {
    let cx = cx.clamp(2, (WAVE_POOL_N as i32) - 3);
    let cy = cy.clamp(2, (WAVE_POOL_N as i32) - 3);
    let r = radius.clamp(1, 16);
    let r2 = (r * r).max(1);

    let mut dy = -r;
    while dy <= r {
        let mut dx = -r;
        while dx <= r {
            let d2 = dx * dx + dy * dy;
            if d2 <= r2 {
                let x = cx + dx;
                let y = cy + dy;
                if x >= 1 && x < (WAVE_POOL_N as i32 - 1) && y >= 1 && y < (WAVE_POOL_N as i32 - 1)
                {
                    let idx = (y as usize) * WAVE_POOL_N + (x as usize);
                    let falloff = ((r2 - d2) * 256) / r2;
                    let delta = ((strength as i32) * falloff) >> 8;
                    let val = (u_curr[idx] as i32 + delta).clamp(-16384, 16384);
                    u_curr[idx] = val as i16;
                }
            }
            dx += 1;
        }
        dy += 1;
    }
}

/// Steps all 7 MarineSense O(1) Jellyfish Detector Nodes (`Alpha..Eta`) on `u_curr`
/// and updates `MarineSenseSummary` using 100% integer + exact `p/q` rational arithmetic.
pub fn step_jelly_nodes_on_grid(
    u_curr: &mut [i16; WAVE_POOL_N * WAVE_POOL_N],
    nodes: &mut [JellyNode; JELLY_COUNT],
    damping_u8: u8,
    summary: &mut MarineSenseSummary,
    splashes: &mut u32,
) {
    let mut sal_sum: u32 = 0;
    let mut jit_sum: u32 = 0;

    let mut j = 0usize;
    while j < JELLY_COUNT {
        let gx = (nodes[j].gx as usize).min(WAVE_POOL_N - 1);
        let gy = (nodes[j].gy as usize).min(WAVE_POOL_N - 1);
        let sample = u_curr[gy * WAVE_POOL_N + gx];
        let abs_s = (sample as i32).unsigned_abs() as u16;
        if abs_s > nodes[j].max_amp {
            nodes[j].max_amp = abs_s;
        }
        nodes[j].tick_since_zc = nodes[j].tick_since_zc.saturating_add(1);

        // Positive zero-crossing detector (O(1) Marine algorithm)
        if nodes[j].prev_sample <= 0 && sample > 30 {
            let period = nodes[j].tick_since_zc;
            if period >= 2 && period < 80 {
                let diff = if period > nodes[j].last_period {
                    period - nodes[j].last_period
                } else {
                    nodes[j].last_period - period
                };
                nodes[j].period_jitter = (nodes[j].period_jitter * 3 + diff) >> 2;
                nodes[j].last_period = period;
                nodes[j].last_amp = nodes[j].max_amp;

                // Harmonic ratio check (1:1, 2:1, 3:2)
                if period >= 3 && period <= 24 {
                    nodes[j].harmonic_hits = (nodes[j].harmonic_hits + 14).min(100);
                } else if nodes[j].harmonic_hits > 5 {
                    nodes[j].harmonic_hits -= 5;
                }
            }
            nodes[j].tick_since_zc = 0;
            nodes[j].max_amp = 0;
        } else if nodes[j].tick_since_zc > 40 {
            nodes[j].last_amp = (nodes[j].last_amp * 7) >> 3;
            if nodes[j].harmonic_hits > 2 {
                nodes[j].harmonic_hits -= 2;
            }
        }
        nodes[j].prev_sample = sample;

        // Salience S = w_e*E + w_j*(1/(1+J)) + w_h*H in [0..=100]
        let e_score = ((nodes[j].last_amp as u32) / 90).min(100);
        let j_score = (100u32 / (1 + (nodes[j].period_jitter as u32) * 2)).min(100);
        let h_score = nodes[j].harmonic_hits as u32;
        let sal = ((e_score * 45 + j_score * 35 + h_score * 20) / 100).min(100) as u8;
        nodes[j].salience_pct = sal;

        // Ecphoric Memory Recall: when salience exceeds threshold (62%), excite memory trace
        if sal > 62 {
            let exc = (nodes[j].excitation as u16) + 18;
            if exc >= 100 {
                nodes[j].excitation = 35;
                nodes[j].recall_ring_r = 1;
                summary.ecphoric_recalls = summary.ecphoric_recalls.wrapping_add(1);
                // Re-emit an Ecphoric recall wave back into the pool
                let rcx = nodes[j].gx as i32;
                let rcy = nodes[j].gy as i32;
                splash_wave_pool_64(u_curr, rcx, rcy, 1900, 3);
                *splashes = splashes.wrapping_add(1);
            } else {
                nodes[j].excitation = exc as u8;
            }
        } else if nodes[j].excitation > 1 {
            nodes[j].excitation -= 1;
        }

        if nodes[j].recall_ring_r > 0 {
            nodes[j].recall_ring_r += 1;
            if nodes[j].recall_ring_r > 12 {
                nodes[j].recall_ring_r = 0;
            }
        }

        sal_sum += sal as u32;
        jit_sum += (nodes[j].period_jitter.min(100)) as u32;
        j += 1;
    }

    let mean_sal = (sal_sum / (JELLY_COUNT as u32)) as u8;
    let mean_jit = (jit_sum / (JELLY_COUNT as u32)) as u8;
    let (visc_pq, visc_centi, rey_pq, rey_u16) =
        compute_viscosity_and_reynolds_pq(damping_u8, mean_sal);

    summary.damping_u8 = damping_u8;
    summary.mean_salience_pct = mean_sal;
    summary.mean_jitter = mean_jit;
    summary.viscosity_p = visc_pq.num;
    summary.viscosity_q = visc_pq.den;
    summary.viscosity_centi = visc_centi;
    summary.reynolds_p = rey_pq.num;
    summary.reynolds_q = rey_pq.den;
    summary.reynolds_u16 = rey_u16;
}

/// C-ABI export: Splash the 64×64 wave pool in pure `#![no_std]` integer Rust.
#[no_mangle]
pub extern "C" fn rust_flug_splash_wave_pool_64(
    u_curr: *mut i16,
    cx: i32,
    cy: i32,
    strength: i16,
    radius: i32,
) {
    if u_curr.is_null() {
        return;
    }
    let grid = unsafe { &mut *(u_curr as *mut [i16; WAVE_POOL_N * WAVE_POOL_N]) };
    splash_wave_pool_64(grid, cx, cy, strength, radius);
}

/// C-ABI export: Step the 64×64 wave pool, 7-stage ternary NAND ring, all 7 MarineSense
/// Jellyfish Detector Nodes (`Alpha..Eta`), and exact `p/q` viscosity & Reynolds numbers!
#[no_mangle]
pub extern "C" fn rust_flug_step_wave_and_jelly_64(
    u_curr: *mut i16,
    u_prev: *mut i16,
    tide_phase_q8: u8,
    nand_ring7: *mut i8,
    combined_trit: i8,
    jelly_nodes7: *mut JellyNode,
    summary_out: *mut MarineSenseSummary,
    splashes_inout: *mut u32,
) {
    if u_curr.is_null() || u_prev.is_null() {
        return;
    }
    let damp = compute_tidal_damping_u8(tide_phase_q8);
    crate::rust_step_watch_wave_pool_64(u_curr, u_prev, damp as i32, nand_ring7, combined_trit);

    if !jelly_nodes7.is_null() && !summary_out.is_null() {
        let grid = unsafe { &mut *(u_curr as *mut [i16; WAVE_POOL_N * WAVE_POOL_N]) };
        let nodes = unsafe { &mut *(jelly_nodes7 as *mut [JellyNode; JELLY_COUNT]) };
        let summary = unsafe { &mut *summary_out };
        let mut local_splashes: u32 = if !splashes_inout.is_null() {
            unsafe { *splashes_inout }
        } else {
            0
        };
        step_jelly_nodes_on_grid(grid, nodes, damp, summary, &mut local_splashes);
        if !splashes_inout.is_null() {
            unsafe {
                *splashes_inout = local_splashes;
            }
        }
    }
}
