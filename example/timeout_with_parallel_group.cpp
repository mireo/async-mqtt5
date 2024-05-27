//
// Copyright (c) 2023-2024 Ivica Siladic, Bruno Iljazovic, Korina Simicevic
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//

//[timeout_with_parallel_group
#include <iostream>

#include <boost/asio/io_context.hpp>
#include <boost/asio/deferred.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/experimental/parallel_group.hpp>
#include <boost/asio/ip/tcp.hpp>

#include <async_mqtt5.hpp>

int main() {
	boost::asio::io_context ioc;

	// Construct the Client.
	async_mqtt5::mqtt_client<boost::asio::ip::tcp::socket> client(ioc);

	// Construct the timer.
	boost::asio::steady_timer timer(ioc, std::chrono::seconds(5));

	client.brokers("<your-mqtt-broker>", 1883)
		.async_run(boost::asio::detached);

	// Subscribe to a Topic.
	client.async_subscribe(
		{ "<your-mqtt-topic>" }, async_mqtt5::subscribe_props {},
		[](async_mqtt5::error_code ec, std::vector<async_mqtt5::reason_code> rcs, async_mqtt5::suback_props) {
			std::cout << "[subscribe ec]: " << ec.message() << std::endl;
			std::cout << "[subscribe rc]: " << rcs[0].message() << std::endl;
		}
	);

	// Create a parallel group to wait up to 5 seconds to receive a message
	// using client.async_receive(...).
	boost::asio::experimental::make_parallel_group(
		timer.async_wait(boost::asio::deferred),
		client.async_receive(boost::asio::deferred)
	).async_wait(
		boost::asio::experimental::wait_for_one(),
		[&client](
			std::array<std::size_t, 2> ord, // Completion order
			async_mqtt5::error_code /* timer_ec */, // timer.async_wait(...) handler signature
			// client.async_receive(...) handler signature
			async_mqtt5::error_code receive_ec, 
			std::string topic, std::string payload, async_mqtt5::publish_props /* props */
		) {
			if (ord[0] == 1) {
				std::cout << "Received a message!" << std::endl;
				std::cout << "[receive ec]: " << receive_ec.message() << std::endl;
				std::cout << "[receive topic]: " << topic << std::endl;
				std::cout << "[receive payload]: " << payload << std::endl;
			}
			else
				std::cout << "Timed out! Did not receive a message within 5 seconds." << std::endl;

			client.cancel();
		}
	);

	ioc.run();
}

//]
