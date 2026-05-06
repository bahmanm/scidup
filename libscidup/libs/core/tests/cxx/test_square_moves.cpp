#include "scidup/core/square_moves.h"

#include <gtest/gtest.h>

namespace {

using namespace scid::database;

TEST(SquareMovesTest, MovesOneStepInCardinalAndDiagonalDirections) {
	EXPECT_EQ(A2, square_Move(A1, UP));
	EXPECT_EQ(NS, square_Move(A1, DOWN));
	EXPECT_EQ(NS, square_Move(A1, LEFT));
	EXPECT_EQ(B1, square_Move(A1, RIGHT));
	EXPECT_EQ(B2, square_Move(A1, UP_RIGHT));
	EXPECT_EQ(NS, square_Move(A1, DOWN_RIGHT));

	EXPECT_EQ(E5, square_Move(E4, UP));
	EXPECT_EQ(E3, square_Move(E4, DOWN));
	EXPECT_EQ(D4, square_Move(E4, LEFT));
	EXPECT_EQ(F4, square_Move(E4, RIGHT));
}

TEST(SquareMovesTest, FindsLastReachableSquareInDirection) {
	EXPECT_EQ(A8, square_Last(A1, UP));
	EXPECT_EQ(H1, square_Last(A1, RIGHT));
	EXPECT_EQ(H8, square_Last(A1, UP_RIGHT));

	EXPECT_EQ(E8, square_Last(E4, UP));
	EXPECT_EQ(E1, square_Last(E4, DOWN));
	EXPECT_EQ(A4, square_Last(E4, LEFT));
	EXPECT_EQ(H4, square_Last(E4, RIGHT));
	EXPECT_EQ(H7, square_Last(E4, UP_RIGHT));
	EXPECT_EQ(B1, square_Last(E4, DOWN_LEFT));
}

TEST(SquareMovesTest, SentinelSquaresStaySentinel) {
	EXPECT_EQ(NS, square_Move(COLOR_SQUARE, UP));
	EXPECT_EQ(NS, square_Move(NULL_SQUARE, RIGHT));
	EXPECT_EQ(NS, square_Last(COLOR_SQUARE, UP_RIGHT));
	EXPECT_EQ(NS, square_Last(NULL_SQUARE, DOWN_LEFT));
}

} // namespace
