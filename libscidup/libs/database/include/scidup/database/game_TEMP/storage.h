#pragma once

#include "scidup/database/game.h"

namespace scid::database::game_storage {

std::pair<IndexEntry, TagRoster> encode(const Game& game,
                                        std::vector<byte>& dest);
void loadStandardTags(Game& game, IndexEntry const& ie, TagRoster const& tags);
errorT decode(Game& game, IndexEntry const& ie, TagRoster const& tags,
              ByteBuffer buf);
errorT decodeMovesOnly(Game& game, ByteBuffer& buf);
errorT decodeSkipTags(Game& game, ByteBuffer* buf);
errorT decodeNextMove(Game& game, ByteBuffer* buf, simpleMoveT& sm);

} // namespace scid::database::game_storage
