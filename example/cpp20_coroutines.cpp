#include <boost/asio/use_awaitable.hpp>
#ifdef BOOST_ASIO_HAS_CO_AWAIT

#include <boost/asio/as_tuple.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>

#include <boost/asio/ip/tcp.hpp>

#include <async_mqtt5.hpp>

namespace asio = boost::asio;

using stream_type = asio::ip::tcp::socket;
using client_type = async_mqtt5::mqtt_client<stream_type>;

asio::awaitable<void> coroutine(client_type& client) {
//[publish_coro
	// Publish an Application Message with QoS 0.
	// The handler signature for this function is void (error_code).
	// However, when using asio::use_awaitable as a completion token,
	// the error_code is not returned but thrown as an exception if an error occurrs.
	co_await client.async_publish<async_mqtt5::qos_e::at_most_once>(
		"test/mqtt-test", "Hello world!",
		async_mqtt5::retain_e::yes, async_mqtt5::publish_props {},
		asio::use_awaitable
	);

	// Publish an Application Message with QoS 1.
	// The handler signature for this function is void (error_code, reason_code, puback_props).
	// With asio::use_awaitable as a completion token, the co_await will return reason_code and puback_props.
	auto [puback_rc, puback_props] = co_await client.async_publish<async_mqtt5::qos_e::at_least_once>(
		"test/mqtt-test", "Hello world!",
		async_mqtt5::retain_e::yes, async_mqtt5::publish_props {},
		asio::use_awaitable
	);

	// Publish an Application Message with QoS 2.
	// The handler signature for this function is void (error_code, reason_code, pubcomp_props).
	// With asio::use_awaitable as a completion token, the co_await will return reason_code and pubcomp_props.
	auto [pubcomp_rc, pubcomp_props] = co_await client.async_publish<async_mqtt5::qos_e::exactly_once>(
		"test/mqtt-test", "Hello world!",
		async_mqtt5::retain_e::yes, async_mqtt5::publish_props {},
		asio::use_awaitable
	);
//]

//[subscribe_coro
	// Subscribe to a single Topic.
	// The handler signature for this function is void (error_code, std::vector<reason_code>, suback_props).
	// With asio::use_awaitable as a completion token, the co_await
	// will return std::vector<reason_code> and suback_props.
	auto [sub_codes, sub_props] = co_await client.async_subscribe(
		{ "test/mqtt-test", { async_mqtt5::qos_e::exactly_once } },
		async_mqtt5::subscribe_props {},  asio::use_awaitable
	);
//]

//[receive_coro
	// Receive an Application Message.
	// The co_await call will return std::string (topic), std::string (payload) and publish_props.
	// Note: the coroutine will be suspended until an Application Message is ready to be received
	// or an error has occurred. In theory, the coroutine could be suspended indefinitely.
	// Avoid calling this if you have not successfully subscribed to a Topic.
	auto [topic, payload, publish_props] = co_await client.async_receive(asio::use_awaitable);
//]

//[unsubscribe_coro
	// Unsubscribe from the Topic.
	// The handler signature for this function is void (error_code, std::vector<reason_code>, unsuback_props).
	// With asio::use_awaitable as a completion token, the co_await
	// will return std::vector<reason_code> and unsuback_props.
	auto [unsub_codes, unsub_props] = co_await client.async_unsubscribe(
		"test/mqtt-test", async_mqtt5::unsubscribe_props {},
		asio::use_awaitable
	);
//]

//[disconnect_coro
	// Disconnect the Client.
	// With asio::use_awaitable as a completion token and void (error_code) as the completion signature,
	// the co_await has nothing to return.
	co_await client.async_disconnect(
		async_mqtt5::disconnect_rc_e::disconnect_with_will_message,
		async_mqtt5::disconnect_props {},
		asio::use_awaitable
	);
//]

	co_return;
}

//[no_throw_awaitable
constexpr auto use_nothrow_awaitable = asio::as_tuple(asio::use_awaitable);
//]


asio::awaitable<void> nothrow_coroutine(client_type& client) {
//[publish_coro_nothrow
	async_mqtt5::error_code ec;
	async_mqtt5::reason_code rc;
	
	std::tie(ec) = co_await client.async_publish<async_mqtt5::qos_e::at_most_once>(
		"test/mqtt-test", "Hello world!",
		async_mqtt5::retain_e::yes, async_mqtt5::publish_props {},
		use_nothrow_awaitable
	);

	async_mqtt5::puback_props puback_props;
	std::tie(ec, rc, puback_props) = co_await client.async_publish<async_mqtt5::qos_e::at_least_once>(
		"test/mqtt-test", "Hello world!",
		async_mqtt5::retain_e::yes, async_mqtt5::publish_props {},
		use_nothrow_awaitable
	);

	async_mqtt5::pubcomp_props pubcomp_props;
	std::tie(ec, rc, pubcomp_props) = co_await client.async_publish<async_mqtt5::qos_e::exactly_once>(
		"test/mqtt-test", "Hello world!",
		async_mqtt5::retain_e::yes, async_mqtt5::publish_props {},
		use_nothrow_awaitable
	);
//]

//[subscribe_coro_nothrow
	std::vector<async_mqtt5::reason_code> rcs;
	async_mqtt5::suback_props suback_props;
	std::tie(ec, rcs, suback_props) = co_await client.async_subscribe(
		{ "test/mqtt-test", { async_mqtt5::qos_e::exactly_once } },
		async_mqtt5::subscribe_props {}, use_nothrow_awaitable
	);
//]

//[receive_coro_nothrow
	std::string topic, payload;
	async_mqtt5::publish_props publish_props;
	std::tie(ec, topic, payload, publish_props) = co_await client.async_receive(use_nothrow_awaitable);
//]

//[unsubscribe_coro_nothrow
	async_mqtt5::unsuback_props unsuback_props;
	std::tie(ec, rcs, unsuback_props) = co_await client.async_unsubscribe(
		std::vector<std::string>{ "test/mqtt-test" }, async_mqtt5::unsubscribe_props {},
		use_nothrow_awaitable
	);
//]

//[disconnect_coro_nothrow
	std::tie(ec) = co_await client.async_disconnect(
		async_mqtt5::disconnect_rc_e::disconnect_with_will_message,
		async_mqtt5::disconnect_props {},
		use_nothrow_awaitable
	);
//]

	co_return;
}


int main(int argc, char** argv) {
	asio::io_context ioc;

	// Make an instance of mqtt_client. Establish a TCP connection with the Broker.
	client_type c(ioc.get_executor());

	c.credentials("test-client", "username", "password")
		.brokers("mqtt.broker", 1883)
		.async_run(asio::detached);

	co_spawn(ioc.get_executor(), coroutine(c), asio::detached);
	// or...
	co_spawn(ioc.get_executor(), nothrow_coroutine(c), asio::detached);

	ioc.run();
}

#endif
