#pragma once

#include <utility>
#include <vector>

#include "./chunk.hpp"
#include "types.hpp"
#include "utils/assert.hpp"
#include "utils/performance_warning.hpp"

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
  using iterator = Vector::iterator;
  using const_iterator = Vector::const_iterator;
  using reverse_iterator = Vector::reverse_iterator;
  using const_reverse_iterator = Vector::const_reverse_iterator;

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
      : _matches_all_chunk(matches_all_chunk), _matches_all_chunk_id(chunkID) {
        guarantee_single_chunk();
      }

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

    if (_matches_all_chunk) {
      DebugAssert(_references_single_chunk == true, "Chunk was marked to reference a whole chunk but references_single_chunk would return false");
    }

    return _references_single_chunk;
  }

  bool matches_complete_chunk() const {
    return _matches_all_chunk != nullptr;
  }

  // For chunks that share a common ChunkID, returns that ID.
  ChunkID common_chunk_id() const {
    DebugAssert(references_single_chunk(),
                "Can only retrieve the common_chunk_id if the PosList is guaranteed to reference a single chunk.");
    Assert(!empty(), "Cannot retrieve common_chunk_id of an empty chunk");

    if (_matches_all_chunk) {
      return _matches_all_chunk_id;
    }

    return (*this)[0].chunk_id;
  }

  /* using Vector::assign; */
  /* using Vector::get_allocator; */

  // Element access
  // using Vector::at; - Oh no. People have misused this in the past.
  /* using Vector::back; */
  /* using Vector::data; */
  /* using Vector::front; */

  reference operator[] (size_type n) {
    _materialize_if_necessary();
    return Vector::operator[](n);
  }

  const_reference operator[] (size_type n) const {
    // TODO: No need to materialize here
    const_cast<PosList*>(this)->_materialize_if_necessary();
    return Vector::operator[](n);
  }

  // Iterators
  iterator begin() {
    _materialize_if_necessary();
    return Vector::begin();
  }

  const_iterator begin() const {
    return cbegin();
  }

  iterator end() {
    _materialize_if_necessary();
    return Vector::end();
  }

  const_iterator end() const {
    return cend();
  }

  const_iterator cbegin() const {
    const_cast<PosList*>(this)->_materialize_if_necessary();
    return Vector::cbegin();
  }

  const_iterator cend() const {
    const_cast<PosList*>(this)->_materialize_if_necessary();
    return Vector::cend();
  }

  const_reverse_iterator crbegin() const {
    const_cast<PosList*>(this)->_materialize_if_necessary();
    return Vector::crbegin();
  }

  const_reverse_iterator crend() const {
    const_cast<PosList*>(this)->_materialize_if_necessary();
    return Vector::crend();
  }

  reverse_iterator rbegin() {
    _materialize_if_necessary();
    return Vector::rbegin();
  }

  reverse_iterator rend() {
    _materialize_if_necessary();
    return Vector::rend();
  }

  const_reverse_iterator rbegin() const {
    return crbegin();
  }

  const_reverse_iterator rend() const {
    return crend();
  }

  // Capacity
  /* using Vector::capacity; */
  /* using Vector::max_size; */

  // Using these two should be save.
  using Vector::shrink_to_fit;
  using Vector::reserve;

  size_t size() const {
    if (matches_complete_chunk()) {
      return _matches_all_chunk->size();
    } else {
      return Vector::size();
    }
  }

  void clear() noexcept{
    _matches_all_chunk = nullptr;
    _matches_all_chunk_id = INVALID_CHUNK_ID;
    Vector::clear();
  }

  bool empty() const {
    return size() == 0;
  }

  // Modifiers
  /* using Vector::emplace; */
  /* using Vector::erase; */
  /* using Vector::insert; */
  /* using Vector::swap; */

  iterator insert (const_iterator position, const value_type& val) {
    _materialize_if_necessary();
    return Vector::insert(position, val);
  }

  iterator insert (const_iterator position, size_type n, const value_type& val) {
    _materialize_if_necessary();
    return Vector::insert(position, n, val);
  }

  template<typename _InputIterator>
  iterator insert (const_iterator position, _InputIterator first, _InputIterator last) {
    _materialize_if_necessary();
    return Vector::insert(position, first, last);
  }

  iterator insert (const_iterator position, value_type&& val) {
    _materialize_if_necessary();
    return Vector::insert(position, val);
  }

  iterator insert (const_iterator position, std::initializer_list<value_type> il) {
    _materialize_if_necessary();
    return Vector::insert(position, il);
  }

  template <class... Args>
  void emplace_back (Args&&... args) {
    _materialize_if_necessary();
    Vector::emplace_back(std::forward<Args>(args)...);
  }

  void pop_back() {
    _materialize_if_necessary();
    return Vector::pop_back();
  }

  void push_back (const value_type& val) {
    _materialize_if_necessary();
    return Vector::push_back(val);
  }

  void push_back (value_type&& val) {
    _materialize_if_necessary();
    return Vector::push_back(val);
  }

  void resize (size_type n) {
    _materialize_if_necessary();
    return Vector::resize(n);
  }

  void resize (size_type n, const value_type& val) {
    _materialize_if_necessary();
    return Vector::resize(n, val);
  }

  friend bool operator==(const PosList& lhs, const PosList& rhs);

  friend bool operator==(const PosList& lhs, const pmr_vector<RowID>& rhs);

  friend bool operator==(const pmr_vector<RowID>& lhs, const PosList& rhs);

 private:
  // TODO: This is needed in many noexcept methods - can we somehow work around the resize call
  // This method should only access vector methods, non of the overriden methods - they would probably call _materialize again
  // leading to endless recursion
  void _materialize() {
    DebugAssert(_matches_all_chunk != nullptr, "Called materialize on poslist that is already materialized");
    DebugAssert(_matches_all_chunk_id != INVALID_CHUNK_ID, "Called materialize on poslist that is already materialized");
    DebugAssert(Vector::size() == 0, "Unexpected precondition on PosList::_materialize");
    PerformanceWarning("Materializing PosList that had a _matches_all_chunk set");

    ChunkOffset chunk_size = _matches_all_chunk->size();
    Vector::resize(chunk_size);

    for(ChunkOffset offset = ChunkOffset{0}; offset < chunk_size; ++offset) {
      Vector::operator[](offset) = {_matches_all_chunk_id, offset};
    }

    _matches_all_chunk = nullptr;
    _matches_all_chunk_id = INVALID_CHUNK_ID;
  }

  void _materialize_if_necessary() {
    if (matches_complete_chunk()) {
      _materialize();
    }
  }

  std::shared_ptr<const Chunk> _matches_all_chunk = nullptr;
  ChunkID _matches_all_chunk_id = INVALID_CHUNK_ID;
  bool _references_single_chunk = false;
};

inline bool matches_all_equal_to_materialized(const PosList& matches_all_list, const pmr_vector<RowID>& materialized_list) {
  DebugAssert(matches_all_list.matches_complete_chunk(), "called with invalid first argument");

  if (matches_all_list.size() != materialized_list.size()) {
    return false;
  }

  // TODO: We maybe want to use an iterator here if we implement a non-materializing iterator on the PosList
  RowID expected_id{matches_all_list.common_chunk_id(), ChunkOffset{0}};

  for (const auto& materialized_row_id : materialized_list) {
    if (expected_id == materialized_row_id) {
      expected_id.chunk_offset++;
    } else {
      return false;
    }
  }

  return true;
}

inline bool operator==(const PosList& lhs, const PosList& rhs) {
  if (lhs.matches_complete_chunk() && rhs.matches_complete_chunk()) {
    return lhs._matches_all_chunk == rhs._matches_all_chunk;
  }
  if (lhs.matches_complete_chunk()) {
    return matches_all_equal_to_materialized(lhs, rhs);
  }
  if (rhs.matches_complete_chunk()) {
    return matches_all_equal_to_materialized(rhs, lhs);
  }

  return static_cast<const pmr_vector<RowID>&>(lhs) == static_cast<const pmr_vector<RowID>&>(rhs);
}

inline bool operator==(const PosList& lhs, const pmr_vector<RowID>& rhs) {
  if (lhs.matches_complete_chunk()) {
    return matches_all_equal_to_materialized(lhs, rhs);
  }

  return static_cast<const pmr_vector<RowID>&>(lhs) == rhs;
}

inline bool operator==(const pmr_vector<RowID>& lhs, const PosList& rhs) {
  if (rhs.matches_complete_chunk()) {
    return matches_all_equal_to_materialized(rhs, lhs);
  }

  return lhs == static_cast<const pmr_vector<RowID>&>(rhs);
}

}  // namespace opossum
