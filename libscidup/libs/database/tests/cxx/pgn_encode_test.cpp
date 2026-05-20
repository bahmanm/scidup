/*
 * Copyright (C) 2022  Fulvio Benini.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH
 * THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include "scidup/core/game.h"
#include "scidup/core/game_cursor.h"
#include "scidup/core/movetext_cursor.h"
#include "scidup/core/pgn/encode.h"
#include "scidup/database/pgnparse.h"
#include "pgnparse_impl.h"
#include <gtest/gtest.h>
#include <optional>
#include <string>
#include <string_view>

namespace {

std::optional<scid::database::Position>
currentPosition(const scid::core::Game& game,
                scid::core::MovetextLocation location) {
	scid::core::GameCursor cursor(game);
	EXPECT_TRUE(cursor.restore(location));
	auto position = cursor.currentPosition();
	EXPECT_TRUE(position.has_value());
	return position;
}

scid::database::simpleMoveT makeCurrentMove(scid::core::Game& game,
                                            scid::core::MovetextLocation location,
                                            scid::database::squareT from,
                                            scid::database::squareT to) {
	scid::database::simpleMoveT move;
	if (auto position = currentPosition(game, location))
		position->makeMove(from, to, scid::database::EMPTY, move);
	return move;
}

void setCurrentComment(scid::core::Game& game,
                       scid::core::MovetextLocation location,
                       std::string_view comment) {
	scid::core::MovetextCursor cursor(game);
	ASSERT_TRUE(cursor.restore(location));
	ASSERT_TRUE(cursor.setComment(comment));
}

void addMove(scid::core::Game& game, scid::core::MovetextLocation& location,
             scid::database::simpleMoveT const& move) {
	scid::core::MovetextCursor cursor(game);
	ASSERT_TRUE(cursor.restore(location));
	cursor.addMove({move.from, move.to, move.promote, move.isCastle() != 0});
	location = cursor.location();
}

} // namespace

TEST(Test_PgnEncode, break_lines) {
	using namespace std::literals;
	{
		auto pgn =
		    "1. e4\0e5\0{ very long comment, with space at the beginning should remain unaltered, even if it is longer than 80 chars}\0"sv
		    "2. Nf3\0Nf6"sv;

		auto expected =
		    "1. e4 e5\n{ very long comment, with space at the beginning should remain unaltered, even if it is longer than 80 chars}\n"sv
		    "2. Nf3 Nf6"sv;
		auto text = std::string(pgn);
		scid::core::pgn::break_lines(text.begin(), text.end());
		EXPECT_EQ(text, expected);

		auto expected_hard_len =
		    "1. e4 e5\n{ very long comment, with space at the beginning should remain unaltered, even\n"sv
		    "if it is longer than 80 chars} 2. Nf3 Nf6"sv;
		auto hard = std::string(pgn);
		scid::core::pgn::break_lines<80, '\0', 80>(hard.begin(), hard.end());
		EXPECT_EQ(hard, expected_hard_len);
	}
	{
		auto pgn =
		    "1. e4\0e5\0{normal comment, not very long, should be inline}\0"sv
		    "2. Nf3\0Nf6\0(2... Nc6)\0"sv
		    "3. Bb5\0Nxe4"sv;

		auto expected =
		    "1. e4 e5 {normal comment, not very long, should be inline} 2. Nf3 Nf6 (2... Nc6)\n3. Bb5 Nxe4"sv;
		auto text = std::string(pgn);
		scid::core::pgn::break_lines(text.begin(), text.end());
		EXPECT_EQ(text, expected);

		auto expected79 =
		    "1. e4 e5 {normal comment, not very long, should be inline} 2. Nf3 Nf6\n(2... Nc6) 3. Bb5 Nxe4"sv;
		text = std::string(pgn);
		scid::core::pgn::break_lines<79>(text.begin(), text.end());
		EXPECT_EQ(text, expected79);

		auto expected58 =
		    "1. e4 e5 {normal comment, not very long, should be inline}\n2. Nf3 Nf6 (2... Nc6) 3. Bb5 Nxe4"sv;
		text = std::string(pgn);
		scid::core::pgn::break_lines<58>(text.begin(), text.end());
		EXPECT_EQ(text, expected58);
	}
}

TEST(Test_PgnEncode, escape_string) {
	using namespace std::literals;
	{
		auto text = std::string(R"(escape \ test \\ "White "Sen\pai"")");
		auto expected = R"(escape \\ test \\\\ \"White \"Sen\\pai\"\")"sv;
		scid::core::pgn::escape_string(text, 0);
		EXPECT_EQ(text, expected);
	}
	{
		auto text = std::string(R"(escape \ test \\ "White "Sen\pai"")");
		auto expected = R"(escape \ test \\\\ \"White \"Sen\\pai\"\")"sv;
		scid::core::pgn::escape_string(text, 8);
		EXPECT_EQ(text, expected);
	}
}

TEST(Test_PgnEncode, encode_tag_pair) {
	using namespace std::literals;
	{
		std::string text;
		scid::core::pgn::encode_tag_pair("White", "Senpai e kohai", text);
		EXPECT_EQ("[White\0\"Senpai e kohai\"]\n"sv, text);
		scid::core::pgn::break_lines(text.begin(), text.end());
		EXPECT_STREQ("[White \"Senpai e kohai\"]\n", text.c_str());
	}
	{
		std::string text;
		scid::core::pgn::encode_tag_pair<true>("Event", "", text);
		EXPECT_EQ("[Event\0\"?\"]\n"sv, text);
		scid::core::pgn::break_lines(text.begin(), text.end());
		EXPECT_STREQ("[Event \"?\"]\n", text.c_str());
	}
	{
		std::string text;
		scid::core::pgn::encode_tag_pair("Event", "", text);
		EXPECT_EQ("[Event\0\"\"]\n"sv, text);
		scid::core::pgn::break_lines(text.begin(), text.end());
		EXPECT_STREQ("[Event \"\"]\n", text.c_str());
	}
	{
		std::string text;
		scid::core::pgn::encode_tag_pair("empty", "", text);
		EXPECT_EQ("[empty\0\"\"]\n"sv, text);
		scid::core::pgn::break_lines(text.begin(), text.end());
		EXPECT_STREQ("[empty \"\"]\n", text.c_str());
	}
}

TEST(Test_PgnEncode, encode_comment_rest_of_line) {
	{
		std::string text;
		EXPECT_TRUE(
		    scid::core::pgn::encode_comment_rest_of_line("rest of line comment", text));
		EXPECT_STREQ(text.c_str(), ";rest of line comment\n");
	}
	{
		std::string text = "1.e4";
		EXPECT_TRUE(
		    scid::core::pgn::encode_comment_rest_of_line("rest of line comment", text));
		EXPECT_STREQ(text.c_str(), "1.e4\0;rest of line comment\n");
	}
	{
		std::string text = "1.e4\0";
		EXPECT_TRUE(
		    scid::core::pgn::encode_comment_rest_of_line("rest of line comment", text));
		EXPECT_STREQ(text.c_str(), "1.e4\0;rest of line comment\n");
	}
	{
		std::string text;
		EXPECT_FALSE(scid::core::pgn::encode_comment_rest_of_line("no\nnewline", text));
		EXPECT_EQ(0, text.size());
	}
}

TEST(Test_PgnEncode, encode_comment) {
	using namespace std::literals;
	{
		std::string text;
		scid::core::pgn::encode_comment("normal comment", text);
		EXPECT_EQ(text, "{normal comment}\0"sv);
	}
	{
		std::string text;
		scid::core::pgn::encode_comment("comment with curly } brace", text);
		EXPECT_STREQ(text.c_str(), ";comment with curly } brace\n");
	}
	{
		std::string text;
		scid::core::pgn::encode_comment("comment with\nnewline", text);
		EXPECT_EQ(text, "{comment with\nnewline}\0"sv);
	}
	{
		std::string text;
		scid::core::pgn::encode_comment("both curly { and newline\n", text);
		EXPECT_EQ(text, "{both curly \xEF\xBD\x9B and newline\n}\0"sv);
	}
}

TEST(Test_PgnEncode, encode_game) {
	using namespace std::literals;
	{
		scid::core::Game empty;
		auto expected = "[Event\0\"\"]\n"sv
		                "[Site\0\"\"]\n"sv
		                "[Date\0\"????.??.??\"]\n"sv
		                "[Round\0\"\"]\n"sv
		                "[White\0\"\"]\n"sv
		                "[Black\0\"\"]\n"sv
		                "[Result\0\"*\"]\n"sv
		                "\n*\n"sv;
		std::string pgn;
		scid::core::pgn::encode_game(empty, pgn);
		EXPECT_EQ(pgn, expected);
	}
	{
		scid::core::Game game;
		scid::core::MovetextLocation location;
		game.setEco("A01");
		setCurrentComment(game, location, "before the move");
		auto sm = makeCurrentMove(game, location, scid::database::E2, scid::database::E4);
		addMove(game, location, sm);
		setCurrentComment(game, location, "after the move");
		auto expected = "[Event\0\"\"]\n"sv
		                "[Site\0\"\"]\n"sv
		                "[Date\0\"????.??.??\"]\n"sv
		                "[Round\0\"\"]\n"sv
		                "[White\0\"\"]\n"sv
		                "[Black\0\"\"]\n"sv
		                "[Result\0\"*\"]\n"sv
		                "[ECO\0\"A01\"]\n"sv
		                "\n"sv
		                "{before the move}\0"sv
		                "1.e4\0{after the move}\n"sv
		                "*\n"sv;
		std::string pgn;
		scid::core::pgn::encode_game(game, pgn);
		EXPECT_EQ(pgn, expected);
	}
}

TEST(Test_PgnEncode, encode) {
	{
		scid::core::Game empty;
		auto expected = "[Event \"\"]\n"
		                "[Site \"\"]\n"
		                "[Date \"????.??.??\"]\n"
		                "[Round \"\"]\n"
		                "[White \"\"]\n"
		                "[Black \"\"]\n"
		                "[Result \"*\"]\n"
		                "\n*\n";
		std::string pgn;
		scid::core::pgn::encode(empty, pgn);
		EXPECT_STREQ(pgn.c_str(), expected);
	}
	{
		scid::core::Game game;
		scid::core::MovetextLocation location;
		setCurrentComment(game, location, "before the move");
		auto sm = makeCurrentMove(game, location, scid::database::E2, scid::database::E4);
		addMove(game, location, sm);
		setCurrentComment(game, location, "after the move");
		auto expected = "[Event \"\"]\n"
		                "[Site \"\"]\n"
		                "[Date \"????.??.??\"]\n"
		                "[Round \"\"]\n"
		                "[White \"\"]\n"
		                "[Black \"\"]\n"
		                "[Result \"*\"]\n"
		                "\n"
		                "{before the move} 1.e4 {after the move}\n"
		                "*\n";
		std::string pgn;
		scid::core::pgn::encode(game, pgn);
		EXPECT_STREQ(pgn.c_str(), expected);
	}
	{
		std::string_view src =
		    "[ECO \"B01\"]\n"
		    "{pre} 1. e4 {comm} ({pre var} 1. d4 d5 {end var with comm}) 1... "
		    "e5 $1 {nag} (1... c5 $2) 2. Nf3 {last}";
		scid::core::Game game;
		scid::database::pgn::parse_game({src.data(), src.data() + src.size()},
		                scid::database::PgnVisitor{game});
		auto expected =
		    "[Event \"\"]\n"
		    "[Site \"\"]\n"
		    "[Date \"????.??.??\"]\n"
		    "[Round \"\"]\n"
		    "[White \"\"]\n"
		    "[Black \"\"]\n"
		    "[Result \"*\"]\n"
		    "[ECO \"B01\"]\n"
		    "\n"
		    "{pre} 1.e4 {comm} ({pre var} 1.d4 d5 {end var with comm}) 1...e5 "
		    "$1 {nag}\n(1...c5 $2) 2.Nf3 {last}\n"
		    "*\n";
		std::string pgn;
		scid::core::pgn::encode(game, pgn);
		EXPECT_STREQ(pgn.c_str(), expected);
	}
}
