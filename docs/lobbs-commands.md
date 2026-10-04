# How LoBBS commands work

Command reference for players lives in the [README](../README.md). This page is for people wiring or extending the board: how a line is parsed, how help is built, and how plugins hook in.

## One verb, rest of the line untouched

You DM a line that starts with `/`. The node may peel an optional request id (`/42 mail list`). Then it takes **one** verb token and leaves everything after it as a single remainder string.

Example: `/mail send ben this is my long message` keeps `this is my long message` intact. Mail peels `send`, then `ben`, then treats the rest as the body. Nothing re-tokenizes the whole line up front, so spaces inside a message stay put.

Optional paging uses a token like `p2` where a command already expects it (`/mail list p2`, `/help p3`). If the next token is not a page marker, it stays in the remainder for the handler.

## Help and `/hi`

`/help` and `/hi` show the same **root verb** list: top-level commands you may use **right now** (`login`, `mail`, `news`, `users`, …). Logged out you see `login`. SysOps also see file verbs (`ls`, `cat`, and friends). Subcommands are not mixed into that list.

`/help mail` or `/help mail send` runs the **`command_help`** filter with that remainder. The owning module adds usage lines. Other modules ignore the query. If nobody adds a line, the reply is `No help for that.` One reply either way, paged like any long list.

A remainder that is only `pN` pages the catalog, not a topic. `/help mail p2` pages help for `mail` on page 2.

## Slash handlers

Every plugin may register a `slash_cmd` **action**. They all run. Each handler compares the verb to what it owns. The owner replies; everyone else returns without saying anything.

There is no central "unknown command" from the bus. If no module owns the verb, you get silence.

## Filters for shared screens

Three **filters** build lists any module can append to:

| Filter | When it runs |
| --- | --- |
| `root_commands` | Bare `/help`, `/hi`, or `/help pN` |
| `command_help` | `/help` with a topic remainder |
| `status_lines` | `/status` (optional `pN` pages the lines) |

Priority order matches the old board: Auth before Mail, News, Yarn, Wall on status. Lower number runs first, then registration order.

## Extending LoBBS

In your plugin's `lobbs*RegisterCommands()`:

1. `lobbsAddAction("slash_cmd", …)` and compare `verb`.
2. Use `lobbsArgShift`, `lobbsArgTakePage`, `lobbsArgRest`, and `lobbsArgShiftUint` / `lobbsArgPeekIsUint` on `ctx.rest` instead of a pre-split argv or raw `atoi`.
3. Register filters for catalog lines, topic help, and status if needed.

Wireup only resets hooks and calls each registrar. There is no install pass and no capped hook table.
