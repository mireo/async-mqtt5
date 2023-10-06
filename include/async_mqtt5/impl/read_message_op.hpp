#ifndef ASYNC_MQTT5_READ_MESSAGE_OP_HPP
#define ASYNC_MQTT5_READ_MESSAGE_OP_HPP

#include <chrono>

#include <boost/asio/prepend.hpp>
#include <boost/asio/recycling_allocator.hpp>

#include <async_mqtt5/types.hpp>

#include <async_mqtt5/detail/control_packet.hpp>

#include <async_mqtt5/impl/disconnect_op.hpp>
#include <async_mqtt5/impl/publish_rec_op.hpp>

#include <async_mqtt5/impl/internal/codecs/message_decoders.hpp>

namespace async_mqtt5::detail {

namespace asio = boost::asio;

template <typename ClientService>
class read_message_op {
	using client_service = ClientService;
	struct on_message {};
	struct on_disconnect {};

	std::shared_ptr<client_service> _svc_ptr;
public:
	read_message_op(
		const std::shared_ptr<client_service>& svc_ptr
	) :
		_svc_ptr(svc_ptr)
	{}

	read_message_op(read_message_op&&) noexcept = default;
	read_message_op(const read_message_op&) = delete;

	using executor_type = typename client_service::executor_type;
	executor_type get_executor() const noexcept {
		return _svc_ptr->get_executor();
	}

	using allocator_type = asio::recycling_allocator<void>;
	allocator_type get_allocator() const noexcept {
		return allocator_type {};
	}

	void perform() {
		_svc_ptr->async_assemble(
			std::chrono::seconds(20),
			asio::prepend(std::move(*this), on_message {})
		);
	}

	void operator()(
		on_message, error_code ec,
		uint16_t packet_id, uint8_t control_code,
		byte_citer first, byte_citer last
	) {
		if (ec == client::error::malformed_packet)
			return on_malformed_packet(
				"Malformed Packet received from the Server"
			);

		if (
			ec == asio::error::operation_aborted ||
			ec == asio::error::no_recovery
		)
			return;

		dispatch(ec, packet_id, control_code, first, last);
	}

	void operator()(on_disconnect, error_code ec) {
		if (!ec || ec == asio::error::try_again)
			perform();
	}

private:

	// TODO: ec & packet_id are not used here
	void dispatch(
		error_code ec, uint16_t packet_id, uint8_t control_byte,
		byte_citer first, byte_citer last
	) {
		using enum control_code_e;
		auto code = control_code_e(control_byte & 0b11110000);

		switch (code) {
			case publish: {
				auto msg = decoders::decode_publish(
					control_byte, std::distance(first, last), first
				);
				if (!msg.has_value())
					return on_malformed_packet(
						"Malformed PUBLISH received: cannot decode"
					);

				publish_rec_op { _svc_ptr }.perform(std::move(*msg));
			}
			break;
			case disconnect: {
				_svc_ptr->close_stream();
				_svc_ptr->open_stream();
			}
			break;
			case auth: {
				// TODO: dispatch auth
			}
			break;
		}

		perform();
	}

	void on_malformed_packet(const std::string& reason) {
		auto props = disconnect_props {};
		props[prop::reason_string] = reason;
		auto svc_ptr = _svc_ptr; // copy before this is moved
		async_disconnect(
			disconnect_rc_e::malformed_packet, props, false, svc_ptr,
			asio::prepend(std::move(*this), on_disconnect {})
		);
	}

};


} // end namespace async_mqtt5::detail

#endif // !ASYNC_MQTT5_READ_MESSAGE_OP_HPP
