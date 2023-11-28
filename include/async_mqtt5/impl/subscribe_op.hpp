#ifndef ASYNC_MQTT5_SUBSCRIBE_OP_HPP
#define ASYNC_MQTT5_SUBSCRIBE_OP_HPP

#include <boost/asio/detached.hpp>

#include <async_mqtt5/error.hpp>

#include <async_mqtt5/detail/cancellable_handler.hpp>
#include <async_mqtt5/detail/control_packet.hpp>
#include <async_mqtt5/detail/internal_types.hpp>

#include <async_mqtt5/impl/internal/codecs/message_decoders.hpp>
#include <async_mqtt5/impl/internal/codecs/message_encoders.hpp>

#include <async_mqtt5/impl/disconnect_op.hpp>

namespace async_mqtt5::detail {

namespace asio = boost::asio;

template <typename ClientService, typename Handler>
class subscribe_op {
	using client_service = ClientService;
	struct on_subscribe {};
	struct on_suback {};

	std::shared_ptr<client_service> _svc_ptr;

	cancellable_handler<
		Handler,
		typename client_service::executor_type,
		std::tuple<std::vector<reason_code>, suback_props>
	> _handler;

public:
	subscribe_op(
		const std::shared_ptr<client_service>& svc_ptr, Handler&& handler
	) :
		_svc_ptr(svc_ptr),
		_handler(std::move(handler), get_executor())
	{}

	subscribe_op(subscribe_op&&) noexcept = default;
	subscribe_op(const subscribe_op&) noexcept = delete;

	using executor_type = typename client_service::executor_type;
	executor_type get_executor() const noexcept {
		return _svc_ptr->get_executor();
	}

	using allocator_type = asio::associated_allocator_t<Handler>;
	allocator_type get_allocator() const noexcept {
		return asio::get_associated_allocator(_handler);
	}

	void perform(
		const std::vector<subscribe_topic>& topics,
		const subscribe_props& props
	) {
		uint16_t packet_id = _svc_ptr->allocate_pid();
		if (packet_id == 0)
			return complete_post(client::error::pid_overrun);

		auto subscribe = control_packet<allocator_type>::of(
			with_pid, get_allocator(),
			encoders::encode_subscribe, packet_id,
			topics, props
		);

		send_subscribe(std::move(subscribe));
	}

	void send_subscribe(control_packet<allocator_type> subscribe) {
		if (_handler.empty()) // already cancelled
			return _svc_ptr->free_pid(subscribe.packet_id());

		const auto& wire_data = subscribe.wire_data();
		_svc_ptr->async_send(
			wire_data,
			no_serial, send_flag::none,
			asio::prepend(
				std::move(*this), on_subscribe {}, std::move(subscribe)
			)
		);
	}

	void operator()(
		on_subscribe, control_packet<allocator_type> packet,
		error_code ec
	) {
		if (ec == asio::error::try_again)
			return send_subscribe(std::move(packet));

		auto packet_id = packet.packet_id();

		if (ec)
			return complete(ec, packet_id, {}, {});

		_svc_ptr->async_wait_reply(
			control_code_e::suback, packet_id,
			asio::prepend(std::move(*this), on_suback {}, std::move(packet))
		);
	}

	void operator()(
		on_suback, control_packet<allocator_type> packet,
		error_code ec, byte_citer first, byte_citer last
	) {
		if (ec == asio::error::try_again) // "resend unanswered"
			return send_subscribe(std::move(packet));

		uint16_t packet_id = packet.packet_id();

		if (ec)
			return complete(ec, packet_id, {}, {});

		auto suback = decoders::decode_suback(std::distance(first, last), first);
		if (!suback.has_value()) {
			on_malformed_packet("Malformed SUBACK: cannot decode");
			return send_subscribe(std::move(packet));
		}

		auto& [props, reason_codes] = *suback;
		// TODO: perhaps do something with the topics we subscribed to (one day)

		complete(
			ec, packet_id,
			to_reason_codes(std::move(reason_codes)), std::move(props)
		);
	}

private:

	static std::vector<reason_code> to_reason_codes(std::vector<uint8_t> codes) {
		std::vector<reason_code> ret;
		for (uint8_t code : codes) {
			auto rc = to_reason_code<reason_codes::category::suback>(code);
			if (rc)
				ret.push_back(*rc);
			// TODO: on off chance that one of the rcs is invalid, should we push something to mark that?
		}
		return ret;
	}

	void on_malformed_packet(const std::string& reason) {
		auto props = disconnect_props{};
		props[prop::reason_string] = reason;
		async_disconnect(
			disconnect_rc_e::malformed_packet, props, false, _svc_ptr,
			asio::detached
		);
	}


	void complete_post(error_code ec) {
		_handler.complete_post(
			ec, std::vector<reason_code> {}, suback_props {}
		);
	}

	void complete(
		error_code ec, uint16_t packet_id,
		std::vector<reason_code> reason_codes, suback_props props
	) {
		_svc_ptr->free_pid(packet_id);
		_handler.complete(ec, std::move(reason_codes), std::move(props));
	}
};


} // end namespace async_mqtt5::detail

#endif // !ASYNC_MQTT5_SUBSCRIBE_OP_HPP
