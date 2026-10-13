# Agent instructions — LoBBS Meshtastic fork

This repo is upstream [Meshtastic firmware](https://github.com/meshtastic/firmware) plus a thin LoBBS overlay (plugin pin in `lib_deps`, LoBBS enabled on envs).

## Where guidance lives

| Topic | Location |
| --- | --- |
| Upstream firmware (build, MCP, tests, conventions) | Other workspace folder: [meshtastic/firmware/AGENTS.md](../meshtastic/firmware/AGENTS.md) and `.github/copilot-instructions.md` there |
| LoBBS plugin (architecture, releases, greenfield) | [lobbs/.cursor/rules/](../lobbs/.cursor/rules/) and [lobbs/AGENTS.md](../lobbs/AGENTS.md) |
| Fork-only rules (docs, verification, command IoC, newlib printf) | This repo: `.cursor/rules/` (`alwaysApply: false`, glob-scoped) |

## Fork delta

- Do not treat this tree as the canonical Meshtastic MCP home. Upstream documents [meshtastic/meshtastic-mcp](https://github.com/meshtastic/meshtastic-mcp) with `MESHTASTIC_FIRMWARE_ROOT` pointed at the upstream firmware checkout.
- Committed plugin pin only (`lobbs=https://github.com/MeshEnvy/lobbs.git#<sha>`). Local dev: gitignored `lobbs.local.ini` with `symlink://../lobbs`.

## Quick build

```bash
pio run -e <env>
```

Pick an env from upstream docs or the fork's enabled LoBBS matrix.
