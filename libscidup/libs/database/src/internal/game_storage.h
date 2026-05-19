#pragma once

#include "scidup/database/bytebuf.h"
#include "scidup/core/position.h"
#include "scidup/database/common.h"

#include <utility>
#include <vector>

namespace scid::database {
class Game;
class IndexEntry;
struct TagRoster;
} // namespace scid::database

namespace scid::database::game_storage {

std::pair<IndexEntry, TagRoster> encode(const Game& game,
                                        std::vector<byte>& dest);
void loadStandardTags(Game& game, IndexEntry const& ie, TagRoster const& tags);
errorT decode(Game& game, IndexEntry const& ie, TagRoster const& tags,
              ByteBuffer buf);
errorT decodeMovesOnly(Game& game, ByteBuffer& buf);

struct ByteBufferAccess {
	template <typename MoveFn, typename CommentFn, typename VariationFn,
	          typename NagFn>
	static std::pair<errorT, unsigned char>
	nextMove(ByteBuffer& buf, int varDepth, MoveFn acceptMove,
	         CommentFn commentMarker, VariationFn changeVar, NagFn addNag) {
		return buf.nextMove(varDepth, acceptMove, commentMarker, changeVar,
		                    addNag);
	}

	static std::pair<errorT, unsigned char> nextLineMove(ByteBuffer& buf) {
		return buf.nextLineMove();
	}

	static std::pair<int, pieceT> decodeMove(ByteBuffer& buf, colorT toMove,
	                                         pieceT movingPiece, squareT from,
	                                         unsigned char moveCode) {
		return buf.decodeMove(toMove, movingPiece, from, moveCode);
	}
};

errorT decodeEncodedMove(ByteBuffer& buf, byte val, const Position& pos,
                         simpleMoveT& sm);
errorT decodeMainlineMove(ByteBuffer& buf, const Position& pos,
                          simpleMoveT& sm);

} // namespace scid::database::game_storage
