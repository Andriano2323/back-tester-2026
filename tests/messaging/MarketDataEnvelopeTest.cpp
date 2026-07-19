#include "TestSupport.hpp"

#include "messaging/MarketDataEnvelope.hpp"

#include <cstdint>
#include <type_traits>
#include <utility>

namespace md::test
{
namespace
{

bool eventsAreEqual(const MarketDataEvent& lhs, const MarketDataEvent& rhs)
{
    return lhs.timestamp == rhs.timestamp && lhs.ts_recv == rhs.ts_recv && lhs.ts_event == rhs.ts_event &&
           lhs.order_id == rhs.order_id && lhs.side == rhs.side && lhs.price == rhs.price && lhs.size == rhs.size &&
           lhs.action == rhs.action && lhs.instrument_id == rhs.instrument_id &&
           lhs.source_file_id == rhs.source_file_id && lhs.source_sequence == rhs.source_sequence &&
           lhs.line_number == rhs.line_number;
}

MarketDataEvent makeEnvelopeTestEvent()
{
    MarketDataEvent event;
    event.timestamp = 101;
    event.ts_recv = 102;
    event.ts_event = 103;
    event.order_id = 104;
    event.side = Side::Bid;
    event.price = 105;
    event.size = 106;
    event.action = Action::Modify;
    event.instrument_id = 107;
    event.source_file_id = 8;
    event.source_sequence = 109;
    event.line_number = 10;
    return event;
}

} // namespace

void testMarketDataEnvelopeSemantics()
{
    static_assert(std::is_copy_constructible_v<MarketDataEnvelope>);
    static_assert(std::is_move_constructible_v<MarketDataEnvelope>);

    const MarketDataEnvelope default_envelope;
    require(!isValidMarketDataEnvelope(default_envelope), "default market-data envelope is invalid");

    const auto event = makeEnvelopeTestEvent();
    const MarketDataEnvelope envelope{first_dispatch_seq, event};
    require(isValidMarketDataEnvelope(envelope), "market-data envelope with sequence one is valid");
    require(eventsAreEqual(envelope.event, event), "market-data envelope preserves the complete event payload");
    require(envelope.event.timestamp == event.timestamp, "historical timestamp remains in the contained event");

    const MarketDataEnvelope copied{envelope};
    require(eventsAreEqual(copied.event, event), "copied market-data envelope preserves the event");

    MarketDataEnvelope movable{envelope};
    const MarketDataEnvelope moved{std::move(movable)};
    require(eventsAreEqual(moved.event, event), "moved market-data envelope preserves the event");

    const SeqNo application_sequence = 77;
    require(envelope.dispatch_seq != event.source_sequence, "dispatch sequence is separate from source sequence");
    require(envelope.dispatch_seq != application_sequence, "dispatch sequence is semantically separate from SeqNo");
    require(envelope.event.source_sequence == 109, "envelope does not replace event source sequence");
}

} // namespace md::test
