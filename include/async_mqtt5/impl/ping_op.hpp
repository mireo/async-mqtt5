#ifndef ASYNC_MQTT5_PING_OP_HPP
#define ASYNC_MQTT5_PING_OP_HPP

#include <chrono>
#include <memory>

#include <boost/asio/consign.hpp>
#include <boost/asio/prepend.hpp>
#include <boost/asio/recycling_allocator.hpp>
#include <boost/asio/steady_timer.hpp>

#include <async_mqtt5/detail/control_packet.hpp>
#include <async_mqtt5/detail/internal_types.hpp>

#include <async_mqtt5/impl/codecs/message_encoders.hpp>

namespace async_mqtt5::detail {

namespace asio = boost::asio;

template <typename ClientService>
class ping_op {
	using client_service = ClientService;
	struct on_timer {};
	struct on_pingreq {};

	static constexpr auto ping_interval = std::chrono::seconds(5);

	std::shared_ptr<client_service> _svc_ptr;
	std::unique_ptr<asio::steady_timer> _ping_timer;

public:
	ping_op(const std::shared_ptr<client_service>& svc_ptr) :
		_svc_ptr(svc_ptr),
		_ping_timer(new asio::steady_timer(svc_ptr->get_executor()))
	{}

	ping_op(ping_op&&) noexcept = default;
	ping_op(const ping_op&) = delete;

	using executor_type = typename client_service::executor_type;
	executor_type get_executor() const noexcept {
		return _svc_ptr->get_executor();
	}

	using allocator_type = asio::recycling_allocator<void>;
	allocator_type get_allocator() const noexcept {
		return allocator_type {};
	}

	using cancellation_slot_type = asio::cancellation_slot;
	asio::cancellation_slot get_cancellation_slot() const noexcept {
		return _svc_ptr->_cancel_ping.slot();
	}

	void perform(duration from_now) {
		_ping_timer->expires_from_now(from_now);
		_ping_timer->async_wait(
			asio::prepend(std::move(*this), on_timer {})
		);
	}

	void operator()(on_timer, error_code ec) {
		get_cancellation_slot().clear();

		if (ec == asio::error::operation_aborted || !_svc_ptr->is_open())
			return;

		auto pingreq = control_packet<allocator_type>::of(
			no_pid, get_allocator(), encoders::encode_pingreq
		);

		const auto& wire_data = pingreq.wire_data();
		_svc_ptr->async_send(
			wire_data,
			no_serial, send_flag::none,
			asio::consign(
				asio::prepend(std::move(*this), on_pingreq {}),
				std::move(pingreq)
			)
		);
	}

	void operator()(on_pingreq, error_code ec) {
		get_cancellation_slot().clear();

		if (!ec || ec == asio::error::try_again)
			perform(ping_interval - std::chrono::seconds(1));
	}
};


} // end namespace async_mqtt5::detail

#endif // !ASYNC_MQTT5_PING_OP_HPP
