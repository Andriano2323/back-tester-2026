#pragma once

#include "domain/Types.hpp"

#include <limits>
#include <optional>

namespace md
{

[[nodiscard]] constexpr std::optional<TimestampNs> checkedTimestampFromRaw(
    RawTimestampNs raw_timestamp) noexcept
{
    if (raw_timestamp == raw_undefined_timestamp ||
        raw_timestamp > static_cast<RawTimestampNs>(std::numeric_limits<TimestampNs>::max()))
    {
        return std::nullopt;
    }

    return static_cast<TimestampNs>(raw_timestamp);
}

[[nodiscard]] constexpr std::optional<TimestampNs> checkedAddLatency(
    TimestampNs base_timestamp_ns,
    TimestampNs latency_ns) noexcept
{
    if (base_timestamp_ns < 0 || latency_ns < 0)
    {
        return std::nullopt;
    }

    if (base_timestamp_ns > std::numeric_limits<TimestampNs>::max() - latency_ns)
    {
        return std::nullopt;
    }

    return base_timestamp_ns + latency_ns;
}

} // namespace md
