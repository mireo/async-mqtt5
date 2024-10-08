//
// Copyright (c) 2023-2024 Ivica Siladic, Bruno Iljazovic, Korina Simicevic
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef ASYNC_MQTT5_PING_OP_HPP
#define ASYNC_MQTT5_PING_OP_HPP

#include <limits>
#include <chrono>
#include <memory>

#include <boost/asio/cancellation_state.hpp>
#include <boost/asio/consign.hpp>
#include <boost/asio/prepend.hpp>
#include <boost/asio/recycling_allocator.hpp>
#include <boost/asio/steady_timer.hpp>

#include <async_mqtt5/detail/control_packet.hpp>
#include <async_mqtt5/detail/internal_types.hpp>

#include <async_mqtt5/impl/codecs/message_encoders.hpp>

namespace async_mqtt5::detail {

namespace asio = boost::asio;

template <typename ClientService, typename Executor>
class ping_op {
public:
	using executor_type = Executor;
private:
	using client_service = ClientService;

	struct on_timer {};
	struct on_pingreq {};

	std::shared_ptr<client_service> _svc_ptr;
	executor_type _executor;
	std::unique_ptr<asio::steady_timer> _ping_timer;
	asio::cancellation_state _cancellation_state;

public:
	ping_op(
		std::shared_ptr<client_service> svc_ptr,
		executor_type ex
	) :
		_svc_ptr(std::move(svc_ptr)), _executor(ex),
		_ping_timer(new asio::steady_timer(_svc_ptr->get_executor())),
		_cancellation_state(
			_svc_ptr->_cancel_ping.slot(),
			asio::enable_total_cancellation {},
			asio::enable_total_cancellation {}
		)
	{}

	ping_op(ping_op&&) noexcept = default;
	ping_op(const ping_op&) = delete;

	ping_op& operator=(ping_op&&) noexcept = default;
	ping_op& operator=(const ping_op&) = delete;

	using allocator_type = asio::recycling_allocator<void>;
	allocator_type get_allocator() const noexcept {
		return allocator_type {};
	}

	using cancellation_slot_type = asio::cancellation_slot;
	cancellation_slot_type get_cancellation_slot() const noexcept {
		return _cancellation_state.slot();
	}

	executor_type get_executor() const noexcept {
		return _executor;
	}

	void perform() {
		_ping_timer->expires_after(compute_wait_time());
		_ping_timer->async_wait(
			asio::prepend(std::move(*this), on_timer {})
		);
	}

	void operator()(on_timer, error_code) {
		if (
			_cancellation_state.cancelled() == asio::cancellation_type::terminal ||
			!_svc_ptr->is_open()
		)
			return;
		else if (_cancellation_state.cancelled() == asio::cancellation_type::total) {
			_cancellation_state.clear();
			return perform();
		}

		auto pingreq = control_packet<allocator_type>::of(
			no_pid, get_allocator(), encoders::encode_pingreq
		);

		auto wire_data = pingreq.wire_data();
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
		if (
			_cancellation_state.cancelled() == asio::cancellation_type::terminal ||
			ec == asio::error::no_recovery
		)
			return;

		_cancellation_state.clear();
		perform();
	}

private:
	duration compute_wait_time() const {
		auto negotiated_ka = _svc_ptr->negotiated_keep_alive();
		return negotiated_ka ?
			std::chrono::seconds(negotiated_ka) :
			duration(std::numeric_limits<duration::rep>::max());
	}
};


} // end namespace async_mqtt5::detail

#endif // !ASYNC_MQTT5_PING_OP_HPP
