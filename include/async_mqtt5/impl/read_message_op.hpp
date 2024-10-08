//
// Copyright (c) 2023-2024 Ivica Siladic, Bruno Iljazovic, Korina Simicevic
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef ASYNC_MQTT5_READ_MESSAGE_OP_HPP
#define ASYNC_MQTT5_READ_MESSAGE_OP_HPP

#include <chrono>
#include <cstdint>
#include <memory>

#include <boost/asio/error.hpp>
#include <boost/asio/prepend.hpp>
#include <boost/asio/recycling_allocator.hpp>

#include <async_mqtt5/types.hpp>

#include <async_mqtt5/detail/control_packet.hpp>

#include <async_mqtt5/impl/disconnect_op.hpp>
#include <async_mqtt5/impl/publish_rec_op.hpp>
#include <async_mqtt5/impl/re_auth_op.hpp>

#include <async_mqtt5/impl/codecs/message_decoders.hpp>

namespace async_mqtt5::detail {

namespace asio = boost::asio;

template <typename ClientService, typename Executor>
class read_message_op {
public:
	using executor_type = Executor;
private:
	using client_service = ClientService;

	struct on_message {};
	struct on_disconnect {};

	std::shared_ptr<client_service> _svc_ptr;
	executor_type _executor;
public:
	read_message_op(
		std::shared_ptr<client_service> svc_ptr,
		executor_type ex
	) :
		_svc_ptr(std::move(svc_ptr)), _executor(ex)
	{}

	read_message_op(read_message_op&&) noexcept = default;
	read_message_op(const read_message_op&) = delete;

	read_message_op& operator=(read_message_op&&) noexcept = default;
	read_message_op& operator=(const read_message_op&) = delete;

	using allocator_type = asio::recycling_allocator<void>;
	allocator_type get_allocator() const noexcept {
		return allocator_type {};
	}

	executor_type get_executor() const noexcept {
		return _executor;
	}

	void perform() {
		_svc_ptr->async_assemble(
			asio::prepend(std::move(*this), on_message {})
		);
	}

	void operator()(
		on_message, error_code ec,
		uint8_t control_code,
		byte_citer first, byte_citer last
	) {
		if (ec == client::error::malformed_packet)
			return on_malformed_packet(
				"Malformed Packet received from the Server"
			);

		if (ec == asio::error::no_recovery)
			return _svc_ptr->cancel();

		if (ec == asio::error::operation_aborted)
			return;

		dispatch(control_code, first, last);
	}

	void operator()(on_disconnect, error_code ec) {
		if (!ec)
			perform();
	}

private:
	void dispatch(
		uint8_t control_byte,
		byte_citer first, byte_citer last
	) {
		auto code = control_code_e(control_byte & 0b11110000);

		switch (code) {
			case control_code_e::publish: {
				auto msg = decoders::decode_publish(
					control_byte, static_cast<uint32_t>(std::distance(first, last)), first
				);
				if (!msg.has_value())
					return on_malformed_packet(
						"Malformed PUBLISH received: cannot decode"
					);

				publish_rec_op { _svc_ptr }.perform(std::move(*msg));
			}
			break;
			case control_code_e::disconnect: {
				_svc_ptr->close_stream();
				_svc_ptr->open_stream();
			}
			break;
			case control_code_e::auth: {
				auto rv = decoders::decode_auth(
					static_cast<uint32_t>(std::distance(first, last)), first
				);
				if (!rv.has_value())
					return on_malformed_packet(
						"Malformed AUTH received: cannot decode"
					);

				re_auth_op { _svc_ptr }.perform(std::move(*rv));
			}
			break;
			default:
				assert(false);
		}

		perform();
	}

	void on_malformed_packet(const std::string& reason) {
		auto props = disconnect_props {};
		props[prop::reason_string] = reason;
		auto svc_ptr = _svc_ptr; // copy before this is moved

		async_disconnect(
			disconnect_rc_e::malformed_packet, props, svc_ptr,
			asio::prepend(std::move(*this), on_disconnect {})
		);
	}

};


} // end namespace async_mqtt5::detail

#endif // !ASYNC_MQTT5_READ_MESSAGE_OP_HPP
