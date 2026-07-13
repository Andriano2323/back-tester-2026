#include "TestSupport.hpp"

#include "trading/TradingEngineConfig.hpp"

#include <limits>

namespace md::test
{

void testTradingEngineConfigValidation()
{
    const TradingEngineConfig valid_config{
        .engine_id = 1,
        .market_data_latency_ns = 100,
        .order_latency_ns = 200,
    };
    require(
        validateTradingEngineConfig(valid_config) == TradingEngineConfigError::None,
        "valid trading engine config is accepted");

    const TradingEngineConfig invalid_engine{
        .engine_id = invalid_trading_engine_id,
        .market_data_latency_ns = -1,
        .order_latency_ns = -1,
    };
    require(
        validateTradingEngineConfig(invalid_engine) == TradingEngineConfigError::InvalidEngineId,
        "invalid engine id has validation precedence");

    const TradingEngineConfig negative_market_data{
        .engine_id = 1,
        .market_data_latency_ns = -1,
        .order_latency_ns = -1,
    };
    require(
        validateTradingEngineConfig(negative_market_data) == TradingEngineConfigError::NegativeMarketDataLatency,
        "negative market-data latency precedes negative order latency");

    const TradingEngineConfig negative_order{
        .engine_id = 1,
        .market_data_latency_ns = 0,
        .order_latency_ns = -1,
    };
    require(
        validateTradingEngineConfig(negative_order) == TradingEngineConfigError::NegativeOrderLatency,
        "negative order latency is rejected");

    const TradingEngineConfig zero_latencies{
        .engine_id = 1,
        .market_data_latency_ns = 0,
        .order_latency_ns = 0,
    };
    require(
        validateTradingEngineConfig(zero_latencies) == TradingEngineConfigError::None,
        "zero latencies are valid");

    const TradingEngineConfig large_latencies{
        .engine_id = 1,
        .market_data_latency_ns = std::numeric_limits<TimestampNs>::max(),
        .order_latency_ns = std::numeric_limits<TimestampNs>::max(),
    };
    require(
        validateTradingEngineConfig(large_latencies) == TradingEngineConfigError::None,
        "non-negative latency is valid independently of timestamp addition");

    const TradingEngineConfig engine_one{
        .engine_id = 1,
        .market_data_latency_ns = 10,
        .order_latency_ns = 20,
    };
    const TradingEngineConfig engine_two{
        .engine_id = 2,
        .market_data_latency_ns = 30,
        .order_latency_ns = 40,
    };
    require(
        engine_one.market_data_latency_ns != engine_two.market_data_latency_ns &&
            engine_one.order_latency_ns != engine_two.order_latency_ns,
        "engines may use different fixed latencies");
    require(engine_one == TradingEngineConfig{engine_one}, "equal trading engine configs compare equal");
    require(!(engine_one == engine_two), "different trading engine configs compare unequal");
}

} // namespace md::test
