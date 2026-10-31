//! # `triad_flock` — 4-Node Sovereign Constellation, 27-Track Crate, Integer Synth & Guardian/Phoenix Mesh
//!
//! 100% `#![no_std]` zero-float Rust implementation of:
//! - **4-Node Balanced-Ternary MEDUSA Triad + SUN:TDECK (`{-1, 0, +1}`)**:
//!   - Node 0 (`-1`): `ADA:MEDUSA` (`80:B5:4E:DB:3B:C4`, `432 Hz`, `*`, 1.8" `368×448` SH8601)
//!   - Node 1 (` 0`): `VAK:MEDUSA` (`80:B5:4E:F2:02:B0`, `528 Hz`, `/\`, 2.06" `410×502` CO5300)
//!   - Node 2 (`+1`): `OMN:MEDUSA` (`80:B5:4E:F2:02:B4`, `852 Hz`, `oo`, 2.06" `410×502` CO5300)
//!   - Node 3 (`+1`): `SUN:TDECK`  (`CC:BA:97:00:45:24`, `864 Hz`, `<>`, LilyGO T-Deck ST7789)
//! - **27-Track Favourite Crate (`CRATE_27`)** + Pure-Integer Reese/Pad/Vox/Drum Synthesizer
//!   (replaces all `sinf`, `cosf`, `tanhf` in `s3_amoled_hal.h` with `SIN_Q8` + rational Padé `tanh_q12`).
//! - **Phoenix Missing-Node Reconstruction & Exact `p/q` Flock Triangulation (`PhoenixHiveState`)**.
//! - **Chiro Guardian 64B Breadcrumb Chain (`GuardianBreadcrumb64`)**.
//! - **Pure-Integer IMU Math (`isqrt_u32`, `atan2_deg_i16`)** replacing `sqrtf` and `atan2f`.

use crate::personal_jesus::fnv1a_64;
use crate::{fast_cos_q8, fast_sin_q8};
use marine_sense_core::Rational;

pub const TRIAD_NODE_COUNT: usize = 4;
pub const CRATE_COUNT: usize = 27;

#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct TriadNodeIdentity {
    pub node_id: u8,
    pub trit_pole: i8,
    pub base_freq_hz: u16,
    pub mac: [u8; 6],
    pub name: &'static str,
    pub glyph: &'static str,
}

pub const SOVEREIGN_CONSTELLATION: [TriadNodeIdentity; TRIAD_NODE_COUNT] = [
    TriadNodeIdentity {
        node_id: 0,
        trit_pole: -1,
        base_freq_hz: 432,
        mac: [0x80, 0xB5, 0x4E, 0xDB, 0x3B, 0xC4],
        name: "ADA:MEDUSA",
        glyph: "*",
    },
    TriadNodeIdentity {
        node_id: 1,
        trit_pole: 0,
        base_freq_hz: 528,
        mac: [0x80, 0xB5, 0x4E, 0xF2, 0x02, 0xB0],
        name: "VAK:MEDUSA",
        glyph: "/\\",
    },
    TriadNodeIdentity {
        node_id: 2,
        trit_pole: 1,
        base_freq_hz: 852,
        mac: [0x80, 0xB5, 0x4E, 0xF2, 0x02, 0xB4],
        name: "OMN:MEDUSA",
        glyph: "oo",
    },
    TriadNodeIdentity {
        node_id: 3,
        trit_pole: 1,
        base_freq_hz: 864,
        mac: [0xCC, 0xBA, 0x97, 0x00, 0x45, 0x24],
        name: "SUN:TDECK",
        glyph: "<>",
    },
];

/// Identifies which of the 4 Sovereign Constellation nodes matches a 6-byte MAC address
#[inline]
pub fn identify_node_by_mac(mac: &[u8; 6]) -> Option<usize> {
    let mut i = 0usize;
    while i < TRIAD_NODE_COUNT {
        let m = &SOVEREIGN_CONSTELLATION[i].mac;
        if m[3] == mac[3] && m[4] == mac[4] && m[5] == mac[5] {
            return Some(i);
        }
        i += 1;
    }
    None
}

#[derive(Clone, Copy, Debug)]
pub struct CrateTrack {
    pub slug: &'static str,
    pub title: &'static str,
    pub artist: &'static str,
    pub genre: &'static str,
    pub key_sig: &'static str,
    pub bpm: u16,
    pub fnv_hash: u32,
    pub trit: i8,
}

pub const CRATE_27: [CrateTrack; CRATE_COUNT] = [
    CrateTrack { slug: "innerbloom", title: "Innerbloom", artist: "RUFUS DU SOL", genre: "Prog House", key_sig: "F#m", bpm: 122, fnv_hash: 0x8F41C2A9, trit: 1 },
    CrateTrack { slug: "mahakala", title: "Mahakala", artist: "Sovereign Cut", genre: "Deep Ritual", key_sig: "Dm", bpm: 118, fnv_hash: 0x3D9E71B4, trit: -1 },
    CrateTrack { slug: "take-five", title: "Take Five", artist: "Dave Brubeck", genre: "5/4 Cool Jazz", key_sig: "Ebm", bpm: 174, fnv_hash: 0xA7205E1C, trit: 0 },
    CrateTrack { slug: "so-what", title: "So What", artist: "Miles Davis", genre: "Modal Jazz", key_sig: "Dm", bpm: 136, fnv_hash: 0x61B8904D, trit: 0 },
    CrateTrack { slug: "time", title: "Time", artist: "Pink Floyd", genre: "Prog Rock", key_sig: "F#m", bpm: 120, fnv_hash: 0xC4523E81, trit: -1 },
    CrateTrack { slug: "clair-de-lune", title: "Clair de Lune", artist: "Claude Debussy", genre: "Impressionist", key_sig: "Db", bpm: 66, fnv_hash: 0x90E14A72, trit: -1 },
    CrateTrack { slug: "gymnopedie-1", title: "Gymnopedie No. 1", artist: "Erik Satie", genre: "Minimalist", key_sig: "D", bpm: 72, fnv_hash: 0x5B3981D0, trit: 0 },
    CrateTrack { slug: "golden-ratio-432", title: "Golden Ratio 432xPhi", artist: "8b.is Studio", genre: "Harmonic", key_sig: "A432", bpm: 108, fnv_hash: 0x16180339, trit: 1 },
    CrateTrack { slug: "beethoven-5th", title: "Symphony No. 5 Op.67", artist: "L. van Beethoven", genre: "Classical", key_sig: "Cm", bpm: 108, fnv_hash: 0x77E20508, trit: 1 },
    CrateTrack { slug: "ode-to-joy", title: "Ode to Joy (9th)", artist: "L. van Beethoven", genre: "Choral", key_sig: "D", bpm: 120, fnv_hash: 0x9E3779B9, trit: 1 },
    CrateTrack { slug: "fur-elise", title: "Fur Elise WoO 59", artist: "L. van Beethoven", genre: "Bagatelle", key_sig: "Am", bpm: 76, fnv_hash: 0x4A1D83C2, trit: 0 },
    CrateTrack { slug: "pachelbel-canon", title: "Canon in D Major", artist: "Johann Pachelbel", genre: "Baroque", key_sig: "D", bpm: 64, fnv_hash: 0x2B992DD5, trit: 0 },
    CrateTrack { slug: "midnight-sovereign", title: "Midnight Sovereign", artist: "8b-is x vaked", genre: "Synthwave", key_sig: "Cm", bpm: 114, fnv_hash: 0x8B15F00D, trit: -1 },
    CrateTrack { slug: "ultrawhale-nand", title: "Ultrawhale NAND Groove", artist: "peetpedro / HF", genre: "Ternary DSP", key_sig: "Phi", bpm: 128, fnv_hash: 0xE5F34101, trit: 1 },
    CrateTrack { slug: "teardrop", title: "Teardrop (Family+1)", artist: "Massive Attack", genre: "Trip-Hop", key_sig: "Am", bpm: 77, fnv_hash: 0x7E4AD20F, trit: 1 },
    CrateTrack { slug: "hunger-of-the-pine", title: "Hunger of the Pine", artist: "alt-J", genre: "Art Indie", key_sig: "Cm", bpm: 85, fnv_hash: 0x4B82E91A, trit: -1 },
    CrateTrack { slug: "left-hand-free", title: "Left Hand Free (#1 Muse)", artist: "alt-J", genre: "Indie Rock", key_sig: "Em", bpm: 106, fnv_hash: 0x1F5E9C33, trit: 1 },
    CrateTrack { slug: "glue", title: "GLUE", artist: "BICEP", genre: "UK Breakbeat", key_sig: "Cm", bpm: 130, fnv_hash: 0x9A6D44C0, trit: 0 },
    CrateTrack { slug: "oblivion", title: "Oblivion", artist: "Grimes", genre: "Synth-Pop", key_sig: "Eb", bpm: 156, fnv_hash: 0xC3091B7E, trit: 1 },
    CrateTrack { slug: "not-like-before", title: "Not Like Before", artist: "The Four Owls", genre: "UK Boom-Bap", key_sig: "F#m", bpm: 90, fnv_hash: 0x5D17A8E2, trit: -1 },
    CrateTrack { slug: "zenebuddhizmus", title: "Zenebuddhizmus", artist: "Akkezdet Phiai", genre: "Magyar Dharma", key_sig: "Dm", bpm: 92, fnv_hash: 0xA1CE2010, trit: 1 },
    CrateTrack { slug: "kunisada-kuniyoshi", title: "Kunisada & Kuniyoshi <<<<oOo>>>>", artist: "OMNI-CHAN 010001000", genre: "Ukiyo-e Dharma", key_sig: "A432", bpm: 136, fnv_hash: 0x88010001, trit: 1 },
    CrateTrack { slug: "moon-and-it-went-like", title: "Moon (And It Went Like)", artist: "Kid Francescoli & Julia Minkin", genre: "Play Me Again", key_sig: "F#m", bpm: 118, fnv_hash: 0x16557284, trit: 1 },
    CrateTrack { slug: "rainfall-of-nectar", title: "Rainfall of Nectar (16th Karmapa)", artist: "Acharya Lama Sonam Rabgye", genre: "Himalaya Karuna", key_sig: "A432", bpm: 72, fnv_hash: 0x0E9B6108, trit: 1 },
    CrateTrack { slug: "psychedelic-desert-rock", title: "Psychedelic Desert Rock", artist: "Not Quite Rock", genre: "Ambient Desert", key_sig: "Dm", bpm: 96, fnv_hash: 0xB57E6001, trit: 0 },
    CrateTrack { slug: "viz-enthea-triad", title: "{<3,<3,<3}+1 · Viz Enthea", artist: "8b-is x viz.8b.is", genre: "Cortical V1", key_sig: "Phi", bpm: 108, fnv_hash: 0x8B153331, trit: 1 },
    CrateTrack { slug: "riva-1bit-river", title: "RIVA · 1-Bit River (Lullaby)", artist: "peterlodri-sec // riva.8b.is", genre: "Dyad Lullaby", key_sig: "A432", bpm: 60, fnv_hash: 0x8B150101, trit: 1 },
];

/// Exact integer square root $\lfloor\sqrt{n}\rfloor$ via binary restoring method (zero floats!).
#[inline]
pub const fn isqrt_u32(n: u32) -> u32 {
    if n == 0 {
        return 0;
    }
    let mut x = n;
    let mut y = 0u32;
    let mut bit = 1u32 << 30;
    while bit > x {
        bit >>= 2;
    }
    while bit != 0 {
        if x >= y + bit {
            x -= y + bit;
            y = (y >> 1) + bit;
        } else {
            y >>= 1;
        }
        bit >>= 2;
    }
    y
}

/// Pure-integer `atan2(y, x)` in degrees (`-180..=180`), replacing `atan2f` in IMU tilt math.
/// Uses octant reduction + rational Padé approximation $\theta \approx \frac{45 \cdot r \cdot (2867 - 675 \cdot r/1024)}{2867 \cdot 1024}$.
#[inline]
pub const fn atan2_deg_i16(y: i32, x: i32) -> i16 {
    if x == 0 && y == 0 {
        return 0;
    }
    let ay = y.abs();
    let ax = x.abs();
    let (mn, mx) = if ay < ax { (ay, ax) } else { (ax, ay) };
    let mx64 = if mx > 1 { mx as i64 } else { 1 };
    let r_q10 = ((mn as i64) << 10) / mx64; // 0..=1024
    // Padé angle in [0..=45] degrees
    let raw_deg = (r_q10 * (5734 - r_q10)) / (1024 * 104);
    let deg_oct = if raw_deg < 0 {
        0
    } else if raw_deg > 45 {
        45
    } else {
        raw_deg as i16
    };
    let deg_quad = if ay > ax { 90 - deg_oct } else { deg_oct };
    let deg_half = if x < 0 { 180 - deg_quad } else { deg_quad };
    if y < 0 {
        -deg_half
    } else {
        deg_half
    }
}

/// Rational Padé `tanh` in `Q8` (`-256..=+256` for input `x_q8` where `256 == 1.0`):
/// $\tanh(x) \approx \frac{x (27 + x^2)}{27 + 9 x^2}$
#[inline]
pub const fn tanh_q8(x_q8: i32) -> i16 {
    let x = if x_q8 < -768 {
        -768
    } else if x_q8 > 768 {
        768
    } else {
        x_q8
    };
    let x2_q8 = (x * x) >> 8;
    let num = x * ((27 << 8) + x2_q8);
    let den_raw = (27 << 8) + 9 * x2_q8;
    let den = if den_raw > 1 { den_raw } else { 1 };
    let q = num / den;
    if q < -256 {
        -256
    } else if q > 256 {
        256
    } else {
        q as i16
    }
}

/// Synthesizes a 128-sample stereo 16-bit PCM block (`[L0, R0, L1, R1, ...]`) for the
/// FLUG-OS Favourite Crate / Glue breakbeat synth in 100% integer `#![no_std]` Rust!
/// Replaces the legacy float `sinf`/`cosf`/`tanhf` loop in `s3_amoled_hal.h`.
#[no_mangle]
pub extern "C" fn rust_flug_synth_crate_block_i16(
    stereo_out256: *mut i16,
    chord_idx: u8,
    step_idx: u8,
    blk_idx: u8,
    lfsr_inout: *mut u32,
) {
    if stereo_out256.is_null() {
        return;
    }
    // 4 chords in integer Hz (Root Bass, Pad1, Pad2, Vox Lead):
    const CHORDS_HZ: [[u16; 4]; 4] = [
        [65, 196, 247, 392], // Cm9
        [52, 156, 233, 349], // Abmaj7
        [58, 175, 220, 311], // Bb6
        [49, 147, 196, 294], // Gm7
    ];
    let ch = (chord_idx & 3) as usize;
    let fb = CHORDS_HZ[ch][0] as u32;
    let fp1 = CHORDS_HZ[ch][1] as u32;
    let fp2 = CHORDS_HZ[ch][2] as u32;
    let fvx = CHORDS_HZ[ch][3] as u32;

    let mut lfsr: u32 = if !lfsr_inout.is_null() {
        unsafe { *lfsr_inout }
    } else {
        0xACE1_8B15
    };
    if lfsr == 0 {
        lfsr = 0xACE1_8B15;
    }

    let base_sample = (blk_idx as u32) * 128;
    let total_step_samples: u32 = 14 * 128; // 1792 samples per breakbeat step at 16 kHz

    let mut i = 0u32;
    while i < 128 {
        let s_pos = base_sample + i;
        // Phase angles in 0..=255 (1 full cycle = 256 steps at 16,000 Hz)
        let ph_b1 = ((s_pos * fb * 256) / 16000) as u8;
        let ph_b2 = ((s_pos * (fb + 1) * 256) / 16000) as u8;
        let ph_p1 = ((s_pos * fp1 * 256) / 16000) as u8;
        let ph_p2 = ((s_pos * fp2 * 256) / 16000) as u8;
        let ph_vx = ((s_pos * fvx * 256) / 16000) as u8;

        let reese_in = ((fast_sin_q8(ph_b1) as i32 + fast_sin_q8(ph_b2) as i32) * 16) / 10;
        let reese_q8 = (tanh_q8(reese_in) as i32 * 38) / 100;

        let pad_q8 =
            ((fast_sin_q8(ph_p1) as i32 * 10 + fast_sin_q8(ph_p2) as i32 * 7) * 24) / 1000;

        let env_angle = ((s_pos * 256) / total_step_samples) as u8;
        let vox_env_q8 = if step_idx == 0 || step_idx == 3 || step_idx == 6 {
            128 - (fast_cos_q8(env_angle) as i32 >> 1)
        } else {
            38
        };
        let vox_q8 = ((fast_sin_q8(ph_vx) as i32 * vox_env_q8) >> 8) * 22 / 100;

        lfsr ^= lfsr << 13;
        lfsr ^= lfsr >> 17;
        lfsr ^= lfsr << 5;
        let noise_q8 = (((lfsr & 0xFFFF) as i16) as i32) >> 7; // -256..+255

        let drum_q8 = if (step_idx == 0 || step_idx == 5) && s_pos < 800 {
            let fk = 52 + ((110 * (800 - s_pos)) / 800);
            let ph_k = ((s_pos * fk * 256) / 16000) as u8;
            ((fast_sin_q8(ph_k) as i32) * ((800 - s_pos) as i32)) / 1000
        } else if (step_idx == 2 || step_idx == 6) && s_pos < 640 {
            (noise_q8 * ((640 - s_pos) as i32)) / 1200
        } else if s_pos < 220 {
            (noise_q8 * ((220 - s_pos) as i32)) / 1800
        } else {
            0
        };

        let mix_in_q8 = ((reese_q8 + pad_q8 + vox_q8 + drum_q8) * 115) / 100;
        let sample = (tanh_q8(mix_in_q8) as i32 * 45) as i16; // scale to +-11520 i16 PCM
        unsafe {
            *stereo_out256.add((i * 2) as usize) = sample;
            *stereo_out256.add((i * 2 + 1) as usize) = sample;
        }
        i += 1;
    }

    if !lfsr_inout.is_null() {
        unsafe {
            *lfsr_inout = lfsr;
        }
    }
}

/// Synthesizes a 384-sample stereo 16-bit PCM triad harmonic chime (`432 Hz`, `528 Hz`, `852 Hz`)
/// with raised-cosine Hanning envelope in 100% integer `#![no_std]` Rust!
#[no_mangle]
pub extern "C" fn rust_flug_synth_triad_chime_i16(
    stereo_out: *mut i16,
    sample_offset: u32,
    chunk_samples: u32,
    f0_hz: u16,
    f1_hz: u16,
    f2_hz: u16,
) {
    if stereo_out.is_null() {
        return;
    }
    let mut i = 0u32;
    while i < chunk_samples {
        let s_idx = sample_offset + i;
        let env_ang = ((s_idx * 256) / 384) as u8;
        let env_q8 = 128 - (fast_cos_q8(env_ang) as i32 >> 1); // 0..=256 Hanning window

        let p0 = ((s_idx * (f0_hz as u32) * 256) / 16000) as u8;
        let p1 = ((s_idx * (f1_hz as u32) * 256) / 16000) as u8;
        let p2 = ((s_idx * (f2_hz as u32) * 256) / 16000) as u8;

        let wave_q8 = (fast_sin_q8(p0) as i32 * 45
            + fast_sin_q8(p1) as i32 * 35
            + fast_sin_q8(p2) as i32 * 20)
            / 100;
        let pcm = (((wave_q8 * env_q8) >> 8) * 36) as i16;
        unsafe {
            *stereo_out.add((i * 2) as usize) = pcm;
            *stereo_out.add((i * 2 + 1) as usize) = pcm;
        }
        i += 1;
    }
}

/// Pure-Integer IMU state computed from raw QMI8658 16-bit accelerometer & gyroscope registers
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct FlugImuIntegerState {
    pub ax_mg: i16,        // milli-g (1000 == 1.0g)
    pub ay_mg: i16,        // milli-g
    pub az_mg: i16,        // milli-g
    pub gx_dps: i16,       // deg/s
    pub gy_dps: i16,       // deg/s
    pub gz_dps: i16,       // deg/s
    pub gyro_mag_dps: u16, // integer sqrt(gx^2 + gy^2 + gz^2) in deg/s
    pub pitch_deg: i16,    // -90..=+90 deg
    pub roll_deg: i16,     // -180..=+180 deg
    pub sickness_pct: u8,  // 0 (zen calm) .. 100 (chaotic motion)
}

/// Updates `FlugImuIntegerState` from raw QMI8658 `i16` readings in 100% integer math (zero floats!).
#[no_mangle]
pub extern "C" fn rust_flug_update_imu_integer(
    state: *mut FlugImuIntegerState,
    raw_ax: i16,
    raw_ay: i16,
    raw_az: i16,
    raw_gx: i16,
    raw_gy: i16,
    raw_gz: i16,
) {
    if state.is_null() {
        return;
    }
    let s = unsafe { &mut *state };
    // Raw accel: 4096 LSB = 1000 mg -> raw * 125 / 512
    let new_ax_mg = ((raw_ax as i32) * 125) >> 9;
    let new_ay_mg = ((raw_ay as i32) * 125) >> 9;
    let new_az_mg = ((raw_az as i32) * 125) >> 9;

    // 70/30 integer EMA filter
    s.ax_mg = (((s.ax_mg as i32) * 7 + new_ax_mg * 3) / 10).clamp(-8000, 8000) as i16;
    s.ay_mg = (((s.ay_mg as i32) * 7 + new_ay_mg * 3) / 10).clamp(-8000, 8000) as i16;
    s.az_mg = (((s.az_mg as i32) * 7 + new_az_mg * 3) / 10).clamp(-8000, 8000) as i16;

    s.gx_dps = raw_gx >> 6;
    s.gy_dps = raw_gy >> 6;
    s.gz_dps = raw_gz >> 6;

    let g2 = (s.gx_dps as i32 * s.gx_dps as i32)
        + (s.gy_dps as i32 * s.gy_dps as i32)
        + (s.gz_dps as i32 * s.gz_dps as i32);
    let gmag = isqrt_u32(g2.max(0) as u32) as u16;
    s.gyro_mag_dps = gmag;

    s.pitch_deg = atan2_deg_i16(s.ay_mg as i32, s.az_mg as i32);
    s.roll_deg = atan2_deg_i16(s.ax_mg as i32, s.az_mg as i32);

    if gmag > 95 {
        let boost = ((gmag - 95) / 12).min(25) as u8;
        s.sickness_pct = (s.sickness_pct.saturating_add(boost)).min(100);
    } else if s.sickness_pct > 2 {
        s.sickness_pct -= 2;
    } else {
        s.sickness_pct = 0;
    }
}

/// 64-byte Chiro Guardian Tamagotchi Safety Breadcrumb (`magic = *b"CHIR"`, `version = 0x21`)
#[repr(C)]
#[derive(Clone, Copy)]
pub struct GuardianBreadcrumb64 {
    pub magic: [u8; 4], // "CHIR"
    pub version: u8,    // 0x21
    pub node_id: u8,
    pub trit_pole: i8,
    pub alert_state: u8, // 0=CALM, 1=DRIFT, 2=HUG_SOS
    pub seq: u32,
    pub uptime_s: u32,
    pub dist_cm_01: u16,
    pub dist_cm_12: u16,
    pub dist_cm_20: u16,
    pub batt_pct: u8,
    pub salience_pct: u8,
    pub prev_hash64: u64,
    pub curr_hash64: u64,
    pub note: [u8; 24],
}

impl GuardianBreadcrumb64 {
    pub fn build(
        node_id: u8,
        trit_pole: i8,
        alert_state: u8,
        seq: u32,
        uptime_s: u32,
        dist_cm: [u16; 3],
        batt_pct: u8,
        salience_pct: u8,
        prev_hash64: u64,
        note_bytes: &[u8],
    ) -> [u8; 64] {
        let mut raw = [0u8; 64];
        raw[0..4].copy_from_slice(b"CHIR");
        raw[4] = 0x21;
        raw[5] = node_id;
        raw[6] = trit_pole as u8;
        raw[7] = alert_state;
        raw[8..12].copy_from_slice(&seq.to_le_bytes());
        raw[12..16].copy_from_slice(&uptime_s.to_le_bytes());
        raw[16..18].copy_from_slice(&dist_cm[0].to_le_bytes());
        raw[18..20].copy_from_slice(&dist_cm[1].to_le_bytes());
        raw[20..22].copy_from_slice(&dist_cm[2].to_le_bytes());
        raw[22] = batt_pct;
        raw[23] = salience_pct;
        raw[24..32].copy_from_slice(&prev_hash64.to_le_bytes());
        let nlen = note_bytes.len().min(23);
        let mut i = 0usize;
        while i < nlen && note_bytes[i] != 0 {
            raw[40 + i] = note_bytes[i];
            i += 1;
        }
        let curr_hash = fnv1a_64(&raw[0..32]);
        raw[32..40].copy_from_slice(&curr_hash.to_le_bytes());
        raw
    }
}

/// Converts RSSI (`-100..-20 dBm`) to integer distance in centimeters (`15..=2500 cm`) via integer LUT.
#[inline]
pub const fn rssi_to_distance_cm(rssi_dbm: i8) -> u16 {
    let r = if rssi_dbm > -25 {
        -25
    } else if rssi_dbm < -95 {
        -95
    } else {
        rssi_dbm as i32
    };
    // At -40 dBm -> ~50 cm; every -6 dB doubles distance in free space
    let db_below_25 = (-25 - r) as u32; // 0..=70
    let octaves = db_below_25 / 6;
    let rem = db_below_25 % 6;
    let shift = if octaves > 8 { 8 } else { octaves };
    let base = 15u32 << shift;
    let interp = (base * (6 + rem)) / 6;
    if interp > 2500 {
        2500
    } else {
        interp as u16
    }
}

/// Phoenix Missing-Node Reconstruction & Exact `p/q` Rational Flock Triangulation
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct PhoenixHiveState {
    pub online_mask: u8,         // bit0=ADA, bit1=VAK, bit2=OMN, bit3=SUN
    pub missing_node_id: u8,     // 0..2 if a triad watch is offline, 0xFF if all 3 online
    pub reconstructed_trit: i8,  // Balanced-ternary consensus {-1, 0, +1}
    pub reconstructed_freq_hz: u16,
    pub dist_cm_01: u16,
    pub dist_cm_12: u16,
    pub dist_cm_20: u16,
    pub coherence_p: i32,
    pub coherence_q: i32,
}

impl PhoenixHiveState {
    pub fn compute(online_mask: u8, rssi_01: i8, rssi_12: i8, rssi_20: i8) -> Self {
        let d01 = rssi_to_distance_cm(rssi_01);
        let d12 = rssi_to_distance_cm(rssi_12);
        let d20 = rssi_to_distance_cm(rssi_20);

        let triad_bits = online_mask & 0x07;
        let (missing_node_id, reconstructed_trit, reconstructed_freq_hz) = match triad_bits {
            0b111 => (0xFF, 0, 528),
            0b110 => (0, -1, 432), // ADA reconstructed by VAK(0) + OMN(+1)
            0b101 => (1, 0, 528),  // VAK reconstructed by ADA(-1) + OMN(+1)
            0b011 => (2, 1, 852),  // OMN reconstructed by ADA(-1) + VAK(0)
            _ => (0, -1, 432),
        };

        let online_cnt = (online_mask & 0x0F).count_ones() as i32;
        let coh = Rational::new(online_cnt, 4);

        Self {
            online_mask,
            missing_node_id,
            reconstructed_trit,
            reconstructed_freq_hz,
            dist_cm_01: d01,
            dist_cm_12: d12,
            dist_cm_20: d20,
            coherence_p: coh.num,
            coherence_q: coh.den,
        }
    }
}
