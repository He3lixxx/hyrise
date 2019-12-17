#pragma once

#include <utility>
#include <vector>

#include "./chunk.hpp"
#include "types.hpp"
#include "utils/assert.hpp"

namespace opossum {

// For a long time, PosList was just a pmr_vector<RowID>. With this class, we want to add functionality to that vector,
// more specifically, flags that give us some guarantees about its contents. If we know, e.g., that all entries point
// into the same chunk, we can simplify things in split_pos_list_by_chunk_id.
// Inheriting from std::vector is generally not encouraged, because the STL containers are not prepared for
// inheritance. By making the inheritance private and this class final, we can assure that the problems that come with
// a non-virtual destructor do not occur.

struct PosList final : private pmr_vector<RowID> {
 public:
  using Vector = pmr_vector<RowID>;

  using value_type = Vector::value_type;
  using allocator_type = Vector::allocator_type;
  using size_type = Vector::size_type;
  using difference_type = Vector::difference_type;
  using reference = Vector::reference;
  using const_reference = Vector::const_reference;
  using pointer = Vector::pointer;
  using const_pointer = Vector::const_pointer;
  using reverse_iterator = Vector::reverse_iterator;
  using const_reverse_iterator = Vector::const_reverse_iterator;

  /********************************************
  * MatchesAllIterator
  *******************************************/
  template<bool Const = false>
  class MatchesAllIterator {
   public:
    // TODO: Do we want to give out a random access iterator? This breaks quite some stuff then
    // (We can not return pointers or references that do make much sense. Modification breaks.)
    using iterator_category = std::input_iterator_tag;
    using value_type = RowID;
    using difference_type = std::ptrdiff_t;
    using reference = typename std::conditional_t<Const, RowID const &, RowID &>;
    using pointer = typename std::conditional_t<Const, RowID const *, RowID *>;

    MatchesAllIterator(const ChunkID chunk_id, const ChunkOffset offset) : _current_row_id({chunk_id, offset}){};
    MatchesAllIterator(const MatchesAllIterator& other) = default;

    MatchesAllIterator& operator=(const MatchesAllIterator&) = default;

    MatchesAllIterator& operator++() {
      _current_row_id.chunk_offset++;
      return *this;
    }

    const MatchesAllIterator operator++(int) {
      MatchesAllIterator retval = *this;
      ++(*this);
      return retval;
    }

    bool operator==(MatchesAllIterator other) const { return _current_row_id == other._current_row_id; }

    bool operator!=(MatchesAllIterator other) const { return !(*this == other); }

    template<bool _Const = Const>
    std::enable_if_t<_Const, reference>
    operator*() const { return _current_row_id;}

    template<bool _Const = Const>
    std::enable_if_t<!_Const, reference>
    operator*() { return _current_row_id;}

   private:
    RowID _current_row_id;
  };

  /********************************************
  * Iterator - wrapper to either Vector::iterator or MatchesAllIterator
  *******************************************/
  template<bool Const = false, class BaseVectorIterator = Vector::iterator, class BaseMatchesAllIterator = MatchesAllIterator<Const>>
  class IteratorWrapper {
   public:
    // TODO: The returned iterators are used as random access iterators, e.g. there is a std::sort call.
    // Thus, to properly support this, we'd need to properly implement a random access iterator
    using iterator_category = std::input_iterator_tag;
    using value_type = RowID;
    using difference_type = std::ptrdiff_t;
    using reference = typename std::conditional_t<Const, RowID const &, RowID &>;
    using pointer = typename std::conditional_t<Const, RowID const *, RowID *>;

    IteratorWrapper() = delete;
    IteratorWrapper(std::unique_ptr<Vector::iterator> _vector_base_it) : _vector_base_it(std::move(_vector_base_it)){};
    IteratorWrapper(std::unique_ptr<BaseMatchesAllIterator> _matches_all_base_it)
        : _matches_all_base_it(std::move(_matches_all_base_it)){};

    IteratorWrapper(IteratorWrapper& from)
        : _vector_base_it(from._vector_base_it ? std::make_unique<Vector::iterator>(*from._vector_base_it) : nullptr),
          _matches_all_base_it(
              from._matches_all_base_it ? std::make_unique<BaseMatchesAllIterator>(*from._matches_all_base_it) : nullptr) {}

    IteratorWrapper& operator=(IteratorWrapper& from) {
      if (from._vector_base_it) {
        _vector_base_it = std::make_unique<Vector::iterator>(*from._vector_base_it);
        _matches_all_base_it = nullptr;
      } else {
        _vector_base_it = nullptr;
        _matches_all_base_it = std::make_unique<BaseMatchesAllIterator>(*from._matches_all_base_it);
      }
      return *this;
    }

    bool operator==(IteratorWrapper& other) {
      return _vector_base_it ? *_vector_base_it == *other._vector_base_it
                             : *_matches_all_base_it == *other._matches_all_base_it;
    }

    bool operator!=(IteratorWrapper& other) { return !(*this == other); }

    template<bool _Const = Const>
    std::enable_if_t<_Const, reference>
    operator*() const { return _vector_base_it ? **_vector_base_it : **_matches_all_base_it; }

    template<bool _Const = Const>
    std::enable_if_t<!_Const, reference>
    operator*() { return _vector_base_it ? **_vector_base_it : **_matches_all_base_it; }

    IteratorWrapper& operator++() {
      if (_vector_base_it)
        ++(*_vector_base_it);
      else
        ++(*_matches_all_base_it);
      return *this;
    }

    const IteratorWrapper operator++(int) {
      IteratorWrapper retval = *this;
      ++(*this);
      return retval;
    }

   private:
    std::unique_ptr<BaseVectorIterator> _vector_base_it;
    std::unique_ptr<BaseMatchesAllIterator> _matches_all_base_it;
  };

  using iterator = IteratorWrapper<false>;

  // TODO:
  /* using const_iterator = IteratorWrapper<true>; */
  using const_iterator = Vector::const_iterator;

  /* (1 ) */ PosList() noexcept(noexcept(allocator_type())) {}
  /* (1 ) */ explicit PosList(const allocator_type& allocator) noexcept : Vector(allocator) {}
  /* (2 ) */ PosList(size_type count, const RowID& value, const allocator_type& alloc = allocator_type())
      : Vector(count, value, alloc) {}
  /* (3 ) */ explicit PosList(size_type count, const allocator_type& alloc = allocator_type()) : Vector(count, alloc) {}
  /* (4 ) */ template <class InputIt>
  PosList(InputIt first, InputIt last, const allocator_type& alloc = allocator_type())
      : Vector(std::move(first), std::move(last)) {}
  /* (5 ) */  // PosList(const Vector& other) : Vector(other); - Oh no, you don't.
  /* (5 ) */  // PosList(const Vector& other, const allocator_type& alloc) : Vector(other, alloc);
  /* (6 ) */ PosList(PosList&& other) noexcept
      : Vector(std::move(other)), _references_single_chunk{other._references_single_chunk} {}
  /* (6+) */ explicit PosList(Vector&& other) noexcept : Vector(std::move(other)) {}
  /* (7 ) */ PosList(PosList&& other, const allocator_type& alloc)
      : Vector(std::move(other), alloc), _references_single_chunk{other._references_single_chunk} {}
  /* (7+) */ PosList(Vector&& other, const allocator_type& alloc) : Vector(std::move(other), alloc) {}
  /* (8 ) */ PosList(std::initializer_list<RowID> init, const allocator_type& alloc = allocator_type())
      : Vector(std::move(init), alloc) {}

  /* custom ctor: Match all entries in the given chunk */
  explicit PosList(std::shared_ptr<const Chunk> matches_all_chunk, const ChunkID chunkID)
      : _matches_all_chunk(matches_all_chunk), _matches_all_chunk_id(chunkID), _references_single_chunk(true) {}

  PosList& operator=(PosList&& other) = default;

  // If all entries in the PosList shares a single ChunkID, it makes sense to explicitly give this guarantee in order
  // to enable some optimizations.
  void guarantee_single_chunk() { _references_single_chunk = true; }

  // Returns whether the single ChunkID has been given (not necessarily, if it has been met)
  bool references_single_chunk() const {
    if (_references_single_chunk) {
      DebugAssert(
          [&]() {
            if (size() == 0) return true;
            const auto& common_chunk_id = (*this)[0].chunk_id;
            return std::all_of(cbegin(), cend(),
                               [&](const auto& row_id) { return row_id.chunk_id == common_chunk_id; });
          }(),
          "Chunk was marked as referencing only a single chunk, but references more");
    }
    return _references_single_chunk;
  }

  // For chunks that share a common ChunkID, returns that ID.
  ChunkID common_chunk_id() const {
    DebugAssert(references_single_chunk(),
                "Can only retrieve the common_chunk_id if the PosList is guaranteed to reference a single chunk.");
    Assert(!empty(), "Cannot retrieve common_chunk_id of an empty chunk");
    return (*this)[0].chunk_id;
  }

  using Vector::assign;
  using Vector::get_allocator;

  // Element access
  // using Vector::at; - Oh no. People have misused this in the past.
  using Vector::operator[];
  using Vector::back;
  using Vector::data;
  using Vector::front;

  // Iterators
  iterator begin() noexcept {
    if (_matches_all_chunk) {
      return iterator(std::make_unique<MatchesAllIterator<false>>(_matches_all_chunk_id, ChunkOffset(0)));
    }
    return iterator(std::make_unique<Vector::iterator>(Vector::begin()));
  }

  iterator end() noexcept {
    if (_matches_all_chunk) {
      ChunkOffset size = _matches_all_chunk->size();
      return iterator(std::make_unique<MatchesAllIterator<false>>(_matches_all_chunk_id, size));
    }
    return iterator(std::make_unique<Vector::iterator>(Vector::end()));
  }


  // TODO
  const_iterator begin() const noexcept {
    return Vector::begin();
  }

  // TODO
  const_iterator end() const noexcept {
    return Vector::end();
  }

  using Vector::cbegin;
  using Vector::cend;
  using Vector::crbegin;
  using Vector::crend;
  using Vector::rbegin;
  using Vector::rend;

  // Capacity
  using Vector::capacity;
  using Vector::empty;
  using Vector::max_size;
  using Vector::reserve;
  using Vector::shrink_to_fit;
  using Vector::size;

  // Modifiers
  using Vector::clear;
  using Vector::emplace;
  using Vector::emplace_back;
  using Vector::erase;
  using Vector::insert;
  using Vector::pop_back;
  using Vector::push_back;
  using Vector::resize;
  using Vector::swap;

  friend bool operator==(const PosList& lhs, const PosList& rhs);

  friend bool operator==(const PosList& lhs, const pmr_vector<RowID>& rhs);

  friend bool operator==(const pmr_vector<RowID>& lhs, const PosList& rhs);

 private:
  std::shared_ptr<const Chunk> _matches_all_chunk;
  ChunkID _matches_all_chunk_id;
  bool _references_single_chunk = false;
};

inline bool operator==(const PosList& lhs, const PosList& rhs) {
  return static_cast<const pmr_vector<RowID>&>(lhs) == static_cast<const pmr_vector<RowID>&>(rhs);
}

inline bool operator==(const PosList& lhs, const pmr_vector<RowID>& rhs) {
  return static_cast<const pmr_vector<RowID>&>(lhs) == rhs;
}

inline bool operator==(const pmr_vector<RowID>& lhs, const PosList& rhs) {
  return lhs == static_cast<const pmr_vector<RowID>&>(rhs);
}

}  // namespace opossum
