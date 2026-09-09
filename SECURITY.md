# Security

Mastro OS is experimental firmware for a developer board. It is not hardened for
medical, emergency, payment, access-control or other safety-critical use.

## Current security posture

| Area | Current behavior |
| --- | --- |
| Wi-Fi mode | Station mode using ESP-IDF Wi-Fi APIs |
| Wi-Fi secrets | Stored once in `watch_cfg`; ESP-IDF Wi-Fi storage is RAM-only |
| Storage protection | Flash encryption is disabled |
| Boot authenticity | ESP32-S3 secure boot is disabled |
| Weather | HTTPS to Open-Meteo with ESP-IDF's full certificate bundle |
| Time sync | Public SNTP servers; responses are not cryptographically authenticated |
| Bluetooth | Phone pairing and Bluetooth services are not implemented |
| Remote control | No inbound web server or remote actuator API is implemented |
| OTA | Signed or automatic over-the-air updates are not implemented |
| Release hashes | SHA-256 integrity checks only; releases are not digitally signed |

The firmware validates weather response status, JSON structure, buffer limits and
temperature ranges. TLS verifies the weather server certificate against the
bundled trust store. Location coordinates are included in the Open-Meteo request,
so the weather provider and network infrastructure can observe the requested
location and device IP address.

SNTP protects neither source identity nor message integrity. A hostile network can
potentially influence displayed time. RTC and SNTP values must not authorize
access, validate certificates, expire credentials or drive safety-critical logic.

## Secrets and backups

Do not define real credentials in `config.h`, commit them, place them in release
archives or paste them into issue reports. Entering Wi-Fi credentials on the watch
stores them as ordinary NVS strings. With flash encryption disabled, physical
access to the device can expose those values.

Compile-time Wi-Fi credential macros are intentionally ignored so passwords are
not embedded in distributable firmware. Settings provides a long-press **Hold
Erase** action. It deletes the application NVS keys, clears legacy ESP-IDF Wi-Fi
configuration, disconnects the radio and overwrites application credential
buffers. Use it before transferring or decommissioning a watch. Logical erasure
is not forensic erasure on unencrypted flash.

`scripts/flash_safe.sh` reads a complete 16 MB factory image before flashing.
That image includes NVS, settings and any credentials present on the watch. Files
under `backups/` are ignored by Git, but ignore rules are not access controls.
The safe-flash script creates new backups with owner-only permissions, but host
disk encryption is still required. Store backups only on trusted encrypted
storage, do not upload them, and never restore one watch's image onto another
watch.

## Physical attacker boundary

The development build cannot prevent forceful extraction or firmware replacement
by someone holding the watch. USB Serial/JTAG, ROM download mode, plaintext flash
and unsigned boot remain available for development and recovery. A malicious
replacement image can read credentials after the owner next enters them.

Protecting against that threat requires one coordinated provisioning process,
validated first on disposable hardware:

1. Generate and protect offline Secure Boot v2 signing keys. Never commit keys or
	store them beside release binaries.
2. Enable Secure Boot v2 and verify that unsigned and modified images are rejected.
3. Enable flash encryption in release mode, NVS encryption, encrypted updates and
	anti-rollback for the pinned ESP-IDF release.
4. Test power-loss recovery, signed updates, rollback rejection and credential
	erasure before restricting debug access.
5. Disable JTAG and restrict ROM download through ESP32-S3 eFuses only after the
	complete recovery path succeeds on a pilot device.
6. Track each provisioned device and key-revocation state without storing private
	keys in this repository.

These eFuse operations can permanently prevent normal reflashing or recovery.
They must never run from ordinary build or flash scripts and require explicit
owner consent for each physical device.

## Flashing trust boundary

Use only firmware built from a reviewed source revision or downloaded from a
trusted release location. Verify published checksums after download, while
remembering that an unsigned checksum hosted beside a binary does not establish
authorship. The safe-flash script verifies the ESP32-S3 chip family and backup
size; it cannot identify the exact watch model or detect malicious firmware.

Never force the Waveshare profile onto another board. Wrong pin assignments,
power polarity, reset sequencing or flash geometry can erase factory software,
make recovery difficult or electrically conflict with peripherals.

## Production hardening

A production fork should define and test a threat model before enabling security
eFuses. At minimum, evaluate:

- ESP32-S3 secure boot with protected offline signing keys
- flash encryption with a documented provisioning and recovery process
- encrypted NVS or a design that avoids persistent network passwords
- signed firmware releases and rollback policy
- authenticated update transport with anti-rollback controls
- dependency and certificate-bundle update procedures
- removal or protection of debug and serial access
- credential reset and device decommissioning workflows

The firmware already uses RAM-only ESP-IDF Wi-Fi storage, bounded credential
inputs, memory zeroization, TLS certificate and hostname verification, disabled
weather redirects, and an on-watch credential-erasure workflow. These reduce
accidental exposure but do not substitute for encrypted flash and authenticated
boot.

Secure boot and flash encryption can permanently change device behavior. Develop
their provisioning process on disposable hardware before using a personal watch.

## Reporting a vulnerability

Report suspected vulnerabilities to the repository owner through GitHub without
including passwords, flash dumps, private keys or other secrets. Avoid publishing
working exploit details in a public issue before the owner has had an opportunity
to investigate.