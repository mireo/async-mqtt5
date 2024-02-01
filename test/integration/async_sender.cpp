#include <boost/test/unit_test.hpp>

#include <boost/asio/io_context.hpp>

#include <async_mqtt5/mqtt_client.hpp>

#include "test_common/message_exchange.hpp"
#include "test_common/test_service.hpp"
#include "test_common/test_stream.hpp"

using namespace async_mqtt5;

BOOST_AUTO_TEST_SUITE(async_sender/*, *boost::unit_test::disabled()*/)

struct shared_test_data {
	error_code success {};
	error_code fail = asio::error::not_connected;

	const std::string connect = encoders::encode_connect(
		"", std::nullopt, std::nullopt, 60, false, {}, std::nullopt
	);
	const std::string connack = encoders::encode_connack(
		false, reason_codes::success.value(), {}
	);

	const std::string topic = "topic";
	const std::string payload = "payload";
};

using test::after;
using namespace std::chrono;

BOOST_FIXTURE_TEST_CASE(ordering_after_reconnect, shared_test_data) {
	constexpr int expected_handlers_called = 2;
	int handlers_called = 0;

	// packets
	auto publish_qos1 = encoders::encode_publish(
		1, topic, payload, qos_e::at_least_once, retain_e::no, dup_e::no, {}
	);
	auto publish_qos1_dup = encoders::encode_publish(
		1, topic, payload, qos_e::at_least_once, retain_e::no, dup_e::yes, {}
	);

	auto publish_qos2 = encoders::encode_publish(
		2, topic, payload, qos_e::exactly_once, retain_e::no, dup_e::no, {}
	);

	auto puback = encoders::encode_puback(1, uint8_t(0x00), {});
	auto pubrec = encoders::encode_pubrec(2, uint8_t(0x00), {});
	auto pubrel = encoders::encode_pubrel(2, uint8_t(0x00), {});
	auto pubcomp = encoders::encode_pubcomp(2, uint8_t(0x00), {});

	test::msg_exchange broker_side;
	broker_side
		.expect(connect)
			.complete_with(success, after(1ms))
			.reply_with(connack, after(2ms))
		.expect(publish_qos1, publish_qos2)
			.complete_with(success, after(1ms))
			.reply_with(pubrec, after(2ms))
		.expect(pubrel)
			.complete_with(fail, after(1ms))
		.expect(connect)
			.complete_with(success, after(1ms))
			.reply_with(connack, after(2ms))
		.expect(pubrel, publish_qos1_dup)
			.complete_with(success, after(1ms))
			.reply_with(pubcomp, puback, after(2ms));

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
		[&](error_code ec, reason_code rc, puback_props) {
			++handlers_called;

			BOOST_CHECK_MESSAGE(!ec, ec.message());
			BOOST_CHECK_MESSAGE(!rc, rc.message());

			if (handlers_called == expected_handlers_called)
				c.cancel();
		}
	);

	c.async_publish<qos_e::exactly_once>(
		topic, payload, retain_e::no, publish_props{},
		[&](error_code ec, reason_code rc, pubcomp_props) {
			++handlers_called;

			BOOST_CHECK_MESSAGE(!ec, ec.message());
			BOOST_CHECK_MESSAGE(!rc, rc.message());

			if (handlers_called == expected_handlers_called)
				c.cancel();
		}
	);

	ioc.run_for(1s);
	BOOST_CHECK_EQUAL(handlers_called, expected_handlers_called);
	BOOST_CHECK(broker.received_all_expected());
}

BOOST_FIXTURE_TEST_CASE(throttling, shared_test_data) {
	constexpr int expected_handlers_called = 3;
	int handlers_called = 0;

	connack_props props;
	props[prop::receive_maximum] = int16_t(1);

	//packets
	auto connack_rm = encoders::encode_connack(
		false, reason_codes::success.value(), props
	);

	auto publish_1 = encoders::encode_publish(
		1, topic, payload, qos_e::at_least_once, retain_e::no, dup_e::no, {}
	);
	auto publish_2 = encoders::encode_publish(
		2, topic, payload, qos_e::at_least_once, retain_e::no, dup_e::no, {}
	);
	auto publish_3 = encoders::encode_publish(
		3, topic, payload, qos_e::at_least_once, retain_e::no, dup_e::no, {}
	);

	auto puback_1 = encoders::encode_puback(1, uint8_t(0x00), {});
	auto puback_2 = encoders::encode_puback(2, uint8_t(0x00), {});
	auto puback_3 = encoders::encode_puback(3, uint8_t(0x00), {});

	test::msg_exchange broker_side;
	broker_side
		.expect(connect)
			.complete_with(success, after(1ms))
			.reply_with(connack_rm, after(2ms))
		.expect(publish_1)
			.complete_with(success, after(1ms))
			.reply_with(puback_1, after(2ms))
		.expect(publish_2)
			.complete_with(success, after(1ms))
			.reply_with(puback_2, after(2ms))
		.expect(publish_3)
			.complete_with(success, after(1ms))
			.reply_with(puback_3, after(2ms));

	asio::io_context ioc;
	auto executor = ioc.get_executor();
	auto& broker = asio::make_service<test::test_broker>(
		ioc, executor, std::move(broker_side)
	);

	using client_type = mqtt_client<test::test_stream>;
	client_type c(executor, "");
	c.brokers("127.0.0.1")
		.async_run(asio::detached);

	for (auto i = 0; i < 3; i++)
		c.async_publish<qos_e::at_least_once>(
			topic, payload, retain_e::no, publish_props {},
			[&c, &handlers_called, i](error_code ec, reason_code rc, puback_props) {
				++handlers_called;

				BOOST_CHECK_MESSAGE(!ec, ec.message());
				BOOST_CHECK_MESSAGE(!rc, rc.message());
				BOOST_CHECK_EQUAL(handlers_called, i + 1);

				if (i == 2)
					c.cancel();
			}
		);

	ioc.run_for(1s);
	BOOST_CHECK_EQUAL(handlers_called, expected_handlers_called);
	BOOST_CHECK(broker.received_all_expected());
}

BOOST_AUTO_TEST_SUITE_END();