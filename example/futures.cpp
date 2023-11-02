//[futures_examples

#include <boost/asio/io_context.hpp>
#include <boost/asio/use_future.hpp>

#include <boost/asio/ip/tcp.hpp>

#include <async_mqtt5.hpp>

#include <iostream>
#include <thread>

namespace asio = boost::asio;

using stream_type = asio::ip::tcp::socket;
using client_type = async_mqtt5::mqtt_client<stream_type>;

/**
 * This function is a reference on how to use the mqtt_client with ``__USE_FUTURE__``
 * as the completion token. Each get() call on std::future will block the current
 * thread and wait until the future has a valid result. That is why it is essential
 * to ensure that the execution context is running in more than one thread.
 */
void run_with_future(client_type& client) {
	// Just like the asio::use_awaitable completion token
	// (see ``__EXAMPLE_COROUTINE__``), the ``__USE_FUTURE__`` completion token
	// will not return the error_code. Instead, it will throw an exception if an error has occurred.
	std::future<void> pub_qos0_fut =
		client.async_publish<async_mqtt5::qos_e::at_most_once>(
			"test/mqtt-test", "Hello world!",
			async_mqtt5::retain_e::yes, async_mqtt5::publish_props {},
			asio::use_future
	);
	pub_qos0_fut.get(); // Blocking call!

	using qos1_fut_type = std::tuple<async_mqtt5::reason_code, async_mqtt5::puback_props>;
	std::future<qos1_fut_type> pub_qos1_fut =
		client.async_publish<async_mqtt5::qos_e::at_least_once>(
			"test/mqtt-test", "Hello world!",
			async_mqtt5::retain_e::yes, async_mqtt5::publish_props {},
			asio::use_future
	);
	auto [qos1_rc, puback_props] = pub_qos1_fut.get();
	std::cout << "Publish QoS 1 Reason Code: " << qos1_rc.message() << std::endl;

	using qos2_fut_type = std::tuple<async_mqtt5::reason_code, async_mqtt5::pubcomp_props>;
	std::future<qos2_fut_type> pub_qos2_fut =
		client.async_publish<async_mqtt5::qos_e::exactly_once>(
			"test/mqtt-test", "Hello world!",
			async_mqtt5::retain_e::yes, async_mqtt5::publish_props {},
			asio::use_future
	);
	auto [qos2_rc, pubcomp_props] = pub_qos2_fut.get();
	std::cout << "Publish QoS 2 Reason Code: " << qos2_rc.message() << std::endl;

	auto sub_topic = async_mqtt5::subscribe_topic{
		"test/mqtt-test", {
			async_mqtt5::qos_e::exactly_once,
			async_mqtt5::subscribe_options::no_local_e::no,
			async_mqtt5::subscribe_options::retain_as_published_e::retain,
			async_mqtt5::subscribe_options::retain_handling_e::send
		}
	};

	using sub_fut_type = std::tuple<std::vector<async_mqtt5::reason_code>, async_mqtt5::suback_props>;
	std::future<sub_fut_type> sub_fut = client.async_subscribe(
		sub_topic, async_mqtt5::subscribe_props {}, asio::use_future
	);
	auto [sub_rcs, suback_props] = sub_fut.get();
	std::cout << "Subscribe Reason Code: " << sub_rcs[0].message() << std::endl;

	// Note: the get() call on async_receive future could block indefinitely if the mqtt_client
	// failed to subscribe or there are no Application Messages to be received from the subcribed Topic!
	if (!sub_rcs[0]) {
		using rec_fut_type = std::tuple<std::string, std::string, async_mqtt5::publish_props>;
		std::future<rec_fut_type> rec_fut = client.async_receive(asio::use_future);
		auto [topic, payload, publish_props] = rec_fut.get();
		std::cout << "Received message from Topic: " << topic << ", " << payload << std::endl;
	}

	using unsub_fut_type = std::tuple<std::vector<async_mqtt5::reason_code>, async_mqtt5::unsuback_props>;
	std::future<unsub_fut_type> unsub_fut = client.async_unsubscribe(
		"test/mqtt-test", async_mqtt5::unsubscribe_props {}, asio::use_future
	);
	auto [unsub_rcs, unsuback_props] = unsub_fut.get();
	std::cout << "Unubscribe Reason Code: " << unsub_rcs[0].message() << std::endl;

	std::future<void> dc_fut = client.async_disconnect(asio::use_future);
	dc_fut.get();

	return;
}

int main(int argc, char** argv) {
	// asio::io_context must be running in more than one thread!
	constexpr auto thread_num = 2;
	asio::io_context ioc(thread_num);

	std::vector<std::thread> threads;
	threads.reserve(thread_num - 1);

	// Make an instance of mqtt_client. Establish a TCP connection with the Broker.
	client_type c(ioc.get_executor(), "");

	c.credentials("test-client", "", "")
		.brokers("mqtt.mireo.local", 1883)
		.run();

	for (int i = 0; i < thread_num - 1; ++i)
		threads.emplace_back([&ioc] { ioc.run(); });

	run_with_future(c);

	ioc.run();

	for (auto& t : threads)
		if (t.joinable()) t.join();
}


//]
