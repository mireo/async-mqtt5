#include <boost/test/unit_test.hpp>

#include <boost/asio/as_tuple.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/use_awaitable.hpp>

#include <async_mqtt5.hpp>

using namespace async_mqtt5;

namespace async_mqtt5::test {

enum cancel_type {
	ioc_stop = 1,
	client_cancel
};

} // end namespace async_mqtt5::test

template <test::cancel_type type>
void cancel_async_receive() {
	using namespace test;

	constexpr int num_handlers = 3;
	constexpr int expected_handlers_called = type == ioc_stop ? 0 : num_handlers;
	int handlers_called = 0;

	asio::io_context ioc;

	using stream_type = asio::ip::tcp::socket;
	using client_type = mqtt_client<stream_type>;
	client_type c(ioc, "");

	c.brokers("mqtt.mireo.local", 1883)
		.run();

	for (auto i = 0; i < num_handlers; ++i)
		c.async_receive([&handlers_called](
			error_code ec, std::string, std::string, publish_props
		) {
			BOOST_CHECK_EQUAL(ec, asio::error::operation_aborted);
			handlers_called++;
		});

	asio::steady_timer timer(c.get_executor());
	timer.expires_after(std::chrono::seconds(1));
	timer.async_wait([&](auto) {
		if constexpr (type == ioc_stop)
			ioc.stop();
		else
			c.cancel();
	});

	ioc.run();
	BOOST_CHECK_EQUAL(handlers_called, expected_handlers_called);
}

template <test::cancel_type type>
void cancel_async_publish() {
	using namespace test;

	constexpr int expected_handlers_called = type == ioc_stop ? 0 : 3;
	int handlers_called = 0;

	asio::io_context ioc;

	using stream_type = asio::ip::tcp::socket;
	using client_type = mqtt_client<stream_type>;
	client_type c(ioc, "");

	c.brokers("mqtt.mireo.local", 1883)
		.run();

	c.async_publish<qos_e::at_most_once>(
		"topic", "payload", retain_e::yes, {},
		[&handlers_called](error_code ec) {
			BOOST_CHECK_EQUAL(ec, asio::error::operation_aborted);
			handlers_called++;
		}
	);

	c.async_publish<qos_e::at_least_once>(
		"topic", "payload", retain_e::yes, {},
		[&handlers_called](error_code ec, reason_code rc, puback_props) {
			BOOST_CHECK_EQUAL(ec, asio::error::operation_aborted);
			BOOST_CHECK_EQUAL(rc, reason_codes::empty);
			handlers_called++;
		}
	);


	c.async_publish<qos_e::exactly_once>(
		"topic", "payload", retain_e::yes, {},
		[&handlers_called](error_code ec, reason_code rc, pubcomp_props) {
			BOOST_CHECK_EQUAL(ec, asio::error::operation_aborted);
			BOOST_CHECK_EQUAL(rc, reason_codes::empty);
			handlers_called++;
		}
	);

	asio::steady_timer timer(c.get_executor());
	timer.expires_after(std::chrono::seconds(1));
	timer.async_wait([&](auto) {
		if constexpr (type == ioc_stop)
			ioc.stop();
		else
			c.cancel();
	});

	ioc.run();
	BOOST_CHECK_EQUAL(handlers_called, expected_handlers_called);
}

template <test::cancel_type type>
void cancel_during_connecting() {
	using namespace test;

	constexpr int expected_handlers_called = type == ioc_stop ? 0 : 1;
	int handlers_called = 0;

	asio::io_context ioc;

	using stream_type = asio::ip::tcp::socket;
	using client_type = mqtt_client<stream_type>;
	client_type c(ioc, "");

	c.brokers("127.0.0.1", 1883)
		.run();

	c.async_publish<qos_e::at_most_once>(
		"topic", "payload", retain_e::yes, {},
		[&handlers_called](error_code ec) {
			BOOST_CHECK_EQUAL(ec, asio::error::operation_aborted);
			handlers_called++;
		}
	);

	asio::steady_timer timer(c.get_executor());
	timer.expires_after(std::chrono::seconds(2));
	timer.async_wait([&](auto) {
		if constexpr (type == ioc_stop)
			ioc.stop();
		else
			c.cancel();
	});

	ioc.run();
	BOOST_CHECK_EQUAL(handlers_called, expected_handlers_called);
}


BOOST_AUTO_TEST_SUITE(cancellation/*, *boost::unit_test::disabled()*/)


BOOST_AUTO_TEST_CASE(ioc_stop_async_receive) {
	cancel_async_receive<test::cancel_type::ioc_stop>();
}


BOOST_AUTO_TEST_CASE(client_cancel_async_receive) {
	cancel_async_receive<test::cancel_type::client_cancel>();
}

// passes on debian, hangs on windows in io_context destructor
BOOST_AUTO_TEST_CASE(ioc_stop_async_publish, *boost::unit_test::disabled() ) {
	cancel_async_publish<test::cancel_type::ioc_stop>();
}

BOOST_AUTO_TEST_CASE(client_cancel_async_publish) {
	cancel_async_publish<test::cancel_type::client_cancel>();
}

// passes on debian, hangs on windows 
BOOST_AUTO_TEST_CASE(ioc_stop_cancel_during_connecting, *boost::unit_test::disabled() ) {
	cancel_during_connecting<test::cancel_type::ioc_stop>();
}

BOOST_AUTO_TEST_CASE(client_cancel_during_connecting) {
	cancel_during_connecting<test::cancel_type::client_cancel>();
}

#ifdef BOOST_ASIO_HAS_CO_AWAIT

constexpr auto use_nothrow_awaitable = asio::as_tuple(asio::use_awaitable);

BOOST_AUTO_TEST_CASE(rerunning_the_client) {
	asio::io_context ioc;

	co_spawn(ioc,
		[&ioc]() -> asio::awaitable<void> {
			using stream_type = asio::ip::tcp::socket;
			using client_type = mqtt_client<stream_type>;
			client_type c(ioc, "");

			c.brokers("mqtt.mireo.local", 1883)
				.credentials("test-cli", "", "")
				.run();

			auto [ec] = co_await c.async_publish<qos_e::at_most_once>(
				"t", "p", retain_e::yes, publish_props {}, use_nothrow_awaitable
			);
			BOOST_CHECK(!ec);

			c.cancel();

			auto [cec] = co_await c.async_publish<qos_e::at_most_once>(
				"ct", "cp", retain_e::yes, publish_props {}, use_nothrow_awaitable
			);
			BOOST_CHECK(cec == asio::error::operation_aborted);

			c.run();

			auto [rec] = co_await c.async_publish<qos_e::at_most_once>(
				"ct", "cp", retain_e::yes, publish_props {}, use_nothrow_awaitable
			);
			BOOST_CHECK(!rec);

			co_await c.async_disconnect(use_nothrow_awaitable);
			co_return;
		},
		asio::detached
	);

	ioc.run();
}

#endif

BOOST_AUTO_TEST_SUITE_END();
