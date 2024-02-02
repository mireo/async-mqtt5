#include <iostream>

#include <boost/asio/io_context.hpp>

#include <boost/asio/ip/tcp.hpp>

#include <boost/beast/websocket.hpp>

#include <async_mqtt5.hpp>

namespace asio = boost::asio;

void publish_qos0_websocket_tcp() {
	std::cout << "[Test-publish-qos0-websocket-tcp]" << std::endl;
	using namespace async_mqtt5;
	asio::io_context ioc;

	using stream_type = boost::beast::websocket::stream<
		asio::ip::tcp::socket
	>;

	using client_type = mqtt_client<stream_type>;
	client_type c(ioc, "");

	c.credentials("test-qos0-websocket-tcp", "", "")
		.brokers("emqtt.mireo.local/mqtt", 8083)
		.will({ "test/mqtt-test", "Client disconnected!", qos_e::at_least_once })
		.async_run(asio::detached);

	c.async_publish<qos_e::at_most_once>(
		"test/mqtt-test", "hello world with qos0!",
		retain_e::no, publish_props{},
		[&c](error_code ec) {
			std::cout << "error_code: " << ec.message() << std::endl;
			c.async_disconnect(asio::detached);
		}
	);

	ioc.run();
	return;
}

void publish_qos1_websocket_tcp() {
	std::cout << "[Test-publish-qos1-websocket-tcp]" << std::endl;
	using namespace async_mqtt5;
	asio::io_context ioc;

	using stream_type = boost::beast::websocket::stream<
		asio::ip::tcp::socket
	>;

	using client_type = mqtt_client<stream_type>;
	client_type c(ioc, "");

	c.credentials("test-qos1-websocket-tcp", "", "")
		.brokers("emqtt.mireo.local/mqtt", 8083)
		.will({ "test/mqtt-test", "Client disconnected!", qos_e::at_least_once })
		.async_run(asio::detached);

	c.async_publish<qos_e::at_least_once>(
		"test/mqtt-test", "hello world with qos1!",
		async_mqtt5::retain_e::no, publish_props{},
		[&c](error_code ec, reason_code rc, puback_props) {
			std::cout << "error_code: " << ec.message() << std::endl;
			std::cout << "reason_code: " << rc.message() << std::endl;
			c.async_disconnect(asio::detached);
		}
	);

	ioc.run();
	return;
}

void publish_qos2_websocket_tcp() {
	std::cout << "[Test-publish-qos2-websocket-tcp]" << std::endl;
	using namespace async_mqtt5;
	asio::io_context ioc;

	using stream_type = boost::beast::websocket::stream<
		asio::ip::tcp::socket
	>;

	using client_type = mqtt_client<stream_type>;
	client_type c(ioc, "");

	c.credentials("test-qos2-websocket-tcp", "", "")
		.brokers("emqtt.mireo.local/mqtt", 8083)
		.will({ "test/mqtt-test", "Client disconnected!", qos_e::at_least_once })
		.async_run(asio::detached);

	c.async_publish<qos_e::exactly_once>(
		"test/mqtt-test", "hello world with qos2!",
		retain_e::no, publish_props{},
		[&c](error_code ec, reason_code rc, pubcomp_props) {
			std::cout << "error_code: " << ec.message() << std::endl;
			std::cout << "reason_code: " << rc.message() << std::endl;
			c.async_disconnect(asio::detached);
		}
	);

	ioc.run();
	return;
}


void subscribe_and_receive_websocket_tcp(int num_receive) {
	std::cout << "[Test-subscribe-and-receive-websocket-tcp]" << std::endl;
	using namespace async_mqtt5;
	asio::io_context ioc;

	using stream_type = boost::beast::websocket::stream<
		asio::ip::tcp::socket
	>;

	using client_type = mqtt_client<stream_type>;
	client_type c(ioc, "");

	c.credentials("test-subscriber-websocket-tcp", "", "")
		.brokers("emqtt.mireo.local/mqtt", 8083)
		.will({ "test/mqtt-test", "Client disconnected!", qos_e::at_least_once })
		.async_run(asio::detached);

	std::vector<subscribe_topic> topics;
	topics.push_back(subscribe_topic{
		"test/mqtt-test", {
			qos_e::exactly_once,
			no_local_e::no,
			retain_as_published_e::retain,
			retain_handling_e::send
		}
	});

	c.async_subscribe(
		topics, subscribe_props{},
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
	return;
}


void run_websocket_tcp_examples() {
	publish_qos0_websocket_tcp();
	publish_qos1_websocket_tcp();
	publish_qos2_websocket_tcp();
	subscribe_and_receive_websocket_tcp(1);
}
