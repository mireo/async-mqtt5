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
	BOOST_CHECK_EQUAL(is_valid_mqtt_utf8(std::string(75000, 'a')), false);

	BOOST_CHECK_EQUAL(is_valid_mqtt_utf8(to_str(0x1)), false);
	BOOST_CHECK_EQUAL(is_valid_mqtt_utf8(to_str(0x1F)), false);
	BOOST_CHECK_EQUAL(is_valid_mqtt_utf8(to_str(0x20)), true);
	BOOST_CHECK_EQUAL(is_valid_mqtt_utf8(to_str(0x7E)), true);
	BOOST_CHECK_EQUAL(is_valid_mqtt_utf8(to_str(0x7F)), false);
	BOOST_CHECK_EQUAL(is_valid_mqtt_utf8(to_str(0x9F)), false);
	BOOST_CHECK_EQUAL(is_valid_mqtt_utf8(to_str(0xA0)), true);
	BOOST_CHECK_EQUAL(is_valid_mqtt_utf8(to_str(0xD800)), false);
	BOOST_CHECK_EQUAL(is_valid_mqtt_utf8(to_str(0xDFFF)), false);
	BOOST_CHECK_EQUAL(is_valid_mqtt_utf8(to_str(0xFDD0)), false);
	BOOST_CHECK_EQUAL(is_valid_mqtt_utf8(to_str(0xFDEF)), false);
	BOOST_CHECK_EQUAL(is_valid_mqtt_utf8(to_str(0xFDF0)), true);
	BOOST_CHECK_EQUAL(is_valid_mqtt_utf8(to_str(0x1FFFE)), false);
	BOOST_CHECK_EQUAL(is_valid_mqtt_utf8(to_str(0x1FFFF)), false);
}

BOOST_AUTO_TEST_CASE(topic_filter_validation) {
	using namespace async_mqtt5::detail;

	BOOST_CHECK_EQUAL(is_valid_topic_filter(""), false);
	BOOST_CHECK_EQUAL(is_valid_topic_filter("topic"), true);
	BOOST_CHECK_EQUAL(is_valid_topic_filter("topic/subtopic"), true);

	BOOST_CHECK_EQUAL(is_valid_topic_filter("#"), true);
	BOOST_CHECK_EQUAL(is_valid_topic_filter("#sport"), false);
	BOOST_CHECK_EQUAL(is_valid_topic_filter("sport#"), false);
	BOOST_CHECK_EQUAL(is_valid_topic_filter("sport/#/tennis"), false);
	BOOST_CHECK_EQUAL(is_valid_topic_filter("#/sport"), false);
	BOOST_CHECK_EQUAL(is_valid_topic_filter("spo#rt/#"), false);
	BOOST_CHECK_EQUAL(is_valid_topic_filter("sport/#"), true);
	BOOST_CHECK_EQUAL(is_valid_topic_filter("sport/tennis/#"), true);
	BOOST_CHECK_EQUAL(is_valid_topic_filter("sport/tennis#"), false);

	BOOST_CHECK_EQUAL(is_valid_topic_filter("+"), true);
	BOOST_CHECK_EQUAL(is_valid_topic_filter("+/"), true);
	BOOST_CHECK_EQUAL(is_valid_topic_filter("/+"), true);
	BOOST_CHECK_EQUAL(is_valid_topic_filter("+/+"), true);
	BOOST_CHECK_EQUAL(is_valid_topic_filter("+/+/+"), true);
	BOOST_CHECK_EQUAL(is_valid_topic_filter("+sport"), false);
	BOOST_CHECK_EQUAL(is_valid_topic_filter("sport+"), false);
	BOOST_CHECK_EQUAL(is_valid_topic_filter("sport+/player1"), false);
	BOOST_CHECK_EQUAL(is_valid_topic_filter("sport/+player1"), false);
	BOOST_CHECK_EQUAL(is_valid_topic_filter("sport/+"), true);
	BOOST_CHECK_EQUAL(is_valid_topic_filter("sport/+/player1"), true);
	BOOST_CHECK_EQUAL(is_valid_topic_filter("+/sport/+/player1/+"), true);

	BOOST_CHECK_EQUAL(is_valid_topic_filter("+/tennis/#"), true);
}

BOOST_AUTO_TEST_CASE(topic_name_validation) {
	using namespace async_mqtt5::detail;

	BOOST_CHECK_EQUAL(is_valid_topic_name(""), false);
	BOOST_CHECK_EQUAL(is_valid_topic_name("topic"), true);
	BOOST_CHECK_EQUAL(is_valid_topic_name("topic/subtopic"), true);

	BOOST_CHECK_EQUAL(is_valid_topic_name("#"), false);
	BOOST_CHECK_EQUAL(is_valid_topic_name("sport#"), false);
	BOOST_CHECK_EQUAL(is_valid_topic_name("sport/#"), false);

	BOOST_CHECK_EQUAL(is_valid_topic_name("+"), false);
	BOOST_CHECK_EQUAL(is_valid_topic_name("+sport"), false);
	BOOST_CHECK_EQUAL(is_valid_topic_name("sport+"), false);
	BOOST_CHECK_EQUAL(is_valid_topic_name("sport/+/player1"), false);

	BOOST_CHECK_EQUAL(is_valid_topic_name("+/tennis/#"), false);
}

BOOST_AUTO_TEST_SUITE_END();
