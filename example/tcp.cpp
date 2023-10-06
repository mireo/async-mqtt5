#include <fmt/format.h>

#include <boost/asio/io_context.hpp>

#include <boost/asio/ip/tcp.hpp>

#include <async_mqtt5.hpp>

namespace asio = boost::asio;

void publish_qos0_tcp() {
	fmt::print("[Test-publish-qos0-tcp]\n");
	using namespace async_mqtt5;
	asio::io_context ioc;

	using stream_type = asio::ip::tcp::socket;
	using client_type = mqtt_client<stream_type>;
	client_type c(ioc, "");

	c.credentials("test-qos0-tcp", "", "")
		.brokers("mqtt.mireo.local", 1883)
		.will({ "test/mqtt-test", "i died",qos_e::at_least_once })
		.run();

	c.async_publish<qos_e::at_most_once>(
		"test/mqtt-test", "hello world with qos0!",
		retain_e::no, publish_props {},
		[&c](error_code ec) {
			fmt::print("\terror_code: {}\n", ec.message());
			c.cancel();
		}
	);

	ioc.run();
}


void publish_qos1_tcp() {
	fmt::print("[Test-publish-qos1-tcp]\n");
	using namespace async_mqtt5;
	asio::io_context ioc;

	using stream_type = asio::ip::tcp::socket;
	using client_type = mqtt_client<stream_type>;
	client_type c(ioc, "");

	c.credentials("test-qos1-tcp", "", "")
		.brokers("mqtt.mireo.local", 1883)
		.will({ "test/mqtt-test", "i died", qos_e::at_least_once })
		.run();

	c.async_publish<qos_e::at_least_once>(
		"test/mqtt-test", "hello world with qos1!",
		retain_e::no, publish_props {},
		[&c](error_code ec, reason_code rc, puback_props) {
			fmt::print(
				"\terror_code: {}, reason_code: {}\n",
				ec.message(), rc.message()
			);
			c.cancel();
		}
	);

	ioc.run();
}

void publish_qos2_tcp() {
	fmt::print("[Test-publish-qos2-tcp]\n");
	using namespace async_mqtt5;
	asio::io_context ioc;

	using stream_type = asio::ip::tcp::socket;
	using client_type = mqtt_client<stream_type>;
	client_type c(ioc, "");

	c.credentials("test-qos2-tcp", "", "")
		.brokers("mqtt.mireo.local", 1883)
		.will({ "test/mqtt-test", "i died", qos_e::at_least_once })
		.run();

	c.async_publish<qos_e::exactly_once>(
		"test/mqtt-test", "hello world with qos2!",
		retain_e::no, publish_props{},
		[&c](error_code ec, reason_code rc, pubcomp_props) {
			fmt::print(
				"\terror_code: {}, reason_code: {}\n",
				ec.message(), rc.message()
			);
			c.cancel();
		}
	);

	ioc.run();
}


void subscribe_and_receive_tcp(int num_receive) {
	fmt::print("[Test-subscribe-and-receive-tcp]\n");
	using namespace async_mqtt5;
	asio::io_context ioc;

	using stream_type = asio::ip::tcp::socket;
	using client_type = mqtt_client<stream_type>;
	client_type c(ioc, "");

	c.credentials("test-subscriber-tcp", "", "")
		.brokers("mqtt.mireo.local", 1883)
		.will({ "test/mqtt-test", "i died", qos_e::at_least_once })
		.run();

	c.async_subscribe(
		{ "test/mqtt-test", { qos_e::exactly_once } }, subscribe_props {},
		[](error_code ec, std::vector<reason_code> codes, suback_props) {
			if (ec == asio::error::operation_aborted)
				return;
			fmt::print(
				"\tsubscribe error_code: {}, reason_code: {}\n",
				ec.message(), codes[0].message()
			);
		}
	);


	for (auto i = 0; i < num_receive; i++) {
		c.async_receive(
			[&c, i, num_receive] (
				error_code ec, std::string topic,
				std::string payload, publish_props
			) {
				if (ec == asio::error::operation_aborted)
					return;
				fmt::print(
					"\tmessage {}/{}: ec: {}, topic: {}, payload: {}\n",
					i + 1, num_receive, ec.message(), topic, payload
				);

				if (i == num_receive - 1)
					c.cancel();
			}
		);
	}

	ioc.run();
}


void run_tcp_examples() {
	publish_qos0_tcp();
	publish_qos1_tcp();
	publish_qos2_tcp();
	subscribe_and_receive_tcp(1);
}
