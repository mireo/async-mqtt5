#ifndef ASYNC_MQTT5_CLIENT_SERVICE_HPP
#define ASYNC_MQTT5_CLIENT_SERVICE_HPP

#include <boost/asio/experimental/basic_concurrent_channel.hpp>

#include <async_mqtt5/detail/internal_types.hpp>
#include <async_mqtt5/detail/channel_traits.hpp>

#include <async_mqtt5/impl/assemble_op.hpp>
#include <async_mqtt5/impl/async_sender.hpp>
#include <async_mqtt5/impl/autoconnect_stream.hpp>
#include <async_mqtt5/impl/ping_op.hpp>
#include <async_mqtt5/impl/replies.hpp>
#include <async_mqtt5/impl/sentry_op.hpp>

namespace async_mqtt5::detail {

namespace asio = boost::asio;

template <typename StreamType, typename TlsContext>
class stream_context;

template <typename StreamType, typename TlsContext>
requires has_tls_layer<StreamType>
class stream_context<StreamType, TlsContext> {
	using tls_context_type = TlsContext;

	mqtt_context _mqtt_context;
	tls_context_type _tls_context;
public:
	stream_context(TlsContext tls_context) :
		_tls_context(std::move(tls_context))
	{}

	mqtt_context& mqtt_context() {
		return _mqtt_context;
	}

	TlsContext& tls_context() {
		return _tls_context;
	}

	void will(will will) {
		_mqtt_context.will = std::move(will);
	}

	template <typename Prop>
	decltype(auto) connack_prop(Prop p) {
		return _mqtt_context.ca_props[p];
	}

	void credentials(
		std::string client_id,
		std::string username = "", std::string password = ""
	) {
		_mqtt_context.credentials = {
			std::move(client_id),
			std::move(username), std::move(password)
		};
	}

	template <typename Authenticator>
	void authenticator(Authenticator&& authenticator) {
		_mqtt_context.authenticator = any_authenticator(
			std::forward<Authenticator>(authenticator)
		);
	}
};

template <typename StreamType>
requires (!has_tls_layer<StreamType>)
class stream_context<StreamType, std::monostate> {
	mqtt_context _mqtt_context;
public:
	stream_context(std::monostate) {}

	mqtt_context& mqtt_context() {
		return _mqtt_context;
	}

	void will(will will) {
		_mqtt_context.will = std::move(will);
	}

	template <typename Prop>
	decltype(auto) connack_prop(Prop p) {
		return _mqtt_context.ca_props[p];
	}

	void credentials(
		std::string client_id,
		std::string username = "", std::string password = ""
	) {
		_mqtt_context.credentials = {
			std::move(client_id),
			std::move(username), std::move(password)
		};
	}

	template <typename Authenticator>
	void authenticator(Authenticator&& authenticator) {
		_mqtt_context.authenticator = any_authenticator(
			std::forward<Authenticator>(authenticator)
		);
	}
};

template <
	typename StreamType,
	typename TlsContext = std::monostate
>
class client_service {
	using stream_context_type = stream_context<StreamType, TlsContext>;
	using stream_type = autoconnect_stream<
		StreamType, stream_context_type
	>;
public:
	using executor_type = typename stream_type::executor_type;
private:
	using tls_context_type = TlsContext;
	using receive_channel = asio::experimental::basic_concurrent_channel<
		asio::any_io_executor,
		channel_traits<>,
		void (error_code, std::string, std::string, publish_props)
	>;

	template <typename ClientService>
	friend class async_sender;

	template <typename ClientService, typename Handler>
	friend class assemble_op;

	template <typename ClientService>
	friend class ping_op;

	template <typename ClientService>
	friend class sentry_op;

	template <typename ClientService>
	friend class re_auth_op;

	stream_context_type _stream_context;
	stream_type _stream;

	packet_id_allocator _pid_allocator;
	replies _replies;
	async_sender<client_service> _async_sender;

	std::string _read_buff;
	data_span _active_span;

	receive_channel _rec_channel;

	asio::cancellation_signal _cancel_ping;
	asio::cancellation_signal _cancel_sentry;

public:

	client_service(
		const executor_type& ex,
		const std::string& cnf,
		tls_context_type tls_context = {}
	) :
		_stream_context(std::move(tls_context)),
		_stream(ex, _stream_context),
		_async_sender(*this),
		_active_span(_read_buff.cend(), _read_buff.cend()),
		_rec_channel(ex, std::numeric_limits<size_t>::max())
	{}

	executor_type get_executor() const noexcept {
		return _stream.get_executor();
	}

	decltype(auto) tls_context()
	requires (!std::is_same_v<TlsContext, std::monostate>) {
		return _stream_context.tls_context();
	}

	void will(will will) {
		if (!is_open())
			_stream_context.will(std::move(will));
	}

	void credentials(
		std::string client_id,
		std::string username = "", std::string password = ""
	) {
		if (!is_open())
			_stream_context.credentials(
				std::move(client_id),
				std::move(username), std::move(password)
			);
	}

	void brokers(std::string hosts, uint16_t default_port) {
		if (!is_open())
			_stream.brokers(std::move(hosts), default_port);
	}

	template <typename Authenticator>
	void authenticator(Authenticator&& authenticator) {
		if (!is_open())
			_stream_context.authenticator(
				std::forward<Authenticator>(authenticator)
			);
	}

	template <typename Prop>
	decltype(auto) connack_prop(Prop p) {
		return _stream_context.connack_prop(p);
	}

	void run() {
		_stream.open();
		_rec_channel.reset();
	}

	void open_stream() {
		_stream.open();
	}

	bool is_open() const {
		return _stream.is_open();
	}

	void close_stream() {
		_stream.close();
	}

	void cancel() {
		_cancel_ping.emit(asio::cancellation_type::terminal);
		_cancel_sentry.emit(asio::cancellation_type::terminal);

		_rec_channel.close();
		_replies.cancel_unanswered();
		_async_sender.cancel();
		_stream.close();
	}

	uint16_t allocate_pid() {
		return _pid_allocator.allocate();
	}

	void free_pid(uint16_t pid, bool was_throttled = false) {
		_pid_allocator.free(pid);
		if (was_throttled)
			_async_sender.throttled_op_done();
	}

	serial_num_t next_serial_num() {
		return _async_sender.next_serial_num();
	}

	template <typename BufferType, typename CompletionToken>
	decltype(auto) async_send(
		const BufferType& buffer,
		serial_num_t serial_num, unsigned flags,
		CompletionToken&& token
	) {
		return _async_sender.async_send(
			buffer, serial_num, flags, std::forward<CompletionToken>(token)
		);
	}

	template <typename CompletionToken>
	decltype(auto) async_assemble(duration wait_for, CompletionToken&& token) {
		auto initiation = [this] (auto handler, duration wait_for) mutable {
			assemble_op {
				*this, std::move(handler),
				_read_buff, _active_span
			}.perform(wait_for, asio::transfer_at_least(0));
		};

		using Signature = void (
			error_code, uint8_t, byte_citer, byte_citer
		);
		return asio::async_initiate<CompletionToken, Signature> (
			std::move(initiation), token, wait_for
		);
	}

	template <typename CompletionToken>
	decltype(auto) async_wait_reply(
		control_code_e code, uint16_t packet_id, CompletionToken&& token
	) {
		return _replies.async_wait_reply(
			code, packet_id, std::forward<CompletionToken>(token)
		);
	}

	bool channel_store(decoders::publish_message message) {
		auto& [topic, packet_id, flags, props, payload] = message;
		return _rec_channel.try_send(
			error_code {}, std::move(topic),
			std::move(payload), std::move(props)
		);
	}

	template <typename CompletionToken>
	decltype(auto) async_channel_receive(CompletionToken&& token) {
		// sig = void (error_code, std::string, std::string, publish_props)
		return _rec_channel.async_receive(
			std::forward<CompletionToken>(token)
		);
	}

};


} // namespace async_mqtt5::detail

#endif // !ASYNC_MQTT5_CLIENT_SERVICE_HPP
