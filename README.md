# LoBBS - Firmware-based BBS for Meshtastic

**A 100% Meshtastic BBS. No sidecar, no Python, just mesh**

------

### Watch the Walkthrough

[![LoBBS Walkthrough](https://img.youtube.com/vi/FwtDY1QBXpQ/0.jpg)](https://www.youtube.com/watch?v=FwtDY1QBXpQ)

-------

This repository is Meshtastic firmware with LoBBS integrated in the tree. Flash it like any other Meshtastic build for your board: you get mesh networking and a full bulletin board right on the device. User accounts, private mail, news, and administration over the mesh, with no sidecar services or host computer.

## Features

- **User directory** with username registration and secure password storage
- **Private mail inbox** with paging, read receipts, and inline `@mention` delivery
- **News feed** with threaded announcements and per-user read tracking
- Session-aware command parser with **contextual help**
- Backed by [LoDB](https://github.com/MeshEnvy/lodb) for on-device storage so the entire BBS persists across reboots

## Installation

We use **meshforge.org** to make it super easy to get up and running with LoBBS. Just go to [https://meshforge.org/MeshEnvy/lobbs-meshtastic-firmware](https://meshforge.org/MeshEnvy/lobbs-meshtastic-firmware) to get started.


## Using LoBBS

- **Joining the BBS** — Send a direct message to your node containing `/hi <username> <password>`. The command logs you in if the account exists or creates a new account if it does not.
- **Logging out** — Use `/bye` to terminate the current session and clear the binding between your node ID and account.
- **Mail** — `/mail` lists the 10 most recent messages, `/mail 3` reads message 3, and `/mail 5-` starts the listing at item 5. Mention another user in any authenticated message using `@username` to deliver instant mail.
- **News** — `/news` mirrors the mail workflow for public announcements. Append a message after the command (for example `/news Hello mesh!`) to post a new item.
- **User discovery** — `/users` returns the directory. Supply an optional filter string (e.g. `/users mesh`) to narrow the results.

LoBBS replies inline with human-readable summaries. Unread content is flagged with an asterisk in list views, and relative timestamps (for example, `2h ago`) provide context for each entry.


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

