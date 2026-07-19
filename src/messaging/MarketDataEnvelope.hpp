#pragma once

#include "domain/MarketDataEvent.hpp"

namespace md
{

struct MarketDataEnvelope
{
    DispatchSeq dispatch_seq{invalid_dispatch_seq};
    MarketDataEvent event{};
};

[[nodiscard]] constexpr bool isValidMarketDataEnvelope(const MarketDataEnvelope& envelope) noexcept
{
    return isValidDispatchSeq(envelope.dispatch_seq);
}

} // namespace md
