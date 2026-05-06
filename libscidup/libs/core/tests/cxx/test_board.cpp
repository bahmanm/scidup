#include "scidup/core/board.h"

#include <gtest/gtest.h>

namespace {

using namespace scid::database;

TEST(BoardTest, DefinesMinorPieceAndSliderHelpers) {
	EXPECT_EQ(16, WM);
	EXPECT_EQ(17, BM);
	EXPECT_EQ(18U, MAX_PIECE_TYPES);

	EXPECT_EQ(BQ, PIECE_FLIP[WQ]);
	EXPECT_EQ(WQ, PIECE_FLIP[BQ]);
	EXPECT_EQ(BM, PIECE_FLIP[WM]);
	EXPECT_EQ(WM, PIECE_FLIP[BM]);

	EXPECT_TRUE(piece_IsKing(WK));
	EXPECT_TRUE(piece_IsKing(BK));
	EXPECT_FALSE(piece_IsKing(WQ));

	EXPECT_TRUE(piece_IsSlider(WQ));
	EXPECT_TRUE(piece_IsSlider(BB));
	EXPECT_FALSE(piece_IsSlider(WN));

	EXPECT_EQ(KING, piece_FromChar('K'));
	EXPECT_EQ(QUEEN, piece_FromChar('Q'));
	EXPECT_EQ(ROOK, piece_FromChar('R'));
	EXPECT_EQ(KNIGHT, piece_FromChar('N'));
	EXPECT_EQ(BISHOP, piece_FromChar('B'));
	EXPECT_EQ(EMPTY, piece_FromChar('P'));
}

TEST(BoardTest, ComputesSquareColoursDiagonalsAndFlips) {
	EXPECT_EQ(0, square_LeftDiag(A1));
	EXPECT_EQ(7, square_RightDiag(A1));
	EXPECT_EQ(7, square_LeftDiag(H1));
	EXPECT_EQ(7, square_RightDiag(H8));

	EXPECT_EQ(BLACK, square_Color(A1));
	EXPECT_EQ(WHITE, square_Color(B1));

	EXPECT_EQ(H1, square_FlipFyle(A1));
	EXPECT_EQ(A8, square_FlipRank(A1));
	EXPECT_EQ(A1, square_FlipDiag(A1));
	EXPECT_EQ(D5, square_FlipDiag(E4));

	EXPECT_EQ('a', square_FyleChar(A1));
	EXPECT_EQ('h', square_FyleChar(H8));
	EXPECT_EQ('1', square_RankChar(A1));
	EXPECT_EQ('8', square_RankChar(H8));
}

TEST(BoardTest, ComputesSquareDistanceAndEdges) {
	EXPECT_EQ(0U, square_Distance(E4, E4));
	EXPECT_EQ(1U, square_Distance(E4, F5));
	EXPECT_EQ(7U, square_Distance(A1, H8));

	EXPECT_EQ(A1, square_NearestCorner(B2));
	EXPECT_EQ(H8, square_NearestCorner(G7));
	EXPECT_TRUE(square_IsCornerSquare(H8));
	EXPECT_FALSE(square_IsCornerSquare(E4));

	EXPECT_TRUE(square_IsEdgeSquare(A4));
	EXPECT_TRUE(square_IsEdgeSquare(E8));
	EXPECT_FALSE(square_IsEdgeSquare(E4));

	EXPECT_EQ(0, square_EdgeDistance(A1));
	EXPECT_EQ(3, square_EdgeDistance(D4));
	EXPECT_EQ(-1, square_EdgeDistance(NULL_SQUARE));
}

TEST(BoardTest, DetectsKnightHopsAndAdjacency) {
	EXPECT_TRUE(square_IsKnightHop(E4, F6));
	EXPECT_TRUE(square_IsKnightHop(E4, C3));
	EXPECT_FALSE(square_IsKnightHop(E4, E5));

	EXPECT_TRUE(square_Adjacent(E4, E4));
	EXPECT_TRUE(square_Adjacent(E4, F5));
	EXPECT_TRUE(square_Adjacent(A1, B1));
	EXPECT_FALSE(square_Adjacent(A1, C1));
}

TEST(BoardTest, DefinesDirectionsAndDeltas) {
	EXPECT_EQ(0, NULL_DIR);
	EXPECT_EQ(1, UP);
	EXPECT_EQ(2, DOWN);
	EXPECT_EQ(4, LEFT);
	EXPECT_EQ(8, RIGHT);
	EXPECT_EQ(5, UP_LEFT);
	EXPECT_EQ(9, UP_RIGHT);
	EXPECT_EQ(6, DOWN_LEFT);
	EXPECT_EQ(10, DOWN_RIGHT);

	EXPECT_EQ(DOWN, direction_Opposite(UP));
	EXPECT_EQ(UP_LEFT, direction_Opposite(DOWN_RIGHT));
	EXPECT_TRUE(direction_IsDiagonal(UP_RIGHT));
	EXPECT_FALSE(direction_IsDiagonal(RIGHT));

	EXPECT_EQ(8, direction_Delta(UP));
	EXPECT_EQ(-8, direction_Delta(DOWN));
	EXPECT_EQ(-1, direction_Delta(LEFT));
	EXPECT_EQ(1, direction_Delta(RIGHT));
}

TEST(BoardTest, DefinesStartingBoardAndSquareColours) {
	EXPECT_EQ(WR, START_BOARD[A1]);
	EXPECT_EQ(WK, START_BOARD[E1]);
	EXPECT_EQ(WP, START_BOARD[A2]);
	EXPECT_EQ(BP, START_BOARD[A7]);
	EXPECT_EQ(BK, START_BOARD[E8]);
	EXPECT_EQ(BR, START_BOARD[H8]);
	EXPECT_EQ(EMPTY, START_BOARD[COLOR_SQUARE]);
	EXPECT_EQ(END_OF_BOARD, START_BOARD[NULL_SQUARE]);

	EXPECT_EQ(BLACK, BOARD_SQUARECOLOR[A1]);
	EXPECT_EQ(WHITE, BOARD_SQUARECOLOR[H1]);
	EXPECT_EQ(WHITE, BOARD_SQUARECOLOR[A8]);
	EXPECT_EQ(BLACK, BOARD_SQUARECOLOR[H8]);
	EXPECT_EQ(NOCOLOR, BOARD_SQUARECOLOR[COLOR_SQUARE]);
	EXPECT_EQ(NOCOLOR, BOARD_SQUARECOLOR[NULL_SQUARE]);
}

} // namespace
