#include "scidup/core/pgn/encode.h"

#include <gtest/gtest.h>
#include <string>
#include <string_view>

TEST(Test_PgnEncodeCore, BreakLines) {
	using namespace std::literals;

	auto text = std::string("1. e4\0e5\0{inline comment}\0"
	                        "2. Nf3\0Nf6"sv);
	scid::core::pgn::break_lines(text.begin(), text.end());

	EXPECT_EQ("1. e4 e5 {inline comment} 2. Nf3 Nf6"sv, text);
}

TEST(Test_PgnEncodeCore, EncodeTagPairEscapesValue) {
	using namespace std::literals;

	std::string text;
	scid::core::pgn::encode_tag_pair("White", R"(Sen\pai "A")", text);

	EXPECT_EQ("[White\0\"Sen\\\\pai \\\"A\\\"\"]\n"sv, text);
}

TEST(Test_PgnEncodeCore, EncodeComment) {
	using namespace std::literals;

	std::string text;
	scid::core::pgn::encode_comment("normal comment", text);

	EXPECT_EQ("{normal comment}\0"sv, text);
}
