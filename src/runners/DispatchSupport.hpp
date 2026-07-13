#pragma once

#include "domain/DispatchSequence.hpp"
#include "messaging/MarketDataEnvelope.hpp"
#include "processing/IMarketDataEventProcessor.hpp"

#include <stdexcept>

namespace md::detail
{

inline constexpr char dispatch_sequence_overflow_message[] = "dispatch sequence exhausted";

inline void dispatchMarketDataEvent(
    DispatchSeq& last_issued,
    const MarketDataEvent& event,
    IMarketDataEventProcessor& processor)
{
    const auto next_sequence = checkedNextDispatchSeq(last_issued);
    if (!next_sequence.has_value())
    {
        throw std::overflow_error(dispatch_sequence_overflow_message);
    }

    last_issued = *next_sequence;
    processor.processMarketDataEnvelope(MarketDataEnvelope{last_issued, event});
}

} // namespace md::detail
