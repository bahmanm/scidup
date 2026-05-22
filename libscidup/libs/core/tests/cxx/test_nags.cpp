#include "scidup/core/nags.h"

#include <gtest/gtest.h>

TEST(Test_Nags, CommonAnnotationValues) {
	EXPECT_EQ(1, scid::core::nagCode(scid::core::Nag::GoodMove));
	EXPECT_EQ(2, scid::core::nagCode(scid::core::Nag::PoorMove));
	EXPECT_EQ(3, scid::core::nagCode(scid::core::Nag::ExcellentMove));
	EXPECT_EQ(4, scid::core::nagCode(scid::core::Nag::Blunder));
	EXPECT_EQ(201, scid::core::nagCode(scid::core::Nag::Diagram));
	EXPECT_EQ(215, scid::core::maxNagCode);
}

TEST(Test_Nags, ParsePlainNagText) {
	EXPECT_EQ(scid::core::Nag::ExcellentMove, scid::core::nagFromString("!!"));
	EXPECT_EQ(scid::core::Nag::Diagram, scid::core::nagFromString("D"));
	EXPECT_EQ(scid::core::Nag::Comment, scid::core::nagFromString("$145"));
	EXPECT_EQ(scid::core::Nag::None, scid::core::nagFromString("unknown"));
}

TEST(Test_Nags, FormatPlainNagText) {
	EXPECT_EQ("D", scid::core::nagToString(scid::core::Nag::Diagram, true));
	EXPECT_EQ("!", scid::core::nagToString(scid::core::Nag::GoodMove, true));
	EXPECT_EQ("$1", scid::core::nagToString(scid::core::Nag::GoodMove, false));
	EXPECT_EQ("$250", scid::core::nagToString(scid::core::nagFromCode(250), true));
}
