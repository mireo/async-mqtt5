#include <boost/test/unit_test.hpp>

#include <async_mqtt5/mqtt_client.hpp>

#include "test_common/message_exchange.hpp"
#include "test_common/test_service.hpp"
#include "test_common/test_stream.hpp"

using namespace async_mqtt5;

BOOST_AUTO_TEST_SUITE(resending/* , *boost::unit_test::disabled()*/)

BOOST_AUTO_TEST_CASE(resend_multiple_publishes) {
	using test::after;
	using std::chrono_literals::operator ""ms;

	constexpr int expected_handlers_called = 2;
	int handlers_called = 0;

	// packets
	auto connect = encoders::encode_connect(
		"", std::nullopt, std::nullopt, 10, false, {}, std::nullopt
	);
	auto connack = encoders::encode_connack(
		false, reason_codes::success.value(), {}
	);
	auto publish_1 = encoders::encode_publish(
		1, "t", "p_1", qos_e::at_least_once, retain_e::no, dup_e::no, {}
	);
	auto puback_1 = encoders::encode_puback(
		1, reason_codes::success.value(), {}
	);
	auto publish_2 = encoders::encode_publish(
		2, "t", "p_2", qos_e::at_least_once, retain_e::no, dup_e::no, {}
	);
	auto puback_2 = encoders::encode_puback(
		2, reason_codes::success.value(), {}
	);

	test::msg_exchange broker_side;
	error_code success {};
	error_code fail = asio::error::not_connected;

	broker_side
		.expect(connect)
			.complete_with(success, after(10ms))
			.reply_with(connack, after(10ms))
		.expect(publish_1, publish_2)
			.complete_with(fail, after(10ms))
		.expect(connect)
			.complete_with(success, after(10ms))
			.reply_with(connack, after(10ms))
		.expect(publish_1, publish_2)
			.complete_with(success, after(10ms))
			.reply_with(puback_1, puback_2, after(20ms));

	asio::io_context ioc;
	auto executor = ioc.get_executor();
	auto& broker = asio::make_service<test::test_broker>(
		ioc, executor, std::move(broker_side)
	);

	using client_type = mqtt_client<test::test_stream>;
	client_type c(executor, "");
	c.brokers("127.0.0.1,127.0.0.1") // to avoid reconnect backoff
		.run();

	c.async_publish<qos_e::at_least_once>(
		"t", "p_1", retain_e::no, publish_props{},
		[&](error_code ec, reason_code rc, auto) {
			BOOST_CHECK_MESSAGE(!ec, ec.message());
			BOOST_CHECK_MESSAGE(!rc, rc.message());
			++handlers_called;

			if (handlers_called == expected_handlers_called)
				c.cancel();
		}
	);

	c.async_publish<qos_e::at_least_once>(
		"t", "p_2", retain_e::no, publish_props{},
		[&](error_code ec, reason_code rc, auto) {
			BOOST_CHECK_MESSAGE(!ec, ec.message());
			BOOST_CHECK_MESSAGE(!rc, rc.message());
			++handlers_called;

			if (handlers_called == expected_handlers_called)
				c.cancel();
		}
	);

	ioc.run_for(std::chrono::seconds(6));
	BOOST_CHECK_EQUAL(handlers_called, expected_handlers_called);
	BOOST_CHECK(broker.received_all_expected());
}

BOOST_AUTO_TEST_CASE(resend_pubrel) {
	using test::after;
	using std::chrono_literals::operator ""ms;

	constexpr int expected_handlers_called = 1;
	int handlers_called = 0;

	// packets
	auto connect = encoders::encode_connect(
		"", std::nullopt, std::nullopt, 10, false, {}, std::nullopt
	);
	auto connack = encoders::encode_connack(
		false, reason_codes::success.value(), {}
	);
	auto publish = encoders::encode_publish(
		1, "t_1", "p_1", qos_e::exactly_once, retain_e::no, dup_e::no, {}
	);
	auto pubrec = encoders::encode_pubrec(
		1, reason_codes::success.value(), {}
	);
	auto pubrel = encoders::encode_pubrel(
		1, reason_codes::success.value(), {}
	);
	auto pubcomp = encoders::encode_pubcomp(
		1, reason_codes::success.value(), {}
	);

	test::msg_exchange broker_side;
	error_code success {};
	error_code fail = asio::error::not_connected;

	broker_side
		.expect(connect)
			.complete_with(success, after(10ms))
			.reply_with(connack, after(20ms))
		.expect(publish)
			.complete_with(success, after(10ms))
			.reply_with(pubrec, after(25ms))
		.expect(pubrel)
			.complete_with(success, after(10ms))
			.reply_with(fail, after(10ms))
		.expect(connect)
			.complete_with(success, after(10ms))
			.reply_with(connack, after(20ms))
		.expect(pubrel)
			.complete_with(success, after(10ms))
			.reply_with(pubcomp, after(20ms));

	asio::io_context ioc;
	auto executor = ioc.get_executor();
	auto& broker = asio::make_service<test::test_broker>(
		ioc, executor, std::move(broker_side)
	);

	using client_type = mqtt_client<test::test_stream>;
	client_type c(executor, "");

	c.brokers("127.0.0.1,127.0.0.1") // to avoid reconnect backoff
		.run();

	c.async_publish<qos_e::exactly_once>(
		"t_1", "p_1", retain_e::no, publish_props{},
		[&](error_code ec, reason_code rc, auto) {
			BOOST_CHECK_MESSAGE(!ec, ec.message());
			BOOST_CHECK_MESSAGE(!rc, rc.message());
			++handlers_called;
			c.cancel();
		}
	);

	ioc.run_for(std::chrono::seconds(6));
	BOOST_CHECK_EQUAL(handlers_called, expected_handlers_called);
	BOOST_CHECK(broker.received_all_expected());
}

BOOST_AUTO_TEST_CASE(resend_subscribe) {
	using test::after;
	using std::chrono_literals::operator ""ms;

	constexpr int expected_handlers_called = 1;
	int handlers_called = 0;

	// data
	std::vector<subscribe_topic> topics = {
		subscribe_topic { "topic", subscribe_options {} }
	};
	subscribe_props subscribe_props;

	std::vector<uint8_t> rcs = { reason_codes::granted_qos_0.value() };
	suback_props suback_props;

	// packets
	auto connect = encoders::encode_connect(
		"", std::nullopt, std::nullopt, 10, false, {}, std::nullopt
	);
	auto connack = encoders::encode_connack(false, reason_codes::success.value(), {});

	auto subscribe = encoders::encode_subscribe(1, topics, subscribe_props);
	auto suback = encoders::encode_suback(1, rcs, suback_props);

	test::msg_exchange broker_side;
	error_code success {};
	error_code fail = asio::error::not_connected;

	broker_side
		.expect(connect)
			.complete_with(success, after(0ms))
			.reply_with(connack, after(0ms))
		.expect(subscribe)
			.complete_with(fail, after(0ms))
		.expect(connect)
			.complete_with(success, after(0ms))
			.reply_with(connack, after(0ms))
		.expect(subscribe)
			.complete_with(success, after(0ms))
			.reply_with(suback, after(0ms));

	asio::io_context ioc;
	auto executor = ioc.get_executor();
	auto& broker = asio::make_service<test::test_broker>(
		ioc, executor, std::move(broker_side)
	);

	using client_type = mqtt_client<test::test_stream>;
	client_type c(executor, "");
	c.brokers("127.0.0.1,127.0.0.1") // to avoid reconnect backoff
		.run();

	c.async_subscribe(
		topics, subscribe_props,
		[&](error_code ec, std::vector<reason_code> rcs, auto) {
			handlers_called++;

			BOOST_CHECK(!ec);
			BOOST_ASSERT(rcs.size() == 1);
			BOOST_CHECK_EQUAL(rcs[0], reason_codes::granted_qos_0);

			c.cancel();
		}
	);

	ioc.run_for(std::chrono::seconds(10));
	BOOST_CHECK_EQUAL(handlers_called, expected_handlers_called);
	BOOST_CHECK(broker.received_all_expected());
}

BOOST_AUTO_TEST_CASE(resend_unsubscribe) {
	using test::after;
	using std::chrono_literals::operator ""ms;

	constexpr int expected_handlers_called = 1;
	int handlers_called = 0;

	// data
	std::vector<std::string> topics = { "topic" };
	unsubscribe_props unsubscribe_props;

	std::vector<uint8_t> rcs = { reason_codes::success.value() };
	unsuback_props unsuback_props;

	// packets
	auto connect = encoders::encode_connect(
		"", std::nullopt, std::nullopt, 10, false, {}, std::nullopt
	);
	auto connack = encoders::encode_connack(false, reason_codes::success.value(), {});

	auto unsubscribe = encoders::encode_unsubscribe(1, topics, unsubscribe_props);
	auto unsuback = encoders::encode_unsuback(1, rcs, unsuback_props);

	test::msg_exchange broker_side;
	error_code success {};
	error_code fail = asio::error::not_connected;

	broker_side
		.expect(connect)
			.complete_with(success, after(0ms))
			.reply_with(connack, after(0ms))
		.expect(unsubscribe)
			.complete_with(fail, after(0ms))
		.expect(connect)
			.complete_with(success, after(0ms))
			.reply_with(connack, after(0ms))
		.expect(unsubscribe)
			.complete_with(success, after(0ms))
			.reply_with(unsuback, after(0ms));

	asio::io_context ioc;
	auto executor = ioc.get_executor();
	auto& broker = asio::make_service<test::test_broker>(
		ioc, executor, std::move(broker_side)
	);

	using client_type = mqtt_client<test::test_stream>;
	client_type c(executor, "");
	c.brokers("127.0.0.1,127.0.0.1") // to avoid reconnect backoff
		.run();

	c.async_unsubscribe(
		topics, unsubscribe_props,
		[&](error_code ec, std::vector<reason_code> rcs, auto) {
			handlers_called++;

			BOOST_CHECK(!ec);
			BOOST_ASSERT(rcs.size() == 1);
			BOOST_CHECK_EQUAL(rcs[0], reason_codes::success);

			c.cancel();
		}
	);

	ioc.run_for(std::chrono::seconds(10));
	BOOST_CHECK_EQUAL(handlers_called, expected_handlers_called);
	BOOST_CHECK(broker.received_all_expected());
}

BOOST_AUTO_TEST_SUITE_END();
