#pragma once

#include "scidup/core/movelist.h"

#include <span>
#include <string_view>

namespace scid::database {

enum class GameMoveViewKind {
	InitialComment,
	VariationStart,
	VariationEnd,
	Move
};

struct GameMoveView {
	GameMoveViewKind kind;
	simpleMoveT move;
	std::string_view san;
	std::string_view comment;
	std::span<const byte> nags;
};

} // namespace scid::database
