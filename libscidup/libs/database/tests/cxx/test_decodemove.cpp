#include "scidup/database/bytebuf.h"
#include "scidup/database/game.h"
#include "scidup/database/game_TEMP/notation.h"
#include "scidup/database/game_TEMP/storage.h"
#include "scidup/database/game_TEMP/pgnparse.h"

#include <gtest/gtest.h>
#include <string_view>
#include <vector>

namespace {

void expect_roundtrip(std::string_view pgn) {
	SCOPED_TRACE(std::string(pgn));
	scid::database::Game original;
	scid::database::PgnParseLog parseLog;
	ASSERT_TRUE(scid::database::pgnParseGame(pgn.data(), pgn.size(), original,
	                                         parseLog));

	std::vector<scid::database::byte> encoded;
	scid::database::game_storage::encode(original, encoded);

	scid::database::ByteBuffer bbuf(encoded.data(), encoded.size());
	scid::database::Game decoded;
	ASSERT_EQ(scid::database::OK,
	          scid::database::game_storage::decodeMovesOnly(decoded, bbuf));

	original.toStart();
	decoded.toStart();

	for (;;) {
		char originalFen[256];
		char decodedFen[256];
		original.currentPos()->PrintFEN(originalFen, sizeof(originalFen));
		decoded.currentPos()->PrintFEN(decodedFen, sizeof(decodedFen));
		EXPECT_STREQ(originalFen, decodedFen);
		EXPECT_EQ(scid::database::game_notation::nextSan(original),
		          scid::database::game_notation::nextSan(decoded));

		const auto originalErr = original.next();
		const auto decodedErr = decoded.next();
		EXPECT_EQ(originalErr, decodedErr);
		if (originalErr != scid::database::OK)
			break;
	}
}

} // namespace

TEST(Test_DecodeMove, representative_encoded_moves_roundtrip) {
	expect_roundtrip(
	    "1.e4 e5 2.Nf3 Nc6 3.Bb5 a6 4.Ba4 Nf6 5.O-O Be7 6.Re1 b5 "
	    "7.Bb3 d6 8.c3 O-O 9.h3");
	expect_roundtrip(
	    "1.d4 d5 2.Qd3 Nf6 3.Qb5+ c6 4.Qa4");
	expect_roundtrip(
	    "[FEN \"6k1/P7/8/8/8/8/6K1/8 w - - 0 1\"] "
	    "1.a8=Q+ Kh7 2.Qf8");
	expect_roundtrip("1.e4 -- 2.e5 Nc6");
}
