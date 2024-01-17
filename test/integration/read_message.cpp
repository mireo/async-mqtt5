#include <boost/test/unit_test.hpp>

#include <boost/asio/prepend.hpp>
#include <boost/asio/post.hpp>

#include <async_mqtt5/mqtt_client.hpp>

#include "test_common/message_exchange.hpp"
#include "test_common/test_service.hpp"
#include "test_common/test_stream.hpp"

using namespace async_mqtt5;

BOOST_AUTO_TEST_SUITE(read_message/*, *boost::unit_test::disabled()*/)

void test_receive_malformed_packet(
	std::string malformed_packet, std::string reason_string
) {
	using test::after;
	using std::chrono_literals::operator ""ms;

	// packets
	auto connect = encoders::encode_connect(
		"", std::nullopt, std::nullopt, 10, false, {}, std::nullopt
	);
	connack_props co_props;
	co_props[prop::maximum_packet_size] = 2000;
	auto connack = encoders::encode_connack(false, reason_codes::success.value(), co_props);

	disconnect_props dc_props;
	dc_props[prop::reason_string] = reason_string;
	auto disconnect = encoders::encode_disconnect(
		reason_codes::malformed_packet.value(), dc_props
	);

	test::msg_exchange broker_side;
	error_code success {};

	broker_side
		.expect(connect)
			.complete_with(success, after(0ms))
			.reply_with(connack, after(0ms))
		.send(malformed_packet, after(5ms))
		.expect(disconnect)
			.complete_with(success, after(0ms))
		.expect(connect)
			.complete_with(success, after(0ms))
			.reply_with(connack, after(0ms));


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
	timer.expires_after(std::chrono::milliseconds(100));
	timer.async_wait([&](auto) { c.cancel(); });

	ioc.run();
	BOOST_CHECK(broker.received_all_expected());
}

BOOST_AUTO_TEST_CASE(forbidden_packet_type) {
	test_receive_malformed_packet(
		std::string({ 0x00 }),
		"Malformed Packet received from the Server"
	);
}

BOOST_AUTO_TEST_CASE(malformed_varint) {
	test_receive_malformed_packet(
		std::string({ 0x10, -1 /* 0xFF */, -1, -1, -1 }),
		"Malformed Packet received from the Server"
	);
}

BOOST_AUTO_TEST_CASE(malformed_fixed_header) {
	test_receive_malformed_packet(
		std::string({ 0x60, 1, 0 }),
		"Malformed Packet received from the Server"
	);
}

BOOST_AUTO_TEST_CASE(packet_larger_than_allowed) {
	test_receive_malformed_packet(
		std::string({ 0x10, -1, -1, -1, 0 }),
		"Malformed Packet received from the Server"
	);
}

BOOST_AUTO_TEST_CASE(receive_malformed_publish) {
	test_receive_malformed_packet(
		std::string({ 0x30, 1, -1 }),
		"Malformed PUBLISH received: cannot decode"
	);
}

BOOST_AUTO_TEST_CASE(receive_disconnect) {
	using test::after;
	using std::chrono_literals::operator ""ms;

	// packets
	auto connect = encoders::encode_connect(
		"", std::nullopt, std::nullopt, 10, false, {}, std::nullopt
	);
	auto connack = encoders::encode_connack(false, reason_codes::success.value(), {});

	auto disconnect = encoders::encode_disconnect(0x00, {});

	test::msg_exchange broker_side;
	error_code success {};

	broker_side
		.expect(connect)
			.complete_with(success, after(0ms))
			.reply_with(connack, after(0ms))
		.send(disconnect, after(5ms))
		.expect(connect)
			.complete_with(success, after(0ms))
			.reply_with(connack, after(0ms));


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
	timer.expires_after(std::chrono::milliseconds(100));
	timer.async_wait([&](auto) { c.cancel(); });

	ioc.run();
	BOOST_CHECK(broker.received_all_expected());

}

BOOST_AUTO_TEST_CASE(receive_pingresp) {
	using test::after;
	using std::chrono_literals::operator ""ms;

	// packets
	auto connect = encoders::encode_connect(
		"", std::nullopt, std::nullopt, 10, false, {}, std::nullopt
	);
	auto connack = encoders::encode_connack(false, reason_codes::success.value(), {});

	auto pingresp = encoders::encode_pingresp();

	test::msg_exchange broker_side;
	error_code success {};

	broker_side
		.expect(connect)
			.complete_with(success, after(0ms))
			.reply_with(connack, after(0ms))
		.send(pingresp, after(10ms));


	asio::io_context ioc;
	auto executor = ioc.get_executor();
	auto& broker = asio::make_service<test::test_broker>(
		ioc, executor, std::move(broker_side)
	);

	using client_type = mqtt_client<test::test_stream>;
	client_type c(executor, "");
	c.brokers("127.0.0.1")
		.async_run(asio::detached);

	asio::steady_timer timer(c.get_executor());
	timer.expires_after(std::chrono::milliseconds(100));
	timer.async_wait([&](auto) { c.cancel(); });

	ioc.run();
	BOOST_CHECK(broker.received_all_expected());
}


BOOST_AUTO_TEST_CASE(send_byte_by_byte) {
	using test::after;
	using std::chrono_literals::operator ""ms;

	constexpr int expected_handlers_called = 1;
	int handlers_called = 0;

	// data
	std::string topic = "topic";
	std::string payload = "payload";

	// packets
	auto connect = encoders::encode_connect(
		"", std::nullopt, std::nullopt, 10, false, {}, std::nullopt
	);
	auto connack = encoders::encode_connack(false, reason_codes::success.value(), {});

	auto publish = encoders::encode_publish(
		0, topic, payload, qos_e::at_most_once, retain_e::no, dup_e::no, {}
	);

	test::msg_exchange broker_side;
	error_code success {};

	broker_side
		.expect(connect)
			.complete_with(success, after(0ms))
			.reply_with(connack, after(0ms));

	for (size_t i = 0; i < publish.size(); i++)
		broker_side.send(
			std::string { publish[i] }, after(std::chrono::milliseconds(i + 2))
		);

	asio::io_context ioc;
	auto executor = ioc.get_executor();
	auto& broker = asio::make_service<test::test_broker>(
		ioc, executor, std::move(broker_side)
	);

	using client_type = mqtt_client<test::test_stream>;
	client_type c(executor, "");
	c.brokers("127.0.0.1")
		.async_run(asio::detached);

	c.async_receive([&](
		error_code ec, std::string topic_, std::string payload_, publish_props
	) {
		handlers_called++;

		BOOST_CHECK(!ec);
		BOOST_CHECK_EQUAL(topic, topic_);
		BOOST_CHECK_EQUAL(payload, payload_);

		c.cancel();
	});

	ioc.run_for(std::chrono::milliseconds(100));
	BOOST_CHECK_EQUAL(handlers_called, expected_handlers_called);
	BOOST_CHECK(broker.received_all_expected());
}

BOOST_AUTO_TEST_SUITE_END();
