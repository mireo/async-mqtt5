#include <boost/test/unit_test.hpp>

#include <boost/asio/io_context.hpp>

#include <async_mqtt5/mqtt_client.hpp>

#include "test_common/message_exchange.hpp"
#include "test_common/test_service.hpp"
#include "test_common/test_stream.hpp"

using namespace async_mqtt5;

namespace async_mqtt5::test {

enum operation_type {
	subscribe = 1,
	unsubscribe
};

} // end namespace async_mqtt5::test

BOOST_AUTO_TEST_SUITE(sub_unsub/*, *boost::unit_test::disabled()*/)

struct shared_test_data {
	error_code success {};
	error_code fail = asio::error::not_connected;

	const std::string connect = encoders::encode_connect(
		"", std::nullopt, std::nullopt, 10, false, {}, std::nullopt
	);
	const std::string connack = encoders::encode_connack(
		false, reason_codes::success.value(), {}
	);

	std::vector<subscribe_topic> sub_topics = {
		subscribe_topic { "topic", subscribe_options {} }
	};
	std::vector<std::string> unsub_topics = { "topic" };
	std::vector<uint8_t> rcs = { uint8_t(0x00) };

	const std::string subscribe = encoders::encode_subscribe(
		1, sub_topics, subscribe_props {}
	);
	const std::string suback = encoders::encode_suback(1, rcs, suback_props {});

	const std::string unsubscribe = encoders::encode_unsubscribe(
		1, unsub_topics, unsubscribe_props {}
	);
	const std::string unsuback = encoders::encode_unsuback(1, rcs, unsuback_props {});
};

using test::after;
using namespace std::chrono;

template <test::operation_type op_type>
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

	auto data = shared_test_data();
	if constexpr (op_type == test::operation_type::subscribe)
		c.async_subscribe(
			data.sub_topics, subscribe_props {},
			[&handlers_called, &c](error_code ec, std::vector<reason_code> rcs, suback_props) {
				++handlers_called;

				BOOST_CHECK(!ec);
				BOOST_ASSERT(rcs.size() == 1);
				BOOST_CHECK_EQUAL(rcs[0], reason_codes::granted_qos_0);

				c.cancel();
			}
		);
	else 
		c.async_unsubscribe(
			data.unsub_topics, unsubscribe_props {},
			[&handlers_called, &c](error_code ec, std::vector<reason_code> rcs, unsuback_props) {
				++handlers_called;

				BOOST_CHECK(!ec);
				BOOST_ASSERT(rcs.size() == 1);
				BOOST_CHECK_EQUAL(rcs[0], reason_codes::success);

				c.cancel();
			}
		);

	ioc.run_for(5s);
	BOOST_CHECK_EQUAL(handlers_called, expected_handlers_called);
	BOOST_CHECK(broker.received_all_expected());
}

// subscribe

BOOST_FIXTURE_TEST_CASE(fail_to_send_subscribe, shared_test_data) {
	test::msg_exchange broker_side;
	broker_side
		.expect(connect)
			.complete_with(success, after(1ms))
			.reply_with(connack, after(2ms))
		.expect(subscribe)
			.complete_with(fail, after(1ms))
		.expect(connect)
			.complete_with(success, after(1ms))
			.reply_with(connack, after(2ms))
		.expect(subscribe)
			.complete_with(success, after(1ms))
			.reply_with(suback, after(2ms));

	run_test<test::operation_type::subscribe>(std::move(broker_side));
}

BOOST_FIXTURE_TEST_CASE(fail_to_receive_suback, shared_test_data) {
	test::msg_exchange broker_side;
	broker_side
		.expect(connect)
			.complete_with(success, after(1ms))
			.reply_with(connack, after(2ms))
		.expect(subscribe)
			.complete_with(success, after(1ms))
		.send(fail, after(15ms))
		.expect(connect)
			.complete_with(success, after(1ms))
			.reply_with(connack, after(2ms))
		.expect(subscribe)
			.complete_with(success, after(1ms))
			.reply_with(suback, after(2ms));

	run_test<test::operation_type::subscribe>(std::move(broker_side));
}

// unsubscribe

BOOST_FIXTURE_TEST_CASE(fail_to_send_unsubscribe, shared_test_data) {
	test::msg_exchange broker_side;
	broker_side
		.expect(connect)
			.complete_with(success, after(1ms))
			.reply_with(connack, after(2ms))
		.expect(unsubscribe)
			.complete_with(fail, after(1ms))
		.expect(connect)
			.complete_with(success, after(1ms))
			.reply_with(connack, after(2ms))
		.expect(unsubscribe)
			.complete_with(success, after(1ms))
			.reply_with(unsuback, after(2ms));

	run_test<test::operation_type::unsubscribe>(std::move(broker_side));
}

BOOST_FIXTURE_TEST_CASE(fail_to_receive_unsuback, shared_test_data) {
	test::msg_exchange broker_side;
	broker_side
		.expect(connect)
			.complete_with(success, after(1ms))
			.reply_with(connack, after(2ms))
		.expect(unsubscribe)
			.complete_with(success, after(1ms))
		.send(fail, after(15ms))
		.expect(connect)
			.complete_with(success, after(1ms))
			.reply_with(connack, after(2ms))
		.expect(unsubscribe)
			.complete_with(success, after(1ms))
			.reply_with(unsuback, after(2ms));

	run_test<test::operation_type::unsubscribe>(std::move(broker_side));
}

BOOST_AUTO_TEST_SUITE_END();
