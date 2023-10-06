#ifndef ASYNC_MQTT5_CONNECT_OP_HPP
#define ASYNC_MQTT5_CONNECT_OP_HPP

#include <boost/asio/append.hpp>
#include <boost/asio/consign.hpp>
#include <boost/asio/dispatch.hpp>
#include <boost/asio/prepend.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/write.hpp>

#include <boost/beast/websocket.hpp>

#include <boost/asio/ip/tcp.hpp>

#include <async_mqtt5/error.hpp>

#include <async_mqtt5/detail/async_traits.hpp>
#include <async_mqtt5/detail/control_packet.hpp>
#include <async_mqtt5/detail/internal_types.hpp>

#include <async_mqtt5/impl/internal/codecs/base_decoders.hpp>
#include <async_mqtt5/impl/internal/codecs/message_decoders.hpp>
#include <async_mqtt5/impl/internal/codecs/message_encoders.hpp>

namespace async_mqtt5::detail {

template <
	typename Stream, typename Handler
>
class connect_op {
	struct on_connect {};
	struct on_tls_handshake {};
	struct on_ws_handshake {};
	struct on_send_connect {};
	struct on_fixed_header {};
	struct on_read_connack {};

	Stream& _stream;
	mqtt_context& _ctx;
	std::decay_t<Handler> _handler;
	std::unique_ptr<std::string> _buffer_ptr;

	using endpoint = asio::ip::tcp::endpoint;
	using epoints = asio::ip::tcp::resolver::results_type;

public:
	connect_op(
		Stream& stream, Handler&& handler, mqtt_context& ctx
	) :
		_stream(stream), _ctx(ctx),
		_handler(std::move(handler))
	{}

	connect_op(connect_op&&) noexcept = default;
	connect_op(const connect_op&) = delete;

	using executor_type = typename Stream::executor_type;
	executor_type get_executor() const noexcept {
		return _stream.get_executor();
	}

	using allocator_type = asio::associated_allocator_t<Handler>;
	allocator_type get_allocator() const noexcept {
		return asio::get_associated_allocator(_handler);
	}

	using cancellation_slot_type =
		asio::associated_cancellation_slot_t<Handler>;
	cancellation_slot_type get_cancellation_slot() const noexcept {
		return asio::get_associated_cancellation_slot(_handler);
	}

	void perform(
		const epoints& eps, authority_path ap
	) {
		lowest_layer(_stream).async_connect(
			*std::begin(eps),
			asio::append(
				asio::prepend(std::move(*this), on_connect {}),
				*std::begin(eps), std::move(ap)
			)
		);
	}

	void operator()(
		on_connect, error_code ec, endpoint ep, authority_path ap
	) {
		if (ec)
			return complete(ec);

		do_tls_handshake(std::move(ep), std::move(ap));
	}

	void do_tls_handshake(endpoint ep, authority_path ap) {
		if constexpr (has_tls_handshake<Stream>) {
			_stream.async_handshake(
				tls_handshake_type<Stream>::client,
				asio::append(
					asio::prepend(std::move(*this), on_tls_handshake {}),
					std::move(ep), std::move(ap)
				)
			);
		}
		else if constexpr (has_tls_handshake<typename next_layer_type<Stream>::type>) {
			_stream.next_layer().async_handshake(
				tls_handshake_type<typename next_layer_type<Stream>::type>::client,
				asio::append(
					asio::prepend(std::move(*this), on_tls_handshake {}),
					std::move(ep), std::move(ap)
				)
			);
		}
		else
			do_ws_handshake(std::move(ep), std::move(ap));
	}

	void operator()(
		on_tls_handshake, error_code ec,
		endpoint ep, authority_path ap
	) {
		if (ec)
			return complete(ec);

		do_ws_handshake(std::move(ep), std::move(ap));
	}

	void do_ws_handshake(endpoint ep, authority_path ap) {
		if constexpr (has_ws_handshake<Stream>) {
			using namespace boost::beast;

			// We'll need to turn off read timeouts on the underlying stream
			// because the websocket stream has its own timeout system.

			// Set suggested timeout settings for the websocket
			_stream.set_option(
				websocket::stream_base::timeout::suggested(role_type::client)
			);

			_stream.binary(true);

			// Set a decorator to change the User-Agent of the handshake
			_stream.set_option(websocket::stream_base::decorator(
				[](websocket::request_type& req) {
					req.set(http::field::sec_websocket_protocol, "mqtt");
					req.set(http::field::user_agent, "boost.mqtt");
				})
			);

			_stream.async_handshake(
				ap.host + ':' + ap.port, ap.path,
				asio::prepend(std::move(*this), on_ws_handshake {})
			);
		}
		else
			send_connect();
	}

	void operator()(on_ws_handshake, error_code ec) {
		if (ec)
			return complete(ec);

		send_connect();
	}

	void send_connect() {
		auto packet = control_packet<allocator_type>::of(
			no_pid, get_allocator(),
			encoders::encode_connect,
			_ctx.credentials.client_id,
			_ctx.credentials.username, _ctx.credentials.password,
			10u, false, _ctx.co_props, _ctx.will
		);

		const auto& wire_data = packet.wire_data();

		detail::async_write(
			_stream, asio::buffer(wire_data),
			asio::consign(
				asio::prepend(std::move(*this), on_send_connect{}),
				std::move(packet)
			)
		);
	}

	void operator()(on_send_connect, error_code ec, size_t) {
		if (ec)
			return complete(ec);

		constexpr size_t min_connack_sz = 5;
		_buffer_ptr = std::make_unique<std::string>(min_connack_sz, 0);

		auto buff = asio::buffer(_buffer_ptr->data(), min_connack_sz);
		asio::async_read(
			_stream, buff,
			asio::prepend(std::move(*this), on_fixed_header {})
		);
	}

	void operator()(
		on_fixed_header, error_code ec, size_t num_read
	) {
		if (ec)
			return complete(ec);

		auto control_byte = (*_buffer_ptr)[0];
		if (control_byte != 0b00100000)
			return complete(asio::error::try_again);

		auto varlen_ptr = _buffer_ptr->cbegin() + 1;
		auto varlen = decoders::type_parse(
			varlen_ptr, _buffer_ptr->cend(), decoders::basic::varint_
		);
		if (!varlen)
			complete(asio::error::try_again);

		auto varlen_sz = std::distance(_buffer_ptr->cbegin() + 1, varlen_ptr);
		auto remain_len = *varlen -
			std::distance(varlen_ptr, _buffer_ptr->cbegin() + num_read);

		_buffer_ptr->resize(_buffer_ptr->size() + remain_len);

		auto buff = asio::buffer(_buffer_ptr->data() + num_read, remain_len);
		auto first = _buffer_ptr->cbegin() + varlen_sz + 1;
		auto last = first + *varlen;

		asio::async_read(
			_stream, buff,
			asio::prepend(
				asio::append(
					std::move(*this), uint8_t(control_byte), first, last
				), on_read_connack {}
			)
		);
	}

	void operator()(
		on_read_connack, error_code ec, size_t, uint8_t control_code,
		byte_citer first, byte_citer last
	) {
		if (ec)
			return complete(ec);

		auto packet_length = std::distance(first, last);
		auto rv = decoders::decode_connack(packet_length, first);
		const auto& [session_present, reason_code, ca_props] = *rv;

		_ctx.ca_props = ca_props;

		// TODO: session_present logic
		//  Unexpected result handling:
		//  - If we don't have a Session State, and we get session_present = true,
		//	  we must close the network connection (and restart with a clean start)
		//  - If we have a Session State, and we get session_present = false,
		//	  we must discard our Session State

		auto rc = to_reason_code<reason_codes::category::connack>(reason_code);
		if (!rc.has_value()) // reason code not allowed in CONNACK
			return complete(client::error::malformed_packet);

		complete(to_asio_error(*rc));
	}

private:
	void complete(error_code ec) {
		get_cancellation_slot().clear();

		asio::dispatch(
			get_executor(),
			asio::prepend(std::move(_handler), ec)
		);
	}

	static error_code to_asio_error(reason_code rc) {
		using namespace boost::asio::error;
		using namespace reason_codes;

		if (rc == success)
			return {};

		if (rc == unspecified_error || rc == server_unavailable ||
			rc == server_busy || rc == connection_rate_exceeded)
			return connection_refused;

		return access_denied;
	}
};


} // end namespace async_mqtt5::detail

#endif // !ASYNC_MQTT5_CONNECT_OP_HPP
