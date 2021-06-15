// PSIEngine Copyright (c) 2021 Sakari Lehtonen <sakari@psitriangle.net>
//
// String utilities.

#pragma once

#include <iostream>
#include <string>
#include <locale>
#include <codecvt>

#include "PSITypes.h"

namespace PSIString {
	// Convert std::string to unicode version.
	extern std::wstring string_to_wide(const std::string &str);
	// Convert unicode string to std::string.
	extern std::string wide_to_string(const std::wstring &wstr);
	// Create std::string with formatting and args.
	extern std::string string_format(const char *fmt, ...);
};

