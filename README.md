# T1 Pocket

An independent MeshCore companion firmware for the **Heltec Mesh Node T1** with the nRF52840, SX1262 and 160 × 80 colour screen. It builds application-only updates against a pinned official MeshCore companion release. The combined build accepts USB serial or paired Bluetooth, one active client at a time. A USB charger leaves Bluetooth available. [Heltec hardware](https://heltec.org/project/mesh-node-t1/) · [Upstream firmware](https://github.com/meshcore-dev/MeshCore)

## On the node

Tap the user button to wake Home. Single taps advance through Home, GPS, Hibernate and Pairing. Double-tap to go back. Hold on Home to send one advert after clock sync. Hold on GPS to switch the receiver, or hold and release on Hibernate to power down. Pairing displays the current Bluetooth PIN. The dim screen goes dark after ten seconds, including on USB. MeshCore’s rescue hold takes precedence in the first eight seconds after boot.

Each restart asks for a phone or computer time sync. When the owner has enabled GPS, a fresh dated fix can also set time. The firmware uses the local companion’s clock-sync command and accepts corrections backwards. Mesh timestamps use UTC in both presets.

| Build preset | Clock display | Home button advert |
| --- | --- | --- |
| `community` (default) | UTC | Flood with the node’s configured default scope |
| `nearby-pacific` | Vancouver Pacific time, UTC−7 year-round | Zero-hop |

[Preview the community screens](screen-preview.png)

The icon in the header shows four coarse voltage bands when running on battery. Home also shows measured terminal voltage. Bars appear at 3.45, 3.65, 3.80 and 4.00 V. These are approximate bands for the T1’s single-cell lithium pack. They have not been calibrated against a full discharge of a specific unit. On USB the icon shows a plug and Home still shows voltage. The charger’s CHRG output drives the red LED. Firmware can see USB power and battery voltage, but the V1.0 schematic shows no processor connection to CHRG or DONE. An out-of-range battery reading displays “-- V”. [T1 schematic](https://resource.heltec.cn/download/Mesh_Node_T1/schematic/Mesh_Node_T1_V1.0.pdf)

## Build and flash

With Python 3.9+, Git, a C++ compiler and PlatformIO Core on PATH, build from the root of this directory. CI uses PlatformIO 6.2.0.

```sh
python3 update.py --out builds
```

The default is the combined USB/Bluetooth companion and the community preset. For a Vancouver clock and nearby adverts, add `--preset nearby-pacific`. Use `--profile pocket` for Bluetooth only with the pocket screens or `--profile clock` for the Bluetooth clock fix with the original MeshCore screens. The build produces a UF2, a Nordic application DFU ZIP and a manifest with the upstream commit and SHA-256 values. Dependencies follow the selected upstream release. Manifests identify individual builds.

1. Start with a **Heltec Mesh Node T1 already running a compatible MeshCore companion**, and allow the phone to receive queued messages. Export that node’s identity, contacts, channels and settings to its owner’s private backup.
2. Connect by USB and double-press the **rear power button**. Open the mounted drive’s `INFO_UF2.TXT` and confirm `Board-ID: mesh-node-t1` and `SoftDevice: S140 6.1.1`. These images cover only the T1 application partition. Other boards, repeaters, room servers and migrations from another firmware family need their own build or migration procedure.
3. Copy the built UF2 onto that drive and allow it to restart. Reconnect, sync the clock and confirm the node’s own identity, radio region/preset, contacts, channels and GPS setting. USB and Bluetooth clients should successively identify the same node. Let the first client disconnect before opening the second. [Heltec USB update instructions](https://github.com/HelTecAutomation/HeltecWiKi/blob/main/docs/devices/open-source-hardware/nrf52840-series/mesh-node-t1/Usage.md)

On a correctly identified T1, an application-only MeshCore update preserves the existing configuration area. The firmware reads the node name and keys on that device. Use the official MeshCore T1 companion application UF2 for rollback. Messages queued in RAM can be lost at restart.

## Following MeshCore releases

`upstream.json` pins the reviewed release. To test the latest stable release on a computer, run `python3 update.py --latest --check-only` and then `python3 update.py --latest --out builds`. Host checks exercise clock arithmetic, screen behaviour and companion-port ownership. Full builds cross-check the UF2 pages against the Intel HEX, the Nordic application package against the UF2, and the combined build’s USB-update entry point against its ELF.

The included GitHub Actions workflow checks for a new stable companion tag daily and on manual dispatch. A new tag triggers five builds covering the supported profiles and presets. A separate job opens a draft PR that updates the pinned tag and commit. Its description links to the test run and records success or failure. Conflicts stay visible for a maintainer to port and retest. Pull requests run the checks again when a contributor pushes changes. Review and merge on the repository’s default branch after a T1 hardware check.

To activate this in your own GitHub repository, push this source to `main`, enable Actions, and allow GitHub Actions to create pull requests in repository settings. The workflow publishes build manifests as CI artifacts. Creating a public firmware release is a separate maintainer action.

## Contributions and licensing

The isolated upstream clock proposal includes the observed failure, a minimal patch and validation instructions. [Upstream proposal](upstream/README.md) · [MeshCore contribution guide](https://github.com/meshcore-dev/MeshCore/blob/dev/CONTRIBUTING.md)

The overlay source and tools are MIT-licensed. The original MeshCore notice is included. The build fetches MeshCore, the Arduino nRF52 framework and libraries under their respective licenses. For redistribution of compiled firmware, include applicable third-party notices and corresponding-source or relinking materials required by linked components, including the LGPL Arduino core. This source repository contains the overlay and its tests. [Overlay license](LICENSE) · [MeshCore license](UPSTREAM-LICENSE.txt) · [Adafruit framework license](https://github.com/adafruit/Adafruit_nRF52_Arduino/blob/master/LICENSE)
