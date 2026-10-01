# Changelog

LoBBS changes in this Meshtastic firmware fork. Meshtastic upstream release notes live in [meshtastic/firmware](https://github.com/meshtastic/firmware); the `[VERSION]` section in `version.properties` tracks that base.

LoBBS semver is the `[LOBBS]` section in `version.properties`. Builds inject `LOBBS_VERSION` / `LOBBS_VERSION_SHORT` the same way as `APP_VERSION` (see `bin/readprops.py`, `bin/platformio-custom.py`).

## 2.0.0

- LoBBS version at build time from `version.properties` (replaces hardcoded `LoBBSVersion.h` string).
- `/help` and `/hi` show the command list; `/login` replaces `/hi` for sign-in and registration.
- LoBBS enabled on ESP32 builds (removed `MESHTASTIC_EXCLUDE_LOBBS` from default ESP32 flags).
- DM handler ignores non-slash traffic and bare `/` to avoid self-DM help loops.

## 1.2.1 (historical)

- Last release that advertised LoBBS v1.2.1 from a manual macro in source (not tied to git tags).
