#ifndef SCIDUP_APP_EDITOR_H
#define SCIDUP_APP_EDITOR_H

#include "scidbase.h"
#include <memory>
#include <optional>
#include <unordered_map>

namespace scidup::app::editor {

struct State {
	std::unique_ptr<Game> game = std::make_unique<Game>();
	std::optional<gamenumT> loadedGameId;
	bool dirty = false;
	UndoRedo<Game, 100> history;
	std::pair<Game*, bool> deprecatedPushPop{nullptr, false};

	~State() { delete deprecatedPushPop.first; }

	void reset() {
		game = std::make_unique<Game>();
		loadedGameId.reset();
		dirty = false;
		history.clear();
		delete deprecatedPushPop.first;
		deprecatedPushPop = {nullptr, false};
	}
};

namespace detail {
inline auto& states() {
	static std::unordered_map<scidBaseT*, std::unique_ptr<State>> perBaseStates;
	return perBaseStates;
}

inline State& stateFor(scidBaseT& base) {
	auto& allStates = states();
	auto& slot = allStates[&base];
	if (!slot)
		slot = std::make_unique<State>();
	return *slot;
}
} // namespace detail

inline void reset(scidBaseT& base) { detail::stateFor(base).reset(); }

inline void release(scidBaseT& base) { detail::states().erase(&base); }

class GameSession {
public:
	explicit GameSession(scidBaseT& base) : base_(&base) {}

	Game& game() const { return *state().game; }

	std::optional<gamenumT> loadedGameId() const { return state().loadedGameId; }

	const IndexEntry* loadedIndexEntry() const {
		const auto gameId = loadedGameId();
		return gameId ? base_->getIndexEntry(*gameId) : nullptr;
	}

	void setLoadedGameId(std::optional<gamenumT> gameId) const {
		state().loadedGameId = gameId;
	}

	bool matchesLoadedGame(gamenumT gameId) const {
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

	void replace(Game* game, std::optional<gamenumT> gameId, bool dirty) const {
		auto& s = state();
		s.game.reset(game);
		s.loadedGameId = gameId;
		s.dirty = dirty;
		s.history.clear();
		delete s.deprecatedPushPop.first;
		s.deprecatedPushPop = {nullptr, false};
	}

	errorT load(gamenumT gameId) const {
		auto& s = state();
		s.history.clear();
		const auto err = base_->loadGame(gameId, *s.game);
		if (err != OK)
			return err;

		if (base_->defaultFilterGet(gameId) > 0) {
			s.game->MoveToPly(base_->defaultFilterGet(gameId) - 1);
		} else {
			s.game->MoveToStart();
		}
		s.loadedGameId = gameId;
		s.dirty = false;
		return OK;
	}

	errorT undoAll() const {
		auto& s = state();
		s.dirty = false;
		s.history.clear();
		if (!s.loadedGameId) {
			s.game->Clear();
			return OK;
		}

		const auto err = base_->loadGame(*s.loadedGameId, *s.game);
		if (err != OK)
			return err;
		s.game->MoveToStart();
		return OK;
	}

	void push(bool copy) const {
		auto& s = state();
		Game* next = copy ? s.game->clone() : new Game;
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
	scidBaseT* base_;
};

inline GameSession gameSession(scidBaseT& base) { return GameSession(base); }

} // namespace scidup::app::editor

#endif
