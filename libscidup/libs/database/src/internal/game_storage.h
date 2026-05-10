#pragma once

#include "scidup/database/bytebuf.h"
#include "scidup/core/position.h"

namespace scid::database::game_storage {

errorT decodeEncodedMove(ByteBuffer& buf, byte val, const Position& pos,
                         simpleMoveT& sm);
errorT decodeMainlineMove(ByteBuffer& buf, const Position& pos,
                          simpleMoveT& sm);

} // namespace scid::database::game_storage
