//
// Copyright (c) 2023-2024 Ivica Siladic, Bruno Iljazovic, Korina Simicevic
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//

//[publisher
#include <cstdlib>
#include <iostream>

#include <boost/asio/as_tuple.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/use_awaitable.hpp>

#include <boost/asio/ip/tcp.hpp>

#include <async_mqtt5.hpp>

#ifdef BOOST_ASIO_HAS_CO_AWAIT

// Modified completion token that will prevent co_await from throwing exceptions.
constexpr auto use_nothrow_awaitable = boost::asio::as_tuple(boost::asio::use_awaitable);

using client_type = async_mqtt5::mqtt_client<boost::asio::ip::tcp::socket>;

int next_sensor_reading() {
	srand(static_cast<unsigned int>(std::time(0)));
	return rand() % 100;
}

boost::asio::awaitable<void> publish_sensor_readings(
	client_type& client, boost::asio::steady_timer& timer
) {
	// Configure the Client.
	// It is mandatory to call brokers() and async_run() to configure the Brokers to connect to and start the Client.
	client.brokers("<your-mqtt-broker>", 1883) // Broker that we want to connect to. 1883 is the default TCP port.
		.async_run(boost::asio::detached); // Start the client.

	for (;;) {
		// Get the next sensor reading.
		auto reading = std::to_string(next_sensor_reading());

		// Publish the sensor reading with QoS 1.
		auto&& [ec, rc, props] = co_await client.async_publish<async_mqtt5::qos_e::at_least_once>(
			"<your-mqtt-topic>", reading,
			async_mqtt5::retain_e::no, async_mqtt5::publish_props {}, use_nothrow_awaitable
		);		
		// An error can occur as a result of:
		//  a) wrong publish parameters
		//  b) mqtt_client::cancel is called while the Client is publishing the message
		//     resulting in cancellation.
		if (ec) {
			std::cout << "Publish error occurred: " << ec.message() << std::endl;
			break;
		}

		// Reason code is the reply from the server presenting the result of the publish operation.
		std::cout << "Result of publish request: " << rc.message() << std::endl;
		if (!rc)
			std::cout << "Published sensor reading: " << reading << std::endl;

		// Wait 5 seconds before publishing the next reading.
		timer.expires_after(std::chrono::seconds(5));
		auto&& [tec] = co_await timer.async_wait(use_nothrow_awaitable);

		// An error occurred if we cancelled the timer.
		if (tec)
			break;
	}

	co_return;
}

int main() {
	// Initialise execution context.
	boost::asio::io_context ioc;

	// Initialise the Client to connect to the Broker over TCP.
	client_type client(ioc);

	// Initialise the timer.
	boost::asio::steady_timer timer(ioc);

	// Set up signals to stop the program on demand.
	boost::asio::signal_set signals(ioc, SIGINT, SIGTERM);
	signals.async_wait([&client, &timer](async_mqtt5::error_code /* ec */, int /* signal */) {
		// After we are done with publishing all the messages, cancel the timer and the Client.
		// Alternatively, use mqtt_client::async_disconnect.
		timer.cancel();
		client.cancel();
	});

	// Spawn the coroutine.
	co_spawn(ioc.get_executor(), publish_sensor_readings(client, timer), boost::asio::detached);

	// Start the execution.
	ioc.run();
}

#endif

//]
