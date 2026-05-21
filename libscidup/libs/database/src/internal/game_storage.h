#pragma once

#include "bytebuf.h"
#include "scidup/core/position.h"
#include "scidup/database/common.h"
#include "scidup/database/indexentry.h"
#include "scidup/database/namebase.h"

#include <cstddef>
#include <utility>
#include <vector>

namespace scid::core {
class Game;
struct MoveAction;
} // namespace scid::core

namespace scid::database::game_storage {

std::pair<IndexEntry, TagRoster> encode(const scid::core::Game& game,
                                        const char* scidFlags,
                                        std::vector<scid::core::byte>& dest);
void loadStandardTags(scid::core::Game& game, char* scidFlags,
                      std::size_t scidFlagsLen, IndexEntry const& ie,
                      TagRoster const& tags);
scid::core::errorT decode(scid::core::Game& game, char* scidFlags,
              std::size_t scidFlagsLen, IndexEntry const& ie,
              TagRoster const& tags, ByteBuffer buf);
scid::core::errorT decodeMovesOnly(scid::core::Game& game, ByteBuffer& buf);

struct ByteBufferAccess {
	template <typename MoveFn, typename CommentFn, typename VariationFn,
	          typename NagFn>
	static std::pair<scid::core::errorT, unsigned char>
	nextMove(ByteBuffer& buf, int varDepth, MoveFn acceptMove,
	         CommentFn commentMarker, VariationFn changeVar, NagFn addNag) {
		return buf.nextMove(varDepth, acceptMove, commentMarker, changeVar,
		                    addNag);
	}

	static std::pair<scid::core::errorT, unsigned char> nextLineMove(ByteBuffer& buf) {
		return buf.nextLineMove();
	}

	static std::pair<int, scid::core::pieceT> decodeMove(ByteBuffer& buf, scid::core::colorT toMove,
	                                         scid::core::pieceT movingPiece, scid::core::squareT from,
	                                         unsigned char moveCode) {
		return buf.decodeMove(toMove, movingPiece, from, moveCode);
	}
};

scid::core::errorT decodeEncodedMove(ByteBuffer& buf, scid::core::byte val,
                                      const scid::core::Position& pos,
                                      scid::core::MoveAction& action);
scid::core::errorT decodeMainlineMove(ByteBuffer& buf,
                                      const scid::core::Position& pos,
                                      scid::core::MoveAction& action);

} // namespace scid::database::game_storage
