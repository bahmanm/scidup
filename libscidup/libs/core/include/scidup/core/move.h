/** @file
 * Portable move intent.
 */
#pragma once

#include "scidup/core/board.h"

#include <string>

namespace scid::core {

struct MoveSpec {
	scid::core::squareT from = scid::core::NULL_SQUARE;
	scid::core::squareT to = scid::core::NULL_SQUARE;
	scid::core::pieceT promotion = scid::core::EMPTY;
	bool castling = false;

	bool isNull() const;
	std::string longNotation() const;
};

} // namespace scid::core
