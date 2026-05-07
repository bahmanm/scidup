#include "scidup/core/nags.h"

#include <gtest/gtest.h>

TEST(Test_Nags, CommonAnnotationValues) {
	EXPECT_EQ(1, scid::core::NAG_GoodMove);
	EXPECT_EQ(2, scid::core::NAG_PoorMove);
	EXPECT_EQ(3, scid::core::NAG_ExcellentMove);
	EXPECT_EQ(4, scid::core::NAG_Blunder);
	EXPECT_EQ(201, scid::core::NAG_Diagram);
	EXPECT_EQ(215, scid::core::MAX_NAGS_ARRAY);
}
