//
// Copyright (c) 2023-2024 Ivica Siladic, Bruno Iljazovic, Korina Simicevic
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef ASYNC_MQTT5_SENTRY_OP_HPP
#define ASYNC_MQTT5_SENTRY_OP_HPP

#include <chrono>
#include <memory>

#include <boost/asio/cancellation_signal.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/prepend.hpp>
#include <boost/asio/recycling_allocator.hpp>
#include <boost/asio/steady_timer.hpp>

#include <async_mqtt5/error.hpp>
#include <async_mqtt5/types.hpp>

#include <async_mqtt5/impl/disconnect_op.hpp>

namespace async_mqtt5::detail {

namespace asio = boost::asio;

template <typename ClientService, typename Executor>
class sentry_op {
public:
	using executor_type = Executor;
private:
	using client_service = ClientService;

	struct on_timer {};
	struct on_disconnect {};

	static constexpr auto check_interval = std::chrono::seconds(3);

	std::shared_ptr<client_service> _svc_ptr;
	executor_type _executor;
	std::unique_ptr<asio::steady_timer> _sentry_timer;

public:
	sentry_op(
		std::shared_ptr<client_service> svc_ptr, executor_type ex
	) :
		_svc_ptr(std::move(svc_ptr)), _executor(ex),
		_sentry_timer(new asio::steady_timer(_svc_ptr->get_executor()))
	{}

	sentry_op(sentry_op&&) noexcept = default;
	sentry_op(const sentry_op&) = delete;

	sentry_op& operator=(sentry_op&&) noexcept = default;
	sentry_op& operator=(const sentry_op&) = delete;

	using allocator_type = asio::recycling_allocator<void>;
	allocator_type get_allocator() const noexcept {
		return allocator_type {};
	}

	using cancellation_slot_type = asio::cancellation_slot;
	cancellation_slot_type get_cancellation_slot() const noexcept {
		return _svc_ptr->_cancel_sentry.slot();
	}

	executor_type get_executor() const noexcept {
		return _executor;
	}

	void perform() {
		_sentry_timer->expires_after(check_interval);
		_sentry_timer->async_wait(
			asio::prepend(std::move(*this), on_timer {})
		);
	}

	void operator()(on_timer, error_code ec) {
		if (ec == asio::error::operation_aborted || !_svc_ptr->is_open())
			return;

		if (_svc_ptr->_replies.any_expired()) {
			auto props = disconnect_props {};
			// TODO add what packet was expected?
			props[prop::reason_string] = "No reply received within 20 seconds";
			auto svc_ptr = _svc_ptr;
			return async_disconnect(
				disconnect_rc_e::unspecified_error, props, svc_ptr,
				asio::prepend(std::move(*this), on_disconnect {})
			);
		}

		perform();
	}

	void operator()(on_disconnect, error_code ec) {
		if (!ec)
			perform();
	}
};


} // end namespace async_mqtt5::detail

#endif // !ASYNC_MQTT5_SENTRY_OP_HPP
