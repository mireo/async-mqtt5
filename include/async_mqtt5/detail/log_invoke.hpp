//
// Copyright (c) 2023-2024 Ivica Siladic, Bruno Iljazovic, Korina Simicevic
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef ASYNC_MQTT5_LOG_INVOKE_HPP
#define ASYNC_MQTT5_LOG_INVOKE_HPP

#include <string_view>
#include <type_traits>

#include <boost/asio/ip/tcp.hpp>
#include <boost/system/error_code.hpp>
#include <boost/type_traits/is_detected.hpp>

#include <async_mqtt5/reason_codes.hpp>
#include <async_mqtt5/property_types.hpp>
#include <async_mqtt5/types.hpp>

namespace async_mqtt5::detail {

namespace asio = boost::asio;
using boost::system::error_code;

// NOOP Logger
class noop_logger {};

// at_resolve

template <typename T>
using at_resolve_sig = decltype(
	std::declval<T&>().at_resolve(
		std::declval<error_code>(),
		std::declval<std::string_view>(), std::declval<std::string_view>(),
		std::declval<const asio::ip::tcp::resolver::results_type&>()
	)
);
template <typename T>
constexpr bool has_at_resolve = boost::is_detected<at_resolve_sig, T>::value;

// at_tcp_connect

template <typename T>
using at_tcp_connect_sig = decltype(
	std::declval<T&>().at_tcp_connect(
		std::declval<error_code>(), std::declval<asio::ip::tcp::endpoint>()
	)
);
template <typename T>
constexpr bool has_at_tcp_connect = boost::is_detected<at_tcp_connect_sig, T>::value;

// at_tls_handshake

template <typename T>
using at_tls_handshake_sig = decltype(
	std::declval<T&>().at_tls_handshake(
		std::declval<error_code>(), std::declval<asio::ip::tcp::endpoint>()
	)
);
template <typename T>
constexpr bool has_at_tls_handshake = boost::is_detected<at_tls_handshake_sig, T>::value;

// at_ws_handshake

template <typename T>
using at_ws_handshake_sig = decltype(
	std::declval<T&>().at_ws_handshake(
		std::declval<error_code>(), std::declval<asio::ip::tcp::endpoint>()
	)
);
template <typename T>
constexpr bool has_at_ws_handshake = boost::is_detected<at_ws_handshake_sig, T>::value;

// at_connack

template <typename T>
using at_connack_sig = decltype(
	std::declval<T&>().at_connack(
		std::declval<reason_code>(),
		std::declval<bool>(), std::declval<const connack_props&>()
	)
);
template <typename T>
constexpr bool has_at_connack = boost::is_detected<at_connack_sig, T>::value;

// at_disconnect
template <typename T>
using at_disconnect_sig = decltype(
	std::declval<T&>().at_disconnect(
		std::declval<reason_code>(), std::declval<const disconnect_props&>()
	)
);
template <typename T>
constexpr bool has_at_disconnect = boost::is_detected<at_disconnect_sig, T>::value;

template <typename LoggerType = noop_logger>
class log_invoke {
	LoggerType _logger;
public:
	explicit log_invoke(LoggerType&& logger = {}) :
		_logger(std::forward<LoggerType>(logger))
	{}

	void at_resolve(
		error_code ec, std::string_view host, std::string_view port,
		const asio::ip::tcp::resolver::results_type& eps
	) {
		if constexpr (has_at_resolve<LoggerType>)
			_logger.at_resolve(ec, host, port, eps);
	}

	void at_tcp_connect(error_code ec, asio::ip::tcp::endpoint ep) {
		if constexpr (has_at_tcp_connect<LoggerType>)
			_logger.at_tcp_connect(ec, ep);
	}

	void at_tls_handshake(error_code ec, asio::ip::tcp::endpoint ep) {
		if constexpr (has_at_tls_handshake<LoggerType>)
			_logger.at_tls_handshake(ec, ep);
	}

	void at_ws_handshake(error_code ec, asio::ip::tcp::endpoint ep) {
		if constexpr (has_at_ws_handshake<LoggerType>)
			_logger.at_ws_handshake(ec, ep);
	}

	void at_connack(
		reason_code rc,
		bool session_present, const connack_props& ca_props
	) {
		if constexpr (has_at_connack<LoggerType>)
			_logger.at_connack(rc, session_present, ca_props);
	}

	void at_disconnect(reason_code rc, const disconnect_props& dc_props) {
		if constexpr (has_at_disconnect<LoggerType>)
			_logger.at_disconnect(rc, dc_props);
	}

};

} // end namespace async_mqtt5::detail


#endif // !ASYNC_MQTT5_LOG_INVOKE_HPP
