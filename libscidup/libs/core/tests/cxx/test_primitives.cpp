#include "scidup/core/primitives.h"

#include <gtest/gtest.h>

namespace {

using namespace scid::core;

TEST(PrimitivesTest, DefinesScalarAndDirectionTypes) {
	EXPECT_EQ(1U, sizeof(byte));
	EXPECT_EQ(2U, sizeof(ushort));
	EXPECT_EQ(4U, sizeof(uint));
	EXPECT_EQ(4U, sizeof(sint));

	EXPECT_EQ(1U, sizeof(directionT));
	EXPECT_EQ(1U, sizeof(leftDiagT));
	EXPECT_EQ(1U, sizeof(rightDiagT));
	EXPECT_EQ(1U, sizeof(castleDirT));

	EXPECT_EQ(0, QSIDE);
	EXPECT_EQ(1, KSIDE);
}

TEST(PrimitivesTest, EncodesPieceColourAndType) {
	EXPECT_EQ(WHITE, piece_Color(WK));
	EXPECT_EQ(BLACK, piece_Color(BK));
	EXPECT_EQ(NOCOLOR, piece_Color(EMPTY));

	EXPECT_EQ(KING, piece_Type(WK));
	EXPECT_EQ(KING, piece_Type(BK));
	EXPECT_EQ(PAWN, piece_Type(WP));
	EXPECT_EQ(PAWN, piece_Type(BP));

	EXPECT_EQ(WQ, piece_Make(WHITE, QUEEN));
	EXPECT_EQ(BQ, piece_Make(BLACK, QUEEN));
	EXPECT_EQ(BP, piece_Make(BLACK, PAWN));
}

TEST(PrimitivesTest, MapsPieceCharacters) {
	EXPECT_EQ(WK, pieceFromByte[static_cast<unsigned char>('K')]);
	EXPECT_EQ(BK, pieceFromByte[static_cast<unsigned char>('k')]);
	EXPECT_EQ(WQ, pieceFromByte[static_cast<unsigned char>('Q')]);
	EXPECT_EQ(BQ, pieceFromByte[static_cast<unsigned char>('q')]);
	EXPECT_EQ(EMPTY, pieceFromByte[static_cast<unsigned char>('x')]);

	EXPECT_EQ('K', piece_Char(WK));
	EXPECT_EQ('Q', piece_Char(WQ));
	EXPECT_EQ('P', piece_Char(WP));
}

TEST(PrimitivesTest, EncodesSquaresRanksAndFyles) {
	EXPECT_EQ(0, A1);
	EXPECT_EQ(7, H1);
	EXPECT_EQ(56, A8);
	EXPECT_EQ(63, H8);
	EXPECT_EQ(65, NULL_SQUARE);
	EXPECT_EQ(NULL_SQUARE, NS);

	EXPECT_EQ(E4, square_Make(E_FYLE, RANK_4));
	EXPECT_EQ(E_FYLE, square_Fyle(E4));
	EXPECT_EQ(RANK_4, square_Rank(E4));

	EXPECT_EQ(RANK_1, rank_FromChar('1'));
	EXPECT_EQ(RANK_8, rank_FromChar('8'));
	EXPECT_EQ(NO_RANK, rank_FromChar('9'));
	EXPECT_EQ(A_FYLE, fyle_FromChar('a'));
	EXPECT_EQ(H_FYLE, fyle_FromChar('h'));
	EXPECT_EQ(NO_FYLE, fyle_FromChar('i'));
}

TEST(PrimitivesTest, ComputesRelativeSquaresAndRanks) {
	EXPECT_EQ(A1, square_Relative(WHITE, A1));
	EXPECT_EQ(A8, square_Relative(BLACK, A1));
	EXPECT_EQ(E4, square_Relative(WHITE, E4));
	EXPECT_EQ(E5, square_Relative(BLACK, E4));

	EXPECT_EQ(RANK_1, rank_Relative(WHITE, RANK_1));
	EXPECT_EQ(RANK_8, rank_Relative(BLACK, RANK_1));
	EXPECT_EQ(RANK_4, rank_Relative(WHITE, RANK_4));
	EXPECT_EQ(RANK_5, rank_Relative(BLACK, RANK_4));
}

TEST(PrimitivesTest, FlipsColours) {
	EXPECT_EQ(BLACK, color_Flip(WHITE));
	EXPECT_EQ(WHITE, color_Flip(BLACK));
	EXPECT_EQ('W', color_Char(WHITE));
	EXPECT_EQ('B', color_Char(BLACK));
	EXPECT_EQ('_', color_Char(NOCOLOR));
}

} // namespace
