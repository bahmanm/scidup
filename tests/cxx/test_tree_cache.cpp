#include "scidup_app_tree_cache.h"

#include "scid/core/position.h"

#include <gtest/gtest.h>

TEST(AppTreeCacheTest, RestoresFilterForMatchingPositionOnly) {
  scidup::app::tree::TreeCache cache;
  cache.CacheResize(2);

  auto position = scid::core::Position::getStdStart();
  scid::database::Filter source(5);
  source.Fill(0);
  source.Set(1, 2);
  source.Set(3, 4);

  cache.cacheAdd(position, source);

  scid::database::Filter restored(5);
  restored.Fill(1);
  ASSERT_TRUE(cache.cacheRestore(position, restored));
  EXPECT_EQ(0, restored.Get(0));
  EXPECT_EQ(2, restored.Get(1));
  EXPECT_EQ(0, restored.Get(2));
  EXPECT_EQ(4, restored.Get(3));
  EXPECT_EQ(0, restored.Get(4));

  scid::core::Position other;
  ASSERT_EQ(scid::core::OK, other.ReadFromFEN("8/K7/8/8/7k/8/8/8 w - - 45 25"));
  EXPECT_FALSE(cache.cacheRestore(other, restored));
}
