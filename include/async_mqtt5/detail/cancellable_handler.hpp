#ifndef ASYNC_MQTT5_CANCELLABLE_HANDLER_HPP
#define ASYNC_MQTT5_CANCELLABLE_HANDLER_HPP

#include <boost/asio/cancellation_type.hpp>
#include <boost/asio/associated_allocator.hpp>
#include <boost/asio/associated_cancellation_slot.hpp>
#include <boost/asio/associated_executor.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/prepend.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/dispatch.hpp>
#include <boost/asio/bind_executor.hpp>
#include <memory>

#include <async_mqtt5/detail/async_traits.hpp>

namespace async_mqtt5::detail {

template <
	typename Handler, typename Executor,
	typename CancelArgs = std::tuple<>
>
class cancellable_handler {
	struct op_state {
		std::decay_t<Handler> _handler;
		tracking_type<Handler> _handler_ex;
		cancellable_handler* _owner;

		op_state(Handler&& handler, cancellable_handler* owner) :
			_handler(std::move(handler)),
			_handler_ex(tracking_executor(_handler)),
			_owner(owner)
		{}

		void cancel_op(asio::cancellation_type_t ct) {
			if (ct != asio::cancellation_type_t::none)
				_owner->cancel();
		}
	};

	struct cancel_proxy {
		std::weak_ptr<op_state> _state_weak_ptr;
		Executor _executor;

		cancel_proxy(std::shared_ptr<op_state> state, const Executor& ex) :
			_state_weak_ptr(std::move(state)), _executor(ex)
		{}

		void operator()(asio::cancellation_type_t type) {
			auto op = [](
				std::weak_ptr<op_state> wptr,
				asio::cancellation_type_t type
			) {
				if (auto state = wptr.lock())
					state->cancel_op(type);
			};
			asio::dispatch(
				_executor,
				asio::prepend(std::move(op), _state_weak_ptr, type)
			);
		}
	};

	std::shared_ptr<op_state> _state;
	Executor _executor;

public:
	cancellable_handler(Handler&& handler, const Executor& ex) {
		auto alloc = asio::get_associated_allocator(handler);
		_state = std::allocate_shared<op_state>(
			alloc, std::move(handler),this
		);
		_executor = ex;
		auto slot = asio::get_associated_cancellation_slot(_state->_handler);
		if (slot.is_connected())
			slot.template emplace<cancel_proxy>(_state, ex);
	}

	cancellable_handler(cancellable_handler&& other) noexcept :
		_state(std::exchange(other._state, nullptr)),
		_executor(std::move(other._executor))
	{
		if (!empty())
			_state->_owner = this;
	}

	cancellable_handler(const cancellable_handler&) = delete;

	~cancellable_handler() {
		cancel();
	}

	bool empty() const noexcept {
		return _state == nullptr;
	}

	using allocator_type = asio::associated_allocator_t<Handler>;
	allocator_type get_allocator() const noexcept {
		return asio::get_associated_allocator(_state->_handler);
	}

	void cancel() {
		if (empty()) return;

		auto h = std::move(_state->_handler);
		_state.reset();
		asio::get_associated_cancellation_slot(h).clear();

		auto op = std::apply([&h](auto... args) {
			return asio::prepend(
				std::move(h), asio::error::operation_aborted, args...
			);
		}, CancelArgs {});

		asio::dispatch(std::move(_executor), std::move(op));
	}

	template <typename... Args>
	void complete(Args&&... args) {
		if (empty()) return;

		auto h = std::move(_state->_handler);
		_state.reset();
		asio::get_associated_cancellation_slot(h).clear();

		asio::dispatch(
			std::move(_executor),
			asio::prepend(std::move(h), std::forward<Args>(args)...)
		);
	}

	template <typename... Args>
	void complete_post(Args&&... args) {
		if (empty()) return;

		auto h = std::move(_state->_handler);
		_state.reset();
		asio::get_associated_cancellation_slot(h).clear();

		asio::post(
			std::move(_executor),
			asio::prepend(std::move(h), std::forward<Args>(args)...)
		);

	}

};

} // end async_mqtt5::detail

#endif // !ASYNC_MQTT5_CANCELLABLE_HANDLER_HPP
