#ifndef ASYNC_MQTT5_PUBLISH_SEND_OP_HPP
#define ASYNC_MQTT5_PUBLISH_SEND_OP_HPP

#include <boost/asio/detached.hpp>
#include <boost/asio/prepend.hpp>

#include <async_mqtt5/error.hpp>
#include <async_mqtt5/types.hpp>

#include <async_mqtt5/detail/cancellable_handler.hpp>
#include <async_mqtt5/detail/control_packet.hpp>
#include <async_mqtt5/detail/internal_types.hpp>

#include <async_mqtt5/impl/disconnect_op.hpp>
#include <async_mqtt5/impl/internal/codecs/message_decoders.hpp>
#include <async_mqtt5/impl/internal/codecs/message_encoders.hpp>

namespace async_mqtt5::detail {

namespace asio = boost::asio;

template <qos_e qos_type>
using on_publish_signature = std::conditional_t<
	qos_type == qos_e::at_most_once,
		void (error_code),
		std::conditional_t<
			qos_type == qos_e::at_least_once,
				void (error_code, reason_code, puback_props),
				void (error_code, reason_code, pubcomp_props)
		>
>;

template <qos_e qos_type>
using on_publish_props_type = std::conditional_t<
	qos_type == qos_e::at_most_once,
		void,
		std::conditional_t<
			qos_type == qos_e::at_least_once,
				puback_props,
				pubcomp_props
		>
>;

template <qos_e qos_type>
using cancel_args = std::conditional_t<
	qos_type == qos_e::at_most_once,
		std::tuple<>,
		std::conditional_t<
			qos_type == qos_e::at_least_once,
				std::tuple<reason_code, puback_props>,
				std::tuple<reason_code, pubcomp_props>
		>
>;

template <typename ClientService, typename Handler, qos_e qos_type>
class publish_send_op {
	using client_service = ClientService;

	struct on_publish {};
	struct on_puback {};
	struct on_pubrec {};
	struct on_pubrel {};
	struct on_pubcomp {};

	std::shared_ptr<client_service> _svc_ptr;

	cancellable_handler<
		Handler,
		typename client_service::executor_type,
		cancel_args<qos_type>
	> _handler;

	serial_num_t _serial_num;

public:
	publish_send_op(
		const std::shared_ptr<client_service>& svc_ptr, Handler&& handler
	) :
		_svc_ptr(svc_ptr),
		_handler(std::move(handler), get_executor())
	{}

	publish_send_op(publish_send_op&&) noexcept = default;
	publish_send_op(const publish_send_op&) = delete;

	using executor_type = typename client_service::executor_type;
	executor_type get_executor() const noexcept {
		return _svc_ptr->get_executor();
	}

	using allocator_type = asio::associated_allocator_t<Handler>;
	allocator_type get_allocator() const noexcept {
		return asio::get_associated_allocator(_handler);
	}

	void perform(
		std::string topic, std::string payload,
		retain_e retain, const publish_props& props
	) {	
		auto ec = validate_publish(retain, props);
		if (ec)
			return complete_post(ec);

		uint16_t packet_id = 0;
		if constexpr (qos_type != qos_e::at_most_once) {
			packet_id = _svc_ptr->allocate_pid();
			if (packet_id == 0)
				return complete_post(client::error::pid_overrun);
		}

		_serial_num = _svc_ptr->next_serial_num();

		auto publish = control_packet<allocator_type>::of(
			with_pid, get_allocator(),
			encoders::encode_publish, packet_id,
			std::move(topic), std::move(payload),
			qos_type, retain, dup_e::no, props
		);

		send_publish(std::move(publish));
	}

	error_code validate_publish(
		retain_e retain, const publish_props& props
	) {
		auto max_qos = _svc_ptr->connack_prop(prop::maximum_qos);
		if (max_qos && uint8_t(qos_type) > *max_qos)
			return client::error::qos_not_supported;

		auto retain_available = _svc_ptr->connack_prop(prop::retain_available);
		if (retain_available && *retain_available == 0 && retain == retain_e::yes)
			return client::error::retain_not_available;

		// TODO: topic alias mapping
		auto topic_alias_max = _svc_ptr->connack_prop(prop::topic_alias_maximum);
		auto topic_alias = props[prop::topic_alias];
		if ((!topic_alias_max || topic_alias_max && *topic_alias_max == 0) && topic_alias)
			return client::error::topic_alias_maximum_reached;
		if (topic_alias_max && topic_alias && *topic_alias > *topic_alias_max)
			return client::error::topic_alias_maximum_reached;
		return {};
	}

	void send_publish(control_packet<allocator_type> publish) {
		const auto& wire_data = publish.wire_data();
		_svc_ptr->async_send(
			wire_data,
			_serial_num,
			send_flag::throttled * (qos_type != qos_e::at_most_once),
			asio::prepend(std::move(*this), on_publish {}, std::move(publish))
		);
	}

	void operator()(
		on_publish, control_packet<allocator_type> publish,
		error_code ec
	) {
		if (ec == asio::error::try_again)
			return send_publish(std::move(publish));

		if constexpr (qos_type == qos_e::at_most_once)
			return complete(ec);

		else {
			auto packet_id = publish.packet_id();

			if constexpr (qos_type == qos_e::at_least_once) {
				if (ec)
					return complete(
						ec, reason_codes::empty, packet_id, puback_props {}
					);
				_svc_ptr->async_wait_reply(
					control_code_e::puback, packet_id,
					asio::prepend(
						std::move(*this), on_puback {}, std::move(publish)
					)
				);
			}

			else if constexpr (qos_type == qos_e::exactly_once) {
				if (ec)
					return complete(
						ec, reason_codes::empty, packet_id, pubcomp_props {}
					);
				_svc_ptr->async_wait_reply(
					control_code_e::pubrec, packet_id,
					asio::prepend(
						std::move(*this), on_pubrec {}, std::move(publish)
					)
				);
			}
		}
	}

	void operator()(
		on_puback, control_packet<allocator_type> publish,
		error_code ec, byte_citer first, byte_citer last
	)
	requires (qos_type == qos_e::at_least_once) {

		if (ec == asio::error::try_again) // "resend unanswered"
			return send_publish(std::move(publish.set_dup()));

		uint16_t packet_id = publish.packet_id();

		if (ec)
			return complete(
				ec, reason_codes::empty, packet_id, puback_props {}
			);

		auto puback = decoders::decode_puback(std::distance(first, last), first);
		if (!puback.has_value()) {
			on_malformed_packet("Malformed PUBACK: cannot decode");
			return send_publish(std::move(publish.set_dup()));
		}

		auto& [reason_code, props] = *puback;
		auto rc = to_reason_code<reason_codes::category::puback>(reason_code);
		if (!rc) {
			on_malformed_packet("Malformed PUBACK: invalid Reason Code");
			return send_publish(std::move(publish.set_dup()));
		}

		complete(ec, *rc, packet_id, std::move(props));
	}

	void operator()(
		on_pubrec, control_packet<allocator_type> publish,
		error_code ec, byte_citer first, byte_citer last
	)
	requires (qos_type == qos_e::exactly_once) {

		if (ec == asio::error::try_again) // "resend unanswered"
			return send_publish(std::move(publish.set_dup()));

		uint16_t packet_id = publish.packet_id();

		if (ec)
			return complete(
				ec, reason_codes::empty, packet_id, pubcomp_props {}
			);

		auto pubrec = decoders::decode_pubrec(std::distance(first, last), first);
		if (!pubrec.has_value()) {
			on_malformed_packet("Malformed PUBREC: cannot decode");
			return send_publish(std::move(publish.set_dup()));
		}

		auto& [reason_code, props] = *pubrec;

		auto rc = to_reason_code<reason_codes::category::pubrec>(reason_code);
		if (!rc) {
			on_malformed_packet("Malformed PUBREC: invalid Reason Code");
			return send_publish(std::move(publish.set_dup()));
		}

		if (*rc)
			return complete(ec, *rc, packet_id, pubcomp_props {});

		auto pubrel = control_packet<allocator_type>::of(
			with_pid, get_allocator(),
			encoders::encode_pubrel, packet_id,
			0, pubrel_props {}
		);

		send_pubrel(std::move(pubrel), false);
	}

	void send_pubrel(control_packet<allocator_type> pubrel, bool throttled) {
		const auto& wire_data = pubrel.wire_data();
		_svc_ptr->async_send(
			wire_data,
			_serial_num,
			(send_flag::throttled * throttled) | send_flag::prioritized,
			asio::prepend(std::move(*this), on_pubrel {}, std::move(pubrel))
		);
	}

	void operator()(
		on_pubrel, control_packet<allocator_type> pubrel, error_code ec
	)
	requires (qos_type == qos_e::exactly_once) {

		if (ec == asio::error::try_again)
			return send_pubrel(std::move(pubrel), true);

		uint16_t packet_id = pubrel.packet_id();

		if (ec)
			return complete(
				ec, reason_codes::empty, packet_id, pubcomp_props {}
			);

		_svc_ptr->async_wait_reply(
			control_code_e::pubcomp, packet_id,
			asio::prepend(std::move(*this), on_pubcomp {}, std::move(pubrel))
		);
	}

	void operator()(
		on_pubcomp, control_packet<allocator_type> pubrel,
		error_code ec,
		byte_citer first, byte_citer last
	)
	requires (qos_type == qos_e::exactly_once) {

		if (ec == asio::error::try_again) // "resend unanswered"
			return send_pubrel(std::move(pubrel), true);

		uint16_t packet_id = pubrel.packet_id();

		if (ec)
			return complete(
				ec, reason_codes::empty, packet_id, pubcomp_props {}
			);

		auto pubcomp = decoders::decode_pubcomp(std::distance(first, last), first);
		if (!pubcomp.has_value()) {
			on_malformed_packet("Malformed PUBCOMP: cannot decode");
			return send_pubrel(std::move(pubrel), true);
		}

		auto& [reason_code, props] = *pubcomp;

		auto rc = to_reason_code<reason_codes::category::pubcomp>(reason_code);
		if (!rc) {
			on_malformed_packet("Malformed PUBCOMP: invalid Reason Code");
			return send_pubrel(std::move(pubrel), true);
		}

		return complete(ec, *rc, pubrel.packet_id(), pubcomp_props{});
	}


private:
	void on_malformed_packet(const std::string& reason) {
		auto props = disconnect_props {};
		props[prop::reason_string] = reason;
		async_disconnect(
			disconnect_rc_e::malformed_packet, props, false, _svc_ptr,
			asio::detached
		);
	}

	void complete(error_code ec)
	requires (qos_type == qos_e::at_most_once)
	{
		_handler.complete(ec);
	}

	void complete_post(error_code ec)
	requires (qos_type == qos_e::at_most_once)
	{
		_handler.complete_post(ec);
	}

	template <typename Props = on_publish_props_type<qos_type>>
	requires (
		std::is_same_v<Props, puback_props> ||
		std::is_same_v<Props, pubcomp_props>
	)
	void complete(
		error_code ec, reason_code rc,
		uint16_t packet_id, Props&& props
	) {
		_svc_ptr->free_pid(packet_id, true);
		_handler.complete(ec, rc, std::forward<Props>(props));
	}

	template <typename Props = on_publish_props_type<qos_type>>
	requires (
		std::is_same_v<Props, puback_props> ||
		std::is_same_v<Props, pubcomp_props>
	)
	void complete_post(error_code ec) {
		_handler.complete_post(ec, reason_codes::empty, Props {});
	}
};


} // end namespace async_mqtt5::detail

#endif // !ASYNC_MQTT5_PUBLISH_SEND_OP_HPP
