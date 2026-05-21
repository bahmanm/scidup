#ifndef SCIDUP_APP_EDITOR_H
#define SCIDUP_APP_EDITOR_H

// ScidUp application state. This is intentionally not part of the database
// library boundary: ScidUp owns the current editing session, dirty state,
// undo/redo, push/pop state, core Game, and application/database Scid flags.

#include "scidup/core/game.h"
#include "scidup/core/game_cursor.h"
#include "scidup/core/movetext_location.h"
#include "scidup/database/game_id.h"
#include "scidup/database/scidbase.h"
#include "scidup_app_undo_redo.h"
#include <algorithm>
#include <array>
#include <memory>
#include <optional>
#include <string_view>
#include <unordered_map>

namespace scidup::app::editor {

class EditableGame {
	scid::core::Game coreGame_;
	std::array<char, 22> scidFlags_{};

public:
	scid::core::Game& coreGame() { return coreGame_; }
	const scid::core::Game& coreGame() const { return coreGame_; }

	void clear() {
		coreGame_.clear();
		scidFlags_[0] = 0;
	}

	void setScidFlags(const char* flags, std::size_t len) {
		scidFlags_.fill(0);
		std::copy_n(flags, std::min(scidFlags_.size() - 1, len),
		            scidFlags_.data());
	}

	const char* scidFlags() const { return scidFlags_.data(); }
	char* scidFlagsData() { return scidFlags_.data(); }
	std::size_t scidFlagsCapacity() const { return scidFlags_.size(); }

	EditableGame* clone() const { return new EditableGame(*this); }
};

struct GameSnapshot {
	std::unique_ptr<EditableGame> game;
	scid::core::MovetextLocation location;

	GameSnapshot() : game(std::make_unique<EditableGame>()), location() {}
	GameSnapshot(EditableGame* game, scid::core::MovetextLocation location)
	    : game(game), location(location) {}

	GameSnapshot* clone() const { return new GameSnapshot(game->clone(), location); }
};

struct PushPopState {
	EditableGame* game = nullptr;
	bool dirty = false;
	scid::core::MovetextLocation location;
};

struct State {
	std::unique_ptr<EditableGame> game = std::make_unique<EditableGame>();
	scid::core::MovetextLocation location;
	std::optional<scid::database::gamenumT> loadedGameId;
	bool dirty = false;
	scidup::app::UndoRedo<GameSnapshot, 100> history;
	PushPopState deprecatedPushPop;

	~State() { delete deprecatedPushPop.game; }

	void reset() {
		game = std::make_unique<EditableGame>();
		location = {};
		loadedGameId.reset();
		dirty = false;
		history.clear();
		delete deprecatedPushPop.game;
		deprecatedPushPop = {};
	}
};

namespace detail {
inline auto& states() {
	static std::unordered_map<scid::database::scidBaseT*, std::unique_ptr<State>> perBaseStates;
	return perBaseStates;
}

inline State& stateFor(scid::database::scidBaseT& base) {
	auto& allStates = states();
	auto& slot = allStates[&base];
	if (!slot)
		slot = std::make_unique<State>();
	return *slot;
}
} // namespace detail

inline void reset(scid::database::scidBaseT& base) { detail::stateFor(base).reset(); }

inline void release(scid::database::scidBaseT& base) { detail::states().erase(&base); }

class GameSession {
public:
	explicit GameSession(scid::database::scidBaseT& base) : base_(&base) {}

	EditableGame& game() const { return *state().game; }
	scid::core::MovetextLocation location() const { return state().location; }
	void setLocation(scid::core::MovetextLocation location) const {
		state().location = location;
	}

	std::optional<scid::database::gamenumT> loadedGameId() const { return state().loadedGameId; }

	const scid::database::IndexEntry* loadedIndexEntry() const {
		const auto gameId = loadedGameId();
		return gameId ? base_->getIndexEntry(*gameId) : nullptr;
	}

	void setLoadedGameId(std::optional<scid::database::gamenumT> gameId) const {
		state().loadedGameId = gameId;
	}

	bool matchesLoadedGame(scid::database::gamenumT gameId) const {
		const auto loaded = loadedGameId();
		return loaded && *loaded == gameId;
	}

	bool isDirty() const { return state().dirty; }
	void setDirty(bool dirty = true) const { state().dirty = dirty; }

	void clearHistory() const { state().history.clear(); }
	size_t undoSize() const { return state().history.undoSize(); }
	size_t redoSize() const { return state().history.redoSize(); }
	void storeUndoPoint() const {
		auto& s = state();
		GameSnapshot current{s.game->clone(), s.location};
		s.history.store(&current);
	}
	void undo() const {
		auto& s = state();
		auto current = new GameSnapshot{s.game.release(), s.location};
		auto restored = s.history.undo(current);
		s.game.reset(restored->game.release());
		s.location = restored->location;
		delete restored;
	}
	void redo() const {
		auto& s = state();
		auto current = new GameSnapshot{s.game.release(), s.location};
		auto restored = s.history.redo(current);
		s.game.reset(restored->game.release());
		s.location = restored->location;
		delete restored;
	}

	void resetToNewGame() const { state().reset(); }

	void replace(EditableGame* game, scid::core::MovetextLocation location,
	             std::optional<scid::database::gamenumT> gameId,
	             bool dirty) const {
		auto& s = state();
		s.game.reset(game);
		s.loadedGameId = gameId;
		s.dirty = dirty;
		s.location = location;
		s.history.clear();
		delete s.deprecatedPushPop.game;
		s.deprecatedPushPop = {};
	}

	scid::core::errorT load(scid::database::gamenumT gameId) const {
		auto& s = state();
		s.history.clear();
		const auto err = base_->loadGame(gameId, s.game->coreGame(),
		                                 s.game->scidFlagsData(),
		                                 s.game->scidFlagsCapacity());
		if (err != scid::core::OK)
			return err;

		if (base_->defaultFilterGet(gameId) > 0) {
			scid::core::GameCursor cursor(s.game->coreGame());
			if (!cursor.toPly(base_->defaultFilterGet(gameId) - 1))
				cursor.toEnd();
			s.location = cursor.location();
		} else {
			s.location = {};
		}
		s.loadedGameId = gameId;
		s.dirty = false;
		return scid::core::OK;
	}

	scid::core::errorT undoAll() const {
		auto& s = state();
		s.dirty = false;
		s.history.clear();
		if (!s.loadedGameId) {
			s.game->clear();
			s.location = {};
			return scid::core::OK;
		}

		const auto err = base_->loadGame(*s.loadedGameId, s.game->coreGame(),
		                                 s.game->scidFlagsData(),
		                                 s.game->scidFlagsCapacity());
		if (err != scid::core::OK)
			return err;
		s.location = {};
		return scid::core::OK;
	}

	void push(bool copy) const {
		auto& s = state();
		EditableGame* next = copy ? s.game->clone() : new EditableGame;
		if (s.deprecatedPushPop.game) {
			delete s.deprecatedPushPop.game;
		}
		s.deprecatedPushPop = {s.game.release(), s.dirty, s.location};
		s.game.reset(next);
		s.location = {};
		s.dirty = false;
	}

	void pop() const {
		auto& s = state();
		if (!s.deprecatedPushPop.game)
			return;

		s.game.reset(s.deprecatedPushPop.game);
		s.location = s.deprecatedPushPop.location;
		s.dirty = s.deprecatedPushPop.dirty;
		s.deprecatedPushPop = {};
	}

private:
	State& state() const { return detail::stateFor(*base_); }
	scid::database::scidBaseT* base_;
};

inline GameSession gameSession(scid::database::scidBaseT& base) { return GameSession(base); }

} // namespace scidup::app::editor

#endif
