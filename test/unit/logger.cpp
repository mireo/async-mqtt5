//
// Copyright (c) 2023-2024 Ivica Siladic, Bruno Iljazovic, Korina Simicevic
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/test/unit_test.hpp>
#include <boost/test/tools/output_test_stream.hpp>

#include <chrono>
#include <iostream>
#include <string>
#include <type_traits>

#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/stream.hpp>

#include <boost/beast/websocket/stream.hpp>
#include <boost/beast/ssl/ssl_stream.hpp> // async_teardown specialization for websocket ssl stream

#include <async_mqtt5/detail/log_invoke.hpp>

#include <async_mqtt5/mqtt_client.hpp>
#include <async_mqtt5/logger.hpp>

#include "test_common/message_exchange.hpp"
#include "test_common/test_service.hpp"
#include "test_common/test_stream.hpp"

using namespace async_mqtt5;
namespace asio = boost::asio;

namespace async_mqtt5 {

template <typename StreamBase>
struct tls_handshake_type<asio::ssl::stream<StreamBase>> {
	static constexpr auto client = asio::ssl::stream_base::client;
	static constexpr auto server = asio::ssl::stream_base::server;
};

template <typename StreamBase>
void assign_tls_sni(
	const authority_path& ap,
	asio::ssl::context& /* ctx */,
	asio::ssl::stream<StreamBase>& stream
) {
	SSL_set_tlsext_host_name(stream.native_handle(), ap.host.c_str());
}

} // end namespace async_mqtt5


void logger_test() {
	BOOST_STATIC_ASSERT(detail::has_at_resolve<logger>);
	BOOST_STATIC_ASSERT(detail::has_at_tcp_connect<logger>);
	BOOST_STATIC_ASSERT(detail::has_at_tls_handshake<logger>);
	BOOST_STATIC_ASSERT(detail::has_at_ws_handshake<logger>);
	BOOST_STATIC_ASSERT(detail::has_at_connack<logger>);
	BOOST_STATIC_ASSERT(detail::has_at_disconnect<logger>);
}

using stream_type = boost::beast::websocket::stream<
	asio::ssl::stream<asio::ip::tcp::socket>
>;
using context_type = asio::ssl::context;
using logger_type = logger;
using client_type = mqtt_client<stream_type, context_type, logger_type>;

BOOST_AUTO_TEST_SUITE(logger_tests)

class clog_redirect {
	std::streambuf* _old_buffer;

public:
	clog_redirect(
		std::streambuf* new_buffer
	) :
		_old_buffer(std::clog.rdbuf(new_buffer))
	{}

	~clog_redirect() {
		std::clog.rdbuf(_old_buffer);
	}

};

bool contains(const std::string& str, const std::string& substr) {
	return str.find(substr) != std::string::npos;
}

BOOST_AUTO_TEST_CASE(successful_connect_debug) {
	boost::test_tools::output_test_stream output;

	{
		clog_redirect guard(output.rdbuf());
		asio::io_context ioc;

		asio::ssl::context tls_context(asio::ssl::context::tls_client);
		client_type c(
			ioc, std::move(tls_context), logger(log_level::debug)
		);

		c.brokers("broker.hivemq.com/mqtt", 8884)
			.async_run(asio::detached);

		c.async_disconnect([](error_code) {});

		ioc.run();
	}

	std::string log = output.rdbuf()->str();
	BOOST_TEST_MESSAGE(log);
	BOOST_TEST_WARN(contains(log, "resolve"));
	BOOST_TEST_WARN(contains(log, "connect"));
	BOOST_TEST_WARN(contains(log, "tls handshake"));
	BOOST_TEST_WARN(contains(log, "ws handshake"));
	BOOST_TEST_WARN(contains(log, "connack"));
}

BOOST_AUTO_TEST_CASE(successful_connect_warning) {
	boost::test_tools::output_test_stream output;

	{
		clog_redirect guard(output.rdbuf());

		asio::io_context ioc;
		asio::ssl::context tls_context(asio::ssl::context::tls_client);
		client_type c(
			ioc, std::move(tls_context), logger(log_level::warning)
		);

		c.brokers("broker.hivemq.com/mqtt", 8884)
			.async_run(asio::detached);

		c.async_disconnect([](error_code) {});

		ioc.run();
	}

	// If connection is successful, nothing should be printed.
	// However if the Broker is down or overloaded, this will cause logs to be printed.
	// We should not fail the test because of it.
	BOOST_TEST_WARN(output.is_empty());
}

BOOST_AUTO_TEST_CASE(disconnect) {
	using test::after;
	using namespace std::chrono_literals;

	boost::test_tools::output_test_stream output;
	{
		clog_redirect guard(output.rdbuf());

		// packets
		auto connect = encoders::encode_connect(
			"", std::nullopt, std::nullopt, 60, false, {}, std::nullopt
		);
		auto connack = encoders::encode_connack(false, uint8_t(0x00), {});

		disconnect_props dc_props;
		dc_props[prop::reason_string] = "No reason.";
		auto disconnect = encoders::encode_disconnect(0x00, dc_props);

		test::msg_exchange broker_side;
		broker_side
			.expect(connect)
				.complete_with(error_code{}, after(0ms))
				.reply_with(connack, after(0ms))
			.send(disconnect, after(50ms))
			.expect(connect);

		asio::io_context ioc;
		auto executor = ioc.get_executor();
		auto& broker = asio::make_service<test::test_broker>(
			ioc, executor, std::move(broker_side)
		);

		mqtt_client<test::test_stream, std::monostate, logger> c(executor);
		c.brokers("127.0.0.1,127.0.0.1") // to avoid reconnect backoff
			.async_run(asio::detached);

		asio::steady_timer timer(c.get_executor());
		timer.expires_after(100ms);
		timer.async_wait([&c](error_code) { c.cancel(); });

		ioc.run();
		BOOST_TEST(broker.received_all_expected());
	}

	std::string log = output.rdbuf()->str();
	BOOST_TEST_MESSAGE(log);
	BOOST_TEST(contains(log, "disconnect"));
}

BOOST_AUTO_TEST_SUITE_END();