#include <boost/test/unit_test.hpp>

#include <string>
#include <vector>

#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/asio/io_context.hpp>

#include <boost/beast/websocket/stream.hpp>

#include <boost/type_traits/remove_cv_ref.hpp>

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

	void async_auth(std::string /* data */) {}

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

	connack_props ca_props;
	ca_props.visit([&tcp_client](const auto& p, auto&) -> bool {
		using ptype = boost::remove_cv_ref_t<decltype(p)>;
		prop::value_type_t<ptype::value> value = tcp_client.connack_property(p);
		return true;
	});

	connack_props ret_ca_props = tcp_client.connack_properties();

	connect_props co_props;
	co_props[prop::maximum_packet_size] = 1234;
	tcp_client.connect_properties(std::move(co_props));

	tcp_client.connect_property(prop::session_expiry_interval, 40);
	tcp_client.connect_property(prop::receive_maximum, int16_t(10123));
	tcp_client.connect_property(prop::maximum_packet_size, 103);
	tcp_client.connect_property(prop::topic_alias_maximum, uint16_t(12345));
	tcp_client.connect_property(prop::request_response_information, uint8_t(1));
	tcp_client.connect_property(prop::request_problem_information, uint8_t(0));
	tcp_client.connect_property(prop::user_property, std::vector<std::string> { "prop", "prop" });
	tcp_client.connect_property(prop::authentication_method, "method");
	tcp_client.connect_property(prop::authentication_data, "data");

	asio::ssl::context ctx(asio::ssl::context::tls_client);
	mqtt_client<
		tls_layer, asio::ssl::context
	> tls_client(ioc.get_executor(), "", std::move(ctx));
	tls_client.tls_context();
}


BOOST_AUTO_TEST_SUITE_END();
