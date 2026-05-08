#include "scidup/core/game.h"

#include <algorithm>
#include <utility>

namespace scid::core {

Game::Game() {
	clear();
}

void Game::clear() {
	event_.clear();
	site_.clear();
	round_.clear();
	white_ = {};
	black_ = {};
	date_ = scid::database::ZERO_DATE;
	eventDate_ = scid::database::ZERO_DATE;
	result_ = scid::database::RESULT_None;
	extraTags_.clear();
	startPosition_.reset();
}

const std::string& Game::event() const {
	return event_;
}

const std::string& Game::site() const {
	return site_;
}

const std::string& Game::round() const {
	return round_;
}

const Player& Game::white() const {
	return white_;
}

const Player& Game::black() const {
	return black_;
}

scid::database::dateT Game::date() const {
	return date_;
}

scid::database::dateT Game::eventDate() const {
	return eventDate_;
}

scid::database::resultT Game::result() const {
	return result_;
}

void Game::setEvent(std::string_view value) {
	event_.assign(value.begin(), value.end());
}

void Game::setSite(std::string_view value) {
	site_.assign(value.begin(), value.end());
}

void Game::setRound(std::string_view value) {
	round_.assign(value.begin(), value.end());
}

void Game::setWhite(Player value) {
	white_ = std::move(value);
}

void Game::setBlack(Player value) {
	black_ = std::move(value);
}

void Game::setDate(scid::database::dateT value) {
	date_ = value;
}

void Game::setEventDate(scid::database::dateT value) {
	eventDate_ = value;
}

void Game::setResult(scid::database::resultT value) {
	result_ = value;
}

std::string* Game::findStandardTag(std::string_view tag) {
	if (tag.size() == 5) {
		if (tag == "Event")
			return &event_;
		if (tag == "Round")
			return &round_;
		if (tag == "White")
			return &white_.name;
		if (tag == "Black")
			return &black_.name;
	} else if (tag.size() == 4) {
		if (tag == "Site")
			return &site_;
	}
	return nullptr;
}

std::string& Game::addTag(std::string_view tag, std::string_view value) {
	if (auto standard = findStandardTag(tag)) {
		standard->assign(value.begin(), value.end());
		return *standard;
	}

	return extraTags_.emplace_back(std::string(tag), std::string(value)).second;
}

const std::vector<std::pair<std::string, std::string>>& Game::extraTags() const {
	return extraTags_;
}

const std::string* Game::findExtraTag(std::string_view tag) const {
	for (auto const& entry : extraTags_) {
		if (entry.first == tag)
			return &entry.second;
	}
	return nullptr;
}

void Game::clearExtraTags() {
	extraTags_.clear();
}

void Game::removeExtraTag(std::string_view tag) {
	extraTags_.erase(
	    std::remove_if(extraTags_.begin(), extraTags_.end(),
	                   [&](auto const& entry) { return entry.first == tag; }),
	    extraTags_.end());
}

bool Game::hasNonStandardStart() const {
	return startPosition_.has_value();
}

bool Game::hasNonStandardStart(char* outFen, std::size_t outFenLen) const {
	if (!startPosition_)
		return false;

	if (outFen && outFenLen)
		startPosition_->PrintFEN(outFen, outFenLen);
	return true;
}

const scid::database::Position* Game::startPosition() const {
	return startPosition_ ? &*startPosition_ : nullptr;
}

scid::database::errorT Game::setStartFen(const char* fen) {
	scid::database::Position position;
	if (auto err = position.ReadFromFEN(fen))
		return err;

	setStartPosition(position);
	return scid::database::OK;
}

void Game::setStartPosition(const scid::database::Position& position) {
	startPosition_ = position;
}

void Game::clearStartPosition() {
	startPosition_.reset();
}

long long Game::initialPlyCounter() const {
	return startPosition_ ? startPosition_->GetPlyCounter() : 0;
}

} // namespace scid::core
