# LoBBS SysOp Guide

[https://discord.gg/DMrGcGQfMN](https://discord.gg/DMrGcGQfMN)

If you run a LoBBS node, you are the System Operator (SysOp) and this guide is for you.

Normal player commands are in the [daily user guide](lobbs-daily.md).

## Install

[Install on MeshForge](https://meshforge.org/MeshEnvy/lobbs-meshtastic-firmware) or compile and flash from source.

A new LoBBS node has no database until you install it. Only `/help` and `/install` work until then. The install hint lists mounts that are present on this hardware (`flash`, plus `sd` when a card is present and `extra` on builds with `LOBBS_EXTRA_QSPI=1`).

`/install <flash|extra|sd> <user> <pass>` creates the database on that mount and makes `<user>` the SysOp. Use it from the local client (phone app connected over serial/BLE) or from a remote DM encrypted with a configured admin key. If the target already holds a LoBBS database, the same command adopts it and you must supply an existing SysOp username and password.

The choice is stored in `/flash/lobbs.ls`. If that mount is missing on a later boot, every command fails with an offline message until the hardware is fixed or you delete the marker and reinstall.

## Demo builds

Add `-D LOBBS_DEMO_MODE` to `build_flags` (or `PLATFORMIO_BUILD_FLAGS="-D LOBBS_DEMO_MODE" pio run -e <env>`) to wipe `/flash/lodb/lobbs` on every boot and seed demo data. Accounts: `sysop` and `demo01` through `demo12`, password `demo1`. Sample mail, news, wall paint, and yarn text are included for paging tests.

## SysOp commands

SysOp-only lines reply `SysOp only.` to everyone else.

| Command                       | Notes                                                                                          |
| ----------------------------- | ---------------------------------------------------------------------------------------------- |
| `/install flash user pass`    | First-time setup. Blank node only unless adopting an existing database.                        |
| `/passwd user new confirm`    | Reset a user's password                                                                        |
| `/users kick user`            | Log that user out                                                                              |
| `/users promote user`         | Make them a SysOp                                                                              |
| `/users demote user`          | Remove SysOp (not the last SysOp)                                                              |
| `/mail list user`             | Their inbox                                                                                    |
| `/mail read user N`           | Read their message (does not mark it read)                                                     |
| `/news delete N`              | Delete a news item                                                                             |
| `/wall limit SEC CELLS`       | Paint quota (default 3600 seconds, 1 cell)                                                     |
| `/yarn limit SEC WORDS CHARS` | Yarn quota (default 3600 seconds, 1 word, 32 characters)                                       |
| `/time unix`                  | Set the clock. The time must be after this firmware was built, or the reply is `Invalid time.` |
| `/cd [path]`                  | Set your working directory (default `/`). Each session keeps its own                           |
| `/pwd`                        | Show your working directory                                                                    |
| `/ls [path]`                  | List a directory, default the working directory (`*` glob on the last name only)               |
| `/cat path`                   | Read a file as text                                                                            |
| `/hex path`                   | Hex dump a file                                                                                |
| `/rm /path`                   | Delete a file (absolute path only)                                                             |
| `/rmdir /path`                | Remove an empty directory (absolute path only)                                                 |
| `/rmtree /path /path`         | Recursive delete (absolute path, typed twice)                                                  |
| `/mkdir path`                 | Create a directory under a mount (`/flash/...`, etc.)                                          |
| `/cp src dst`                 | Copy a file (no overwrite; directories refused)                                                |
| `/mv src dst`                 | Rename on one mount, or copy then delete across mounts (files only)                            |
| `/upload path [offset:b62]`   | Chunked file write at byte offset, or show current size                                        |
| `/commit src dst`             | Move when file CRC32 matches the 8-hex segment in the source name                              |
| `/stat path`                  | `file N` or `dir`                                                                              |
| `/df`                         | Used/total KB per mount (`?` when unknown)                                                     |

SysOps painting the wall or adding yarn do not use quota.

## Files

Paths use mount prefixes: `/flash/...`, `/sd/...`, `/extra/...` on builds with `LOBBS_EXTRA_QSPI=1` and a QSPI flash chip. `/` lists mounts. Do not `/rmtree` `/flash/lodb` unless you intend to wipe the BBS.

Relative paths work under your session working directory for `ls`, `cat`, `hex`, `stat`, `mkdir`, `cp`, `mv`, `upload`, and `commit`. `rm`, `rmdir`, and `rmtree` require absolute paths.

## Chunked upload

`/upload path offset:b62` writes base62-decoded bytes at `offset`. `/upload path` with no offset replies with the current size. `/commit src dst` verifies CRC32 from the source filename before replacing the destination.

From the firmware repo root:

```bash
python src/modules/LoBBS/bin/lobbs_chunkify.py local.txt /flash/path/dest.txt
```

The script prints `/mkdir`, `/upload`, and `/commit` lines. Temp files use `<name>.<crc8>.tmp`.
