#include "scidup/database/game_info.h"

#include <cctype>

namespace scid::database {

std::uint32_t gameFlagMaskFromChar(char flag) {
	switch (std::toupper(static_cast<unsigned char>(flag))) {
	case 'S': return 1u << GAME_FLAG_START;
	case 'X': return 1u << GAME_FLAG_PROMO;
	case 'Y': return 1u << GAME_FLAG_UPROMO;
	case 'D': return 1u << GAME_FLAG_DELETE;
	case 'W': return 1u << GAME_FLAG_WHITE_OP;
	case 'B': return 1u << GAME_FLAG_BLACK_OP;
	case 'M': return 1u << GAME_FLAG_MIDDLEGAME;
	case 'E': return 1u << GAME_FLAG_ENDGAME;
	case 'N': return 1u << GAME_FLAG_NOVELTY;
	case 'P': return 1u << GAME_FLAG_PAWN;
	case 'T': return 1u << GAME_FLAG_TACTICS;
	case 'K': return 1u << GAME_FLAG_KSIDE;
	case 'Q': return 1u << GAME_FLAG_QSIDE;
	case '!': return 1u << GAME_FLAG_BRILLIANCY;
	case '?': return 1u << GAME_FLAG_BLUNDER;
	case 'U': return 1u << GAME_FLAG_USER;
	case '1': return 1u << GAME_FLAG_CUSTOM1;
	case '2': return 1u << GAME_FLAG_CUSTOM2;
	case '3': return 1u << GAME_FLAG_CUSTOM3;
	case '4': return 1u << GAME_FLAG_CUSTOM4;
	case '5': return 1u << GAME_FLAG_CUSTOM5;
	case '6': return 1u << GAME_FLAG_CUSTOM6;
	default: return 0;
	}
}

scid::core::uint gameFlagIndexFromChar(char flag) {
	switch (std::toupper(static_cast<unsigned char>(flag))) {
	case 'D': return GAME_FLAG_DELETE;
	case 'W': return GAME_FLAG_WHITE_OP;
	case 'B': return GAME_FLAG_BLACK_OP;
	case 'M': return GAME_FLAG_MIDDLEGAME;
	case 'E': return GAME_FLAG_ENDGAME;
	case 'N': return GAME_FLAG_NOVELTY;
	case 'P': return GAME_FLAG_PAWN;
	case 'T': return GAME_FLAG_TACTICS;
	case 'K': return GAME_FLAG_KSIDE;
	case 'Q': return GAME_FLAG_QSIDE;
	case '!': return GAME_FLAG_BRILLIANCY;
	case '?': return GAME_FLAG_BLUNDER;
	case 'U': return GAME_FLAG_USER;
	case '1': return GAME_FLAG_CUSTOM1;
	case '2': return GAME_FLAG_CUSTOM2;
	case '3': return GAME_FLAG_CUSTOM3;
	case '4': return GAME_FLAG_CUSTOM4;
	case '5': return GAME_FLAG_CUSTOM5;
	case '6': return GAME_FLAG_CUSTOM6;
	default: return 0;
	}
}

std::uint32_t gameFlagMaskFromString(const char* flags) {
	std::uint32_t res = 0;
	while (flags && *flags != '\0') {
		res |= gameFlagMaskFromChar(*flags++);
	}
	return res;
}

scid::core::byte GameInfo::rating() const {
	auto value = (whiteElo != 0 && blackElo != 0) ? (whiteElo + blackElo) / 140 : 0;
	static_assert(std::is_signed_v<decltype(value)>);

	if (commentCount > 2 || nagCount > 2) {
		if (value < 21) {
			value = 38;
		} else {
			value += 6;
		}
	}

	if (result == scid::core::RESULT_Draw) {
		scid::core::uint moves = halfMoveCount;
		if (moves < 80) {
			value -= 3;
			if (moves < 60) {
				value -= 2;
				if (moves < 40)
					value -= 2;
			}
		}
	}

	return (value < 0) ? 0 : static_cast<scid::core::byte>(value);
}

scid::core::uint GameInfo::flagString(char* dest, const char* flagChars) const {
	if (!flagChars)
		flagChars = "DWBMENPTKQ!?U123456";

	scid::core::uint count = 0;
	while (*flagChars != '\0') {
		const auto mask = gameFlagMaskFromChar(*flagChars);
		if (mask != 0 && hasFlag(mask)) {
			*dest++ = *flagChars;
			++count;
		}
		++flagChars;
	}
	*dest = '\0';
	return count;
}

} // namespace scid::database
