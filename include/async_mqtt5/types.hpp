#ifndef ASYNC_MQTT5_TYPES_HPP
#define ASYNC_MQTT5_TYPES_HPP

#include <cstdint>
#include <string>
#include <boost/system/error_code.hpp>

#include <async_mqtt5/property_types.hpp>
#include <async_mqtt5/error.hpp>


namespace async_mqtt5 {

using error_code = boost::system::error_code;

struct authority_path {
	std::string host, port, path;
};

enum class qos_e : std::uint8_t {
	at_most_once = 0b00,
	at_least_once = 0b01,
	exactly_once = 0b10
};

enum class retain_e : std::uint8_t {
	yes = 0b1, no = 0b0,
};

enum class dup_e : std::uint8_t {
	yes = 0b1, no = 0b0,
};


struct subscribe_options {
	enum class no_local_e : std::uint8_t {
		no = 0b0,
		yes = 0b1,
	};

	enum class retain_as_published_e : std::uint8_t {
		dont = 0b0,
		retain = 0b1,
	};

	enum class retain_handling_e : std::uint8_t {
		send = 0b00,
		new_subscription_only = 0b01,
		not_send = 0b10,
	};

	qos_e max_qos = qos_e::exactly_once;
	no_local_e no_local = no_local_e::yes;
	retain_as_published_e retain_as_published = retain_as_published_e::retain;
	retain_handling_e retain_handling = retain_handling_e::new_subscription_only;
};

struct subscribe_topic {
	std::string topic_filter;
	subscribe_options sub_opts;
};

/*

reason codes:

	0x00 success
	0x01 success_qos_1
	0x02 success_qos_2
	0x04 disconnect_with_will_message
	0x10 no_matching_subscribers
	0x11 no_subscription_existed
	0x18 continue_authentication
	0x19 re_authenticate
	0x80 failure
	0x81 malformed_packet
	0x82 protocol_error
	0x83 implementation_specific_error
	0x84 unsupported_protocol_version
	0x85 client_identifier_not_valid
	0x86 bad_user_name_or_password
	0x87 not_authorized
	0x88 server_unavailable
	0x89 server_busy
	0x8a banned
	0x8b server_shutting_down
	0x8c bad_authentication_method
	0x8d keep_alive_timeout
	0x8e session_taken_over
	0x8f topic_filter_invalid
	0x90 topic_name_invalid
	0x91 packet_identifier_in_use
	0x92 packet_identifier_not_found
	0x93 receive_maximum_exceeded
	0x94 topic_alias_invalid
	0x95 packet_too_large
	0x95 packet_too_large
	0x96 message_rate_too_high
	0x97 quota_exceeded
	0x98 administrative_action
	0x99 payload_format_invalid
	0x9a retain_not_supported
	0x9b qos_not_supported
	0x9c use_another_server
	0x9d server_moved
	0x9e shared_subscriptions_not_supported
	0x9f connection_rate_exceeded
	0xa0 maximum_connect_time
	0xa1 subscription_identifiers_not_supported
	0xa2 wildcard_subscriptions_not_supported

*/

class connect_props : public prop::properties<
	prop::session_expiry_interval,
	prop::receive_maximum,
	prop::maximum_packet_size,
	prop::topic_alias_maximum,
	prop::request_response_information,
	prop::request_problem_information,
	prop::user_property,
	prop::authentication_method,
	prop::authentication_data
> {};

class connack_props : public prop::properties<
	prop::session_expiry_interval,
	prop::receive_maximum,
	prop::maximum_qos,
	prop::retain_available,
	prop::maximum_packet_size,
	prop::assigned_client_identifier,
	prop::topic_alias_maximum,
	prop::reason_string,
	prop::user_property,
	prop::wildcard_subscription_available,
	prop::subscription_identifier_available,
	prop::shared_subscription_available,
	prop::server_keep_alive,
	prop::response_information,
	prop::server_reference,
	prop::authentication_method,
	prop::authentication_data
> {};

class publish_props : public prop::properties<
	prop::payload_format_indicator,
	prop::message_expiry_interval,
	prop::content_type,
	prop::response_topic,
	prop::correlation_data,
	prop::subscription_identifier,
	prop::topic_alias,
	prop::user_property
> {};

// puback, pubcomp
class puback_props : public prop::properties<
	prop::reason_string,
	prop::user_property
> {};

class pubcomp_props : public prop::properties<
	prop::reason_string,
	prop::user_property
> {};

class pubrec_props : public prop::properties<
	prop::reason_string,
	prop::user_property
> {};

class pubrel_props : public prop::properties<
	prop::reason_string,
	prop::user_property
> {};


class subscribe_props : public prop::properties<
	prop::subscription_identifier,
	prop::user_property
> {};

class suback_props : public prop::properties<
	prop::reason_string,
	prop::user_property
> {};

class unsubscribe_props : public prop::properties<
	prop::subscription_identifier
> {};

class unsuback_props : public prop::properties<
	prop::reason_string,
	prop::user_property
> {};

class disconnect_props : public prop::properties<
	prop::session_expiry_interval,
	prop::reason_string,
	prop::user_property,
	prop::server_reference
> {};

class auth_props : public prop::properties<
	prop::authentication_method,
	prop::authentication_data,
	prop::reason_string,
	prop::user_property
> {};


class will_props : public prop::properties<
	prop::will_delay_interval,
	prop::payload_format_indicator,
	prop::message_expiry_interval,
	prop::content_type,
	prop::response_topic,
	prop::correlation_data,
	prop::user_property
>{};

class will : public will_props {
	std::string _topic;
	std::string _message;
	qos_e _qos; retain_e _retain;

public:
	will() = default;

	will(
		std::string topic, std::string message,
		qos_e qos = qos_e::at_most_once, retain_e retain = retain_e::no
	) :
		_topic(std::move(topic)), _message(std::move(message)),
		_qos(qos), _retain(retain)
	{}

	will(
		std::string topic, std::string message,
		qos_e qos, retain_e retain, will_props props
	) :
		will_props(std::move(props)),
		_topic(std::move(topic)), _message(std::move(message)),
		_qos(qos), _retain(retain)
	{}

	// just to make sure that we don't accidentally make a copy
	will(const will&) = delete;
	will(will&&) noexcept = default;

	will& operator=(const will&) = delete;
	will& operator=(will&&) noexcept = default;

	constexpr std::string_view topic() const {
		return _topic;
	}
	constexpr std::string_view message() const {
		return _message;
	}
	constexpr qos_e qos() const {
		return _qos;
	}
	constexpr retain_e retain() const {
		return _retain;
	}
};


} // end namespace async_mqtt5

#endif // !ASYNC_MQTT5_TYPES_HPP
