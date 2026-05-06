/** @file
 * Position regressions that depend only on libscidup-core.
 */

#include "scidup/core/position.h"

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

	scid::database::Position pos;
	char buf[64];
	auto it = std::begin(positions);
	for (; it != std::end(positions); ++it) {
		auto slen = std::strlen(*it);
		if (slen > 5) {
			ASSERT_EQ(scid::database::OK, pos.ReadFromFEN(*it));
			continue;
		}

		scid::database::simpleMoveT sm;
		ASSERT_EQ(scid::database::OK, pos.ReadCoordMove(&sm, *it++, int(slen), false));
		pos.MakeSANString(&sm, buf, scid::database::SAN_MATETEST);
		EXPECT_STREQ(*it, buf);
	}
}

TEST(Test_ReadFromFen, RejectsInvalidFEN) {
	scid::database::Position pos;
	EXPECT_EQ(scid::database::OK,
	          pos.ReadFromFEN("rnb1k2Q/1p5p/p7/4p3/4q3/8/PPP2R1P/2K5 b"));
	EXPECT_NE(scid::database::OK,
	          pos.ReadFromFEN("rnb1k2/Q1p5p/p7/4p3/4q3/8/PPP2R1P/2K5 b"));
	EXPECT_NE(scid::database::OK,
	          pos.ReadFromFEN("rnb1k2Q/1p5p/p7/4a3/4q3/8/PPP2R1P/2K5 b"));
	EXPECT_NE(scid::database::OK,
	          pos.ReadFromFEN("rnb1k2Q/1p5p/p7/4p3/4q3/8/PKP2R1P/2K5 b"));
	EXPECT_NE(scid::database::OK,
	          pos.ReadFromFEN("rnb1k2Q/1p5p/p7/4p3/4q3/8/PPP2R1P/2K5 a"));
	EXPECT_NE(scid::database::OK,
	          pos.ReadFromFEN(
	              "rnbqkbn1/ppppppppr/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"));
}

TEST(Test_ReadFromFen, ParsesCountersAndEpTarget) {
	scid::database::Position pos;
	char buf[1024];

	EXPECT_EQ(scid::database::OK, pos.ReadFromFEN("8/K7/8/8/7k/8/8/8 w - - 45 25"));
	EXPECT_EQ(scid::database::NULL_SQUARE, pos.GetEPTarget());
	EXPECT_EQ(pos.GetPlyCounter() / 2 + 1, 25);
	pos.PrintFEN(buf, sizeof(buf));
	EXPECT_STREQ(buf, "8/K7/8/8/7k/8/8/8 w - - 45 25");

	EXPECT_EQ(scid::database::OK, pos.ReadFromFEN("8/K7/8/8/7k/8/8/8 w - f3 1 1"));
	EXPECT_EQ(scid::database::F3, pos.GetEPTarget());
	EXPECT_NE(scid::database::OK, pos.ReadFromFEN("8/K7/8/8/7k/8/8/8 w - i6 1 1"));
}

TEST(Test_PositionDoSimpleMove, RestoresCastlingFlags) {
	std::vector<scid::database::simpleMoveT> moves;
	char buf[1024];
	scid::database::Position pos;
	ASSERT_EQ(scid::database::OK,
	          pos.ReadFromFEN("r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1"));

	parse_move(pos, &moves.emplace_back(), "e1g1");
	pos.DoSimpleMove(moves.back());
	parse_move(pos, &moves.emplace_back(), "h8g8");
	pos.DoSimpleMove(moves.back());
	parse_move(pos, &moves.emplace_back(), "g1h2");
	pos.DoSimpleMove(moves.back());
	parse_move(pos, &moves.emplace_back(), "e8c8");
	pos.DoSimpleMove(moves.back());
	pos.PrintFEN(buf, sizeof(buf));
	EXPECT_STREQ(buf, "2kr2r1/8/8/8/8/8/7K/R4R2 w - - 4 3");

	for (auto it = moves.crbegin(); it != moves.crend(); ++it) {
		pos.UndoSimpleMove(*it);
	}
	pos.PrintFEN(buf, sizeof(buf));
	EXPECT_STREQ(buf, "r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1");
}

TEST(Test_PositionIsLegalMove, CoversCastlingCheckAndEnPassant) {
	{
		scid::database::Position pos;
		ASSERT_EQ(scid::database::OK,
		          pos.ReadFromFEN("8/8/8/8/8/8/6k1/4K2R w K -"));
		EXPECT_FALSE(pos.IsLegalMove(scid::database::E1, scid::database::G1,
		                             scid::database::EMPTY));
	}
	{
		scid::database::Position pos;
		ASSERT_EQ(scid::database::OK,
		          pos.ReadFromFEN("8/2B5/8/8/4pP2/8/7k/3K4 b - f3 0 1"));
		EXPECT_FALSE(pos.IsLegalMove(scid::database::E4, scid::database::F3,
		                             scid::database::EMPTY));
	}
	{
		scid::database::Position pos = scid::database::Position::getStdStart();
		EXPECT_TRUE(pos.IsLegalMove(scid::database::B1, scid::database::C3,
		                            scid::database::EMPTY));
		EXPECT_FALSE(pos.IsLegalMove(scid::database::E3, scid::database::E4,
		                             scid::database::EMPTY));
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

			scid::database::Position pos;
			pos.ReadFromFEN(fen.c_str());
			char buf[1024];
			pos.PrintFEN(buf, sizeof(buf));
			EXPECT_STREQ(buf, expected.c_str());
		}
	}
}
