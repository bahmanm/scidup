#pragma once

namespace scid::core {
class MovetextCursor;
}

namespace scid::database {

struct moveT;

namespace legacy_movetext {

bool moveCursorToLegacyLocation(scid::core::MovetextCursor& cursor,
                                const moveT* lineStart,
                                const moveT* target);

} // namespace legacy_movetext

} // namespace scid::database
