//
// Copyright (c) 2023-2024 Ivica Siladic, Bruno Iljazovic, Korina Simicevic
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef ASYNC_MQTT5_TEST_AUTOCONNECT_STREAM_HPP
#define ASYNC_MQTT5_TEST_AUTOCONNECT_STREAM_HPP

#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include <boost/asio/steady_timer.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/system/error_code.hpp>

#include <async_mqtt5/detail/async_mutex.hpp>
#include <async_mqtt5/detail/async_traits.hpp>
#include <async_mqtt5/detail/log_invoke.hpp>

#include <async_mqtt5/impl/endpoints.hpp>

namespace async_mqtt5::test {

namespace asio = boost::asio;
using error_code = boost::system::error_code;

template <
	typename StreamType,
	typename StreamContext = std::monostate,
	typename LoggerType = async_mqtt5::noop_logger
>
class test_autoconnect_stream {
public:
	using stream_type = StreamType;
	using stream_ptr = std::shared_ptr<stream_type>;
	using stream_context_type = StreamContext;
	using executor_type = typename stream_type::executor_type;
	using logger_type = LoggerType;
private:
	executor_type _stream_executor;
	detail::async_mutex _conn_mtx;
	asio::steady_timer _connect_timer;
	detail::endpoints<logger_type> _endpoints;

	stream_ptr _stream_ptr;
	stream_context_type& _stream_context;

	detail::log_invoke<logger_type> _log;

	template <typename Stream>
	friend class async_mqtt5::detail::reconnect_op;

public:
	test_autoconnect_stream(
		const executor_type& ex,
		stream_context_type& context,
		detail::log_invoke<logger_type>& log
	) :
		_stream_executor(ex),
		_conn_mtx(_stream_executor),
		_connect_timer(_stream_executor),
		_endpoints(_stream_executor, _connect_timer, log),
		_stream_context(context),
		_log(log)
	{
		replace_next_layer(construct_next_layer());
		open_lowest_layer(_stream_ptr, asio::ip::tcp::v4());
	}

	test_autoconnect_stream(const test_autoconnect_stream&) = delete;
	test_autoconnect_stream& operator=(const test_autoconnect_stream&) = delete;

	stream_ptr stream_pointer() const {
		return _stream_ptr;
	}

	bool is_open() const noexcept {
		return detail::lowest_layer(*_stream_ptr).is_open();
	}

	void brokers(std::string hosts, uint16_t default_port) {
		_endpoints.brokers(std::move(hosts), default_port);
	}

	static void open_lowest_layer(const stream_ptr& sptr, asio::ip::tcp protocol) {
		error_code ec;
		auto& layer = detail::lowest_layer(*sptr);
		layer.open(protocol, ec);
		layer.set_option(asio::socket_base::reuse_address(true), ec);
		layer.set_option(asio::ip::tcp::no_delay(true), ec);
	}

	void close() {
		error_code ec;
		detail::lowest_layer(*_stream_ptr).shutdown(asio::ip::tcp::socket::shutdown_both, ec);
		detail::lowest_layer(*_stream_ptr).close(ec);
	}

	stream_ptr construct_next_layer() const {
		stream_ptr sptr;
		if constexpr (detail::has_tls_context<StreamContext>)
			sptr = std::make_shared<stream_type>(
				_stream_executor, _stream_context.tls_context()
			);
		else
			sptr = std::make_shared<stream_type>(_stream_executor);

		return sptr;
	}

	stream_ptr construct_and_open_next_layer(asio::ip::tcp protocol) const {
		auto sptr = construct_next_layer();
		open_lowest_layer(sptr, protocol);
		return sptr;
	}

	void replace_next_layer(stream_ptr sptr) {
		if (_stream_ptr)
			close();
		std::exchange(_stream_ptr, std::move(sptr));
	}

private:
	detail::log_invoke<logger_type>& log() {
		return _log;
	}
};

} // end namespace async_mqtt5::test

#endif // !ASYNC_MQTT5_TEST_AUTOCONNECT_STREAM_HPP
