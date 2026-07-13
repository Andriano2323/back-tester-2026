#pragma once

#include "domain/Types.hpp"

#include <cstddef>
#include <optional>
#include <vector>

namespace md
{

struct PriceLevelSnapshot
{
    Price price{};
    Quantity size{};
};

struct InstrumentBookSnapshot
{
    InstrumentId instrument_id{};
    std::size_t resting_orders{};
    std::optional<Price> best_bid;
    std::optional<Price> best_ask;
    std::vector<PriceLevelSnapshot> bids;
    std::vector<PriceLevelSnapshot> asks;
};

struct BookManagerSnapshot
{
    std::size_t event_count{};
    RawTimestampNs timestamp{};
    std::size_t processed_events{};
    std::size_t unresolved_events{};
    std::vector<InstrumentBookSnapshot> instruments;
};

} // namespace md
