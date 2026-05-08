#pragma once

#include "scidup/core/movelist.h"

#include <span>
#include <string_view>

namespace scid::core::pgn {

enum class MovetextEntryKind {
	InitialComment,
	VariationStart,
	VariationEnd,
	Move
};

struct MovetextEntry {
	MovetextEntryKind kind;
	scid::database::simpleMoveT move;
	std::string_view san;
	std::string_view comment;
	std::span<const scid::database::byte> nags;
};

} // namespace scid::core::pgn
