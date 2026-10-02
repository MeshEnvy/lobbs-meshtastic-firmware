# Changelog

LoBBS changes in this Meshtastic firmware fork. Meshtastic upstream release notes live in [meshtastic/firmware](https://github.com/meshtastic/firmware); the `[VERSION]` section in `version.properties` tracks that base.

LoBBS semver is the `[LOBBS]` section in `version.properties`. Builds inject `LOBBS_VERSION` / `LOBBS_VERSION_SHORT` the same way as `APP_VERSION` (see `bin/readprops.py`, `bin/platformio-custom.py`).

## 2.2.0

- Numbered menus over DM: pick `1`, `2`, … instead of typing topic verbs. Prompts for login, mail, and news.
- Navigation: `?` reprint, `<` back, `<<` home, `p` / `p2` pages (same as `/p`). Slash verbs remain as shortcuts.
- Help (`/help`, `/hi`, `--help`) reprints the current menu screen.
- Ignore echoed LoBBS replies before LoDB. `<` clears the session page buffer. Page text is fixed RAM, not heap strings.
- History stack of menu frames and prompt frames. `?` redraws, `<` pops one frame, `<<` returns to root. Reading a mail message is its own frame, so `<` returns to the inbox list.
- Serial breadcrumbs on `<` / `<<` and on mail/news read (query, bounds, format, send, mark, free).
- Mail and news reads copy sender and body with an explicit field cap.

## 2.1.0

- Topic commands: `/mail`, `/news`, and `/user` use verbs (`send`, `list`, `read`, `post`, `del`, etc.). Old flat syntax and `@mention` mail redirect to the new forms.
- Unified help: `/help` and `--help` on any command path (topic + verb). `/hi` is bare `/help`.
- Admin privilege is `is_admin` only (no loopback or phone self-DM admin). Optional other-user args on mail verbs for admins.
- User admin: `/user kick`, `promote`, `demote` (demote refuses last admin).
- RAM pagination: list results up to 10 pages of 200 bytes; `/p` and `/p <n>`. FIFO eviction across four session slots.

## 2.0.0

- LoBBS version at build time from `version.properties` (replaces hardcoded `LoBBSVersion.h` string).
- `/help` and `/hi` show the command list; `/login` replaces `/hi` for sign-in and registration.
- LoBBS enabled on ESP32 builds (removed `MESHTASTIC_EXCLUDE_LOBBS` from default ESP32 flags).
- DM handler ignores non-slash traffic and bare `/` to avoid self-DM help loops.
- `/whoami` shows session user and admin status; welcome text notes first admin.

## 1.2.1 (historical)

- Last release that advertised LoBBS v1.2.1 from a manual macro in source (not tied to git tags).
