#include <boost/test/unit_test.hpp>

#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>

#include <async_mqtt5/mqtt_client.hpp>

#include "test_common/message_exchange.hpp"
#include "test_common/test_service.hpp"
#include "test_common/test_stream.hpp"

using namespace async_mqtt5;

BOOST_AUTO_TEST_SUITE(publish_receive/*, *boost::unit_test::disabled()*/)

struct shared_test_data {
	error_code success {};
	error_code fail = asio::error::not_connected;

	const std::string connect = encoders::encode_connect(
		"", std::nullopt, std::nullopt, 10, false, {}, std::nullopt
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

	c.async_receive(
		[&](
			error_code ec,
			std::string rec_topic, std::string rec_payload,
			publish_props
		) {
			++handlers_called;

			BOOST_CHECK_MESSAGE(!ec, ec.message());
			BOOST_CHECK_EQUAL(shared_test_data().topic, rec_topic);
			BOOST_CHECK_EQUAL(shared_test_data().payload,  rec_payload);

			c.cancel();
		}
	);

	ioc.run_for(std::chrono::seconds(10));
	BOOST_CHECK_EQUAL(handlers_called, expected_handlers_called);
	BOOST_CHECK(broker.received_all_expected());
}


BOOST_FIXTURE_TEST_CASE(test_receive_publish_qos0, shared_test_data) {
	test::msg_exchange broker_side;
	broker_side
		.expect(connect)
			.complete_with(success, after(0ms))
			.reply_with(connack, after(0ms))
		.send(publish_qos0, after(10ms));

	run_test(std::move(broker_side));
}

BOOST_FIXTURE_TEST_CASE(test_receive_publish_qos1, shared_test_data) {
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

BOOST_FIXTURE_TEST_CASE(test_receive_publish_qos2, shared_test_data) {
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

BOOST_FIXTURE_TEST_CASE(fail_to_send_pubrec, shared_test_data) {
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

BOOST_AUTO_TEST_SUITE_END();
