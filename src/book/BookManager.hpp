#pragma once

#include "book/BookSnapshot.hpp"
#include "book/LimitOrderBook.hpp"
#include "domain/MarketDataEvent.hpp"

#include <cstddef>
#include <iosfwd>
#include <string>
#include <unordered_map>

namespace md
{

class BookManager
{
  public:
    void apply(const MarketDataEvent& event);

    [[nodiscard]] const LimitOrderBook* findBook(InstrumentId instrument_id) const;
    [[nodiscard]] LimitOrderBook& getOrCreateBook(InstrumentId instrument_id);

    [[nodiscard]] std::size_t instrumentCount() const noexcept;
    [[nodiscard]] std::size_t processedEvents() const noexcept;
    [[nodiscard]] std::size_t unresolvedEvents() const noexcept;
    [[nodiscard]] std::size_t unknownModifyRecoveredAsAddCount() const noexcept;
    [[nodiscard]] std::size_t unknownModifySkippedCount() const noexcept;
    [[nodiscard]] std::size_t unknownCancelSkippedCount() const noexcept;
    [[nodiscard]] std::string stableStateDigest() const;
    [[nodiscard]] BookManagerSnapshot snapshot(
        std::size_t event_count,
        RawTimestampNs timestamp,
        std::size_t depth) const;

    void printSnapshot(std::ostream& out, std::size_t depth) const;
    void printFinalBestBidAsk(std::ostream& out) const;

  private:
    [[nodiscard]] InstrumentId resolveInstrumentId(const MarketDataEvent& event) const;
    void updateOrderMapping(const MarketDataEvent& event, const LimitOrderBook& book);
    void eraseOrderMappingIfMatches(HistoricalOrderId order_id, InstrumentId instrument_id);
    void eraseOrderMappingsForInstrument(InstrumentId instrument_id);
    void removePreviousInstrumentMapping(const MarketDataEvent& event, InstrumentId target_instrument_id);

    std::unordered_map<InstrumentId, LimitOrderBook> books_by_instrument_;
    std::unordered_map<HistoricalOrderId, InstrumentId> order_to_instrument_;
    std::size_t processed_events_{};
    std::size_t unresolved_events_{};
};

} // namespace md
