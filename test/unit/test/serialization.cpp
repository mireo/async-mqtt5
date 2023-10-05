#include <boost/test/unit_test.hpp>

#include <async_mqtt5/types.hpp>
#include <async_mqtt5/impl/internal/codecs/message_decoders.hpp>
#include <async_mqtt5/impl/internal/codecs/message_encoders.hpp>

using namespace async_mqtt5;
using byte_citer = detail::byte_citer;

BOOST_AUTO_TEST_SUITE(serialization/*, *boost::unit_test::disabled()*/)

BOOST_AUTO_TEST_CASE(test_connect) {
	// testing variables
	std::string_view client_id = "async_mqtt_client_id";
	std::string_view uname = "username";
	std::optional<std::string_view> password = std::nullopt;
	uint16_t keep_alive = 60;
	bool clean_start = true;
	std::string will_topic = "will_topic";
	std::string will_message = "will_message";

	connect_props cprops;
	cprops[prop::session_expiry_interval] = 29;
	cprops[prop::user_property].emplace_back("connect user prop");

	will w{ will_topic, will_message };
	w[prop::content_type] = "will content type";
	w[prop::response_topic] = "will response topic";
	w[prop::user_property].emplace_back("first user prop");
	w[prop::user_property].emplace_back("second user prop");
	std::optional<will> will_opt { std::move(w) };

	auto msg = encoders::encode_connect(
		client_id, uname, password, keep_alive, clean_start, cprops, will_opt
	);

	byte_citer it = msg.cbegin(), last = msg.cend();
	auto header = decoders::decode_fixed_header(it, last);
	BOOST_CHECK_MESSAGE(header, "Parsing CONNECT fixed header failed.");

	const auto& [control_byte, remain_length] = *header;
	auto rv = decoders::decode_connect(remain_length, it);
	BOOST_CHECK_MESSAGE(rv, "Parsing CONNECT failed.");

	const auto& [client_id_, uname_, password_, keep_alive_, clean_start_, _, w_] = *rv;
	BOOST_CHECK_EQUAL(client_id_, client_id);
	BOOST_CHECK(uname_);
	BOOST_CHECK_EQUAL(*uname_, uname);
	BOOST_CHECK(password_ == password);
	BOOST_CHECK_EQUAL(keep_alive_, keep_alive);
	BOOST_CHECK_EQUAL(clean_start_, clean_start);
	BOOST_CHECK(w_);
	BOOST_CHECK_EQUAL((*w_).topic(), will_topic);
	BOOST_CHECK_EQUAL((*w_).message(), will_message);
	
}

BOOST_AUTO_TEST_CASE(test_connack) {
	// testing variables
	bool session_present = true;
	uint8_t reason_code = 0x89;
	bool wildcard_sub = true;

	connack_props cap;
	cap[prop::wildcard_subscription_available] = wildcard_sub;
	auto msg = encoders::encode_connack(session_present, reason_code, cap);

	byte_citer it = msg.cbegin(), last = msg.cend();
	auto header = decoders::decode_fixed_header(it, last);
	BOOST_CHECK_MESSAGE(header, "Parsing CONNACK fixed header failed.");

	const auto& [control_byte, remain_length] = *header;
	auto rv = decoders::decode_connack(remain_length, it);
	BOOST_CHECK_MESSAGE(rv, "Parsing CONNACK failed.");

	const auto& [session_present_, reason_code_, caprops] = *rv;
	BOOST_CHECK_EQUAL(session_present_, session_present);
	BOOST_CHECK_EQUAL(reason_code_, reason_code);
	BOOST_CHECK_EQUAL(*caprops[prop::wildcard_subscription_available], wildcard_sub);
}

BOOST_AUTO_TEST_CASE(test_publish) {
	// testing variables
	uint16_t packet_id = 31283;
	std::string_view topic = "publish_topic";
	std::string_view payload = "This is some payload I am publishing!";
	uint8_t message_expiry = 70;
	std::string content_type = "application/octet-stream";
	std::string publish_prop_1 = "first publish prop";
	std::string publish_prop_2 = "second publish prop";

	publish_props pp;
	pp[prop::message_expiry_interval] = message_expiry;
	pp[prop::content_type] = content_type;
	pp[prop::user_property].emplace_back(publish_prop_1);
	pp[prop::user_property].emplace_back(publish_prop_2);

	auto msg = encoders::encode_publish(
		packet_id, topic, payload, 
		qos_e::at_least_once, retain_e::yes, dup_e::no,
		pp
	);

	byte_citer it = msg.cbegin(), last = msg.cend();
	auto header = decoders::decode_fixed_header(it, last);
	BOOST_CHECK_MESSAGE(header, "Parsing PUBLISH fixed header failed.");

	const auto& [control_byte, remain_length] = *header;
	auto rv = decoders::decode_publish(control_byte, remain_length, it);
	BOOST_CHECK_MESSAGE(rv, "Parsing PUBLISH failed.");

	const auto& [topic_, packet_id_, flags, pprops, payload_] = *rv;
	BOOST_CHECK(packet_id);
	BOOST_CHECK_EQUAL(*packet_id_, packet_id);
	BOOST_CHECK_EQUAL(topic_, topic);
	BOOST_CHECK_EQUAL(payload_, payload_);
	BOOST_CHECK_EQUAL(*pprops[prop::message_expiry_interval], message_expiry);
	BOOST_CHECK_EQUAL(*pprops[prop::content_type], content_type);
	BOOST_CHECK_EQUAL(pprops[prop::user_property][0], publish_prop_1);
	BOOST_CHECK_EQUAL(pprops[prop::user_property][1], publish_prop_2);
}

BOOST_AUTO_TEST_CASE(test_puback) {
	// testing variables
	uint16_t packet_id = 9199;
	uint8_t reason_code = 0x93;
	std::string reason_string = "PUBACK reason string";
	std::string user_prop = "PUBACK user prop";

	puback_props pp;
	pp[prop::reason_string] = reason_string;
	pp[prop::user_property].emplace_back(user_prop);

	auto msg = encoders::encode_puback(packet_id, reason_code, pp);

	byte_citer it = msg.cbegin(), last = msg.cend();
	auto header = decoders::decode_fixed_header(it, last);
	BOOST_CHECK_MESSAGE(header, "Parsing PUBACK fixed header failed.");

	auto packet_id_ = decoders::decode_packet_id(it);
	BOOST_CHECK(packet_id);
	BOOST_CHECK_EQUAL(*packet_id_, packet_id);

	const auto& [control_byte, remain_length] = *header;
	auto rv = decoders::decode_puback(remain_length, it);
	BOOST_CHECK_MESSAGE(rv, "Parsing PUBACK failed.");

	const auto& [reason_code_, pprops] = *rv;
	BOOST_CHECK_EQUAL(reason_code_, reason_code);
	BOOST_CHECK_EQUAL(*pprops[prop::reason_string], reason_string);
	BOOST_CHECK_EQUAL(pprops[prop::user_property][0], user_prop);
}

BOOST_AUTO_TEST_CASE(test_subscribe) {
	subscribe_props sp;
	std::vector<subscribe_topic> filters {
		{ "subscribe topic", { qos_e::at_least_once } }
	};
	uint16_t packet_id = 65535;

	auto msg = encoders::encode_subscribe(packet_id, filters, sp);

	byte_citer it = msg.cbegin(), last = msg.cend();
	auto header = decoders::decode_fixed_header(it, last);
	BOOST_CHECK_MESSAGE(header, "Parsing SUBSCRIBE fixed header failed.");

	const auto& [control_byte, remain_length] = *header;
	auto packet_id_ = decoders::decode_packet_id(it);
	BOOST_CHECK(packet_id);
	BOOST_CHECK_EQUAL(*packet_id_, packet_id);
	auto rv = decoders::decode_subscribe(remain_length - sizeof(uint16_t), it);
	BOOST_CHECK_MESSAGE(rv, "Parsing SUBSCRIBE failed.");

	const auto& [props_, filters_] = *rv;
	BOOST_CHECK_EQUAL(filters[0].topic_filter, std::get<0>(filters_[0]));
	//TODO: sub options
}

BOOST_AUTO_TEST_CASE(test_suback) {
	//testing variables
	suback_props sp;
	std::vector<uint8_t> reason_codes { 48, 28 };
	uint16_t packet_id = 142;

	auto msg = encoders::encode_suback(packet_id, reason_codes, sp);

	byte_citer it = msg.cbegin(), last = msg.cend();
	auto header = decoders::decode_fixed_header(it, last);
	BOOST_CHECK_MESSAGE(header, "Parsing SUBACK fixed header failed.");

	const auto& [control_byte, remain_length] = *header;
	auto packet_id_ = decoders::decode_packet_id(it);
	BOOST_CHECK(packet_id);
	BOOST_CHECK_EQUAL(*packet_id_, packet_id);
	auto rv = decoders::decode_suback(remain_length - sizeof(uint16_t), it);
	BOOST_CHECK_MESSAGE(rv, "Parsing SUBACK failed.");

	const auto& [sp_, reason_codes_] = *rv;
	BOOST_CHECK(reason_codes_ == reason_codes);
}

BOOST_AUTO_TEST_CASE(test_unsubscribe) {
	// testing variables
	unsubscribe_props sp;
	std::vector<std::string> topics { "first topic", "second/topic" };
	uint16_t packet_id = 14423;

	auto msg = encoders::encode_unsubscribe(packet_id, topics, sp);

	byte_citer it = msg.cbegin(), last = msg.cend();
	auto header = decoders::decode_fixed_header(it, last);
	BOOST_CHECK_MESSAGE(header, "Parsing UNSUBSCRIBE fixed header failed.");

	const auto& [control_byte, remain_length] = *header;
	auto packet_id_ = decoders::decode_packet_id(it);
	BOOST_CHECK(packet_id);
	BOOST_CHECK_EQUAL(*packet_id_, packet_id);
	auto rv = decoders::decode_unsubscribe(remain_length - sizeof(uint16_t), it);
	BOOST_CHECK_MESSAGE(rv, "Parsing UNSUBSCRIBE failed.");

	const auto& [props_, topics_] = *rv;
	BOOST_CHECK(topics_ == topics);
}

BOOST_AUTO_TEST_CASE(test_unsuback) {
	// testing variables
	std::string reason_string = "some unsuback reason string";
	unsuback_props sp;
	sp[prop::reason_string] = reason_string;
	std::vector<uint8_t> reason_codes { 48, 28 };
	uint16_t packet_id = 42;

	auto msg = encoders::encode_unsuback(packet_id, reason_codes, sp);

	byte_citer it = msg.cbegin(), last = msg.cend();
	auto header = decoders::decode_fixed_header(it, last);
	BOOST_CHECK_MESSAGE(header, "Parsing UNSUBACK fixed header failed.");

	const auto& [control_byte, remain_length] = *header;
	auto packet_id_ = decoders::decode_packet_id(it);
	BOOST_CHECK(packet_id);
	BOOST_CHECK_EQUAL(*packet_id_, packet_id);
	auto rv = decoders::decode_unsuback(remain_length - sizeof(uint16_t), it);
	BOOST_CHECK_MESSAGE(header, "Parsing UNSUBACK failed.");

	const auto& [props_, reason_codes_] = *rv;
	BOOST_CHECK(reason_codes_ == reason_codes);
	BOOST_CHECK_EQUAL(*props_[prop::reason_string], reason_string);
}

BOOST_AUTO_TEST_CASE(test_disconnect) {
	// testing variables
	uint8_t reason_code = 0;
	std::string user_property = "DISCONNECT user property";
	disconnect_props sp;
	sp[prop::user_property].emplace_back(user_property);

	auto msg = encoders::encode_disconnect(reason_code, sp);

	byte_citer it = msg.cbegin(), last = msg.cend();
	auto header = decoders::decode_fixed_header(it, last);
	BOOST_CHECK_MESSAGE(header, "Parsing DISCONNECT fixed header failed.");

	const auto& [control_byte, remain_length] = *header;
	auto rv = decoders::decode_disconnect(remain_length, it);
	BOOST_CHECK_MESSAGE(header, "Parsing DISCONNECT failed.");

	const auto& [reason_code_, props_] = *rv;
	BOOST_CHECK_EQUAL(reason_code_, reason_code);
	BOOST_CHECK_EQUAL(props_[prop::user_property][0], user_property);
}

BOOST_AUTO_TEST_CASE(test_auth) {
	// testing variables
	uint8_t reason_code = 0x18;
	std::string reason_string = "AUTH reason";
	std::string user_property = "AUTH user propety";
	auth_props sp;
	sp[prop::reason_string] = reason_string;
	sp[prop::user_property].emplace_back(user_property);

	auto msg = encoders::encode_auth(reason_code, sp);

	byte_citer it = msg.cbegin(), last = msg.cend();
	auto header = decoders::decode_fixed_header(it, last);
	BOOST_CHECK_MESSAGE(header, "Parsing AUTH fixed header failed.");

	const auto& [control_byte, remain_length] = *header;
	auto rv = decoders::decode_auth(remain_length, it);
	BOOST_CHECK_MESSAGE(rv, "Parsing AUTH failed.");

	const auto& [reason_code_, props_] = *rv;
	BOOST_CHECK_EQUAL(reason_code_, reason_code);
	BOOST_CHECK_EQUAL(*props_[prop::reason_string], reason_string);
	BOOST_CHECK_EQUAL(props_[prop::user_property][0], user_property);
}

BOOST_AUTO_TEST_SUITE_END()
