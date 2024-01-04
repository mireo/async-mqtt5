#ifndef ASYNC_MQTT5_CLIENT_SERVICE_HPP
#define ASYNC_MQTT5_CLIENT_SERVICE_HPP

#include <utility>

#include <boost/asio/experimental/basic_concurrent_channel.hpp>

#include <async_mqtt5/detail/channel_traits.hpp>
#include <async_mqtt5/detail/internal_types.hpp>

#include <async_mqtt5/impl/assemble_op.hpp>
#include <async_mqtt5/impl/async_sender.hpp>
#include <async_mqtt5/impl/autoconnect_stream.hpp>
#include <async_mqtt5/impl/ping_op.hpp>
#include <async_mqtt5/impl/replies.hpp>
#include <async_mqtt5/impl/sentry_op.hpp>

namespace async_mqtt5::detail {

namespace asio = boost::asio;

template <
	typename StreamType, typename TlsContext,
	typename Enable = void
>
class stream_context;

template <
	typename StreamType, typename TlsContext
>
class stream_context<
	StreamType, TlsContext,
	std::enable_if_t<has_tls_layer<StreamType>>
> {
	using tls_context_type = TlsContext;

	mqtt_ctx _mqtt_context;
	tls_context_type _tls_context;
public:
	explicit stream_context(TlsContext tls_context) :
		_tls_context(std::move(tls_context))
	{}

	auto& mqtt_context() {
		return _mqtt_context;
	}

	auto& tls_context() {
		return _tls_context;
	}

	auto& session_state() {
		return _mqtt_context.state;
	}

	const auto& session_state() const {
		return _mqtt_context.state;
	}

	void will(will will) {
		_mqtt_context.will_msg = std::move(will);
	}

	template <typename Prop>
	const auto& connack_property(Prop p) const {
		return _mqtt_context.ca_props[p];
	}

	const auto& connack_properties() const {
		return _mqtt_context.ca_props;
	}

	template <typename Prop>
	const auto& connect_property(Prop p) const {
		return _mqtt_context.co_props[p];
	}

	template <typename Prop>
	auto& connect_property(Prop p) {
		return _mqtt_context.co_props[p];
	}

	void connect_propertiess(connect_props props) {
		_mqtt_context.co_props = std::move(props);
	}

	void credentials(
		std::string client_id,
		std::string username = "", std::string password = ""
	) {
		_mqtt_context.creds = {
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
class stream_context<
	StreamType, std::monostate,
	std::enable_if_t<!has_tls_layer<StreamType>>
> {
	mqtt_ctx _mqtt_context;
public:
	explicit stream_context(std::monostate) {}

	auto& mqtt_context() {
		return _mqtt_context;
	}

	auto& session_state() {
		return _mqtt_context.state;
	}

	const auto& session_state() const {
		return _mqtt_context.state;
	}

	void will(will will) {
		_mqtt_context.will_msg = std::move(will);
	}

	template <typename Prop>
	const auto& connack_property(Prop p) const {
		return _mqtt_context.ca_props[p];
	}

	const auto& connack_properties() const {
		return _mqtt_context.ca_props;
	}

	template <typename Prop>
	const auto& connect_property(Prop p) const {
		return _mqtt_context.co_props[p];
	}

	template <typename Prop>
	auto& connect_property(Prop p) {
		return _mqtt_context.co_props[p];
	}

	void connect_properties(connect_props props) {
		_mqtt_context.co_props = std::move(props);
	}

	void credentials(
		std::string client_id,
		std::string username = "", std::string password = ""
	) {
		_mqtt_context.creds = {
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
	using self_type = client_service<StreamType, TlsContext>;
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
		const std::string& /* cnf */,
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

	template <
		typename Ctx = TlsContext,
		std::enable_if_t<!std::is_same_v<Ctx, std::monostate>, bool> = true
	>
	decltype(auto) tls_context() {
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

	template <
		typename Authenticator,
		std::enable_if_t<is_authenticator<Authenticator>, bool> = true
	>
	void authenticator(Authenticator&& authenticator) {
		if (!is_open())
			_stream_context.authenticator(
				std::forward<Authenticator>(authenticator)
			);
	}

	template <typename Prop>
	const auto& connect_property(Prop p) const {
		return _stream_context.connect_property(p);
	}

	template <typename Prop>
	auto& connect_property(Prop p) {
		return _stream_context.connect_property(p);
	}

	void connect_properties(connect_props props) {
		if (!is_open())
			_stream_context.connect_properties(std::move(props));
	}

	template <typename Prop>
	const auto& connack_property(Prop p) const {
		return _stream_context.connack_property(p);
	}

	const auto& connack_properties() const {
		return _stream_context.connack_properties();
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
		using Signature = void (error_code, uint8_t, byte_citer, byte_citer);

		auto initiation = [] (
			auto handler, self_type& self,
			duration wait_for, std::string& read_buff, data_span& active_span
		) {
			assemble_op {
				self, std::move(handler), read_buff, active_span
			}.perform(wait_for, asio::transfer_at_least(0));
		};

		return asio::async_initiate<CompletionToken, Signature> (
			initiation, token, std::ref(*this),
			wait_for, std::ref(_read_buff), std::ref(_active_span)
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

	bool subscriptions_present() const {
		return _stream_context.session_state().subscriptions_present();
	}

	void subscriptions_present(bool present) {
		_stream_context.session_state().subscriptions_present(present);
	}

	void update_session_state() {
		auto& session_state = _stream_context.session_state();

		if (!session_state.session_present()) {
			_replies.clear_pending_pubrels();
			session_state.session_present(true);

			if (session_state.subscriptions_present()) {
				channel_store_error(client::error::session_expired);
				session_state.subscriptions_present(false);
			}
		}
	}

	bool channel_store(decoders::publish_message message) {
		auto& [topic, packet_id, flags, props, payload] = message;
		return _rec_channel.try_send(
			error_code {}, std::move(topic),
			std::move(payload), std::move(props)
		);
	}

	bool channel_store_error(error_code ec) {
		return _rec_channel.try_send(
			ec, std::string {}, std::string {}, publish_props {}
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
