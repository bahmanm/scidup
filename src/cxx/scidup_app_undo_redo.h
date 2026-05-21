#ifndef SCIDUP_APP_UNDO_REDO_H
#define SCIDUP_APP_UNDO_REDO_H

#include <cstddef>
#include <vector>

namespace scidup::app {

template <class TElem, size_t UNDOMAX> class UndoRedo {
	std::vector<TElem*> undo_;
	std::vector<TElem*> redo_;

public:
	~UndoRedo() { clear(); }
	void clear() {
		clear(undo_);
		clear(redo_);
	}
	size_t undoSize() const { return undo_.size(); }
	size_t redoSize() const { return redo_.size(); }

	void store(TElem* current) {
		clear(redo_);
		undo_.push_back(current->clone());
		if (undo_.size() > UNDOMAX) {
			delete undo_.front();
			undo_.erase(undo_.begin());
		}
	}

	TElem* undo(TElem* current) { return doUndoRedo(undo_, redo_, current); }
	TElem* redo(TElem* current) { return doUndoRedo(redo_, undo_, current); }

private:
	template <typename TCont>
	TElem* doUndoRedo(TCont& cont1, TCont& cont2, TElem* current) {
		if (cont1.empty())
			return current;

		if (cont2.empty() || cont2.back() != current)
			cont2.push_back(current);

		auto res = cont1.back();
		cont1.pop_back();
		return res;
	}

	template <typename TCont> void clear(TCont& cont) {
		for (auto& e : cont)
			delete e;
		cont.clear();
	}
};

} // namespace scidup::app

#endif
