//[hello_world_in_coro_multithreaded_env
#include <vector>
#include <thread>

#include <boost/asio/use_awaitable.hpp>
#ifdef BOOST_ASIO_HAS_CO_AWAIT

#include <boost/asio/as_tuple.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/strand.hpp>

#include <boost/asio/ip/tcp.hpp>

#include <async_mqtt5.hpp>

using client_type = async_mqtt5::mqtt_client<boost::asio::ip::tcp::socket>;

// Modified completion token that will prevent co_await from throwing exceptions.
constexpr auto use_nothrow_awaitable = boost::asio::as_tuple(boost::asio::use_awaitable);

boost::asio::awaitable<void> publish_hello_world(
	client_type& client,
	const boost::asio::strand<boost::asio::io_context::executor_type>& strand
) {
	// Confirmation that the coroutine running in the strand.
	assert(strand.running_in_this_thread());

	// All these function calls will be executed by the strand that is executing the coroutine.
	// All the completion handler's associated executors will be that same strand
	// because the Client was constructed with it as the default associated executor.
	client.brokers("<your-mqtt-broker>", 1883)
		.async_run(boost::asio::detached);

	auto&& [ec, rc, puback_props] = co_await client.async_publish<async_mqtt5::qos_e::at_least_once>(
		"<your-mqtt-topic>", "Hello world!", async_mqtt5::retain_e::no,
		async_mqtt5::publish_props {}, use_nothrow_awaitable);

	co_await client.async_disconnect(use_nothrow_awaitable);
	co_return;
}

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
	client_type client(strand);

	// Spawn the coroutine.
	// The executor that executes the coroutine must be the same executor
	// that is the Client's default associated executor.
	co_spawn(strand, publish_hello_world(client, strand), boost::asio::detached);

	// Call ioc.run() in the other threads.
	for (int i = 0; i < thread_num - 1; ++i)
		threads.emplace_back([&ioc] { ioc.run(); });

	// Call ioc.run() on this thread.
	ioc.run();

	for (auto& t : threads)
		if (t.joinable()) t.join();

	return 0;
}

#endif

//]
