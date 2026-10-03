# Changelog

LoBBS changes in this Meshtastic firmware fork. Meshtastic upstream release notes live in [meshtastic/firmware](https://github.com/meshtastic/firmware); the `[VERSION]` section in `version.properties` tracks that base.

LoBBS semver is the `[LOBBS]` section in `version.properties`. Builds inject `LOBBS_VERSION` / `LOBBS_VERSION_SHORT` the same way as `APP_VERSION` (see `bin/readprops.py`, `bin/platformio-custom.py`).

## 2.3.0

- Stateless slash CLI only: `/[id] command args…`, replies optionally prefixed `<id>`. No menu stack or page cache on device.
- Lists paginate with `pN` (re-query offset/limit). `/status` shows all-time users, mail, and news counts; Wall shows new vs seen for the shared 12×12 ASCII canvas.
- `/wall` displays and marks the canvas seen; batch paint with tokens like `/wall a4x b2|` or erase `-a4`. Paint quota defaults to 1 cell per 3600s (admins bypass); `/wall limit SEC CELLS` sets policy. `/news` is the text announcement feed (list, read, post, unread, admin delete).
- Ignore non-slash DMs and self-originated loopback (`from` = local node).

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
