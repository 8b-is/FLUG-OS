/*
 * FLUG-OS — Fluid Operating System {-1, 0, +1}
 * Version header — single source of truth.
 *
 * Keep in sync with ROADMAP.md and the GitHub release tags.
 * Current target: v1.4.2 (parity with MLX-QUANT v1.4.2).
 */

#ifndef FLUGOS_VERSION_H
#define FLUGOS_VERSION_H

#define FLUGOS_VERSION_MAJOR  0
#define FLUGOS_VERSION_MINOR  9
#define FLUGOS_VERSION_PATCH  0
#define FLUGOS_VERSION_STR    "0.9.0"
#define FLUGOS_VERSION_STRING "0.9.0"

/* Build metadata — populated by PlatformIO build_flags if set */
#ifndef FLUGOS_COMMIT
#define FLUGOS_COMMIT         "kunisada-kuniyoshi-omni-chan"
#endif

#define FLUGOS_FULL_VERSION   "v" FLUGOS_VERSION_STR "+" FLUGOS_COMMIT

/* Roadmap milestone alignment */
#define FLUGOS_MLX_QUANT_PARITY_TARGET  "1.4.2"

/* Feature flags for this release */
#define FLUGOS_HAS_SPECTRUM       1   /* v0.3.0: 14-ch RSSI + waterfall */
#define FLUGOS_HAS_WAVE_OUTPUT    1   /* v0.3.0: sine kernel wave modes */
#define FLUGOS_HAS_MATRIX_DECODER 1   /* v0.3.0: packet -> ternary row  */
#define FLUGOS_HAS_TERNARY_SEAM   1   /* v0.4.0: radiation-delta seam   */
#define FLUGOS_HAS_S3_AMOLED      1   /* v0.4.0: ESP32-S3 AMOLED 410x502*/
#define FLUGOS_HAS_ULTRA_OMNI_ZEN 1   /* v0.5.0: 1.8" 368x448 + 2D Wave */
#define FLUGOS_HAS_TRIAD_INFINITY 1   /* v0.6.0: {<3,<3,<3}+1 3-Node Mesh*/
#define FLUGOS_HAS_IMPERMANENCE   1   /* v0.7.0: MarineSense O(1) + Anicca*/
#define FLUGOS_HAS_MEDUSA_OMNI    1   /* v0.8.0: MEDUSA WebSerial + 20-track crate */
#define FLUGOS_HAS_KOMPRESS_OMNI_CHAN 1 /* v0.9.0: All-Entropy Kompress-Super-Ultra <<<<oOo>>>> ~ Kunisada & Kuniyoshi 010001000 */
#define FLUGOS_HAS_MEMNET_EXPORT  0   /* v1.1.0: ayeOS capsule over UART*/
#define FLUGOS_HAS_PWM_AUDIO      0   /* v1.2.0: GPIO headphone synth   */
#define FLUGOS_HAS_PCAP_EXPORT    0   /* v1.3.0: Wireshark over UART    */

#endif /* FLUGOS_VERSION_H */
