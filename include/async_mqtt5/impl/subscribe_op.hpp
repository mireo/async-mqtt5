#ifndef ASYNC_MQTT5_SUBSCRIBE_OP_HPP
#define ASYNC_MQTT5_SUBSCRIBE_OP_HPP

#include <algorithm>
#include <cstdint>

#include <boost/asio/detached.hpp>

#include <async_mqtt5/error.hpp>
#include <async_mqtt5/types.hpp>

#include <async_mqtt5/detail/cancellable_handler.hpp>
#include <async_mqtt5/detail/control_packet.hpp>
#include <async_mqtt5/detail/internal_types.hpp>
#include <async_mqtt5/detail/topic_validation.hpp>

#include <async_mqtt5/impl/codecs/message_decoders.hpp>
#include <async_mqtt5/impl/codecs/message_encoders.hpp>

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
		const std::shared_ptr<client_service>& svc_ptr,
		Handler&& handler
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
		auto ec = validate_subscribe(topics, props);
		if (ec)
			return complete_post(ec, topics.size());

		asio::dispatch(
			asio::prepend(std::move(*this), topics, props)
		);
	}

	void operator()(
		const std::vector<subscribe_topic>& topics,
		const subscribe_props& props
	) {
		uint16_t packet_id = _svc_ptr->allocate_pid();
		if (packet_id == 0)
			return complete_post(client::error::pid_overrun, topics.size());

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

		complete(
			ec, packet_id,
			to_reason_codes(std::move(reason_codes)), std::move(props)
		);
	}

private:

	static bool is_option_available(std::optional<uint8_t> sub_opt) {
		return !sub_opt.has_value() || *sub_opt == 1;
	}

	static error_code validate_props(
		const subscribe_props& props, bool sub_id_available
	) {
		auto user_properties = props[prop::user_property];
		for (const auto& user_prop: user_properties)
			if (validate_mqtt_utf8(user_prop) != validation_result::valid)
				return client::error::malformed_packet;

		auto sub_id = props[prop::subscription_identifier];
		if (!sub_id.has_value())
			return error_code {};

		if (!sub_id_available)
			return client::error::subscription_identifier_not_available;

		constexpr uint32_t min_sub_id = 1;
		constexpr uint32_t max_sub_id = 268'435'455;
		return min_sub_id <= *sub_id && *sub_id <= max_sub_id ?
			error_code {} :
			client::error::subscription_identifier_not_available;
	}

	static error_code validate_topic(
		const subscribe_topic& topic, bool wildcard_available, bool shared_available
	) {
		std::string_view topic_filter = topic.topic_filter;

		constexpr std::string_view shared_sub_id = "$share/";
		validation_result result = validation_result::valid;
		if (
			topic_filter.compare(0, shared_sub_id.size(), shared_sub_id) == 0
		) {
			if (!shared_available)
				return client::error::shared_subscription_not_available;

			result = validate_shared_topic_filter(topic_filter, wildcard_available);
		} else
			result = wildcard_available ?
				validate_topic_filter(topic_filter) :
				validate_topic_name(topic_filter);

		if (result == validation_result::invalid)
			return client::error::invalid_topic;
		if (!wildcard_available && result != validation_result::valid)
			return client::error::wildcard_subscription_not_available;
		return error_code {};
	}

	error_code validate_subscribe(
		const std::vector<subscribe_topic>& topics,
		const subscribe_props& props
	) {
		auto [wildcard_available, shared_available, sub_id_available] =
			std::apply(
				[](auto ...opt) {
					return std::make_tuple(is_option_available(opt)...);
				},
				_svc_ptr->connack_props(
					prop::wildcard_subscription_available,
					prop::shared_subscription_available,
					prop::subscription_identifier_available
				)
			);

		error_code ec;
		for (const auto& topic: topics) {
			ec = validate_topic(topic, wildcard_available, shared_available);
			if (ec)
				return ec;
		}

		ec = validate_props(props, sub_id_available);
		return ec;
	}

	static std::vector<reason_code> to_reason_codes(
		std::vector<uint8_t> codes
	) {
		std::vector<reason_code> ret;
		for (uint8_t code : codes) {
			auto rc = to_reason_code<reason_codes::category::suback>(code);
			if (rc)
				ret.push_back(*rc);
		}
		return ret;
	}

	void on_malformed_packet(const std::string& reason) {
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
			suback_props {}
		);
	}

	void complete(
		error_code ec, uint16_t packet_id,
		std::vector<reason_code> reason_codes, suback_props props
	) {
		if (!_svc_ptr->subscriptions_present()) {
			bool has_success_rc = std::any_of(
				reason_codes.cbegin(), reason_codes.cend(),
				[](const reason_code& rc) { return !rc; }
			);
			if (has_success_rc)
				_svc_ptr->subscriptions_present(true);
		}

		_svc_ptr->free_pid(packet_id);
		_handler.complete(ec, std::move(reason_codes), std::move(props));
	}
};


} // end namespace async_mqtt5::detail

#endif // !ASYNC_MQTT5_SUBSCRIBE_OP_HPP
