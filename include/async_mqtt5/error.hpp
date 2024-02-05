#ifndef ASYNC_MQTT5_ERROR_HPP
#define ASYNC_MQTT5_ERROR_HPP

#include <boost/asio/error.hpp>

namespace async_mqtt5 {

/**
 * \brief A representation of Disconnect Reason Code.
 *
 * \details Represents all Reason Codes that the Client can send to the Server
 * in the \__DISCONNECT\__ packet as the reason for the disconnection.
 */
enum class disconnect_rc_e : uint8_t {
	/** Close the connection normally. Do not send the Will Message. */
	normal_disconnection = 0x00,

	/** The Client wishes to disconnect but requires that
	 the Server also publishes its Will Message. */
	disconnect_with_will_message = 0x04
};

namespace detail {

enum class disconnect_rc_e : uint8_t {
	normal_disconnection = 0x00,
	disconnect_with_will_message = 0x04,

	unspecified_error = 0x80,
	malformed_packet = 0x81,
	protocol_error = 0x82,
	implementation_specific_error = 0x83,
	topic_name_invalid = 0x90,
	receive_maximum_exceeded = 0x93,
	topic_alias_invalid = 0x94,
	packet_too_large = 0x95,
	message_rate_too_high = 0x96,
	quota_exceeded = 0x97,
	administrative_action = 0x98,
	payload_format_invalid = 0x99
};

}


namespace client {
/**
 * \brief Defines error codes related to MQTT client.
 *
 * \details Encapsulates errors that occur on the client side.
 */
enum class error : int {
	/** The packet is malformed */
	malformed_packet = 100,

	/** The packet has exceeded the Maximum Packet Size the Server is willing to accept */
	packet_too_large,

	/** The Client's session does not exist or it has expired */
	session_expired,

	/** There are no more available Packet Identifiers to use */
	pid_overrun,

	/** The Topic is invalid and does not conform to the specification */
	invalid_topic,

	// publish
	/** The Server does not support the specified \ref qos_e */
	qos_not_supported,

	/** The Server does not support retained messages */
	retain_not_available,

	/** The Client attempted to send a Topic Alias that is greater than Topic Alias Maximum */
	topic_alias_maximum_reached,

	// subscribe
	/** The Server does not support Wildcard Subscriptions */
	wildcard_subscription_not_available,

	/** The Server does not support this Subscription Identifier */
	subscription_identifier_not_available,

	/** The Server does not support Shared Subscriptions */
	shared_subscription_not_available
};


inline std::string client_error_to_string(error err) {
	switch (err) {
		case error::malformed_packet:
			return "The packet is malformed";
		case error::packet_too_large:
			return "The packet has exceeded the Maximum Packet Size "
				"the Server is willing to accept";
		case error::session_expired:
			return "The Client's session does not exist or it has expired";
		case error::pid_overrun:
			return "There are no more available Packet Identifiers to use";
		case error::invalid_topic:
			return "The Topic is invalid and "
				"does not conform to the specification";
		case error::qos_not_supported:
			return "The Server does not support the specified QoS";
		case error::retain_not_available:
			return "The Server does not support retained messages";
		case error::topic_alias_maximum_reached:
			return "The Client attempted to send a Topic Alias "
				"that is greater than Topic Alias Maximum";
		case error::wildcard_subscription_not_available:
			return "The Server does not support Wildcard Subscriptions";
		case error::subscription_identifier_not_available:
			return "The Server does not support this Subscription Identifier";
		case error::shared_subscription_not_available:
			return "The Server does not support Shared Subscriptions";
		default:
			return "Unknown client error";
	}
}

struct client_ec_category : public boost::system::error_category {
	const char* name() const noexcept override { return "mqtt_client_error"; }
	std::string message(int ev) const noexcept override {
		return client_error_to_string(static_cast<error>(ev));
	}
};

/// Returns the error category associated with \ref client::error.
inline const client_ec_category& get_error_code_category() {
	static client_ec_category cat;
	return cat;
}

/// Creates an \ref error_code from a \ref client::error.
inline boost::system::error_code make_error_code(error r) {
	return { static_cast<int>(r), get_error_code_category() };
}

inline std::ostream& operator<<(std::ostream& os, const error& err) {
	os << get_error_code_category().name() << ":" << static_cast<int>(err);
	return os;
}

} // end namespace client


namespace connection {

/**
* \brief Defines error codes related to MQTT client connection.
*
* \details Encapsulates errors encountered during the process of establishing a connection.
*/
enum class error : int {
	/** Connection has been successfully established */
	success = 0,

	/** An error occured during TLS handshake */
	tls_handshake_error = 1,

	/** An error occured during WebSocket handshake */
	websocket_handshake_error = 2,

	// CONNACK
	/** The Server does not wish to reveal the reason for the failure */
	unspecified_error = 128,

	/** Data within the CONNECT packet could not be correctly parsed */
	malformed_packet = 129,

	/** Data in the CONNECT packet does not conform to this specification */
	protocol_error = 130,

	/** The CONNECT is valid but is not accepted by this Server */
	implementation_specific_error = 131,

	/** The Server does not support the version of the MQTT protocol requested by the Client */
	unsupported_protocol_version = 132,

	/** The Client Identifier is a valid string but is not allowed by the Server */
	client_identifier_not_valid = 133,

	/** The Server does not accept the User Name or Password specified by the Client */
	bad_username_or_password = 134,

	/** The Client is not authorized to connect */
	not_authorized = 135,

	/** The MQTT Server is not available */
	server_unavailable = 136,

	/** The Server is busy, try again later */
	server_busy = 137,

	/** This Client has been banned by administrative action */
	banned = 138,

	/** The authentication method is not supported or does not match the one currently in use. */
	bad_authentication_method = 140,

	/** The Will Topic Name is not malformed, but is not accepted by this Server */
	topic_name_invalid = 144,

	/** The CONNECT packet exceeded the maximum permissible size */
	packet_too_large = 149,

	/** An implementation or administrative imposed limit has been exceeded */
	quota_exceeded = 151,

	/** The Will Payload does not match the specified Payload Format Indicator */
	payload_format_invalid = 153,

	/** The Server does not support retained messages, and Will Retain was set to 1 */
	retain_not_supported = 154,

	/** The Server does not support the QoS set in Will QoS */
	qos_not_supported = 155,

	/** The Client should temporarily use another server */
	use_another_server = 156,

	/** The Client should permanently use another server */
	server_moved = 157,

	/** The connection rate limit has been exceeded */
	connection_rate_exceeded = 159
};

inline std::string connection_error_to_string(error err) {
	switch (err) {
		case error::success:
			return "Connection has been successfully established";
		case error::tls_handshake_error:
			return "Connection failed: An error occured during TLS handshake";
		case error::websocket_handshake_error:
			return "Connection failed: An error occured during WebSocket handshake";
		case error::unspecified_error:
			return "Connection failed: The Server does not wish to reveal "
				"the reason for the failure or none of the codes apply";
		case error::malformed_packet:
			return "Connection failed: Data within the CONNECT packet "
				"could not be correctly parsed";
		case error::protocol_error:
			return "Connection failed: Data in the CONNECT packet does"
				" not conform to this specification";
		case error::implementation_specific_error:
			return "Connection failed : The CONNECT is valid but "
				"is not accepted by this Server";
		case error::unsupported_protocol_version:
			return "Connection failed: The Server does not support the "
				"version of the MQTT protocol requested by the Client";
		case error::client_identifier_not_valid:
			return "Connection failed: The Client Identifier is a valid "
				"string but is not allowed by the Server";
		case error::bad_username_or_password:
			return "Connection failed: The Server does not accept the User Name "
				" or Password specified by the Client";
		case error::not_authorized:
			return "Connection failed: The Client is not authorized to connect";
		case error::server_unavailable:
			return "Connection failed: The MQTT Server is not available";
		case error::server_busy:
			return "Connection failed: The Server is busy, try again later";
		case error::banned:
			return "Connection failed: This Client has been banned "
				"by administrative action, contact the server administrator";
		case error::bad_authentication_method:
			return "Connection failed: The authentication method is not supported "
				"or does not match the authentication method currently in use";
		case error::topic_name_invalid:
			return "Connection failed: The Will Topic Name is not malformed, "
				"but is not accepted by this Server";
		case error::packet_too_large:
			return "Connection failed: The CONNECT packet exceeded the maximum "
				" permissible size";
		case error::quota_exceeded:
			return "Connection failed: An implementation or administrative "
				"imposed limit has been exceeded";
		case error::payload_format_invalid:
			return "Connection failed: The Will Payload does not match "
				"the specified Payload Format Indicator";
		case error::retain_not_supported:
			return "Connection failed: The Server does not support "
				"retained messages, and Will Retain was set to 1";
		case error::qos_not_supported:
			return "Connection failed: The Server does not support "
				"the QoS set in Will QoS";
		case error::use_another_server:
			return "Connection failed: The Client should temporarily "
				"use another server";
		case error::server_moved:
			return "Connection failed: The Client should permanently "
				"use another server";
		case error::connection_rate_exceeded:
			return "Connection failed: The connection rate limit "
				"has been exceeded";
		default:
			return "Unknown connection error";
	}
}

struct connection_ec_category : public boost::system::error_category {
	const char* name() const noexcept override { return "mqtt_connection_error"; }
	std::string message(int ev) const noexcept override {
		return connection_error_to_string(static_cast<error>(ev));
	}
};

/// Returns the error category associated with \ref connection::error.
inline const connection_ec_category& get_error_code_category() {
	static connection_ec_category cat;
	return cat;
}

/// Creates an \ref error_code from a \ref connection::error.
inline boost::system::error_code make_error_code(error r) {
	return { static_cast<int>(r), get_error_code_category() };
}

inline std::ostream& operator<<(std::ostream& os, const error& err) {
	os << get_error_code_category().name() << ":" << static_cast<int>(err);
	return os;
}

} // end namespace connection

} // end namespace async_mqtt5

namespace boost::system {

template <>
struct is_error_code_enum <async_mqtt5::client::error> : std::true_type {};

template <>
struct is_error_code_enum <async_mqtt5::connection::error> : std::true_type {};

} // end namespace boost::system

namespace async_mqtt5 {

inline bool is_not_recoverable(boost::system::error_code ec) {
	using namespace connection;
	return ec == boost::asio::error::no_recovery ||
		ec == error::tls_handshake_error ||
		ec == error::websocket_handshake_error ||
		ec == error::malformed_packet ||
		ec == error::implementation_specific_error ||
		ec == error::unsupported_protocol_version ||
		ec == error::client_identifier_not_valid ||
		ec == error::bad_username_or_password ||
		ec == error::not_authorized ||
		ec == error::banned ||
		ec == error::bad_authentication_method ||
		ec == error::topic_name_invalid ||
		ec == error::packet_too_large ||
		ec == error::quota_exceeded ||
		ec == error::payload_format_invalid ||
		ec == error::retain_not_supported ||
		ec == error::qos_not_supported ||
		ec == error::use_another_server ||
		ec == error::server_moved;
}

} // end namespace async_mqtt5

#endif // !ASYNC_MQTT5_ERROR_HPP
