#ifndef ASYNC_MQTT5_DISCONNECT_OP_HPP
#define ASYNC_MQTT5_DISCONNECT_OP_HPP

#include <boost/asio/consign.hpp>
#include <boost/asio/prepend.hpp>

#include <async_mqtt5/types.hpp>

#include <async_mqtt5/detail/cancellable_handler.hpp>
#include <async_mqtt5/detail/control_packet.hpp>
#include <async_mqtt5/detail/internal_types.hpp>
#include <async_mqtt5/detail/topic_validation.hpp>
#include <async_mqtt5/detail/utf8_mqtt.hpp>

#include <async_mqtt5/impl/codecs/message_encoders.hpp>

namespace async_mqtt5::detail {

namespace asio = boost::asio;

template <
	typename ClientService,
	typename DisconnectContext
>
class disconnect_op {
	using client_service = ClientService;

	struct on_disconnect {};

	std::shared_ptr<client_service> _svc_ptr;
	DisconnectContext _context;

	using handler_type = cancellable_handler<
		asio::any_completion_handler<void (error_code)>,
		typename ClientService::executor_type
	>;
	handler_type _handler;

public:
	template <typename Handler>
	disconnect_op(
		const std::shared_ptr<client_service>& svc_ptr,
		DisconnectContext&& context, Handler&& handler
	) :
		_svc_ptr(svc_ptr),
		_context(std::move(context)),
		_handler(std::move(handler), get_executor())
	{}

	disconnect_op(disconnect_op&&) noexcept = default;
	disconnect_op(const disconnect_op&) = delete;

	using executor_type = typename client_service::executor_type;
	executor_type get_executor() const noexcept {
		return _svc_ptr->get_executor();
	}

	using allocator_type = asio::associated_allocator_t<handler_type>;
	allocator_type get_allocator() const noexcept {
		return asio::get_associated_allocator(_handler);
	}

	void perform() {
		error_code ec = validate_disconnect(_context.props);
		if (ec)
			return complete_post(ec);

		auto disconnect = control_packet<allocator_type>::of(
			no_pid, get_allocator(),
			encoders::encode_disconnect,
			static_cast<uint8_t>(_context.reason_code), _context.props
		);

		auto max_packet_size = static_cast<size_t>(
			_svc_ptr->connack_property(prop::maximum_packet_size)
				.value_or(default_max_send_size)
		);
		if (disconnect.size() > max_packet_size)
			// drop properties
			return send_disconnect(control_packet<allocator_type>::of(
				no_pid, get_allocator(),
				encoders::encode_disconnect,
				static_cast<uint8_t>(_context.reason_code), disconnect_props {}
			));

		send_disconnect(std::move(disconnect));
	}

	void send_disconnect(control_packet<allocator_type> disconnect) {
		auto wire_data = disconnect.wire_data();
		_svc_ptr->async_send(
			wire_data,
			no_serial, send_flag::terminal,
			asio::consign(
				asio::prepend(std::move(*this), on_disconnect {}),
				std::move(disconnect)
			)
		);
	}

	void operator()(on_disconnect, error_code ec) {
		// The connection must be closed even
		// if we failed to send the DISCONNECT packet
		// with Reason Code of 0x80 or greater.

		if (
			ec == asio::error::operation_aborted ||
			ec == asio::error::no_recovery
		)
			return complete(asio::error::operation_aborted);

		if (_context.terminal) {
			_svc_ptr->cancel();
			return complete(error_code {});
		}

		if (ec == asio::error::try_again)
			return complete(error_code {});

		_svc_ptr->close_stream();
		_svc_ptr->open_stream();

		complete(error_code {});
	}

private:
	static error_code validate_disconnect(const disconnect_props& props) {
		auto reason_string = props[prop::reason_string];
		if (
			reason_string &&
			validate_mqtt_utf8(*reason_string) != validation_result::valid
		)
			return client::error::malformed_packet;

		auto user_properties = props[prop::user_property];
		for (const auto& user_prop: user_properties)
			if (validate_mqtt_utf8(user_prop) != validation_result::valid)
				return client::error::malformed_packet;
		return error_code {};
	}

	void complete(error_code ec) {
		_handler.complete(ec);
	}

	void complete_post(error_code ec) {
		_handler.complete_post(ec);
	}
};

template <typename ClientService, typename CompletionToken>
decltype(auto) async_disconnect(
	disconnect_rc_e reason_code, const disconnect_props& props,
	bool terminal, const std::shared_ptr<ClientService>& svc_ptr,
	CompletionToken&& token
) {
	using Signature = void (error_code);

	auto initiation = [](
		auto handler, disconnect_ctx ctx,
		const std::shared_ptr<ClientService>& svc_ptr
	) {
		disconnect_op {
			svc_ptr, std::move(ctx), std::move(handler)
		}.perform();
	};

	return asio::async_initiate<CompletionToken, Signature>(
		initiation, token,
		disconnect_ctx { reason_code, props, terminal },
		svc_ptr
	);
}


} // end namespace async_mqtt5::detail

#endif // !ASYNC_MQTT5_DISCONNECT_HPP
