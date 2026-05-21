#include "bytebuf.h"
#include "scidup/core/game.h"
#include "scidup/core/game_cursor.h"
#include "scidup/core/notation.h"
#include "scidup/core/pgn/decode.h"
#include "game_storage.h"

#include <gtest/gtest.h>
#include <string>
#include <string_view>
#include <vector>

namespace {

std::string currentFen(const scid::core::Game& game,
                       scid::core::MovetextLocation location) {
	scid::core::GameCursor cursor(game);
	EXPECT_TRUE(cursor.restore(location));
	auto position = cursor.currentPosition();
	EXPECT_TRUE(position.has_value());
	if (!position)
		return {};

	char buf[256];
	position->PrintFEN(buf, sizeof(buf));
	return buf;
}

void expect_roundtrip(std::string_view pgn) {
	SCOPED_TRACE(std::string(pgn));
	scid::core::Game original;
	scid::core::MovetextLocation originalLocation;
	scid::core::pgn::ParseLog parseLog;
	ASSERT_TRUE(scid::core::pgn::parseGame(pgn.data(), pgn.size(), original,
	                                         originalLocation,
	                                         parseLog));

	std::vector<scid::core::byte> encoded;
	scid::database::game_storage::encode(original,
	                                     "", encoded);

	scid::database::ByteBuffer bbuf(encoded.data(), encoded.size());
	scid::core::Game decoded;
	decoded.clear();
	ASSERT_EQ(scid::core::OK,
	          scid::database::game_storage::decodeMovesOnly(decoded,
	                                                        bbuf));

	originalLocation = {};
	scid::core::MovetextLocation decodedLocation;

	for (;;) {
		EXPECT_EQ(currentFen(original, originalLocation),
		          currentFen(decoded, decodedLocation));
		EXPECT_EQ(scid::core::notation::nextSan(original,
		                                        originalLocation),
		          scid::core::notation::nextSan(decoded,
		                                        decodedLocation));

		scid::core::GameCursor originalCursor(original);
		EXPECT_TRUE(originalCursor.restore(originalLocation));
		scid::core::GameCursor decodedCursor(decoded);
		EXPECT_TRUE(decodedCursor.restore(decodedLocation));
		const auto originalErr = originalCursor.next()
		                             ? scid::core::OK
		                             : scid::core::ERROR_EndOfMoveList;
		const auto decodedErr = decodedCursor.next()
		                            ? scid::core::OK
		                            : scid::core::ERROR_EndOfMoveList;
		EXPECT_EQ(originalErr, decodedErr);
		if (originalErr != scid::core::OK)
			break;
		originalLocation = originalCursor.location();
		decodedLocation = decodedCursor.location();
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
