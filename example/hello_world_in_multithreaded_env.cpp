//
// Copyright (c) 2023-2024 Ivica Siladic, Bruno Iljazovic, Korina Simicevic
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//

//[hello_world_in_multithreaded_env
#include <iostream>
#include <vector>
#include <thread>

#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/ip/tcp.hpp>

#include <async_mqtt5.hpp>

int main() {
	// Create a multithreaded environment where 4 threads
	// will be calling ioc.run().

	// Number of threads that will call io_context::run().
	int thread_num = 4;
	boost::asio::io_context ioc(4);

	// Create the remaining threads (aside of this one).
	std::vector<std::thread> threads;
	threads.reserve(thread_num - 1);
	
	// Create an explicit strand from io_context's executor.
	// The strand guarantees a serialised handler execution regardless of the 
	// number of threads running in the io_context.
	boost::asio::strand strand = boost::asio::make_strand(ioc.get_executor());

	// Create the Client with the explicit strand as the default associated executor.
	async_mqtt5::mqtt_client<boost::asio::ip::tcp::socket> client(strand);

	// Configure the client.
	client.brokers("<your-mqtt-broker>", 1883);

	// Start the Client.
	// The async_run function call must be posted/dispatched to the strand.
	boost::asio::post(
		strand,
		[&client, &strand] { 
			// Considering that the default associated executor of all completion handlers is the strand,
			// it is not necessary to explicitly bind it to async_run or other async_xxx's handlers.
			client.async_run(boost::asio::detached);
		}
	);
	
	// The async_publish function call must be posted/dispatched to the strand.
	// The associated executor of async_publish's completion handler must be the same strand.
	boost::asio::post(
		strand,
		[&client, &strand] { 
			assert(strand.running_in_this_thread());

			client.async_publish<async_mqtt5::qos_e::at_least_once>(
				"<your-mqtt-topic>", "Hello world!", async_mqtt5::retain_e::no,
				async_mqtt5::publish_props {},
				// You may bind the strand to this handler, but it is not necessary
				// as the strand is already the default associated handler.
				// However, you must not bind it to any other executor!
				[&client, &strand](
					async_mqtt5::error_code ec, async_mqtt5::reason_code rc,
					async_mqtt5::puback_props props
				) {
					assert(strand.running_in_this_thread());

					std::cout << ec.message() << std::endl;
					std::cout << rc.message() << std::endl;

					// Stop the Client. This will cause ioc.run() to return.
					client.cancel();
				}
			);
		}
	);

	// Call ioc.run() on the other threads.
	for (int i = 0; i < thread_num - 1; ++i)
		threads.emplace_back([&ioc] { ioc.run(); });

	// Call ioc.run() on this thread.
	ioc.run();

	for (auto& t : threads)
		if (t.joinable()) t.join();

	return 0;
}

//]
