#pragma once

#include "domain/MarketDataEvent.hpp"

#include <cstddef>
#include <functional>
#include <iosfwd>
#include <map>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace md
{

struct UnknownOrderDiagnostic
{
    std::string operation;
    std::string decision;
    RawTimestampNs timestamp{};
    InstrumentId instrument_id{};
    HistoricalOrderId order_id{};
    Side side{Side::None};
    Price price{};
    Quantity size{};
    SourceFileId source_file_id{};
    SourceSequence source_sequence{};
    std::size_t line_number{};
};

class LimitOrderBook
{
  public:
    using BidLevels = std::map<Price, Quantity, std::greater<>>;
    using AskLevels = std::map<Price, Quantity>;

    explicit LimitOrderBook(InstrumentId instrument_id);

    void apply(const MarketDataEvent& event);

    [[nodiscard]] std::optional<Price> bestBid() const;
    [[nodiscard]] std::optional<Price> bestAsk() const;

    [[nodiscard]] Quantity volumeAt(Side side, Price price) const;
    [[nodiscard]] std::size_t restingOrderCount() const noexcept;
    [[nodiscard]] std::size_t skippedUnknownOrderCount() const noexcept;
    [[nodiscard]] std::size_t unknownModifyRecoveredAsAddCount() const noexcept;
    [[nodiscard]] std::size_t unknownModifySkippedCount() const noexcept;
    [[nodiscard]] std::size_t unknownCancelSkippedCount() const noexcept;
    [[nodiscard]] std::size_t tradeCount() const noexcept;
    [[nodiscard]] std::size_t fillCount() const noexcept;
    [[nodiscard]] InstrumentId instrumentId() const noexcept;
    [[nodiscard]] bool containsOrder(HistoricalOrderId order_id) const noexcept;
    [[nodiscard]] const BidLevels& bidLevelsView() const noexcept;
    [[nodiscard]] const AskLevels& askLevelsView() const noexcept;
    [[nodiscard]] std::vector<std::pair<Price, Quantity>> bidLevels() const;
    [[nodiscard]] std::vector<std::pair<Price, Quantity>> askLevels() const;
    [[nodiscard]] std::vector<std::pair<Price, Quantity>> bidLevels(std::size_t depth) const;
    [[nodiscard]] std::vector<std::pair<Price, Quantity>> askLevels(std::size_t depth) const;
    [[nodiscard]] const std::vector<UnknownOrderDiagnostic>& unknownOrderDiagnostics() const noexcept;

    void printSnapshot(std::ostream& out, std::size_t depth) const;

  private:
    struct RestingOrder
    {
        Side side{Side::None};
        Price price{};
        Quantity size{};
    };

    void applyAdd(const MarketDataEvent& event);
    void applyCancel(const MarketDataEvent& event);
    void applyModify(const MarketDataEvent& event);
    void applyClear(const MarketDataEvent& event);
    void applyTrade(const MarketDataEvent& event);
    void applyFill(const MarketDataEvent& event);
    void removeOrder(HistoricalOrderId order_id);
    void addLevelVolume(Side side, Price price, Quantity size);
    void removeLevelVolume(Side side, Price price, Quantity size);
    void recordUnknownOrderDiagnostic(
        const MarketDataEvent& event,
        const std::string& operation,
        const std::string& decision);

    InstrumentId instrument_id_{};
    BidLevels bids_;
    AskLevels asks_;
    std::unordered_map<HistoricalOrderId, RestingOrder> orders_;
    std::size_t unknown_modify_recovered_as_add_count_{};
    std::size_t unknown_modify_skipped_count_{};
    std::size_t unknown_cancel_skipped_count_{};
    std::size_t trade_count_{};
    std::size_t fill_count_{};
    std::vector<UnknownOrderDiagnostic> unknown_order_diagnostics_;
};

} // namespace md
