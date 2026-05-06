#include "scidup/core/move_predicates.h"

#include <algorithm>
#include <gtest/gtest.h>

namespace {

using namespace scid::database;

TEST(MovePredicatesTest, ValidatesPieceGeometry) {
	EXPECT_TRUE(move_predicates::valid_king(E4, E5));
	EXPECT_FALSE(move_predicates::valid_king(E4, E6));

	EXPECT_TRUE(move_predicates::valid_knight(B1, C3));
	EXPECT_FALSE(move_predicates::valid_knight(B1, C2));

	EXPECT_EQ(move_predicates::NSQUARES, move_predicates::valid_slider(A1, A8, ROOK));
	EXPECT_EQ(move_predicates::NSQUARES + 1,
	          move_predicates::valid_slider(A1, H8, BISHOP));
	EXPECT_EQ(1, move_predicates::valid_slider(A1, H1, QUEEN));
	EXPECT_EQ(0, move_predicates::valid_slider(A1, H8, ROOK));
}

TEST(MovePredicatesTest, DetectsAttacksByPieceType) {
	const int empty = 1234;
	int board[64];
	std::fill_n(board, 64, empty);

	board[12] = !empty;
	board[19] = !empty;
	board[21] = !empty;
	board[28] = !empty;
	board[33] = !empty;
	board[52] = !empty;

	auto isOccupied = [&](auto square) { return board[square] != empty; };
	EXPECT_TRUE(move_predicates::attack(31, 29, WHITE, QUEEN, isOccupied));
	EXPECT_FALSE(move_predicates::attack(31, 27, WHITE, QUEEN, isOccupied));
	EXPECT_TRUE(move_predicates::attack(52, 28, BLACK, ROOK, isOccupied));
	EXPECT_FALSE(move_predicates::attack(52, 20, BLACK, ROOK, isOccupied));
	EXPECT_TRUE(move_predicates::attack(1, 11, WHITE, KNIGHT, isOccupied));
	EXPECT_FALSE(move_predicates::attack(1, 6, WHITE, KNIGHT, isOccupied));
	EXPECT_TRUE(move_predicates::attack(33, 19, BLACK, BISHOP, isOccupied));
	EXPECT_FALSE(move_predicates::attack(33, 12, BLACK, BISHOP, isOccupied));
	EXPECT_TRUE(move_predicates::attack(12, 19, WHITE, PAWN, isOccupied));
	EXPECT_FALSE(move_predicates::attack(12, 20, WHITE, PAWN, isOccupied));
	EXPECT_TRUE(move_predicates::attack(19, 12, BLACK, PAWN, isOccupied));
	EXPECT_FALSE(move_predicates::attack(12, 19, BLACK, PAWN, isOccupied));
	EXPECT_TRUE(move_predicates::attack(12, 3, WHITE, KING, isOccupied));
	EXPECT_FALSE(move_predicates::attack(12, 14, WHITE, KING, isOccupied));
}

TEST(MovePredicatesTest, DetectsPawnAdvancesAndPseudoMoves) {
	bool occupied[64] = {};
	auto isOccupied = [&](auto square) { return occupied[square]; };

	EXPECT_TRUE(move_predicates::pseudo_advance_pawn(E2, E3, WHITE, isOccupied));
	EXPECT_TRUE(move_predicates::pseudo_advance_pawn(E2, E4, WHITE, isOccupied));
	EXPECT_FALSE(move_predicates::pseudo_advance_pawn(E2, E5, WHITE, isOccupied));

	occupied[E3] = true;
	EXPECT_FALSE(move_predicates::pseudo_advance_pawn(E2, E4, WHITE, isOccupied));
	occupied[E3] = false;

	EXPECT_TRUE(move_predicates::pseudo(E7, E5, BLACK, PAWN, isOccupied));
	EXPECT_TRUE(move_predicates::pseudo(B1, C3, WHITE, KNIGHT, isOccupied));
	EXPECT_FALSE(move_predicates::pseudo(B1, C2, WHITE, KNIGHT, isOccupied));
}

TEST(MovePredicatesTest, DetectsOpenedRays) {
	const int empty = 7777;
	int board[64];
	std::fill_n(board, 64, empty);

	board[12] = !empty;
	board[19] = !empty;
	board[21] = !empty;
	board[28] = !empty;
	board[33] = !empty;
	board[52] = !empty;

	auto isOccupied = [&](auto square) { return board[square] != empty; };
	auto test = move_predicates::opens_ray(19, 27, 12, isOccupied);
	EXPECT_EQ(BISHOP, test.first);
	EXPECT_EQ(33, test.second);

	test = move_predicates::opens_ray(21, 29, 12, isOccupied);
	EXPECT_EQ(INVALID_PIECE, test.first);

	test = move_predicates::opens_ray(28, 20, 12, isOccupied);
	EXPECT_EQ(INVALID_PIECE, test.first);

	test = move_predicates::opens_ray(28, 27, 12, isOccupied);
	EXPECT_EQ(ROOK, test.first);
	EXPECT_EQ(52, test.second);
}

TEST(MovePredicatesTest, DetectsRayBlocks) {
	EXPECT_TRUE(move_predicates::blocks_ray(A1, H8, D4));
	EXPECT_TRUE(move_predicates::blocks_ray(A1, H1, D1));
	EXPECT_FALSE(move_predicates::blocks_ray(A1, H8, D5));
}

} // namespace
