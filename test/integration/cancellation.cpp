#include <boost/test/unit_test.hpp>

#include <boost/asio/as_tuple.hpp>
#include <boost/asio/bind_cancellation_slot.hpp>
#include <boost/asio/cancellation_signal.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/use_awaitable.hpp>

#include <async_mqtt5.hpp>

#include "test_common/message_exchange.hpp"
#include "test_common/test_service.hpp"
#include "test_common/test_stream.hpp"

using namespace async_mqtt5;

namespace async_mqtt5::test {

enum operation_type {
	async_run = 1,
	publish,
	receive,
	subscribe,
	unsubscribe
};

enum cancel_type {
	ioc_stop = 1,
	client_cancel,
	signal_emit
};

} // end namespace async_mqtt5::test

using stream_type = asio::ip::tcp::socket;
using client_type = mqtt_client<stream_type>;

template <
	test::operation_type op_type,
	std::enable_if_t<op_type == test::operation_type::async_run, bool> = true
>
void setup_cancel_op_test_case(
	client_type& c, asio::cancellation_signal& signal, int& handlers_called
) {
	c.async_run(
		asio::bind_cancellation_slot(
			signal.slot(),
			[&handlers_called](error_code ec) {
				handlers_called++;
				BOOST_CHECK_EQUAL(ec, asio::error::operation_aborted);
			}
		)
	);
}

template <
	test::operation_type op_type,
	std::enable_if_t<op_type == test::operation_type::publish, bool> = true
>
void setup_cancel_op_test_case(
	client_type& c, asio::cancellation_signal& signal, int& handlers_called
) {
	c.async_run(asio::detached);
	c.async_publish<qos_e::at_most_once>("topic", "payload", retain_e::no, publish_props {},
		asio::bind_cancellation_slot(
			signal.slot(),
			[&handlers_called](error_code ec) {
				handlers_called++;
				BOOST_CHECK_EQUAL(ec, asio::error::operation_aborted);
			}
		)
	);
}

template <
	test::operation_type op_type,
	std::enable_if_t<op_type == test::operation_type::receive, bool> = true
>
void setup_cancel_op_test_case(
	client_type& c, asio::cancellation_signal& signal, int& handlers_called
) {
	c.async_run(asio::detached);
	c.async_receive(
		asio::bind_cancellation_slot(
			signal.slot(),
			[&handlers_called](
				error_code ec, std::string t, std::string p, publish_props
			) {
					handlers_called++;
					BOOST_CHECK_EQUAL(ec, asio::error::operation_aborted);
					BOOST_CHECK_EQUAL(t, "");
					BOOST_CHECK_EQUAL(p, "");
			}
		)
	);
}

template <
	test::operation_type op_type,
	std::enable_if_t<op_type == test::operation_type::subscribe, bool > = true
>
void setup_cancel_op_test_case(
	client_type& c, asio::cancellation_signal& signal, int& handlers_called
) {
	c.async_run(asio::detached);
	c.async_unsubscribe(
		"topic" ,unsubscribe_props {},
		asio::bind_cancellation_slot(
			signal.slot(),
			[&handlers_called](
				error_code ec, std::vector<reason_code> rcs, unsuback_props
			) {
				handlers_called++;
				BOOST_CHECK_EQUAL(ec, asio::error::operation_aborted);
				// TODO: be consistent with complete_post
				//BOOST_ASSERT(rcs.size() == 1);
				//BOOST_CHECK(rcs[0] == reason_codes::empty);
			}
		)
	);
}

template <
	test::operation_type op_type,
	std::enable_if_t<op_type == test::operation_type::unsubscribe, bool> = true
>
void setup_cancel_op_test_case(
	client_type& c, asio::cancellation_signal& signal, int& handlers_called
) {
	c.async_run(asio::detached);
	c.async_subscribe(
		subscribe_topic { "topic", subscribe_options {} }, subscribe_props {},
		asio::bind_cancellation_slot(
			signal.slot(),
			[&handlers_called](
				error_code ec, std::vector<reason_code> rcs, suback_props
			) {
				handlers_called++;
				BOOST_CHECK_EQUAL(ec, asio::error::operation_aborted);
				// TODO: be consistent with complete_post
				//BOOST_ASSERT(rcs.size() == 1);
				//BOOST_CHECK(rcs[0] == reason_codes::empty);
			}
		)
	);
}

template<test::cancel_type c_type, test::operation_type op_type>
void run_cancel_op_test() {
	using namespace test;

	constexpr int expected_handlers_called = c_type == ioc_stop ? 0 : 1;
	int handlers_called = 0;

	asio::io_context ioc;
	client_type c(ioc, "");
	c.brokers("127.0.0.1");

	asio::cancellation_signal signal;
	setup_cancel_op_test_case<op_type>(c, signal, handlers_called);

	asio::steady_timer timer(c.get_executor());
	timer.expires_after(std::chrono::milliseconds(10));
	timer.async_wait([&](auto) {
		if constexpr (c_type == ioc_stop)
			ioc.stop();
		else if constexpr (c_type == client_cancel)
			c.cancel();
		else if constexpr (c_type == signal_emit)
			signal.emit(asio::cancellation_type_t::terminal);
	});


	ioc.run_for(std::chrono::seconds(1));
	BOOST_CHECK_EQUAL(handlers_called, expected_handlers_called);
}

BOOST_AUTO_TEST_SUITE(cancellation/*, *boost::unit_test::disabled()*/)

// hangs
BOOST_AUTO_TEST_CASE(ioc_stop_async_run, *boost::unit_test::disabled()) {
	run_cancel_op_test<test::ioc_stop, test::async_run>();
}

BOOST_AUTO_TEST_CASE(client_cancel_async_run) {
	run_cancel_op_test<test::client_cancel, test::async_run>();
}

BOOST_AUTO_TEST_CASE(signal_emit_async_run) {
	run_cancel_op_test<test::signal_emit, test::async_run>();
}

// hangs
BOOST_AUTO_TEST_CASE(ioc_stop_async_publish, *boost::unit_test::disabled() ) {
	run_cancel_op_test<test::ioc_stop, test::publish>();
}

BOOST_AUTO_TEST_CASE(client_cancel_async_publish) {
	run_cancel_op_test<test::client_cancel, test::publish>();
}

BOOST_AUTO_TEST_CASE(signal_emit_async_publish) {
	run_cancel_op_test<test::signal_emit, test::publish>();
}

BOOST_AUTO_TEST_CASE(ioc_stop_async_receive) {
	run_cancel_op_test<test::ioc_stop, test::receive>();
}

BOOST_AUTO_TEST_CASE(client_cancel_async_receive) {
	run_cancel_op_test<test::client_cancel, test::receive>();
}

BOOST_AUTO_TEST_CASE(signal_emit_async_receive) {
	run_cancel_op_test<test::signal_emit, test::receive>();
}

// hangs
BOOST_AUTO_TEST_CASE(ioc_stop_async_subscribe, *boost::unit_test::disabled()) {
	run_cancel_op_test<test::ioc_stop, test::subscribe>();
}

BOOST_AUTO_TEST_CASE(client_cancel_async_subscribe) {
	run_cancel_op_test<test::client_cancel, test::subscribe>();
}

BOOST_AUTO_TEST_CASE(signal_emit_async_subscribe) {
	run_cancel_op_test<test::signal_emit, test::subscribe>();
}

// hangs
BOOST_AUTO_TEST_CASE(ioc_stop_async_unsubscribe, *boost::unit_test::disabled()) {
	run_cancel_op_test<test::ioc_stop, test::unsubscribe>();
}

BOOST_AUTO_TEST_CASE(client_cancel_async_unsubscribe) {
	run_cancel_op_test<test::client_cancel, test::unsubscribe>();
}

BOOST_AUTO_TEST_CASE(signal_emit_async_unsubscribe) {
	run_cancel_op_test<test::signal_emit, test::unsubscribe>();
}

struct shared_test_data {
	error_code success {};
	error_code fail = asio::error::not_connected;

	const std::string connect = encoders::encode_connect(
		"", std::nullopt, std::nullopt, 10, false, {}, std::nullopt
	);
	const std::string connack = encoders::encode_connack(
		false, reason_codes::success.value(), {}
	);

	const std::string topic = "topic";
	const std::string payload = "payload";

	const std::string publish_qos1 = encoders::encode_publish(
		1, topic, payload, qos_e::at_least_once, retain_e::no, dup_e::no, {}
	);

	const std::string puback = encoders::encode_puback(1, uint8_t(0x00), {});
};

using test::after;
using namespace std::chrono;

#ifdef BOOST_ASIO_HAS_CO_AWAIT

constexpr auto use_nothrow_awaitable = asio::as_tuple(asio::use_awaitable);

BOOST_FIXTURE_TEST_CASE(rerunning_the_client, shared_test_data) {
	test::msg_exchange broker_side;
	broker_side
		.expect(connect)
			.complete_with(success, after(1ms))
			.reply_with(connack, after(2ms))
		.expect(publish_qos1)
			.complete_with(success, after(1ms))
			.reply_with(puback, after(2ms))
		.expect(connect)
			.complete_with(success, after(1ms))
			.reply_with(connack, after(2ms))
		.expect(publish_qos1)
			.complete_with(success, after(1ms))
			.reply_with(puback, after(2ms));

	asio::io_context ioc;
	auto executor = ioc.get_executor();
	auto& broker = asio::make_service<test::test_broker>(
		ioc, executor, std::move(broker_side)
	);

	co_spawn(ioc,
		[&]() -> asio::awaitable<void> {
			mqtt_client<test::test_stream> c(ioc, "");
			c.brokers("127.0.0.1,127.0.0.1", 1883) // to avoid reconnect backoff
				.async_run(asio::detached);

			auto [ec, rc, props] = co_await c.async_publish<qos_e::at_least_once>(
				topic, payload, retain_e::no, publish_props {}, use_nothrow_awaitable
			);
			BOOST_CHECK(!ec);
			BOOST_CHECK(!rc);

			c.cancel();

			auto [cec, crc, cprops] = co_await c.async_publish<qos_e::at_least_once>(
				topic, payload, retain_e::no, publish_props {}, use_nothrow_awaitable
			);
			BOOST_CHECK_EQUAL(cec, asio::error::operation_aborted);
			BOOST_CHECK_EQUAL(crc, reason_codes::empty);

			c.async_run(asio::detached);

			auto [rec, rrc, rprops] = co_await c.async_publish<qos_e::at_least_once>(
				topic, payload, retain_e::no, publish_props {}, use_nothrow_awaitable
			);
			BOOST_CHECK(!rec);
			BOOST_CHECK(!rrc);

			co_await c.async_disconnect(use_nothrow_awaitable);
			co_return;
		},
		asio::detached
	);

	ioc.run();
	BOOST_CHECK(broker.received_all_expected());
}

#endif

BOOST_AUTO_TEST_SUITE_END();
