#include <fmt/format.h>

#include <boost/asio/prepend.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/associated_executor.hpp>
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/cancellation_signal.hpp>
#include <boost/asio/cancellation_state.hpp>
#include <boost/asio/bind_cancellation_slot.hpp>
#include <boost/asio/dispatch.hpp>
#include <boost/asio/thread_pool.hpp>

namespace asio = boost::asio;

template <typename Handler>
decltype(auto) tracking_executor(Handler&& handler) {
	return asio::prefer(
		asio::get_associated_executor(std::forward<Handler>(handler)),
		asio::execution::outstanding_work.tracked
	);
}

template <typename Handler>
using tracking_type = std::decay_t<
	decltype(tracking_executor(std::declval<Handler>()))
>;

template <typename Handler>
class async_op {
	struct on_timer {};

	std::decay_t<Handler> _handler;
	tracking_type<Handler> _handler_ex;
	asio::cancellation_state _cancel_state;

	// must be unique_ptr because move(timer) cancels previous op
	std::unique_ptr<asio::steady_timer> _timer;
public:
	template <typename Executor>
	async_op(const Executor& ex, Handler&& handler, const asio::cancellation_slot& cs) :
		_handler(std::forward<Handler>(handler)),
		_handler_ex(tracking_executor(_handler)),
		_cancel_state(cs),
		_timer(std::make_unique<asio::steady_timer>(ex))
	{}

	async_op(async_op&&) noexcept = default;
	async_op& operator=(async_op&&) noexcept = default;

	using executor_type = asio::steady_timer::executor_type;
	executor_type get_executor() const noexcept {
		return _timer->get_executor();
	}

	using allocator_type = asio::associated_allocator_t<Handler>;
	allocator_type get_allocator() const noexcept {
		return asio::get_associated_allocator(_handler);
	}

	using cancellation_slot_type = asio::cancellation_slot;
	asio::cancellation_slot get_cancellation_slot() const noexcept {
		return _cancel_state.slot();
	}

	void perform() {
		_timer->expires_from_now(std::chrono::seconds(5));
		_timer->async_wait(asio::prepend(std::move(*this), on_timer {}));
	}

	void operator()(on_timer, boost::system::error_code ec) {
		if (ec == asio::error::operation_aborted) {
			fmt::print(stderr, "Aborted {}\n", ec.message());
			return;
		}
		_cancel_state.slot().clear();

		fmt::print(stderr, "Dispatching with error {}\n", ec.message());
		asio::dispatch(_handler_ex, [h = std::move(_handler), ec]() mutable {
			std::move(h)(ec);
		});
	}
};

class owner {
	asio::cancellation_signal _cancel_signal;
public:
	void cancel() {
		_cancel_signal.emit(asio::cancellation_type::terminal);
		_cancel_signal.slot().clear();
	}
	~owner() {
		cancel();
	}

	template <typename CompletionToken>
	decltype(auto) async_perform(asio::io_context& ioc, CompletionToken&& token) {
		auto initiation = [this, &ioc](auto handler) {
			auto slot = asio::get_associated_cancellation_slot(handler);
			async_op<decltype(handler)>(
				ioc.get_executor(), std::move(handler),
				slot.is_connected() ? slot : _cancel_signal.slot()
			).perform();
		};

		return asio::async_initiate<CompletionToken, void (boost::system::error_code)>(
			std::move(initiation), token
		);
	}
};

void cancel_test(asio::io_context& ioc) {
	asio::cancellation_signal cancel_signal;

	asio::thread_pool thp;

	{
		owner b;
		b.async_perform(ioc, 
			asio::bind_cancellation_slot(
				cancel_signal.slot(),
				asio::bind_executor(thp.get_executor(),
					[](boost::system::error_code ec) {
						fmt::print(stderr, "Finished with error {}\n", ec.message());
					}
				)
			)
		);
	}

/*
	{
		asio::steady_timer timer3(ioc);
		timer3.expires_from_now(std::chrono::seconds(3));
		timer3.async_wait(
			asio::bind_cancellation_slot(
				cancel_signal.slot(),
				[&] (boost::system::error_code) {
					// cancel_signal.emit(asio::cancellation_type::terminal);
					// b.cancel();
				}
			)
		);
	}
*/
	asio::steady_timer timer2(ioc);
	timer2.expires_from_now(std::chrono::seconds(1));
	timer2.async_wait([&] (boost::system::error_code) {
		// cancel_signal.emit(asio::cancellation_type::terminal);
		// b.cancel();
	});

	ioc.run();
	thp.join();
}

