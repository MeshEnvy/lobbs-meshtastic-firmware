#pragma once
#if !MESHTASTIC_EXCLUDE_LOBBS

#include <cstdint>

/** `wall_canvas`. */
namespace WallCanvasField
{
inline constexpr uint32_t FIELD_CELLS = 0;
inline constexpr uint32_t FIELD_CRC32 = 1;
} // namespace WallCanvasField

/** `wall_config`. */
namespace WallConfigField
{
inline constexpr uint32_t FIELD_PERIOD_SEC = 0;
inline constexpr uint32_t FIELD_MAX_CELLS = 1;
} // namespace WallConfigField

/** `wall_quota`. */
namespace WallQuotaField
{
inline constexpr uint32_t FIELD_USER_UUID = 0;
inline constexpr uint32_t FIELD_CYCLE_START = 1;
inline constexpr uint32_t FIELD_CELLS_USED = 2;
} // namespace WallQuotaField

/** `wall_seen`. */
namespace WallSeenField
{
inline constexpr uint32_t FIELD_USER_UUID = 0;
inline constexpr uint32_t FIELD_CRC32 = 1;
} // namespace WallSeenField

#endif
