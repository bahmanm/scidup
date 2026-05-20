#include "pgnparse_impl.h"

namespace scid::database {

namespace {

bool pgnParseGameImpl(const char* input, size_t inputLen,
                      scid::core::Game& game,
                      scid::core::MovetextLocation* location,
                      PgnParseLog& log,
                      std::optional<std::string>* scidFlags) {
	struct VisitorNoEOF : public PgnVisitor {
		VisitorNoEOF(scid::core::Game& g,
		             scid::core::MovetextLocation* location,
		             std::optional<std::string>* scidFlags)
		    : PgnVisitor(g, location, scidFlags) {}
		void visitPGN_inputEOF() {}
	} visitor(game, location, scidFlags);

	auto parse = pgn::parse_game({input, input + inputLen}, visitor);
	if (!pgn_impl::logGame(log, parse.first, visitor))
		return false;

	if (parse.first == inputLen && !parse.second &&
	    currentMoveComment(game, location).empty())
		return false;

	return true;
}

} // namespace

bool pgnParseGame(const char* input, size_t inputLen,
                  scid::core::Game& game, PgnParseLog& log,
                  std::optional<std::string>* scidFlags) {
	scid::core::MovetextLocation location;
	return pgnParseGameImpl(input, inputLen, game, &location, log, scidFlags);
}

bool pgnParseGame(const char* input, size_t inputLen,
                  scid::core::Game& game,
                  scid::core::MovetextLocation& location, PgnParseLog& log,
                  std::optional<std::string>* scidFlags) {
	return pgnParseGameImpl(input, inputLen, game, &location, log, scidFlags);
}

} // namespace scid::database
