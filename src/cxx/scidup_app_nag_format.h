#pragma once

#include "scid/core/nags.h"
#include "scidup_app_legacy_encode_options.h"

namespace scidup::app {

void game_printNag(scid::core::Nag nag, char* str, bool asSymbol, gameFormatT format);

} // namespace scidup::app
