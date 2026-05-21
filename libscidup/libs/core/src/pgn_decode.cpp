#include "pgnparse_impl.h"

namespace scid::core::pgn {

namespace {

bool parseGameImpl(const char* input, size_t inputLen,
                   scid::core::Game& game,
                   scid::core::MovetextLocation* location, ParseLog& log,
                   std::optional<std::string>* scidFlags) {
	struct VisitorNoEOF : public pgn_impl::PgnVisitor {
		VisitorNoEOF(scid::core::Game& g,
		             scid::core::MovetextLocation* location,
		             std::optional<std::string>* scidFlags)
		    : pgn_impl::PgnVisitor(g, location, scidFlags) {}
		void visitPGN_inputEOF() {}
	} visitor(game, location, scidFlags);

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
               scid::core::Game& game, ParseLog& log,
               std::optional<std::string>* scidFlags) {
	scid::core::MovetextLocation location;
	return parseGameImpl(input, inputLen, game, &location, log, scidFlags);
}

bool parseGame(const char* input, size_t inputLen,
               scid::core::Game& game,
               scid::core::MovetextLocation& location, ParseLog& log,
               std::optional<std::string>* scidFlags) {
	return parseGameImpl(input, inputLen, game, &location, log, scidFlags);
}

} // namespace scid::core::pgn
