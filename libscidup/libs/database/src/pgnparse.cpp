#include "pgnparse_impl.h"

namespace scid::database {

bool pgnParseGame(const char* input, size_t inputLen, Game& game,
                  PgnParseLog& log) {
	struct VisitorNoEOF : public PgnVisitor {
		explicit VisitorNoEOF(Game& g) : PgnVisitor(g) {}
		void visitPGN_inputEOF() {}
	} visitor(game);

	auto parse = pgn::parse_game({input, input + inputLen}, visitor);
	if (!pgn_impl::logGame(log, parse.first, visitor))
		return false;

	if (parse.first == inputLen && !parse.second &&
	    *game.moveComment() == '\0')
		return false;

	return true;
}

} // namespace scid::database
