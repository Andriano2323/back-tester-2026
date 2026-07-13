#pragma once

#include "domain/MarketDataEvent.hpp"
#include "domain/Types.hpp"

namespace md::lob
{

using InstrumentId = md::InstrumentId;
using HistoricalOrderId = md::HistoricalOrderId;
using SyntheticOrderId = md::SyntheticOrderId;
using EngineId = md::TradingEngineId;
using TimestampNs = md::TimestampNs;
using Price = md::Price;
using Quantity = md::Quantity;

using Side = md::Side;
using Action = md::Action;

} // namespace md::lob
