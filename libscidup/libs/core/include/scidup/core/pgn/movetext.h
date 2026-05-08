#pragma once

#include "scidup/core/primitives.h"

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
	std::string_view san;
	std::string_view comment;
	std::span<const scid::database::byte> nags;
};

} // namespace scid::core::pgn
