#include <loutil/LoUtil.h>

const char *loU64ToDec(uint64_t value, char (&buf)[LO_U64_DEC_LEN])
{
    char *p = buf + LO_U64_DEC_LEN - 1;
    *p = '\0';
    do {
        *--p = (char)('0' + (value % 10));
        value /= 10;
    } while (value);
    return p;
}
