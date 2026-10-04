#pragma once
#if !MESHTASTIC_EXCLUDE_LOBBS

#include <cstdint>

/** App field slots for `news` rows (message body is LODB_F_DESCRIPTION). */
namespace NewsField
{
inline constexpr uint32_t FIELD_AUTHOR = 0;
} // namespace NewsField

/** App field slots for `news_reads` rows. */
namespace NewsReadField
{
inline constexpr uint32_t FIELD_NEWS_UUID = 0;
inline constexpr uint32_t FIELD_USER_UUID = 1;
} // namespace NewsReadField

#endif
