#include "scidup/eco/code.h"

#include <gtest/gtest.h>
#include <string>

namespace {

std::string toExtended(scidup::eco::Code code) {
	scidup::eco::String str;
	scidup::eco::toExtendedString(code, str);
	return str;
}

std::string toBasic(scidup::eco::Code code) {
	scidup::eco::String str;
	scidup::eco::toBasicString(code, str);
	return str;
}

} // namespace

TEST(EcoCodeTest, ParsesCanonicalAndExtendedCodes) {
	EXPECT_EQ(scidup::eco::ECO_None, scidup::eco::fromString(""));
	EXPECT_EQ(scidup::eco::ECO_None, scidup::eco::fromString("Z99"));
	EXPECT_EQ(scidup::eco::fromString("A00"), scidup::eco::fromString("a00"));
	EXPECT_EQ("A00", toExtended(scidup::eco::fromString("A00")));
	EXPECT_EQ("A00a", toExtended(scidup::eco::fromString("A00a")));
	EXPECT_EQ("A00a4", toExtended(scidup::eco::fromString("A00a4")));
	EXPECT_EQ("E99z4", toExtended(scidup::eco::fromString("E99z4")));
}

TEST(EcoCodeTest, ConvertsBasicAndExtendedStrings) {
	auto code = scidup::eco::fromString("B91a4");

	EXPECT_EQ("B91", toBasic(code));
	EXPECT_EQ("B91a4", toExtended(code));
}

TEST(EcoCodeTest, ComputesBasicAndLastSubCodes) {
	auto code = scidup::eco::fromString("B91a");

	EXPECT_EQ("B91", toExtended(scidup::eco::basicCode(code)));
	EXPECT_EQ("B91a4", toExtended(scidup::eco::lastSubCode(code)));
	EXPECT_EQ("B91z4", toExtended(scidup::eco::lastSubCode(scidup::eco::fromString("B91"))));
	EXPECT_EQ(scidup::eco::ECO_None, scidup::eco::lastSubCode(scidup::eco::ECO_None));
}

TEST(EcoCodeTest, ReducesScidExtendedSubcodes) {
	EXPECT_EQ(0, scidup::eco::reduce(scidup::eco::fromString("A00")));
	EXPECT_EQ(1, scidup::eco::reduce(scidup::eco::fromString("A00a")));
	EXPECT_EQ(1, scidup::eco::reduce(scidup::eco::fromString("A00a4")));
	EXPECT_EQ(2, scidup::eco::reduce(scidup::eco::fromString("A00b")));
	EXPECT_EQ(2700, scidup::eco::reduce(scidup::eco::fromString("B00")));
}
