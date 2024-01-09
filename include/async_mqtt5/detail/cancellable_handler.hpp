#ifndef ASYNC_MQTT5_CANCELLABLE_HANDLER_HPP
#define ASYNC_MQTT5_CANCELLABLE_HANDLER_HPP

#include <boost/asio/associated_allocator.hpp>
#include <boost/asio/associated_executor.hpp>
#include <boost/asio/associated_cancellation_slot.hpp>
#include <boost/asio/cancellation_state.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/prepend.hpp>

#include <async_mqtt5/detail/async_traits.hpp>

namespace async_mqtt5::detail {

template <typename Handler, typename Executor>
class cancellable_handler {
	Executor _executor;
	Handler _handler;
	tracking_type<Handler, Executor> _handler_ex;
	asio::cancellation_state _cancellation_state;

public:
	cancellable_handler(Handler&& handler, const Executor& ex) :
		_executor(ex),
		_handler(std::move(handler)),
		_handler_ex(tracking_executor(_handler, ex)),
		_cancellation_state(
			asio::get_associated_cancellation_slot(_handler),
			asio::enable_total_cancellation {},
			asio::enable_terminal_cancellation {}
		)
	{}

	cancellable_handler(cancellable_handler&& other) noexcept = default;
	cancellable_handler(const cancellable_handler&) = delete;

	using executor_type = tracking_type<Handler, Executor>;
	executor_type get_executor() const noexcept {
		return _handler_ex;
	}

	using allocator_type = asio::associated_allocator_t<Handler>;
	allocator_type get_allocator() const noexcept {
		return asio::get_associated_allocator(_handler);
	}

	using cancellation_slot_type = asio::associated_cancellation_slot_t<Handler>;
	cancellation_slot_type get_cancellation_slot() const noexcept {
		return _cancellation_state.slot();
	}

	asio::cancellation_type_t cancelled() const {
		return _cancellation_state.cancelled();
	}

	template <typename... Args>
	void complete(Args&&... args) {
		asio::get_associated_cancellation_slot(_handler).clear();
		std::move(_handler)(std::forward<Args>(args)...);
	}

	template <typename... Args>
	void complete_post(Args&&... args) {
		asio::get_associated_cancellation_slot(_handler).clear();
		asio::post(
			_executor,
			asio::prepend(std::move(_handler), std::forward<Args>(args)...)
		);
	}

};

} // end async_mqtt5::detail

#endif // !ASYNC_MQTT5_CANCELLABLE_HANDLER_HPP
