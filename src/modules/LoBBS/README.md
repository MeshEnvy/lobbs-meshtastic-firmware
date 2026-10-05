# LoBBS module

Firmware BBS for Meshtastic (`src/modules/LoBBS/`). Player-facing command reference stays in the [repo README](../../../README.md).

**Filesystem layering:** [`lofs/LoFS`](lofs/LoFS.h) is the mount-aware VFS (move, copy, list, writeAt). [`lofs/Glob`](lofs/Glob.h) expands globs on absolute paths. [`apps/Fs/FsCommands.cpp`](apps/Fs/FsCommands.cpp) is the SysOp shell: session cwd, wire formats (`/upload` gap check, base62), and reply text.

## Docs

| Doc                                              | Audience                                             |
| ------------------------------------------------ | ---------------------------------------------------- |
| [docs/lobbs-commands.md](docs/lobbs-commands.md) | Extending the board: hook bus, paging, LoFS, install |
| [docs/lobbs-sysop.md](docs/lobbs-sysop.md)       | SysOp guide (stub)                                   |
| [docs/lobbs-daily.md](docs/lobbs-daily.md)       | Daily user guide (stub)                              |
| [CHANGELOG.md](CHANGELOG.md)                     | LoBBS product history and release notes              |

## Host tools

Chunked file upload over mesh DMs:

```bash
python bin/lobbs_chunkify.py local.txt /flash/path/dest.txt
```

From the firmware repo root, use `python src/modules/LoBBS/bin/lobbs_chunkify.py …`. The script prints `/mkdir`, `/upload offset:b62`, and `/commit` lines. Temp files use `<name>.<crc8>.tmp`; `/commit` verifies the CRC before replacing the destination.
