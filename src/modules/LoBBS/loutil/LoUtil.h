#pragma once

#include <cstdint>

static constexpr unsigned LO_U64_DEC_LEN = 21;

/** Writes `value` in decimal into `buf` and returns the first digit. newlib-nano (nRF52) has no `%llu`. */
const char *loU64ToDec(uint64_t value, char (&buf)[LO_U64_DEC_LEN]);
