#ifndef ASYNC_MQTT5_DISCONNECT_OP_HPP
#define ASYNC_MQTT5_DISCONNECT_OP_HPP

#include <boost/asio/consign.hpp>
#include <boost/asio/dispatch.hpp>
#include <boost/asio/prepend.hpp>

#include <async_mqtt5/types.hpp>

#include <async_mqtt5/detail/control_packet.hpp>
#include <async_mqtt5/detail/internal_types.hpp>

#include <async_mqtt5/impl/internal/codecs/message_encoders.hpp>

namespace async_mqtt5::detail {

namespace asio = boost::asio;

template <
	typename ClientService,
	typename DisconnectContext,
	typename Handler
>
class disconnect_op {
	using client_service = ClientService;

	struct on_disconnect {};

	std::shared_ptr<client_service> _svc_ptr;
	DisconnectContext _context;
	std::decay_t<Handler> _handler;

public:
	disconnect_op(
		const std::shared_ptr<client_service>& svc_ptr,
		DisconnectContext&& context, Handler&& handler
	) :
		_svc_ptr(svc_ptr),
		_context(std::move(context)),
		_handler(std::move(handler))
	{}

	disconnect_op(disconnect_op&&) noexcept = default;
	disconnect_op(const disconnect_op&) = delete;

	using executor_type = typename client_service::executor_type;
	executor_type get_executor() const noexcept {
		return _svc_ptr->get_executor();
	}

	using allocator_type = asio::associated_allocator_t<Handler>;
	allocator_type get_allocator() const noexcept {
		return asio::get_associated_allocator(_handler);
	}

	void perform() {
		auto disconnect = control_packet<allocator_type>::of(
			no_pid, get_allocator(),
			encoders::encode_disconnect,
			static_cast<uint8_t>(_context.reason_code), _context.props
		);

		send_disconnect(std::move(disconnect));
	}

	void send_disconnect(control_packet<allocator_type> disconnect) {
		const auto& wire_data = disconnect.wire_data();

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
	void complete(error_code ec) {
		asio::dispatch(
			get_executor(),
			asio::prepend(std::move(_handler), ec)
		);
	}
};

template <typename ClientService, typename CompletionToken>
decltype(auto) async_disconnect(
	disconnect_rc_e reason_code, const disconnect_props& props,
	bool terminal, const std::shared_ptr<ClientService>& svc_ptr,
	CompletionToken&& token
) {
	using Signature = void (error_code);

	auto initiate = [](
		auto handler, disconnect_context ctx,
		const std::shared_ptr<ClientService>& svc_ptr
	) {
		disconnect_op {
			svc_ptr, std::move(ctx), std::move(handler)
		}.perform();
	};

	return asio::async_initiate<CompletionToken, Signature>(
		std::move(initiate), token,
		disconnect_context { reason_code, props, terminal },
		svc_ptr
	);
}


} // end namespace async_mqtt5::detail

#endif // !ASYNC_MQTT5_DISCONNECT_HPP
