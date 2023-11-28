//[network_connectiong


//[tcp
#include <boost/asio/io_context.hpp>

#include <boost/asio/ip/tcp.hpp>

#include <async_mqtt5.hpp>

void tcp_setup() {
	boost::asio::io_context ioc;

	// Use ``__TCP_SOCKET__`` as the underlying stream.
	async_mqtt5::mqtt_client<boost::asio::ip::tcp::socket> client(ioc, "");
}

//]


//[tls
#include <boost/asio/io_context.hpp>
#include <boost/asio/ssl.hpp>

#include <boost/asio/ip/tcp.hpp>

#include <async_mqtt5.hpp>


// External customization point.
namespace async_mqtt5 {

template <typename StreamBase>
struct tls_handshake_type<asio::ssl::stream<StreamBase>> {
	static constexpr auto client = asio::ssl::stream_base::client;
	static constexpr auto server = asio::ssl::stream_base::server;
};

// This client uses this funcction to indicate which hostname it is
// attempting to connect to at the start of the handshaking process.
template <typename StreamBase>
void assign_tls_sni(
	const authority_path& ap,
	boost::asio::ssl::context& ctx,
	boost::asio::ssl::stream<StreamBase>& stream
) {
	SSL_set_tlsext_host_name(stream.native_handle(), ap.host.c_str());
}

} // end namespace async_mqtt5

// The certificate file in PEM format.
constexpr char certificate[] =
"-----BEGIN CERTIFICATE-----\n"
"...........................\n"
"-----END CERTIFICATE-----\n"
;

void ssl_setup() {
	boost::asio::io_context ioc;

	// Context satisfying ``__TlsContext__`` requirements that the underlying SSL stream will use.
	// The purpose of the context is to allow us to set up TLS/SSL-related options. 
	// See ``__SSL__`` for more information and options.
	boost::asio::ssl::context context(boost::asio::ssl::context::tls_client);

	async_mqtt5::error_code ec;

	// Add the trusted certificate authority for performing verification.
	context.add_certificate_authority(boost::asio::buffer(certificate), ec);

	// Set peer verification mode used by the context.
	// This will verify that the server's certificate is valid and signed by a trusted certificate authority.
	context.set_verify_mode(boost::asio::ssl::verify_peer, ec);

	// Use ``__SSL_STREAM__`` as the ``__StreamType__`` with ``__SSL_CONTEXT__`` as the ``__TlsContext__``.
	async_mqtt5::mqtt_client<
		boost::asio::ssl::stream<boost::asio::ip::tcp::socket>,
		boost::asio::ssl::context
	> client(ioc, "", std::move(context));
}
//]

//[websocket_tcp
#include <boost/asio/io_context.hpp>

#include <boost/asio/ip/tcp.hpp>

#include <boost/beast/websocket.hpp>

#include <async_mqtt5.hpp>

void websocket_tcp_setup() {
	boost::asio::io_context ioc;

	// Use ``[beastreflink boost__beast__websocket__stream websocket::stream<__TCP_SOCKET__>]`` as the underlying stream.
	async_mqtt5::mqtt_client<
		boost::beast::websocket::stream<boost::asio::ip::tcp::socket>
	> client(ioc, "");
}

//]

//[websocket_tls
#include <boost/asio/io_context.hpp>
#include <boost/asio/ssl.hpp>

#include <boost/asio/ip/tcp.hpp>

#include <boost/beast/websocket.hpp>

#include <async_mqtt5.hpp>

constexpr char ca[] =
"-----BEGIN CERTIFICATE-----\n"
"...........................\n"
"-----END CERTIFICATE-----\n"
;

namespace boost::beast::websocket {

// ``[beastreflink boost__beast__websocket__async_teardown boost::beast::websocket::async_teardown]`` is a free function
// designed to initiate the asynchronous teardown of a connection.
// The specific behaviour of this function is based on the NextLayer type (Socket type) used to create the ``__WEBSOCKET_STREAM__``.
// ``__Beast__`` library includes an implementation of this function for ``__TCP_SOCKET__``.
// However, the callers are responsible for providing a suitable overload of this function for any other type,
// such as ``__SSL_STREAM__`` as shown in this example.
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

template <typename streambase>
void assign_tls_sni(
	const authority_path& ap,
	asio::ssl::context& ctx,
	asio::ssl::stream<streambase>& stream
) {
	ssl_set_tlsext_host_name(stream.native_handle(), ap.host.c_str());
}

} // end namespace async_mqtt5

void websocket_tls_setup() {
	boost::asio::io_context ioc;

	boost::asio::ssl::context context(boost::asio::ssl::context::tls_client);

	async_mqtt5::error_code ec;
	context.add_certificate_authority(boost::asio::buffer(ca), ec);
	context.set_verify_mode(boost::asio::ssl::verify_peer, ec);

	async_mqtt5::mqtt_client<
		boost::beast::websocket::stream<boost::asio::ssl::stream<boost::asio::ip::tcp::socket>>,
		boost::asio::ssl::context
	> client(ioc, "", std::move(context));
}

//]


//]
