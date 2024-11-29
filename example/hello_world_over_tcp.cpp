//
// Copyright (c) 2023-2024 Ivica Siladic, Bruno Iljazovic, Korina Simicevic
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//

//[hello_world_over_tcp
#include <iostream>

#include <boost/asio/io_context.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/ip/tcp.hpp>

#include <async_mqtt5.hpp>

int main() {
	//[init_tcp_client
	// Initialize the execution context required to run I/O operations.
	boost::asio::io_context ioc;

	// Construct the Client with ``__TCP_SOCKET__`` as the underlying stream.
	//async_mqtt5::mqtt_client<boost::asio::ip::tcp::socket> client(ioc);
	async_mqtt5::mqtt_client<boost::asio::ip::tcp::socket, std::monostate /* TlsContext */, async_mqtt5::logger> client(
		ioc, {} /* tls_context */, async_mqtt5::logger(async_mqtt5::log_level::debug)
	);
	//]

	//[configure_tcp_client
	// 1883 is the default TCP MQTT port.
	client.brokers("broker.hivemq.com", 1883)
		.credentials("async_mqtt5_tester")
		.async_run(boost::asio::detached);
	//]

	//[publish_hello_world
	client.async_publish<async_mqtt5::qos_e::at_most_once>(
		"async-mqtt5/test", "Hello world!",
		async_mqtt5::retain_e::yes, async_mqtt5::publish_props {},
		[&client](async_mqtt5::error_code ec) {
			std::cout << ec.message() << std::endl;

			// Disconnnect the Client.
			client.async_disconnect(boost::asio::detached);
		}
	);
	//]

	ioc.run();
}
//]
