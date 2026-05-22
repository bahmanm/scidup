#include "scidup/core/movetext_cursor.h"

#include <gtest/gtest.h>

namespace {

scid::core::MoveSpec quiet(scid::core::squareT from,
                             scid::core::squareT to) {
	return {from, to, scid::core::EMPTY};
}

TEST(CoreMovetextCursorTest, AddsMainlineMovesAtCursorAndAdvances) {
	scid::core::Game game;
	scid::core::MovetextCursor cursor(game);

	auto& first = cursor.addMove(quiet(scid::core::E2, scid::core::E4));
	first.san = "e4";
	auto& second =
	    cursor.addMove(quiet(scid::core::E7, scid::core::E5));
	second.san = "e5";

	EXPECT_TRUE(cursor.isAtGameEnd());
	EXPECT_EQ(2U, cursor.ply());
	ASSERT_NE(nullptr, cursor.previousMove());
	EXPECT_EQ("e5", cursor.previousMove()->san);

	auto const& mainline = game.movetext().mainline.moves;
	ASSERT_EQ(2U, mainline.size());
	EXPECT_EQ("e2e4", mainline[0].spec.longNotation());
	EXPECT_EQ("e7e5", mainline[1].spec.longNotation());
}

TEST(CoreMovetextCursorTest, ReplacesContinuationWhenAddingBeforeLineEnd) {
	scid::core::Game game;
	scid::core::MovetextCursor cursor(game);
	cursor.addMove(quiet(scid::core::E2, scid::core::E4));
	cursor.addMove(quiet(scid::core::E7, scid::core::E5));
	ASSERT_TRUE(cursor.toPly(1));

	cursor.addMove(quiet(scid::core::C7, scid::core::C5));

	auto const& mainline = game.movetext().mainline.moves;
	ASSERT_EQ(2U, mainline.size());
	EXPECT_EQ("e2e4", mainline[0].spec.longNotation());
	EXPECT_EQ("c7c5", mainline[1].spec.longNotation());
	EXPECT_EQ(2U, cursor.ply());
	EXPECT_TRUE(cursor.isAtGameEnd());
}

TEST(CoreMovetextCursorTest, AddsVariationToNextMoveAndEntersIt) {
	scid::core::Game game;
	game.appendMainlineMove(quiet(scid::core::E2, scid::core::E4));
	game.appendMainlineMove(quiet(scid::core::E7, scid::core::E5));
	scid::core::MovetextCursor cursor(game);

	auto variation = cursor.addVariation("Queen pawn alternative");
	ASSERT_NE(nullptr, variation);
	ASSERT_EQ(variation, cursor.currentVariation());
	EXPECT_EQ(1U, cursor.variationDepth());
	EXPECT_TRUE(cursor.isAtVariationStart());
	EXPECT_TRUE(cursor.isAtEmptyVariation());

	auto& variationMove =
	    cursor.addMove(quiet(scid::core::D2, scid::core::D4));
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
	EXPECT_EQ("e2e4", cursor.nextMove()->spec.longNotation());
}

TEST(CoreMovetextCursorTest, SetsPreviousMoveMetadataAtCursor) {
	scid::core::Game game;
	scid::core::MovetextCursor cursor(game);
	cursor.addMove(quiet(scid::core::E2, scid::core::E4));

	scid::core::MoveMetadata metadata;
	metadata.comment = "Best by test";
	metadata.nags = {scid::core::Nag::GoodMove, scid::core::Nag::WhiteSlight};

	ASSERT_TRUE(cursor.setPreviousMoveMetadata(std::move(metadata)));

	auto const& move = game.movetext().mainline.moves[0];
	EXPECT_EQ("Best by test", move.metadata.comment);
	ASSERT_EQ(2U, move.metadata.nags.size());
	EXPECT_EQ(scid::core::Nag::GoodMove, move.metadata.nags[0]);
	EXPECT_EQ(scid::core::Nag::WhiteSlight, move.metadata.nags[1]);
}

TEST(CoreMovetextCursorTest, RefusesToSetPreviousMoveMetadataAtLineStart) {
	scid::core::Game game;
	scid::core::MovetextCursor cursor(game);

	EXPECT_FALSE(cursor.setPreviousMoveMetadata({}));
}

TEST(CoreMovetextCursorTest, SetsPreviousAndNextMoveSanAtCursor) {
	scid::core::Game game;
	scid::core::MovetextCursor cursor(game);
	cursor.addMove(quiet(scid::core::E2, scid::core::E4));
	cursor.addMove(quiet(scid::core::E7, scid::core::E5));
	ASSERT_TRUE(cursor.previous());

	ASSERT_TRUE(cursor.setPreviousMoveSan("e4"));
	ASSERT_TRUE(cursor.setNextMoveSan("e5"));

	auto const& mainline = game.movetext().mainline.moves;
	ASSERT_EQ(2U, mainline.size());
	EXPECT_EQ("e4", mainline[0].san);
	EXPECT_EQ("e5", mainline[1].san);
}

TEST(CoreMovetextCursorTest, RefusesToSetSanWhenMoveIsMissing) {
	scid::core::Game game;
	scid::core::MovetextCursor cursor(game);

	EXPECT_FALSE(cursor.setPreviousMoveSan("none"));
	EXPECT_FALSE(cursor.setNextMoveSan("none"));
}

TEST(CoreMovetextCursorTest, SetsCurrentVariationInitialComment) {
	scid::core::Game game;
	scid::core::MovetextCursor cursor(game);
	cursor.addMove(quiet(scid::core::E2, scid::core::E4));
	cursor.toStart();
	ASSERT_NE(nullptr, cursor.addVariation("Old comment"));

	ASSERT_TRUE(cursor.setCurrentVariationInitialComment("New comment"));

	EXPECT_EQ("New comment",
	          game.movetext()
	              .mainline.moves[0]
	              .childVariations[0]
	              .initialComment);
}

TEST(CoreMovetextCursorTest, RefusesToSetVariationCommentFromMainline) {
	scid::core::Game game;
	scid::core::MovetextCursor cursor(game);

	EXPECT_FALSE(cursor.setCurrentVariationInitialComment("No variation"));
}

TEST(CoreMovetextCursorTest, RefusesToAddVariationAtLineEnd) {
	scid::core::Game game;
	scid::core::MovetextCursor cursor(game);

	EXPECT_EQ(nullptr, cursor.addVariation());
	cursor.addMove(quiet(scid::core::E2, scid::core::E4));
	EXPECT_TRUE(cursor.isAtGameEnd());
	EXPECT_EQ(nullptr, cursor.addVariation());
}

TEST(CoreMovetextCursorTest, SavesAndRestoresVariationLocation) {
	scid::core::Game game;
	scid::core::MovetextCursor cursor(game);

	cursor.addMove(quiet(scid::core::E2, scid::core::E4));
	cursor.toStart();
	ASSERT_NE(nullptr, cursor.addVariation());
	cursor.addMove(quiet(scid::core::D2, scid::core::D4));
	auto variationLocation = cursor.location();

	ASSERT_TRUE(cursor.exitVariation());
	cursor.toEnd();
	ASSERT_TRUE(cursor.restore(variationLocation));

	EXPECT_EQ(1U, cursor.variationDepth());
	EXPECT_EQ(1U, cursor.ply());
	ASSERT_NE(nullptr, cursor.previousMove());
	EXPECT_EQ("d2d4", cursor.previousMove()->spec.longNotation());
}

TEST(CoreMovetextCursorTest, TruncatesMainlineAtCursor) {
	scid::core::Game game;
	scid::core::MovetextCursor cursor(game);

	cursor.addMove(quiet(scid::core::E2, scid::core::E4));
	cursor.addMove(quiet(scid::core::E7, scid::core::E5));
	cursor.addMove(quiet(scid::core::G1, scid::core::F3));
	ASSERT_TRUE(cursor.toPly(1));

	cursor.truncate();

	ASSERT_EQ(1U, game.movetext().mainline.moves.size());
	EXPECT_TRUE(cursor.isAtLineEnd());
	EXPECT_EQ("e2e4",
	          game.movetext().mainline.moves[0].spec.longNotation());
}

TEST(CoreMovetextCursorTest, TruncatesVariationLineAtCursor) {
	scid::core::Game game;
	scid::core::MovetextCursor cursor(game);

	cursor.addMove(quiet(scid::core::E2, scid::core::E4));
	cursor.toStart();
	auto variation = cursor.addVariation();
	ASSERT_NE(nullptr, variation);
	cursor.addMove(quiet(scid::core::D2, scid::core::D4));
	cursor.addMove(quiet(scid::core::D7, scid::core::D5));
	ASSERT_TRUE(cursor.previous());

	cursor.truncate();

	ASSERT_EQ(1U, variation->line.moves.size());
	EXPECT_TRUE(cursor.isAtLineEnd());
	EXPECT_EQ("d2d4", variation->line.moves[0].spec.longNotation());
}

TEST(CoreMovetextCursorTest, DeletesCurrentVariationAndExitsToParent) {
	scid::core::Game game;
	scid::core::MovetextCursor cursor(game);

	cursor.addMove(quiet(scid::core::E2, scid::core::E4));
	cursor.toStart();
	auto firstVariation = cursor.addVariation("Queen pawn alternative");
	ASSERT_NE(nullptr, firstVariation);
	cursor.addMove(quiet(scid::core::D2, scid::core::D4));
	ASSERT_TRUE(cursor.exitVariation());
	auto secondVariation = cursor.addVariation("English alternative");
	ASSERT_NE(nullptr, secondVariation);
	cursor.addMove(quiet(scid::core::C2, scid::core::C4));

	ASSERT_TRUE(cursor.deleteVariation());

	EXPECT_EQ(0U, cursor.variationDepth());
	ASSERT_NE(nullptr, cursor.nextMove());
	EXPECT_EQ("e2e4", cursor.nextMove()->spec.longNotation());
	auto const& variations =
	    game.movetext().mainline.moves[0].childVariations;
	ASSERT_EQ(1U, variations.size());
	EXPECT_EQ("Queen pawn alternative", variations[0].initialComment);
	ASSERT_EQ(1U, variations[0].line.moves.size());
	EXPECT_EQ("d2d4", variations[0].line.moves[0].spec.longNotation());
}

TEST(CoreMovetextCursorTest, RefusesToDeleteVariationFromMainline) {
	scid::core::Game game;
	scid::core::MovetextCursor cursor(game);

	cursor.addMove(quiet(scid::core::E2, scid::core::E4));

	EXPECT_FALSE(cursor.deleteVariation());
	ASSERT_EQ(1U, game.movetext().mainline.moves.size());
	EXPECT_EQ("e2e4",
	          game.movetext().mainline.moves[0].spec.longNotation());
}

TEST(CoreMovetextCursorTest, PromotesCurrentVariationToFirst) {
	scid::core::Game game;
	scid::core::MovetextCursor cursor(game);

	cursor.addMove(quiet(scid::core::E2, scid::core::E4));
	cursor.toStart();
	auto queenPawn = cursor.addVariation("Queen pawn alternative");
	ASSERT_NE(nullptr, queenPawn);
	cursor.addMove(quiet(scid::core::D2, scid::core::D4));
	ASSERT_TRUE(cursor.exitVariation());
	auto english = cursor.addVariation("English alternative");
	ASSERT_NE(nullptr, english);
	cursor.addMove(quiet(scid::core::C2, scid::core::C4));

	ASSERT_TRUE(cursor.promoteVariationToFirst());

	EXPECT_EQ(1U, cursor.variationDepth());
	EXPECT_EQ(0U, cursor.variationIndex());
	ASSERT_NE(nullptr, cursor.previousMove());
	EXPECT_EQ("c2c4", cursor.previousMove()->spec.longNotation());
	auto const& variations =
	    game.movetext().mainline.moves[0].childVariations;
	ASSERT_EQ(2U, variations.size());
	EXPECT_EQ("English alternative", variations[0].initialComment);
	EXPECT_EQ("Queen pawn alternative", variations[1].initialComment);
}

TEST(CoreMovetextCursorTest, RefusesToPromoteVariationFromMainline) {
	scid::core::Game game;
	scid::core::MovetextCursor cursor(game);

	cursor.addMove(quiet(scid::core::E2, scid::core::E4));

	EXPECT_FALSE(cursor.promoteVariationToFirst());
	ASSERT_EQ(1U, game.movetext().mainline.moves.size());
	EXPECT_EQ("e2e4",
	          game.movetext().mainline.moves[0].spec.longNotation());
}

TEST(CoreMovetextCursorTest, PromotesCurrentVariationToMainline) {
	scid::core::Game game;
	scid::core::MovetextCursor cursor(game);

	cursor.addMove(quiet(scid::core::E2, scid::core::E4));
	cursor.addMove(quiet(scid::core::E7, scid::core::E5));
	cursor.toStart();
	auto queenPawn = cursor.addVariation("Queen pawn alternative");
	ASSERT_NE(nullptr, queenPawn);
	cursor.addMove(quiet(scid::core::D2, scid::core::D4));
	ASSERT_TRUE(cursor.exitVariation());
	auto english = cursor.addVariation("English alternative");
	ASSERT_NE(nullptr, english);
	cursor.addMove(quiet(scid::core::C2, scid::core::C4));
	cursor.addMove(quiet(scid::core::C7, scid::core::C5));

	ASSERT_TRUE(cursor.promoteVariationToMainline());

	EXPECT_EQ(0U, cursor.variationDepth());
	EXPECT_TRUE(cursor.isAtGameEnd());
	ASSERT_NE(nullptr, cursor.previousMove());
	EXPECT_EQ("c7c5", cursor.previousMove()->spec.longNotation());
	auto const& mainline = game.movetext().mainline.moves;
	ASSERT_EQ(2U, mainline.size());
	EXPECT_EQ("c2c4", mainline[0].spec.longNotation());
	EXPECT_EQ("c7c5", mainline[1].spec.longNotation());

	auto const& variations = mainline[0].childVariations;
	ASSERT_EQ(2U, variations.size());
	EXPECT_EQ("English alternative", variations[0].initialComment);
	ASSERT_EQ(2U, variations[0].line.moves.size());
	EXPECT_EQ("e2e4", variations[0].line.moves[0].spec.longNotation());
	EXPECT_EQ("e7e5", variations[0].line.moves[1].spec.longNotation());
	EXPECT_EQ("Queen pawn alternative", variations[1].initialComment);
	ASSERT_EQ(1U, variations[1].line.moves.size());
	EXPECT_EQ("d2d4", variations[1].line.moves[0].spec.longNotation());
}

TEST(CoreMovetextCursorTest, RefusesToPromoteEmptyVariationToMainlineAsNoOp) {
	scid::core::Game game;
	scid::core::MovetextCursor cursor(game);

	cursor.addMove(quiet(scid::core::E2, scid::core::E4));
	cursor.toStart();
	ASSERT_NE(nullptr, cursor.addVariation());

	EXPECT_TRUE(cursor.promoteVariationToMainline());
	EXPECT_EQ(1U, cursor.variationDepth());
	ASSERT_EQ(1U, game.movetext().mainline.moves.size());
	EXPECT_EQ("e2e4",
	          game.movetext().mainline.moves[0].spec.longNotation());
}

TEST(CoreMovetextCursorTest, TruncatesBeforeMainlineCursor) {
	scid::core::Game game;
	scid::core::MovetextCursor cursor(game);

	cursor.addMove(quiet(scid::core::E2, scid::core::E4));
	cursor.addMove(quiet(scid::core::E7, scid::core::E5));
	cursor.addMove(quiet(scid::core::G1, scid::core::F3));
	ASSERT_TRUE(cursor.toPly(1));

	cursor.truncateBeforeCursor();

	EXPECT_EQ(0U, cursor.variationDepth());
	EXPECT_TRUE(cursor.isAtGameStart());
	auto const& mainline = game.movetext().mainline.moves;
	ASSERT_EQ(2U, mainline.size());
	EXPECT_EQ("e7e5", mainline[0].spec.longNotation());
	EXPECT_EQ("g1f3", mainline[1].spec.longNotation());
}

TEST(CoreMovetextCursorTest, TruncatesBeforeVariationCursor) {
	scid::core::Game game;
	scid::core::MovetextCursor cursor(game);

	cursor.addMove(quiet(scid::core::E2, scid::core::E4));
	cursor.toStart();
	ASSERT_NE(nullptr, cursor.addVariation());
	cursor.addMove(quiet(scid::core::C2, scid::core::C4));
	cursor.addMove(quiet(scid::core::C7, scid::core::C5));
	ASSERT_TRUE(cursor.previous());

	cursor.truncateBeforeCursor();

	EXPECT_EQ(0U, cursor.variationDepth());
	EXPECT_TRUE(cursor.isAtGameStart());
	auto const& mainline = game.movetext().mainline.moves;
	ASSERT_EQ(1U, mainline.size());
	EXPECT_EQ("c7c5", mainline[0].spec.longNotation());
}

} // namespace
