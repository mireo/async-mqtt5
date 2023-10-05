// #include <async/run_loop.h>

#include <boost/asio/io_context.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/thread_pool.hpp>
#include <boost/asio/bind_cancellation_slot.hpp>
#include <thread>
#include <vector>
#include <mutex>

#include <mqtt-client/detail/async_mutex.hpp>
#include <async/mutex.h>
#include <fmt/format.h>

#define ANKERL_NANOBENCH_IMPLEMENT
#include <nanobench/nanobench.h>

constexpr size_t nthreads = 4;
constexpr size_t per_job_count = 10'000 / nthreads;

using namespace async_mqtt5::detail;

void with_async_mutex(async_mutex& mutex, std::size_t* count) {
	for (int c = 0; c < per_job_count; ++c) {
		mutex.lock([&mutex, count](error_code) {
			++(*count);
			mutex.unlock();
		});
	}
}

void test_with_async_mutex(asio::any_io_executor e, async_mutex& mutex, size_t* count) {
	asio::post(e, [&mutex, count]() {
		with_async_mutex(mutex, count);
	});
}

void with_old_async_mutex(async::mutex& mutex, std::size_t* count) {
	for (int c = 0; c < per_job_count; ++c) {
		mutex.lock([&mutex, count]() {
			++(*count);
			mutex.unlock();
		});
	}
}

void test_with_old_async_mutex(asio::any_io_executor e, async::mutex& mutex, size_t* count) {
	asio::post(e, [&mutex, count]() {
		with_old_async_mutex(mutex, count);
	});
}


struct std_mutex : public std::mutex {
	std_mutex(asio::any_io_executor) : std::mutex() { }
};

void with_mutex(std_mutex& mutex, size_t* count) {
	for (int c = 0; c < per_job_count; ++c) {
		mutex.lock();
		++(*count);
		mutex.unlock();
	}
}

void test_with_mutex(asio::any_io_executor e, std_mutex& mutex, size_t* count) {
	asio::post(e, [&mutex, count]() {
		with_mutex(mutex, count);
	});
}

template <class Mutex, class Test>
void test_with(Test func) {
	asio::thread_pool pool(nthreads);
	auto ex = pool.get_executor();
	auto count = std::make_unique<size_t>(0);
	Mutex mutex { ex };
	{
		for (auto i = 0; i < nthreads; ++i)  {
			func(ex, mutex, count.get());
		}
	}
	pool.wait();
	if (*count != nthreads * per_job_count)
		throw "greska!";
}

void test_cancellation() {

	asio::thread_pool tp;
	asio::cancellation_signal cancel_signal;

	async_mutex mutex(tp.get_executor());
	asio::steady_timer timer(tp);

	auto op = [&](error_code ec) {
		if (ec == asio::error::operation_aborted) {
			mutex.unlock();
			return;
		}
		timer.expires_from_now(std::chrono::seconds(2));
		timer.async_wait([&] (boost::system::error_code) {
			fmt::print(stderr, "Async-locked operation done\n");
			mutex.unlock();
		});
	};

	auto cancellable_op = [&](error_code ec) {
		fmt::print(
			stderr,
			"Cancellable async-locked operation finished with ec: {}\n", ec.message()
		);
		if (ec == asio::error::operation_aborted)
			return;
		mutex.unlock();
	};

	mutex.lock(std::move(op));

	mutex.lock(
		asio::bind_cancellation_slot(
			cancel_signal.slot(), std::move(cancellable_op)
		)
	);

	asio::steady_timer timer2(tp);
	timer2.expires_from_now(std::chrono::seconds(1));
	timer2.async_wait([&] (boost::system::error_code) {
		cancel_signal.emit(asio::cancellation_type::terminal);
	});

	tp.wait();
}

void test_destructor() {
	asio::thread_pool tp;
	asio::cancellation_signal cancel_signal;

	{
		async_mutex mutex(tp.get_executor());
		asio::steady_timer timer(tp);

		auto op = [&](error_code ec_mtx) {
			if (ec_mtx == asio::error::operation_aborted) {
				fmt::print(
					stderr,
					"Mutex operation cancelled error_code {}\n",
					ec_mtx.message()
				);
				mutex.unlock();
				return;
			}

			timer.expires_from_now(std::chrono::seconds(2));
			timer.async_wait([&] (boost::system::error_code ec) {
				if (ec == asio::error::operation_aborted)
					return;

				fmt::print(
					stderr,
					"Async-locked operation done with error_code {}\n",
					ec.message()
				);
				mutex.unlock();
			});
		};

		mutex.lock(std::move(op));
	}

	tp.wait();
}

void test_basics() {
	asio::thread_pool tp;

	// {
		asio::cancellation_signal cs;
		async_mutex mutex(tp.get_executor());
		auto s1 = asio::make_strand(tp.get_executor());
		auto s2 = asio::make_strand(tp.get_executor());

		mutex.lock(asio::bind_executor(s1, [&mutex, s1](boost::system::error_code ec) mutable {
			fmt::print(
				stderr,
				"Scoped-locked operation (1) done with error_code {} ({})\n",
				ec.message(),
				s1.running_in_this_thread()
			);
			if (ec != asio::error::operation_aborted)
				mutex.unlock();
		}));

		mutex.lock(
			asio::bind_cancellation_slot(
				cs.slot(),
				asio::bind_executor(s2, [s2](boost::system::error_code ec){
					fmt::print(
						stderr,
						"Scoped-locked operation (2) done with error_code {} ({})\n",
						ec.message(),
						s2.running_in_this_thread()
					);
				})
			)
		);
		cs.emit(asio::cancellation_type_t::terminal);
		cs.slot().clear();
	// }

	tp.wait();
}

void test_mutex() {
	// test_basics();
	// return;
	// test_destructor();
	// test_cancellation();
	// return;

	auto bench = ankerl::nanobench::Bench();
	bench.relative(true);

	bench.run("std::mutex", [] {
		test_with<std_mutex>(test_with_mutex);
	});

	bench.run("async_mutex", [] {
		test_with<async_mutex>(test_with_async_mutex);
	});

	bench.run("async::mutex", [] {
		test_with<async::mutex>(test_with_old_async_mutex);
	});
}
