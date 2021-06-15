#include "PSIString.h"

namespace PSIString {
	std::wstring string_to_wide(const std::string &str) {
		typedef std::codecvt_utf8<wchar_t> type;
		std::wstring_convert<type, wchar_t> converter;

		return converter.from_bytes(str);
	}

	std::string wide_to_string(const std::wstring &wstr) {
		typedef std::codecvt_utf8<wchar_t> type;
		std::wstring_convert<type, wchar_t> converter;

		return converter.to_bytes(wstr);
	}

	std::string string_format(const char *fmt, ...) {
		char *ret;
		va_list ap;

		va_start(ap, fmt);
		vasprintf(&ret, fmt, ap);
		va_end(ap);

		std::string str(ret);
		free(ret);

		return str;
	}
}
