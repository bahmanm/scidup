#include "scidup/core/position.h"
#include "scidup/database/gameview.h"
#include "scidup/database/pgnparse.h"
#include "scidup/database/searchpos.h"

#include <cstring>
#include <gtest/gtest.h>
#include <string_view>

TEST(Test_FastBoardSAN, UCItoSAN) {
	// clang-format off
	static const char* positions[] = {
		"2k4r/ppprnp1p/5pq1/1P2b3/P1R1P3/Q1N2N2/5PPP/4K1R1 b - - 0 22",
			"h8d8", "Rhd8", "d7d8", "Rdd8",
		"2kr1b1r/pp1qpp1p/2n2p2/1BPp3b/3P4/P1N2P2/1P4PP/R2QK1NR w KQ - 0 12",
			"g1e2", "Nge2",
		"8/1R2r2k/p7/P6p/5PpP/3pP1P1/4r1B1/3K4 b - - 0 53",
			"e2e3", "Rxe3",
		"rnb2r2/pppnq1p1/6k1/2p1PpN1/2Pp4/3Q3P/PP3PP1/R1B2RK1 w - f6 0 15",
			"e5f6", "exf6+",
		"r6r/pppknp1p/6p1/4p3/8/NP2P3/P1P2PPP/R3K2R w KQ - 0 13",
			"e1c1", "O-O-O+",
		"8/6k1/1K1Q4/8/8/8/6pQ/q1q5 b - - 0 23",
			"g2g1q", "g1=Q+", "g2g1b", "g1=B+", "g2g1r", "g1=R", "c1g1", "Qg1+",
		"r4r2/1pq1nkb1/p1pnp1pp/P2p4/NB1P1PP1/3PP1N1/1P2K1Q1/R6R b - - 0 22",
			"f8h8", "Rh8",
		"q3k1q1/8/4q3/8/8/8/3K4/8 b - - 0 1",
			"a8d8", "Qd8+", "a8d5", "Qad5+", "e6d5", "Qed5+", "a8g2", "Qag2+", "g8g2", "Qgg2+",
		"4k3/2n1n3/8/3B4/8/2n5/8/4K3 b - - 0 1",
			"c7d5", "Nc7xd5", "e7d5", "Nexd5", "c3d5", "N3xd5",
		"rnbqk1nr/pppp1ppp/4p3/8/1b1P4/5N2/PPP1PPPP/RNBQKB1R w KQkq - 0 3",
			"b1d2", "Nbd2", "f3d2", "Nfd2", "c1d2", "Bd2",
		"rnbq1rk1/pppp1ppp/4pn2/8/1bPP4/2N1P3/PP3PPP/R1BQKBNR w KQ - 0 5",
			"g1e2", "Ne2",
		"4r3/3P1Pk1/8/3K4/8/8/8/8 w - - 0 5",
			"d7e8q", "dxe8=Q", "f7e8q", "fxe8=Q",
		"7k/8/8/1R3R2/8/3R4/8/K7 w - - 0 1",
			"d3d5", "Rdd5", "b5d5", "Rbd5", "f5d5", "Rfd5",
		"7k/8/2B1B3/8/2B5/8/8/K7 w - - 0 1",
			"c4d5", "B4d5", "c6d5", "Bc6d5", "e6d5", "Bed5",
		"7k/8/2q1q3/8/2q5/8/8/K7 b - - 0 1",
			"c4d5", "Q4d5", "c6d5", "Qc6d5", "e6d5", "Qed5"
	};
	// clang-format on

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

		pos.DoSimpleMove(sm);
		scid::database::FullMove fullmove;
		scid::database::colorT col = scid::database::piece_Color(sm.movingPiece);
		scid::database::pieceT pt = scid::database::piece_Type(sm.movingPiece);
		int castle = (pt == scid::database::KING) ? sm.isCastle() : 0;
		if (!castle) {
			fullmove = scid::database::FullMove(col, sm.from, sm.to, pt);
			if (sm.promote != scid::database::EMPTY) {
				fullmove.setPromo(scid::database::piece_Type(sm.promote));
			}
			if (sm.capturedPiece != scid::database::EMPTY) {
				fullmove.setCapture(sm.capturedPiece,
				                    pos.GetBoard()[sm.to] == scid::database::EMPTY);
			}
		} else if (col == scid::database::WHITE) {
			fullmove = scid::database::FullMove(
			    scid::database::WHITE, scid::database::E1,
			    (castle > 0) ? scid::database::H1 : scid::database::A1);
		} else {
			fullmove = scid::database::FullMove(
			    scid::database::BLACK, scid::database::E8,
			    (castle > 0) ? scid::database::H8 : scid::database::A8);
		}

		scid::database::FastBoard fastboard(pos);
		pos.UndoSimpleMove(sm);
		fastboard.fillSANInfo(fullmove);
		EXPECT_STREQ(*it, fullmove.getSAN().c_str());
	}
}

TEST(Test_MaterialCount, material_count) {
	scid::database::MaterialCount mt;
	scid::database::MaterialCount mt_ref;

	EXPECT_TRUE(mt == mt_ref);
	EXPECT_FALSE(mt != mt_ref);
	EXPECT_EQ(0, mt.count(scid::database::WHITE));
	EXPECT_EQ(0, mt.count(scid::database::BLACK));
	EXPECT_EQ(0, mt.count(scid::database::WHITE, scid::database::BISHOP));
	EXPECT_EQ(0, mt.count(scid::database::BLACK, scid::database::BISHOP));
	EXPECT_EQ(0, mt.count(scid::database::WHITE, scid::database::KING));
	EXPECT_EQ(0, mt.count(scid::database::BLACK, scid::database::KING));
	EXPECT_EQ(0, mt.count(scid::database::WHITE, scid::database::KNIGHT));
	EXPECT_EQ(0, mt.count(scid::database::BLACK, scid::database::KNIGHT));
	EXPECT_EQ(0, mt.count(scid::database::WHITE, scid::database::PAWN));
	EXPECT_EQ(0, mt.count(scid::database::BLACK, scid::database::PAWN));
	EXPECT_EQ(0, mt.count(scid::database::WHITE, scid::database::QUEEN));
	EXPECT_EQ(0, mt.count(scid::database::BLACK, scid::database::QUEEN));
	EXPECT_EQ(0, mt.count(scid::database::WHITE, scid::database::ROOK));
	EXPECT_EQ(0, mt.count(scid::database::BLACK, scid::database::ROOK));

	auto change_count = [](auto& obj) {
		obj.incr(scid::database::BLACK, scid::database::ROOK);
		obj.incr(scid::database::BLACK, scid::database::ROOK);

		obj.incr(scid::database::WHITE, scid::database::QUEEN);
		obj.incr(scid::database::WHITE, scid::database::QUEEN);
		obj.decr(scid::database::WHITE, scid::database::QUEEN);
		obj.incr(scid::database::WHITE, scid::database::QUEEN);
		obj.incr(scid::database::WHITE, scid::database::QUEEN);

		obj.incr(scid::database::WHITE, scid::database::PAWN);

		obj.incr(scid::database::BLACK, scid::database::BISHOP);
		obj.decr(scid::database::BLACK, scid::database::BISHOP);
	};

	change_count(mt);
	EXPECT_FALSE(mt == mt_ref);
	EXPECT_TRUE(mt != mt_ref);
	EXPECT_EQ(4, mt.count(scid::database::WHITE));
	EXPECT_EQ(2, mt.count(scid::database::BLACK));
	EXPECT_EQ(0, mt.count(scid::database::WHITE, scid::database::BISHOP));
	EXPECT_EQ(0, mt.count(scid::database::BLACK, scid::database::BISHOP));
	EXPECT_EQ(0, mt.count(scid::database::WHITE, scid::database::KING));
	EXPECT_EQ(0, mt.count(scid::database::BLACK, scid::database::KING));
	EXPECT_EQ(0, mt.count(scid::database::WHITE, scid::database::KNIGHT));
	EXPECT_EQ(0, mt.count(scid::database::BLACK, scid::database::KNIGHT));
	EXPECT_EQ(1, mt.count(scid::database::WHITE, scid::database::PAWN));
	EXPECT_EQ(0, mt.count(scid::database::BLACK, scid::database::PAWN));
	EXPECT_EQ(3, mt.count(scid::database::WHITE, scid::database::QUEEN));
	EXPECT_EQ(0, mt.count(scid::database::BLACK, scid::database::QUEEN));
	EXPECT_EQ(0, mt.count(scid::database::WHITE, scid::database::ROOK));
	EXPECT_EQ(2, mt.count(scid::database::BLACK, scid::database::ROOK));

	change_count(mt_ref);
	EXPECT_TRUE(mt == mt_ref);
	EXPECT_FALSE(mt != mt_ref);
	EXPECT_EQ(mt_ref.count(scid::database::WHITE), mt.count(scid::database::WHITE));
	EXPECT_EQ(mt_ref.count(scid::database::BLACK), mt.count(scid::database::BLACK));
}

TEST(Test_MaterialCount, less_mat) {
	auto count_pieces = [](const scid::database::pieceT* board) {
		scid::database::MaterialCount mt_count;
		for (int i = 0; i < 64; ++i) {
			if (board[i] != scid::database::EMPTY) {
				mt_count.incr(scid::database::piece_Color(board[i]),
				              scid::database::piece_Type(board[i]));
			}
		}
		return mt_count;
	};

	scid::database::Position pos;
	ASSERT_EQ(scid::database::OK,
	          pos.ReadFromFEN(
	              "2k4r/ppprnp1p/5pq1/1P2b3/P1R1P3/Q1N2N2/5PPP/4K1R1 b"));
	auto mt_count = count_pieces(pos.GetBoard());
	auto matSig = scid::database::matsig_Make(pos.GetMaterial());
	EXPECT_FALSE(scid::database::less_mat(mt_count, matSig, true, true));
	EXPECT_FALSE(scid::database::less_mat(mt_count, matSig, true, false));
	EXPECT_FALSE(scid::database::less_mat(mt_count, matSig, false, true));
	EXPECT_FALSE(scid::database::less_mat(mt_count, matSig, false, false));

	mt_count.decr(scid::database::WHITE, scid::database::QUEEN);
	EXPECT_TRUE(scid::database::less_mat(mt_count, matSig, true, true));
	EXPECT_TRUE(scid::database::less_mat(mt_count, matSig, true, false));
	EXPECT_TRUE(scid::database::less_mat(mt_count, matSig, false, true));
	EXPECT_TRUE(scid::database::less_mat(mt_count, matSig, false, false));

	mt_count.incr(scid::database::WHITE, scid::database::PAWN);
	EXPECT_FALSE(scid::database::less_mat(mt_count, matSig, true, true));
	EXPECT_FALSE(scid::database::less_mat(mt_count, matSig, true, false));
	EXPECT_TRUE(scid::database::less_mat(mt_count, matSig, false, true));
	EXPECT_TRUE(scid::database::less_mat(mt_count, matSig, false, false));

	mt_count.incr(scid::database::BLACK, scid::database::PAWN);
	mt_count.decr(scid::database::BLACK, scid::database::KNIGHT);
	EXPECT_FALSE(scid::database::less_mat(mt_count, matSig, true, true));
	EXPECT_TRUE(scid::database::less_mat(mt_count, matSig, true, false));
	EXPECT_TRUE(scid::database::less_mat(mt_count, matSig, false, true));
	EXPECT_TRUE(scid::database::less_mat(mt_count, matSig, false, false));
}

TEST(Test_PrintFen, castling_flag_kside_from_pgn) {
	std::string_view pgn =
	    "[FEN \"Brbnk1r1/3pppq1/8/ppp3pp/PPP3PP/8/3PPPQ1/bRBNK1R1 w KQkq - "
	    "0 1\"]"
	    "1. Rxa1 Rxa8 2. Ra3 Ra6 3. Rh3 Rh6 4. Rhh1 Rhh8 5. O-O O-O";
	scid::database::Game game;
	scid::database::PgnParseLog parseLog;
	ASSERT_TRUE(scid::database::pgnParseGame(pgn.data(), pgn.size(), game, parseLog));
	game.MoveToStart();

	const char* fens[] = {
	    "Brbnk1r1/3pppq1/8/ppp3pp/PPP3PP/8/3PPPQ1/bRBNK1R1 w KQkq - 0 1",
	    "Brbnk1r1/3pppq1/8/ppp3pp/PPP3PP/8/3PPPQ1/R1BNK1R1 b Kkq - 0 1",
	    "r1bnk1r1/3pppq1/8/ppp3pp/PPP3PP/8/3PPPQ1/R1BNK1R1 w Kk - 0 2",
	    "r1bnk1r1/3pppq1/8/ppp3pp/PPP3PP/R7/3PPPQ1/2BNK1R1 b Kk - 1 2",
	    "2bnk1r1/3pppq1/r7/ppp3pp/PPP3PP/R7/3PPPQ1/2BNK1R1 w Kk - 2 3",
	    "2bnk1r1/3pppq1/r7/ppp3pp/PPP3PP/7R/3PPPQ1/2BNK1R1 b Kk - 3 3",
	    "2bnk1r1/3pppq1/7r/ppp3pp/PPP3PP/7R/3PPPQ1/2BNK1R1 w Kk - 4 4",
	    "2bnk1r1/3pppq1/7r/ppp3pp/PPP3PP/8/3PPPQ1/2BNK1RR b Gk - 5 4",
	    "2bnk1rr/3pppq1/8/ppp3pp/PPP3PP/8/3PPPQ1/2BNK1RR w Gg - 6 5",
	    "2bnk1rr/3pppq1/8/ppp3pp/PPP3PP/8/3PPPQ1/2BN1RKR b g - 7 5",
	    "2bn1rkr/3pppq1/8/ppp3pp/PPP3PP/8/3PPPQ1/2BN1RKR w - - 8 6"};
	for (auto expected : fens) {
		char buf[1024];
		game.currentPos()->PrintFEN(buf, sizeof(buf));
		EXPECT_STREQ(buf, expected);
		game.MoveForwardInPGN();
	}
}

TEST(Test_PrintFen, castling_flag_qside_from_pgn) {
	std::string_view pgn =
	    "[FEN \"Br2k1r1/1b1ppn2/8/pppQ1pPp/PPPq1PP1/8/1B1PPN2/bR2K1R1 b "
	    "KQkq - 0 1\"]"
	    "1... Rg6 2. Rg3 Ra6 3. Ra3 Raxa8 4. Raxa1 O-O-O 5. O-O-O";
	scid::database::Game game;
	scid::database::PgnParseLog parseLog;
	ASSERT_TRUE(scid::database::pgnParseGame(pgn.data(), pgn.size(), game, parseLog));
	game.MoveToStart();

	const char* fens[] = {
	    "Br2k1r1/1b1ppn2/8/pppQ1pPp/PPPq1PP1/8/1B1PPN2/bR2K1R1 b KQkq - 0 1",
	    "Br2k3/1b1ppn2/6r1/pppQ1pPp/PPPq1PP1/8/1B1PPN2/bR2K1R1 w KQq - 1 2",
	    "Br2k3/1b1ppn2/6r1/pppQ1pPp/PPPq1PP1/6R1/1B1PPN2/bR2K3 b Qq - 2 2",
	    "Br2k3/1b1ppn2/r7/pppQ1pPp/PPPq1PP1/6R1/1B1PPN2/bR2K3 w Qq - 3 3",
	    "Br2k3/1b1ppn2/r7/pppQ1pPp/PPPq1PP1/R7/1B1PPN2/bR2K3 b Qq - 4 3",
	    "rr2k3/1b1ppn2/8/pppQ1pPp/PPPq1PP1/R7/1B1PPN2/bR2K3 w Qb - 0 4",
	    "rr2k3/1b1ppn2/8/pppQ1pPp/PPPq1PP1/8/1B1PPN2/RR2K3 b Bb - 0 4",
	    "r1kr4/1b1ppn2/8/pppQ1pPp/PPPq1PP1/8/1B1PPN2/RR2K3 w B - 1 5",
	    "r1kr4/1b1ppn2/8/pppQ1pPp/PPPq1PP1/8/1B1PPN2/R1KR4 b - - 2 5"};
	for (auto expected : fens) {
		char buf[1024];
		game.currentPos()->PrintFEN(buf, sizeof(buf));
		EXPECT_STREQ(buf, expected);
		game.MoveForwardInPGN();
	}
}
