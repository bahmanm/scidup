/** @file
 * Deprecated dynamic string helper.
 */

#pragma once

#include "scidup/core/primitives.h"

#include <cstddef>
#include <cstdio>
#include <string>

namespace scid::database {

class DString { // DEPRECATED
	std::string s_;

public:
	void Clear() { s_.clear(); }

	const char* Data() { return s_.c_str(); }

	size_t Length(void) { return s_.size(); }

	void AddChar(char ch) { s_.append(1, ch); }

	void AppendUint(uint i) {
		char s[16];
		std::snprintf(s, sizeof(s), "%u", i);
		s_.append(s);
	}

	void AppendInt(int i) {
		char s[16];
		std::snprintf(s, sizeof(s), "%d", i);
		s_.append(s);
	}

	void Append(uint i) { AppendUint(i); }

	void Append(const char* str) { s_.append(str); }

	// To allow convenient appending of multiple strings without resorting
	// to messy variable-length argument lists, we define DString::Append()
	// for up to five string arguments, and for up to four arguments where
	// one is an unsigned integer and the rest are strings.
	void Append(const char* s1, const char* s2) {
		Append(s1);
		Append(s2);
	}
	void Append(const char* s1, uint i2) {
		Append(s1);
		Append(i2);
	}
	void Append(uint i1, const char* s2) {
		Append(i1);
		Append(s2);
	}
	void Append(const char* s1, const char* s2, const char* s3) {
		Append(s1);
		Append(s2);
		Append(s3);
	}
	void Append(const char* s1, const char* s2, uint i3) {
		Append(s1);
		Append(s2);
		Append(i3);
	}
	void Append(const char* s1, uint i2, const char* s3) {
		Append(s1);
		Append(i2);
		Append(s3);
	}
	void Append(uint i1, const char* s2, const char* s3) {
		Append(i1);
		Append(s2);
		Append(s3);
	}
	void Append(const char* s1, const char* s2, const char* s3,
	            const char* s4) {
		Append(s1);
		Append(s2);
		Append(s3);
		Append(s4);
	}
	void Append(const char* s1, const char* s2, const char* s3, uint i4) {
		Append(s1);
		Append(s2);
		Append(s3);
		Append(i4);
	}
	void Append(const char* s1, const char* s2, uint i3, const char* s4) {
		Append(s1);
		Append(s2);
		Append(i3);
		Append(s4);
	}
	void Append(const char* s1, uint i2, const char* s3, const char* s4) {
		Append(s1);
		Append(i2);
		Append(s3);
		Append(s4);
	}
	void Append(uint i1, const char* s2, const char* s3, const char* s4) {
		Append(i1);
		Append(s2);
		Append(s3);
		Append(s4);
	}
	void Append(const char* s1, const char* s2, const char* s3, const char* s4,
	            const char* s5) {
		Append(s1);
		Append(s2);
		Append(s3);
		Append(s4);
		Append(s5);
	}
};


} // namespace scid::database
