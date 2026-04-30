#ifndef SCIDUP_APP_EDITOR_H
#define SCIDUP_APP_EDITOR_H

#include "scidbase.h"
#include <optional>

namespace scidup::app::editor {

// Transitional app-side view over the active editor state.
// The state still lives on scidBaseT for now, but callers can migrate to this
// surface before the ownership moves out of the database layer.
class GameSession {
public:
	explicit GameSession(scidBaseT& base) : base_(&base) {}

	Game& game() const { return *base_->game; }

	std::optional<gamenumT> loadedGameId() const {
		if (base_->gameNumber < 0)
			return std::nullopt;
		return static_cast<gamenumT>(base_->gameNumber);
	}

	const IndexEntry* loadedIndexEntry() const {
		const auto gameId = loadedGameId();
		return gameId ? base_->getIndexEntry(*gameId) : nullptr;
	}

	void setLoadedGameId(std::optional<gamenumT> gameId) const {
		base_->gameNumber = gameId ? static_cast<int>(*gameId) : -1;
	}

	bool matchesLoadedGame(gamenumT gameId) const {
		const auto loaded = loadedGameId();
		return loaded && *loaded == gameId;
	}

	bool isDirty() const { return base_->gameAltered; }
	void setDirty(bool dirty = true) const { base_->gameAltered = dirty; }

	void clearHistory() const { base_->gameAlterations.clear(); }
	size_t undoSize() const { return base_->gameAlterations.undoSize(); }
	size_t redoSize() const { return base_->gameAlterations.redoSize(); }
	void storeUndoPoint() const { base_->gameAlterations.store(base_->game); }
	void undo() const { base_->game = base_->gameAlterations.undo(base_->game); }
	void redo() const { base_->game = base_->gameAlterations.redo(base_->game); }

	void resetToNewGame() const {
		base_->game->Clear();
		setLoadedGameId(std::nullopt);
		setDirty(false);
	}

	void replace(Game* game, std::optional<gamenumT> gameId, bool dirty) const {
		delete base_->game;
		base_->game = game;
		clearHistory();
		setLoadedGameId(gameId);
		setDirty(dirty);
	}

	errorT load(gamenumT gameId) const {
		clearHistory();
		const auto err = base_->loadGame(gameId, *base_->game);
		if (err != OK)
			return err;

		if (base_->dbFilter->Get(gameId) > 0) {
			base_->game->MoveToPly(base_->dbFilter->Get(gameId) - 1);
		} else {
			base_->game->MoveToStart();
		}
		setLoadedGameId(gameId);
		setDirty(false);
		return OK;
	}

	errorT undoAll() const {
		setDirty(false);
		clearHistory();
		const auto gameId = loadedGameId();
		if (!gameId) {
			base_->game->Clear();
			return OK;
		}

		const auto err = base_->loadGame(*gameId, *base_->game);
		if (err != OK)
			return err;
		base_->game->MoveToStart();
		return OK;
	}

	void push(bool copy) const {
		Game* next = copy ? base_->game->clone() : new Game;
		if (base_->deprecated_push_pop.first) {
			delete base_->deprecated_push_pop.first;
		}
		base_->deprecated_push_pop = {base_->game, base_->gameAltered};
		base_->game = next;
		base_->gameAltered = false;
	}

	void pop() const {
		if (!base_->deprecated_push_pop.first)
			return;

		delete base_->game;
		base_->game = base_->deprecated_push_pop.first;
		base_->gameAltered = base_->deprecated_push_pop.second;
		base_->deprecated_push_pop.first = nullptr;
	}

private:
	scidBaseT* base_;
};

inline GameSession gameSession(scidBaseT& base) { return GameSession(base); }

} // namespace scidup::app::editor

#endif
