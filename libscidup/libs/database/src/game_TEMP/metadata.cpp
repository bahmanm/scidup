#include "scidup/database/game.h"

#include <string>
#include <utility>
#include <vector>

namespace scid::database {

std::string& Game::addTag(std::string_view tag, std::string_view value) {
	return coreGame_.addTag(tag, value);
}

std::string& Game::find_or_create_tag(std::string_view tag) {
	return coreGame_.findOrCreateTag(tag);
}

std::string& Game::assignTagValue(std::string_view tag,
                                  std::string_view value) {
	auto& dest = find_or_create_tag(tag);
	dest.assign(value.begin(), value.end());
	return dest;
}

void Game::RemoveExtraTag(std::string_view tag) {
	coreGame_.removeExtraTag(tag);
}

void Game::SetWhiteElo(ratingT elo) {
	auto rating = coreGame_.white().rating;
	rating.value = elo;
	coreGame_.setWhiteRating(rating);
}

void Game::SetBlackElo(ratingT elo) {
	auto rating = coreGame_.black().rating;
	rating.value = elo;
	coreGame_.setBlackRating(rating);
}

void Game::SetWhiteRatingType(ratingTypeT b) {
	auto rating = coreGame_.white().rating;
	rating.type = b > 7 ? 0 : b;
	coreGame_.setWhiteRating(rating);
}

void Game::SetBlackRatingType(ratingTypeT b) {
	auto rating = coreGame_.black().rating;
	rating.type = b > 7 ? 0 : b;
	coreGame_.setBlackRating(rating);
}

void Game::SetEco(scidup::eco::Code eco) {
	EcoCode = eco;
	if (eco == scidup::eco::ECO_None) {
		coreGame_.setEco({});
		return;
	}

	char ecoStr[sizeof(scidup::eco::String)] = {};
	scidup::eco::toExtendedString(eco, ecoStr);
	coreGame_.setEco(ecoStr);
}

} // namespace scid::database
