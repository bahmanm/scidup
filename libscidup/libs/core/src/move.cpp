#include "scidup/core/move.h"

namespace scid::core {

bool MoveSpec::isNull() const {
	return from == to && !castling;
}

std::string MoveSpec::longNotation() const {
	char buf[8] = {};
	char* dest = buf;
	if (from == to) {
		// UCI standard for null move.
		*dest++ = '0';
		*dest++ = '0';
		*dest++ = '0';
		*dest++ = '0';
	} else {
		*dest++ = square_FyleChar(from);
		*dest++ = square_RankChar(from);
		*dest++ = square_FyleChar(to);
		*dest++ = square_RankChar(to);
		if (promotion != EMPTY) {
			constexpr const char promoChars[] = "  qrbn ";
			*dest++ = promoChars[piece_Type(promotion)];
		}
	}
	return {buf, static_cast<std::size_t>(dest - buf)};
}

} // namespace scid::core
