#include <boost/test/unit_test.hpp>

#include <async_mqtt5/detail/utf8_mqtt.hpp>

BOOST_AUTO_TEST_SUITE(utf8_mqtt/*, *boost::unit_test::disabled()*/)

std::string to_str(async_mqtt5::detail::code_point cp) {
	return cp.size == 1 ? std::string { char(cp.val) }
		: cp.size == 2 ? std::string { char((cp.val >> 6) | 0xC0), char((cp.val & 0x3F) | 0x80) }
		: cp.size == 3 ? std::string {
			char((cp.val >> 12) | 0xE0),
			char(((cp.val >> 6) & 0x3F) | 0x80),
			char((cp.val & 0x3F) | 0x80)
		}
		: std::string {  // cp.size == 4
			char((cp.val >> 18) | 0xF0),
			char(((cp.val >> 12) & 0x3F) | 0x80),
			char(((cp.val >> 6) & 0x3F) | 0x80),
			char((cp.val & 0x3F) | 0x80)

		};
}

async_mqtt5::detail::code_point cp(int32_t val) {
	return { val, uint32_t(val < 0x80 ? 1 : val < 0x800 ? 2 : val < 0xFFFF ? 3 : /* val < 0x10FFFF */ 4) };
}

BOOST_AUTO_TEST_CASE(utf8_string_validation) {
	using namespace async_mqtt5::detail;
	BOOST_CHECK_EQUAL(is_valid_mqtt_utf8("stringy"), true);
	BOOST_CHECK_EQUAL(is_valid_mqtt_utf8(""), true);
	BOOST_CHECK_EQUAL(is_valid_mqtt_utf8(to_str(cp(1))), false);
	BOOST_CHECK_EQUAL(is_valid_mqtt_utf8(to_str(cp(31))), false);
	BOOST_CHECK_EQUAL(is_valid_mqtt_utf8(to_str(cp(32))), true);
	BOOST_CHECK_EQUAL(is_valid_mqtt_utf8(to_str(cp(126))), true);
	BOOST_CHECK_EQUAL(is_valid_mqtt_utf8(to_str(cp(127))), false);
	BOOST_CHECK_EQUAL(is_valid_mqtt_utf8(to_str(cp(159))), false);
	BOOST_CHECK_EQUAL(is_valid_mqtt_utf8(to_str(cp(160))), true);
	BOOST_CHECK_EQUAL(is_valid_mqtt_utf8(to_str(cp(55296))), false);
	BOOST_CHECK_EQUAL(is_valid_mqtt_utf8(to_str(cp(57343))), false);
	BOOST_CHECK_EQUAL(is_valid_mqtt_utf8(to_str(cp(64976))), false);
	BOOST_CHECK_EQUAL(is_valid_mqtt_utf8(to_str(cp(65007))), false);
	BOOST_CHECK_EQUAL(is_valid_mqtt_utf8(to_str(cp(65008))), true);
	BOOST_CHECK_EQUAL(is_valid_mqtt_utf8(to_str(cp(131070))), false);
	BOOST_CHECK_EQUAL(is_valid_mqtt_utf8(to_str(cp(131071))), false);
}

BOOST_AUTO_TEST_CASE(utf8_topic_validation) {
	using namespace async_mqtt5::detail;
	BOOST_CHECK_EQUAL(is_valid_utf8_topic(""), false);
	BOOST_CHECK_EQUAL(is_valid_utf8_topic("topic"), true);
}


BOOST_AUTO_TEST_SUITE_END();
