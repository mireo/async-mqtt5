//
// Copyright (c) 2023-2024 Ivica Siladic, Bruno Iljazovic, Korina Simicevic
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/asio/use_awaitable.hpp>
#ifdef BOOST_ASIO_HAS_CO_AWAIT

//[receiver
#include <iostream>
#include <string>

#include <boost/asio/as_tuple.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/deferred.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>

#include <boost/asio/ip/tcp.hpp>

#include <async_mqtt5/logger.hpp>
#include <async_mqtt5/mqtt_client.hpp>
#include <async_mqtt5/reason_codes.hpp>
#include <async_mqtt5/types.hpp>

struct config {
	std::string brokers = "broker.hivemq.com";
	uint16_t port = 1883;
	std::string client_id = "async_mqtt5_tester";
};

// Modified completion token that will prevent co_await from throwing exceptions.
constexpr auto use_nothrow_awaitable = boost::asio::as_tuple(boost::asio::deferred);

// client_type with logging enabled
using client_type = async_mqtt5::mqtt_client<
	boost::asio::ip::tcp::socket, std::monostate /* TlsContext */, async_mqtt5::logger
>;

// client_type without logging
//using client_type = async_mqtt5::mqtt_client<boost::asio::ip::tcp::socket>;

boost::asio::awaitable<bool> subscribe(client_type& client) {
	// Configure the request to subscribe to a Topic.
	async_mqtt5::subscribe_topic sub_topic = async_mqtt5::subscribe_topic{
		"test" /* topic */,
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

boost::asio::awaitable<void> subscribe_and_receive(
	const config& cfg, client_type& client
) {
	// Configure the Client.
	// It is mandatory to call brokers() and async_run() to configure the Brokers to connect to and start the Client.
	client.brokers(cfg.brokers, cfg.port) // Broker that we want to connect to.
		.credentials(cfg.client_id) // Set the Client Identifier. (optional)
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

int main(int argc, char** argv) {
	config cfg;

	if (argc == 4) {
		cfg.brokers = argv[1];
		cfg.port = uint16_t(std::stoi(argv[2]));
		cfg.client_id = argv[3];
	}

	// Initialise execution context.
	boost::asio::io_context ioc;

	// Initialise the Client to connect to the Broker over TCP.
	client_type client(ioc, {} /* tls_context */, async_mqtt5::logger(async_mqtt5::log_level::info));

	// Set up signals to stop the program on demand.
	boost::asio::signal_set signals(ioc, SIGINT, SIGTERM);
	signals.async_wait([&client](async_mqtt5::error_code /* ec */, int /* signal */) {
		// After we are done with publishing all the messages, cancel the timer and the Client.
		// Alternatively, use mqtt_client::async_disconnect.
		client.cancel();
	});

	// Spawn the coroutine.
	co_spawn(
		ioc,
		subscribe_and_receive(cfg, client),
		[](std::exception_ptr e) {
			if (e)
				std::rethrow_exception(e);
		}
	);

	// Start the execution.
	ioc.run();
}

//]

#else

#include <iostream>

int main() {
	std::cout << "This example requires C++20 standard to compile and run" << std::endl;
}

#endif
