#include "scidup/core/game_cursor.h"

#include <gtest/gtest.h>

namespace {

scid::core::MoveAction quiet(scid::database::squareT from,
                             scid::database::squareT to) {
	return {from, to, scid::database::EMPTY};
}

TEST(CoreGameCursorTest, StartsBeforeFirstMainlineMove) {
	scid::core::Game game;
	auto& first = game.appendMainlineMove(
	    quiet(scid::database::E2, scid::database::E4));
	first.san = "e4";
	game.appendMainlineMove(quiet(scid::database::E7, scid::database::E5));

	scid::core::GameCursor cursor(game);

	EXPECT_TRUE(cursor.isAtLineStart());
	EXPECT_TRUE(cursor.isAtVariationStart());
	EXPECT_TRUE(cursor.isAtGameStart());
	EXPECT_FALSE(cursor.isAtLineEnd());
	EXPECT_FALSE(cursor.isAtVariationEnd());
	EXPECT_FALSE(cursor.isAtGameEnd());
	EXPECT_FALSE(cursor.isAtEmptyVariation());
	EXPECT_EQ(0U, cursor.ply());
	EXPECT_EQ(0U, cursor.variationIndex());
	EXPECT_EQ(nullptr, cursor.previousMove());
	ASSERT_NE(nullptr, cursor.nextMove());
	EXPECT_EQ("e4", cursor.nextMove()->san);
}

TEST(CoreGameCursorTest, MovesNextAndPreviousThroughMainline) {
	scid::core::Game game;
	game.appendMainlineMove(quiet(scid::database::E2, scid::database::E4));
	game.appendMainlineMove(quiet(scid::database::E7, scid::database::E5));

	scid::core::GameCursor cursor(game);

	ASSERT_TRUE(cursor.next());
	EXPECT_FALSE(cursor.isAtLineStart());
	EXPECT_FALSE(cursor.isAtVariationStart());
	EXPECT_FALSE(cursor.isAtGameStart());
	EXPECT_FALSE(cursor.isAtLineEnd());
	EXPECT_FALSE(cursor.isAtVariationEnd());
	EXPECT_FALSE(cursor.isAtGameEnd());
	EXPECT_EQ(1U, cursor.ply());
	ASSERT_NE(nullptr, cursor.previousMove());
	EXPECT_EQ("e2e4", cursor.previousMove()->action.longNotation());
	ASSERT_NE(nullptr, cursor.nextMove());
	EXPECT_EQ("e7e5", cursor.nextMove()->action.longNotation());

	ASSERT_TRUE(cursor.next());
	EXPECT_TRUE(cursor.isAtLineEnd());
	EXPECT_TRUE(cursor.isAtVariationEnd());
	EXPECT_TRUE(cursor.isAtGameEnd());
	EXPECT_EQ(nullptr, cursor.nextMove());
	ASSERT_NE(nullptr, cursor.previousMove());
	EXPECT_EQ("e7e5", cursor.previousMove()->action.longNotation());
	EXPECT_FALSE(cursor.next());

	ASSERT_TRUE(cursor.previous());
	EXPECT_EQ(1U, cursor.ply());
	ASSERT_TRUE(cursor.previous());
	EXPECT_TRUE(cursor.isAtLineStart());
	EXPECT_FALSE(cursor.previous());
}

TEST(CoreGameCursorTest, SavesAndRestoresLocation) {
	scid::core::Game game;
	game.appendMainlineMove(quiet(scid::database::D2, scid::database::D4));
	game.appendMainlineMove(quiet(scid::database::D7, scid::database::D5));

	scid::core::GameCursor cursor(game);
	ASSERT_TRUE(cursor.next());
	auto location = cursor.location();
	cursor.toEnd();
	ASSERT_TRUE(cursor.isAtLineEnd());

	ASSERT_TRUE(cursor.restore(location));
	EXPECT_EQ(1U, cursor.ply());
	ASSERT_NE(nullptr, cursor.nextMove());
	EXPECT_EQ("d7d5", cursor.nextMove()->action.longNotation());
}

TEST(CoreGameCursorTest, ReturnsMainlineMovesToCursor) {
	scid::core::Game game;
	game.appendMainlineMove(quiet(scid::database::D2, scid::database::D4));
	game.appendMainlineMove(quiet(scid::database::D7, scid::database::D5));
	scid::core::GameCursor cursor(game);
	ASSERT_TRUE(cursor.toPly(2));

	auto moves = cursor.movesToCursor();

	ASSERT_EQ(2U, moves.size());
	EXPECT_EQ("d2d4", moves[0]->action.longNotation());
	EXPECT_EQ("d7d5", moves[1]->action.longNotation());
}

TEST(CoreGameCursorTest, ReturnsVariationMovesToCursor) {
	scid::core::Game game;
	auto& first = game.appendMainlineMove(
	    quiet(scid::database::D2, scid::database::D4));
	first.childVariations.emplace_back().line.moves.push_back(
	    {quiet(scid::database::E2, scid::database::E4), "e4", {}, {}});
	first.childVariations[0].line.moves.push_back(
	    {quiet(scid::database::E7, scid::database::E5), "e5", {}, {}});

	scid::core::GameCursor cursor(game);
	ASSERT_TRUE(cursor.enterVariation(0));
	ASSERT_TRUE(cursor.next());
	ASSERT_TRUE(cursor.next());

	auto moves = cursor.movesToCursor();

	ASSERT_EQ(2U, moves.size());
	EXPECT_EQ("e2e4", moves[0]->action.longNotation());
	EXPECT_EQ("e7e5", moves[1]->action.longNotation());
}

TEST(CoreGameCursorTest, ReturnsNestedVariationMovesToCursor) {
	scid::core::Game game;
	auto& first = game.appendMainlineMove(
	    quiet(scid::database::D2, scid::database::D4));
	auto& variation = first.childVariations.emplace_back();
	variation.line.appendMove(quiet(scid::database::E2, scid::database::E4));
	auto& variationSecond = variation.line.appendMove(
	    quiet(scid::database::E7, scid::database::E5));
	variationSecond.childVariations.emplace_back().line.moves.push_back(
	    {quiet(scid::database::C7, scid::database::C5), "c5", {}, {}});

	scid::core::GameCursor cursor(game);
	ASSERT_TRUE(cursor.enterVariation(0));
	ASSERT_TRUE(cursor.next());
	ASSERT_TRUE(cursor.enterVariation(0));
	ASSERT_TRUE(cursor.next());

	auto moves = cursor.movesToCursor();

	ASSERT_EQ(2U, moves.size());
	EXPECT_EQ("e2e4", moves[0]->action.longNotation());
	EXPECT_EQ("c7c5", moves[1]->action.longNotation());
}

TEST(CoreGameCursorTest, SeeksToMainlineStartAndEnd) {
	scid::core::Game game;
	auto& first = game.appendMainlineMove(
	    quiet(scid::database::E2, scid::database::E4));
	first.childVariations.emplace_back().line.moves.push_back(
	    {quiet(scid::database::D2, scid::database::D4), "d4", {}, {}});
	game.appendMainlineMove(quiet(scid::database::E7, scid::database::E5));

	scid::core::GameCursor cursor(game);
	ASSERT_TRUE(cursor.enterVariation(0));
	ASSERT_TRUE(cursor.next());
	EXPECT_EQ(1U, cursor.variationDepth());

	cursor.toStart();
	EXPECT_EQ(0U, cursor.variationDepth());
	EXPECT_TRUE(cursor.isAtGameStart());
	ASSERT_NE(nullptr, cursor.nextMove());
	EXPECT_EQ("e2e4", cursor.nextMove()->action.longNotation());

	ASSERT_TRUE(cursor.enterVariation(0));
	cursor.toEnd();
	EXPECT_EQ(0U, cursor.variationDepth());
	EXPECT_TRUE(cursor.isAtGameEnd());
	ASSERT_NE(nullptr, cursor.previousMove());
	EXPECT_EQ("e7e5", cursor.previousMove()->action.longNotation());
}

TEST(CoreGameCursorTest, SeeksToMainlinePly) {
	scid::core::Game game;
	game.appendMainlineMove(quiet(scid::database::E2, scid::database::E4));
	auto& second = game.appendMainlineMove(
	    quiet(scid::database::E7, scid::database::E5));
	second.childVariations.emplace_back().line.moves.push_back(
	    {quiet(scid::database::C7, scid::database::C5), "c5", {}, {}});
	game.appendMainlineMove(quiet(scid::database::G1, scid::database::F3));

	scid::core::GameCursor cursor(game);
	ASSERT_TRUE(cursor.toPly(2));
	EXPECT_EQ(2U, cursor.ply());
	ASSERT_NE(nullptr, cursor.previousMove());
	EXPECT_EQ("e7e5", cursor.previousMove()->action.longNotation());
	ASSERT_NE(nullptr, cursor.nextMove());
	EXPECT_EQ("g1f3", cursor.nextMove()->action.longNotation());
	EXPECT_FALSE(cursor.isAtGameEnd());

	ASSERT_TRUE(cursor.toPly(1));
	ASSERT_TRUE(cursor.enterVariation(0));
	EXPECT_EQ(1U, cursor.variationDepth());
	ASSERT_TRUE(cursor.next());
	EXPECT_EQ(2U, cursor.ply());
	ASSERT_TRUE(cursor.previous());
	ASSERT_TRUE(cursor.toPly(2));
	EXPECT_EQ(0U, cursor.variationDepth());
	EXPECT_EQ(2U, cursor.ply());
	ASSERT_NE(nullptr, cursor.previousMove());
	EXPECT_EQ("e7e5", cursor.previousMove()->action.longNotation());
	ASSERT_NE(nullptr, cursor.nextMove());
	EXPECT_EQ("g1f3", cursor.nextMove()->action.longNotation());

	EXPECT_FALSE(cursor.toPly(10));
	EXPECT_EQ(2U, cursor.ply());
	ASSERT_NE(nullptr, cursor.nextMove());
	EXPECT_EQ("g1f3", cursor.nextMove()->action.longNotation());
}

TEST(CoreGameCursorTest, EntersAndExitsVariationFromNextMove) {
	scid::core::Game game;
	auto& first = game.appendMainlineMove(
	    quiet(scid::database::E2, scid::database::E4));
	first.childVariations.emplace_back("Queen pawn alternative").line.moves.push_back(
	    {quiet(scid::database::D2, scid::database::D4), "d4", {}, {}});
	game.appendMainlineMove(quiet(scid::database::E7, scid::database::E5));

	scid::core::GameCursor cursor(game);

	EXPECT_EQ(1U, cursor.variationCount());
	EXPECT_EQ(0U, cursor.variationDepth());
	EXPECT_EQ(0U, cursor.variationIndex());
	EXPECT_EQ(nullptr, cursor.currentVariation());
	ASSERT_TRUE(cursor.enterVariation(0));
	ASSERT_NE(nullptr, cursor.currentVariation());
	EXPECT_EQ("Queen pawn alternative",
	          cursor.currentVariation()->initialComment);
	EXPECT_EQ(1U, cursor.variationDepth());
	EXPECT_EQ(0U, cursor.variationIndex());
	EXPECT_TRUE(cursor.isAtLineStart());
	EXPECT_TRUE(cursor.isAtVariationStart());
	EXPECT_FALSE(cursor.isAtGameStart());
	EXPECT_FALSE(cursor.isAtGameEnd());
	EXPECT_FALSE(cursor.isAtEmptyVariation());
	ASSERT_NE(nullptr, cursor.nextMove());
	EXPECT_EQ("d4", cursor.nextMove()->san);
	EXPECT_EQ(0U, cursor.variationCount());

	ASSERT_TRUE(cursor.next());
	EXPECT_TRUE(cursor.isAtLineEnd());
	EXPECT_TRUE(cursor.isAtVariationEnd());
	EXPECT_FALSE(cursor.isAtGameEnd());
	ASSERT_TRUE(cursor.exitVariation());
	EXPECT_EQ(0U, cursor.variationDepth());
	ASSERT_NE(nullptr, cursor.nextMove());
	EXPECT_EQ("e2e4", cursor.nextMove()->action.longNotation());
}

TEST(CoreGameCursorTest, SavesAndRestoresVariationLocation) {
	scid::core::Game game;
	game.appendMainlineMove(quiet(scid::database::E2, scid::database::E4));
	auto& second = game.appendMainlineMove(
	    quiet(scid::database::E7, scid::database::E5));
	auto& variationMoves = second.childVariations.emplace_back().line.moves;
	variationMoves.push_back(
	    {quiet(scid::database::C2, scid::database::C4), "c4", {}, {}});
	variationMoves.push_back(
	    {quiet(scid::database::E7, scid::database::E5), "e5", {}, {}});

	scid::core::GameCursor cursor(game);
	ASSERT_TRUE(cursor.next());
	ASSERT_TRUE(cursor.enterVariation(0));
	ASSERT_TRUE(cursor.next());
	auto location = cursor.location();
	ASSERT_TRUE(cursor.exitVariation());
	ASSERT_TRUE(cursor.next());

	ASSERT_TRUE(cursor.restore(location));
	EXPECT_EQ(1U, cursor.variationDepth());
	EXPECT_EQ(0U, cursor.variationIndex());
	EXPECT_EQ(2U, cursor.ply());
	ASSERT_NE(nullptr, cursor.previousMove());
	EXPECT_EQ("c4", cursor.previousMove()->san);
	ASSERT_NE(nullptr, cursor.nextMove());
	EXPECT_EQ("e5", cursor.nextMove()->san);
}

TEST(CoreGameCursorTest, ReportsVariationIndexAndEmptyVariationState) {
	scid::core::Game game;
	auto& first = game.appendMainlineMove(
	    quiet(scid::database::E2, scid::database::E4));
	first.childVariations.emplace_back();
	first.childVariations.emplace_back().line.moves.push_back(
	    {quiet(scid::database::G1, scid::database::F3), "Nf3", {}, {}});

	scid::core::GameCursor cursor(game);
	ASSERT_TRUE(cursor.enterVariation(0));
	EXPECT_EQ(1U, cursor.variationDepth());
	EXPECT_EQ(0U, cursor.variationIndex());
	EXPECT_TRUE(cursor.isAtVariationStart());
	EXPECT_TRUE(cursor.isAtVariationEnd());
	EXPECT_TRUE(cursor.isAtEmptyVariation());
	EXPECT_FALSE(cursor.isAtGameStart());
	EXPECT_FALSE(cursor.isAtGameEnd());

	ASSERT_TRUE(cursor.exitVariation());
	ASSERT_TRUE(cursor.enterVariation(1));
	EXPECT_EQ(1U, cursor.variationIndex());
	EXPECT_TRUE(cursor.isAtVariationStart());
	EXPECT_FALSE(cursor.isAtVariationEnd());
	EXPECT_FALSE(cursor.isAtEmptyVariation());
	ASSERT_NE(nullptr, cursor.nextMove());
	EXPECT_EQ("Nf3", cursor.nextMove()->san);
}

TEST(CoreGameCursorTest, HandlesEmptyGame) {
	scid::core::Game game;
	scid::core::GameCursor cursor(game);

	EXPECT_TRUE(cursor.isAtLineStart());
	EXPECT_TRUE(cursor.isAtLineEnd());
	EXPECT_TRUE(cursor.isAtVariationStart());
	EXPECT_TRUE(cursor.isAtVariationEnd());
	EXPECT_TRUE(cursor.isAtGameStart());
	EXPECT_TRUE(cursor.isAtGameEnd());
	EXPECT_FALSE(cursor.isAtEmptyVariation());
	EXPECT_EQ(nullptr, cursor.previousMove());
	EXPECT_EQ(nullptr, cursor.nextMove());
	EXPECT_FALSE(cursor.next());
	EXPECT_FALSE(cursor.previous());
	EXPECT_EQ(0U, cursor.variationCount());
	EXPECT_EQ(0U, cursor.variationDepth());
	EXPECT_EQ(0U, cursor.variationIndex());
	EXPECT_FALSE(cursor.enterVariation(0));
	EXPECT_FALSE(cursor.exitVariation());
}

} // namespace
