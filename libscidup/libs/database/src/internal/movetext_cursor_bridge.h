#pragma once

namespace scid::core {
class MovetextCursor;
}

namespace scid::database {

struct moveT;

namespace TEMP_movetext {

bool moveCursorToLegacyLocation(scid::core::MovetextCursor& cursor,
                                const moveT* lineStart,
                                const moveT* target);

} // namespace TEMP_movetext

} // namespace scid::database
