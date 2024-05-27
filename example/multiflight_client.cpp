//
// Copyright (c) 2023-2024 Ivica Siladic, Bruno Iljazovic, Korina Simicevic
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//

//[multiflight_client
#include <iostream>

#include <boost/asio/io_context.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/signal_set.hpp>

#include <async_mqtt5.hpp>

int main() {
	boost::asio::io_context ioc;

	async_mqtt5::mqtt_client<boost::asio::ip::tcp::socket> client(ioc);

	client.brokers("<your-mqtt-broker>", 1883)
		.async_run(boost::asio::detached);

	// Publish with QoS 2 five times in a row without waiting for the handler
	// of the previous async_publish call to be invoked.
	for (auto i = 1; i <= 5; ++i)
		client.async_publish<async_mqtt5::qos_e::exactly_once>(
			"<topic>", "Hello world!",
			async_mqtt5::retain_e::no, async_mqtt5::publish_props {},
			[i](async_mqtt5::error_code ec, async_mqtt5::reason_code rc, async_mqtt5::pubcomp_props) {
				std::cout << "Publish number " << i << " completed with: " << std::endl;
				std::cout << "\t ec: " << ec.message() << std::endl;
				std::cout << "\t rc: " << rc.message() << std::endl;
			}
		);

	// We can stop the Client by using signals.
	boost::asio::signal_set signals(ioc, SIGINT, SIGTERM);
	signals.async_wait([&client](async_mqtt5::error_code, int) {
		client.async_disconnect(boost::asio::detached);
	});

	ioc.run();
}
//]
