#ifndef SCIDUP_APP_EDITOR_H
#define SCIDUP_APP_EDITOR_H

// ScidUp application state. This is intentionally not part of the database
// library boundary: the database loads and saves Game objects, while ScidUp
// owns the current editing session, dirty state, undo/redo, and push/pop state.

#include "scidup/database/game_id.h"
#include "scidup/database/scidbase.h"
#include "scidup_app_undo_redo.h"
#include <memory>
#include <optional>
#include <unordered_map>

namespace scidup::app::editor {

struct State {
	std::unique_ptr<scid::database::Game> game = std::make_unique<scid::database::Game>();
	std::optional<scid::database::gamenumT> loadedGameId;
	bool dirty = false;
	scidup::app::UndoRedo<scid::database::Game, 100> history;
	std::pair<scid::database::Game*, bool> deprecatedPushPop{nullptr, false};

	~State() { delete deprecatedPushPop.first; }

	void reset() {
		game = std::make_unique<scid::database::Game>();
		loadedGameId.reset();
		dirty = false;
		history.clear();
		delete deprecatedPushPop.first;
		deprecatedPushPop = {nullptr, false};
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

	scid::database::Game& game() const { return *state().game; }

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
	void storeUndoPoint() const { state().history.store(state().game.get()); }
	void undo() const {
		auto& s = state();
		s.game.reset(s.history.undo(s.game.release()));
	}
	void redo() const {
		auto& s = state();
		s.game.reset(s.history.redo(s.game.release()));
	}

	void resetToNewGame() const { state().reset(); }

	void replace(scid::database::Game* game, std::optional<scid::database::gamenumT> gameId, bool dirty) const {
		auto& s = state();
		s.game.reset(game);
		s.loadedGameId = gameId;
		s.dirty = dirty;
		s.history.clear();
		delete s.deprecatedPushPop.first;
		s.deprecatedPushPop = {nullptr, false};
	}

	scid::database::errorT load(scid::database::gamenumT gameId) const {
		auto& s = state();
		s.history.clear();
		const auto err = base_->loadGame(gameId, *s.game);
		if (err != scid::database::OK)
			return err;

		if (base_->defaultFilterGet(gameId) > 0) {
			s.game->toPly(base_->defaultFilterGet(gameId) - 1);
		} else {
			s.game->toStart();
		}
		s.loadedGameId = gameId;
		s.dirty = false;
		return scid::database::OK;
	}

	scid::database::errorT undoAll() const {
		auto& s = state();
		s.dirty = false;
		s.history.clear();
		if (!s.loadedGameId) {
			s.game->clear();
			return scid::database::OK;
		}

		const auto err = base_->loadGame(*s.loadedGameId, *s.game);
		if (err != scid::database::OK)
			return err;
		s.game->toStart();
		return scid::database::OK;
	}

	void push(bool copy) const {
		auto& s = state();
		scid::database::Game* next = copy ? s.game->clone() : new scid::database::Game;
		if (s.deprecatedPushPop.first) {
			delete s.deprecatedPushPop.first;
		}
		s.deprecatedPushPop = {s.game.release(), s.dirty};
		s.game.reset(next);
		s.dirty = false;
	}

	void pop() const {
		auto& s = state();
		if (!s.deprecatedPushPop.first)
			return;

		s.game.reset(s.deprecatedPushPop.first);
		s.dirty = s.deprecatedPushPop.second;
		s.deprecatedPushPop = {nullptr, false};
	}

private:
	State& state() const { return detail::stateFor(*base_); }
	scid::database::scidBaseT* base_;
};

inline GameSession gameSession(scid::database::scidBaseT& base) { return GameSession(base); }

} // namespace scidup::app::editor

#endif
