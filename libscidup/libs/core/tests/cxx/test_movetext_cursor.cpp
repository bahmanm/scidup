#include "scidup/core/movetext_cursor.h"

#include <gtest/gtest.h>

namespace {

scid::core::MoveAction quiet(scid::database::squareT from,
                             scid::database::squareT to) {
	return {from, to, scid::database::EMPTY};
}

TEST(CoreMovetextCursorTest, AddsMainlineMovesAtCursorAndAdvances) {
	scid::core::Game game;
	scid::core::MovetextCursor cursor(game);

	auto& first = cursor.addMove(quiet(scid::database::E2, scid::database::E4));
	first.san = "e4";
	auto& second =
	    cursor.addMove(quiet(scid::database::E7, scid::database::E5));
	second.san = "e5";

	EXPECT_TRUE(cursor.isAtGameEnd());
	EXPECT_EQ(2U, cursor.ply());
	ASSERT_NE(nullptr, cursor.previousMove());
	EXPECT_EQ("e5", cursor.previousMove()->san);

	auto const& mainline = game.movetext().mainline.moves;
	ASSERT_EQ(2U, mainline.size());
	EXPECT_EQ("e2e4", mainline[0].action.longNotation());
	EXPECT_EQ("e7e5", mainline[1].action.longNotation());
}

TEST(CoreMovetextCursorTest, ReplacesContinuationWhenAddingBeforeLineEnd) {
	scid::core::Game game;
	scid::core::MovetextCursor cursor(game);
	cursor.addMove(quiet(scid::database::E2, scid::database::E4));
	cursor.addMove(quiet(scid::database::E7, scid::database::E5));
	ASSERT_TRUE(cursor.toPly(1));

	cursor.addMove(quiet(scid::database::C7, scid::database::C5));

	auto const& mainline = game.movetext().mainline.moves;
	ASSERT_EQ(2U, mainline.size());
	EXPECT_EQ("e2e4", mainline[0].action.longNotation());
	EXPECT_EQ("c7c5", mainline[1].action.longNotation());
	EXPECT_EQ(2U, cursor.ply());
	EXPECT_TRUE(cursor.isAtGameEnd());
}

TEST(CoreMovetextCursorTest, AddsVariationToNextMoveAndEntersIt) {
	scid::core::Game game;
	game.appendMainlineMove(quiet(scid::database::E2, scid::database::E4));
	game.appendMainlineMove(quiet(scid::database::E7, scid::database::E5));
	scid::core::MovetextCursor cursor(game);

	auto variation = cursor.addVariation("Queen pawn alternative");
	ASSERT_NE(nullptr, variation);
	ASSERT_EQ(variation, cursor.currentVariation());
	EXPECT_EQ(1U, cursor.variationDepth());
	EXPECT_TRUE(cursor.isAtVariationStart());
	EXPECT_TRUE(cursor.isAtEmptyVariation());

	auto& variationMove =
	    cursor.addMove(quiet(scid::database::D2, scid::database::D4));
	variationMove.san = "d4";

	auto const& first = game.movetext().mainline.moves[0];
	ASSERT_EQ(1U, first.childVariations.size());
	EXPECT_EQ("Queen pawn alternative",
	          first.childVariations[0].initialComment);
	ASSERT_EQ(1U, first.childVariations[0].line.moves.size());
	EXPECT_EQ("d4", first.childVariations[0].line.moves[0].san);
	EXPECT_TRUE(cursor.isAtVariationEnd());
	ASSERT_TRUE(cursor.exitVariation());
	EXPECT_EQ(nullptr, cursor.currentVariation());
	EXPECT_EQ(0U, cursor.variationDepth());
	ASSERT_NE(nullptr, cursor.nextMove());
	EXPECT_EQ("e2e4", cursor.nextMove()->action.longNotation());
}

TEST(CoreMovetextCursorTest, RefusesToAddVariationAtLineEnd) {
	scid::core::Game game;
	scid::core::MovetextCursor cursor(game);

	EXPECT_EQ(nullptr, cursor.addVariation());
	cursor.addMove(quiet(scid::database::E2, scid::database::E4));
	EXPECT_TRUE(cursor.isAtGameEnd());
	EXPECT_EQ(nullptr, cursor.addVariation());
}

TEST(CoreMovetextCursorTest, SavesAndRestoresVariationLocation) {
	scid::core::Game game;
	scid::core::MovetextCursor cursor(game);

	cursor.addMove(quiet(scid::database::E2, scid::database::E4));
	cursor.toStart();
	ASSERT_NE(nullptr, cursor.addVariation());
	cursor.addMove(quiet(scid::database::D2, scid::database::D4));
	auto variationLocation = cursor.location();

	ASSERT_TRUE(cursor.exitVariation());
	cursor.toEnd();
	ASSERT_TRUE(cursor.restore(variationLocation));

	EXPECT_EQ(1U, cursor.variationDepth());
	EXPECT_EQ(1U, cursor.ply());
	ASSERT_NE(nullptr, cursor.previousMove());
	EXPECT_EQ("d2d4", cursor.previousMove()->action.longNotation());
}

TEST(CoreMovetextCursorTest, TruncatesMainlineAtCursor) {
	scid::core::Game game;
	scid::core::MovetextCursor cursor(game);

	cursor.addMove(quiet(scid::database::E2, scid::database::E4));
	cursor.addMove(quiet(scid::database::E7, scid::database::E5));
	cursor.addMove(quiet(scid::database::G1, scid::database::F3));
	ASSERT_TRUE(cursor.toPly(1));

	cursor.truncate();

	ASSERT_EQ(1U, game.movetext().mainline.moves.size());
	EXPECT_TRUE(cursor.isAtLineEnd());
	EXPECT_EQ("e2e4",
	          game.movetext().mainline.moves[0].action.longNotation());
}

TEST(CoreMovetextCursorTest, TruncatesVariationLineAtCursor) {
	scid::core::Game game;
	scid::core::MovetextCursor cursor(game);

	cursor.addMove(quiet(scid::database::E2, scid::database::E4));
	cursor.toStart();
	auto variation = cursor.addVariation();
	ASSERT_NE(nullptr, variation);
	cursor.addMove(quiet(scid::database::D2, scid::database::D4));
	cursor.addMove(quiet(scid::database::D7, scid::database::D5));
	ASSERT_TRUE(cursor.previous());

	cursor.truncate();

	ASSERT_EQ(1U, variation->line.moves.size());
	EXPECT_TRUE(cursor.isAtLineEnd());
	EXPECT_EQ("d2d4", variation->line.moves[0].action.longNotation());
}

} // namespace
