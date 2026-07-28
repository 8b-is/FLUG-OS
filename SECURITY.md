# Security Policy for FLUG-OS {-1, 0, +1}

## Supported versions

| Version | Supported | PQC signed |
|---------|-----------|------------|
| 0.3.x   | ✅ | ✅ ML-DSA-65 + SLH-DSA |
| < 0.3   | ❌ | ❌ |

## Post-quantum security

FLUG-OS releases are signed with **ML-DSA-65** (FIPS 204, CRYSTALS-Dilithium) and
**SLH-DSA-SHAKE-128s** (FIPS 205, SPHINCS+) per NIST standards finalized August 13, 2024.

These are structured lattice and hash-based signatures respectively — neither is
vulnerable to Shor's algorithm. A future quantum computer cannot forge firmware signatures.

Each release publishes:
- `.mldsa65.sig` — primary ML-DSA signature
- `.slhdsa.sig` — backup SLH-DSA signature
- `.pub` — public keys for both algorithms
- `.sha256` — checksum manifest, itself ML-DSA signed

Verify with:
```bash
openssl dgst -verify flugos-signing-mldsa65.pub \
  -signature firmware.bin.mldsa65.sig firmware.bin
```

Reference: [NIST FIPS 203/204/205 — August 13, 2024](https://security.googleblog.com/2024/08/post-quantum-cryptography-standards.html)

## Reporting a vulnerability

This firmware operates in promiscuous mode on 2.4 GHz WiFi. The attack surface is:

- **UART command interface**: accepts commands over serial. No authentication.
  Intended for local access only. Do not expose the serial port to untrusted networks.
- **WiFi promiscuous mode**: receives all frames on the channel. Designed for
  passive monitoring only. Does not transmit. Does not associate with networks.
- **Firmware signing**: post-quantum signatures prevent unauthorized firmware.
  Do not disable signature verification.

### To report

Open a [GitHub Security Advisory](https://github.com/8b-is/FLUG-OS/security/advisories)
or email the 8b-is constellation directly.

We practice responsible disclosure. No drama. Just fix it and move on.

---

*Built by the 8b-is constellation. 🜂 {-1, 0, +1}*
