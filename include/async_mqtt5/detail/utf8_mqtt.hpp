#ifndef ASYNC_MQTT5_UTF8_MQTT_HPP
#define ASYNC_MQTT5_UTF8_MQTT_HPP

#include <cstdint>
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

inline bool is_valid_mqtt_utf8(std::string_view str) {
	constexpr size_t max_sz = 65535;

	if (str.size() > max_sz)
		return false;

	constexpr int fe_flag = 0xFE;
	constexpr int ff_flag = 0xFF;

	while (!str.empty()) {
		int c = pop_front_unichar(str);

		auto is_valid = c > 0x001F && // U+0000...U+001F control characters
			(c < 0x007F || c > 0x009F) && // U+007F...0+009F control characters
			(c < 0xD800 || c > 0xDFFF) && // U+D800...U+DFFF surrogates
			(c < 0xFDD0 || c > 0xFDEF) && // U+FDD0...U+FDEF non-characters
			(c & fe_flag) != fe_flag && // non-characters
			(c & ff_flag) != ff_flag;

		if (!is_valid)
			return false;
	}

	return true;
}

inline bool is_valid_utf8_topic(std::string_view str) {
	return !str.empty() && is_valid_mqtt_utf8(str);
}

} // namespace async_mqtt5::detail

#endif //ASYNC_MQTT5_UTF8_MQTT_HPP
