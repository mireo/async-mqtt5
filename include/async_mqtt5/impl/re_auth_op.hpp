#ifndef ASYNC_MQTT5_RE_AUTH_OP_hpp
#define ASYNC_MQTT5_RE_AUTH_OP_hpp

#include <boost/asio/detached.hpp>

#include <async_mqtt5/error.hpp>

#include <async_mqtt5/detail/control_packet.hpp>
#include <async_mqtt5/detail/any_authenticator.hpp>

#include <async_mqtt5/impl/codecs/message_decoders.hpp>
#include <async_mqtt5/impl/codecs/message_encoders.hpp>

#include <async_mqtt5/impl/disconnect_op.hpp>

namespace async_mqtt5::detail {

namespace asio = boost::asio;

template <typename ClientService>
class re_auth_op {
	using client_service = ClientService;
	struct on_auth_data {};

	std::shared_ptr<client_service> _svc_ptr;
	any_authenticator& _auth;

public:
	re_auth_op(const std::shared_ptr<client_service>& svc_ptr) :
		_svc_ptr(svc_ptr),
		_auth(_svc_ptr->_stream_context.mqtt_context().authenticator)
	{}

	re_auth_op(re_auth_op&&) noexcept = default;
	re_auth_op(const re_auth_op&) noexcept = delete;

	using executor_type = typename client_service::executor_type;
	executor_type get_executor() const noexcept {
		return _svc_ptr->get_executor();
	}

	using allocator_type = asio::recycling_allocator<void>;
	allocator_type get_allocator() const noexcept {
		return allocator_type {};
	}

	void perform() {
		if (_auth.method().empty())
			return;

		auto auth_step = auth_step_e::client_initial;
		return _auth.async_auth(
			auth_step, "",
			asio::prepend(std::move(*this), on_auth_data {}, auth_step)
		);
	}

	void perform(decoders::auth_message auth_message) {
		if (_auth.method().empty())
			return on_auth_fail(
				"Unexpected AUTH received",
				disconnect_rc_e::protocol_error
			);

		const auto& [rc, props] = auth_message;
		auto auth_rc = to_reason_code<reason_codes::category::auth>(rc);
		if (!auth_rc.has_value())
			return on_auth_fail(
				"Malformed AUTH received: bad reason code",
				disconnect_rc_e::malformed_packet
			);

		const auto& server_auth_method = props[prop::authentication_method];
		if (!server_auth_method || *server_auth_method != _auth.method())
			return on_auth_fail(
				"Malformed AUTH received: wrong authentication method",
				disconnect_rc_e::protocol_error
			);

		auto auth_step = auth_rc == reason_codes::success ?
			auth_step_e::server_final : auth_step_e::server_challenge;
		auto data = props[prop::authentication_data].value_or("");

		return _auth.async_auth(
			auth_step, std::move(data),
			asio::prepend(std::move(*this), on_auth_data {}, auth_step)
		);
	}

	void operator()(
		on_auth_data, auth_step_e auth_step, error_code ec, std::string data
	) {
		if (ec)
			return on_auth_fail(
				"Re-authentication: authentication fail",
				disconnect_rc_e::unspecified_error
			);

		if (auth_step == auth_step_e::server_final)
			return;

		auth_props props;
		props[prop::authentication_method] = _auth.method();
		props[prop::authentication_data] = std::move(data);
		auto rc = auth_step == auth_step_e::client_initial ?
			reason_codes::reauthenticate : reason_codes::continue_authentication;

		auto packet = control_packet<allocator_type>::of(
			no_pid, get_allocator(),
			encoders::encode_auth,
			rc.value(), props
		);

		auto wire_data = packet.wire_data();

		_svc_ptr->async_send(
			wire_data,
			no_serial, send_flag::none,
			asio::consign(asio::detached, std::move(packet))
		);
	}

private:
	void on_auth_fail(std::string message, disconnect_rc_e reason) {
		auto props = disconnect_props {};
		props[prop::reason_string] = std::move(message);

		async_disconnect(reason, props, _svc_ptr, asio::detached);
	}

};


} // end namespace async_mqtt5::detail

#endif // !ASYNC_MQTT5_RE_AUTH_OP_HPP
