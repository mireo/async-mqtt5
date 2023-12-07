#include <boost/test/unit_test.hpp>

#include <chrono>

#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>

#include <async_mqtt5/impl/codecs/message_encoders.hpp>

#include <async_mqtt5.hpp>

#include "test_common/message_exchange.hpp"
#include "test_common/test_stream.hpp"

using namespace async_mqtt5;

BOOST_AUTO_TEST_SUITE(framework, *boost::unit_test::disabled())

BOOST_AUTO_TEST_CASE(publish_qos_0) {
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
	auto publish_1 = encoders::encode_publish(
		65535, "t", "p_1", qos_e::at_most_once, retain_e::no, dup_e::no, {}
	);

	test::msg_exchange broker_side;
	error_code success {};

	broker_side
		.expect(connect)
			.complete_with(success, after(10ms))
			.reply_with(connack, after(20ms))
		.expect(publish_1);

	asio::io_context ioc;
	auto executor = ioc.get_executor();
	asio::make_service<test::test_broker>(ioc, executor, std::move(broker_side));

	using client_type = mqtt_client<test::test_stream>;
	client_type c(executor, "");
	c.brokers("127.0.0.1")
		.run();

	c.async_publish<qos_e::at_most_once>(
		"t", "p_1", retain_e::no, publish_props{},
		[&](error_code ec) {
			BOOST_CHECK_MESSAGE(!ec, ec.message());
			++handlers_called;
		}
	);

	asio::steady_timer timer(c.get_executor());
	timer.expires_after(std::chrono::seconds(1));
	timer.async_wait([&](auto) { c.cancel(); });


	ioc.run();
	BOOST_CHECK_EQUAL(handlers_called, expected_handlers_called);
}


BOOST_AUTO_TEST_CASE(two_publishes_qos_1_with_fail_on_write) {
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
		.expect(publish_1)
			.complete_with(fail, after(10ms))
		.expect(connect)
			.complete_with(success, after(10ms))
			.reply_with(connack, after(10ms))
		.expect(publish_1, publish_2)
			.complete_with(success, after(10ms))
			.reply_with(puback_1, puback_2, after(20ms));

	asio::io_context ioc;
	auto executor = ioc.get_executor();
	asio::make_service<test::test_broker>(ioc, executor, std::move(broker_side));

	using client_type = mqtt_client<test::test_stream>;
	client_type c(executor, "");
	c.brokers("127.0.0.1")
		.run();

	c.async_publish<qos_e::at_least_once>(
		"t", "p_1", retain_e::no, publish_props{},
		[&](error_code ec, reason_code rc, auto) {
			BOOST_CHECK_MESSAGE(!ec, ec.message());
			BOOST_CHECK_MESSAGE(!rc, rc.message());
			++handlers_called;
		}
	);

	c.async_publish<qos_e::at_least_once>(
		"t", "p_2", retain_e::no, publish_props{},
		[&](error_code ec, reason_code rc, auto) {
			BOOST_CHECK_MESSAGE(!ec, ec.message());
			BOOST_CHECK_MESSAGE(!rc, rc.message());
			++handlers_called;
		}
	);

	asio::steady_timer timer(c.get_executor());
	timer.expires_after(std::chrono::seconds(6));
	timer.async_wait([&](auto) { c.cancel(); });

	ioc.run();
	BOOST_CHECK_EQUAL(handlers_called, expected_handlers_called);
}

BOOST_AUTO_TEST_CASE(receive_publish_qos_2) {
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
		65535, topic, payload, qos_e::exactly_once, retain_e::no, dup_e::no, {}
	);
	auto pubrec = encoders::encode_pubrec(
		65535, reason_codes::success.value(), {}
	);
	auto pubrel = encoders::encode_pubrel(
		65535, reason_codes::success.value(), {}
	);
	auto pubcomp = encoders::encode_pubcomp(
		65535, reason_codes::success.value(), {}
	);

	test::msg_exchange broker_side;
	error_code success {};

	broker_side
		.expect(connect)
			.complete_with(success, after(10ms))
			.reply_with(connack, after(15ms))
		.send(publish, after(300ms))
		.expect(pubrec)
			.complete_with(success, after(10ms))
			.reply_with(pubrel, after(15ms))
		.expect(pubcomp)
			.complete_with(success, after(5ms));


	asio::io_context ioc;
	auto executor = ioc.get_executor();
	asio::make_service<test::test_broker>(ioc, executor, std::move(broker_side));

	using client_type = mqtt_client<test::test_stream>;
	client_type c(executor, "");
	c.brokers("127.0.0.1")
		.run();

	c.async_receive(
		[&](error_code ec, std::string rec_topic, std::string rec_payload, publish_props)
		{
			BOOST_CHECK_MESSAGE(!ec, ec.message());
			BOOST_CHECK_EQUAL(topic, rec_topic);
			BOOST_CHECK_EQUAL(payload,  rec_payload);
			++handlers_called;
		}
	);

	asio::steady_timer timer(c.get_executor());
	timer.expires_after(std::chrono::seconds(1));
	timer.async_wait([&](auto) { c.cancel(); });

	ioc.run();
	BOOST_CHECK_EQUAL(handlers_called, expected_handlers_called);
}

BOOST_AUTO_TEST_CASE(send_publish_qos_2_with_fail_on_read) {
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
		65535, "t_1", "p_1", qos_e::exactly_once, retain_e::no, dup_e::no, {}
	);
	auto pubrec = encoders::encode_pubrec(
		65535, reason_codes::success.value(), {}
	);
	auto pubrel = encoders::encode_pubrel(
		65535, reason_codes::success.value(), {}
	);
	auto pubcomp = encoders::encode_pubcomp(
		65535, reason_codes::success.value(), {}
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
	asio::make_service<test::test_broker>(ioc, executor, std::move(broker_side));

	using client_type = mqtt_client<test::test_stream>;
	client_type c(executor, "");

	c.brokers("127.0.0.1")
		.run();

	c.async_publish<qos_e::exactly_once>(
		"t_1", "p_1", retain_e::no, publish_props{},
		[&](error_code ec, reason_code rc, auto) {
			BOOST_CHECK_MESSAGE(!ec, ec.message());
			BOOST_CHECK_MESSAGE(!rc, rc.message());
			++handlers_called;
		}
	);

	asio::steady_timer timer(c.get_executor());
	timer.expires_after(std::chrono::seconds(7));
	timer.async_wait([&](auto) { c.cancel(); });

	ioc.run();
	BOOST_CHECK_EQUAL(handlers_called, expected_handlers_called);
}

BOOST_AUTO_TEST_CASE(test_ordering_after_reconnect) {
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
		65535, "t_1", "p_1", qos_e::at_least_once, retain_e::no, dup_e::no, {}
	);
	auto publish_1_dup = encoders::encode_publish(
		65535, "t_1", "p_1", qos_e::at_least_once, retain_e::no, dup_e::yes, {}
	);
	auto puback = encoders::encode_puback(
		65535, reason_codes::success.value(), {}
	);
	auto publish_2 = encoders::encode_publish(
		65534, "t_2", "p_2", qos_e::exactly_once, retain_e::no, dup_e::no, {}
	);
	auto pubrec = encoders::encode_pubrec(
		65534, reason_codes::success.value(), {}
	);
	auto pubrel = encoders::encode_pubrel(
		65534, reason_codes::success.value(), {}
	);
	auto pubcomp = encoders::encode_pubcomp(
		65534, reason_codes::success.value(), {}
	);

	test::msg_exchange broker_side;
	error_code success {};
	error_code fail = asio::error::not_connected;

	broker_side
		.expect(connect)
			.complete_with(success, after(10ms))
			.reply_with(connack, after(20ms))
		.expect(publish_1, publish_2)
			.complete_with(success, after(10ms))
			.reply_with(pubrec, after(20ms))
		.expect(pubrel)
			.complete_with(fail, after(10ms))
		.expect(connect)
			.complete_with(success, after(10ms))
			.reply_with(connack, after(15ms))
		.expect(pubrel, publish_1_dup)
			.complete_with(success, after(10ms))
			.reply_with(pubcomp, puback, after(20ms));


	asio::io_context ioc;
	auto executor = ioc.get_executor();
	asio::make_service<test::test_broker>(ioc, executor, std::move(broker_side));

	using client_type = mqtt_client<test::test_stream>;
	client_type c(executor, "");
	c.brokers("127.0.0.1")
		.run();

	c.async_publish<qos_e::at_least_once>(
		"t_1", "p_1", retain_e::no, publish_props{},
		[&](error_code ec, reason_code rc, auto) {
			BOOST_CHECK_MESSAGE(!ec, ec.message());
			BOOST_CHECK_MESSAGE(!rc, rc.message());
			++handlers_called;
		}
	);

	c.async_publish<qos_e::exactly_once>(
		"t_2", "p_2", retain_e::no, publish_props{},
		[&](error_code ec, reason_code rc, auto) {
			BOOST_CHECK_MESSAGE(!ec, ec.message());
			BOOST_CHECK_MESSAGE(!rc, rc.message());
			++handlers_called;
		}
	);

	asio::steady_timer timer(c.get_executor());
	timer.expires_after(std::chrono::seconds(7));
	timer.async_wait([&](auto) { c.cancel(); });

	ioc.run();
	BOOST_CHECK_EQUAL(handlers_called, expected_handlers_called);
}

BOOST_AUTO_TEST_CASE(throttling) {
	using test::after;
	using std::chrono_literals::operator ""ms;

	constexpr int expected_handlers_called = 3;
	int handlers_called = 0;

	connack_props props;
	props[prop::receive_maximum] = 1;

	//packets
	auto connect = encoders::encode_connect(
		"", std::nullopt, std::nullopt, 10, false, {}, std::nullopt
	);
	auto connack = encoders::encode_connack(
		false, reason_codes::success.value(), props
	);
	auto publish_1 = encoders::encode_publish(
		65535, "t_1", "p_1", qos_e::at_least_once, retain_e::no, dup_e::no, {}
	);
	auto publish_2 = encoders::encode_publish(
		65534, "t_1", "p_2", qos_e::at_least_once, retain_e::no, dup_e::no, {}
	);
	auto publish_3 = encoders::encode_publish(
		65533, "t_1", "p_3", qos_e::at_least_once, retain_e::no, dup_e::no, {}
	);
	auto puback_1 = encoders::encode_puback(
		65535, reason_codes::success.value(), {}
	);
	auto puback_2 = encoders::encode_puback(
		65534, reason_codes::success.value(), {}
	);
	auto puback_3 = encoders::encode_puback(
		65533, reason_codes::success.value(), {}
	);

	test::msg_exchange broker_side;
	error_code success {};
	error_code fail = asio::error::not_connected;

	broker_side
		.expect(connect)
			.complete_with(success, after(10ms))
			.reply_with(connack, after(15ms))
		.expect(publish_1)
			.complete_with(success, after(10ms))
			.reply_with(puback_1, after(15ms))
		.expect(publish_2)
			.complete_with(success, after(10ms))
			.reply_with(puback_2, after(15ms))
		.expect(publish_3)
			.complete_with(success, after(10ms))
			.reply_with(puback_3, after(15ms));

	asio::io_context ioc;
	auto executor = ioc.get_executor();
	asio::make_service<test::test_broker>(ioc, executor, std::move(broker_side));

	using client_type = mqtt_client<test::test_stream>;
	client_type c(executor, "");
	c.brokers("127.0.0.1")
		.run();

	c.async_publish<qos_e::at_least_once>(
		"t_1", "p_1", retain_e::no, publish_props{},
		[&](error_code ec, reason_code rc, auto) {
			BOOST_CHECK_MESSAGE(!ec, ec.message());
			BOOST_CHECK_MESSAGE(!rc, rc.message());
			BOOST_CHECK_EQUAL(handlers_called, 0);
			++handlers_called;
		}
	);


	c.async_publish<qos_e::at_least_once>(
		"t_1", "p_2", retain_e::no, publish_props{},
		[&](error_code ec, reason_code rc, auto) {
			BOOST_CHECK_MESSAGE(!ec, ec.message());
			BOOST_CHECK_MESSAGE(!rc, rc.message());
			BOOST_CHECK_EQUAL(handlers_called, 1);
			++handlers_called;
		}
	);

	c.async_publish<qos_e::at_least_once>(
		"t_1", "p_3", retain_e::no, publish_props{},
		[&](error_code ec, reason_code rc, auto) {
			BOOST_CHECK_MESSAGE(!ec, ec.message());
			BOOST_CHECK_MESSAGE(!rc, rc.message());
			BOOST_CHECK_EQUAL(handlers_called, 2);
			++handlers_called;
		}
	);

	asio::steady_timer timer(c.get_executor());
	timer.expires_after(std::chrono::seconds(2));
	timer.async_wait([&](auto) { c.cancel(); });

	ioc.run();
	BOOST_CHECK_EQUAL(handlers_called, expected_handlers_called);
}


BOOST_AUTO_TEST_CASE(cancel_multiple_ops) {
	using test::after;
	using namespace std::chrono;

	constexpr int expected_handlers_called = 1;
	int handlers_called = 0;

	auto begin = high_resolution_clock::now();

	// packets
	auto connect = encoders::encode_connect(
		"", std::nullopt, std::nullopt, 10, false, {}, std::nullopt
	);
	auto connack = encoders::encode_connack(
		false, reason_codes::success.value(), {}
	);
	auto publish_1 = encoders::encode_publish(
		65535, "t_1", "p_1", qos_e::at_least_once, retain_e::no, dup_e::no, {}
	);
	auto puback = encoders::encode_puback(
		65535, reason_codes::success.value(), {}
	);

	test::msg_exchange broker_side;
	error_code success{};
	error_code fail = asio::error::not_connected;

	broker_side
		.expect(connect)
			.complete_with(success, after(10ms))
			.reply_with(connack, after(20ms))
		.expect(publish_1)
			.complete_with(success, after(10s))
			.reply_with(puback, after(10s));
		//.send(publish_1, after(10s));

	asio::io_context ioc;
	auto executor = ioc.get_executor();
	asio::make_service<test::test_broker>(ioc, executor, std::move(broker_side));

	using client_type = mqtt_client<test::test_stream>;
	client_type c(executor, "");
	c.brokers("127.0.0.1")
		.run();

	c.async_publish<qos_e::at_least_once>(
		"t_1", "p_1", retain_e::no, publish_props{},
		[&](error_code ec, reason_code rc, auto) {
			BOOST_CHECK_MESSAGE(ec, ec.message());
			BOOST_CHECK_MESSAGE(rc, rc.message());
			++handlers_called;
		}
	);

	asio::steady_timer timer(c.get_executor());
	timer.expires_after(std::chrono::seconds(2));
	timer.async_wait([&](auto) { c.cancel(); });

	ioc.run();
	auto end = high_resolution_clock::now();
	auto duration = duration_cast<milliseconds>(end - begin);

	BOOST_CHECK_MESSAGE(
		duration <= std::chrono::seconds(3),
		"The client did not cancel properly!"
	);

	BOOST_CHECK_EQUAL(handlers_called, expected_handlers_called);
}

BOOST_AUTO_TEST_SUITE_END()
