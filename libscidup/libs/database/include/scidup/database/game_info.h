#pragma once

#include "scidup/core/date.h"
#include "scidup/core/game_result.h"
#include "scidup/core/rating.h"
#include "scidup/database/common.h"
#include "scidup/database/game_id.h"
#include "scidup/database/matsig.h"
#include <array>
#include <cstdint>
#include <optional>

namespace scid::database {

inline constexpr std::uint32_t GAME_FLAG_MASK_ALL = 0xffffffff;

enum gameFlagT : std::uint32_t {
	GAME_FLAG_START = 0,
	GAME_FLAG_PROMO,
	GAME_FLAG_UPROMO,
	GAME_FLAG_DELETE,
	GAME_FLAG_WHITE_OP,
	GAME_FLAG_BLACK_OP,
	GAME_FLAG_MIDDLEGAME,
	GAME_FLAG_ENDGAME,
	GAME_FLAG_NOVELTY,
	GAME_FLAG_PAWN,
	GAME_FLAG_TACTICS,
	GAME_FLAG_KSIDE,
	GAME_FLAG_QSIDE,
	GAME_FLAG_BRILLIANCY,
	GAME_FLAG_BLUNDER,
	GAME_FLAG_USER,
	GAME_FLAG_CUSTOM1,
	GAME_FLAG_CUSTOM2,
	GAME_FLAG_CUSTOM3,
	GAME_FLAG_CUSTOM4,
	GAME_FLAG_CUSTOM5,
	GAME_FLAG_CUSTOM6,
	GAME_FLAG_COUNT,
};

std::uint32_t gameFlagMaskFromChar(char flag);
scid::core::uint gameFlagIndexFromChar(char flag);
std::uint32_t gameFlagMaskFromString(const char* flags);

struct GameInfo {
	std::uint64_t offset = 0;
	std::uint32_t length = 0;
	idNumberT white = 0;
	idNumberT black = 0;
	idNumberT event = 0;
	idNumberT site = 0;
	idNumberT round = 0;
	scid::core::ratingT whiteElo = 0;
	scid::core::ratingT blackElo = 0;
	scid::core::ratingTypeT whiteRatingType = 0;
	scid::core::ratingTypeT blackRatingType = 0;
	scid::core::dateT date = scid::core::ZERO_DATE;
	scid::core::dateT eventDate = scid::core::ZERO_DATE;
	scid::core::resultT result = scid::core::RESULT_None;
	scid::core::uint variationCount = 0;
	scid::core::uint commentCount = 0;
	scid::core::uint nagCount = 0;
	std::uint16_t halfMoveCount = 0;
	matSigT finalMaterial = 0;
	scid::core::byte storedLineCode = 0;
	EcoCode ecoCode = ECO_CODE_NONE;
	std::uint32_t flags = 0;
	std::array<scid::core::byte, 9> homePawnData = {};
	bool chessStd = true;

	bool hasFlag(std::uint32_t mask) const { return (flags & mask) == mask; }
	bool hasStartFlag() const { return hasFlag(1u << GAME_FLAG_START); }
	bool hasPromotionsFlag() const { return hasFlag(1u << GAME_FLAG_PROMO); }
	bool hasUnderPromoFlag() const { return hasFlag(1u << GAME_FLAG_UPROMO); }
	bool hasDeleteFlag() const { return hasFlag(1u << GAME_FLAG_DELETE); }
	bool hasComments() const { return commentCount > 0; }
	bool hasVariations() const { return variationCount > 0; }
	scid::core::uint year() const { return scid::core::date_GetYear(date); }
	scid::core::uint month() const { return scid::core::date_GetMonth(date); }
	scid::core::uint day() const { return scid::core::date_GetDay(date); }
	scid::core::byte rating() const;
	scid::core::uint flagString(char* dest, const char* flags) const;
};

struct GameInfoUpdate {
	std::optional<scid::core::dateT> date;
	std::optional<idNumberT> event;
	std::optional<idNumberT> round;
	std::optional<scid::core::ratingT> whiteElo;
	std::optional<scid::core::ratingT> blackElo;
	std::optional<EcoCode> ecoCode;

	bool empty() const {
		return !date && !event && !round && !whiteElo && !blackElo &&
		       !ecoCode;
	}
};

} // namespace scid::database
