#include "scidup/database/game.h"

#include <string>
#include <utility>
#include <vector>

namespace scid::database {

std::string& Game::find_or_create_tag(std::string_view tag) {
	return coreGame_.findOrCreateTag(tag);
}

std::string& Game::assignTagValue(std::string_view tag,
                                  std::string_view value) {
	auto& dest = find_or_create_tag(tag);
	dest.assign(value.begin(), value.end());
	return dest;
}

} // namespace scid::database
