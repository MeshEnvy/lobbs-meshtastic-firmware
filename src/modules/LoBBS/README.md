# LoBBS module

Firmware BBS for Meshtastic (`src/modules/LoBBS/`).

## Docs

| Doc                                                | Audience                                |
| -------------------------------------------------- | --------------------------------------- |
| [docs/lobbs-daily.md](docs/lobbs-daily.md)         | Daily user commands                     |
| [docs/lobbs-sysop.md](docs/lobbs-sysop.md)         | SysOp install, files, operator commands |
| [docs/lobbs-machine.md](docs/lobbs-machine.md)     | Request ids and machine paging          |
| [docs/lobbs-internals.md](docs/lobbs-internals.md) | Hook bus, cache, LoFS, LoDB             |
| [docs/lobbs-plugins.md](docs/lobbs-plugins.md)     | New app checklist                       |
| [CHANGELOG.md](CHANGELOG.md)                       | LoBBS product history and release notes |

Repo landing page: [README.md](../../../README.md).

## Host tools

Chunked file upload over mesh DMs:

```bash
python bin/lobbs_chunkify.py local.txt /flash/path/dest.txt
```

From the firmware repo root, use `python src/modules/LoBBS/bin/lobbs_chunkify.py …`. The script prints `/mkdir`, `/upload offset:b62`, and `/commit` lines. Temp files use `<name>.<crc8>.tmp`. `/commit` verifies the CRC before replacing the destination.
