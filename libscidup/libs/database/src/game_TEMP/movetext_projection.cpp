#include "movetext_projection.h"

#include "movetree.h"
#include "scidup/core/game.h"

namespace scid::database {
namespace {

scid::core::MoveAction toMoveAction(simpleMoveT const& sm) {
	return {sm.from, sm.to, sm.promote};
}

void copyMoveData(moveT const& source, scid::core::Move& dest);

void copyLine(moveT const* source, scid::core::MoveSequence& dest) {
	for (auto move = source; move && !move->endMarker(); move = move->next) {
		if (move->startMarker())
			continue;

		auto& destMove = dest.appendMove(toMoveAction(move->moveData));
		copyMoveData(*move, destMove);
	}
}

void copyVariations(moveT const& source, scid::core::Move& dest) {
	for (auto variation = source.varChild; variation;
	     variation = variation->varChild) {
		auto& destVariation = dest.addVariation(variation->comment);
		copyLine(variation->next, destVariation.line);
	}
}

void copyMoveData(moveT const& source, scid::core::Move& dest) {
	dest.action = toMoveAction(source.moveData);
	dest.san = source.san;
	dest.metadata.comment = source.comment;
	dest.metadata.nags.assign(source.nags, source.nags + source.nagCount);
	copyVariations(source, dest);
}

} // namespace

void TEMP_movetext::syncCoreMovetext(scid::core::Game& coreGame,
                                       const moveT* firstMove) {
	coreGame.clearMovetext();
	coreGame.setInitialComment(firstMove->comment);
	for (auto move = firstMove->next; move && !move->endMarker();
	     move = move->next) {
		if (move->startMarker())
			continue;

		auto& dest = coreGame.appendMainlineMove(toMoveAction(move->moveData));
		copyMoveData(*move, dest);
	}
}

} // namespace scid::database
