#pragma once

#include <cstdint>
#include <limits>

namespace md
{

using InstrumentId = std::uint64_t;
using HistoricalOrderId = std::uint64_t;
using OrderId = std::uint64_t;
using SyntheticOrderId = std::uint64_t;
using TradingEngineId = std::uint64_t;

using TimestampNs = std::int64_t;
using RawTimestampNs = std::uint64_t;

using Price = std::int64_t;
using Quantity = std::uint64_t;

using SeqNo = std::uint64_t;
using DispatchSeq = std::uint64_t;
using SourceFileId = std::uint32_t;
using SourceSequence = std::uint64_t;

inline constexpr InstrumentId invalid_instrument_id = 0;
inline constexpr HistoricalOrderId invalid_historical_order_id = 0;
inline constexpr OrderId invalid_order_id = 0;
inline constexpr SyntheticOrderId invalid_synthetic_order_id = 0;
inline constexpr TradingEngineId invalid_trading_engine_id = 0;
inline constexpr DispatchSeq invalid_dispatch_seq = 0;
inline constexpr DispatchSeq first_dispatch_seq = 1;

inline constexpr Price undefined_price = std::numeric_limits<Price>::max();
inline constexpr RawTimestampNs raw_undefined_timestamp = std::numeric_limits<RawTimestampNs>::max();

enum class Side : char
{
    Ask = 'A',
    Bid = 'B',
    None = 'N'
};

enum class OrderStatus : std::uint8_t
{
    PendingNew,
    Active,
    PartiallyFilled,
    Filled,
    CancelPending,
    Cancelled,
    Rejected
};

[[nodiscard]] constexpr bool isValidInstrumentId(InstrumentId instrument_id) noexcept
{
    return instrument_id != invalid_instrument_id;
}

[[nodiscard]] constexpr bool isValidHistoricalOrderId(HistoricalOrderId order_id) noexcept
{
    return order_id != invalid_historical_order_id;
}

[[nodiscard]] constexpr bool isValidOrderId(OrderId order_id) noexcept
{
    return order_id != invalid_order_id;
}

[[nodiscard]] constexpr bool isValidSyntheticOrderId(SyntheticOrderId order_id) noexcept
{
    return order_id != invalid_synthetic_order_id;
}

[[nodiscard]] constexpr bool isValidTradingEngineId(TradingEngineId engine_id) noexcept
{
    return engine_id != invalid_trading_engine_id;
}

[[nodiscard]] constexpr bool isValidDispatchSeq(DispatchSeq sequence) noexcept
{
    return sequence != invalid_dispatch_seq;
}

[[nodiscard]] constexpr bool isValidSide(Side side) noexcept
{
    return side == Side::Ask || side == Side::Bid;
}

[[nodiscard]] constexpr bool isDefinedPrice(Price price) noexcept
{
    return price != undefined_price;
}

} // namespace md
