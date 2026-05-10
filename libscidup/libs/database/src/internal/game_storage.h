#pragma once

#include "scidup/database/bytebuf.h"
#include "scidup/core/position.h"

namespace scid::database::game_storage {

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
