#include "scidup/database/index.h"
#include "scidup/database/containers.h"

namespace scid::database {

struct Index::Impl {
	// The complete index is loaded in memory and can be huge. Chunked storage
	// avoids reallocating existing entries, so old entry pointers stay valid.
	VectorChunked<IndexEntry, 16> entries;
	int nInvalidNameId = 0;
};

Index::Index() : impl_(std::make_unique<Impl>()) {
	Init();
}

Index::~Index() = default;

void Index::Close() {
	Init();
}

const IndexEntry* Index::GetEntry(gamenumT g) const {
	ASSERT(g < GetNumGames());
	return &impl_->entries[g];
}

int Index::GetBadNameIdCount() const {
	return impl_->nInvalidNameId;
}

void Index::setBadNameIdCount(int count) {
	impl_->nInvalidNameId = count;
}

gamenumT Index::GetNumGames() const {
	return static_cast<gamenumT>(impl_->entries.size());
}

void Index::addEntry(const IndexEntry& ie) {
	impl_->entries.push_back(ie);
}

void Index::replaceEntry(const IndexEntry& ie, gamenumT replaced) {
	ASSERT(replaced < GetNumGames());
	impl_->entries[replaced] = ie;
}

void Index::Init() {
	impl_->nInvalidNameId = 0;
	impl_->entries.resize(0);
}

} // namespace scid::database
