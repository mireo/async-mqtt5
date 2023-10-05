#ifndef ASYNC_MQTT5_WRITE_OP_HPP
#define ASYNC_MQTT5_WRITE_OP_HPP

#include <boost/asio/prepend.hpp>
#include <boost/asio/write.hpp>
#include <boost/asio/dispatch.hpp>

#include <async_mqtt5/detail/internal_types.hpp>
#include <async_mqtt5/detail/async_traits.hpp>

namespace async_mqtt5::detail {

template <typename Owner, typename Handler>
class write_op {
	struct on_write_locked {};
	struct on_write {};
	struct on_reconnect {};

	Owner& _owner;
	std::decay_t<Handler> _handler;

public:
	write_op(
		Owner& owner, Handler&& handler) :
		_owner(owner),
		_handler(std::forward<Handler>(handler))
	{}

	write_op(write_op&&) noexcept = default;
	write_op(const write_op&) = delete;

	using executor_type = typename Owner::executor_type;
	executor_type get_executor() const noexcept {
		return _owner.get_executor();
	}

	using allocator_type = asio::associated_allocator_t<Handler>;
	allocator_type get_allocator() const noexcept {
		return asio::get_associated_allocator(_handler);
	}

	template <typename BufferType>
	void perform(BufferType& buffer) {
		auto stream_ptr = _owner._stream_ptr;
		if (_owner.was_connected())
			// note: write operation should not be time-limited
			detail::async_write(
				*stream_ptr, buffer,
				asio::prepend(std::move(*this), on_write {}, stream_ptr)
			);
		else
			(*this)(on_write {}, stream_ptr, asio::error::not_connected, 0);
	}

	void operator()(
		on_write, typename Owner::stream_ptr stream_ptr,
		error_code ec, size_t bytes_written
	) {
		if (!_owner.is_open())
			return complete(asio::error::operation_aborted, 0);

		if (!ec)
			return complete(ec, bytes_written);

		// websocket returns operation_aborted if disconnected
		if (should_reconnect(ec) || ec == asio::error::operation_aborted)
			return _owner.async_reconnect(
				stream_ptr, asio::prepend(std::move(*this), on_reconnect {})
			);

		return complete(asio::error::no_recovery, 0);
	}

	void operator()(on_reconnect, error_code ec) {
		if ((ec == asio::error::operation_aborted && _owner.is_open()) || !ec)
			ec = asio::error::try_again;

		return complete(ec, 0);
	}

private:
	void complete(error_code ec, size_t bytes_written) {
		asio::dispatch(
			get_executor(),
			asio::prepend(std::move(_handler), ec, bytes_written)
		);
	}

	static bool should_reconnect(error_code ec) {
		using namespace asio::error;
		return ec == connection_aborted || ec == not_connected
			|| ec == timed_out || ec == connection_reset
			|| ec == broken_pipe || ec == asio::error::eof;
	}

};

} // end namespace async_mqtt5::detail

#endif // !ASYNC_MQTT5_WRITE_OP_HPP
