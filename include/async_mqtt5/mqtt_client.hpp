#ifndef ASYNC_MQTT5_MQTT_CLIENT_HPP
#define ASYNC_MQTT5_MQTT_CLIENT_HPP

#include <boost/system/error_code.hpp>

#include <async_mqtt5/error.hpp>
#include <async_mqtt5/types.hpp>

#include <async_mqtt5/impl/client_service.hpp>
#include <async_mqtt5/impl/publish_send_op.hpp>
#include <async_mqtt5/impl/read_message_op.hpp>
#include <async_mqtt5/impl/subscribe_op.hpp>
#include <async_mqtt5/impl/unsubscribe_op.hpp>
#include <async_mqtt5/impl/re_auth_op.hpp>

namespace async_mqtt5 {

namespace asio = boost::asio;

/**
 * \brief \__MQTT\__ client used to connect and communicate with a Broker.
 *
 * \tparam \__StreamType\__ Type of the underlying transport protocol used to transfer
 * the stream of bytes between the Client and the Broker. The transport must be
 * ordered and lossless.
 * \tparam \__TlsContext\__ Type of the context object used in TLS/SSL connections.
 */
template <
	typename StreamType,
	typename TlsContext = std::monostate
>
class mqtt_client {
public:
	/// The executor type associated with the client.
	using executor_type = typename StreamType::executor_type;
private:
	using stream_type = StreamType;
	using tls_context_type = TlsContext;

	static constexpr auto read_timeout = std::chrono::seconds(5);

	using client_service_type = detail::client_service<
		stream_type, tls_context_type
	>;
	using clisvc_ptr = std::shared_ptr<client_service_type>;
	clisvc_ptr _svc_ptr;

public:
	/**
	 * \brief Constructs a Client with given parameters.
	 *
	 * \param ex An executor that will be associated with the Client.
	 * \param cnf 
	 * \param tls_context A context object used in TLS/SLL connection.
	 */
	explicit mqtt_client(
		const executor_type& ex,
		const std::string& cnf,
		TlsContext tls_context = {}
	) :
		_svc_ptr(std::make_shared<client_service_type>(
			ex, cnf, std::move(tls_context)
		))
	{}

	/**
	 * \brief Constructs a Client with given parameters.
	 *
	 * \tparam \__ExecutionContext\__ Type of a concrete execution context.
	 * \param context Execution context whose executor will be associated with the Client.
	 * \param cnf 
	 * \param tls_context A context object used in TLS/SLL connection.
	 *
	 * \par Precondition
	 * \code
	 * std::is_convertible_v<ExecutionContext&, asio::execution_context&>
	 * \endcode
	 */
	template <typename ExecutionContext>
	requires (std::is_convertible_v<ExecutionContext&, asio::execution_context&>)
	explicit mqtt_client(
		ExecutionContext& context,
		const std::string& cnf,
		TlsContext tls_context = {}
	) :
		mqtt_client(context.get_executor(), cnf, std::move(tls_context))
	{}

	/**
	 * \brief Move-construct an mqtt_client from another.
	 *
	 * \details Moved-from client can only be destructed
	 */
	mqtt_client(mqtt_client&& other) noexcept = default;


	/**
	 * \brief Move assignment operator.
	 *
	 * \details Cancels this client first. Moved-from client can only be destructed.
	 */
	mqtt_client& operator=(mqtt_client&& other) noexcept {
		cancel();
		_svc_ptr = std::move(other._svc_ptr);
		return *this;
	}

	/**
	 * \brief Destructor.
	 *
	 * \details Automatically calls \ref mqtt_client::cancel.
	 */
	~mqtt_client() {
		if (_svc_ptr) cancel();
	}

	/**
	 * \brief Get the executor associated with the object.
	 */
	executor_type get_executor() const noexcept {
		return _svc_ptr->get_executor();
	}


	/**
	 * \brief Get the context object used in TLS/SSL connection.
	 *
	 * \note This function may only be invoked
	 * when the template parameter \__TlsContext\__ was configured
	 * with non-default type during the creation of a \ref mqtt_client.
	 *
	 * \par Precondition
	 * \code
	 * !std::is_same_v<TlsContext, std::monostate>
	 * \endcode
	 */
	decltype(auto) tls_context()
	requires (!std::is_same_v<TlsContext, std::monostate>) {
		return _svc_ptr->tls_context();
	}

	/**
	 * \brief Start the Client.
	 */
	void run() {
		_svc_ptr->run();
		detail::ping_op { _svc_ptr }
			.perform(read_timeout - std::chrono::seconds(1));
		detail::read_message_op { _svc_ptr }.perform();
		detail::sentry_op { _svc_ptr }.perform();
	}

	/**
	 * \brief Cancel all asynchronous operations. This function has terminal effects.
	 *
	 * \details All outstanding operations will complete
	 * with `boost::asio::error::operation_aborted`.
	 *
	 * \attention This function has terminal effects and will close the Client.
	 * The Client cannot be used before calling \ref mqtt_client::run again.
	 */
	void cancel() {
		get_executor().execute([svc_ptr = _svc_ptr]() {
			svc_ptr->cancel();
		});
	}

	/**
	 * \brief Retrieves the value of a specific property from the last \__CONNACK\__ packet received.
	 *
	 * \details The return type varies according to the property requested.
	 * For all properties, the return type will be `std::optional` of their respective value type.
	 * For `async_mqtt5::prop::user_property`, the return type is `std::vector<std::string>`.
	 *
	 * \param prop The \__CONNACK\__ property value to retrieve.
	 *
	 * \par Example
	 * \code
	 *	std::optional<std::string> auth_method = client.connection_property(async_mqtt5::prop::authentication_method); // ok
	 *	std::optional<std::string> c_type = client.connection_property(async_mqtt5::prop::content_type); // does not compile!
	 * \endcode
	 *
	 * \see See \__CONNACK_PROPS\__ for all eligible properties.
	 */
	template <uint8_t p>
	decltype(auto) connection_property(std::integral_constant<uint8_t, p> prop) {
		return _svc_ptr->connack_prop(prop);
	}

	/**
	 * \brief Assign a \ref will Message.
	 *
	 * \details The \ref will Message that the Broker should publish
	 * after the Network Connection is closed and it is not
	 * closed normally.
	 *
	 * \attention This function takes action when the client is in a non-operational state,
	 * meaning the \ref run function has not been invoked.
	 * Furthermore, you can use this function after the \ref cancel function has been called,
	 * before the \ref run function is invoked again.
	 */
	mqtt_client& will(will will) {
		_svc_ptr->will(std::move(will));
		return *this;
	}

	/**
	 * \brief Assign credentials that will be used to connect to a Broker.
	 *
	 * \details Credentials consist of a unique Client Identifier and, optionally,
	 * a User Name and Password.
	 *
	 * \attention This function takes action when the client is in a non-operational state,
	 * meaning the \ref run function has not been invoked.
	 * Furthermore, you can use this function after the \ref cancel function has been called,
	 * before the \ref run function is invoked again.
	 */
	mqtt_client& credentials(
		std::string client_id,
		std::string username = "", std::string password = ""
	) {
		_svc_ptr->credentials(
			std::move(client_id),
			std::move(username), std::move(password)
		);
		return *this;
	}

	/**
	 * \brief Assign a list of Brokers that the Client will attempt to connect to.
	 *
	 * \details The Client will cycle through the list of hosts,
	 * attempting to establish a connection with each
	 * until it successfully establishes a connection.
	 *
	 * \param hosts List of Broker addresses and ports.
	 * Address and ports are separated with a colon `:` while
	 * pairs of addresses and ports are separated with a comma `,`.
	 * \param default_port The default port to connect to in case the port is not
	 * explicitly specified in the `hosts` list.
	 *
	 * \attention This function takes action when the client is in a non-operational state,
	 * meaning the \ref run function has not been invoked.
	 * Furthermore, you can use this function after the \ref cancel function has been called,
	 * before the \ref run function is invoked again.
	 *
	 * \par Example
	 * Some valid `hosts` string:
	 *
	 * \code
	 *		std::string valid_hosts_1 = "broker1:1883, broker2, broker3:1883";
	 *		std::string valid_hosts_2 = "broker1";
	 * \endcode
	 *
	 */
	mqtt_client& brokers(std::string hosts, uint16_t default_port = 1883) {
		_svc_ptr->brokers(std::move(hosts), default_port);
		return *this;
	}

	/**
	 * \brief Assign an authenticator that the Client will use for
	 * \__ENHANCED_AUTH\__ on every connect to a Broker.
	 * Re-authentication can be initiated by calling \ref re_authenticate.
	 *
	 * \param authenticator Object that will be stored (move-constructed or by reference)
	 * and used for authentication. It needs to satisfy \__is_authenticator\__ concept.
	 *
	 * \attention This function takes action when the client is in a non-operational state,
	 * meaning the \ref run function has not been invoked.
	 * Furthermore, you can use this function after the \ref cancel function has been called,
	 * before the \ref run function is invoked again.
	 *
	 */
	template <detail::is_authenticator Authenticator>
	mqtt_client& authenticator(Authenticator&& authenticator) {
		_svc_ptr->authenticator(std::forward<Authenticator>(authenticator));
		return *this;
	}

	/**
	 * \brief Initiates [mqttlink 3901257 Re-authentication]
	 * using the authenticator given in the \ref authenticator method.
	 *
	 * \note If \ref authenticator was not called, this method does nothing.
	 */
	void re_authenticate() {
		detail::re_auth_op { _svc_ptr }.perform();
	}

	/**
	 * \brief Send a \__PUBLISH\__ packet to Broker to transport an
	 * Application Message.
	 *
	 * \tparam qos_type The \ref qos_e level of assurance for delivery.
	 * \param topic Identification of the information channel to which
	 * Payload data is published.
	 * \param payload The Application Message that is being published.
	 * \param retain The \ref retain_e flag.
	 * \param props An instance of \__PUBLISH_PROPS\__. 
	 * \param token Completion token that will be used to produce a
	 * completion handler. The handler will be invoked when the operation completes.
	 * On immediate completion, invocation of the handler will be performed in a manner
	 * equivalent to using \__POST\__.
	 *
	 * \par Handler signature
	 * The handler signature for this operation depends on the \ref qos_e specified:\n
	 *
	 *	`qos` == `qos_e::at_most_once`:
	 *		\code
	 *			void (
	 *				__ERROR_CODE__	// Result of operation
	 *			)
	 *		\endcode
	 *
	 *	`qos` == `qos_e::at_least_once`:
	 *		\code
	 *			void (
	 *				__ERROR_CODE__,	// Result of operation.
	 *				__REASON_CODE__,	// Reason Code received from Broker.
	 *				__PUBACK_PROPS__	// Properties received in the PUBACK packet.
	 *			)
	 *		\endcode
	 *
	 *	`qos` == `qos_e::exactly_once`:
	 *		\code
	 *			void (
	 *				__ERROR_CODE__,	// Result of operation.
	 *				__REASON_CODE__,	// Reason Code received from Broker.
	 *				__PUBCOMP_PROPS__	// Properties received in the PUBCOMP packet.
	 *			)
	 *		\endcode
	 *
	 *	\par Completion condition
	 *	Depending on the \ref qos_e specified, the asynchronous operation will complete
	 *	when one of the following conditions is true:\n
	 *		- If `qos` == `qos_e::at_most_once` and the Client
	 *		has successfully written the packet to the transport. \n
	 *		- If `qos` == `qos_e::at_least_once` and the packet has
	 *		been sent and acknowledged through the reception of a \__PUBACK\__ packet.
	 *		- If `qos` == `qos_e::exactly_once` and the packet has
	 *		been sent and fully acknowledged through the reception of a \__PUBCOMP\__ packet.
	 *		- An error occurred. This is indicated by an associated \__ERROR_CODE\__ in the handler.\n
	 *
	 *	\par Error codes
	 *	The list of all possible error codes that this operation can finish with:\n
	 *		- `boost::system::errc::errc_t::success` \n
	 *		- `boost::asio::error::operation_aborted` \n
	 *		- `boost::asio::error::no_recovery` \n
	 *		- \link async_mqtt5::client::error::pid_overrun \endlink
	 *		- \link async_mqtt5::client::error::qos_not_supported \endlink
	 *		- \link async_mqtt5::client::error::retain_not_available \endlink
	 *		- \link async_mqtt5::client::error::topic_alias_maximum_reached \endlink
	 *
	 * Refer to the section on \__ERROR_HANDLING\__ to find the underlying causes for each error code.
	 */
	template <qos_e qos_type, typename CompletionToken>
	decltype(auto) async_publish(
		std::string topic, std::string payload,
		retain_e retain, const publish_props& props,
		CompletionToken&& token
	) {
		using Signature = detail::on_publish_signature<qos_type>;

		auto initiate = [] (
			auto handler, std::string topic, std::string payload,
			retain_e retain, const publish_props& props,
			const clisvc_ptr& svc_ptr
		) {
			detail::publish_send_op<
				client_service_type, decltype(handler), qos_type
			> { svc_ptr, std::move(handler) }
				.perform(
					std::move(topic), std::move(payload),
					retain, props
				);
		};

		return asio::async_initiate<CompletionToken, Signature>(
			std::move(initiate), token,
			std::move(topic), std::move(payload), retain, props, _svc_ptr
		);
	}

	/**
	 * \brief Send a \__SUBSCRIBE\__ packet to Broker to create a subscription
	 * to one or more Topics of interest.
	 *
	 * \details After the subscription has been established, the Broker will send
	 * PUBLISH packets to the Client to forward Application Messages that were published
	 * to Topics that the Client subscribed to. The Application Messages can be received
	 * with \ref mqtt_client::async_receive function.
	 *
	 * \param topics A list of \ref subscribe_topic of interest.
	 * \param props An instance of \__SUBSCRIBE_PROPS\__.
	 * \param token Completion token that will be used to produce a
	 * completion handler. The handler will be invoked when the operation completes.
	 * On immediate completion, invocation of the handler will be performed in a manner
	 * equivalent to using \__POST\__.
	 *
	 * \par Handler signature
	 * The handler signature for this operation:
	 *	\code
	 *		void (
	 *			__ERROR_CODE__,	// Result of operation.
	 *			std::vector<__REASON_CODE__>,	// Vector of Reason Codes indicating
	 *													// the subscription result for each Topic
	 *													// in the SUBSCRIBE packet.
	 *			__SUBACK_PROPS__,	// Properties received in the SUBACK packet.
	 *		)
	 *	\endcode
	 *
	 *	\par Completion condition
	 *	The asynchronous operation will complete when one of the following conditions is true:\n
	 *		- The Client has successfully sent a \__SUBSCRIBE\__ packet
	 *		and has received a \__SUBACK\__ response from the Broker.\n
	 *		- An error occurred. This is indicated by an associated \__ERROR_CODE\__ in the handler.\n
	 *
	 *	\par Error codes
	 *	The list of all possible error codes that this operation can finish with:\n
	 *		- `boost::system::errc::errc_t::success` \n
	 *		- `boost::asio::error::no_recovery` \n
	 *		- `boost::asio::error::operation_aborted` \n
	 *		- \link async_mqtt5::client::error::pid_overrun \endlink
	 *
	 * Refer to the section on \__ERROR_HANDLING\__ to find the underlying causes for each error code.
	 */
	template <typename CompletionToken>
	decltype(auto) async_subscribe(
		const std::vector<subscribe_topic>& topics,
		const subscribe_props& props,
		CompletionToken&& token
	) {
		using Signature = void (
			error_code, std::vector<reason_code>, suback_props
		);

		auto initiate = [] (
			auto handler, const std::vector<subscribe_topic>& topics,
			const subscribe_props& props, const clisvc_ptr& impl
		) {
			detail::subscribe_op { impl, std::move(handler) }
				.perform(topics, props);
		};

		return asio::async_initiate<CompletionToken, Signature>(
			std::move(initiate), token, topics, props, _svc_ptr
		);
	}

	/**
	 * \brief Send a \__SUBSCRIBE\__ packet to Broker to create a subscription
	 * to one Topics of interest.
	 *
	 * \details After the subscription has been established, the Broker will send
	 * \__PUBLISH\__ packets to the Client to forward Application Messages that were published
	 * to Topics that the Client subscribed to. The Application Messages can be received
	 * with \ref mqtt_client::async_receive function.
	 *
	 * \param topic A \ref subscribe_topic of interest.
	 * \param props An instance of \__SUBSCRIBE_PROPS\__.
	 * \param token Completion token that will be used to produce a
	 * completion handler. The handler will be invoked when the operation completes.
	 * On immediate completion, invocation of the handler will be performed in a manner
	 * equivalent to using \__POST\__.
	 *
	 * \par Handler signature
	 * The handler signature for this operation:
	 *	\code
	 *		void (
	 *			__ERROR_CODE__,	// Result of operation.
	 *			std::vector<__REASON_CODE__>,	// Vector of Reason Codes containing the
	 *													// single subscription result for the Topic
	 *													// in the SUBSCRIBE packet.
	 *			__SUBACK_PROPS__,	// Properties received in the SUBACK packet.
	 *		)
	 *	\endcode
	 *
	 *	\par Completion condition
	 *	The asynchronous operation will complete when one of the following conditions is true:\n
	 *		- The Client has successfully sent a \__SUBSCRIBE\__ packet
	 *		and has received a \__SUBACK\__ response from the Broker.\n
	 *		- An error occurred. This is indicated by an associated \__ERROR_CODE\__ in the handler.\n
	 *
	 *	\par Error codes
	 *	The list of all possible error codes that this operation can finish with:\n
	 *		- `boost::system::errc::errc_t::success` \n
	 *		- `boost::asio::error::no_recovery` \n
	 *		- `boost::asio::error::operation_aborted` \n
	 *		- \link async_mqtt5::client::error::pid_overrun \endlink
	 *
	 * Refer to the section on \__ERROR_HANDLING\__ to find the underlying causes for each error code.
	 */
	template <typename CompletionToken>
	decltype(auto) async_subscribe(
		const subscribe_topic& topic, const subscribe_props& props,
		CompletionToken&& token
	) {
		return async_subscribe(
			std::vector<subscribe_topic> { topic }, props,
			std::forward<CompletionToken>(token)
		);
	}


	/**
	 * \brief Send an \__UNSUBSCRIBE\__ packet to Broker to unsubscribe from one
	 * or more Topics.
	 *
	 * \note The Client may still receive residual Application Messages
	 * through the \ref mqtt_client::async_receive function
	 * from Topics the Client just unsubscribed to.
	 *
	 * \param topics List of Topics to unsubscribe from.
	 * \param props An instance of \__UNSUBSCRIBE_PROPS\__.
	 * \param token Completion token that will be used to produce a
	 * completion handler. The handler will be invoked when the operation completes.
	 * On immediate completion, invocation of the handler will be performed in a manner
	 * equivalent to using \__POST\__.
	 *
	 * \par Handler signature
	 * The handler signature for this operation:
	 *	\code
	 *		void (
	 *			__ERROR_CODE__, // Result of operation.
	 *			std::vector<__REASON_CODE__>,	// Vector of Reason Codes indicating
	 *													// the result of unsubscribe operation
	 *													// for each Topic in the UNSUBSCRIBE packet.
	 *			__UNSUBACK_PROPS__, // Properties received in the UNSUBACK packet.
	 *		)
	 *	\endcode
	 *
	 *	\par Completion condition
	 *	The asynchronous operation will complete when one of the following conditions is true:\n
	 *		- The Client has successfully sent an \__UNSUBSCRIBE\__ packet
	 *		and has received an \__UNSUBACK\__ response from the Broker.\n
	 *		- An error occurred. This is indicated by an associated \__ERROR_CODE\__ in the handler.\n
	 *
	 *	\par Error codes
	 *	The list of all possible error codes that this operation can finish with:\n
	 *		- `boost::system::errc::errc_t::success` \n
	 *		- `boost::asio::error::no_recovery` \n
	 *		- `boost::asio::error::operation_aborted` \n
	 *		- \link async_mqtt5::client::error::pid_overrun \endlink
	 *
	 * Refer to the section on \__ERROR_HANDLING\__ to find the underlying causes for each error code.
	 */
	template <typename CompletionToken>
	decltype(auto) async_unsubscribe(
		const std::vector<std::string>& topics,	const unsubscribe_props& props,
		CompletionToken&& token
	) {
		using Signature = void (
			error_code, std::vector<reason_code>, unsuback_props
		);

		auto initiate = [](
			auto handler,
			const std::vector<std::string>& topics,
			const unsubscribe_props& props,	const clisvc_ptr& impl
		) {
			detail::unsubscribe_op { impl, std::move(handler) }
				.perform(topics, props);
		};

		return asio::async_initiate<CompletionToken, Signature>(
			std::move(initiate), token, topics, props, _svc_ptr
		);
	}

	/**
	 * \brief Send an \__UNSUBSCRIBE\__ packet to Broker to unsubscribe
	 * from one Topic.
	 *
	 * \note The Client may still receive residual Application Messages
	 * through the \ref mqtt_client::async_receive function
	 * from Topics the Client just unsubscribed to.
	 *
	 * \param topic Topic to unsubscribe from.
	 * \param props An instance of \__UNSUBSCRIBE_PROPS\__.
	 * \param token Completion token that will be used to produce a
	 * completion handler. The handler will be invoked when the operation completes.
	 * On immediate completion, invocation of the handler will be performed in a manner
	 * equivalent to using \__POST\__.
	 *
	 * \par Handler signature
	 * The handler signature for this operation:
	 *	\code
	 *		void (
	 *			__ERROR_CODE__, // Result of operation.
	 *			std::vector<__REASON_CODE__>,	// Vector of Reason Codes containing
	 *													// the result of unsubscribe operation
	 *													// for the Topic in the UNSUBSCRIBE packet.
	 *			__UNSUBACK_PROPS__, // Properties received in the UNSUBACK packet.
	 *		)
	 *	\endcode
	 *
	 *	\par Completion condition
	 *	The asynchronous operation will complete when one of the following conditions is true:\n
	 *		- The Client has successfully sent an \__UNSUBSCRIBE\__ packet
	 *		and has received an \__UNSUBACK\__ response from the Broker.\n
	 *		- An error occurred. This is indicated by an associated \__ERROR_CODE\__ in the handler.\n
	 *
	 *	\par Error codes
	 *	The list of all possible error codes that this operation can finish with:\n
	 *		- `boost::system::errc::errc_t::success` \n
	 *		- `boost::asio::error::no_recovery` \n
	 *		- `boost::asio::error::operation_aborted` \n
	 *		- \link async_mqtt5::client::error::pid_overrun \endlink
	 *
	 * Refer to the section on \__ERROR_HANDLING\__ to find the underlying causes for each error code.
	 */
	template <typename CompletionToken>
	decltype(auto) async_unsubscribe(
		const std::string& topic, const unsubscribe_props& props,
		CompletionToken&& token
	) {
		return async_unsubscribe(
			std::vector<std::string> { topic }, props,
			std::forward<CompletionToken>(token)
		);
	}


	/**
	 * \brief Asynchronously receive an Application Message.
	 *
	 * \details The Client will receive and complete deliveries for all the
	 * \__PUBLISH\__ packets received from the Broker throughout its lifetime.
	 * The Client will store them internally in order they were delivered.
	 * Calling this function will attempt to receive an Application Message
	 * from internal storage.
	 *
	 * \note It is only recommended to call this function if you have established
	 * a successful subscription to a Topic using the \ref async_subscribe function.
	 *
	 * \param token Completion token that will be used to produce a
	 * completion handler. The handler will be invoked when the operation completes.
	 * On immediate completion, invocation of the handler will be performed in a manner
	 * equivalent to using \__POST\__.
	 *
	 * \par Handler signature
	 * The handler signature for this operation:
	 *	\code
	 *		void (
	 *			__ERROR_CODE__, // Result of operation.
	 *			std::string,	// Topic, the origin of the Application Message.
	 *			std::string,	// Payload, the content of the Application Message.
	 *			__PUBLISH_PROPS__, // Properties received in the PUBLISH packet.
	 *		)
	 *	\endcode
	 *
	 * \par Completion condition
	 *	The asynchronous operation will complete when one of the following conditions is true:\n
	 *		- The Client has a pending Application Message in its internal storage
	 *		ready to be received.
	 *		- An error occurred. This is indicated by an associated \__ERROR_CODE\__ in the handler.\n
	 *
	 *	\par Error codes
	 *	The list of all possible error codes that this operation can finish with:\n
	 *		- `boost::system::errc::errc_t::success`\n
	 *		- `boost::asio::error::operation_aborted`\n
	 *
	 * Refer to the section on \__ERROR_HANDLING\__ to find the underlying causes for each error code.
	 */
	template <typename CompletionToken>
	decltype(auto) async_receive(CompletionToken&& token) {
		// Sig = void (error_code, std::string, std::string, publish_props)
		return _svc_ptr->async_channel_receive(
			std::forward<CompletionToken>(token)
		);
	}

	/**
	 * \brief Disconnect the Client by sending a \__DISCONNECT\__ packet
	 * with a specified Reason Code. This function has terminal effects.
	 *
	 * \details Send a \__DISCONNECT\__ packet to the Broker with a Reason Code
	 * describing the reason for disconnection.
	 *
	 * \attention This function has terminal effects and will close the Client.
	 * See \ref mqtt_client::cancel.
	 *
	 * \note If you wish to close the Client regardless of its state,
	 * prefer calling the \ref cancel function instead. This function will only
	 * take effect when the connection has been successfully established.
	 *
	 * \param reason_code Reason Code to notify
	 * the Broker of the reason for disconnection.
	 * \param props An instance of \__DISCONNECT_PROPS\__.
	 * \param token Completion token that will be used to produce a
	 * completion handler. The handler will be invoked when the operation completes.
	 * On immediate completion, invocation of the handler will be performed in a manner
	 * equivalent to using \__POST\__.
	 *
	 * \par Handler signature
	 * The handler signature for this operation:
	 *	\code
	 *		void (
	 *			__ERROR_CODE__ // Result of operation.
	 *		)
	 *	\endcode
	 *
	 *	\par Completion condition
	 *	The asynchronous operation will complete when one of the following conditions is true:\n
	 *		- The Client has attempted to send a \__DISCONNECT\__ packet, regardless of whether
	 *		the sending was successful or not.\n
	 *		- An error occurred. This is indicated by an associated \__ERROR_CODE\__ in the handler.\n
	 *
	 *	\par Error codes
	 *	The list of all possible error codes that this operation can finish with:\n
	 *		- `boost::system::errc::errc_t::success`\n
	 *		- `boost::asio::error::operation_aborted`\n
	 *
	 * Refer to the section on \__ERROR_HANDLING\__ to find the underlying causes for each error code.
	 */
	template <typename CompletionToken>
	decltype(auto) async_disconnect(
		disconnect_rc_e reason_code, const disconnect_props& props,
		CompletionToken&& token
	) {
		return detail::async_disconnect(
			detail::disconnect_rc_e(static_cast<uint8_t>(reason_code)),
			props, true, _svc_ptr,
			std::forward<CompletionToken>(token)
		);
	}

	/**
	 * \brief Disconnect the Client by sending a \__DISCONNECT\__ packet
	 * with a Reason Code of reason_codes.normal_disconnection.
	 * This function has terminal effects.
	 * \copydetails async_disconnect
	 */
	template <typename CompletionToken>
	decltype(auto) async_disconnect(CompletionToken&& token) {
		return async_disconnect(
			disconnect_rc_e::normal_disconnection,
			disconnect_props {}, std::forward<CompletionToken>(token)
		);
	}
};


} // end namespace async_mqtt5

#endif // !ASYNC_MQTT5_MQTT_CLIENT_HPP
