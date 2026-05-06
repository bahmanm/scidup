/*
# Copyright (C) 2015 Fulvio Benini

* This file is part of Scid (Shane's Chess Information Database).
*
* Scid is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation.
*
* Scid is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with Scid. If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef SCID_UI_TCLTK_H
#define SCID_UI_TCLTK_H

#include "scidup/database/misc.h"
#include <chrono>
#include <tcl.h>
#include <sstream>
#include <limits>
#include <array>
#include <vector>

namespace UI_impl {

using namespace scid::database;

typedef int         UI_res_t;
typedef ClientData  UI_extra_t;
typedef Tcl_Interp* UI_handle_t;

inline int initTclTk (UI_handle_t ti);

inline int Main (int argc, char* argv[], void (*exit) (void*)) {
	Tcl_FindExecutable(argv[0]);
	Tcl_CreateExitHandler(exit, 0);
	bool search_tcl = (argc == 1) ? true : false;
	if (argc > 1 && argc < 10) {
		char* ext = strrchr (argv[1], '.');
		if (ext != 0 && strlen(ext) == 4 && std::string(".tcl") != ext) {
			search_tcl = true;
		}
	}
	if (search_tcl) {
		char sourceFileName [4096] = {0};
		#ifndef WIN32
		// Expand symbolic links
		char* exec_name = realpath(Tcl_GetNameOfExecutable(), 0);
		strncpy(sourceFileName, exec_name, 4000);
		free(exec_name);
		#else
		strncpy(sourceFileName, Tcl_GetNameOfExecutable(), 4000);
		#endif

			char* dirname = strrchr(sourceFileName, '/');
			if (dirname == 0) dirname = sourceFileName;
			else dirname += 1;
			strcpy (dirname, "tcl/start.tcl");
			if (0 != Tcl_Access(sourceFileName, 4)) {
				strcpy (dirname, "../tcl/start.tcl");
				if (0 != Tcl_Access(sourceFileName, 4)) {
					strcpy (dirname, "../share/scidup/tcl/start.tcl");
				}
			}
			char* newArgv[10] = { argv[0], sourceFileName };
			std::copy(argv + 1, argv + argc, newArgv + 2);
			Tcl_Main(argc + 1, newArgv, UI_impl::initTclTk);
		} else {
		Tcl_Main (argc, argv, UI_impl::initTclTk);
	}

	return 0;
}

class tcl_Progress : public Progress::Impl {
	UI_handle_t ti_;
	using clock = std::chrono::high_resolution_clock;
	decltype(clock::now()) timer_ = clock::now();

public:
	explicit tcl_Progress(UI_handle_t ti) : ti_(ti) {}

	bool report(size_t done, size_t total, const char* msg) final {
		const auto now = clock::now();
		if (done != total && now - timer_ < std::chrono::milliseconds{30})
			return true;

		timer_ = now;
		Tcl_Obj* cmd[3] = {};
		cmd[0] = Tcl_NewStringObj("::progressCallBack", -1);
		cmd[1] = Tcl_NewDoubleObj(total ? (1.0 * done / total) : 1);
		int n = 2;
		if (msg) {
			cmd[2] = Tcl_NewStringObj(msg, -1);
			n = 3;
		}
		std::for_each(cmd, cmd + n, [](Tcl_Obj* e) { Tcl_IncrRefCount(e); });
		auto res = Tcl_EvalObjv(ti_, n, cmd, 0);
		std::for_each(cmd, cmd + n, [](Tcl_Obj* e) { Tcl_DecrRefCount(e); });
		return res == TCL_OK;
	}
};

inline Progress CreateProgress(UI_handle_t ti) {
	Tcl_Obj* cmd[2];
	cmd[0] = Tcl_NewStringObj("::progressCallBack", -1);
	cmd[1] = Tcl_NewStringObj("init", -1);
	Tcl_IncrRefCount(cmd[0]);
	Tcl_IncrRefCount(cmd[1]);
	auto err = Tcl_EvalObjv(ti, 2, cmd, 0);
	Tcl_DecrRefCount(cmd[0]);
	Tcl_DecrRefCount(cmd[1]);
	if (err != TCL_OK)
		return {};

	return Progress(new UI_impl::tcl_Progress(ti));
}

class List {
	Tcl_Obj** list_;
	mutable int i_;
	Tcl_Obj* stackBuf_[6];

	friend Tcl_Obj* ObjMaker(const List&);

public:
	explicit List(size_t max_size)
	: list_(stackBuf_), i_(0) {
		const size_t stackBuf_size = sizeof(stackBuf_)/sizeof(stackBuf_[0]);
		if (max_size > stackBuf_size) {
			list_ = new Tcl_Obj*[max_size];
		}
	}

	~List() {
		clear();
		if (list_ != stackBuf_) delete [] list_;
	}

	void clear() {
		for (int i=0; i < i_; i++) Tcl_DecrRefCount(list_[i]);
		i_ = 0;
	}

	void push_back(Tcl_Obj* value) {
		ASSERT(value != 0);
		list_[i_++] = value;
	}
	template <typename T>
	void push_back(const T& value);
};

inline Tcl_Obj* ObjMaker(bool v) {
	return Tcl_NewWideIntObj(v);
}
inline Tcl_Obj* ObjMaker(int v) {
	return Tcl_NewWideIntObj(v);
}
inline Tcl_Obj* ObjMaker(unsigned int v) {
	ASSERT(v <= static_cast<unsigned int>(std::numeric_limits<int>::max()));
	return Tcl_NewWideIntObj(static_cast<int>(v));
}
inline Tcl_Obj* ObjMaker(unsigned long v) {
	ASSERT(v <= static_cast<unsigned long>(std::numeric_limits<int>::max()));
	return Tcl_NewWideIntObj(static_cast<int>(v));
}
inline Tcl_Obj* ObjMaker(unsigned long long v) {
	ASSERT(v <= static_cast<unsigned long long>(std::numeric_limits<int>::max()));
	return Tcl_NewWideIntObj(static_cast<int>(v));
}
inline Tcl_Obj* ObjMaker(double v) {
	return Tcl_NewDoubleObj(v);
}
inline Tcl_Obj* ObjMaker(const char* s) {
	return Tcl_NewStringObj(s, -1);
}
inline Tcl_Obj* ObjMaker(const std::string& s) {
	ASSERT(s.size() <= static_cast<size_t>(std::numeric_limits<int>::max()));
	return Tcl_NewStringObj(s.c_str(), static_cast<int>(s.size()));
}
inline Tcl_Obj* ObjMaker(const List& v) {
	Tcl_Obj* res = Tcl_NewListObj(v.i_, v.list_);
	v.i_ = 0;
	return res;
}

template <typename T>
inline void List::push_back(const T& value) {
	push_back(ObjMaker(value));
}


inline UI_res_t ResultHelper(UI_handle_t ti, errorT res) {
	if (res == OK) return TCL_OK;
	Tcl_SetObjErrorCode(ti, Tcl_NewWideIntObj(res));
	return TCL_ERROR;
}

inline UI_res_t Result(UI_handle_t ti, errorT res) {
	Tcl_ResetResult(ti);
	return UI_impl::ResultHelper(ti, res);
}

template <typename T>
inline UI_res_t Result(UI_handle_t ti, errorT res, const T& value) {
	Tcl_SetObjResult(ti, UI_impl::ObjMaker(value));
	return UI_impl::ResultHelper(ti, res);
}

inline int LegacyCmdFromObjv(UI_res_t (*fn)(UI_extra_t, UI_handle_t, int, const char**),
                             UI_extra_t cd,
                             UI_handle_t ti,
                             int objc,
                             Tcl_Obj* const objv[]) {
	std::array<const char*, 16> stackArgv = {};
	std::vector<const char*> heapArgv;

	const char** argv = stackArgv.data();
	if (objc > static_cast<int>(stackArgv.size())) {
		heapArgv.resize(static_cast<size_t>(objc));
		argv = heapArgv.data();
	}

	for (int i = 0; i < objc; ++i) {
		argv[i] = Tcl_GetString(objv[i]);
	}
	return fn(cd, ti, objc, argv);
}

} //End of UI_impl namespace


//TODO:
//Duplicate declarations (already in ui.h)
UI_impl::UI_res_t str_is_prefix  (UI_impl::UI_extra_t, UI_impl::UI_handle_t, int argc, const char ** argv);
UI_impl::UI_res_t str_prefix_len (UI_impl::UI_extra_t, UI_impl::UI_handle_t, int argc, const char ** argv);
UI_impl::UI_res_t sc_base        (UI_impl::UI_extra_t, UI_impl::UI_handle_t, int argc, const char ** argv);
UI_impl::UI_res_t sc_book        (UI_impl::UI_extra_t, UI_impl::UI_handle_t, int argc, const char ** argv);
UI_impl::UI_res_t sc_clipbase    (UI_impl::UI_extra_t, UI_impl::UI_handle_t, int argc, const char ** argv);
UI_impl::UI_res_t sc_eco         (UI_impl::UI_extra_t, UI_impl::UI_handle_t, int argc, const char ** argv);
UI_impl::UI_res_t sc_filter      (UI_impl::UI_extra_t, UI_impl::UI_handle_t, int argc, const char ** argv);
UI_impl::UI_res_t sc_game        (UI_impl::UI_extra_t, UI_impl::UI_handle_t, int argc, const char ** argv);
UI_impl::UI_res_t sc_info        (UI_impl::UI_extra_t, UI_impl::UI_handle_t, int argc, const char ** argv);
UI_impl::UI_res_t sc_move        (UI_impl::UI_extra_t, UI_impl::UI_handle_t, int argc, const char ** argv);
UI_impl::UI_res_t sc_name        (UI_impl::UI_extra_t, UI_impl::UI_handle_t, int argc, const char ** argv);
UI_impl::UI_res_t sc_report      (UI_impl::UI_extra_t, UI_impl::UI_handle_t, int argc, const char ** argv);
UI_impl::UI_res_t sc_pos         (UI_impl::UI_extra_t, UI_impl::UI_handle_t, int argc, const char ** argv);
UI_impl::UI_res_t sc_search      (UI_impl::UI_extra_t, UI_impl::UI_handle_t, int argc, const char ** argv);
UI_impl::UI_res_t sc_tree        (UI_impl::UI_extra_t, UI_impl::UI_handle_t, int argc, const char ** argv);
UI_impl::UI_res_t sc_var         (UI_impl::UI_extra_t, UI_impl::UI_handle_t, int argc, const char ** argv);

inline int str_is_prefix_obj  (ClientData cd, Tcl_Interp* ti, int objc, Tcl_Obj* const objv[]) { return UI_impl::LegacyCmdFromObjv(str_is_prefix, cd, ti, objc, objv); }
inline int str_prefix_len_obj (ClientData cd, Tcl_Interp* ti, int objc, Tcl_Obj* const objv[]) { return UI_impl::LegacyCmdFromObjv(str_prefix_len, cd, ti, objc, objv); }
inline int sc_base_obj        (ClientData cd, Tcl_Interp* ti, int objc, Tcl_Obj* const objv[]) { return UI_impl::LegacyCmdFromObjv(sc_base, cd, ti, objc, objv); }
inline int sc_book_obj        (ClientData cd, Tcl_Interp* ti, int objc, Tcl_Obj* const objv[]) { return UI_impl::LegacyCmdFromObjv(sc_book, cd, ti, objc, objv); }
inline int sc_clipbase_obj    (ClientData cd, Tcl_Interp* ti, int objc, Tcl_Obj* const objv[]) { return UI_impl::LegacyCmdFromObjv(sc_clipbase, cd, ti, objc, objv); }
inline int sc_eco_obj         (ClientData cd, Tcl_Interp* ti, int objc, Tcl_Obj* const objv[]) { return UI_impl::LegacyCmdFromObjv(sc_eco, cd, ti, objc, objv); }
inline int sc_filter_obj      (ClientData cd, Tcl_Interp* ti, int objc, Tcl_Obj* const objv[]) { return UI_impl::LegacyCmdFromObjv(sc_filter, cd, ti, objc, objv); }
inline int sc_game_obj        (ClientData cd, Tcl_Interp* ti, int objc, Tcl_Obj* const objv[]) { return UI_impl::LegacyCmdFromObjv(sc_game, cd, ti, objc, objv); }
inline int sc_info_obj        (ClientData cd, Tcl_Interp* ti, int objc, Tcl_Obj* const objv[]) { return UI_impl::LegacyCmdFromObjv(sc_info, cd, ti, objc, objv); }
inline int sc_move_obj        (ClientData cd, Tcl_Interp* ti, int objc, Tcl_Obj* const objv[]) { return UI_impl::LegacyCmdFromObjv(sc_move, cd, ti, objc, objv); }
inline int sc_name_obj        (ClientData cd, Tcl_Interp* ti, int objc, Tcl_Obj* const objv[]) { return UI_impl::LegacyCmdFromObjv(sc_name, cd, ti, objc, objv); }
inline int sc_report_obj      (ClientData cd, Tcl_Interp* ti, int objc, Tcl_Obj* const objv[]) { return UI_impl::LegacyCmdFromObjv(sc_report, cd, ti, objc, objv); }
inline int sc_pos_obj         (ClientData cd, Tcl_Interp* ti, int objc, Tcl_Obj* const objv[]) { return UI_impl::LegacyCmdFromObjv(sc_pos, cd, ti, objc, objv); }
inline int sc_search_obj      (ClientData cd, Tcl_Interp* ti, int objc, Tcl_Obj* const objv[]) { return UI_impl::LegacyCmdFromObjv(sc_search, cd, ti, objc, objv); }
inline int sc_tree_obj        (ClientData cd, Tcl_Interp* ti, int objc, Tcl_Obj* const objv[]) { return UI_impl::LegacyCmdFromObjv(sc_tree, cd, ti, objc, objv); }
inline int sc_var_obj         (ClientData cd, Tcl_Interp* ti, int objc, Tcl_Obj* const objv[]) { return UI_impl::LegacyCmdFromObjv(sc_var, cd, ti, objc, objv); }

#ifndef SCIDUP_PORTABLE_ARCHIVE_MODE
#define SCIDUP_PORTABLE_ARCHIVE_MODE 0
#endif

inline void ConfigureTclTkForPortableArchive(UI_impl::UI_handle_t ti)
{
	if (SCIDUP_PORTABLE_ARCHIVE_MODE == 0) {
		return;
	}

	const char* executable_name = Tcl_GetNameOfExecutable();
	if (executable_name == nullptr || *executable_name == '\0') {
		return;
	}

	#ifndef WIN32
	char* resolved_executable_name = realpath(executable_name, 0);
	const char* executable_path = (resolved_executable_name != nullptr) ? resolved_executable_name : executable_name;
	#else
	const char* executable_path = executable_name;
	#endif

	std::string executable_path_string(executable_path);

	#ifndef WIN32
	if (resolved_executable_name != nullptr) free(resolved_executable_name);
	#endif

	const auto last_slash = executable_path_string.find_last_of('/');
	if (last_slash == std::string::npos) {
		return;
	}

	std::string executable_directory = executable_path_string.substr(0, last_slash);
	std::string bundle_root = executable_directory + "/..";

	// Prefer bundled Tcl packages (if any) via TCLLIBPATH when running from a portable archive.
	const std::string tcllibpath_directory = bundle_root + "/lib";
	Tcl_Obj* tcllibpath_list = Tcl_NewListObj(0, nullptr);
	Tcl_ListObjAppendElement(ti, tcllibpath_list, Tcl_NewStringObj(tcllibpath_directory.c_str(), -1));
	Tcl_IncrRefCount(tcllibpath_list);
	Tcl_SetVar2Ex(ti, "env", "TCLLIBPATH", tcllibpath_list, TCL_GLOBAL_ONLY);
	Tcl_DecrRefCount(tcllibpath_list);
}

inline int UI_impl::initTclTk (UI_handle_t ti)
{
	ConfigureTclTkForPortableArchive(ti);

	if (Tcl_Init (ti) == TCL_ERROR) { return TCL_ERROR; }

	Tcl_CreateObjCommand(ti, "strIsPrefix" , str_is_prefix_obj , 0, nullptr);
	Tcl_CreateObjCommand(ti, "strPrefixLen", str_prefix_len_obj, 0, nullptr);
	Tcl_CreateObjCommand(ti, "sc_base"     , sc_base_obj       , 0, nullptr);
	Tcl_CreateObjCommand(ti, "sc_book"     , sc_book_obj       , 0, nullptr);
	Tcl_CreateObjCommand(ti, "sc_clipbase" , sc_clipbase_obj   , 0, nullptr);
	Tcl_CreateObjCommand(ti, "sc_eco"      , sc_eco_obj        , 0, nullptr);
	Tcl_CreateObjCommand(ti, "sc_filter"   , sc_filter_obj     , 0, nullptr);
	Tcl_CreateObjCommand(ti, "sc_game"     , sc_game_obj       , 0, nullptr);
	Tcl_CreateObjCommand(ti, "sc_info"     , sc_info_obj       , 0, nullptr);
	Tcl_CreateObjCommand(ti, "sc_move"     , sc_move_obj       , 0, nullptr);
	Tcl_CreateObjCommand(ti, "sc_name"     , sc_name_obj       , 0, nullptr);
	Tcl_CreateObjCommand(ti, "sc_report"   , sc_report_obj     , 0, nullptr);
	Tcl_CreateObjCommand(ti, "sc_pos"      , sc_pos_obj        , 0, nullptr);
	Tcl_CreateObjCommand(ti, "sc_search"   , sc_search_obj     , 0, nullptr);
	Tcl_CreateObjCommand(ti, "sc_tree"     , sc_tree_obj       , 0, nullptr);
	Tcl_CreateObjCommand(ti, "sc_var"      , sc_var_obj        , 0, nullptr);

	return TCL_OK;
}


#endif
