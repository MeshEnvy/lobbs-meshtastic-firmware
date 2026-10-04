#pragma once
#if !MESHTASTIC_EXCLUDE_LOBBS

#define LOBBS_MAX_USERNAME_LEN 32
#define LOBBS_USERNAME_BUFFER_SIZE (LOBBS_MAX_USERNAME_LEN + 1)
#define LOBBS_XSTR(x) LOBBS_STR(x)
#define LOBBS_STR(x) #x

/** Shared in /hi and /help root (omit topic for list, or name a command). */
#define LOBBS_HELP_HINT "Use /help [topic] for help"

#endif
