#ifndef ASYNC_MQTT5_TYPES_HPP
#define ASYNC_MQTT5_TYPES_HPP

#include <cstdint>
#include <string>

#include <boost/system/error_code.hpp>

#include <async_mqtt5/property_types.hpp>

namespace async_mqtt5 {

/** An alias for `boost::system::error_code`; */
using error_code = boost::system::error_code;

struct authority_path {
	std::string host, port, path;
};

/**
 * \brief Represents the Quality of Service (QoS) property of the PUBLISH packets.
 *
 * \details Determines how the PUBLISH packets are delivered from the sender
 * to the receiver.
 */
enum class qos_e : std::uint8_t {
	/** The message arrives at the receiver either once or not at all. */
	at_most_once = 0b00,

	/** Ensures the message arrives at the receiver at least once. */
	at_least_once = 0b01,

	/** All messages arrive at the receiver exactly once without
	 loss or duplication of the messages. */
	exactly_once = 0b10
};

/**
 * \brief Represents the RETAIN flag in the PUBLISH packets.
 *
 * \details This flag informs the Server about whether or not it should
 * store the current message.
 */
enum class retain_e : std::uint8_t {
	/** The Server will replace any existing retained message for this Topic
	 with this message. */
	yes = 0b1,

	/** The Server will not store this message and will not remove or replace
	 any existing retained message. */
	no = 0b0
};


enum class dup_e : std::uint8_t {
	yes = 0b1, no = 0b0
};


/**
 * \brief Represents the Options associated with each Subscription.
 */
struct subscribe_options {

	/**
	 *
	 * \brief Representation of the No Local Subscribe Option.
	 *
	 * \details A Subscribe Option indicating whether or not Application Messages
	 * will be forwarded to a connection with a ClientID equal to the ClientID of the
	 * publishing connection.
	 */
	enum class no_local_e : std::uint8_t {
		/** Application Messages can be forwarded to a connection with equal ClientID. */
		no = 0b0,

		/** Application Messages MUST NOT be forwarded to a connection with equal ClientID. */
		yes = 0b1
	};

	/**
	 * \brief Representation of the Retain As Published Subscribe Option.
	 *
	 * \details A Subscribe Option indicating whether or not Application Messages forwarded
	 * using this subscription keep the \ref retain_e flag they were published with.
	 */
	enum class retain_as_published_e : std::uint8_t {
		/** Application Messages have the \ref retain_e flag set to 0. */
		dont = 0b0,

		/** Application Messages keep the \ref retain_e flag they were published with. */
		retain = 0b1
	};

	/**
	 * \brief Representation of the Retain Handling Subscribe Option.
	 *
	 * \details A Subscribe Option specifying whether retained messages are sent
	 * when the subscription is established.
	 */
	enum class retain_handling_e : std::uint8_t {
		/** Send retained messages at the time of subscribe. */
		send = 0b00,

		/** Send retained message only if the subscription does not currently exist. */
		new_subscription_only = 0b01,

		/** Do not send retained messages at the time of subscribe. */
		not_send = 0b100
	};


	/// Maximum \ref qos_e level at which the Server can send Application Messages to the Client.
	qos_e max_qos = qos_e::exactly_once;

	/// Option determining if Application Messages will be forwarded to a connection with an equal ClientID.
	no_local_e no_local = no_local_e::yes;

	/// Option determining if Application Message will keep their \ref retain_e flag.
	retain_as_published_e retain_as_published = retain_as_published_e::retain;

	/// Option determining if retained messages are sent when the subscription is established.
	retain_handling_e retain_handling = retain_handling_e::new_subscription_only;
};

/**
 * \brief A representation of a Topic Subscription consisting of a Topic Filter and
 * Subscribe Options.
 */
struct subscribe_topic {
	/// An UTF-8 Encoded String indicating the Topics to which the Client wants to subscribe.
	std::string topic_filter;

	/// The \ref subscribe_options associated with the subscription.
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


/**
 * \brief Represents the Will Message.
 *
 * \details A Will Message is an Application Message that
 * the Broker should publish after the Network Connection is closed
 * in cases where the Network Connection is not closed normally.
 */
class will : public will_props {
	std::string _topic;
	std::string _message;
	qos_e _qos; retain_e _retain;

public:
	/**
	 * \brief Constructs an empty Will Message.
	 */
	will() = default;

	/**
	 * \brief Construct a Will Message.
	 *
	 * \param topic Topic, identification of the information channel to which
	 * the Will Message will be published.
	 * \param message The message that will be published.
	 * \param qos The \ref qos_e level used when publishing the Will Message.
	 * \param retain The \ref retain_e flag specifying if the Will Message
	 * is to be retained when it is published.
	 */
	will(
		std::string topic, std::string message,
		qos_e qos = qos_e::at_most_once, retain_e retain = retain_e::no
	) :
		_topic(std::move(topic)), _message(std::move(message)),
		_qos(qos), _retain(retain)
	{}

	/**
	 * \brief Construct a Will Message.
	 *
	 * \param topic Topic name, identification of the information channel to which
	 * the Will Message will be published.
	 * \param message The message that will be published.
	 * \param qos The \ref qos_e level used when publishing the Will Message.
	 * \param retain The \ref retain_e flag specifying if the Will Message
	 * is to be retained when it is published.
	 * \param props Will properties.
	 */
	will(
		std::string topic, std::string message,
		qos_e qos, retain_e retain, will_props props
	) :
		will_props(std::move(props)),
		_topic(std::move(topic)), _message(std::move(message)),
		_qos(qos), _retain(retain)
	{}

	// just to make sure that we don't accidentally make a copy
	/// Copy constructor.
	will(const will&) = delete;

	/// Move constructor.
	will(will&&) noexcept = default;

	/// Copy assignment operator.
	will& operator=(const will&) = delete;

	/// Move assignment operator.
	will& operator=(will&&) noexcept = default;

	/// Get the Topic Name.
	constexpr std::string_view topic() const {
		return _topic;
	}

	/// Get the Application Message.
	constexpr std::string_view message() const {
		return _message;
	}

	/// Get the \ref qos_e.
	constexpr qos_e qos() const {
		return _qos;
	}

	/// Get the \ref retain_e.
	constexpr retain_e retain() const {
		return _retain;
	}
};


} // end namespace async_mqtt5

#endif // !ASYNC_MQTT5_TYPES_HPP
