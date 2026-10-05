# How LoBBS commands work

Command reference for players lives in the [README](../README.md). This page is for people wiring or extending the board: parsing, the hook bus, help, paging, and replies.

## One verb, rest of the line untouched

You DM a line that starts with `/`. The node may peel an optional request id (`/42 mail list`). Then it takes **one** verb token and leaves everything after it as a single remainder string on `ctx.rest`.

Example: `/mail send ben this is my long message` keeps `this is my long message` intact. Mail peels `send`, then `ben`, then treats the rest as the body. Nothing re-tokenizes the whole line up front.

Handlers use `lobbsArgShift`, `lobbsArgPeek`, `lobbsArgRest`, `lobbsArgShiftUint`, and `lobbsArgPeekIsUint` on `ctx.rest`. Do not use partial `atoi` on numeric tokens.

## Paging

Long replies are cached. Follow with `/p2` or `/43 p2` (request id optional) to fetch another page. The help catalog documents paging as topic `pN` via `help_for_topic`.

List and read responses are `LoBBSResponse` records; plain-text serialization runs the `display_human` filter on each record before paging.

## Help and `/hi`

`/help` with no remainder builds a topic list: built-in rows (`help`, `hi`, `pN`) plus whatever plugins append on **`help_topics`**. Logged-in users see mail, news, yarn, wall, and similar feature topics. Auth and sysop-only topics follow each plugin's own rules.

`/help mail` or `/help mail send` runs **`help_for_topic`**. The query is in `args` as `LODB_F_TITLE`. Plugins set `LODB_F_DESCRIPTION` on the value record when they recognize the query. If nothing matches, the reply is `No help found for …`.

`/hi` is a welcome screen (not the full command catalog). `/status` uses **`status_lines`**, not help.

Subcommands use one `LoBBSVerb` table for dispatch (`lobbsDispatchSub`) and help (`lobbsHelpForTable`). Rows with `fn == nullptr` are help-only. Rows flagged `LOBBS_V_SYSOP` are hidden from help and blocked at dispatch unless the caller is a sysop.

## Slash handlers

Every plugin registers a **`slash_cmd`** action. All handlers run for each line. Each compares the verb (`LOBBS_ARG_VERB` / `lobbsSlashVerbIs`) and returns silently when it is not the owner.

There is no central unknown-command reply from the bus. If no module owns the verb, you get silence.

Feature modules with subcommands often use `lobbsDispatchSub` and a `LoBBSVerb` table (`LOBBS_V_LOGIN`, `LOBBS_V_SYSOP` flags). Mail is the reference implementation.

## Filters

| Hook             | Kind        | Role                                                  |
| ---------------- | ----------- | ----------------------------------------------------- |
| `slash_cmd`      | action      | Handle one top-level verb                             |
| `help_topics`    | list        | Append `{title, description}` rows to `/help` catalog |
| `help_for_topic` | record      | Fill usage text for `/help <query>`                   |
| `status_lines`   | record list | Append lines for `/status`                            |
| `display_human`  | record      | Turn a response record into human plain text          |

Hooks are stored in **priority order** at registration time (lower `priority` runs first). Constants live in `LoBBSHooks.h` (`LOBBS_HOOK_PRIORITY_HELP`, `_AUTH`, `_FEATURE`, `_STATUS`, `_TIME`).

## Shared mail/news display

`lobbsMsgRegisterDisplay()` (wireup) registers one `display_human` handler for mail and news list rows (title begins with `[` plus a read flag field) and read views (`From:` header plus body). Mail/news command modules do not register their own `display_human` filters.

## Replies

- **`lobbsCommandReply`** — short text or multiline body (single record).
- **`lobbsCommandReplyResponse`** — structured `LoBBSResponse` (records, errors).
- **`lobbsRecordPush`** — append a `{title, description}` line record to a vector or response.
- **`lobbsReplySendCachedPage`** — send a cached page after `/pN`.

## Extending LoBBS

In `lobbs*RegisterCommands()`:

1. `lobbsAddAction("slash_cmd", …)` and compare the verb.
2. Parse `ctx.rest` with registry arg helpers.
3. Register `help_topics`, `help_for_topic`, and `status_lines` when needed.
4. Call `lobbsWireup()` once from module construction (already lists every registrar).

Do not add a central verb switch. Do not pre-split the full line into `argv`.

## Filesystem mounts and install

LoFS exposes a virtual root `/` that lists mount names. Writable paths always start with `/<mount>/…` (`flash`, optional `sd`, optional `extra` when built with `LOBBS_EXTRA_QSPI=1` and a QSPI flash chip answers). There is no legacy `/internal` alias.

Install state is tracked outside the database in `/flash/lobbs.ls` (LoScalar field `LOBBS_INSTALL_FIELD_ROOT` holds the database root, e.g. `/flash` or `/extra`). `lobbsInstallInit` runs after `LoFS::begin()`:

- **Blank** — no marker; only `/help` and `/install` run; other commands get the install hint with present mounts.
- **Ready** — marker mount is present and `LoDb::open(root)` succeeded.
- **Offline** — marker names a mount that is absent this boot; all non-help commands error.

`/install` authorization matches admin PKI checks in `AdminModule` (`mp.from == 0` for local client, or `pki_encrypted` with a matching `config.security.admin_key`). `AuthDal::createUser` no longer promotes the first signup to SysOp; only `/install` passes `asSysop=true`.

SysOp fs commands use a per-session cwd (`/cd`, `/pwd`). `ls`, `cat`, `hex`, `stat`, `mkdir`, `cp`, and `mv` accept relative paths under cwd. `rm`, `rmdir`, and `rmtree` require absolute paths. Mount roots and `/` cannot be removed or used as move/copy sources. Cross-mount `mv` copies files then deletes the source when space allows; directories require same-mount `rename`.

Fs subcommands are registered as a `LoBBSVerb` table with `LOBBS_V_SYSOP` and top-level verbs (`/ls`, not `/fs ls`).

## LoDB from plugins

Use `LoDb::upsert` when saving singleton or keyed rows instead of open-coding get/update/insert. Wall and yarn DALs load defaults in memory on miss without writing until the user saves.
