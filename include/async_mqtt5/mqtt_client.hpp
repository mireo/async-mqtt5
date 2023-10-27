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
		tls_context_type tls_context = {}
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

	/// Copy constructor.
	mqtt_client(const mqtt_client& other) = delete;

	/**
	 * \brief Destructor.
	 *
	 * \details Automatically calls \ref mqtt_client::cancel.
	 */
	~mqtt_client() {
		cancel();
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
		_svc_ptr->open_stream();
		detail::ping_op { _svc_ptr }
			.perform(read_timeout - std::chrono::seconds(1));
		detail::read_message_op { _svc_ptr }.perform();
		detail::sentry_op { _svc_ptr }.perform();
	}

	// TODO: channel cancel
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
	 * \brief Assign a \ref will Message.
	 *
	 * \details The \ref will Message that the Broker should publish
	 * after the Network Connection is closed and it is not
	 * closed normally.
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
	 * completion handler, which will be called when the operation completed.
	 *
	 * \par Handler signature
	 * The handler signature for this operation depends on the \ref qos_e specified:\n
	 *
	 *	\ref qos_e == `qos_e::at_most_once`:
	 *		\code
	 *			void (
	 *				__ERROR_CODE__	// Result of operation
	 *			)
	 *		\endcode
	 *
	 *	\ref qos_e == `qos_e::at_least_once`:
	 *		\code
	 *			void (
	 *				__ERROR_CODE__,	// Result of operation.
	 *				__REASON_CODE__,	// Reason Code received from Broker.
	 *				__PUBACK_PROPS__	// Properties received in the PUBACK packet.
	 *			)
	 *		\endcode
	 *
	 *	\ref qos_e == `qos_e::exactly_once`:
	 *		\code
	 *			void (
	 *				__ERROR_CODE__,	// Result of operation.
	 *				__REASON_CODE__,	// Reason Code received from Broker.
	 *				__PUBCOMP_PROPS__	// Properties received in the PUBCOMP packet.
	 *			)
	 *		\endcode
	 *
	 *	\par Error codes
	 *	The list of all possible error codes that this operation can finish with:\n
	 *		- `boost::system::errc::errc_t::success`\n
	 *		- `boost::asio::error::operation_aborted` \n
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

	// TODO: perhaps there is a way to not copy documentation (\copybrief, \copydetails)
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
	 * completion handler, which will be called when the operation completed.
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
	 *	\par Error codes
	 *	The list of all possible error codes that this operation can finish with:\n
	 *		- `boost::system::errc::errc_t::success`\n
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
	 * completion handler, which will be called when the operation completed.
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
	 *	\par Error codes
	 *	The list of all possible error codes that this operation can finish with:\n
	 *		- `boost::system::errc::errc_t::success`\n
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
	 * completion handler, which will be called when the operation completed.
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
	 *	\par Error codes
	 *	The list of all possible error codes that this operation can finish with:\n
	 *		- `boost::system::errc::errc_t::success`\n
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
	 * completion handler, which will be called when the operation completed.
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
	 *	\par Error codes
	 *	The list of all possible error codes that this operation can finish with:\n
	 *		- `boost::system::errc::errc_t::success`\n
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


	// TODO: cancel the channel!
	/**
	 * \brief Asynchronously receive an Application Message.
	 *
	 * \details The Client will receive and complete deliveries for all the
	 * \__PUBLISH\__ packets received from the Broker throughout its lifetime.
	 * The Client will store them internally in order they were delivered.
	 * Calling this function will attempt to receive an Application Message
	 * from internal storage.
	 *
	 * \note The completion handler will be only invoked if an error occurred
	 * or there is a pending Application Message.
	 *
	 * \param token Completion token that will be used to produce a
	 * completion handler, which will be called when the operation completed.
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
	 *	\par Error codes
	 *	The list of all possible error codes that this operation can finish with:\n
	 *		- `boost::system::errc::errc_t::success`\n
	 *		- `boost::asio::experimental::error::channel_cancelled`
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
	 * \brief Disconnect the Client. This function has terminal effects.
	 *
	 * \details Send a \__DISCONNECT\__ packet to the Broker with a Reason Code
	 * describing the reason for disconnection.
	 *
	 * \attention This function has terminal effects and will close the Client.
	 * See \ref mqtt_client::cancel.
	 *
	 * \param reason_code Reason Code to notify
	 * the Broker of the reason for disconnection.
	 * \param props An instance of \__DISCONNECT_PROPS\__.
	 * \param token Completion token that will be used to produce a
	 * completion handler, which will be called when the operation completed.
	 *
	 * \par Handler signature
	 * The handler signature for this operation:
	 *	\code
	 *		void (
	 *			__ERROR_CODE__ // Result of operation.
	 *		)
	 *	\endcode
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
	 * \brief Disconnect the Client. This function has terminal effects.
	 *
	 * \details Send a \__DISCONNECT\__ packet to the Broker with a Reason Code
	 * \ref reason_codes::normal_disconnection describing
	 * the reason for disconnection.
	 *
	 * \attention This function has terminal effects and will close the Client.
	 * See \ref mqtt_client::cancel.
	 *
	 * \param token Completion token that will be used to produce a
	 * completion handler, which will be called when the operation completed.
	 *
	 * \par Handler signature
	 * The handler signature for this operation:
	 *	\code
	 *		void (
	 *			__ERROR_CODE__ // Result of operation.
	 *		)
	 *	\endcode
	 *
	 *	\par Error codes
	 *	The list of all possible error codes that this operation can finish with:\n
	 *		- `boost::system::errc::errc_t::success`\n
	 *		- `boost::asio::error::operation_aborted`\n
	 *
	 * Refer to the section on \__ERROR_HANDLING\__ to find the underlying causes for each error code.
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
