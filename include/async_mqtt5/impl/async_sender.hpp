#ifndef ASYNC_MQTT5_ASYNC_SENDER_HPP
#define ASYNC_MQTT5_ASYNC_SENDER_HPP

#include <boost/asio/any_completion_handler.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/prepend.hpp>

#include <boost/asio/ip/tcp.hpp>

#include <async_mqtt5/detail/internal_types.hpp>

namespace async_mqtt5::detail {

namespace asio = boost::asio;

class write_req {
	static constexpr unsigned SERIAL_BITS = sizeof(serial_num_t) * 8;

	asio::const_buffer _buffer;
	serial_num_t _serial_num;
	unsigned _flags;
	asio::any_completion_handler<void (error_code)> _handler;

public:
	write_req(
		asio::const_buffer buffer,
		serial_num_t serial_num, unsigned flags,
		asio::any_completion_handler<void (error_code)> handler
	) : _buffer(buffer), _serial_num(serial_num), _flags(flags),
		_handler(std::move(handler)) {}

	static serial_num_t next_serial_num(serial_num_t last) {
		return last + 1;
	}

	asio::const_buffer buffer() const { return _buffer; }
	void complete(error_code ec) { std::move(_handler)(ec); }
	bool throttled() const { return _flags & send_flag::throttled; }
	bool terminal() const { return _flags & send_flag::terminal; }

	bool operator<(const write_req& other) const {
		if (prioritized() != other.prioritized()) {
			return prioritized();
		}

		auto s1 = _serial_num;
		auto s2 = other._serial_num;

		if (s1 < s2)
			return (s2 - s1) < (1 << (SERIAL_BITS - 1));
		return (s1 - s2) >= (1 << (SERIAL_BITS - 1));
	}

private:
	bool prioritized() const { return _flags & send_flag::prioritized; }
};

template <typename ClientService>
class async_sender {
	using client_service = ClientService;

	using queue_allocator_type = asio::recycling_allocator<write_req>;
	using write_queue_t = std::vector<write_req, queue_allocator_type>;

	ClientService& _svc;
	write_queue_t _write_queue;
	bool _write_in_progress { false };

	static constexpr uint16_t MAX_LIMIT = 65535;
	uint16_t _limit { MAX_LIMIT };
	uint16_t _quota { MAX_LIMIT };

	serial_num_t _last_serial_num { 0 };

public:
	explicit async_sender(ClientService& svc) : _svc(svc) {}

	using executor_type = typename client_service::executor_type;
	executor_type get_executor() const noexcept {
		return _svc.get_executor();
	}

	using allocator_type = queue_allocator_type;
	allocator_type get_allocator() const noexcept {
		return allocator_type {};
	}

	serial_num_t next_serial_num() {
		return _last_serial_num = write_req::next_serial_num(_last_serial_num);
	}

	template <typename CompletionToken, typename BufferType>
	decltype(auto) async_send(
		const BufferType& buffer,
		serial_num_t serial_num, unsigned flags,
		CompletionToken&& token
	) {
		auto initiation = [this](
			auto handler, const BufferType& buffer,
			serial_num_t serial_num, unsigned flags
		) {
			_write_queue.emplace_back(
				asio::buffer(buffer), serial_num, flags, std::move(handler)
			);
			do_write();
		};

		return asio::async_initiate<CompletionToken, void (error_code)>(
			std::move(initiation), token, buffer, serial_num, flags
		);
	}

	void cancel() {
		auto ops = std::move(_write_queue);
		for (auto& op : ops)
			op.complete(asio::error::operation_aborted);
	}

	void resend() {
		if (_write_in_progress)
			return;

		// The _write_in_progress flag is set to true to prevent any write 
		// operations executing before the _write_queue is filled with 
		// all the packets that require resending.
		_write_in_progress = true;

		auto new_limit = _svc._stream_context.connack_prop(prop::receive_maximum);
		_limit = new_limit.value_or(MAX_LIMIT);
		_quota = _limit;

		auto write_queue = std::move(_write_queue);
		_svc._replies.resend_unanswered();

		for (auto& op : write_queue)
			op.complete(asio::error::try_again);

		std::stable_sort(_write_queue.begin(), _write_queue.end());

		_write_in_progress = false;
		do_write();
	}

	void operator()(write_queue_t write_queue, error_code ec, size_t) {
		_write_in_progress = false;

		if (ec == asio::error::try_again) {
			_svc.update_session_state();
			_write_queue.insert(
				_write_queue.begin(),
				std::make_move_iterator(write_queue.begin()),
				std::make_move_iterator(write_queue.end())
			);
			return resend();
		}

		// errors, if any, are propagated to ops
		for (auto& op : write_queue)
			op.complete(ec);

		if (
			ec == asio::error::operation_aborted ||
			ec == asio::error::no_recovery
		)
			return;

		do_write();
	}

	void throttled_op_done() {
		if (_limit == MAX_LIMIT)
			return;

		++_quota;
		do_write();
	}

private:
	void do_write() {
		if (_write_in_progress || _write_queue.empty())
			return;

		_write_in_progress = true;

		write_queue_t write_queue;

		auto terminal_req = std::find_if(
			_write_queue.begin(), _write_queue.end(),
			[](const auto& op) { return op.terminal(); }
		);
		if (terminal_req != _write_queue.end()) {
			write_queue.push_back(std::move(*terminal_req));
			_write_queue.erase(terminal_req);
		}
		else if (_limit == MAX_LIMIT)
			write_queue = std::move(_write_queue);
		else {
			auto throttled_ptr = std::stable_partition(
				_write_queue.begin(), _write_queue.end(),
				[](const auto& op) { return !op.throttled(); }
			);
			uint16_t dist = std::distance(throttled_ptr, _write_queue.end());
			uint16_t throttled_num = std::min(dist, _quota);
			_quota -= throttled_num;
			throttled_ptr += throttled_num;

			if (throttled_ptr == _write_queue.begin()) {
				_write_in_progress = false;
				return;
			}

			write_queue.insert(
				write_queue.end(),
				std::make_move_iterator(_write_queue.begin()),
				std::make_move_iterator(throttled_ptr)
			);
			_write_queue.erase(_write_queue.begin(), throttled_ptr);
		}

		std::vector<asio::const_buffer> buffers;
		buffers.reserve(write_queue.size());
		for (const auto& op : write_queue)
			buffers.push_back(op.buffer());

		_svc._replies.clear_fast_replies();

		_svc._stream.async_write(
			buffers,
			asio::prepend(std::ref(*this), std::move(write_queue))
		);
	}

};

} // end namespace async_mqtt5::detail

#endif // !ASYNC_MQTT5_ASYNC_SENDER_HPP
