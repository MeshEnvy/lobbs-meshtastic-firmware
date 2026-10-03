# LoBBS - Firmware-based BBS for Meshtastic

**A 100% Meshtastic BBS. No sidecar, no Python, just mesh**

------

### Watch the Walkthrough

[![LoBBS 2.0 walkthrough](.github/lobbs-2.0-walkthrough-youtube-thumb.jpg)](https://www.youtube.com/watch?v=FwtDY1QBXpQ)

-------

This repository is Meshtastic firmware with LoBBS integrated in the tree. Flash it like any other Meshtastic build for your board: you get mesh networking and a full bulletin board right on the device. User accounts, private mail, news, and administration over the mesh, with no sidecar services or host computer.

**Canonical home:** [github.com/MeshEnvy/lobbs-meshtastic-firmware](https://github.com/MeshEnvy/lobbs-meshtastic-firmware). The older [MeshEnvy/lobbs](https://github.com/MeshEnvy/lobbs) monorepo is archived; MeshCore compatibility is not being pursued.

## Features

- **User directory** with username registration and secure password storage
- **Private mail inbox** with paging, read receipts, and inline `@mention` delivery
- **News feed** with announcements and per-user read tracking
- **Shared ASCII wall** (12×12 canvas) with batch paint, erase tokens (`-a4`), per-user paint quota, and new/seen on `/status`
- **Yarn** collab word game: `/yarn` shows the tail, `/yarn word …` appends words (quota); `/status` shows how many words were added since you last looked
- Stateless slash CLI over DM (one command, one reply)
- Backed by [LoDB](https://github.com/MeshEnvy/lodb) for on-device storage so the entire BBS persists across reboots

## Installation

We use **meshforge.org** to make it super easy to get up and running with LoBBS. Just go to [https://meshforge.org/MeshEnvy/lobbs-meshtastic-firmware](https://meshforge.org/MeshEnvy/lobbs-meshtastic-firmware) to get started.

## Versions

- **Meshtastic base** — `[VERSION]` in `version.properties` (same as upstream: `APP_VERSION` in the phone app, e.g. `2.7.26.<git sha>`).
- **LoBBS** — `[LOBBS]` in `version.properties`; help text shows `LoBBS v` plus the short semver (e.g. `2.2.0` on branch `lobbs` while 2.0.0 is unreleased). Product history: [CHANGELOG.md](CHANGELOG.md). Bump LoBBS build with `python bin/bump_lobbs_version.py` (Meshtastic build: `bin/bump_version.py`).

### Release tags (source only)

LoBBS cuts annotated git tags on branch `lobbs` (no `v` prefix, no firmware binaries in the tag itself):

`lobbs-{lobbsSemVer}.{lobbsSha7}-meshtastic-{mtSemVer}.{mtSha7}`

Example: `lobbs-1.3.0.f18d6d6-meshtastic-2.7.26.54e0d8d`. The trailing Meshtastic sha is the upstream pin for that release line (default `54e0d8d` for 2.7.26). Preview or create a tag with `bin/lobbs-release-tag.sh` (dry-run) or `bin/lobbs-release-tag.sh --create`. Use `--lobbs-version` when tagging a commit that predates the `[LOBBS]` section in `version.properties`.

## Using LoBBS

LoBBS is intentionally a **stateless CLI** on the radio: you send one slash command, you get one reply. There is no menu session or navigation state on the device. Rich clients (web UI, phone apps, bots) can wrap the same commands with threading, layout, and reply tracking. That design also keeps mesh traffic to a single command and a single response instead of multi-message menu redraws.

DM the node with lines that start with `/`. Optional request id for machines:

```
/42 login ben mypassword
<42>Welcome ben!
```

Humans can omit the id: `/hi` is the intro screen (welcome by name when logged in); `/time` shows Unix time and its source; `/status` works without login and shows all-time totals for users, mail, news, and yarn. When logged in, `/status` shows unread mail and news, new yarn words since your last `/yarn`, and whether the wall canvas changed since you last viewed it. `/yarn` and `/wall` view need no login; adding a word or painting the wall needs login and respects quotas. Change your password with `/passwd new confirm` (admins can reset another user with `/passwd user new confirm`). Admins set limits with `/yarn limit 3600 1 32` and `/wall limit 3600 3`. Lists use `pN` page tokens (re-query each time, no cached pages): `/mail list p2`. Multi-page replies pack as many lines as fit in 200 bytes; the last line is `{p 2/3}` when there is another page. Send `/help` for the command index.

LoBBS replies are capped at 200 bytes per message. Unread items show `*` in lists; timestamps use relative forms like `2h ago`.


## License

LoBBS is distributed under the GPLv3 license. 

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
