#pragma once

#include "domain/Types.hpp"

#include <cstdint>

namespace md
{

struct TradingEngineConfig
{
    TradingEngineId engine_id{invalid_trading_engine_id};
    TimestampNs market_data_latency_ns{};
    TimestampNs order_latency_ns{};

    bool operator==(const TradingEngineConfig&) const = default;
};

enum class TradingEngineConfigError : std::uint8_t
{
    None,
    InvalidEngineId,
    NegativeMarketDataLatency,
    NegativeOrderLatency
};

[[nodiscard]] constexpr TradingEngineConfigError validateTradingEngineConfig(
    const TradingEngineConfig& config) noexcept
{
    if (!isValidTradingEngineId(config.engine_id))
    {
        return TradingEngineConfigError::InvalidEngineId;
    }
    if (config.market_data_latency_ns < 0)
    {
        return TradingEngineConfigError::NegativeMarketDataLatency;
    }
    if (config.order_latency_ns < 0)
    {
        return TradingEngineConfigError::NegativeOrderLatency;
    }
    return TradingEngineConfigError::None;
}

} // namespace md
