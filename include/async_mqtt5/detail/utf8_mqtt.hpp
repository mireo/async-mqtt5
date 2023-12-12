#ifndef ASYNC_MQTT5_UTF8_MQTT_HPP
#define ASYNC_MQTT5_UTF8_MQTT_HPP

#include <string>

namespace async_mqtt5::detail {

inline int pop_front_unichar(std::string_view& s) {
	// assuming that s.length() is > 0

	int n = s[0] & 0xF0;
	int ch = -1;

	if ((n & 0x80) == 0) {
		ch = s[0];
		s.remove_prefix(1);
	}
	else if ((n == 0xC0 || n == 0xD0) && s.size() > 1) {
		ch = ((s[0] & 0x1F) << 6) | (s[1] & 0x3F);
		s.remove_prefix(2);
	}
	else if ((n == 0xE0) && s.size() > 2) {
		ch = ((s[0] & 0x1F) << 12) | ((s[1] & 0x3F) << 6) | (s[2] & 0x3F);
		s.remove_prefix(3);
	}
	else if ((n == 0xF0) && s.size() > 3) {
		ch = ((s[0] & 0x1F) << 18) | ((s[1] & 0x3F) << 12) |
			((s[2] & 0x3F) << 6) | (s[3] & 0x3F);
		s.remove_prefix(4);
	}

	return ch;
}

inline bool is_valid_mqtt_utf8_char(int c) {
	constexpr int fe_flag = 0xFE;
	constexpr int ff_flag = 0xFF;

	return c > 0x001F && // U+0000...U+001F control characters
		(c < 0x007F || c > 0x009F) && // U+007F...0+009F control characters
		(c < 0xD800 || c > 0xDFFF) && // U+D800...U+DFFF surrogates
		(c < 0xFDD0 || c > 0xFDEF) && // U+FDD0...U+FDEF non-characters
		(c & fe_flag) != fe_flag && // non-characters
		(c & ff_flag) != ff_flag;
}

inline bool is_valid_mqtt_utf8_non_wildcard_char(int c) {
	return c != '+' && c != '#' && is_valid_mqtt_utf8_char(c);
}

inline bool is_valid_string_size(size_t sz) {
	constexpr size_t max_sz = 65535;
	return sz <= max_sz;
}

inline bool is_valid_topic_size(size_t sz) {
	constexpr size_t min_sz = 1;
	return min_sz <= sz && is_valid_string_size(sz);
}

template <typename ValidationFun>
bool is_valid_impl(
	std::string_view str, ValidationFun&& validation_fun
) {
	while (!str.empty()) {
		int c = pop_front_unichar(str);
		bool is_valid = validation_fun(c);

		if (!is_valid)
			return false;
	}

	return true;
}

inline bool is_valid_mqtt_utf8(std::string_view str) {
	return is_valid_string_size(str.size()) &&
		is_valid_impl(str, is_valid_mqtt_utf8_char);
}

inline bool is_valid_topic_name(std::string_view str) {
	return is_valid_topic_size(str.size()) &&
		is_valid_impl(str, is_valid_mqtt_utf8_non_wildcard_char);
}

inline bool is_valid_topic_filter(std::string_view str) {
	if (!is_valid_topic_size(str.size()))
		return false;

	constexpr int multi_lvl_wildcard = '#';
	constexpr int single_lvl_wildcard = '+';

	// must be the last character preceded by '/' or stand alone
	// #, .../#
	if (str.back() == multi_lvl_wildcard) {
		str.remove_suffix(1);

		if (!str.empty() && str.back() != '/')
			return false;
	}

	int last_c = -1;
	while (!str.empty()) {
		int c = pop_front_unichar(str);

		// can be used at any level, but must occupy an entire level
		// +, +/..., .../+/..., .../+
		bool is_valid_single_lvl = (c == single_lvl_wildcard) &&
			(str.empty() || str.front() == '/') &&
			(last_c == -1 || last_c == '/');

		bool is_valid_mqtt_utf_8 = is_valid_mqtt_utf8_non_wildcard_char(c);


		if (is_valid_mqtt_utf_8 || is_valid_single_lvl) {
			last_c = c;
			continue;
		}

		return false;
	}

	return true;
}

} // namespace async_mqtt5::detail

#endif //ASYNC_MQTT5_UTF8_MQTT_HPP
