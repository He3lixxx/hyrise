#pragma once

#include <utility>
#include <vector>

#include "types.hpp"
#include "utils/assert.hpp"

namespace opossum {

class AbstractPosList;

template<typename Functor>
void resolve_pos_list_type(const AbstractPosList& untyped_pos_list, const Functor& func);

class AbstractPosList {
 public:
  virtual ~AbstractPosList();

  AbstractPosList& operator=(AbstractPosList&& other) = default;

  // Returns whether it is guaranteed that the PosList references a single ChunkID.
  // However, it may be false even if this is the case.
  virtual bool references_single_chunk() const = 0;

  // For chunks that share a common ChunkID, returns that ID.
  virtual ChunkID common_chunk_id() const = 0;

  virtual RowID operator[](size_t n) const = 0;

  // Capacity
  virtual bool empty() const = 0;
  virtual size_t size() const = 0;

  virtual size_t memory_usage(const MemoryUsageCalculationMode) const = 0;

  virtual bool operator==(const AbstractPosList& other) const = 0;

  template <typename Functor>
  void for_each(const Functor& functor) const {
    resolve_pos_list_type(*this, [&functor](auto& pos_list){
      auto it = make_pos_list_begin_iterator(pos_list);
      auto end = make_pos_list_end_iterator(pos_list);
      for(; it != end; ++it) {
        functor(*it);
      }
    });
  }
};

template <typename PosListType>
typename PosListType::const_iterator make_pos_list_begin_iterator(PosListType& pos_list);

template <typename PosListType>
typename PosListType::const_iterator make_pos_list_end_iterator(PosListType& pos_list);

template <typename PosListType>
typename PosListType::iterator make_pos_list_begin_iterator_nc(PosListType& pos_list);

template <typename PosListType>
typename PosListType::iterator make_pos_list_end_iterator_nc(PosListType& pos_list);


// For a long time, PosList was just a pmr_vector<RowID>. With this class, we want to add functionality to that vector,
// more specifically, flags that give us some guarantees about its contents. If we know, e.g., that all entries point
// into the same chunk, we can simplify things in split_pos_list_by_chunk_id.
// Inheriting from std::vector is generally not encouraged, because the STL containers are not prepared for
// inheritance. By making the inheritance private and this class final, we can assure that the problems that come with
// a non-virtual destructor do not occur.

class PosList : public AbstractPosList, private pmr_vector<RowID> {
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

  PosList& operator=(PosList&& other) = default;

  // If all entries in the PosList shares a single ChunkID, it makes sense to explicitly give this guarantee in order
  // to enable some optimizations.
  void guarantee_single_chunk() { _references_single_chunk = true; }

  // Returns whether the single ChunkID has been given (not necessarily, if it has been met)
  bool references_single_chunk() const override {
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
  ChunkID common_chunk_id() const override {
    DebugAssert(references_single_chunk(),
                "Can only retrieve the common_chunk_id if the PosList is guaranteed to reference a single chunk.");
    Assert(!empty(), "Cannot retrieve common_chunk_id of an empty chunk");
    return (*this)[0].chunk_id;
  }

  using Vector::assign;
  using Vector::get_allocator;

  // Element access
  RowID operator[](size_t n) const override {
    return Vector::operator[](n);
  }

  RowID& operator[](size_t n) {
    return Vector::operator[](n);
  }

  using Vector::back;
  using Vector::data;
  using Vector::front;

  // Iterators
  using Vector::begin;
  using Vector::end;
  using Vector::cbegin;
  using Vector::cend;

  // Capacity
  using Vector::capacity;
  using Vector::max_size;
  using Vector::reserve;
  using Vector::shrink_to_fit;
  size_t size() const override {
    return Vector::size();
  }
  bool empty() const override {
    return Vector::empty();
  }

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

  size_t memory_usage(const MemoryUsageCalculationMode mode) const override {
    // Ignoring MemoryUsageCalculationMode because accurate calculation is efficient.
    return size() * sizeof(Vector::value_type);
  }

  // TODO: Proper support for comparison
  bool operator==(const AbstractPosList& other) const override {
    return other == *this;
  }

  bool operator==(const PosList& other) const {
    return static_cast<const pmr_vector<RowID>&>(*this) == static_cast<const pmr_vector<RowID>&>(other);
  }

  // Kept for testing
  bool operator==(const pmr_vector<RowID>& other) const {
    return static_cast<const pmr_vector<RowID>&>(*this) == other;
  }


 private:
  bool _references_single_chunk = false;
};


// TODO: Comment, const magic like resolve_segment_type
template<typename Functor>
void resolve_pos_list_type(const AbstractPosList& untyped_pos_list, const Functor& func) {
  if (auto pos_list = dynamic_cast<const PosList*>(&untyped_pos_list)) {
    func(*pos_list);
  } else {
    Fail("Unrecoqgnized PosList type encountered");
  }
}

// TODO: Comment, const magic like resolve_segment_type
template<typename Functor>
void resolve_pos_list_type(const std::shared_ptr<const AbstractPosList>& untyped_pos_list, const Functor& func) {
  if (auto pos_list = std::dynamic_pointer_cast<const PosList>(untyped_pos_list)) {
    func(pos_list);
  } else {
    Fail("Unrecoqgnized PosList type encountered");
  }
}
}  // namespace opossum
