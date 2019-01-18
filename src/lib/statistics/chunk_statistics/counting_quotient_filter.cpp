#include "counting_quotient_filter.hpp"

#include <cmath>
#include <iostream>
#include <string>

#include "storage/segment_iterate.hpp"

using namespace gqf2;   // NOLINT
using namespace gqf4;   // NOLINT
using namespace gqf8;   // NOLINT
using namespace gqf16;  // NOLINT
using namespace gqf32;  // NOLINT

namespace opossum {

template <typename ElementType>
CountingQuotientFilter<ElementType>::CountingQuotientFilter(const size_t quotient_size, const size_t remainder_size)
    : _hash_bits(quotient_size + remainder_size) {
  Assert(quotient_size > 0, "Quotient size can not be zero.");
  Assert(_hash_bits <= 64u, "Hash length can not exceed 64 bits.");

  // Floating point types are unsupported because equality checks for floating
  // point values are cumbersome and thus is hashing them even more so.
  Assert(!std::is_floating_point<ElementType>::value, "Quotient filters do not support floating point types.");

  if (remainder_size == 2) {
    _quotient_filter = gqf2::quotient_filter{};
  } else if (remainder_size == 4) {
    _quotient_filter = gqf4::quotient_filter{};
  } else if (remainder_size == 8) {
    _quotient_filter = gqf8::quotient_filter{};
  } else if (remainder_size == 16) {
    _quotient_filter = gqf16::quotient_filter{};
  } else if (remainder_size == 32) {
    _quotient_filter = gqf32::quotient_filter{};
  } else {
    Fail("Invalid remainder_size");
  }

  const auto number_of_slots = static_cast<size_t>(std::pow(2, quotient_size));
  std::visit([&](auto& filter) { qf_init(&filter, number_of_slots, _hash_bits, 0); }, _quotient_filter);
}

template <typename ElementType>
CountingQuotientFilter<ElementType>::~CountingQuotientFilter() {
  std::visit([&](auto& filter) { qf_destroy(&filter); }, _quotient_filter);
}

template <typename ElementType>
void CountingQuotientFilter<ElementType>::insert(ElementType value, size_t count) {
  const auto hash = get_hash_bits(value, _hash_bits);
  std::visit([&](auto& filter) { qf_insert(&filter, hash, 0, count); }, _quotient_filter);
}

template <typename ElementType>
size_t CountingQuotientFilter<ElementType>::count(const AllTypeVariant& value) const {
  DebugAssert(std::holds_alternative<ElementType>(value), "Value does not have the same type as the filter elements");
  return count(type_cast_variant<ElementType>(value));
}

template <typename ElementType>
bool CountingQuotientFilter<ElementType>::can_prune(const PredicateCondition predicate_type,
                                                    const AllTypeVariant& value,
                                                    const std::optional<AllTypeVariant>& variant_value2) const {
  DebugAssert(predicate_type == PredicateCondition::Equals && !variant_value2, "CQF only supports equality predicates");
  return count(value) == 0;
}

template <typename ElementType>
size_t CountingQuotientFilter<ElementType>::count(const ElementType& value) const {
  const auto hash = get_hash_bits(value, _hash_bits);

  auto count = size_t{0};
  std::visit([&](auto& filter) { count = qf_count_key_value(&filter, hash, 0); }, _quotient_filter);
  return count;
}

template <typename ElementType>
inline __attribute__((always_inline)) uint64_t CountingQuotientFilter<ElementType>::get_hash_bits(
    const ElementType& value, const uint64_t bit_count) {
  /*
   * Counting Quotient Filters use variable length hash values to build their internal data structures.
   * These can be as low 6 bits. Hence, it has to be ensured that the lower bits include enough entropy.
   * Using std::hash and the least significant bits can lead to ineffective pruning and bad cardinality
   * estimations. As a consequence, we use multiply-shift (cf. Richter et al., A Seven-Dimensional
   * Analysis of Hashing Methods and its Implications on Query Processing, PVLDB 2015) to generate
   * fast but sufficiently scrambled hashes (here in form of fibonacci hashing, cf.
   * https://probablydance.com/2018/06/16/fibonacci-hashing-the-optimization-
   * that-the-world-forgot-or-a-better-alternative-to-integer-modulo/)
   */
  return static_cast<uint64_t>((std::hash<ElementType>{}(value)*11400714819323198485llu) >> (64 - bit_count));
}

template <typename ElementType>
void CountingQuotientFilter<ElementType>::populate(const std::shared_ptr<const BaseSegment>& segment) {
  segment_iterate<ElementType>(*segment, [&](const auto& position) {
    if (position.is_null()) return;
    insert(position.value());
  });
}

template <typename ElementType>
size_t CountingQuotientFilter<ElementType>::memory_consumption() const {
  size_t consumption = 0;
  std::visit([&](auto& filter) { consumption = qf_memory_consumption(filter); }, _quotient_filter);
  return consumption;
}

template <typename ElementType>
float CountingQuotientFilter<ElementType>::load_factor() const {
  auto load_factor = 0.f;
  std::visit([&](auto& filter) { load_factor = filter.noccupied_slots / static_cast<float>(filter.nslots); },
                       _quotient_filter);
  return load_factor;
}

template <typename ElementType>
bool CountingQuotientFilter<ElementType>::is_full() const {
  return load_factor() > 0.99f;
}

EXPLICITLY_INSTANTIATE_DATA_TYPES(CountingQuotientFilter);

}  // namespace opossum
