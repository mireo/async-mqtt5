#ifndef ASYNC_MQTT5_ASYNC_MUTEX_HPP
#define ASYNC_MQTT5_ASYNC_MUTEX_HPP

#include <boost/asio/any_completion_handler.hpp>
#include <boost/asio/associated_allocator.hpp>
#include <boost/asio/associated_cancellation_slot.hpp>
#include <boost/asio/bind_cancellation_slot.hpp>
#include <boost/asio/execution.hpp>

#include <async_mqtt5/detail/async_traits.hpp>
#include <async_mqtt5/detail/ring_buffer.hpp>
#include <async_mqtt5/detail/spinlock.hpp>

namespace async_mqtt5::detail {

namespace asio = boost::asio;
using error_code = boost::system::error_code;

class async_mutex {
public:
	using executor_type = asio::any_io_executor;
private:
	using queued_op_t = asio::any_completion_handler<
		void (error_code)
	>;
	using queue_t = ring_buffer<queued_op_t>;

	// Handler with assigned tracking executor.
	// Objects of this type are type-erased by any_completion_handler
	// and stored in the waiting queue.
	template <typename Handler, typename Executor>
	class tracked_op {
		tracking_type<Handler, Executor> _executor;
		Handler _handler;
	public:
		tracked_op(Handler&& h, const Executor& ex) :
			_executor(tracking_executor(h, ex)),
			_handler(std::move(h))
		{}

		tracked_op(tracked_op&&) noexcept = default;
		tracked_op(const tracked_op&) = delete;

		using executor_type = tracking_type<Handler, Executor>;
		executor_type get_executor() const noexcept {
			return _executor;
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

		void operator()(error_code ec) {
			std::move(_handler)(ec);
		}
	};

	// Per-operation cancellation helper.
	// It is safe to emit the cancellation signal from any thread
	// provided there are no other concurrent calls to the async_mutex.
	// The helper stores queue iterator to operation since the iterator
	// would not be invalidated by other queue operations.
	class cancel_waiting_op {
		async_mutex& _owner;
		queue_t::iterator _ihandler;
	public:
		cancel_waiting_op(async_mutex& owner, queue_t::iterator ih) :
			_owner(owner), _ihandler(ih)
		{}

		void operator()(asio::cancellation_type_t type) {
			if (type == asio::cancellation_type_t::none)
				return;
			std::unique_lock l { _owner._thread_mutex };
			if (*_ihandler) {
				auto h = std::move(*_ihandler);
				auto ex = asio::get_associated_executor(h);
				l.unlock();
				asio::require(ex, asio::execution::blocking.possibly)
					.execute([h = std::move(h)]() mutable {
						std::move(h)(asio::error::operation_aborted);
					});
			}
		}
	};

	spinlock _thread_mutex;
	std::atomic<bool> _locked { false };
	queue_t _waiting;
	executor_type _ex;

public:
	template <typename Executor>
	async_mutex(Executor&& ex) : _ex(std::forward<Executor>(ex)) {}

	async_mutex(const async_mutex&) = delete;
	async_mutex& operator=(const async_mutex&) = delete;

	~async_mutex() {
		cancel();
	}

	const executor_type& get_executor() const noexcept {
		return _ex;
	}

	bool is_locked() const noexcept {
		return _locked.load(std::memory_order_relaxed);
	}

	// Schedules mutex for lock operation and return immediately.
	// Calls given completion handler when mutex is locked.
	// It's the responsibility of the completion handler to unlock the mutex.
	template <typename CompletionToken>
	decltype(auto) lock(CompletionToken&& token) noexcept {
		using Signature = void (error_code);

		auto initiation = [] (auto handler, async_mutex& self) {
			self.execute_or_queue(std::move(handler));
		};

		return asio::async_initiate<CompletionToken, Signature>(
			initiation, token, std::ref(*this)
		);
	}

	// Unlocks the mutex. The mutex must be in locked state.
	// Next queued operation, if any, will be executed in a manner
	// equivalent to asio::post.
	void unlock() {
		std::unique_lock l { _thread_mutex };
		if (_waiting.empty()) {
			_locked.store(false, std::memory_order_release);
			return;
		}
		while (!_waiting.empty()) {
			auto op = std::move(_waiting.front());
			_waiting.pop_front();
			if (!op) continue;
			op.get_cancellation_slot().clear();
			l.unlock();
			execute_op(std::move(op));
			break;
		}
	}

	// Cancels all outstanding operations waiting on the mutex.
	void cancel() {
		std::unique_lock l { _thread_mutex };

		while (!_waiting.empty()) {
			auto op = std::move(_waiting.front());
			_waiting.pop_front();
			if (!op) continue;
			op.get_cancellation_slot().clear();
			asio::require(_ex, asio::execution::blocking.never)
				.execute([ex = _ex, op = std::move(op)]() mutable {
					auto opex = asio::get_associated_executor(op, ex);
					opex.execute(
						[op = std::move(op)]() mutable {
							op(asio::error::operation_aborted);
						}
					);
				});
		}
	}

private:

	// Schedule operation to `opex` executor using `_ex` executor.
	// The operation is equivalent to asio::post(_ex, op) but
	// for some reason this form of execution is much faster.
	void execute_op(queued_op_t op) {
		asio::require(_ex, asio::execution::blocking.never)
			.execute([ex = _ex, op = std::move(op)]() mutable {
				auto opex = asio::get_associated_executor(op, ex);
				opex.execute(
					[op = std::move(op)]() mutable {
						op(error_code {});
					}
				);
			});
	}

	// Executes operation immediately if mutex is not locked
	// or queues it for later execution otherwise. In both cases
	// the operation will be executed in a manner equivalent
	// to asio::post to avoid recursion.
	template <typename Handler>
	void execute_or_queue(Handler&& handler) noexcept {
		std::unique_lock l { _thread_mutex };
		tracked_op h { std::move(handler), _ex };
		if (_locked.load(std::memory_order_relaxed)) {
			_waiting.emplace_back(std::move(h));
			auto slot = _waiting.back().get_cancellation_slot();
			if (slot.is_connected())
				slot.template emplace<cancel_waiting_op>(
					*this, _waiting.end() - 1
				);
		}
		else {
			_locked.store(true, std::memory_order_release);
			l.unlock();
			execute_op(queued_op_t { std::move(h) });
		}
	}
};

} // end namespace async_mqtt5::detail

#endif // !ASYNC_MQTT5_ASYNC_MUTEX_HPP
