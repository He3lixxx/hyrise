#pragma once

#include "abstract_pos_list.hpp"
#include "storage/index/abstract_index.hpp"

namespace opossum {

class SingleChunkPosList final : public AbstractPosList {
  public:

  SingleChunkPosList() = delete;

  SingleChunkPosList(ChunkID chunkID) : _chunk_id(chunkID) { 
    DebugAssert(chunkID != INVALID_CHUNK_ID, "SingleChunkPosList constructed with INVALID_CHUNK_ID");
  }

  virtual bool empty() const override final;
  virtual size_t size() const override final;
  virtual size_t memory_usage(const MemoryUsageCalculationMode) const override final;

  virtual bool references_single_chunk() const override final;
  virtual ChunkID common_chunk_id() const override final;

  virtual RowID operator[](size_t n) const override final {
    return RowID{_chunk_id, *(std::next(range_begin, n))};
  }

  PosListIterator<const SingleChunkPosList, RowID> begin() const;
  PosListIterator<const SingleChunkPosList, RowID> end() const;
  PosListIterator<const SingleChunkPosList, RowID> cbegin() const;
  PosListIterator<const SingleChunkPosList, RowID> cend() const;

  AbstractIndex::Iterator range_begin = AbstractIndex::Iterator{};
  AbstractIndex::Iterator range_end = AbstractIndex::Iterator{};

  private:
    ChunkID _chunk_id = INVALID_CHUNK_ID;
};

}
