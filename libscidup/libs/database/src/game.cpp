//////////////////////////////////////////////////////////////////////
//
//  FILE:       game.cpp
//              Game class methods
//
//  Part of:    Scid (Shane's Chess Information Database)
//  Version:    3.5
//
//  Notice:     Copyright (c) 2000-2003  Shane Hudson.  All rights reserved.
//
//  Author:     Shane Hudson (sgh@users.sourceforge.net)
//
//////////////////////////////////////////////////////////////////////

#include "scidup/database/game.h"
#include "scidup/database/common.h"
#include <algorithm>
#include <cstring>

namespace scid::database {

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Game::FindExtraTag():
//   Finds and returns an extra PGN tag if it
//   exists, or NULL if it does not exist.
const char* Game::FindExtraTag(const char* tag) const {
    for (auto& e : extraTags_) {
        if (e.first == tag)
            return e.second.c_str();
    }
    return NULL;
}

std::string_view Game::GetResultStr() const {
	using namespace std::literals;
	static std::string_view res[] = {"*"sv, "1-0"sv, "0-1"sv, "1/2-1/2"sv};
	return res[Result];
}

int Game::setRating(colorT col, const char* ratingType, size_t ratingTypeLen,
                    std::pair<const char*, const char*> rating) {
	auto begin = ratingTypeNames;
	const size_t ratingSz = 7;
	auto it = std::find_if(begin, begin + ratingSz, [&](auto rType) {
		return std::equal(ratingType, ratingType + ratingTypeLen, rType,
		                  rType + std::strlen(rType));
	});
	auto rType = static_cast<ratingTypeT>(std::distance(begin, it));
	if (rType >= ratingSz)
		return -1;

	int res = 1;
	auto elo = strGetUnsigned(std::string{rating.first, rating.second}.c_str());
	if (elo > MAX_ELO) {
		elo = 0;
		res = 0;
	}
	if (col == WHITE) {
		SetWhiteElo(static_cast<ratingT>(elo));
		SetWhiteRatingType(rType);
	} else {
		SetBlackElo(static_cast<ratingT>(elo));
		SetBlackRatingType(rType);
	}
	return res;
}

ratingT
Game::GetAverageElo () {
	auto white = WhiteElo;
	auto black = BlackElo;
	return (white == 0 || black == 0) ? 0 : (white + black) / 2;
}

//////////////////////////////////////////////////////////////////////
//  EOF:    game.cpp
//////////////////////////////////////////////////////////////////////

} // namespace scid::database
