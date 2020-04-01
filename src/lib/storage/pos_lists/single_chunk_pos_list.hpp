#pragma once

#include "abstract_pos_list.hpp"
#include "storage/index/abstract_index.hpp"

namespace opossum {

class SingleChunkPosList final : public AbstractPosList {
  public:

  SingleChunkPosList() = delete;

  SingleChunkPosList(ChunkID chunkID) : _chunk_id(chunkID) {}

  virtual bool empty() const override final {
    return range_begin == range_end;
  }
  virtual size_t size() const override final {
    return std::distance(range_begin, range_end);
  }
  
  virtual size_t memory_usage(const MemoryUsageCalculationMode) const override final {
    return sizeof(this);
  }

  virtual bool references_single_chunk() const override final {
    return true;
  }

  virtual ChunkID common_chunk_id() const override final {
    return _chunk_id;
  }

  virtual RowID operator[](size_t n) const override final {
    return RowID{_chunk_id, *(std::next(range_begin, n))};
  }

  PosListIterator<const SingleChunkPosList, RowID> begin() const {
    return PosListIterator<const SingleChunkPosList, RowID>(this, ChunkOffset{0});
  }

  PosListIterator<const SingleChunkPosList, RowID> end() const {
    return PosListIterator<const SingleChunkPosList, RowID>(this, static_cast<ChunkOffset>(size()));
  }

  PosListIterator<const SingleChunkPosList, RowID> cbegin() const {
    return begin();
  }

  PosListIterator<const SingleChunkPosList, RowID> cend() const {
    return end();
  }

  AbstractIndex::Iterator range_begin = AbstractIndex::Iterator{};
  AbstractIndex::Iterator range_end = AbstractIndex::Iterator{};

  private:
    ChunkID _chunk_id = INVALID_CHUNK_ID;
};

}
