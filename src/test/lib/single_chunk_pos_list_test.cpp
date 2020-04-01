#include "base_test.hpp"
#include "gtest/gtest.h"
#include "operators/get_table.hpp"
#include "operators/maintenance/create_table.hpp"
#include "operators/table_wrapper.hpp"
#include "storage/pos_lists/row_id_pos_list.hpp"
#include "storage/pos_lists/single_chunk_pos_list.hpp"

namespace opossum {

class SingleChunkPosListTest : public BaseTest {
 public:
  void SetUp() override {}
};

TEST_F(SingleChunkPosListTest, EqualityToPosList) {
  // TODO(XPERIANER)
}
}  // namespace opossum
