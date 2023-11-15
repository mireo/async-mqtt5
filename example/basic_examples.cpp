//[basic_examples


//[publisher
#include <iostream>

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>

#include <async_mqtt5.hpp>

namespace asio = boost::asio;

void client_publisher() {
	// Initialise execution context.
	asio::io_context ioc;

	// Initialise the ``__Client__``, establish connection to the Broker over TCP.
	async_mqtt5::mqtt_client<asio::ip::tcp::socket> client (ioc.get_executor(), "");
	client.brokers("mqtt.broker", 1883) // Broker that we want to connect to. 1883 is the default TCP port.
		.run(); // Start the client.

	client.async_publish<async_mqtt5::qos_e::at_least_once>(
		"topic", "my application message",
		async_mqtt5::retain_e::yes, async_mqtt5::publish_props {},
		[](async_mqtt5::error_code ec, async_mqtt5::reason_code rc, async_mqtt5::puback_props props) {
			if (ec)
				std::cout << "An application error occurred: " << ec.message() << std::endl;
			if (rc)
				std::cout << "MQTT protocol error occurred: " << rc.message() << std::endl;
		}
	);

	// Publish some more messages...

	// After we are done with publishing all the messages, disconnect the client.
	// You can also use mqtt_client::cancel.
	// Either way, you should make sure all the operations completed before disconnecting the client!
	client.async_disconnect(
		async_mqtt5::disconnect_rc_e::normal_disconnection, async_mqtt5::disconnect_props {},
		[](async_mqtt5::error_code ec) {
				if (ec)
					std::cout << "An error during disconnect occurred: " << ec.message() << std::endl;
		});

	// Start the execution.
	ioc.run();
}

//]


//]