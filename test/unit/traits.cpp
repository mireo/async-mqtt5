//
// Copyright (c) 2023-2024 Ivica Siladic, Bruno Iljazovic, Korina Simicevic
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//

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

struct good_authenticator {
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

struct bad_authenticator {
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

BOOST_AUTO_TEST_CASE(has_next_layer) {
	BOOST_STATIC_ASSERT(!detail::has_next_layer<tcp_layer>);
	BOOST_STATIC_ASSERT(detail::has_next_layer<tls_layer>);
	BOOST_STATIC_ASSERT(detail::has_next_layer<websocket_tcp_layer>);
	BOOST_STATIC_ASSERT(detail::has_next_layer<websocket_tls_layer>);
}

BOOST_AUTO_TEST_CASE(has_tls_layer) {
	BOOST_STATIC_ASSERT(!detail::has_tls_layer<tcp_layer>);
	BOOST_STATIC_ASSERT(detail::has_tls_layer<tls_layer>);
	BOOST_STATIC_ASSERT(!detail::has_tls_layer<websocket_tcp_layer>);
	BOOST_STATIC_ASSERT(detail::has_tls_layer<websocket_tls_layer>);
}

BOOST_AUTO_TEST_CASE(has_tls_handshake) {
	BOOST_STATIC_ASSERT(!detail::has_tls_handshake<tcp_layer>);
	BOOST_STATIC_ASSERT(detail::has_tls_handshake<tls_layer>);
	BOOST_STATIC_ASSERT(!detail::has_tls_handshake<websocket_tcp_layer>);
	BOOST_STATIC_ASSERT(!detail::has_tls_handshake<websocket_tls_layer>);
}

BOOST_AUTO_TEST_CASE(has_ws_handskae) {
	BOOST_STATIC_ASSERT(!detail::has_ws_handshake<tcp_layer>);
	BOOST_STATIC_ASSERT(!detail::has_ws_handshake<tls_layer>);
	BOOST_STATIC_ASSERT(detail::has_ws_handshake<websocket_tcp_layer>);
	BOOST_STATIC_ASSERT(detail::has_ws_handshake<websocket_tls_layer>);
}

BOOST_AUTO_TEST_SUITE_END();
