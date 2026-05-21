#include "scidup/core/attacks.h"

#include <array>
#include <gtest/gtest.h>

namespace {

using namespace scid::core;

template <std::size_t N>
void expectAttackList(const squareT (&actual)[9],
                      const std::array<squareT, N>& expected) {
	for (std::size_t i = 0; i < expected.size(); ++i) {
		EXPECT_EQ(expected[i], actual[i]) << "at index " << i;
	}
	for (std::size_t i = expected.size(); i < 9; ++i) {
		EXPECT_EQ(NS, actual[i]) << "at index " << i;
	}
}

TEST(AttacksTest, KnightAttacksFromCorner) {
	expectAttackList(knightAttacks[A1], std::array{C2, B3});
}

TEST(AttacksTest, KnightAttacksFromCentre) {
	expectAttackList(knightAttacks[E4],
	                 std::array{D2, F2, C3, G3, C5, G5, D6, F6});
}

TEST(AttacksTest, KingAttacksFromCorner) {
	expectAttackList(kingAttacks[A1], std::array{B1, A2, B2});
}

TEST(AttacksTest, KingAttacksFromCentre) {
	expectAttackList(kingAttacks[E4],
	                 std::array{D3, E3, F3, D4, F4, D5, E5, F5});
}

TEST(AttacksTest, SentinelSquaresHaveNoAttacks) {
	expectAttackList(knightAttacks[COLOR_SQUARE], std::array<squareT, 0>{});
	expectAttackList(knightAttacks[NULL_SQUARE], std::array<squareT, 0>{});
	expectAttackList(kingAttacks[COLOR_SQUARE], std::array<squareT, 0>{});
	expectAttackList(kingAttacks[NULL_SQUARE], std::array<squareT, 0>{});
}

} // namespace
