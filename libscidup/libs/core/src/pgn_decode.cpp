#include "pgnparse_impl.h"

namespace scid::core::pgn {

namespace {

bool parseGameImpl(const char* input, size_t inputLen,
                   scid::core::Game& game,
                   scid::core::MovetextLocation* location, ParseLog& log) {
	struct VisitorNoEOF : public pgn_impl::PgnVisitor {
		VisitorNoEOF(scid::core::Game& g,
		             scid::core::MovetextLocation* location)
		    : pgn_impl::PgnVisitor(g, location) {}
		void visitPGN_inputEOF() {}
	} visitor(game, location);

	auto parse = parse_game({input, input + inputLen}, visitor);
	if (!pgn_impl::logGame(log, parse.first, visitor))
		return false;

	if (parse.first == inputLen && !parse.second &&
	    pgn_impl::currentMoveComment(game, location).empty())
		return false;

	return true;
}

} // namespace

bool parseGame(const char* input, size_t inputLen,
               scid::core::Game& game, ParseLog& log) {
	scid::core::MovetextLocation location;
	return parseGameImpl(input, inputLen, game, &location, log);
}

bool parseGame(const char* input, size_t inputLen,
               scid::core::Game& game,
               scid::core::MovetextLocation& location, ParseLog& log) {
	return parseGameImpl(input, inputLen, game, &location, log);
}

} // namespace scid::core::pgn
