//
// Copyright (c) 2023-2024 Ivica Siladic, Bruno Iljazovic, Korina Simicevic
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//

//[timeout_with_awaitable_operators
#include <chrono>
#include <iostream>

#include <boost/asio/as_tuple.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>
#include <boost/asio/experimental/parallel_group.hpp>
#include <boost/asio/ip/tcp.hpp>

#include <async_mqtt5.hpp>

#ifdef BOOST_ASIO_HAS_CO_AWAIT

// Modified completion token that will prevent co_await from throwing exceptions.
constexpr auto use_nothrow_awaitable = boost::asio::as_tuple(boost::asio::use_awaitable);

using client_type = async_mqtt5::mqtt_client<boost::asio::ip::tcp::socket>;

boost::asio::awaitable<void> send_over_mqtt(client_type& client, const std::string& message) {
	client.brokers("<your-mqtt-broker>", 1883)
		.async_run(boost::asio::detached);

	auto&& [pec, prc, puback_props] = co_await client.async_publish<async_mqtt5::qos_e::at_least_once>(
		"<your-mqtt-topic>", message,
		async_mqtt5::retain_e::no, async_mqtt5::publish_props {},
		use_nothrow_awaitable
	);

	co_await client.async_disconnect(use_nothrow_awaitable);
}

int main() {
	boost::asio::io_context ioc;

	co_spawn(ioc, [&ioc]() -> boost::asio::awaitable<void> {
		// Construct the Client.
		async_mqtt5::mqtt_client<boost::asio::ip::tcp::socket> client(ioc);

		// Construct the timer.
		boost::asio::steady_timer timer(ioc, std::chrono::seconds(5));

		using namespace boost::asio::experimental::awaitable_operators;
		auto res = co_await (
			send_over_mqtt(client, "Hello world!") ||
			timer.async_wait(use_nothrow_awaitable)
		);

		// The timer expired first. The client is cancelled.
		if (res.index() == 1)
			std::cout << "Send over MQTT timed out!" << std::endl;
		// send_over_mqtt completed first. The timer is cancelled.
		else
			std::cout << "Send over MQTT completed!" << std::endl;
		
	}, boost::asio::detached);

	ioc.run();
}

#endif

//]
