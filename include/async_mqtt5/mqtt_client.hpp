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

template <
	typename StreamType,
	typename TlsContext = std::monostate
>
class mqtt_client {
public:
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

	explicit mqtt_client(
		const executor_type& ex,
		const std::string& cnf,
		tls_context_type tls_context = {}
	) :
		_svc_ptr(std::make_shared<client_service_type>(
			ex, cnf, std::move(tls_context)
		))
	{}

	template <typename ExecutionContext>
	requires (std::is_convertible_v<ExecutionContext&, asio::execution_context&>)
	explicit mqtt_client(
		ExecutionContext& context,
		const std::string& cnf,
		TlsContext tls_context = {}
	) :
		mqtt_client(context.get_executor(), cnf, std::move(tls_context))
	{}

	~mqtt_client() {
		cancel();
	}

	executor_type get_executor() const noexcept {
		return _svc_ptr->get_executor();
	}

	decltype(auto) tls_context()
	requires (!std::is_same_v<TlsContext, std::monostate>) {
		return _svc_ptr->tls_context();
	}

	void run() {
		_svc_ptr->open_stream();
		detail::ping_op { _svc_ptr }
			.perform(read_timeout - std::chrono::seconds(1));
		detail::read_message_op { _svc_ptr }.perform();
		detail::sentry_op { _svc_ptr }.perform();
	}

	void cancel() {
		get_executor().execute([svc_ptr = _svc_ptr]() {
			svc_ptr->cancel();
		});
	}

	mqtt_client& will(will will) {
		_svc_ptr->will(std::move(will));
		return *this;
	}

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

	mqtt_client& brokers(std::string hosts, uint16_t default_port = 1883) {
		_svc_ptr->brokers(std::move(hosts), default_port);
		return *this;
	}

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

	template <typename CompletionToken>
	decltype(auto) async_receive(CompletionToken&& token) {
		// Sig = void (error_code, std::string, std::string, publish_props)
		return _svc_ptr->async_channel_receive(
			std::forward<CompletionToken>(token)
		);
	}

	template <typename CompletionToken>
	decltype(auto) async_disconnect(
		disconnect_rc_e reason_code, const disconnect_props& props,
		CompletionToken&& token
	) {
		return detail::async_disconnect(
			reason_code, props, true, _svc_ptr,
			std::forward<CompletionToken>(token)
		);
	}

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
