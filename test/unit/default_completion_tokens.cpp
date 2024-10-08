//
// Copyright (c) 2023-2024 Ivica Siladic, Bruno Iljazovic, Korina Simicevic
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/asio/awaitable.hpp>
#include <boost/asio/use_awaitable.hpp>

#ifdef BOOST_ASIO_HAS_CO_AWAIT

#include <cstdint>
#include <string>
#include <variant> // std::monostate
#include <vector>

#include <boost/asio/io_context.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio/ip/tcp.hpp>

#include <boost/beast/websocket/stream.hpp>
#include <boost/beast/websocket/ssl.hpp> // async_teardown for asio::ssl::socket

#include <async_mqtt5.hpp>

namespace async_mqtt5 {

namespace asio = boost::asio;

template <typename StreamBase>
struct tls_handshake_type<asio::ssl::stream<StreamBase>> {
	static constexpr auto client = asio::ssl::stream_base::client;
	static constexpr auto server = asio::ssl::stream_base::server;
};

template <typename StreamBase>
void assign_tls_sni(
	const authority_path& /* ap */,
	asio::ssl::context& /* ctx */,
	asio::ssl::stream<StreamBase>& /* stream */
) {}

namespace test {

// the following code needs to compile

template <typename StreamType, typename TlsContextType = std::monostate>
asio::awaitable<void> test_default_completion_tokens_impl(
	TlsContextType tls_context = {}
) {
	asio::io_context ioc;

	using client_type = asio::use_awaitable_t<>::as_default_on_t<
		mqtt_client<StreamType, TlsContextType>
	>;
	client_type c(ioc, std::move(tls_context));

	co_await c.async_run();
	
	auto pub_props = publish_props {};
	co_await c.template async_publish<qos_e::at_least_once>(
		"topic", "payload", retain_e::no, pub_props
	);

	auto sub_topic = subscribe_topic {};
	auto sub_topics = std::vector<subscribe_topic> { sub_topic };
	auto sub_props = subscribe_props {};
	co_await c.async_subscribe(sub_topics, sub_props);
	co_await c.async_subscribe(sub_topic, sub_props);

	auto unsub_topics = std::vector<std::string> {};
	auto unsub_props = unsubscribe_props {};
	co_await c.async_unsubscribe(unsub_topics, unsub_props);
	co_await c.async_unsubscribe("topic", unsub_props);

	co_await c.async_receive();

	auto dc_props = disconnect_props {};
	co_await c.async_disconnect();
	co_await c.async_disconnect(disconnect_rc_e::normal_disconnection, dc_props);
}

asio::awaitable<void> test_default_completion_tokens() {
	co_await test_default_completion_tokens_impl<asio::ip::tcp::socket>();

	co_await test_default_completion_tokens_impl<
		asio::ssl::stream<asio::ip::tcp::socket>,
		asio::ssl::context
	>(asio::ssl::context(asio::ssl::context::tls_client));

	co_await test_default_completion_tokens_impl<
		boost::beast::websocket::stream<asio::ip::tcp::socket>
	>();

	co_await test_default_completion_tokens_impl<
		boost::beast::websocket::stream<asio::ssl::stream<asio::ip::tcp::socket>>,
		asio::ssl::context
	>(asio::ssl::context(asio::ssl::context::tls_client));
}

} // end namespace test

} // end namespace async_mqtt5

#endif // BOOST_ASIO_HAS_CO_AWAIT
