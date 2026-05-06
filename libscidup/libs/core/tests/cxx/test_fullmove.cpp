#include "scidup/core/fullmove.h"

#include <gtest/gtest.h>

namespace {

using namespace scid::database;

TEST(FullMoveTest, EncodesDefaultNullAndEquality) {
	FullMove none;
	FullMove nullMove(0b01000001);

	EXPECT_FALSE(static_cast<bool>(none));
	EXPECT_EQ("--", none.getSAN());

	EXPECT_TRUE(static_cast<bool>(nullMove));
	EXPECT_TRUE(nullMove.isNull());
	EXPECT_EQ(nullMove, FullMove(0b01000001));
	EXPECT_FALSE(none == nullMove);
}

TEST(FullMoveTest, EncodesNormalMoveFields) {
	FullMove whitePawn(WHITE, E2, E4, PAWN);

	EXPECT_TRUE(static_cast<bool>(whitePawn));
	EXPECT_FALSE(whitePawn.isPromo());
	EXPECT_FALSE(whitePawn.isEnpassant());
	EXPECT_FALSE(whitePawn.isCastle());
	EXPECT_EQ(E2, whitePawn.getFrom());
	EXPECT_EQ(E4, whitePawn.getTo());
	EXPECT_EQ(PAWN, whitePawn.getPiece());
	EXPECT_EQ(WHITE, whitePawn.getColor());
	EXPECT_EQ(0, whitePawn.getCaptured());
	EXPECT_EQ("e4", whitePawn.getSAN());

	FullMove blackKnight(BLACK, G8, F6, KNIGHT);

	EXPECT_EQ(BLACK, blackKnight.getColor());
	EXPECT_EQ(KNIGHT, blackKnight.getPiece());
	EXPECT_EQ("Nf6", blackKnight.getSAN());
}

TEST(FullMoveTest, EncodesCapturesAndEnPassant) {
	FullMove capture(WHITE, E4, D5, PAWN);
	capture.setCapture(PAWN, false);

	EXPECT_EQ(PAWN, capture.getCaptured());
	EXPECT_FALSE(capture.isEnpassant());
	EXPECT_EQ("exd5", capture.getSAN());

	FullMove enPassant(BLACK, D4, E3, PAWN);
	enPassant.setCapture(PAWN, true);

	EXPECT_EQ(PAWN, enPassant.getCaptured());
	EXPECT_TRUE(enPassant.isEnpassant());
	EXPECT_EQ("dxe3", enPassant.getSAN());
}

TEST(FullMoveTest, EncodesPromotions) {
	FullMove promotion(WHITE, E7, E8, PAWN);
	promotion.setPromo(QUEEN);

	EXPECT_TRUE(promotion.isPromo());
	EXPECT_EQ(QUEEN, promotion.getPromo());
	EXPECT_EQ("e8=Q", promotion.getSAN());

	FullMove capturePromotion(BLACK, B2, A1, PAWN);
	capturePromotion.setCapture(ROOK, false);
	capturePromotion.setPromo(KNIGHT);

	EXPECT_TRUE(capturePromotion.isPromo());
	EXPECT_EQ(KNIGHT, capturePromotion.getPromo());
	EXPECT_EQ(ROOK, capturePromotion.getCaptured());
	EXPECT_EQ("bxa1=N", capturePromotion.getSAN());
}

TEST(FullMoveTest, EncodesDisambiguationCheckAndCastling) {
	FullMove knight(WHITE, G1, F3, KNIGHT);
	knight.setAmbiguity(true, false);
	knight.setCheck();

	EXPECT_EQ("Ngf3+", knight.getSAN());

	FullMove queen(BLACK, D8, H4, QUEEN);
	queen.setAmbiguity(false, true);

	EXPECT_EQ("Q8h4", queen.getSAN());

	FullMove kingsideCastle(WHITE, E1, H1);

	EXPECT_TRUE(kingsideCastle.isCastle());
	EXPECT_EQ(KING, kingsideCastle.getPiece());
	EXPECT_EQ("O-O", kingsideCastle.getSAN());

	FullMove queensideCastle(BLACK, E8, A8);
	queensideCastle.setCheck();

	EXPECT_TRUE(queensideCastle.isCastle());
	EXPECT_EQ("O-O-O+", queensideCastle.getSAN());
}

} // namespace
