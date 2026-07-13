#include "TestSupport.hpp"

#include "domain/DispatchSequence.hpp"

#include <cstdint>
#include <limits>
#include <type_traits>

namespace md::test
{

void testDispatchSequenceSemantics()
{
    static_assert(std::is_same_v<DispatchSeq, std::uint64_t>);

    require(invalid_dispatch_seq == 0, "invalid dispatch sequence is zero");
    require(first_dispatch_seq == 1, "first dispatch sequence is one");
    require(!isValidDispatchSeq(invalid_dispatch_seq), "zero dispatch sequence is invalid");
    require(isValidDispatchSeq(first_dispatch_seq), "first dispatch sequence is valid");

    const auto first = checkedNextDispatchSeq(invalid_dispatch_seq);
    require(first.has_value() && *first == first_dispatch_seq, "dispatch sequence starts at one");

    const auto second = checkedNextDispatchSeq(first_dispatch_seq);
    require(second.has_value() && *second == 2, "dispatch sequence advances from one to two");

    DispatchSeq sequence = invalid_dispatch_seq;
    for (DispatchSeq expected = first_dispatch_seq; expected <= 4; ++expected)
    {
        const auto next = checkedNextDispatchSeq(sequence);
        require(next.has_value() && *next == expected, "dispatch sequence advances monotonically");
        sequence = *next;
    }

    constexpr auto maximum = std::numeric_limits<DispatchSeq>::max();
    const auto to_maximum = checkedNextDispatchSeq(maximum - 1);
    require(to_maximum.has_value() && *to_maximum == maximum, "dispatch sequence can advance to maximum");

    const auto overflow = checkedNextDispatchSeq(maximum);
    require(!overflow.has_value(), "maximum dispatch sequence rejects overflow");
    require(!overflow.has_value() || *overflow != invalid_dispatch_seq, "dispatch sequence never wraps to zero");
}

} // namespace md::test
