#include "TestSupport.hpp"

#include "messaging/MarketDataEnvelope.hpp"

#include <cstddef>

namespace md::test
{
namespace
{

class LegacyEnvelopeTestProcessor final : public IMarketDataEventProcessor
{
  public:
    void processMarketDataEvent(const MarketDataEvent& event) override
    {
        ++callback_count;
        captured_event = event;
    }

    std::size_t callback_count{};
    MarketDataEvent captured_event;
};

class EnvelopeAwareTestProcessor final : public IMarketDataEventProcessor
{
  public:
    void processMarketDataEvent(const MarketDataEvent&) override
    {
        ++legacy_callback_count;
    }

    void processMarketDataEnvelope(const MarketDataEnvelope& envelope) override
    {
        ++envelope_callback_count;
        captured_sequence = envelope.dispatch_seq;
    }

    std::size_t legacy_callback_count{};
    std::size_t envelope_callback_count{};
    DispatchSeq captured_sequence{invalid_dispatch_seq};
};

} // namespace

void testMarketDataEnvelopeProcessorCompatibility()
{
    MarketDataEvent event;
    event.timestamp = 101;
    event.order_id = 202;
    event.source_file_id = 3;
    event.source_sequence = 4;
    const MarketDataEnvelope envelope{first_dispatch_seq, event};

    LegacyEnvelopeTestProcessor legacy;
    IMarketDataEventProcessor& legacy_interface = legacy;
    legacy_interface.processMarketDataEnvelope(envelope);
    require(legacy.callback_count == 1, "default envelope bridge invokes legacy processor exactly once");
    require(legacy.captured_event.timestamp == event.timestamp, "default envelope bridge preserves event timestamp");
    require(legacy.captured_event.order_id == event.order_id, "default envelope bridge preserves event payload");
    require(legacy.captured_event.source_file_id == event.source_file_id, "default bridge preserves source file id");
    require(legacy.captured_event.source_sequence == event.source_sequence, "default bridge preserves source sequence");

    EnvelopeAwareTestProcessor envelope_aware;
    IMarketDataEventProcessor& envelope_interface = envelope_aware;
    envelope_interface.processMarketDataEnvelope(envelope);
    require(envelope_aware.envelope_callback_count == 1, "envelope-aware override is invoked exactly once");
    require(envelope_aware.captured_sequence == first_dispatch_seq, "envelope-aware override receives dispatch sequence");
    require(envelope_aware.legacy_callback_count == 0, "interface does not duplicate the legacy callback");
}

} // namespace md::test
