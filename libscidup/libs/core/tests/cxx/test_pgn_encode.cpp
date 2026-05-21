#include "scidup/core/pgn/encode.h"
#include "scidup/core/game.h"
#include "scidup/core/nags.h"

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

TEST(Test_PgnEncodeCore, EncodeCoreGame) {
	using namespace std::literals;

	scid::core::Game game;
	game.setEvent("Candidates");
	game.setSite("Toronto");
	game.setRound("7");
	game.setWhiteName("Player A");
	game.setBlackName("Player B");
	game.setWhiteRating({2800, scid::database::RATING_Rapid});
	game.setResult(scid::database::RESULT_White);
	game.setEco("A01");
	game.setInitialComment("Before the first move");

	auto& first = game.appendMainlineMove(
	    {scid::database::D2, scid::database::D4, scid::database::EMPTY});
	first.metadata.nags.push_back(scid::core::NAG_GoodMove);
	first.metadata.comment = "Best by test";
	auto& childVariation = first.childVariations.emplace_back();
	childVariation.initialComment = "Queen pawn alternative";
	childVariation.line.appendMove(
	    {scid::database::E2, scid::database::E4, scid::database::EMPTY});
	childVariation.line.appendMove(
	    {scid::database::E7, scid::database::E5, scid::database::EMPTY});
	game.appendMainlineMove(
	    {scid::database::D7, scid::database::D5, scid::database::EMPTY});
	game.appendMainlineMove(
	    {scid::database::C2, scid::database::C4, scid::database::EMPTY});

	std::string pgn;
	scid::core::pgn::encode_game(game, pgn);

	auto expected = "[Event\0\"Candidates\"]\n"sv
	                "[Site\0\"Toronto\"]\n"sv
	                "[Date\0\"????.??.??\"]\n"sv
	                "[Round\0\"7\"]\n"sv
	                "[White\0\"Player A\"]\n"sv
	                "[Black\0\"Player B\"]\n"sv
	                "[Result\0\"1-0\"]\n"sv
	                "[WhiteRapid\0\"2800\"]\n"sv
	                "[ECO\0\"A01\"]\n"sv
	                "\n"sv
	                "{Before the first move}\0"sv
	                "1.d4\0$1\0{Best by test}\0"sv
	                "({Queen pawn alternative}\0"sv
	                "1.e4\0e5)\0"sv
	                "1...d5\0"sv
	                "2.c4\n"sv
	                "1-0\n"sv;
	EXPECT_EQ(expected, pgn);
}

TEST(Test_PgnEncodeCore, EncodeCoreGameWithLineBreaking) {
	using namespace std::literals;

	scid::core::Game game;
	game.setEvent("Friendly");
	game.appendMainlineMove(
	    {scid::database::E2, scid::database::E4, scid::database::EMPTY});
	game.appendMainlineMove(
	    {scid::database::E7, scid::database::E5, scid::database::EMPTY});

	std::string pgn;
	scid::core::pgn::encode(game, pgn);

	auto expected = "[Event \"Friendly\"]\n"sv
	                "[Site \"\"]\n"sv
	                "[Date \"????.??.??\"]\n"sv
	                "[Round \"\"]\n"sv
	                "[White \"\"]\n"sv
	                "[Black \"\"]\n"sv
	                "[Result \"*\"]\n"sv
	                "\n"sv
	                "1.e4 e5\n"sv
	                "*\n"sv;
	EXPECT_EQ(expected, pgn);
}

TEST(Test_PgnEncodeCore, EncodeCoreGameWithSymbolicNags) {
	using namespace std::literals;

	scid::core::Game game;
	auto& first = game.appendMainlineMove(
	    {scid::database::E2, scid::database::E4, scid::database::EMPTY});
	first.metadata.nags.push_back(scid::core::NAG_GoodMove);
	first.metadata.nags.push_back(scid::core::NAG_Diagram);

	std::string pgn;
	scid::core::pgn::encode_game(
	    game, pgn, scid::core::pgn::EncodeOptions{.symbolicNags = true});

	auto expected = "[Event\0\"\"]\n"sv
	                "[Site\0\"\"]\n"sv
	                "[Date\0\"????.??.??\"]\n"sv
	                "[Round\0\"\"]\n"sv
	                "[White\0\"\"]\n"sv
	                "[Black\0\"\"]\n"sv
	                "[Result\0\"*\"]\n"sv
	                "\n"sv
	                "1.e4\0!\0D\n"sv
	                "*\n"sv;
	EXPECT_EQ(expected, pgn);
}
