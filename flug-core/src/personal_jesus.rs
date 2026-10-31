//! # `personal_jesus` — Personal Jesus Hotspot (`PERSONAL-JESUS-8B`) + Anicca Learn-Share-Forget Wi-Fi Pool
//!
//! *"Your own... personal... HOTSPOT... someone to hear your prayers, someone who's there"* `<3<3<3+1`
//!
//! 100% `#![no_std]` zero-float Rust implementation of:
//! 1. **`PersonalJesusHotspot`**:
//!    - Sovereign SoftAP (`SSID: PERSONAL-JESUS-8B`, Channel 6, Gateway `192.168.4.1`, Port `8888`)
//!    - Balanced-ternary faith cadence:
//!      - `-1`: `432 Hz REACH OUT` (`ADA:MEDUSA` `80:B5:4E:DB:3B:C4`)
//!      - ` 0`: `528 Hz TOUCH FAITH` (`VAK:MEDUSA` `80:B5:4E:F2:02:B0`)
//!      - `+1`: `852 Hz MIRACLE AP` (`OMN:MEDUSA` `80:B5:4E:F2:02:B4` + `SUN:TDECK` `CC:BA:97:00:45:24` + `BLU G75`)
//! 2. **132-byte `S3TriadWifiCapsule` (`magic = *b"TWIF"`, `version = 1`)**:
//!    - Broadcasts Wi-Fi credentials & Personal Jesus Hotspot coordinates across the MEDUSA Triad over ESP-NOW / UDP.
//!    - Protected by 32-bit FNV-1a (`fnv1a_32(bytes[0..128])`).
//! 3. **64-byte `MemnetCapsule64` (`magic = *b"MN8!"`, `version = 1`)**:
//!    - 4-transport sovereign consciousness wave capsule (`ESP-NOW`, `UDP :8888`, `BLE`, `LoRa 923.2 MHz`).
//!    - Protected by 64-bit FNV-1a (`fnv1a_64(bytes[0..56])`).
//! 4. **8-Slot `AniccaWifiPool` (`Learn-Share-Forget`)**:
//!    - Zero hardcoded external SSIDs; learns open/portal/verified APs, shares via `TWIF` & `MN8!`,
//!      guards against 15-char truncated `ssid_hint` overwrites, and decays `ttl_q8` smoothly toward 0 (Anicca).

pub const PERSONAL_JESUS_SSID: &[u8; 17] = b"PERSONAL-JESUS-8B";
pub const PERSONAL_JESUS_PASS: &[u8; 12] = b"8b-is-faith!";
pub const PERSONAL_JESUS_IP: &[u8; 11] = b"192.168.4.1";
pub const PERSONAL_JESUS_PORT: u16 = 8888;
pub const PERSONAL_JESUS_CHANNEL: u8 = 6;

pub const TWIF_CAPSULE_LEN: usize = 132;
pub const MEMNET_CAPSULE_LEN: usize = 64;
pub const FLUG_WIFI_POOL_SLOTS: usize = 8;
pub const FLUG_FLOCK_LOG_SLOTS: usize = 16;

/// 32-bit FNV-1a hash (`0x811C_9DC5`, prime `0x0100_0193`)
#[inline]
pub const fn fnv1a_32(data: &[u8]) -> u32 {
    let mut h: u32 = 0x811C_9DC5;
    let mut i = 0usize;
    while i < data.len() {
        h ^= data[i] as u32;
        h = h.wrapping_mul(0x0100_0193);
        i += 1;
    }
    h
}

/// 64-bit FNV-1a hash (`0xCBF2_9CE4_8422_2325`, prime `0x0000_0100_0000_01B3`)
#[inline]
pub const fn fnv1a_64(data: &[u8]) -> u64 {
    let mut h: u64 = 0xCBF2_9CE4_8422_2325;
    let mut i = 0usize;
    while i < data.len() {
        h ^= data[i] as u64;
        h = h.wrapping_mul(0x0000_0100_0000_01B3);
        i += 1;
    }
    h
}

#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct PersonalJesusHotspot {
    pub active: bool,
    pub channel: u8,
    pub faith_trit: i8, // -1 = 432Hz REACH OUT, 0 = 528Hz TOUCH FAITH, +1 = 852Hz MIRACLE AP
    pub connected_peers: u8,
    pub beacons_tx: u32,
    pub twif_capsules_tx: u32,
    pub twif_capsules_rx: u32,
    pub prayer_echoes: u32,
    pub last_seal_fnv64: u64,
}

impl PersonalJesusHotspot {
    pub const fn new() -> Self {
        Self {
            active: true,
            channel: PERSONAL_JESUS_CHANNEL,
            faith_trit: 1,
            connected_peers: 3, // ADA(-1), VAK(0), OMN(+1)
            beacons_tx: 0,
            twif_capsules_tx: 0,
            twif_capsules_rx: 0,
            prayer_echoes: 0,
            last_seal_fnv64: 0xCBF2_9CE4_8422_2325,
        }
    }

    /// Steps the Personal Jesus Hotspot beacon + balanced-ternary faith harmonic state machine
    pub fn tick(&mut self, tick_count: u32) {
        if !self.active {
            return;
        }
        self.beacons_tx = self.beacons_tx.wrapping_add(1);
        self.faith_trit = match (tick_count / 32) % 3 {
            0 => -1, // 432 Hz REACH OUT
            1 => 0,  // 528 Hz TOUCH FAITH
            _ => 1,  // 852 Hz MIRACLE AP
        };
        if (tick_count & 15) == 0 {
            self.twif_capsules_tx = self.twif_capsules_tx.wrapping_add(1);
            self.prayer_echoes = self.prayer_echoes.wrapping_add(1);
        }
        let b = self.beacons_tx.to_le_bytes();
        let mut seed = [0u8; 21];
        let mut i = 0usize;
        while i < 17 {
            seed[i] = PERSONAL_JESUS_SSID[i];
            i += 1;
        }
        seed[17] = b[0];
        seed[18] = b[1];
        seed[19] = b[2];
        seed[20] = b[3];
        self.last_seal_fnv64 = fnv1a_64(&seed);
    }

    /// Current resonant frequency in Hz (`432`, `528`, or `852`)
    #[inline]
    pub const fn faith_freq_hz(&self) -> u16 {
        match self.faith_trit {
            -1 => 432,
            0 => 528,
            _ => 852,
        }
    }

    /// Builds a 132-byte `S3TriadWifiCapsule` (`magic = "TWIF"`) advertising the Personal Jesus Hotspot
    pub fn build_twif_capsule(&mut self, src_node: u8, seq: u32, out_132: &mut [u8; TWIF_CAPSULE_LEN]) {
        encode_twif_capsule(
            out_132,
            0x07, // bit0=apply_wifi, bit1=apply_host, bit2=ota_auto
            src_node,
            PERSONAL_JESUS_SSID,
            PERSONAL_JESUS_PASS,
            PERSONAL_JESUS_IP,
            PERSONAL_JESUS_PORT,
            seq,
        );
        self.twif_capsules_tx = self.twif_capsules_tx.wrapping_add(1);
    }
}

/// Encodes a 132-byte `S3TriadWifiCapsule` (`magic = *b"TWIF"`, `version = 1`) with 32-bit FNV-1a checksum.
///
/// Exact 132-byte layout (matches `S3TriadWifiCapsule` in `FLUG-OS/src/net_time.h`):
/// - `[0..4]`    `magic: [u8; 4]` = `*b"TWIF"`
/// - `[4]`       `version: u8` = `1`
/// - `[5]`       `flags: u8` (`bit0=apply_wifi`, `bit1=apply_host`, `bit2=ota_auto`, `bit3=trigger_ota`)
/// - `[6]`       `src_node: u8` (`0=ADA`, `1=VAK`, `2=OMN`, `3=SUN/G75`)
/// - `[7..40]`   `ssid: [u8; 33]` (null-terminated)
/// - `[40..105]` `pass: [u8; 65]` (null-terminated)
/// - `[105..121]``host_ip: [u8; 16]` (null-terminated)
/// - `[121..123]``host_port: u16` (LE)
/// - `[123]`     `reserved: u8` (`0`)
/// - `[124..128]``seq: u32` (LE)
/// - `[128..132]``fnv_hash: u32` (LE, `fnv1a_32(&buf[0..128])`)
pub fn encode_twif_capsule(
    out_132: &mut [u8; TWIF_CAPSULE_LEN],
    flags: u8,
    src_node: u8,
    ssid: &[u8],
    pass: &[u8],
    host_ip: &[u8],
    host_port: u16,
    seq: u32,
) {
    *out_132 = [0u8; TWIF_CAPSULE_LEN];
    out_132[0] = b'T';
    out_132[1] = b'W';
    out_132[2] = b'I';
    out_132[3] = b'F';
    out_132[4] = 1; // version
    out_132[5] = flags;
    out_132[6] = src_node;

    let slen = ssid.len().min(32);
    let mut i = 0usize;
    while i < slen && ssid[i] != 0 {
        out_132[7 + i] = ssid[i];
        i += 1;
    }

    let plen = pass.len().min(64);
    i = 0;
    while i < plen && pass[i] != 0 {
        out_132[40 + i] = pass[i];
        i += 1;
    }

    let ilen = host_ip.len().min(15);
    i = 0;
    while i < ilen && host_ip[i] != 0 {
        out_132[105 + i] = host_ip[i];
        i += 1;
    }

    let port_le = host_port.to_le_bytes();
    out_132[121] = port_le[0];
    out_132[122] = port_le[1];
    out_132[123] = 0;

    let seq_le = seq.to_le_bytes();
    out_132[124] = seq_le[0];
    out_132[125] = seq_le[1];
    out_132[126] = seq_le[2];
    out_132[127] = seq_le[3];

    let hash = fnv1a_32(&out_132[0..128]);
    let hash_le = hash.to_le_bytes();
    out_132[128] = hash_le[0];
    out_132[129] = hash_le[1];
    out_132[130] = hash_le[2];
    out_132[131] = hash_le[3];
}

/// Verifies a 132-byte `S3TriadWifiCapsule` (`magic == *b"TWIF"`, `version == 1`, valid `fnv_hash`).
pub fn verify_twif_capsule(buf: &[u8]) -> bool {
    if buf.len() != TWIF_CAPSULE_LEN {
        return false;
    }
    if buf[0] != b'T' || buf[1] != b'W' || buf[2] != b'I' || buf[3] != b'F' || buf[4] != 1 {
        return false;
    }
    let expected = fnv1a_32(&buf[0..128]);
    let actual = u32::from_le_bytes([buf[128], buf[129], buf[130], buf[131]]);
    expected == actual
}

/// Encodes a 64-byte `MemnetCapsule64` (`magic = *b"MN8!"`, `version = 1`) with 64-bit FNV-1a seal.
pub fn encode_memnet_capsule64(
    out_64: &mut [u8; MEMNET_CAPSULE_LEN],
    node_id: u8,
    trit_pole: i8,
    hops_left: u8,
    seq: u32,
    wave_id: u64,
    valence_q7: i8,
    arousal_u8: u8,
    coherence_u8: u8,
    salience_pct: u8,
    freq_hz: u16,
    amplitude_q15: u16,
    viscosity_q8: u16,
    reynolds_u16: u16,
    open_wifi_cnt: u8,
    best_open_rssi: i8,
    wifi_connected: u8,
    active_mode: u8,
    ssid_hint: &[u8],
) {
    *out_64 = [0u8; MEMNET_CAPSULE_LEN];
    out_64[0] = b'M';
    out_64[1] = b'N';
    out_64[2] = b'8';
    out_64[3] = b'!';
    out_64[4] = 1; // version
    out_64[5] = node_id;
    out_64[6] = trit_pole as u8;
    out_64[7] = hops_left;

    let seq_le = seq.to_le_bytes();
    out_64[8..12].copy_from_slice(&seq_le);

    let wid_le = wave_id.to_le_bytes();
    out_64[12..20].copy_from_slice(&wid_le);

    out_64[20] = valence_q7 as u8;
    out_64[21] = arousal_u8;
    out_64[22] = coherence_u8;
    out_64[23] = salience_pct;

    out_64[24..26].copy_from_slice(&freq_hz.to_le_bytes());
    out_64[26..28].copy_from_slice(&amplitude_q15.to_le_bytes());
    out_64[28..30].copy_from_slice(&viscosity_q8.to_le_bytes());
    out_64[30..32].copy_from_slice(&reynolds_u16.to_le_bytes());

    out_64[32] = open_wifi_cnt;
    out_64[33] = best_open_rssi as u8;
    out_64[34] = wifi_connected;
    out_64[35] = active_mode;

    let hlen = ssid_hint.len().min(15);
    let mut i = 0usize;
    while i < hlen && ssid_hint[i] != 0 {
        out_64[36 + i] = ssid_hint[i];
        i += 1;
    }

    let seal = fnv1a_64(&out_64[0..56]);
    out_64[56..64].copy_from_slice(&seal.to_le_bytes());
}

/// Verifies a 64-byte `MemnetCapsule64` (`magic == *b"MN8!"`, `version == 1`, valid 64-bit FNV-1a seal).
pub fn verify_memnet_capsule64(buf: &[u8]) -> bool {
    if buf.len() != MEMNET_CAPSULE_LEN {
        return false;
    }
    if buf[0] != b'M' || buf[1] != b'N' || buf[2] != b'8' || buf[3] != b'!' || buf[4] != 1 {
        return false;
    }
    let expected = fnv1a_64(&buf[0..56]);
    let actual = u64::from_le_bytes([
        buf[56], buf[57], buf[58], buf[59], buf[60], buf[61], buf[62], buf[63],
    ]);
    expected == actual
}

/// Single entry in the 8-slot Anicca Learn-Share-Forget Wi-Fi Pool
#[repr(C)]
#[derive(Clone, Copy)]
pub struct FlugWifiEntry {
    pub ssid: [u8; 33],
    pub pass: [u8; 65],
    pub ttl_q8: u16, // 256 = fresh; decays smoothly toward 0 (Anicca impermanence)
    pub last_rssi: i8,
    pub flags: u8,   // bit0=open, bit1=verified_inet, bit2=portal_accepted
    pub used: u8,
    pub reserved: u8,
}

impl FlugWifiEntry {
    pub const fn empty() -> Self {
        Self {
            ssid: [0; 33],
            pass: [0; 65],
            ttl_q8: 0,
            last_rssi: -100,
            flags: 0,
            used: 0,
            reserved: 0,
        }
    }

    #[inline]
    pub fn ssid_len(&self) -> usize {
        let mut n = 0usize;
        while n < 32 && self.ssid[n] != 0 {
            n += 1;
        }
        n
    }
}

/// 8-slot Anicca Learn-Share-Forget Wi-Fi Pool in 100% `#![no_std]` zero-float Rust
pub struct AniccaWifiPool {
    pub slots: [FlugWifiEntry; FLUG_WIFI_POOL_SLOTS],
    pub learned_cnt: u32,
    pub forgotten_cnt: u32,
    pub portal_accept_cnt: u32,
}

impl AniccaWifiPool {
    pub const fn new() -> Self {
        Self {
            slots: [FlugWifiEntry::empty(); FLUG_WIFI_POOL_SLOTS],
            learned_cnt: 0,
            forgotten_cnt: 0,
            portal_accept_cnt: 0,
        }
    }

    /// Learns or refreshes a Wi-Fi AP in the Anicca pool with 15-char truncated `ssid_hint` protection.
    pub fn learn(&mut self, ssid: &[u8], pass: &[u8], rssi: i8, verified: bool, is_portal: bool) {
        let mut slen = 0usize;
        while slen < ssid.len().min(32) && ssid[slen] != 0 {
            slen += 1;
        }
        if slen == 0 {
            return;
        }
        let mut plen = 0usize;
        while plen < pass.len().min(64) && pass[plen] != 0 {
            plen += 1;
        }

        // Guard against 15-char truncated ssid_hint from MemnetCapsule64 if a longer matching SSID is known
        if slen == 15 && !verified {
            let mut i = 0usize;
            while i < FLUG_WIFI_POOL_SLOTS {
                if self.slots[i].used != 0 && self.slots[i].ssid_len() > 15 {
                    let mut prefix_eq = true;
                    let mut k = 0usize;
                    while k < 15 {
                        if self.slots[i].ssid[k] != ssid[k] {
                            prefix_eq = false;
                            break;
                        }
                        k += 1;
                    }
                    if prefix_eq {
                        return;
                    }
                }
                i += 1;
            }
        }

        // Conversely, if learning a >15-char SSID, evict any unverified 15-char truncated prefix slot
        if slen > 15 {
            let mut i = 0usize;
            while i < FLUG_WIFI_POOL_SLOTS {
                if self.slots[i].used != 0
                    && self.slots[i].ssid_len() == 15
                    && (self.slots[i].flags & 0x02) == 0
                {
                    let mut prefix_eq = true;
                    let mut k = 0usize;
                    while k < 15 {
                        if self.slots[i].ssid[k] != ssid[k] {
                            prefix_eq = false;
                            break;
                        }
                        k += 1;
                    }
                    if prefix_eq {
                        self.slots[i] = FlugWifiEntry::empty();
                    }
                }
                i += 1;
            }
        }

        // Find existing slot, first empty slot, or lowest-TTL slot
        let mut slot = FLUG_WIFI_POOL_SLOTS;
        let mut first_empty = FLUG_WIFI_POOL_SLOTS;
        let mut lowest = 0usize;
        let mut i = 0usize;
        while i < FLUG_WIFI_POOL_SLOTS {
            if self.slots[i].used != 0 && self.slots[i].ssid_len() == slen {
                let mut eq = true;
                let mut k = 0usize;
                while k < slen {
                    if self.slots[i].ssid[k] != ssid[k] {
                        eq = false;
                        break;
                    }
                    k += 1;
                }
                if eq {
                    slot = i;
                    break;
                }
            }
            if self.slots[i].used == 0 {
                if first_empty == FLUG_WIFI_POOL_SLOTS {
                    first_empty = i;
                }
            } else if self.slots[lowest].used != 0 && self.slots[i].ttl_q8 < self.slots[lowest].ttl_q8
            {
                lowest = i;
            }
            i += 1;
        }

        if slot == FLUG_WIFI_POOL_SLOTS {
            slot = if first_empty < FLUG_WIFI_POOL_SLOTS {
                first_empty
            } else {
                lowest
            };
            if self.slots[slot].used == 0 {
                self.learned_cnt = self.learned_cnt.wrapping_add(1);
            }
        }

        let entry = &mut self.slots[slot];
        entry.ssid = [0; 33];
        let mut k = 0usize;
        while k < slen {
            entry.ssid[k] = ssid[k];
            k += 1;
        }
        if plen > 0 {
            entry.pass = [0; 65];
            let mut p = 0usize;
            while p < plen {
                entry.pass[p] = pass[p];
                p += 1;
            }
        }
        entry.last_rssi = rssi;
        entry.used = 1;
        if entry.pass[0] == 0 {
            entry.flags |= 0x01; // open
        } else {
            entry.flags &= !0x01;
        }
        if verified {
            entry.flags |= 0x02;
        }
        if is_portal {
            entry.flags |= 0x04;
            self.portal_accept_cnt = self.portal_accept_cnt.wrapping_add(1);
        }
        let target_ttl: u16 = if verified { 512 } else { 256 };
        if entry.ttl_q8 < target_ttl {
            entry.ttl_q8 = target_ttl;
        }
    }

    /// Anicca Impermanence Decay: reduces `ttl_q8` across all slots and forgets expired APs.
    pub fn decay(&mut self, amount_q8: u16) {
        let mut i = 0usize;
        while i < FLUG_WIFI_POOL_SLOTS {
            if self.slots[i].used != 0 {
                if self.slots[i].ttl_q8 > amount_q8 {
                    self.slots[i].ttl_q8 -= amount_q8;
                } else {
                    self.slots[i] = FlugWifiEntry::empty();
                    self.forgotten_cnt = self.forgotten_cnt.wrapping_add(1);
                }
            }
            i += 1;
        }
    }

    /// Count of active Wi-Fi APs in the Anicca pool
    pub fn active_count(&self) -> u8 {
        let mut c = 0u8;
        let mut i = 0usize;
        while i < FLUG_WIFI_POOL_SLOTS {
            if self.slots[i].used != 0 && self.slots[i].ttl_q8 > 0 {
                c += 1;
            }
            i += 1;
        }
        c
    }
}

/// C-ABI export: Build a 132-byte `S3TriadWifiCapsule` (`magic = "TWIF"`) in `#![no_std]` Rust
#[no_mangle]
pub extern "C" fn rust_flug_build_twif_capsule(
    out_buf132: *mut u8,
    flags: u8,
    src_node: u8,
    ssid_ptr: *const u8,
    pass_ptr: *const u8,
    host_ip_ptr: *const u8,
    host_port: u16,
    seq: u32,
) {
    if out_buf132.is_null() {
        return;
    }
    let ssid = unsafe {
        if ssid_ptr.is_null() {
            PERSONAL_JESUS_SSID.as_slice()
        } else {
            core::slice::from_raw_parts(ssid_ptr, 32)
        }
    };
    let pass = unsafe {
        if pass_ptr.is_null() {
            PERSONAL_JESUS_PASS.as_slice()
        } else {
            core::slice::from_raw_parts(pass_ptr, 64)
        }
    };
    let ip = unsafe {
        if host_ip_ptr.is_null() {
            PERSONAL_JESUS_IP.as_slice()
        } else {
            core::slice::from_raw_parts(host_ip_ptr, 15)
        }
    };
    let out = unsafe { &mut *(out_buf132 as *mut [u8; TWIF_CAPSULE_LEN]) };
    encode_twif_capsule(out, flags, src_node, ssid, pass, ip, host_port, seq);
}

/// C-ABI export: Verify a 132-byte `S3TriadWifiCapsule` (`magic = "TWIF"`) in `#![no_std]` Rust
#[no_mangle]
pub extern "C" fn rust_flug_verify_twif_capsule(buf132: *const u8, len: usize) -> u8 {
    if buf132.is_null() || len != TWIF_CAPSULE_LEN {
        return 0;
    }
    let slice = unsafe { core::slice::from_raw_parts(buf132, len) };
    if verify_twif_capsule(slice) {
        1
    } else {
        0
    }
}
