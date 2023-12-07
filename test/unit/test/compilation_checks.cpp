#include <boost/test/unit_test.hpp>

#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/asio/io_context.hpp>

#include <boost/beast/websocket/stream.hpp>

#include <async_mqtt5/detail/any_authenticator.hpp>
#include <async_mqtt5/detail/async_traits.hpp>

#include <async_mqtt5/mqtt_client.hpp>

using namespace async_mqtt5;

BOOST_AUTO_TEST_SUITE(traits/*, *boost::unit_test::disabled()*/)

class good_authenticator {
public:
	good_authenticator() = default;

	template <typename CompletionToken>
	decltype(auto) async_auth(
		auth_step_e step, std::string data,
		CompletionToken&& token
	) {
		using error_code = boost::system::error_code;
		using Signature = void(error_code, std::string);

		auto initiate = [](auto, auth_step_e, std::string) {};

		return asio::async_initiate<CompletionToken, Signature>(
			initiate, token, step, std::move(data)
		);
	}

	std::string_view method() const {
		return "method";
	}
};

class bad_authenticator {
public:
	bad_authenticator() = default;

	void async_auth(std::string data) {}

	std::string_view method() const {
		return "method";
	}
};



BOOST_AUTO_TEST_CASE(is_authenticator) {
	BOOST_STATIC_ASSERT(detail::is_authenticator<good_authenticator>);
	BOOST_STATIC_ASSERT(!detail::is_authenticator<bad_authenticator>);
}

namespace asio = boost::asio;
namespace beast = boost::beast;

using tcp_layer = asio::ip::tcp::socket;
using tls_layer = asio::ssl::stream<asio::ip::tcp::socket>;
using websocket_tcp_layer = beast::websocket::stream<tcp_layer>;
using websocket_tls_layer = beast::websocket::stream<tls_layer>;

BOOST_AUTO_TEST_CASE(async_traits) {
	BOOST_STATIC_ASSERT(!detail::has_next_layer<tcp_layer>);
	BOOST_STATIC_ASSERT(detail::has_next_layer<tls_layer>);
	BOOST_STATIC_ASSERT(detail::has_next_layer<websocket_tcp_layer>);
	BOOST_STATIC_ASSERT(detail::has_next_layer<websocket_tls_layer>);

	BOOST_STATIC_ASSERT(!detail::has_tls_layer<tcp_layer>);
	BOOST_STATIC_ASSERT(detail::has_tls_layer<tls_layer>);
	BOOST_STATIC_ASSERT(!detail::has_tls_layer<websocket_tcp_layer>);
	BOOST_STATIC_ASSERT(detail::has_tls_layer<websocket_tls_layer>);

	BOOST_STATIC_ASSERT(!detail::has_ws_handshake<tcp_layer>);
	BOOST_STATIC_ASSERT(!detail::has_ws_handshake<tls_layer>);
	BOOST_STATIC_ASSERT(detail::has_ws_handshake<websocket_tcp_layer>);
	BOOST_STATIC_ASSERT(detail::has_ws_handshake<websocket_tls_layer>);

	BOOST_STATIC_ASSERT(!detail::has_tls_handshake<tcp_layer>);
	BOOST_STATIC_ASSERT(detail::has_tls_handshake<tls_layer>);
	BOOST_STATIC_ASSERT(!detail::has_tls_handshake<websocket_tcp_layer>);
	BOOST_STATIC_ASSERT(!detail::has_tls_handshake<websocket_tls_layer>);
}

BOOST_AUTO_TEST_CASE(client_functions) {
	asio::io_context ioc;

	mqtt_client<tcp_layer> tcp_client(ioc, "");
	tcp_client.authenticator(good_authenticator());
	auto data = tcp_client.connection_property(prop::authentication_data);

	asio::ssl::context ctx(asio::ssl::context::tls_client);
	mqtt_client<
		tls_layer, asio::ssl::context
	> tls_client(ioc.get_executor(), "", std::move(ctx));
	tls_client.tls_context();
}


BOOST_AUTO_TEST_SUITE_END();
