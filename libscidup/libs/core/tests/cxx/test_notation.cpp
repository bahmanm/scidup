#include "scidup/core/game.h"
#include "scidup/core/game_cursor.h"
#include "scidup/core/notation.h"

#include <gtest/gtest.h>

namespace {

scid::core::MoveSpec quiet(scid::core::squareT from,
                             scid::core::squareT to) {
	return {from, to, scid::core::EMPTY};
}

TEST(CoreNotationTest, WritesCurrentPositionUciFromMainlineCursor) {
	scid::core::Game game;
	game.appendMainlineMove(quiet(scid::core::D2, scid::core::D4));
	game.appendMainlineMove(quiet(scid::core::D7, scid::core::D5));
	scid::core::GameCursor cursor(game);
	ASSERT_TRUE(cursor.toPly(2));

	EXPECT_EQ("position startpos moves d2d4 d7d5",
	          scid::core::notation::currentPositionUci(game,
	                                                   cursor.location()));
}

TEST(CoreNotationTest, WritesPreviousAndNextMoveUciAtCursor) {
	scid::core::Game game;
	game.appendMainlineMove(quiet(scid::core::D2, scid::core::D4));
	game.appendMainlineMove(quiet(scid::core::D7, scid::core::D5));
	scid::core::GameCursor cursor(game);
	ASSERT_TRUE(cursor.toPly(1));

	EXPECT_EQ("d2d4",
	          scid::core::notation::previousMoveUci(game, cursor.location()));
	EXPECT_EQ("d7d5",
	          scid::core::notation::nextMoveUci(game, cursor.location()));
}

TEST(CoreNotationTest, WritesEmptyPreviousAndNextMoveUciAtBoundaries) {
	scid::core::Game game;
	game.appendMainlineMove(quiet(scid::core::D2, scid::core::D4));
	scid::core::GameCursor cursor(game);

	EXPECT_EQ("",
	          scid::core::notation::previousMoveUci(game, cursor.location()));
	EXPECT_EQ("d2d4",
	          scid::core::notation::nextMoveUci(game, cursor.location()));

	ASSERT_TRUE(cursor.next());
	EXPECT_EQ("d2d4",
	          scid::core::notation::previousMoveUci(game, cursor.location()));
	EXPECT_EQ("", scid::core::notation::nextMoveUci(game, cursor.location()));
}

TEST(CoreNotationTest, WritesNextAndPreviousSanAtCursor) {
	scid::core::Game game;
	game.appendMainlineMove(quiet(scid::core::D2, scid::core::D4));
	game.appendMainlineMove(quiet(scid::core::D7, scid::core::D5));
	scid::core::GameCursor cursor(game);
	ASSERT_TRUE(cursor.toPly(1));

	EXPECT_EQ("d4",
	          scid::core::notation::previousSan(game, cursor.location()));
	EXPECT_EQ("d5", scid::core::notation::nextSan(game, cursor.location()));
}

TEST(CoreNotationTest, WritesEmptyPreviousAndNextSanAtBoundaries) {
	scid::core::Game game;
	game.appendMainlineMove(quiet(scid::core::D2, scid::core::D4));
	scid::core::GameCursor cursor(game);

	EXPECT_EQ("", scid::core::notation::previousSan(game, cursor.location()));
	EXPECT_EQ("d4", scid::core::notation::nextSan(game, cursor.location()));

	ASSERT_TRUE(cursor.next());
	EXPECT_EQ("d4",
	          scid::core::notation::previousSan(game, cursor.location()));
	EXPECT_EQ("", scid::core::notation::nextSan(game, cursor.location()));
}

TEST(CoreNotationTest, PrefersStoredSanWhenPresent) {
	scid::core::Game game;
	auto& move = game.appendMainlineMove(
	    quiet(scid::core::D2, scid::core::D4));
	move.san = "SAN";
	scid::core::GameCursor cursor(game);

	EXPECT_EQ("SAN", scid::core::notation::nextSan(game, cursor.location()));
	ASSERT_TRUE(cursor.next());
	EXPECT_EQ("SAN",
	          scid::core::notation::previousSan(game, cursor.location()));
}

TEST(CoreNotationTest, ReturnsEmptySanForIllegalAction) {
	scid::core::Game game;
	game.appendMainlineMove(quiet(scid::core::A1, scid::core::A2));
	scid::core::GameCursor cursor(game);

	EXPECT_EQ("", scid::core::notation::nextSan(game, cursor.location()));
}

TEST(CoreNotationTest, WritesPartialMoveListFromMainlineStart) {
	scid::core::Game game;
	game.appendMainlineMove(quiet(scid::core::D2, scid::core::D4));
	game.appendMainlineMove(quiet(scid::core::D7, scid::core::D5));
	game.appendMainlineMove(quiet(scid::core::C2, scid::core::C4));

	EXPECT_EQ("1. d4 d5 2. c4",
	          scid::core::notation::partialMoveList(game, 3));
}

TEST(CoreNotationTest, WritesPartialMoveListWithInitialBlackMoveNumber) {
	scid::core::Game game;
	ASSERT_EQ(scid::core::OK,
	          game.setStartFen("8/8/8/8/2p5/1k1p4/p4N2/2K5 b - - 0 198"));
	game.appendMainlineMove(
	    {scid::core::A2, scid::core::A1, scid::core::QUEEN});

	EXPECT_EQ("198... a1=Q+",
	          scid::core::notation::partialMoveList(game, 1));
}

TEST(CoreNotationTest, WritesCurrentPositionUciFromVariationCursor) {
	scid::core::Game game;
	auto& first = game.appendMainlineMove(
	    quiet(scid::core::D2, scid::core::D4));
	first.childVariations.emplace_back().line.moves.push_back(
	    {quiet(scid::core::E2, scid::core::E4), "e4", {}, {}});
	first.childVariations[0].line.moves.push_back(
	    {quiet(scid::core::E7, scid::core::E5), "e5", {}, {}});

	scid::core::GameCursor cursor(game);
	ASSERT_TRUE(cursor.enterVariation(0));
	ASSERT_TRUE(cursor.next());
	ASSERT_TRUE(cursor.next());

	EXPECT_EQ("position startpos moves e2e4 e7e5",
	          scid::core::notation::currentPositionUci(game,
	                                                   cursor.location()));
}

TEST(CoreNotationTest, WritesCurrentPositionUciFromNonStandardStart) {
	scid::core::Game game;
	ASSERT_EQ(scid::core::OK,
	          game.setStartFen("8/8/8/8/2p5/1k1p4/p4N2/2K5 w - - 0 198"));
	game.appendMainlineMove(quiet(scid::core::C1, scid::core::D2));
	scid::core::GameCursor cursor(game);
	ASSERT_TRUE(cursor.next());

	EXPECT_EQ(
	    "position fen 8/8/8/8/2p5/1k1p4/p4N2/2K5 w - - 0 198 moves c1d2",
	    scid::core::notation::currentPositionUci(game, cursor.location()));
}

TEST(CoreNotationTest, ResetsCurrentPositionUciAtNullMove) {
	scid::core::Game game;
	game.appendMainlineMove(quiet(scid::core::E2, scid::core::E4));
	game.appendMainlineMove({});
	game.appendMainlineMove(quiet(scid::core::G1, scid::core::F3));
	scid::core::GameCursor cursor(game);
	ASSERT_TRUE(cursor.toPly(3));

	EXPECT_EQ("position fen "
	          "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR w KQkq - 1 2"
	          " moves g1f3",
	          scid::core::notation::currentPositionUci(game,
	                                                   cursor.location()));
}

} // namespace
