#include "TestSupport.hpp"

#include "messaging/MarketDataEnvelope.hpp"
#include "runners/FlatMergeRunner.hpp"
#include "runners/HierarchicalMergeRunner.hpp"
#include "runners/StandardRunner.hpp"

#include <cstddef>
#include <sstream>
#include <string>
#include <vector>

namespace md::test
{
namespace
{

class EnvelopeCapturingProcessor final : public IMarketDataEventProcessor
{
  public:
    void processMarketDataEvent(const MarketDataEvent&) override
    {
        ++legacy_callback_count;
    }

    void processMarketDataEnvelope(const MarketDataEnvelope& envelope) override
    {
        envelopes.push_back(envelope);
    }

    std::vector<MarketDataEnvelope> envelopes;
    std::size_t legacy_callback_count{};
};

bool eventsAreEqual(const MarketDataEvent& lhs, const MarketDataEvent& rhs)
{
    return lhs.timestamp == rhs.timestamp && lhs.ts_recv == rhs.ts_recv && lhs.ts_event == rhs.ts_event &&
           lhs.order_id == rhs.order_id && lhs.side == rhs.side && lhs.price == rhs.price && lhs.size == rhs.size &&
           lhs.action == rhs.action && lhs.instrument_id == rhs.instrument_id &&
           lhs.source_file_id == rhs.source_file_id && lhs.source_sequence == rhs.source_sequence &&
           lhs.line_number == rhs.line_number;
}

void requireContiguousSequences(
    const std::vector<MarketDataEnvelope>& envelopes,
    std::size_t expected_count,
    const std::string& case_name)
{
    require(envelopes.size() == expected_count, case_name + ": unexpected envelope count");
    for (std::size_t index = 0; index < envelopes.size(); ++index)
    {
        const auto expected = static_cast<DispatchSeq>(index) + first_dispatch_seq;
        require(envelopes[index].dispatch_seq == expected, case_name + ": non-contiguous dispatch sequence");
    }
}

void requireSameOrderedEvents(
    const std::vector<MarketDataEnvelope>& lhs,
    const std::vector<MarketDataEnvelope>& rhs,
    const std::string& case_name)
{
    require(lhs.size() == rhs.size(), case_name + ": event counts differ");
    for (std::size_t index = 0; index < lhs.size(); ++index)
    {
        require(eventsAreEqual(lhs[index].event, rhs[index].event), case_name + ": ordered event payload differs");
        require(lhs[index].dispatch_seq == rhs[index].dispatch_seq, case_name + ": dispatch sequence differs");
    }
}

} // namespace

void testStandardRunnerDispatchEnvelopes()
{
    const auto input = testDataDir() / "single_valid.ndjson";
    EnvelopeCapturingProcessor envelope_processor;
    std::ostringstream envelope_err;
    const auto result = StandardRunner{}.run(input, envelope_processor, false, envelope_err);

    requireContiguousSequences(
        envelope_processor.envelopes,
        result.summary.total_messages_processed,
        "standard dispatch envelopes");
    require(envelope_processor.legacy_callback_count == 0, "standard uses the envelope-aware callback only");

    CapturingProcessor legacy_processor;
    std::ostringstream legacy_err;
    const auto legacy_result = StandardRunner{}.run(input, legacy_processor, false, legacy_err);
    require(legacy_processor.events.size() == envelope_processor.envelopes.size(), "standard historical event count is unchanged");
    require(legacy_result.summary.total_messages_processed == result.summary.total_messages_processed, "standard summary count is unchanged");
    for (std::size_t index = 0; index < legacy_processor.events.size(); ++index)
    {
        require(
            eventsAreEqual(envelope_processor.envelopes[index].event, legacy_processor.events[index]),
            "standard preserves ordered event payload and source metadata");
    }
}

void testRepeatedStandardRunnerDispatchSequencesRestart()
{
    const auto input = testDataDir() / "single_valid.ndjson";
    EnvelopeCapturingProcessor first_processor;
    EnvelopeCapturingProcessor second_processor;
    std::ostringstream first_err;
    std::ostringstream second_err;

    const auto first_result = StandardRunner{}.run(input, first_processor, false, first_err);
    const auto second_result = StandardRunner{}.run(input, second_processor, false, second_err);

    requireContiguousSequences(
        first_processor.envelopes,
        first_result.summary.total_messages_processed,
        "first repeated standard run");
    requireContiguousSequences(
        second_processor.envelopes,
        second_result.summary.total_messages_processed,
        "second repeated standard run");
    require(first_processor.envelopes.front().dispatch_seq == first_dispatch_seq, "first standard run starts at one");
    require(second_processor.envelopes.front().dispatch_seq == first_dispatch_seq, "second standard run restarts at one");
}

void testFlatAndHierarchyDispatchEnvelopesMatch()
{
    const auto input = testDataDir() / "multi";
    EnvelopeCapturingProcessor flat_processor;
    EnvelopeCapturingProcessor hierarchy_processor;
    std::ostringstream flat_err;
    std::ostringstream hierarchy_err;

    const auto flat_result = FlatMergeRunner{}.run(input, flat_processor, false, flat_err);
    const auto hierarchy_result = HierarchicalMergeRunner{}.run(input, hierarchy_processor, false, hierarchy_err);

    requireContiguousSequences(flat_processor.envelopes, flat_result.summary.total_messages_processed, "flat dispatch envelopes");
    requireContiguousSequences(
        hierarchy_processor.envelopes,
        hierarchy_result.summary.total_messages_processed,
        "hierarchy dispatch envelopes");
    require(flat_result.summary.chronological_violations == 0, "flat dispatch remains chronological");
    require(hierarchy_result.summary.chronological_violations == 0, "hierarchy dispatch remains chronological");
    requireSameOrderedEvents(flat_processor.envelopes, hierarchy_processor.envelopes, "flat and hierarchy dispatch");
}

void testEqualTimestampDispatchOrderRemainsDeterministic()
{
    const auto input = testDataDir() / "hard_lob_equal_timestamps";
    EnvelopeCapturingProcessor flat_processor;
    EnvelopeCapturingProcessor hierarchy_processor;
    std::ostringstream flat_err;
    std::ostringstream hierarchy_err;

    const auto flat_result = FlatMergeRunner{}.run(input, flat_processor, false, flat_err);
    const auto hierarchy_result = HierarchicalMergeRunner{}.run(input, hierarchy_processor, false, hierarchy_err);

    requireContiguousSequences(flat_processor.envelopes, flat_result.summary.total_messages_processed, "equal-time flat dispatch");
    requireContiguousSequences(
        hierarchy_processor.envelopes,
        hierarchy_result.summary.total_messages_processed,
        "equal-time hierarchy dispatch");
    requireSameOrderedEvents(flat_processor.envelopes, hierarchy_processor.envelopes, "equal-time hard-mode dispatch");
    require(flat_processor.envelopes.size() == 2, "equal-time fixture contains two events");
    require(
        flat_processor.envelopes[0].event.timestamp == flat_processor.envelopes[1].event.timestamp,
        "equal historical timestamps remain equal");
    require(flat_processor.envelopes[0].event.source_file_id == 0, "equal-time order starts with source file zero");
    require(flat_processor.envelopes[0].event.source_sequence == 1, "equal-time first source sequence is preserved");
    require(flat_processor.envelopes[1].event.source_file_id == 1, "equal-time order continues with source file one");
    require(flat_processor.envelopes[1].event.source_sequence == 1, "equal-time second source sequence is preserved");
}

void testLegacyProcessorRemainsRunnerCompatible()
{
    CapturingProcessor processor;
    std::ostringstream err;
    const auto result = StandardRunner{}.run(testDataDir() / "single_valid.ndjson", processor, false, err);

    require(processor.events.size() == result.summary.total_messages_processed, "legacy runner processor receives every event");
    require(processor.events.size() == 2, "legacy runner processor receives the complete fixture");
}

} // namespace md::test
