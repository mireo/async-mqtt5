//[publisher
#include <iostream>

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/use_awaitable.hpp>

#include <boost/asio/ip/tcp.hpp>

#include <async_mqtt5.hpp>

#ifdef BOOST_ASIO_HAS_CO_AWAIT

namespace asio = boost::asio;

asio::awaitable<void> client_publisher(asio::io_context& ioc) {
	// Initialise the ``__Client__``, establish connection to the Broker over TCP.
	async_mqtt5::mqtt_client<asio::ip::tcp::socket> client(ioc, "");

	// Configure the ``__Client__``.
	// It is mandatory to call brokers() and async_run() to configure the Brokers to connect to and start the Client.
	client.brokers("mqtt.broker", 1883) // Broker that we want to connect to. 1883 is the default TCP port.
		.async_run(asio::detached); // Start the client.

	// Publish an Application Message with QoS 1.
	auto [rc, props] = co_await client.async_publish<async_mqtt5::qos_e::at_least_once>(
		"test/mqtt-test", "my application message",
		async_mqtt5::retain_e::yes, async_mqtt5::publish_props {}, asio::use_awaitable
	);
	if (rc)
		std::cout << "MQTT protocol error occurred: " << rc.message() << std::endl;

	// Publish some more messages...

	// After we are done with publishing all the messages, disconnect the Client.
	// Alternatively, you can also use mqtt_client::cancel.
	// Regardless, you should ensure all the operations are completed before disconnecting the Client.
	co_await client.async_disconnect(
		async_mqtt5::disconnect_rc_e::normal_disconnection, async_mqtt5::disconnect_props {}, asio::use_awaitable
	);

	co_return;
}

int main() {
	// Initialise execution context.
	asio::io_context ioc;

	// Spawn the coroutine.
	co_spawn(ioc.get_executor(), client_publisher(ioc), asio::detached);

	// Start the execution.
	ioc.run();
}

//]

#endif
