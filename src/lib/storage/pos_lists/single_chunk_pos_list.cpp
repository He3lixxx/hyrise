#include "single_chunk_pos_list.hpp"

namespace opossum {

bool SingleChunkPosList::empty() const { return range_begin == range_end; }

size_t SingleChunkPosList::size() const { return std::distance(range_begin, range_end); }

size_t SingleChunkPosList::memory_usage(const MemoryUsageCalculationMode) const {
  return sizeof(*this);
}

bool SingleChunkPosList::references_single_chunk() const { return true; }

ChunkID SingleChunkPosList::common_chunk_id() const { return _chunk_id; }

AbstractPosList::PosListIterator<const SingleChunkPosList, RowID> SingleChunkPosList::begin() const {
  return AbstractPosList::PosListIterator<const SingleChunkPosList, RowID>(this, ChunkOffset{0});
}

AbstractPosList::PosListIterator<const SingleChunkPosList, RowID> SingleChunkPosList::end() const {
  return AbstractPosList::PosListIterator<const SingleChunkPosList, RowID>(this, static_cast<ChunkOffset>(size()));
}

AbstractPosList::PosListIterator<const SingleChunkPosList, RowID> SingleChunkPosList::cbegin() const { return begin(); }

AbstractPosList::PosListIterator<const SingleChunkPosList, RowID> SingleChunkPosList::cend() const { return end(); }

}  // namespace opossum
