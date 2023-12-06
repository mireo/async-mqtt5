#include <boost/test/unit_test.hpp>

#include <boost/asio/use_awaitable.hpp>
#ifdef BOOST_ASIO_HAS_CO_AWAIT

#include <boost/asio/as_tuple.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>

#include <boost/beast/websocket.hpp>

#include <async_mqtt5.hpp>

BOOST_AUTO_TEST_SUITE(coroutine/*, *boost::unit_test::disabled()*/)

using namespace async_mqtt5;
namespace asio = boost::asio;

constexpr auto use_nothrow_awaitable = asio::as_tuple(asio::use_awaitable);

template<typename StreamType, typename TlsContext>
asio::awaitable<void> sanity_check(mqtt_client<StreamType, TlsContext>& c) {
	auto [ec_0] = co_await c.template async_publish<qos_e::at_most_once>(
		"test/mqtt-test", "hello world with qos0!", retain_e::yes, publish_props {},
		use_nothrow_awaitable
	);
	BOOST_CHECK(!ec_0);

	auto [ec_1, puback_rc, puback_props] = co_await c.template async_publish<qos_e::at_least_once>(
		"test/mqtt-test", "hello world with qos1!",
		retain_e::yes, publish_props {},
		use_nothrow_awaitable
	);
	BOOST_CHECK(!ec_1);
	BOOST_CHECK(!puback_rc);

	auto [ec_2, pubcomp_rc, pubcomp_props] = co_await c.template async_publish<qos_e::exactly_once>(
		"test/mqtt-test", "hello world with qos2!",
		retain_e::yes, publish_props {},
		use_nothrow_awaitable
	);
	BOOST_CHECK(!ec_2);
	BOOST_CHECK(!pubcomp_rc);

	std::vector<subscribe_topic> topics;
	topics.push_back(subscribe_topic{
		"test/mqtt-test", {
			qos_e::exactly_once,
			subscribe_options::no_local_e::no,
			subscribe_options::retain_as_published_e::retain,
			subscribe_options::retain_handling_e::send
		}
	});
	
	auto [sub_ec, sub_codes, sub_props] = co_await c.async_subscribe(
		topics, subscribe_props {}, use_nothrow_awaitable
	);
	BOOST_CHECK(!sub_ec);
	BOOST_CHECK(!sub_codes[0]);
	auto [rec, topic, payload, publish_props] = co_await c.async_receive(use_nothrow_awaitable);

	auto [unsub_ec, unsub_codes, unsub_props] = co_await c.async_unsubscribe(
		std::vector<std::string>{"test/mqtt-test"}, unsubscribe_props {},
		use_nothrow_awaitable
	);
	BOOST_CHECK(!unsub_ec);
	BOOST_CHECK(!unsub_codes[0]);

	co_await c.async_disconnect(use_nothrow_awaitable);
	co_return;
}


BOOST_AUTO_TEST_CASE(tcp_client_check) {
	asio::io_context ioc;

	using stream_type = asio::ip::tcp::socket;
	using client_type = mqtt_client<stream_type>;
	client_type c(ioc, "");

	c.credentials("tcp-tester", "", "")
		.brokers("mqtt.mireo.local", 1883)
		.will({ "test/mqtt-test", "Client disconnected!", qos_e::at_least_once })
		.run();

	asio::steady_timer timer(ioc);
	timer.expires_after(std::chrono::seconds(10));

	timer.async_wait(
		[&](boost::system::error_code ec) {
			BOOST_CHECK_MESSAGE(ec, "Failed to receive all the expected replies!");
			c.cancel();
			ioc.stop();
		}
	);

	co_spawn(ioc, 
		[&]() -> asio::awaitable<void> {
			co_await sanity_check(c);
			timer.cancel();
		},
		asio::detached
	);

	ioc.run();
}

// TODO: SSL

BOOST_AUTO_TEST_CASE(websocket_tcp_client_check) {
	asio::io_context ioc;

	using stream_type = boost::beast::websocket::stream<
		asio::ip::tcp::socket
	>;

	using client_type = mqtt_client<stream_type>;
	client_type c(ioc, "");

	c.credentials("websocket-tcp-tester", "", "")
		.brokers("fcluster-5/mqtt", 8083)
		.will({ "test/mqtt-test", "Client disconnected!", qos_e::at_least_once })
		.run();

	asio::steady_timer timer(ioc);
	timer.expires_after(std::chrono::seconds(10));

	timer.async_wait(
		[&](boost::system::error_code ec) {
			BOOST_CHECK_MESSAGE(ec, "Failed to receive all the expected replies!");
			c.cancel();
			ioc.stop();
		}
	);

	co_spawn(ioc,
		[&]() -> asio::awaitable<void> {
			co_await sanity_check(c);
			timer.cancel();
		},
		asio::detached
	);

	ioc.run();
}

BOOST_AUTO_TEST_SUITE_END()


#endif
