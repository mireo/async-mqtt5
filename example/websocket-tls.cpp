#include <iostream>

#include <boost/asio/io_context.hpp>
#include <boost/asio/ssl.hpp>

#include <boost/asio/ip/tcp.hpp>

#include <boost/beast/websocket.hpp>

#include <async_mqtt5.hpp>

namespace asio = boost::asio;

namespace boost::beast::websocket {

template <typename TeardownHandler>
void async_teardown(
	boost::beast::role_type role,
	asio::ssl::stream<asio::ip::tcp::socket>& stream,
	TeardownHandler&& handler
) {
	return stream.async_shutdown(std::forward<TeardownHandler>(handler));
}

} // end namespace boost::beast::websocket

namespace async_mqtt5 {

template <typename StreamBase>
struct tls_handshake_type<asio::ssl::stream<StreamBase>> {
	static constexpr auto client = asio::ssl::stream_base::client;
	static constexpr auto server = asio::ssl::stream_base::server;
};

template <typename StreamBase>
void assign_tls_sni(
	const authority_path& ap,
	asio::ssl::context& ctx,
	asio::ssl::stream<StreamBase>& stream
) {
	SSL_set_tlsext_host_name(stream.native_handle(), ap.host.c_str());
}

} // end namespace async_mqtt5

constexpr const char spacetime_ca[] =
	"-----BEGIN CERTIFICATE-----\n"
	"MIIDYDCCAkigAwIBAgIUZZsEKT8m+uGZRNMaTuCiZBchSU4wDQYJKoZIhvcNAQEL\n"
	"BQAwHTEbMBkGA1UEAwwSTWlyZW8gU3BhY2VUaW1lIENBMB4XDTIzMDIwNzIwMzU1\n"
	"MFoXDTMzMDIwNDIwMzU1MFowHTEbMBkGA1UEAwwSTWlyZW8gU3BhY2VUaW1lIENB\n"
	"MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAzZshi2nJNyYZ4aJN+q27\n"
	"wA69lUAwRSHiJGBCGzppLue/LFDDC1t8GDicjYLGH5eJOlFwr8TbAr+ZH+/PyBoS\n"
	"7g5tsSn5xZhgEaivnq1MJNqYWHqW5KF2KhGxzzyC6m3JFK21H0xiJu9ej2wQs1tD\n"
	"ZWG3Y7pKeMFhCezEip5ueIyvmjsenK00TJKr6w1Rkr4BA40euLb5r0srWllKKUyl\n"
	"t5AEFghdVU7GeXfC2LPrzzMVngFWTaoL3QRf7VMhvNC0Xq7h2yjwd4wROYiJFZBj\n"
	"UgDSi2W50fPlVDliET2hPBR6lQPgCBRoIdQF8NneSBJ5xH+mw9ZZV8btL8ahwWtL\n"
	"GwIDAQABo4GXMIGUMB0GA1UdDgQWBBSM9pLZlAekgqt7ZXzPOdTEifMLmzBYBgNV\n"
	"HSMEUTBPgBSM9pLZlAekgqt7ZXzPOdTEifMLm6EhpB8wHTEbMBkGA1UEAwwSTWly\n"
	"ZW8gU3BhY2VUaW1lIENBghRlmwQpPyb64ZlE0xpO4KJkFyFJTjAMBgNVHRMEBTAD\n"
	"AQH/MAsGA1UdDwQEAwIBBjANBgkqhkiG9w0BAQsFAAOCAQEAuSe6ZOwc8KnNXs1M\n"
	"KoShOUxZGDFBUJFNAtTSsMi0ap6GIo/yJr+6SAkHkVU0HFkl5lzRo9aUHRw4O7Ez\n"
	"579JMzUDdEGBxtYqda0Rxnw8N2mq5Fxpv+1b6v4GsWA30k6TdqnrFdNpFVI84W6u\n"
	"Fw3HTKA0Ah0jXryc1kC1jU7mYKf66TDI5PSbuZRjHgQzzyUXZmCn1WcLbvunsc4r\n"
	"Tk2FrfXHfvag12yPLc9aIOrtfRW2wtlZcxMzX4oE6wfllAIIsSZGx0muydiMe8bw\n"
	"Od5S0p1sspsWOthj1t9yhHMwznwV81QLePWzgGmml21uA067ZGG8NHxNbERd/9e+\n"
	"Qz9m6w==\n"
	"-----END CERTIFICATE-----\n"
;

void publish_qos0_websocket_tls() {
	std::cout << "[Test-publish-qos0-websocket-tls]" << std::endl;
	using namespace async_mqtt5;
	asio::io_context ioc;

	using stream_type = boost::beast::websocket::stream<
		asio::ssl::stream<asio::ip::tcp::socket>
	>;

	error_code ec;
	asio::ssl::context tls_context(asio::ssl::context::tls_client);
	tls_context.add_certificate_authority(asio::buffer(spacetime_ca), ec);
	tls_context.set_verify_mode(asio::ssl::verify_peer);

	using client_type = mqtt_client<stream_type, decltype(tls_context)>;
	client_type c(ioc, "", std::move(tls_context));

	c.credentials("test-qos0-websocket-tls", "", "")
		.brokers("iot.fcluster.mireo.hr/mqtt", 8884)
		.will({ "test/mqtt-test", "Client disconnected!", async_mqtt5::qos_e::at_least_once })
		.run();

	c.async_publish<qos_e::at_most_once>(
		"test/mqtt-test", "hello world with qos0!",
		retain_e::no, publish_props{},
		[&c](error_code ec) {
			std::cout << "error_code: " << ec.message() << std::endl;
			c.cancel();
		}
	);

	ioc.run();
	return;
}

void publish_qos1_websocket_tls() {
	std::cout << "[Test-publish-qos1-websocket-tls]" << std::endl;
	using namespace async_mqtt5;
	asio::io_context ioc;

	using stream_type = boost::beast::websocket::stream<
		asio::ssl::stream<asio::ip::tcp::socket>
	>;

	error_code ec;
	asio::ssl::context tls_context(asio::ssl::context::tls_client);
	tls_context.add_certificate_authority(asio::buffer(spacetime_ca), ec);
	tls_context.set_verify_mode(asio::ssl::verify_peer);

	using client_type = mqtt_client<stream_type, decltype(tls_context)>;
	client_type c(ioc, "", std::move(tls_context));

	c.credentials("test-qos1-websocket-tls", "", "")
		.brokers("iot.fcluster.mireo.hr/mqtt", 8884)
		.will({ "test/mqtt-test", "Client disconnected!", qos_e::at_least_once })
		.run();

	c.async_publish<qos_e::at_least_once>(
		"test/mqtt-test", "hello world with qos1!",
		retain_e::no, publish_props{},
		[&c](error_code ec, reason_code rc, puback_props) {
			std::cout << "error_code: " << ec.message() << std::endl;
			std::cout << "reason_code: " << rc.message() << std::endl;
			c.cancel();
		}
	);

	ioc.run();
	return;
}

void publish_qos2_websocket_tls() {
	std::cout << "[Test-publish-qos2-websocket-tls]" << std::endl;
	using namespace async_mqtt5;
	asio::io_context ioc;

	using stream_type = boost::beast::websocket::stream<
		asio::ssl::stream<asio::ip::tcp::socket>
	>;

	error_code ec;
	asio::ssl::context tls_context(asio::ssl::context::tls_client);
	tls_context.add_certificate_authority(asio::buffer(spacetime_ca), ec);
	tls_context.set_verify_mode(asio::ssl::verify_peer);

	using client_type = mqtt_client<stream_type, decltype(tls_context)>;
	client_type c(ioc, "", std::move(tls_context));

	c.credentials("test-qos2-websocket-tls", "", "")
		.brokers("iot.fcluster.mireo.hr/mqtt", 8884)
		.will({ "test/mqtt-test", "Client disconnected!", qos_e::at_least_once })
		.run();

	c.async_publish<qos_e::exactly_once>(
		"test/mqtt-test", "hello world with qos2!",
		retain_e::no, publish_props{},
		[&c](error_code ec, reason_code rc, pubcomp_props) {
			std::cout << "error_code: " << ec.message() << std::endl;
			std::cout << "reason_code: " << rc.message() << std::endl;
			c.cancel();
		}
	);

	ioc.run();
	return;
}


void subscribe_and_receive_websocket_tls(int num_receive) {
	std::cout << "[Test-subscribe-and-receive-websocket-tls]" << std::endl;
	using namespace async_mqtt5;
	asio::io_context ioc;

	using stream_type = boost::beast::websocket::stream<
		asio::ssl::stream<asio::ip::tcp::socket>
	>;

	error_code ec;
	asio::ssl::context tls_context(asio::ssl::context::tls_client);
	tls_context.add_certificate_authority(asio::buffer(spacetime_ca), ec);
	tls_context.set_verify_mode(asio::ssl::verify_peer);

	using client_type = mqtt_client<stream_type, decltype(tls_context)>;
	client_type c(ioc, "", std::move(tls_context));

	c.credentials("test-subscriber-websocket-tls", "", "")
		.brokers("iot.fcluster.mireo.hr/mqtt", 8884)
		.will({ "test/mqtt-test", "Client disconnected!", qos_e::at_least_once })
		.run();

	std::vector<subscribe_topic> topics;
	topics.push_back(subscribe_topic{
		"test/mqtt-test", {
			qos_e::exactly_once,
			subscribe_options::no_local_e::no,
			subscribe_options::retain_as_published_e::retain,
			subscribe_options::retain_handling_e::send
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
					c.cancel();
			}
		);
	}

	ioc.run();
	return;
}


void run_websocket_tls_examples() {
	publish_qos0_websocket_tls();
	publish_qos1_websocket_tls();
	publish_qos2_websocket_tls();
	subscribe_and_receive_websocket_tls(1);
}
