//[hello_world_over_tcp
#include <iostream>

#include <boost/asio/io_context.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/ip/tcp.hpp>

#include <async_mqtt5.hpp>

int main() {
	boost::asio::io_context ioc;

	// Construct the Client with ``__TCP_SOCKET__`` as the underlying stream.
	async_mqtt5::mqtt_client<boost::asio::ip::tcp::socket> client(ioc);

	// 1883 is the default TCP MQTT port.
	client.brokers("<your-mqtt-broker>", 1883)
		.async_run(boost::asio::detached);

	client.async_publish<async_mqtt5::qos_e::at_most_once>(
		"<topic>", "Hello world!",
		async_mqtt5::retain_e::no, async_mqtt5::publish_props {},
		[&client](async_mqtt5::error_code ec) {
			std::cout << ec.message() << std::endl;
			client.async_disconnect(boost::asio::detached);
		}
	);

	ioc.run();
}
//]
