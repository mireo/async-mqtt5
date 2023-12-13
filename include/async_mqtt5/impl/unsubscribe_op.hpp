#ifndef ASYNC_MQTT5_UNSUBSCRIBE_OP_HPP
#define ASYNC_MQTT5_UNSUBSCRIBE_OP_HPP

#include <boost/asio/associated_allocator.hpp>
#include <boost/asio/detached.hpp>

#include <async_mqtt5/error.hpp>

#include <async_mqtt5/detail/cancellable_handler.hpp>
#include <async_mqtt5/detail/control_packet.hpp>
#include <async_mqtt5/detail/internal_types.hpp>
#include <async_mqtt5/detail/topic_validation.hpp>

#include <async_mqtt5/impl/disconnect_op.hpp>
#include <async_mqtt5/impl/codecs/message_decoders.hpp>
#include <async_mqtt5/impl/codecs/message_encoders.hpp>

namespace async_mqtt5::detail {

namespace asio = boost::asio;

template <typename ClientService, typename Handler>
class unsubscribe_op {
	using client_service = ClientService;
	struct on_unsubscribe {};
	struct on_unsuback {};

	std::shared_ptr<client_service> _svc_ptr;

	cancellable_handler<
		Handler,
		typename client_service::executor_type,
		std::tuple<std::vector<reason_code>, unsuback_props>
	> _handler;

public:
	unsubscribe_op(
		const std::shared_ptr<client_service>& svc_ptr,
		Handler&& handler
	) :
		_svc_ptr(svc_ptr),
		_handler(std::move(handler), get_executor())
	{}

	unsubscribe_op(unsubscribe_op&&) noexcept = default;
	unsubscribe_op(const unsubscribe_op&) noexcept = delete;

	using executor_type = typename client_service::executor_type;
	executor_type get_executor() const noexcept {
		return _svc_ptr->get_executor();
	}

	using allocator_type = asio::associated_allocator_t<Handler>;
	allocator_type get_allocator() const noexcept {
		return asio::get_associated_allocator(_handler);
	}

	void perform(
		const std::vector<std::string>& topics,
		const unsubscribe_props& props
	) {
		auto ec = validate_topics(topics);
		if (ec)
			return complete_post(ec, topics.size());

		asio::dispatch(
			asio::prepend(std::move(*this), topics, props)
		);
	}

	void operator()(
		const std::vector<std::string>& topics,
		const unsubscribe_props& props
	) {
		uint16_t packet_id = _svc_ptr->allocate_pid();
		if (packet_id == 0)
			return complete_post(client::error::pid_overrun, topics.size());

		auto unsubscribe = control_packet<allocator_type>::of(
			with_pid, get_allocator(),
			encoders::encode_unsubscribe, packet_id,
			topics, props
		);

		send_unsubscribe(std::move(unsubscribe));
	}

	void send_unsubscribe(control_packet<allocator_type> unsubscribe) {
		if (_handler.empty()) // already cancelled
			return _svc_ptr->free_pid(unsubscribe.packet_id());

		const auto& wire_data = unsubscribe.wire_data();
		_svc_ptr->async_send(
			wire_data,
			no_serial, send_flag::none,
			asio::prepend(
				std::move(*this), on_unsubscribe{}, std::move(unsubscribe)
			)
		);
	}

	void operator()(
		on_unsubscribe, control_packet<allocator_type> packet,
		error_code ec
	) {
		if (ec == asio::error::try_again)
			return send_unsubscribe(std::move(packet));

		auto packet_id = packet.packet_id();

		if (ec)
			return complete(ec, packet_id, {}, {});

		_svc_ptr->async_wait_reply(
			control_code_e::unsuback, packet_id,
			asio::prepend(std::move(*this), on_unsuback{}, std::move(packet))
		);
	}

	void operator()(
		on_unsuback, control_packet<allocator_type> packet,
		error_code ec, byte_citer first, byte_citer last
	) {
		if (ec == asio::error::try_again) // "resend unanswered"
			return send_unsubscribe(std::move(packet));

		uint16_t packet_id = packet.packet_id();

		if (ec)
			return complete(ec, packet_id, {}, {});

		auto unsuback = decoders::decode_unsuback(
			std::distance(first, last), first
		);
		if (!unsuback.has_value()) {
			on_malformed_packet("Malformed UNSUBACK: cannot decode");
			return send_unsubscribe(std::move(packet));
		}

		auto& [props, reason_codes] = *unsuback;

		complete(
			ec, packet_id,
			to_reason_codes(std::move(reason_codes)), std::move(props)
		);
	}

private:

	static error_code validate_topics(const std::vector<std::string>& topics) {
		for (const auto& topic : topics)
			if (validate_topic_filter(topic) != validation_result::valid)
				return client::error::invalid_topic;
		return error_code {};
	}

	static std::vector<reason_code> to_reason_codes(std::vector<uint8_t> codes) {
		std::vector<reason_code> ret;
		for (uint8_t code : codes) {
			auto rc = to_reason_code<reason_codes::category::unsuback>(code);
			if (rc)
				ret.push_back(*rc);
		}
		return ret;
	}

	void on_malformed_packet(
		const std::string& reason
	) {
		auto props = disconnect_props {};
		props[prop::reason_string] = reason;
		async_disconnect(
			disconnect_rc_e::malformed_packet, props, false, _svc_ptr,
			asio::detached
		);
	}

	void complete_post(error_code ec, size_t num_topics) {
		_handler.complete_post(
			ec, std::vector<reason_code> { num_topics, reason_codes::empty },
			unsuback_props {}
		);
	}

	void complete(
		error_code ec, uint16_t packet_id,
		std::vector<reason_code> reason_codes, unsuback_props props
	) {
		_svc_ptr->free_pid(packet_id);
		_handler.complete(ec, std::move(reason_codes), std::move(props));
	}
};


} // end namespace async_mqtt5::detail

#endif // !ASYNC_MQTT5_UNSUBSCRIBE_OP_HPP
