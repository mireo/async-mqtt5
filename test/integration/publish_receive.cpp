#include <boost/test/unit_test.hpp>

#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>

#include <async_mqtt5/mqtt_client.hpp>

#include "test_common/message_exchange.hpp"
#include "test_common/test_service.hpp"
#include "test_common/test_stream.hpp"

using namespace async_mqtt5;

BOOST_AUTO_TEST_SUITE(publish_receive/*, *boost::unit_test::disabled()*/)

template <qos_e qos>
void receive_publish() {
	using test::after;
	using std::chrono_literals::operator ""ms;

	constexpr int expected_handlers_called = 1;
	int handlers_called = 0;

	std::string topic = "topic";
	std::string payload = "payload";

	// packets
	auto connect = encoders::encode_connect(
		"", std::nullopt, std::nullopt, 10, false, {}, std::nullopt
	);
	auto connack = encoders::encode_connack(
		false, reason_codes::success.value(), {}
	);

	auto publish = encoders::encode_publish(
		1, topic, payload, qos, retain_e::no, dup_e::no, {}
	);

	auto puback = encoders::encode_puback(1, reason_codes::success.value(), {});

	auto pubrec = encoders::encode_pubrec(1, reason_codes::success.value(), {});
	auto pubrel = encoders::encode_pubrel(1, reason_codes::success.value(), {});
	auto pubcomp = encoders::encode_pubcomp(1, reason_codes::success.value(), {});

	test::msg_exchange broker_side;
	error_code success {};

	broker_side
		.expect(connect)
			.complete_with(success, after(0ms))
			.reply_with(connack, after(0ms))
		.send(publish, after(10ms));

	if constexpr (qos == qos_e::at_least_once) {
		broker_side
			.expect(puback)
				.complete_with(success, after(1ms));
	} else if constexpr (qos == qos_e::exactly_once) {
		broker_side
			.expect(pubrec)
				.complete_with(success, after(1ms))
				.reply_with(pubrel, after(2ms))
			.expect(pubcomp)
				.complete_with(success, after(1ms));
	}

	asio::io_context ioc;
	auto executor = ioc.get_executor();
	auto& broker = asio::make_service<test::test_broker>(
		ioc, executor, std::move(broker_side)
	);

	using client_type = mqtt_client<test::test_stream>;
	client_type c(executor, "");
	c.brokers("127.0.0.1")
		.run();

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

	ioc.run_for(std::chrono::seconds(10));
	BOOST_CHECK_EQUAL(handlers_called, expected_handlers_called);
	BOOST_CHECK(broker.received_all_expected());
}

BOOST_AUTO_TEST_CASE(test_receive_publish_qos0) {
	receive_publish<qos_e::at_most_once>();
}

BOOST_AUTO_TEST_CASE(test_receive_publish_qos1) {
	receive_publish<qos_e::at_least_once>();
}

BOOST_AUTO_TEST_CASE(test_receive_publish_qos2) {
	receive_publish<qos_e::exactly_once>();
}

BOOST_AUTO_TEST_CASE(test_waiting_on_pubrel) {
	using test::after;
	using std::chrono_literals::operator ""ms;

	constexpr int expected_handlers_called = 1;
	int handlers_called = 0;

	std::string topic = "topic";
	std::string payload = "payload";

	// packets
	auto connect = encoders::encode_connect(
		"", std::nullopt, std::nullopt, 10, false, {}, std::nullopt
	);
	auto connack = encoders::encode_connack(
		false, reason_codes::success.value(), {}
	);

	auto publish = encoders::encode_publish(
		1, topic, payload, qos_e::exactly_once, retain_e::yes, dup_e::no, {}
	);

	auto pubrec = encoders::encode_pubrec(1, reason_codes::success.value(), {});
	auto pubrel = encoders::encode_pubrel(1, reason_codes::success.value(), {});
	auto pubcomp = encoders::encode_pubcomp(1, reason_codes::success.value(), {});

	test::msg_exchange broker_side;
	error_code success {};
	error_code fail = asio::error::not_connected;

	broker_side
		.expect(connect)
			.complete_with(success, after(0ms))
			.reply_with(connack, after(0ms))
		.send(publish, after(10ms))
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

	asio::io_context ioc;
	auto executor = ioc.get_executor();
	auto& broker = asio::make_service<test::test_broker>(
		ioc, executor, std::move(broker_side)
	);

	using client_type = mqtt_client<test::test_stream>;
	client_type c(executor, "");
	c.brokers("127.0.0.1,127.0.0.1") // to avoid reconnect backoff
		.run();

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

	ioc.run_for(std::chrono::seconds(10));
	BOOST_CHECK_EQUAL(handlers_called, expected_handlers_called);
	BOOST_CHECK(broker.received_all_expected());
}

BOOST_AUTO_TEST_SUITE_END();
