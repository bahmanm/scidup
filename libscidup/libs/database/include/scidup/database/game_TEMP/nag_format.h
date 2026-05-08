#pragma once

#include "scidup/core/primitives.h"
#include "scidup/database/game_TEMP/legacy_encode_options.h"

#include <utility>

namespace scid::database {

void game_printNag(byte nag, char* str, bool asSymbol, gameFormatT format);
byte game_parseNag(std::pair<const char*, const char*> strview);

} // namespace scid::database
