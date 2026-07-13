#include "TestSupport.hpp"

#include "domain/Time.hpp"
#include "domain/Types.hpp"

#include <cstdint>
#include <limits>
#include <type_traits>

namespace md::test
{

static_assert(std::is_same_v<InstrumentId, std::uint64_t>);
static_assert(std::is_same_v<HistoricalOrderId, std::uint64_t>);
static_assert(std::is_same_v<OrderId, std::uint64_t>);
static_assert(std::is_same_v<SyntheticOrderId, std::uint64_t>);
static_assert(std::is_same_v<TradingEngineId, std::uint64_t>);
static_assert(std::is_same_v<TimestampNs, std::int64_t>);
static_assert(std::is_same_v<RawTimestampNs, std::uint64_t>);
static_assert(std::is_same_v<Price, std::int64_t>);
static_assert(std::is_same_v<Quantity, std::uint64_t>);
static_assert(std::is_same_v<SeqNo, std::uint64_t>);
static_assert(std::is_same_v<SourceFileId, std::uint32_t>);
static_assert(std::is_same_v<SourceSequence, std::uint64_t>);
static_assert(std::is_same_v<std::underlying_type_t<Side>, char>);
static_assert(std::is_same_v<std::underlying_type_t<OrderStatus>, std::uint8_t>);
static_assert(static_cast<char>(Side::Ask) == 'A');
static_assert(static_cast<char>(Side::Bid) == 'B');
static_assert(static_cast<char>(Side::None) == 'N');

void testCanonicalDomainTypes()
{
    require(isValidInstrumentId(1), "positive instrument id is valid");
    require(!isValidInstrumentId(invalid_instrument_id), "invalid instrument id is rejected");
    require(isValidHistoricalOrderId(1), "positive historical order id is valid");
    require(!isValidHistoricalOrderId(invalid_historical_order_id), "invalid historical order id is rejected");
    require(isValidOrderId(1), "positive order id is valid");
    require(!isValidOrderId(invalid_order_id), "invalid order id is rejected");
    require(isValidSyntheticOrderId(1), "positive synthetic order id is valid");
    require(!isValidSyntheticOrderId(invalid_synthetic_order_id), "invalid synthetic order id is rejected");
    require(isValidTradingEngineId(1), "positive trading engine id is valid");
    require(!isValidTradingEngineId(invalid_trading_engine_id), "invalid trading engine id is rejected");

    require(isValidSide(Side::Ask), "ask side is valid");
    require(isValidSide(Side::Bid), "bid side is valid");
    require(!isValidSide(Side::None), "none side is invalid");
    require(isDefinedPrice(0), "zero price is defined");
    require(!isDefinedPrice(undefined_price), "undefined price is rejected");
    require(OrderStatus::PendingNew != OrderStatus::Active, "order status values are available");
}

void testCheckedTimestampAndLatencyArithmetic()
{
    const auto zero_timestamp = checkedTimestampFromRaw(0);
    require(zero_timestamp.has_value() && *zero_timestamp == 0, "raw zero timestamp converts");

    constexpr TimestampNs max_timestamp = std::numeric_limits<TimestampNs>::max();
    constexpr RawTimestampNs raw_max_timestamp = static_cast<RawTimestampNs>(max_timestamp);
    const auto max_conversion = checkedTimestampFromRaw(raw_max_timestamp);
    require(max_conversion.has_value() && *max_conversion == max_timestamp, "raw INT64_MAX timestamp converts");
    require(!checkedTimestampFromRaw(raw_max_timestamp + 1).has_value(), "raw value above INT64_MAX is rejected");
    require(!checkedTimestampFromRaw(raw_undefined_timestamp).has_value(), "raw undefined timestamp is rejected");

    const auto normal_addition = checkedAddLatency(100, 25);
    require(normal_addition.has_value() && *normal_addition == 125, "normal latency addition succeeds");
    const auto zero_latency = checkedAddLatency(100, 0);
    require(zero_latency.has_value() && *zero_latency == 100, "zero latency preserves timestamp");
    require(!checkedAddLatency(-1, 0).has_value(), "negative base timestamp is rejected");
    require(!checkedAddLatency(0, -1).has_value(), "negative latency is rejected");
    require(!checkedAddLatency(max_timestamp, 1).has_value(), "positive timestamp overflow is rejected");

    const auto boundary_addition = checkedAddLatency(max_timestamp - 5, 5);
    require(
        boundary_addition.has_value() && *boundary_addition == max_timestamp,
        "latency addition may exactly reach INT64_MAX");
}

} // namespace md::test
