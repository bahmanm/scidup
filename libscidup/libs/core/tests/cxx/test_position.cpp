/** @file
 * Position regressions that depend only on libscidup-core.
 */

#include "scidup/core/position.h"
#include "scidup/core/game.h"

#include <cstring>
#include <gtest/gtest.h>
#include <string>
#include <unordered_set>
#include <vector>

namespace {

template <typename PosT, typename MoveT>
auto parse_move(PosT& pos, MoveT dest, std::string_view move) {
	return pos.ParseMove(dest, move.data(), move.data() + move.size());
}

} // namespace

TEST(Test_PositionSAN, MakeSANStringFromUCI) {
	static const char* positions[] = {
	    "2k4r/ppprnp1p/5pq1/1P2b3/P1R1P3/Q1N2N2/5PPP/4K1R1 b - - 0 22",
	    "h8d8", "Rhd8", "d7d8", "Rdd8",
	    "2kr1b1r/pp1qpp1p/2n2p2/1BPp3b/3P4/P1N2P2/1P4PP/R2QK1NR w KQ - 0 12",
	    "g1e2", "Nge2",
	    "rnb2r2/pppnq1p1/6k1/2p1PpN1/2Pp4/3Q3P/PP3PP1/R1B2RK1 w - f6 0 15",
	    "e5f6", "exf6+",
	    "r6r/pppknp1p/6p1/4p3/8/NP2P3/P1P2PPP/R3K2R w KQ - 0 13",
	    "e1c1", "O-O-O+",
	    "8/6k1/1K1Q4/8/8/8/6pQ/q1q5 b - - 0 23",
	    "g2g1q", "g1=Q+", "g2g1b", "g1=B+", "g2g1r", "g1=R",
	    "c1g1", "Qg1+"};

	scid::core::Position pos;
	char buf[64];
	auto it = std::begin(positions);
	for (; it != std::end(positions); ++it) {
		auto slen = std::strlen(*it);
		if (slen > 5) {
			ASSERT_EQ(scid::core::OK, pos.ReadFromFEN(*it));
			continue;
		}

		scid::core::MoveAction sm;
		ASSERT_EQ(scid::core::OK, pos.ReadCoordMove(&sm, *it++, int(slen), false));
		pos.MakeSANString(&sm, buf, scid::core::SAN_MATETEST);
		EXPECT_STREQ(*it, buf);
	}
}

TEST(Test_PositionMoveSpec, ParsesAppliesAndFormatsSan) {
	scid::core::Position pos = scid::core::Position::getStdStart();
	scid::core::MoveSpec spec;

	ASSERT_EQ(scid::core::OK, pos.parseMoveSpec(spec, "e4"));
	EXPECT_EQ(scid::core::E2, spec.from);
	EXPECT_EQ(scid::core::E4, spec.to);
	EXPECT_EQ(scid::core::EMPTY, spec.promotion);
	EXPECT_FALSE(spec.castling);
	EXPECT_EQ("e4", pos.makeSan(spec, scid::core::SAN_MATETEST));
	ASSERT_EQ(scid::core::OK, pos.applyMove(spec));

	ASSERT_EQ(scid::core::OK, pos.parseMoveSpec(spec, "c5"));
	EXPECT_EQ(scid::core::C7, spec.from);
	EXPECT_EQ(scid::core::C5, spec.to);
	EXPECT_EQ("c5", pos.makeSan(spec, scid::core::SAN_MATETEST));
	ASSERT_EQ(scid::core::OK, pos.applyMove(spec));

	char fen[128];
	pos.PrintFEN(fen, sizeof(fen));
	EXPECT_STREQ(fen,
	             "rnbqkbnr/pp1ppppp/8/2p5/4P3/8/PPPP1PPP/RNBQKBNR w KQkq - 0 2");
}

TEST(Test_PositionMoveSpec, ParsesCoordinateCastlingAndPromotion) {
	{
		scid::core::Position pos;
		ASSERT_EQ(scid::core::OK,
		          pos.ReadFromFEN("r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1"));
		scid::core::MoveSpec spec;
		ASSERT_EQ(scid::core::OK,
		          pos.readCoordinateMoveSpec(spec, "e1g1", false));
		EXPECT_EQ(scid::core::E1, spec.from);
		EXPECT_EQ(scid::core::G1, spec.to);
		EXPECT_TRUE(spec.castling);
		EXPECT_EQ("O-O", pos.makeSan(spec, scid::core::SAN_MATETEST));
		ASSERT_EQ(scid::core::OK, pos.applyMove(spec));

		char fen[128];
		pos.PrintFEN(fen, sizeof(fen));
		EXPECT_STREQ(fen, "r3k2r/8/8/8/8/8/8/R4RK1 b kq - 1 1");
	}
	{
		scid::core::Position pos;
		ASSERT_EQ(scid::core::OK,
		          pos.ReadFromFEN("k7/6P1/8/8/8/8/8/7K w - - 0 1"));
		scid::core::MoveSpec spec;
		ASSERT_EQ(scid::core::OK,
		          pos.readCoordinateMoveSpec(spec, "g7g8q", false));
		EXPECT_EQ(scid::core::G7, spec.from);
		EXPECT_EQ(scid::core::G8, spec.to);
		EXPECT_EQ(scid::core::QUEEN, spec.promotion);
		EXPECT_EQ("g8=Q+", pos.makeSan(spec, scid::core::SAN_MATETEST));
	}
}

TEST(Test_ReadFromFen, RejectsInvalidFEN) {
	scid::core::Position pos;
	EXPECT_EQ(scid::core::OK,
	          pos.ReadFromFEN("rnb1k2Q/1p5p/p7/4p3/4q3/8/PPP2R1P/2K5 b"));
	EXPECT_NE(scid::core::OK,
	          pos.ReadFromFEN("rnb1k2/Q1p5p/p7/4p3/4q3/8/PPP2R1P/2K5 b"));
	EXPECT_NE(scid::core::OK,
	          pos.ReadFromFEN("rnb1k2Q/1p5p/p7/4a3/4q3/8/PPP2R1P/2K5 b"));
	EXPECT_NE(scid::core::OK,
	          pos.ReadFromFEN("rnb1k2Q/1p5p/p7/4p3/4q3/8/PKP2R1P/2K5 b"));
	EXPECT_NE(scid::core::OK,
	          pos.ReadFromFEN("rnb1k2Q/1p5p/p7/4p3/4q3/8/PPP2R1P/2K5 a"));
	EXPECT_NE(scid::core::OK,
	          pos.ReadFromFEN(
	              "rnbqkbn1/ppppppppr/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"));
}

TEST(Test_ReadFromFen, ParsesCountersAndEpTarget) {
	scid::core::Position pos;
	char buf[1024];

	EXPECT_EQ(scid::core::OK, pos.ReadFromFEN("8/K7/8/8/7k/8/8/8 w - - 45 25"));
	EXPECT_EQ(scid::core::NULL_SQUARE, pos.GetEPTarget());
	EXPECT_EQ(pos.GetPlyCounter() / 2 + 1, 25);
	pos.PrintFEN(buf, sizeof(buf));
	EXPECT_STREQ(buf, "8/K7/8/8/7k/8/8/8 w - - 45 25");

	EXPECT_EQ(scid::core::OK, pos.ReadFromFEN("8/K7/8/8/7k/8/8/8 w - f3 1 1"));
	EXPECT_EQ(scid::core::F3, pos.GetEPTarget());
	EXPECT_NE(scid::core::OK, pos.ReadFromFEN("8/K7/8/8/7k/8/8/8 w - i6 1 1"));
}

TEST(Test_PositionApplyMoveAction, RestoresCastlingFlags) {
	std::vector<scid::core::MoveAction> moves;
	char buf[1024];
	scid::core::Position pos;
	ASSERT_EQ(scid::core::OK,
	          pos.ReadFromFEN("r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1"));

	parse_move(pos, &moves.emplace_back(), "e1g1");
	pos.applyMoveAction(moves.back());
	parse_move(pos, &moves.emplace_back(), "h8g8");
	pos.applyMoveAction(moves.back());
	parse_move(pos, &moves.emplace_back(), "g1h2");
	pos.applyMoveAction(moves.back());
	parse_move(pos, &moves.emplace_back(), "e8c8");
	pos.applyMoveAction(moves.back());
	pos.PrintFEN(buf, sizeof(buf));
	EXPECT_STREQ(buf, "2kr2r1/8/8/8/8/8/7K/R4R2 w - - 4 3");

	for (auto it = moves.crbegin(); it != moves.crend(); ++it) {
		pos.undoMoveAction(*it);
	}
	pos.PrintFEN(buf, sizeof(buf));
	EXPECT_STREQ(buf, "r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1");
}

TEST(Test_PositionIsLegalMove, CoversCastlingCheckAndEnPassant) {
	{
		scid::core::Position pos;
		ASSERT_EQ(scid::core::OK,
		          pos.ReadFromFEN("8/8/8/8/8/8/6k1/4K2R w K -"));
		EXPECT_FALSE(pos.IsLegalMove(scid::core::E1, scid::core::G1,
		                             scid::core::EMPTY));
	}
	{
		scid::core::Position pos;
		ASSERT_EQ(scid::core::OK,
		          pos.ReadFromFEN("8/2B5/8/8/4pP2/8/7k/3K4 b - f3 0 1"));
		EXPECT_FALSE(pos.IsLegalMove(scid::core::E4, scid::core::F3,
		                             scid::core::EMPTY));
	}
	{
		scid::core::Position pos = scid::core::Position::getStdStart();
		EXPECT_TRUE(pos.IsLegalMove(scid::core::B1, scid::core::C3,
		                            scid::core::EMPTY));
		EXPECT_FALSE(pos.IsLegalMove(scid::core::E3, scid::core::E4,
		                             scid::core::EMPTY));
	}
}

TEST(Test_PrintFen, NormalizesIllegalCastlingFlags) {
	std::unordered_set<std::string> flags;
	flags.insert("KQkq");
	std::string str = "KQkq";
	for (int len = 1; len <= 3; len++) {
		for (size_t i = 0; i <= str.length() - len; i++) {
			flags.insert(str.substr(i, len));
		}
	}

	for (auto color : {'w', 'b'}) {
		for (auto const& flag : flags) {
			std::string fen = "4k3/8/8/8/8/8/8/4K2R ";
			fen.append(1, color).append(1, ' ');
			auto expected = fen;
			expected.append(flag.find('K') != std::string::npos ? "K" : "-");
			expected.append(" - 0 1");
			fen.append(flag).append(" - 0 1");

			scid::core::Position pos;
			pos.ReadFromFEN(fen.c_str());
			char buf[1024];
			pos.PrintFEN(buf, sizeof(buf));
			EXPECT_STREQ(buf, expected.c_str());
		}
	}
}
