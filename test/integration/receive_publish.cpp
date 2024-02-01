#include <boost/test/unit_test.hpp>

#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>

#include <async_mqtt5/mqtt_client.hpp>

#include "test_common/message_exchange.hpp"
#include "test_common/packet_util.hpp"
#include "test_common/test_broker.hpp"
#include "test_common/test_service.hpp"
#include "test_common/test_stream.hpp"

using namespace async_mqtt5;

BOOST_AUTO_TEST_SUITE(receive_publish/*, *boost::unit_test::disabled()*/)

struct shared_test_data {
	error_code success {};
	error_code fail = asio::error::not_connected;

	connect_props cprops = allow_big_packets_cprops();
	const std::string connect_with_cprops = encoders::encode_connect(
		"", std::nullopt, std::nullopt, 60, false, cprops, std::nullopt
	);

	const std::string connect = encoders::encode_connect(
		"", std::nullopt, std::nullopt, 60, false, {}, std::nullopt
	);
	const std::string connack = encoders::encode_connack(
		true, reason_codes::success.value(), {}
	);

	const std::string topic = "topic";
	const std::string payload = "payload";

	const std::string publish_qos0 = encoders::encode_publish(
		0, topic, payload, qos_e::at_most_once, retain_e::no, dup_e::no, {}
	);
	const std::string publish_qos1 = encoders::encode_publish(
		1, topic, payload, qos_e::at_least_once, retain_e::no, dup_e::no, {}
	);
	const std::string publish_qos2 = encoders::encode_publish(
		1, topic, payload, qos_e::exactly_once, retain_e::no, dup_e::no, {}
	);

	const std::string puback = encoders::encode_puback(1, uint8_t(0x00), {});

	const std::string pubrec = encoders::encode_pubrec(1, uint8_t(0x00), {});
	const std::string pubrel = encoders::encode_pubrel(1, uint8_t(0x00), {});
	const std::string pubcomp = encoders::encode_pubcomp(1, uint8_t(0x00), {});


private:
	connect_props allow_big_packets_cprops() {
		connect_props c_props;
		c_props[prop::maximum_packet_size] = 10'000'000;
		return c_props;
	}
};

using test::after;
using namespace std::chrono;

void run_test(test::msg_exchange broker_side) {
	constexpr int expected_handlers_called = 1;
	int handlers_called = 0;

	asio::io_context ioc;
	auto executor = ioc.get_executor();
	auto& broker = asio::make_service<test::test_broker>(
		ioc, executor, std::move(broker_side)
	);

	using client_type = mqtt_client<test::test_stream>;
	client_type c(executor, "");
	c.brokers("127.0.0.1,127.0.0.1") // to avoid reconnect backoff
		.async_run(asio::detached);

	c.async_receive([&handlers_called, &c](
			error_code ec, std::string rec_topic, std::string rec_payload, publish_props
		){
			++handlers_called;

			auto data = shared_test_data();
			BOOST_TEST(!ec);
			BOOST_TEST(data.topic == rec_topic);
			BOOST_TEST(data.payload == rec_payload);

			c.cancel();
		}
	);

	ioc.run_for(3s);
	BOOST_TEST(handlers_called == expected_handlers_called);
	BOOST_TEST(broker.received_all_expected());
}

 
BOOST_FIXTURE_TEST_CASE(receive_publish_qos0, shared_test_data) {
	test::msg_exchange broker_side;
	broker_side
		.expect(connect)
			.complete_with(success, after(0ms))
			.reply_with(connack, after(0ms))
		.send(publish_qos0, after(10ms));

	run_test(std::move(broker_side));
}

BOOST_FIXTURE_TEST_CASE(receive_publish_qos1, shared_test_data) {
	test::msg_exchange broker_side;
	broker_side
		.expect(connect)
			.complete_with(success, after(0ms))
			.reply_with(connack, after(0ms))
		.send(publish_qos1, after(10ms))
		.expect(puback)
			.complete_with(success, after(1ms));

	run_test(std::move(broker_side));
}

BOOST_FIXTURE_TEST_CASE(receive_publish_qos2, shared_test_data) {
	test::msg_exchange broker_side;
	broker_side
		.expect(connect)
			.complete_with(success, after(0ms))
			.reply_with(connack, after(0ms))
		.send(publish_qos2, after(10ms))
		.expect(pubrec)
			.complete_with(success, after(1ms))
		.reply_with(pubrel, after(2ms))
			.expect(pubcomp)
		.complete_with(success, after(1ms));

	run_test(std::move(broker_side));
}

BOOST_FIXTURE_TEST_CASE(receive_malformed_publish, shared_test_data) {
	// packets
	auto malformed_publish = encoders::encode_publish(
		1, "malformed topic", "malformed payload",
		static_cast<qos_e>(0b11), retain_e::yes, dup_e::no, {}
	);

	auto disconnect = encoders::encode_disconnect(
		reason_codes::malformed_packet.value(),
		test::dprops_with_reason_string("Malformed PUBLISH received: QoS bits set to 0b11")
	);

	test::msg_exchange broker_side;
	broker_side
		.expect(connect)
			.complete_with(success, after(0ms))
			.reply_with(connack, after(0ms))
		.send(malformed_publish, after(10ms))
		.expect(disconnect)
			.complete_with(success, after(1ms))
		.expect(connect)
			.complete_with(success, after(0ms))
			.reply_with(connack, after(0ms))
		.send(publish_qos0, after(50ms));

	run_test(std::move(broker_side));
}

BOOST_FIXTURE_TEST_CASE(receive_malformed_pubrel, shared_test_data) {
	// packets
	auto malformed_pubrel = encoders::encode_pubrel(1, uint8_t(0x04), {});

	auto disconnect = encoders::encode_disconnect(
		reason_codes::malformed_packet.value(),
		test::dprops_with_reason_string("Malformed PUBREL received: invalid Reason Code")
	);

	test::msg_exchange broker_side;
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

	run_test(std::move(broker_side));
}

BOOST_FIXTURE_TEST_CASE(fail_to_send_puback, shared_test_data) {
	// packets
	auto publish_qos1_dup = encoders::encode_publish(
		1, topic, payload, qos_e::at_least_once, retain_e::no, dup_e::yes, {}
	);

	test::msg_exchange broker_side;
	broker_side
		.expect(connect)
			.complete_with(success, after(0ms))
			.reply_with(connack, after(0ms))
		.send(publish_qos1, after(3ms))
		.expect(puback)
			.complete_with(fail, after(1ms))
		.expect(connect)
			.complete_with(success, after(1ms))
			.reply_with(connack, after(2ms))
		.send(publish_qos1_dup, after(100ms))
		.expect(puback)
			.complete_with(success, after(1ms));

	run_test(std::move(broker_side));
}

BOOST_FIXTURE_TEST_CASE(fail_to_send_pubrec, shared_test_data) {
	// packets
	auto publish_qos2_dup = encoders::encode_publish(
		1, topic, payload, qos_e::exactly_once, retain_e::no, dup_e::yes, {}
	);

	test::msg_exchange broker_side;
	broker_side
		.expect(connect)
			.complete_with(success, after(0ms))
			.reply_with(connack, after(0ms))
		.send(publish_qos2, after(3ms))
		.expect(pubrec)
			.complete_with(fail, after(1ms))
		.expect(connect)
			.complete_with(success, after(0ms))
			.reply_with(connack, after(0ms))
		.send(publish_qos2_dup, after(100ms))
		.expect(pubrec)
			.complete_with(success, after(1ms))
			.reply_with(pubrel, after(2ms))
		.expect(pubcomp)
			.complete_with(success, after(1ms));

	run_test(std::move(broker_side));
}

BOOST_FIXTURE_TEST_CASE(broker_fails_to_receive_pubrec, shared_test_data) {
	// packets
	auto publish_dup = encoders::encode_publish(
		1, topic, payload, qos_e::exactly_once, retain_e::no, dup_e::yes, {}
	);

	test::msg_exchange broker_side;
	broker_side
		.expect(connect)
			.complete_with(success, after(0ms))
			.reply_with(connack, after(0ms))
		.send(publish_qos2, after(3ms))
		// write completed, but the broker did not actually
		// receive pubrec, it will resend publish again
		.expect(pubrec)
			.complete_with(success, after(1ms))
			.reply_with(fail, after(3ms))
		.expect(connect)
			.complete_with(success, after(0ms))
			.reply_with(connack, after(0ms))
		.send(publish_dup, after(100ms))
		.expect(pubrec)
			.complete_with(success, after(1ms))
			.reply_with(pubrel, after(2ms))
		.expect(pubcomp)
			.complete_with(success, after(1ms));

	run_test(std::move(broker_side));
}

BOOST_FIXTURE_TEST_CASE(fail_to_send_pubcomp, shared_test_data) {
	test::msg_exchange broker_side;
	broker_side
		.expect(connect)
			.complete_with(success, after(0ms))
			.reply_with(connack, after(0ms))
		.send(publish_qos2, after(10ms))
		.expect(pubrec)
			.complete_with(success, after(1ms))
			.reply_with(pubrel, after(2ms))
		.expect(pubcomp)
			.complete_with(fail, after(1ms))
		.expect(connect)
			.complete_with(success, after(0ms))
			.reply_with(connack, after(0ms))
		.send(pubrel, after(10ms))
		.expect(pubcomp)
			.complete_with(success, after(1ms));

	run_test(std::move(broker_side));
}

BOOST_FIXTURE_TEST_CASE(receive_big_publish, shared_test_data) {
	const int expected_handlers_called = 1;
	int handlers_called = 0;

	// data
	publish_props big_props;
	for (int i = 0; i < 100; i++)
		big_props[prop::user_property].push_back(std::string(65534, 'u'));

	// packets
	auto big_publish = encoders::encode_publish(
		1, topic, payload,
		qos_e::at_most_once, retain_e::no, dup_e::no,
		std::move(big_props)
	);

	test::msg_exchange broker_side;
	broker_side
		.expect(connect_with_cprops)
			.complete_with(success, after(1ms))
			.reply_with(connack, after(2ms))
		.send(big_publish, after(1s));

	asio::io_context ioc;
	auto executor = ioc.get_executor();
	auto& broker = asio::make_service<test::test_broker>(
		ioc, executor, std::move(broker_side)
	);

	using client_type = mqtt_client<test::test_stream>;
	client_type c(executor, "");
	c.brokers("127.0.0.1")
		.connect_properties(cprops)
		.async_run(asio::detached);

	c.async_receive([&](
		error_code ec, std::string topic_, std::string payload_, publish_props pprops
		) {
			handlers_called++;

			BOOST_TEST(!ec);
			BOOST_TEST(topic == topic_);
			BOOST_TEST(payload == payload_);
			BOOST_TEST(pprops[prop::user_property].size() == 100u);

			c.cancel();
		});

	ioc.run();
	BOOST_TEST(handlers_called == expected_handlers_called);
	BOOST_TEST(broker.received_all_expected());
}

BOOST_AUTO_TEST_SUITE_END();
