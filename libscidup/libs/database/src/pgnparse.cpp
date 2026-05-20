#include "pgnparse_impl.h"

namespace scid::database {

namespace {

bool pgnParseGameImpl(const char* input, size_t inputLen, Game& game,
                      scid::core::MovetextLocation* location,
                      PgnParseLog& log) {
	struct VisitorNoEOF : public PgnVisitor {
		VisitorNoEOF(Game& g, scid::core::MovetextLocation* location)
		    : PgnVisitor(g, location) {}
		void visitPGN_inputEOF() {}
	} visitor(game, location);

	auto parse = pgn::parse_game({input, input + inputLen}, visitor);
	if (!pgn_impl::logGame(log, parse.first, visitor))
		return false;

	if (parse.first == inputLen && !parse.second &&
	    currentMoveComment(game, location).empty())
		return false;

	return true;
}

} // namespace

bool pgnParseGame(const char* input, size_t inputLen, Game& game,
                  PgnParseLog& log) {
	scid::core::MovetextLocation location;
	return pgnParseGameImpl(input, inputLen, game, &location, log);
}

bool pgnParseGame(const char* input, size_t inputLen, Game& game,
                  scid::core::MovetextLocation& location, PgnParseLog& log) {
	return pgnParseGameImpl(input, inputLen, game, &location, log);
}

} // namespace scid::database
