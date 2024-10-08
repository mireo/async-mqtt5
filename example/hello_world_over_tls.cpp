//
// Copyright (c) 2023-2024 Ivica Siladic, Bruno Iljazovic, Korina Simicevic
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//

//[hello_world_over_tls
#include <iostream>

#include <boost/asio/io_context.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio/ip/tcp.hpp>

#include <async_mqtt5.hpp>

// External customization point.
namespace async_mqtt5 {

template <typename StreamBase>
struct tls_handshake_type<boost::asio::ssl::stream<StreamBase>> {
	static constexpr auto client = boost::asio::ssl::stream_base::client;
	static constexpr auto server = boost::asio::ssl::stream_base::server;
};

// This client uses this function to indicate which hostname it is
// attempting to connect to at the start of the handshaking process.
template <typename StreamBase>
void assign_tls_sni(
	const authority_path& ap,
	boost::asio::ssl::context& /* ctx */,
	boost::asio::ssl::stream<StreamBase>& stream
) {
	SSL_set_tlsext_host_name(stream.native_handle(), ap.host.c_str());
}

} // end namespace async_mqtt5

// The certificate file in the PEM format.
constexpr char ca_cert[] =
"-----BEGIN CERTIFICATE-----\n"
"...........................\n"
"-----END CERTIFICATE-----\n"
;

int main() {
	boost::asio::io_context ioc;

	// Context satisfying ``__TlsContext__`` requirements that the underlying SSL stream will use.
	// The purpose of the context is to allow us to set up TLS/SSL-related options. 
	// See ``__SSL__`` for more information and options.
	boost::asio::ssl::context context(boost::asio::ssl::context::tls_client);

	async_mqtt5::error_code ec;

	// Add the trusted certificate authority for performing verification.
	context.add_certificate_authority(boost::asio::buffer(ca_cert), ec);
	if (ec)
		std::cout << "Failed to add certificate authority!" << std::endl;
	ec.clear();

	// Set peer verification mode used by the context.
	// This will verify that the server's certificate is valid and signed by a trusted certificate authority.
	context.set_verify_mode(boost::asio::ssl::verify_peer, ec);
	if (ec)
		std::cout << "Failed to set peer verification mode!" << std::endl;
	ec.clear();

	// Construct the Client with ``__SSL_STREAM__`` as the underlying stream
	// with ``__SSL_CONTEXT__`` as the ``__TlsContext__`` type.
	async_mqtt5::mqtt_client<
		boost::asio::ssl::stream<boost::asio::ip::tcp::socket>,
		boost::asio::ssl::context
	> client(ioc, std::move(context));

	// 8883 is the default TLS MQTT port.
	client.brokers("<your-mqtt-broker>", 8883)
		.async_run(boost::asio::detached);

	client.async_publish<async_mqtt5::qos_e::at_most_once>(
		"<topic>", "Hello world!",
		async_mqtt5::retain_e::no, async_mqtt5::publish_props{},
		[&client](async_mqtt5::error_code ec) {
			std::cout << ec.message() << std::endl;
			client.async_disconnect(boost::asio::detached);
		}
	);

	ioc.run();
}
//]
