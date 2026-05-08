#include "scidup/core/pgn/encode.h"
#include "scidup/core/pgn/movetext.h"

#include <gtest/gtest.h>
#include <array>
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

TEST(Test_PgnEncodeCore, MovetextEntryCarriesPgnTraversalData) {
	using namespace std::literals;

	std::array<scid::database::byte, 2> nags = {1, 3};
	scid::core::pgn::MovetextEntry entry{
	    scid::core::pgn::MovetextEntryKind::Move,
	    {},
	    "Nf3"sv,
	    "develops a knight"sv,
	    nags};

	EXPECT_EQ(scid::core::pgn::MovetextEntryKind::Move, entry.kind);
	EXPECT_EQ("Nf3"sv, entry.san);
	EXPECT_EQ("develops a knight"sv, entry.comment);
	ASSERT_EQ(2u, entry.nags.size());
	EXPECT_EQ(1, entry.nags[0]);
	EXPECT_EQ(3, entry.nags[1]);
}
