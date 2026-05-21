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

TEST(Test_Nags, ParsePlainNagText) {
	EXPECT_EQ(scid::core::NAG_ExcellentMove, scid::core::parseNag("!!"));
	EXPECT_EQ(scid::core::NAG_Diagram, scid::core::parseNag("D"));
	EXPECT_EQ(scid::core::NAG_Comment, scid::core::parseNag("$145"));
	EXPECT_EQ(0, scid::core::parseNag("unknown"));
}

TEST(Test_Nags, FormatPlainNagText) {
	EXPECT_EQ("D", scid::core::formatNag(scid::core::NAG_Diagram, true));
	EXPECT_EQ("!", scid::core::formatNag(scid::core::NAG_GoodMove, true));
	EXPECT_EQ("$1", scid::core::formatNag(scid::core::NAG_GoodMove, false));
	EXPECT_EQ("$250", scid::core::formatNag(250, true));
}
