//[receiver
#include <iostream>

#include <boost/asio/as_tuple.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/use_awaitable.hpp>

#include <boost/asio/ip/tcp.hpp>

#include <async_mqtt5.hpp>

#ifdef BOOST_ASIO_HAS_CO_AWAIT

// Modified completion token that will prevent co_await from throwing exceptions.
constexpr auto use_nothrow_awaitable = boost::asio::as_tuple(boost::asio::use_awaitable);

using client_type = async_mqtt5::mqtt_client<boost::asio::ip::tcp::socket>;

boost::asio::awaitable<bool> subscribe(client_type& client) {
	// Configure the request to subscribe to a Topic.
	async_mqtt5::subscribe_topic sub_topic = async_mqtt5::subscribe_topic{
		"<your-mqtt-topic>",
		async_mqtt5::subscribe_options {
			async_mqtt5::qos_e::exactly_once, // All messages will arrive at QoS 2.
			async_mqtt5::no_local_e::no, // Forward message from Clients with same ID.
			async_mqtt5::retain_as_published_e::retain, // Keep the original RETAIN flag.
			async_mqtt5::retain_handling_e::send // Send retained messages when the subscription is established.
		}
	};

	// Subscribe to a single Topic.
	auto&& [ec, sub_codes, sub_props] = co_await client.async_subscribe(
		sub_topic, async_mqtt5::subscribe_props {}, use_nothrow_awaitable
	);
	// Note: you can subscribe to multiple Topics in one mqtt_client::async_subscribe call.

	// An error can occur as a result of:
		//  a) wrong subscribe parameters
		//  b) mqtt_client::cancel is called while the Client is in the process of subscribing
	if (ec)
		std::cout << "Subscribe error occurred: " << ec.message() << std::endl;
	else
		std::cout << "Result of subscribe request: " << sub_codes[0].message() << std::endl;

	co_return !ec && !sub_codes[0]; // True if the subscription was successfully established.
}

boost::asio::awaitable<void> subscribe_and_receive(client_type& client) {
	// Configure the Client.
	// It is mandatory to call brokers() and async_run() to configure the Brokers to connect to and start the Client.
	client.brokers("<your-mqtt-broker>", 1883) // Broker that we want to connect to. 1883 is the default TCP port.
		.async_run(boost::asio::detached); // Start the client.

	// Before attempting to receive an Application Message from the Topic we just subscribed to,
	// it is advisable to verify that the subscription succeeded.
	// It is not recommended to call mqtt_client::async_receive if you do not have any
	// subscription established as the corresponding handler will never be invoked.
	if (!(co_await subscribe(client)))
		co_return;

	for (;;) {
		// Receive an Appplication Message from the subscribed Topic(s).
		auto&& [ec, topic, payload, publish_props] = co_await client.async_receive(use_nothrow_awaitable);

		if (ec == async_mqtt5::client::error::session_expired) {
			// The Client has reconnected, and the prior session has expired.
			// As a result, any previous subscriptions have been lost and must be reinstated.
			if (co_await subscribe(client))
				continue;
			else
				break;
		} else if (ec)
			break;

		std::cout << "Received message from the Broker" << std::endl;
		std::cout << "\t topic: " << topic << std::endl;
		std::cout << "\t payload: " << payload << std::endl;
	}

	co_return;
}

int main() {
	// Initialise execution context.
	boost::asio::io_context ioc;

	// Initialise the Client to connect to the Broker over TCP.
	client_type client(ioc);

	// Set up signals to stop the program on demand.
	boost::asio::signal_set signals(ioc, SIGINT, SIGTERM);
	signals.async_wait([&client](async_mqtt5::error_code /* ec */, int /* signal */) {
		// After we are done with publishing all the messages, cancel the timer and the Client.
		// Alternatively, use mqtt_client::async_disconnect.
		client.cancel();
	});

	// Spawn the coroutine.
	co_spawn(ioc, subscribe_and_receive(client), boost::asio::detached);

	// Start the execution.
	ioc.run();
}

#endif

//]
