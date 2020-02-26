#pragma once

#include "./chunk.hpp"
#include "abstract_pos_list.hpp"

namespace opossum {

class MatchesAllPosList : public AbstractPosList {
 public:
  explicit MatchesAllPosList(std::shared_ptr<const Chunk> common_chunk, const ChunkID common_chunk_id)
      : _common_chunk(common_chunk), _common_chunk_id(common_chunk_id) {}

  MatchesAllPosList& operator=(MatchesAllPosList&& other) = default;

  MatchesAllPosList() = delete;

  virtual bool references_single_chunk() const override final { return true; }

  virtual ChunkID common_chunk_id() const override final {
    DebugAssert(_common_chunk_id != INVALID_CHUNK_ID, "common_chunk_id called on invalid chunk id");
    return _common_chunk_id;
  }

  virtual RowID operator[](size_t n) const override final {
    DebugAssert(_common_chunk_id != INVALID_CHUNK_ID, "operator[] called on invalid chunk id");
    return RowID{_common_chunk_id, static_cast<ChunkOffset>(n)};
  }

  virtual bool empty() const override final { return size() == 0; }

  virtual size_t size() const override final { return _common_chunk->size(); }

  virtual size_t memory_usage(const MemoryUsageCalculationMode) const override final { return sizeof *this; }

  virtual bool operator==(const MatchesAllPosList* other) const final { return _common_chunk == other->_common_chunk; }

  virtual bool operator==(const AbstractPosList* other) const override final {
    // TODO
    return false;
  }

  PosListIterator<const MatchesAllPosList*, RowID> begin() const {
    return PosListIterator<const MatchesAllPosList*, RowID>(this, ChunkOffset{0}, static_cast<ChunkOffset>(size()));
  }

  PosListIterator<const MatchesAllPosList*, RowID> end() const {
    return PosListIterator<const MatchesAllPosList*, RowID>(this, static_cast<ChunkOffset>(size()),
                                                            static_cast<ChunkOffset>(size()));
  }

  PosListIterator<const MatchesAllPosList*, RowID> cbegin() const { return begin(); }

  PosListIterator<const MatchesAllPosList*, RowID> cend() const { return end(); }

 private:
  std::shared_ptr<const Chunk> _common_chunk = nullptr;
  ChunkID _common_chunk_id = INVALID_CHUNK_ID;
};

}  // namespace opossum
