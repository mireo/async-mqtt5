//
// Copyright (c) 2023-2024 Ivica Siladic, Bruno Iljazovic, Korina Simicevic
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef ASYNC_MQTT5_LOGGER_HPP
#define ASYNC_MQTT5_LOGGER_HPP

#include <iostream>
#include <string_view>

#include <boost/system/error_code.hpp>
#include <boost/asio/ip/tcp.hpp>

#include <async_mqtt5/reason_codes.hpp>
#include <async_mqtt5/property_types.hpp>
#include <async_mqtt5/types.hpp>

#include <async_mqtt5/impl/codecs/traits.hpp>

namespace async_mqtt5 {

namespace asio = boost::asio;
using error_code = boost::system::error_code;

/**
 * \brief Represents the severity level of log messages.
 */
enum class log_level : uint8_t {
	/** Error messages that indicate serious issues. **/
	error = 1,

	/** Warnings that indicate potential problems or non-critical issues. **/
	warning,

	/** Informational messages that highlight normal application behaviour and events. **/
	info,

	/** Detailed messages useful for diagnosing issues. **/
	debug
};

/**
 * \brief A logger class that can used by the \ref mqtt_client to output
 * the results of operations to stderr.
 *
 * \details All functions are invoked directly within the \ref mqtt_client using
 * its default executor. If the \ref mqtt_client is initialized with an explicit or
 * implicit strand, none of the functions will be invoked concurrently.
 * 
 * \par Thread safety
 * ['Distinct objects]: unsafe. \n
 * ['Shared objects]: unsafe. \n
 * This class is [*not thread-safe].
*/
class logger {
	constexpr static auto prefix = "[Async.MQTT5]";

	log_level _level;
public:

	/**
	 * \brief Constructs a logger that filters log messages based on the specified log level.
	 *
	 * \param level Messages with a log level higher than the given log level will be suppressed.
	 */
	logger(log_level level = log_level::warning) : _level(level) {}

	/**
	 * \brief Outputs the results of the resolve operation.
	 *
	 * \param ec Error code returned by the resolve operation.
	 * \param host Hostname used in the resolve operation.
	 * \param port Port used in the resolve operation.
	 * \param eps Endpoints returned by the resolve operation.
	 */
	void at_resolve(
		error_code ec, std::string_view host, std::string_view port,
		const asio::ip::tcp::resolver::results_type& eps
	) {
		if (!ec && _level < log_level::info)
			return;

		write_prefix();
		std::clog
			<< "resolve: "
			<< host << ":" << port;
		std::clog << " - " << ec.message();

		if (_level == log_level::debug) {
			std::clog << " [";
			for (auto it = eps.begin(); it != eps.end();) {
				std::clog << it->endpoint().address().to_string();
				if (++it != eps.end())
					std::clog << ",";
			}
			std::clog << "]" << std::endl;
		}
	}

	/**
	 * \brief Outputs the results of the TCP connect operation.
	 *
	 * \param ec Error code returned by the TCP connect operation.
	 * \param ep The TCP endpoint used to establish the TCP connection.
	 */
	void at_tcp_connect(error_code ec, asio::ip::tcp::endpoint ep) {
		if (!ec && _level < log_level::info)
			return;

		write_prefix();
		std::clog
			<< "connect: "
			<< ep.address().to_string() << ":" << ep.port()
			<< " - " << ec.message()
		<< std::endl;
	}

	/**
	 * \brief Outputs the results of the TLS handshake operation.
	 *
	 * \param ec Error code returned by the TLS handshake operation.
	 * \param ep The TCP endpoint used to establish the TLS handshake.
	 */
	void at_tls_handshake(error_code ec, asio::ip::tcp::endpoint ep) {
		if (!ec && _level < log_level::info)
			return;

		write_prefix();
		std::clog
			<< "tls handshake: "
			<< ep.address().to_string() << ":" << ep.port()
			<< " - " << ec.message()
		<< std::endl;
	}

	/**
	 * \brief Outputs the results of the WebSocket handshake operation.
	 *
	 * \param ec Error code returned by the WebSocket handshake operation.
	 * \param ep The TCP endpoint used to establish the WebSocket handshake.
	 */
	void at_ws_handshake(error_code ec, asio::ip::tcp::endpoint ep) {
		if (!ec && _level < log_level::info)
			return;

		write_prefix();
		std::clog
			<< "ws handshake: "
			<< ep.address().to_string() << ":" << ep.port()
			<< " - " << ec.message()
		<< std::endl;
	}

	/** 
	 * \brief Outputs the contents of the \__CONNACK\__ packet sent by the Broker.
	 * 
	 * \param rc Reason Code in the received \__CONNACK\__ packet indicating
	 * the result of the MQTT handshake.
	 * \param session_present A flag indicating whether the Broker already has a session associated
	 * with this connection.
	 * \param ca_props \__CONNACK_PROPS\__ received in the \__CONNACK\__ packet.
	 */
	void at_connack(
		reason_code rc,
		bool /* session_present */, const connack_props& /* ca_props */
	) {
		if (!rc && _level < log_level::info)
			return;

		write_prefix();
		std::clog << "connack: " << rc.message() << ".";
		std::clog << std::endl;
	}

	/**
	 * \brief Outputs the contents of the \__DISCONNECT\__ packet sent by the Broker.
	 *
	 * \param rc Reason Code in the received \__DISCONNECT\__ packet indicating
	 * the reason behind the disconnection.
	 * \param dc_props \__DISCONNECT_PROPS\__ received in the \__DISCONNECT\__ packet.
	 */
	void at_disconnect(reason_code rc, const disconnect_props& dc_props) {
		write_prefix();
		std::clog << "disconnect: " << rc.message() << ".";
		if (dc_props[prop::reason_string].has_value())
			std::clog << " Reason string: " << * dc_props[prop::reason_string];
		std::clog << std::endl;
	}

private:
	void write_prefix() {
		std::clog << prefix << " ";
	}
};

} // end namespace async_mqtt5


#endif // !ASYNC_MQTT5_LOGGER_HPP
