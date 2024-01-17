#include <boost/test/unit_test.hpp>

#include <async_mqtt5/mqtt_client.hpp>

#include "test_common/message_exchange.hpp"
#include "test_common/test_service.hpp"
#include "test_common/test_stream.hpp"

using namespace async_mqtt5;

struct shared_test_data {
	const std::string connect = encoders::encode_connect(
		"", std::nullopt, std::nullopt, 10, false, {}, std::nullopt
	);
	const std::string connack = encoders::encode_connack(
		true, reason_codes::success.value(), {}
	);

	std::string topic = "topic";
	std::string payload = "payload";

	const std::string publish_qos2 = encoders::encode_publish(
		1, topic, payload, qos_e::exactly_once, retain_e::no, dup_e::no, {}
	);
	const std::string publish_qos2_dup = encoders::encode_publish(
		1, topic, payload, qos_e::exactly_once, retain_e::no, dup_e::yes, {}
	);

	const std::string pubrec = encoders::encode_pubrec(1, uint8_t(0x00), {});
	const std::string pubrel = encoders::encode_pubrel(1, uint8_t(0x00), {});
	const std::string pubcomp = encoders::encode_pubcomp(1, uint8_t(0x00), {});

};

BOOST_FIXTURE_TEST_SUITE(malformed_packet, shared_test_data/* , *boost::unit_test::disabled()*/)

BOOST_AUTO_TEST_CASE(test_malformed_publish) {
	using test::after;
	using std::chrono_literals::operator ""ms;

	// packets
	auto publish = encoders::encode_publish(
		1, topic, payload, static_cast<qos_e>(0b11), retain_e::yes, dup_e::no, {}
	);

	disconnect_props dprops;
	dprops[prop::reason_string] = "Malformed PUBLISH received: QoS bits set to 0b11";
	auto disconnect = encoders::encode_disconnect(
		reason_codes::malformed_packet.value(), dprops
	);

	test::msg_exchange broker_side;
	error_code success {};

	broker_side
		.expect(connect)
			.complete_with(success, after(0ms))
			.reply_with(connack, after(0ms))
		.send(publish, after(10ms))
		.expect(disconnect)
			.complete_with(success, after(1ms));

	asio::io_context ioc;
	auto executor = ioc.get_executor();
	auto& broker = asio::make_service<test::test_broker>(
		ioc, executor, std::move(broker_side)
	);

	using client_type = mqtt_client<test::test_stream>;
	client_type c(executor, "");
	c.brokers("127.0.0.1,127.0.0.1") // to avoid reconnect backoff
		.async_run(asio::detached);

	asio::steady_timer timer(c.get_executor());
	timer.expires_after(std::chrono::seconds(2));
	timer.async_wait([&](auto) { c.cancel(); });

	ioc.run();
	BOOST_CHECK(broker.received_all_expected());
}

BOOST_AUTO_TEST_CASE(malformed_puback) {
	using test::after;
	using std::chrono_literals::operator ""ms;

	constexpr int expected_handlers_called = 1;
	int handlers_called = 0;

	// packets
	auto publish = encoders::encode_publish(
		1, topic, payload, qos_e::at_least_once, retain_e::no, dup_e::no, {}
	);
	auto publish_dup = encoders::encode_publish(
		1, topic, payload, qos_e::at_least_once, retain_e::no, dup_e::yes, {}
	);

	auto puback = encoders::encode_puback(1, uint8_t(0x00), {});
	auto malformed_puback = encoders::encode_puback(1, uint8_t(0x04), {});

	disconnect_props dc_props;
	dc_props[prop::reason_string] = "Malformed PUBACK: invalid Reason Code";
	auto disconnect = encoders::encode_disconnect(
		reason_codes::malformed_packet.value(), dc_props
	);

	test::msg_exchange broker_side;
	error_code success {};

	broker_side
		.expect(connect)
			.complete_with(success, after(0ms))
			.reply_with(connack, after(0ms))
		.expect(publish)
			.complete_with(success, after(0ms))
			.reply_with(malformed_puback, after(0ms))
		.expect(disconnect)
			.complete_with(success, after(0ms))
		.expect(connect)
			.complete_with(success, after(0ms))
			.reply_with(connack, after(0ms))
		.expect(publish_dup)
			.complete_with(success, after(0ms))
			.reply_with(puback, after(0ms));

	asio::io_context ioc;
	auto executor = ioc.get_executor();
	auto& broker = asio::make_service<test::test_broker>(
		ioc, executor, std::move(broker_side)
	);

	using client_type = mqtt_client<test::test_stream>;
	client_type c(executor, "");
	c.brokers("127.0.0.1,127.0.0.1") // to avoid reconnect backoff
		.async_run(asio::detached);

	c.async_publish<qos_e::at_least_once>(
		topic, payload, retain_e::no, publish_props {},
		[&](error_code ec, reason_code rc, auto) {
			++handlers_called;

			BOOST_CHECK(!ec);
			BOOST_CHECK_EQUAL(rc, reason_codes::success);

			c.cancel();
		}
	);

	ioc.run_for(std::chrono::seconds(2));
	BOOST_CHECK_EQUAL(handlers_called, expected_handlers_called);
	BOOST_CHECK(broker.received_all_expected());
}


BOOST_AUTO_TEST_CASE(malformed_pubrel) {
	using test::after;
	using std::chrono_literals::operator ""ms;

	constexpr int expected_handlers_called = 1;
	int handlers_called = 0;

	// packets
	auto malformed_pubrel = encoders::encode_pubrel(1, uint8_t(0x04), {});

	disconnect_props dprops;
	dprops[prop::reason_string] = "Malformed PUBREL received: invalid Reason Code";
	auto disconnect = encoders::encode_disconnect(
		reason_codes::malformed_packet.value(), dprops
	);

	test::msg_exchange broker_side;
	error_code success {};

	broker_side
		.expect(connect)
			.complete_with(success, after(0ms))
			.reply_with(connack, after(0ms))
		.send(publish_qos2, after(10ms))
		.expect(pubrec)
			.complete_with(success, after(1ms))
			.reply_with(malformed_pubrel, after(2ms))
		.expect(disconnect)
			.complete_with(success, after(1ms))
		.expect(connect)
			.complete_with(success, after(0ms))
			.reply_with(connack, after(0ms))
		.send(pubrel, after(100ms))
		.expect(pubcomp)
			.complete_with(success, after(1ms));

	asio::io_context ioc;
	auto executor = ioc.get_executor();
	auto& broker = asio::make_service<test::test_broker>(
		ioc, executor, std::move(broker_side)
	);

	using client_type = mqtt_client<test::test_stream>;
	client_type c(executor, "");
	c.brokers("127.0.0.1,127.0.0.1") // to avoid reconnect backoff
		.async_run(asio::detached);

	c.async_receive(
		[&](
			error_code ec,
			std::string rec_topic, std::string rec_payload,
			publish_props
		) {
			++handlers_called;

			BOOST_CHECK_MESSAGE(!ec, ec.message());
			BOOST_CHECK_EQUAL(topic, rec_topic);
			BOOST_CHECK_EQUAL(payload,  rec_payload);

			c.cancel();
		}
	);

	ioc.run_for(std::chrono::seconds(6));
	BOOST_CHECK_EQUAL(handlers_called, expected_handlers_called);
	BOOST_CHECK(broker.received_all_expected());
}

BOOST_AUTO_TEST_CASE(malformed_pubrec) {
	using test::after;
	using std::chrono_literals::operator ""ms;

	constexpr int expected_handlers_called = 1;
	int handlers_called = 0;

	// packets
	auto malformed_pubrec = encoders::encode_pubrec(1, uint8_t(0x04), {});

	disconnect_props dprops;
	dprops[prop::reason_string] = "Malformed PUBREC: invalid Reason Code";
	auto disconnect = encoders::encode_disconnect(
		reason_codes::malformed_packet.value(), dprops
	);

	test::msg_exchange broker_side;
	error_code success {};

	broker_side
		.expect(connect)
			.complete_with(success, after(0ms))
			.reply_with(connack, after(0ms))
		.expect(publish_qos2)
			.complete_with(success, after(0ms))
			.reply_with(malformed_pubrec, after(0ms))
		.expect(disconnect)
			.complete_with(success, after(0ms))
		.expect(connect)
			.complete_with(success, after(0ms))
			.reply_with(connack, after(0ms))
		.expect(publish_qos2_dup)
			.complete_with(success, after(0ms))
			.reply_with(pubrec, after(0ms))
		.expect(pubrel)
			.complete_with(success, after(0ms))
			.reply_with(pubcomp, after(0ms));

	asio::io_context ioc;
	auto executor = ioc.get_executor();
	auto& broker = asio::make_service<test::test_broker>(
		ioc, executor, std::move(broker_side)
	);

	using client_type = mqtt_client<test::test_stream>;
	client_type c(executor, "");
	c.brokers("127.0.0.1,127.0.0.1") // to avoid reconnect backoff
		.async_run(asio::detached);

	c.async_publish<qos_e::exactly_once>(
		topic, payload, retain_e::no, publish_props {},
		[&](error_code ec, reason_code rc, auto) {
			++handlers_called;

			BOOST_CHECK(!ec);
			BOOST_CHECK_EQUAL(rc, reason_codes::success);

			c.cancel();
		}
	);

	ioc.run_for(std::chrono::seconds(6));
	BOOST_CHECK_EQUAL(handlers_called, expected_handlers_called);
	BOOST_CHECK(broker.received_all_expected());
}


BOOST_AUTO_TEST_CASE(malformed_pubcomp) {
	using test::after;
	using std::chrono_literals::operator ""ms;

	constexpr int expected_handlers_called = 1;
	int handlers_called = 0;

	// packets
	auto malformed_pubcomp = encoders::encode_pubcomp(1, uint8_t(0x04), {});

	disconnect_props dprops;
	dprops[prop::reason_string] = "Malformed PUBCOMP: invalid Reason Code";
	auto disconnect = encoders::encode_disconnect(
		reason_codes::malformed_packet.value(), dprops
	);

	test::msg_exchange broker_side;
	error_code success {};

	broker_side
		.expect(connect)
			.complete_with(success, after(0ms))
			.reply_with(connack, after(0ms))
		.expect(publish_qos2)
			.complete_with(success, after(0ms))
			.reply_with(pubrec, after(0ms))
		.expect(pubrel)
			.complete_with(success, after(0ms))
			.reply_with(malformed_pubcomp, after(0ms))
		.expect(disconnect)
			.complete_with(success, after(0ms))
		.expect(connect)
			.complete_with(success, after(0ms))
			.reply_with(connack, after(0ms))
		.expect(pubrel)
			.complete_with(success, after(0ms))
			.reply_with(pubcomp, after(0ms));

	asio::io_context ioc;
	auto executor = ioc.get_executor();
	auto& broker = asio::make_service<test::test_broker>(
		ioc, executor, std::move(broker_side)
	);

	using client_type = mqtt_client<test::test_stream>;
	client_type c(executor, "");
	c.brokers("127.0.0.1,127.0.0.1") // to avoid reconnect backoff
		.async_run(asio::detached);

	c.async_publish<qos_e::exactly_once>(
		topic, payload, retain_e::no, publish_props {},
		[&](error_code ec, reason_code rc, auto) {
			++handlers_called;

			BOOST_CHECK(!ec);
			BOOST_CHECK_EQUAL(rc, reason_codes::success);

			c.cancel();
		}
	);

	ioc.run_for(std::chrono::seconds(6));
	BOOST_CHECK_EQUAL(handlers_called, expected_handlers_called);
	BOOST_CHECK(broker.received_all_expected());
}

BOOST_AUTO_TEST_SUITE_END();
