#ifndef ASYNC_MQTT5_UTF8_MQTT_HPP
#define ASYNC_MQTT5_UTF8_MQTT_HPP

#include <cstdint>
#include <string>

namespace async_mqtt5::detail {

struct code_point {
	int32_t val;
	uint32_t size;

	auto operator<=>(const code_point&) const = default;

	static code_point from(std::string_view s) {
		auto hnibble = s[0] & 0xF0;
		return
			(hnibble & 0x80) == 0 ?
				code_point { s[0], 1 }
		:
		(hnibble == 0xC0 || hnibble == 0xD0) && s.size() > 1 ?
			code_point {
				(int32_t(s[0] & 0x1F) << 6) | int32_t(s[1] & 0x3F),
				2
			}
		:
		(hnibble == 0xE0) && s.size() > 2 ?
			code_point {
				(int32_t(s[0] & 0x1F) << 12) |
				(int32_t(s[1] & 0x3F) << 6) |
				int32_t(s[2] & 0x3F),
				3
			}
		:
		(hnibble == 0xF0) && s.size() > 3 ?
			code_point {
				(int32_t(s[0] & 0x1F) << 18) |
				(int32_t(s[1] & 0x3F) << 12) |
				(int32_t(s[2] & 0x3F) << 6) |
				int32_t(s[3] & 0x3F),
				4
			}
		:
		code_point { -1, 0 };
	}
};

inline bool is_valid_mqtt_utf8(std::string_view str) {
	constexpr size_t max_sz = 65535;

	if (str.size() > max_sz)
		return false;

	auto is_valid_cp = [](int32_t c) -> bool {
		constexpr int32_t fe_flag = 0xFE;
		constexpr int32_t ff_flag = 0xFF;

		return c >= 32 && // U+0000...U+001F control characters
			(c < 127 || c > 159) && // U+007F...0+009F control characters
			(c < 55296 || c > 57343) && // U+D800...U+DFFF surrogates
			(c < 64976 || c > 65007) &&// U+FDD0...U+FDEF non-characters
			(c & fe_flag) != fe_flag && // non-characters
			(c & ff_flag) != ff_flag;
	};

	while (!str.empty()) {
		auto cp = code_point::from(str.data());
		if (!is_valid_cp(cp.val))
			return false;
		str.remove_prefix(cp.size);
	}

	return true;
}

inline bool is_valid_utf8_topic(std::string_view str) {
	return !str.empty() && is_valid_mqtt_utf8(str);
}

} // namespace async_mqtt5::detail

#endif //ASYNC_MQTT5_UTF8_MQTT_HPP
