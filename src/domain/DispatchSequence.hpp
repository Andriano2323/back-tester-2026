#pragma once

#include "domain/Types.hpp"

#include <limits>
#include <optional>

namespace md
{

[[nodiscard]] constexpr std::optional<DispatchSeq> checkedNextDispatchSeq(DispatchSeq last_issued) noexcept
{
    if (last_issued == std::numeric_limits<DispatchSeq>::max())
    {
        return std::nullopt;
    }

    return last_issued + 1;
}

} // namespace md
