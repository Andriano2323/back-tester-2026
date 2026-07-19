#pragma once

#include "messaging/MarketDataEnvelope.hpp"

namespace md
{

class IMarketDataEventProcessor
{
  public:
    virtual ~IMarketDataEventProcessor() = default;
    virtual void processMarketDataEvent(const MarketDataEvent& event) = 0;

    virtual void processMarketDataEnvelope(const MarketDataEnvelope& envelope)
    {
        processMarketDataEvent(envelope.event);
    }
};

} // namespace md
