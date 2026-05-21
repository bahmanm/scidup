#include "scidup/core/movelist.h"
#include "scidup/core/notation.h"

#include <algorithm>
#include <gtest/gtest.h>
#include <string>
#include <vector>

namespace {

using namespace scid::core;

std::string longNotation(MoveAction const& move) {
	char buf[UCI_MOVE_STRING_SIZE] = {};
	char* end = move.toLongNotation(buf);
	return {buf, end};
}

TEST(MoveListTest, DetectsNullMoves) {
	MoveAction move{};
	move.from = E1;
	move.to = E1;
	move.movingPiece = WK;

	EXPECT_TRUE(move.isNullMove());

	move.movingPiece = WQ;
	EXPECT_FALSE(move.isNullMove());

	move.movingPiece = WK;
	move.from = NULL_SQUARE;
	move.to = NULL_SQUARE;

	EXPECT_FALSE(move.isNullMove());
}

TEST(MoveListTest, DetectsCastlingDirection) {
	MoveAction move{};
	move.from = E1;
	move.to = H1;
	move.castling = 1;

	EXPECT_EQ(2, move.isCastle());

	move.from = E8;
	move.to = A8;

	EXPECT_EQ(-2, move.isCastle());

	move.castling = 0;

	EXPECT_EQ(0, move.isCastle());
}

TEST(MoveListTest, WritesLongNotation) {
	MoveAction move{};
	move.from = E2;
	move.to = E4;
	move.promote = EMPTY;

	EXPECT_EQ("e2e4", longNotation(move));

	move.from = E7;
	move.to = E8;
	move.promote = QUEEN;

	EXPECT_EQ("e7e8q", longNotation(move));

	move.from = E1;
	move.to = E1;

	EXPECT_EQ("0000", longNotation(move));
}

TEST(MoveListTest, OrdersHigherScoresFirst) {
	ScoredMove lower{};
	lower.score = 10;

	ScoredMove higher{};
	higher.score = 20;

	EXPECT_TRUE(higher < lower);
	EXPECT_FALSE(lower < higher);

	std::vector<ScoredMove> moves{lower, higher};
	std::sort(moves.begin(), moves.end());

	EXPECT_EQ(20, moves[0].score);
	EXPECT_EQ(10, moves[1].score);
}

TEST(MoveListTest, StoresAndResizesMoves) {
	MoveList moves;

	EXPECT_EQ(0U, moves.Size());
	EXPECT_EQ(moves.begin(), moves.end());

	ScoredMove& first = moves.emplace_back();

	EXPECT_EQ(1U, moves.Size());
	EXPECT_EQ(moves.begin() + 1, moves.end());
	EXPECT_EQ(0, first.score);
	EXPECT_EQ(0, first.from);

	first.from = E2;
	first.to = E4;
	first.score = 30;

	ScoredMove second{};
	second.from = G1;
	second.to = F3;
	second.score = 40;
	moves.push_back(second);

	ASSERT_EQ(2U, moves.Size());
	EXPECT_EQ(E2, moves.Get(0)->from);
	EXPECT_EQ(E4, moves.Get(0)->to);
	EXPECT_EQ(30, moves.Get(0)->score);
	EXPECT_EQ(G1, moves.Get(1)->from);
	EXPECT_EQ(F3, moves.Get(1)->to);
	EXPECT_EQ(40, moves.Get(1)->score);

	moves.resize(1);

	EXPECT_EQ(1U, moves.Size());
	EXPECT_EQ(moves.begin() + 1, moves.end());
	EXPECT_EQ(E2, moves.Get(0)->from);

	moves.Clear();

	EXPECT_EQ(0U, moves.Size());
	EXPECT_EQ(moves.begin(), moves.end());
}

} // namespace
