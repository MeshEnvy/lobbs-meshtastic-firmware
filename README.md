# LoBBS - Firmware-based BBS for Meshtastic

**A 100% Meshtastic BBS. No sidecar, no Python, just mesh**

---

### Watch the Walkthrough

[![LoBBS 2.0 walkthrough](.github/lobbs-2.0-walkthrough-youtube-thumb.jpg)](https://youtu.be/lMK1zx9DbOI)

---

[Discord](https://discord.gg/DMrGcGQfMN)

## Features

- **User directory**
- **Private mail**
- **News feed**
- **Shared ASCII art wall**
- **Collaborative stories**
- CLI over DM

## Installation

[Install on MeshForge](https://meshforge.org/MeshEnvy/lobbs-meshtastic-firmware) or compile and flash from source.

## Docs

LoBBS source and docs live in the [MeshEnvy/lobbs](https://github.com/MeshEnvy/lobbs) plugin repo (`docs/` there).

## Build from source

Every PlatformIO env includes LoBBS via `lib_deps` (`lobbs-overrides.ini`). For local plugin work, copy `lobbs.local.ini.example` to `lobbs.local.ini` (gitignored) and set `lobbs=symlink://../lobbs`.

```bash
pio run -e seeed_solar_node
pio test -e native-macos -f test_lobbs_commands
```

## Versions

- **Meshtastic base** — `[VERSION]` in `version.properties` (same as upstream: `APP_VERSION` in the phone app, e.g. `2.7.26.<git sha>`).
- **LoBBS** — `[LOBBS]` in `version.properties`; help text shows `LoBBS v` plus the short semver (e.g. `2.0.0`). Product history: [src/modules/LoBBS/CHANGELOG.md](src/modules/LoBBS/CHANGELOG.md). Bump LoBBS build with `python bin/bump_lobbs_version.py` (Meshtastic build: `bin/bump_version.py`).

### Release tags (source only)

LoBBS cuts annotated git tags on branch `lobbs` (no `v` prefix, no firmware binaries in the tag itself):

`lobbs-{lobbsSemVer}.{lobbsSha7}-meshtastic-{mtSemVer}.{mtSha7}`

Example: `lobbs-1.3.0.f18d6d6-meshtastic-2.7.26.54e0d8d`. The trailing Meshtastic sha is the upstream pin for that release line (default `54e0d8d` for 2.7.26). Preview or create a tag with `bin/lobbs-release-tag.sh` (dry-run) or `bin/lobbs-release-tag.sh --create`. Use `--lobbs-version` when tagging a commit that predates the `[LOBBS]` section in `version.properties`.

## License

LoBBS (the BBS) is [MIT](https://opensource.org/license/mit). Meshtastic firmware in this tree is [GPL v3](LICENSE).

## Disclaimer

LoBBS and MeshForge are independent projects not endorsed by or affiliated with the Meshtastic organization.

---

<div align="center" markdown="1">

<img src=".github/meshtastic_logo.png" alt="Meshtastic Logo" width="80"/>
<h1>Meshtastic Firmware</h1>

![GitHub release downloads](https://img.shields.io/github/downloads/meshtastic/firmware/total)
[![CI](https://img.shields.io/github/actions/workflow/status/meshtastic/firmware/main_matrix.yml?branch=master&label=actions&logo=github&color=yellow)](https://github.com/meshtastic/firmware/actions/workflows/ci.yml)
[![CLA assistant](https://cla-assistant.io/readme/badge/meshtastic/firmware)](https://cla-assistant.io/meshtastic/firmware)
[![Fiscal Contributors](https://opencollective.com/meshtastic/tiers/badge.svg?label=Fiscal%20Contributors&color=deeppink)](https://opencollective.com/meshtastic/)
[![Vercel](https://img.shields.io/static/v1?label=Powered%20by&message=Vercel&style=flat&logo=vercel&color=000000)](https://vercel.com?utm_source=meshtastic&utm_campaign=oss)

<a href="https://trendshift.io/repositories/5524" target="_blank"><img src="https://trendshift.io/api/badge/repositories/5524" alt="meshtastic%2Ffirmware | Trendshift" style="width: 250px; height: 55px;" width="250" height="55"/></a>

</div>

</div>

<div align="center">
	<a href="https://meshtastic.org">Website</a>
	-
	<a href="https://meshtastic.org/docs/">Documentation</a>
</div>

## Overview

This repository contains the official device firmware for Meshtastic, an open-source LoRa mesh networking project designed for long-range, low-power communication without relying on internet or cellular infrastructure. The firmware supports various hardware platforms, including ESP32, nRF52, RP2040/RP2350, and Linux-based devices.

Meshtastic enables text messaging, location sharing, and telemetry over a decentralized mesh network, making it ideal for outdoor adventures, emergency preparedness, and remote operations.

### Get Started

- 🔧 **[Building Instructions](https://meshtastic.org/docs/development/firmware/build)** – Learn how to compile the firmware from source.
- ⚡ **[Flashing Instructions](https://meshtastic.org/docs/getting-started/flashing-firmware/)** – Install or update the firmware on your device.

Join our community and help improve Meshtastic! 🚀

## Stats

![Alt](https://repobeats.axiom.co/api/embed/8025e56c482ec63541593cc5bd322c19d5c0bdcf.svg "Repobeats analytics image")
