//[receiver
#include <iostream>

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/use_awaitable.hpp>

#include <boost/asio/ip/tcp.hpp>

#include <async_mqtt5.hpp>

#ifdef BOOST_ASIO_HAS_CO_AWAIT

namespace asio = boost::asio;

asio::awaitable<void> client_receiver(asio::io_context& ioc) {
	// Initialise the Client, establish connection to the Broker over TCP.
	async_mqtt5::mqtt_client<asio::ip::tcp::socket> client(ioc, "");

	// Configure the Client.
	// It is mandatory to call brokers() and async_run() to configure the Brokers to connect to and start the Client.
	client.brokers("mqtt.broker", 1883) // Broker that we want to connect to. 1883 is the default TCP port.
		.async_run(asio::detached); // Start the client.

	// Configure the request to subscribe to a Topic.
	async_mqtt5::subscribe_topic sub_topic = async_mqtt5::subscribe_topic {
		"test/mqtt-test",
		async_mqtt5::subscribe_options {
			async_mqtt5::qos_e::exactly_once, // All messages will arrive at QoS 2.
			async_mqtt5::no_local_e::no, // Forward message from Clients with same ID.
			async_mqtt5::retain_as_published_e::retain, // Keep the original RETAIN flag.
			async_mqtt5::retain_handling_e::send // Send retained messages when the subscription is established.
		}
	};

	// Subscribe to a single Topic.
	auto [sub_codes, sub_props] = co_await client.async_subscribe(
		sub_topic, async_mqtt5::subscribe_props {}, asio::use_awaitable
	);
	// Note: you can subscribe to multiple Topics in one mqtt_client::async_subscribe call.

	// std::vector<async_mqtt5::reason_code> sub_codes contain the result of the subscribe action for every Topic.
	// Before attempting to receive an Application Message from the Topic we just subscribed to,
	// it is advisable to verify that the subscription succeeded.
	// It is not recommended to call mqtt_client::async_receive if you do not have any
	// subscription established as the corresponding handler will never be invoked.
	if (!sub_codes[0])
		auto [topic, payload, publish_props] = co_await client.async_receive(asio::use_awaitable);
		// Receive more messages...

	// Unsubscribe from the Topic.
	// Similar to mqtt_client::async_subscribe call, std::vector<async_mqtt5::reason_code> unsub_codes contain
	// the result of the unsubscribe action for every Topic.
	auto [unsub_codes, unsub_props] = co_await client.async_unsubscribe(
		"test/mqtt-test", async_mqtt5::unsubscribe_props {},
		asio::use_awaitable
	);
	// Note: you can unsubscribe from multiple Topics in one mqtt_client::async_unsubscribe call.

	// Disconnect the Client.
	co_await client.async_disconnect(
		async_mqtt5::disconnect_rc_e::disconnect_with_will_message,
		async_mqtt5::disconnect_props {},
		asio::use_awaitable
	);

	co_return;
}

int main() {
	// Initialise execution context.
	asio::io_context ioc;

	// Spawn the coroutine.
	co_spawn(ioc, client_receiver(ioc), asio::detached);

	// Start the execution.
	ioc.run();
}

#endif

//]
