#include <boost/test/unit_test.hpp>

#include <async_mqtt5/detail/utf8_mqtt.hpp>

BOOST_AUTO_TEST_SUITE(utf8_mqtt/*, *boost::unit_test::disabled()*/)

std::string to_str(int utf8ch) {
	if (utf8ch < 0x80)
		return { char(utf8ch) };
	if (utf8ch < 0x800)
		return {
			char((utf8ch >> 6) | 0xC0),
			char((utf8ch & 0x3F) | 0x80)
		};
	if (utf8ch < 0xFFFF)
		return {
			char((utf8ch >> 12) | 0xE0),
			char(((utf8ch >> 6) & 0x3F) | 0x80),
			char((utf8ch & 0x3F) | 0x80)
		};
	return {
		char((utf8ch >> 18) | 0xF0),
		char(((utf8ch >> 12) & 0x3F) | 0x80),
		char(((utf8ch >> 6) & 0x3F) | 0x80),
		char((utf8ch & 0x3F) | 0x80)
	};
}

BOOST_AUTO_TEST_CASE(utf8_string_validation) {
	using namespace async_mqtt5::detail;
	BOOST_CHECK_EQUAL(is_valid_mqtt_utf8("stringy"), true);
	BOOST_CHECK_EQUAL(is_valid_mqtt_utf8(""), true);
	BOOST_CHECK_EQUAL(is_valid_mqtt_utf8(to_str(1)), false);
	BOOST_CHECK_EQUAL(is_valid_mqtt_utf8(to_str(31)), false);
	BOOST_CHECK_EQUAL(is_valid_mqtt_utf8(to_str(32)), true);
	BOOST_CHECK_EQUAL(is_valid_mqtt_utf8(to_str(126)), true);
	BOOST_CHECK_EQUAL(is_valid_mqtt_utf8(to_str(127)), false);
	BOOST_CHECK_EQUAL(is_valid_mqtt_utf8(to_str(159)), false);
	BOOST_CHECK_EQUAL(is_valid_mqtt_utf8(to_str(160)), true);
	BOOST_CHECK_EQUAL(is_valid_mqtt_utf8(to_str(55296)), false);
	BOOST_CHECK_EQUAL(is_valid_mqtt_utf8(to_str(57343)), false);
	BOOST_CHECK_EQUAL(is_valid_mqtt_utf8(to_str(64976)), false);
	BOOST_CHECK_EQUAL(is_valid_mqtt_utf8(to_str(65007)), false);
	BOOST_CHECK_EQUAL(is_valid_mqtt_utf8(to_str(65008)), true);
	BOOST_CHECK_EQUAL(is_valid_mqtt_utf8(to_str(131070)), false);
	BOOST_CHECK_EQUAL(is_valid_mqtt_utf8(to_str(131071)), false);
}

BOOST_AUTO_TEST_CASE(utf8_topic_validation) {
	using namespace async_mqtt5::detail;
	BOOST_CHECK_EQUAL(is_valid_utf8_topic(""), false);
	BOOST_CHECK_EQUAL(is_valid_utf8_topic("topic"), true);
}


BOOST_AUTO_TEST_SUITE_END();
