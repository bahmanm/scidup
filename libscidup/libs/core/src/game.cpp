#include "scidup/core/game.h"

#include <algorithm>
#include <utility>

namespace scid::core {

Game::Game() {
	clear();
}

void Game::clear() {
	header_ = {};
	startPosition_.reset();
}

const GameHeader& Game::header() const {
	return header_;
}

const std::string& Game::event() const {
	return header_.event.name;
}

const std::string& Game::site() const {
	return header_.event.site;
}

const std::string& Game::round() const {
	return header_.event.round;
}

const Player& Game::white() const {
	return header_.white;
}

const Player& Game::black() const {
	return header_.black;
}

scid::database::dateT Game::date() const {
	return header_.event.date;
}

scid::database::dateT Game::eventDate() const {
	return header_.event.eventDate;
}

scid::database::resultT Game::result() const {
	return header_.result;
}

void Game::setEvent(std::string_view value) {
	header_.event.name.assign(value.begin(), value.end());
}

void Game::setSite(std::string_view value) {
	header_.event.site.assign(value.begin(), value.end());
}

void Game::setRound(std::string_view value) {
	header_.event.round.assign(value.begin(), value.end());
}

void Game::setWhite(Player value) {
	header_.white = std::move(value);
}

void Game::setBlack(Player value) {
	header_.black = std::move(value);
}

void Game::setDate(scid::database::dateT value) {
	header_.event.date = value;
}

void Game::setEventDate(scid::database::dateT value) {
	header_.event.eventDate = value;
}

void Game::setResult(scid::database::resultT value) {
	header_.result = value;
}

std::string* Game::findStandardTag(std::string_view tag) {
	if (tag.size() == 5) {
		if (tag == "Event")
			return &header_.event.name;
		if (tag == "Round")
			return &header_.event.round;
		if (tag == "White")
			return &header_.white.name;
		if (tag == "Black")
			return &header_.black.name;
	} else if (tag.size() == 4) {
		if (tag == "Site")
			return &header_.event.site;
	}
	return nullptr;
}

std::string& Game::addTag(std::string_view tag, std::string_view value) {
	if (auto standard = findStandardTag(tag)) {
		standard->assign(value.begin(), value.end());
		return *standard;
	}

	return header_.tags.emplace_back(std::string(tag), std::string(value)).second;
}

std::string& Game::findOrCreateTag(std::string_view tag) {
	if (auto standard = findStandardTag(tag))
		return *standard;

	auto it = std::find_if(header_.tags.begin(), header_.tags.end(),
	                       [&](auto const& entry) { return entry.first == tag; });
	if (it != header_.tags.end())
		return it->second;

	return header_.tags.emplace_back(std::string(tag), std::string()).second;
}

const std::vector<TagPair>& Game::extraTags() const {
	return header_.tags;
}

const std::string* Game::findExtraTag(std::string_view tag) const {
	for (auto const& entry : header_.tags) {
		if (entry.first == tag)
			return &entry.second;
	}
	return nullptr;
}

void Game::clearExtraTags() {
	header_.tags.clear();
}

void Game::removeExtraTag(std::string_view tag) {
	header_.tags.erase(
	    std::remove_if(header_.tags.begin(), header_.tags.end(),
	                   [&](auto const& entry) { return entry.first == tag; }),
	    header_.tags.end());
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

scid::database::Position* Game::startPosition() {
	return startPosition_ ? &*startPosition_ : nullptr;
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
