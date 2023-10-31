//[callbacks_examples

#include <boost/asio/io_context.hpp>

#include <boost/asio/ip/tcp.hpp>

#include <async_mqtt5.hpp>

#include <iostream>

namespace asio = boost::asio;

using stream_type = asio::ip::tcp::socket;
using client_type = async_mqtt5::mqtt_client<stream_type>;
/**
 * This function showcases how to call each asynchronous function
 * in mqtt_client using callbacks. Note that this example is not
 * intended for direct execution, as the async_disconnect call
 * will promptly close the client.
 */
void run_with_callbacks(client_type& client) {
	// Publish an Application Message with QoS 0.
	client.async_publish<async_mqtt5::qos_e::at_most_once>(
		"test/mqtt-test", "Hello world!",
		async_mqtt5::retain_e::no, async_mqtt5::publish_props {},
		// Callback with signature void (error_code)
		[](async_mqtt5::error_code ec) {
			std::cout << "error_code: " << ec.message() << std::endl;
		}
	);

	// Publish an Application Message with QoS 1.
	client.async_publish<async_mqtt5::qos_e::at_least_once>(
		"test/mqtt-test", "Hello world!",
		async_mqtt5::retain_e::yes, async_mqtt5::publish_props {},
		// Callback with signature void (error_code, reason_code, puback_props)
		[](async_mqtt5::error_code ec, async_mqtt5::reason_code rc, async_mqtt5::puback_props) {
			std::cout << "error_code: " << ec.message() << std::endl;
			std::cout << "reason_code: " << rc.message() << std::endl;
		}
	);

	// Publish an Application Message with QoS 2.
	client.async_publish<async_mqtt5::qos_e::exactly_once>(
		"test/mqtt-test", "Hello world!",
		async_mqtt5::retain_e::no, async_mqtt5::publish_props {},
		// Callback with signature (error_code, reason_code, pubcomp_props)
		[](async_mqtt5::error_code ec, async_mqtt5::reason_code rc, async_mqtt5::pubcomp_props) {
			std::cout << "error_code: " << ec.message() << std::endl;
			std::cout << "reason_code: " << rc.message() << std::endl;
		}
	);

	// Subscribe to a single Topic.
	client.async_subscribe(
		{ "test/mqtt-test", { async_mqtt5::qos_e::exactly_once } }, async_mqtt5::subscribe_props {},
		// Callback with signature void (error_code, std::vector<reason_code>, suback_props)
		[](async_mqtt5::error_code ec, 
			std::vector<async_mqtt5::reason_code> codes, async_mqtt5::suback_props
		) {
			std::cout << "subscribe error_code: " << ec.message() << std::endl;
			std::cout << "subscribe reason_code: " << codes[0].message() << std::endl;
		}
	);

	// Receive an Application Message.
	client.async_receive(
		// Callback with signature void (error_code, std::string, std::string, publish_props)
		[] (
			async_mqtt5::error_code ec, std::string topic,
			std::string payload, async_mqtt5::publish_props
		) {
			std::cout << "topic: " << topic << std::endl;
			std::cout << "payload: " << payload << std::endl;
		}
	);

	// Unsubscribe from the Topic.
	client.async_unsubscribe("test/mqtt-test", async_mqtt5::unsubscribe_props {},
		//Callback with signature void (error_code, std::vector<reason_code>, unsuback_props)
		[](async_mqtt5::error_code ec, 
			std::vector<async_mqtt5::reason_code> codes, async_mqtt5::unsuback_props
		) {
			std::cout << "unsubscribe error_code: " << ec.message() << std::endl;
			std::cout << "unsubscribe reason_code: " << codes[0].message() << std::endl;
		}
	);

	// Disconnect the Client.
	client.async_disconnect(
		async_mqtt5::disconnect_rc_e::disconnect_with_will_message,
		async_mqtt5::disconnect_props {},
		// Callback with signature void (error_code)
		[](async_mqtt5::error_code) {}
	);

}

int main(int argc, char** argv) {
	asio::io_context ioc;

	// Make an instance of mqtt_client. Establish a TCP connection with the Broker.
	client_type c(ioc.get_executor(), "");

	c.credentials("test-client", "username", "password")
		.brokers("mqtt.broker", 1883)
		.run();

	run_with_callbacks(c);

	ioc.run();
}

//]
