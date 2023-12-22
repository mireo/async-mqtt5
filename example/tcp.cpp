#include <iostream>

#include <boost/asio/io_context.hpp>

#include <boost/asio/ip/tcp.hpp>

#include <async_mqtt5.hpp>

namespace asio = boost::asio;

void publish_qos0_tcp() {
	std::cout << "[Test-publish-qos0-tcp]" << std::endl;
	using namespace async_mqtt5;
	asio::io_context ioc;

	using stream_type = asio::ip::tcp::socket;
	using client_type = mqtt_client<stream_type>;
	client_type c(ioc, "");

	connect_props props;
	props[prop::maximum_packet_size] = 1024;

	c.credentials("test-qos0-tcp", "", "")
		.brokers("emqtt.mireo.local", 1883)
		.will({ "test/mqtt-test", "Client disconnected!",qos_e::at_least_once })
		.connect_properties(std::move(props))
		.run();

	c.async_publish<qos_e::at_most_once>(
		"test/mqtt-test", "hello world with qos0!",
		retain_e::no, publish_props{},
		[&c](error_code ec) {
			std::cout << "error_code: " << ec.message() << std::endl;
			c.async_disconnect(asio::detached);
		}
	);


	ioc.run();
}


void publish_qos1_tcp() {
	std::cout << "[Test-publish-qos1-tcp]" << std::endl;
	using namespace async_mqtt5;
	asio::io_context ioc;

	using stream_type = asio::ip::tcp::socket;
	using client_type = mqtt_client<stream_type>;
	client_type c(ioc, "");

	c.credentials("test-qos1-tcp", "", "")
		.brokers("emqtt.mireo.local", 1883)
		.will({ "test/mqtt-test", "Client disconnected!", qos_e::at_least_once })
		.run();

	c.async_publish<qos_e::at_least_once>(
		"test/mqtt-test", "hello world with qos1!",
		retain_e::no, publish_props {},
		[&c](error_code ec, reason_code rc, puback_props) {
			std::cout << "error_code: " << ec.message() << std::endl;
			std::cout << "reason_code: " << rc.message() << std::endl;
			c.async_disconnect(asio::detached);
		}
	);

	ioc.run();
}

void publish_qos2_tcp() {
	std::cout << "[Test-publish-qos2-tcp]" << std::endl;
	using namespace async_mqtt5;
	asio::io_context ioc;

	using stream_type = asio::ip::tcp::socket;
	using client_type = mqtt_client<stream_type>;
	client_type c(ioc, "");

	c.credentials("test-qos2-tcp", "", "")
		.brokers("emqtt.mireo.local", 1883)
		.will({ "test/mqtt-test", "Client disconnected!", qos_e::at_least_once })
		.run();

	c.async_publish<qos_e::exactly_once>(
		"test/mqtt-test", "hello world with qos2!",
		retain_e::no, publish_props {},
		[&c](error_code ec, reason_code rc, pubcomp_props) {
			std::cout << "error_code: " << ec.message() << std::endl;
			std::cout << "reason_code: " << rc.message() << std::endl;
			c.async_disconnect(asio::detached);
		}
	);

	ioc.run();
}


void subscribe_and_receive_tcp(int num_receive) {
	std::cout << "[Test-subscribe-and-receive-tcp]" << std::endl;
	using namespace async_mqtt5;
	asio::io_context ioc;

	using stream_type = asio::ip::tcp::socket;
	using client_type = mqtt_client<stream_type>;
	client_type c(ioc, "");

	c.credentials("test-subscriber-tcp", "", "")
		.brokers("emqtt.mireo.local", 1883)
		.will({ "test/mqtt-test", "Client disconnected!", qos_e::at_least_once })
		.run();

	c.async_subscribe(
		{ "test/mqtt-test", { qos_e::exactly_once } }, subscribe_props {},
		[](error_code ec, std::vector<reason_code> codes, suback_props) {
			if (ec == asio::error::operation_aborted)
				return;
			std::cout << "subscribe error_code: " << ec.message() << std::endl;
			std::cout << "subscribe reason_code: " << codes[0].message() << std::endl;
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
				std::cout << "message " << i + 1 << "/" << num_receive << std::endl;
				std::cout << "error_code: " << ec.message() << std::endl;
				std::cout << "topic: " << topic << std::endl;
				std::cout << "payload: " << payload << std::endl;

				if (i == num_receive - 1)
					c.async_disconnect(asio::detached);
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
