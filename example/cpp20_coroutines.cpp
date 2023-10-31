//[cpp20_coroutines_examples

#include <boost/asio/as_tuple.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/use_awaitable.hpp>

#include <boost/asio/ip/tcp.hpp>

#include <async_mqtt5.hpp>

namespace asio = boost::asio;

using stream_type = asio::ip::tcp::socket;
using client_type = async_mqtt5::mqtt_client<stream_type>;

/**
 * An example of a coroutine. It must have a return type of boost::asio::awaitable<T>.
 * When an asynchronous function is called, the coroutine is suspended.
 * After the asynchronous operation finishes, the coroutine resumes from the point it was suspended.
 *
 * In this example, each asynchronous function is invoked with boost::asio::use_awaitable completion token.
 * When using this completion token, co_await will throw exceptions instead of returning an error code.
 * If you do not wish to throw exceptions, refer to the following nothrow_awaitable and nothrow_coroutine() example.
 */
asio::awaitable<void> coroutine(client_type& client) {
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

	auto sub_topic = async_mqtt5::subscribe_topic {
		"test/mqtt-test", {
			async_mqtt5::qos_e::exactly_once,
			async_mqtt5::subscribe_options::no_local_e::no,
			async_mqtt5::subscribe_options::retain_as_published_e::retain,
			async_mqtt5::subscribe_options::retain_handling_e::send
		}
	};

	// Subscribe to a single Topic.
	// The handler signature for this function is void (error_code, std::vector<reason_code>, suback_props).
	// With asio::use_awaitable as a completion token, the co_await
	// will return std::vector<reason_code> and suback_props.
	auto [sub_codes, sub_props] = co_await client.async_subscribe(
		sub_topic, async_mqtt5::subscribe_props {}, asio::use_awaitable
	);

	// Receive an Application Message.
	// The co_await call will return std::string (topic), std::string (payload) and publish_props.
	// Note: the coroutine will be suspended until an Application Message is ready to be received
	// or an error has occurred. In theory, the coroutine could be suspended indefinitely.
	// Avoid calling this if you have not successfully subscribed to a Topic.
	auto [topic, payload, publish_props] = co_await client.async_receive(asio::use_awaitable);

	// Unsubscribe from the Topic.
	// The handler signature for this function is void (error_code, std::vector<reason_code>, unsuback_props).
	// With asio::use_awaitable as a completion token, the co_await
	// will return std::vector<reason_code> and unsuback_props.
	auto [unsub_codes, unsub_props] = co_await client.async_unsubscribe(
		std::vector<std::string>{ "test/mqtt-test" }, async_mqtt5::unsubscribe_props {},
		asio::use_awaitable
	);

	// Disconnect the Client.
	// With asio::use_awaitable as a completion token and void (error_code) as the completion signature,
	// the co_await has nothing to return.
	co_await client.async_disconnect(
		async_mqtt5::disconnect_rc_e::disconnect_with_will_message,
		async_mqtt5::disconnect_props {},
		asio::use_awaitable
	);

	co_return;
}

/**
 * A modified completion token. Using this completion token instead of asio::use_awaitable
 * will prevent co_await from throwing exceptions. Instead, co_await will return the error code
 * along with other values specified in the handler signature.
 */
constexpr auto nothrow_awaitable = asio::as_tuple(asio::use_awaitable);

/**
 * In this coroutine, each asynchronous function is called with nothrow_awaitable completion token.
 * Unlike the asio::use_awaitable completion token, nothrow_awaitable does not throw an exception
 * when an error has occurred. Instead, each co_await will return an error_code, similar to
 * the behavior of using callbacks.
 */
asio::awaitable<void> nothrow_coroutine(client_type& client) {
	async_mqtt5::error_code ec;
	async_mqtt5::reason_code rc;
	
	std::tie(ec) = co_await client.async_publish<async_mqtt5::qos_e::at_most_once>(
		"test/mqtt-test", "Hello world!",
		async_mqtt5::retain_e::yes, async_mqtt5::publish_props {},
		nothrow_awaitable
	);

	async_mqtt5::puback_props puback_props;
	std::tie(ec, rc, puback_props) = co_await client.async_publish<async_mqtt5::qos_e::at_least_once>(
		"test/mqtt-test", "Hello world!",
		async_mqtt5::retain_e::yes, async_mqtt5::publish_props {},
		nothrow_awaitable
	);

	async_mqtt5::pubcomp_props pubcomp_props;
	std::tie(ec, rc, pubcomp_props) = co_await client.async_publish<async_mqtt5::qos_e::exactly_once>(
		"test/mqtt-test", "Hello world!",
		async_mqtt5::retain_e::yes, async_mqtt5::publish_props {},
		nothrow_awaitable
	);

	auto sub_topic = async_mqtt5::subscribe_topic{
		"test/mqtt-test", {
			async_mqtt5::qos_e::exactly_once,
			async_mqtt5::subscribe_options::no_local_e::no,
			async_mqtt5::subscribe_options::retain_as_published_e::retain,
			async_mqtt5::subscribe_options::retain_handling_e::send
		}
	};

	std::vector<async_mqtt5::reason_code> rcs;
	async_mqtt5::suback_props suback_props;
	std::tie(ec, rcs, suback_props) = co_await client.async_subscribe(
		sub_topic, async_mqtt5::subscribe_props {}, nothrow_awaitable
	);

	std::string topic, payload;
	async_mqtt5::publish_props publish_props;
	std::tie(ec, topic, payload, publish_props) = co_await client.async_receive(nothrow_awaitable);

	async_mqtt5::unsuback_props unsuback_props;
	std::tie(ec, rcs, unsuback_props) = co_await client.async_unsubscribe(
		std::vector<std::string>{ "test/mqtt-test" }, async_mqtt5::unsubscribe_props {},
		nothrow_awaitable
	);

	std::tie(ec) = co_await client.async_disconnect(
		async_mqtt5::disconnect_rc_e::disconnect_with_will_message,
		async_mqtt5::disconnect_props {},
		nothrow_awaitable
	);

	co_return;
}


int main(int argc, char** argv) {
	asio::io_context ioc;

	// Make an instance of mqtt_client. Establish a TCP connection with the Broker.
	client_type c(ioc.get_executor(), "");

	c.credentials("test-client", "username", "password")
		.brokers("mqtt.broker", 1883)
		.run();

	asio::co_spawn(ioc.get_executor(), coroutine(c), asio::detached);
	// or...
	asio::co_spawn(ioc.get_executor(), nothrow_coroutine(c), asio::detached);

	ioc.run();
}

//]
