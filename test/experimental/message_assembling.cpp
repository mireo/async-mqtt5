#include <boost/asio/io_context.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <async/async.h>

#include <mqtt-client/detail/internal_types.hpp>
#include <mqtt-client/codecs/message_encoders.hpp>
#include <mqtt-client/codecs/message_decoders.hpp>
		  
#include <mqtt-client/impl/assemble_op.hpp>

#include <fmt/format.h>

namespace asio = boost::asio;

namespace async_mqtt {

using byte_iter = detail::byte_iter;

class fake_stream {
	asio::any_io_executor _ex;
	std::string _data;
	int _chunk_no = -1;

	std::string _read_buff;
	detail::data_span _data_span;

public:
	fake_stream(asio::any_io_executor ex) : _ex(std::move(ex)) {
		prepare_data();
	}

	using executor_type = asio::any_io_executor;
	const executor_type& get_executor() const noexcept { return _ex; }
	
	template <
		typename BufferType, 
		typename CompletionToken
	>
	auto async_read_some(
		const BufferType& buffer, detail::duration wait_for, 
		CompletionToken&& token
	);

	template <typename Stream, typename CompletionToken>
	decltype(auto) async_assemble(
		Stream& stream, detail::duration wait_for, CompletionToken&& token
	);

private:
	void prepare_data();
	std::string_view next_frame();
};

template <
	typename BufferType, 
	typename CompletionToken
>
auto fake_stream::async_read_some(
	const BufferType& buffer, detail::duration wait_for, 
	CompletionToken&& token
) {
	auto read_op = [this] (auto handler, const BufferType& buffer) {
		auto data = next_frame();
		size_t bytes_read = data.size();
		std::copy(data.begin(), data.end(), static_cast<uint8_t*>(buffer.data()));

		asio::post(get_executor(), [h = std::move(handler), bytes_read] () mutable {
			h(error_code{}, bytes_read);
		});
	};

	return asio::async_initiate<CompletionToken, void (error_code, size_t)>(
		std::move(read_op), token, buffer
	);
}

template <typename Stream, typename CompletionToken>
decltype(auto) fake_stream::async_assemble(
	Stream& stream, detail::duration wait_for, CompletionToken&& token
) {
	auto initiation = [this] (auto handler, Stream& stream, detail::duration wait_for) mutable {
		detail::assemble_op (
			stream, std::move(handler),
			_read_buff, _data_span
		).perform(wait_for, asio::transfer_at_least(0));
	};

	return asio::async_initiate<
		CompletionToken, void (error_code, uint8_t, byte_iter, byte_iter)
	> (
		std::move(initiation), token, std::ref(stream), wait_for
	);
}

void fake_stream::prepare_data() {
	connack_props cap;
	cap[prop::session_expiry_interval] = 60;
	cap[prop::maximum_packet_size] = 16384;
	cap[prop::wildcard_subscription_available] = true;
	_data = encoders::encode_connack(true, 0x8A, cap);

	puback_props pap;
	pap[prop::user_property].emplace_back("PUBACK user property");
	_data += encoders::encode_puback(42, 28, pap);
}

std::string_view fake_stream::next_frame() {
	++_chunk_no;
	if (_chunk_no == 0)
		return { _data.begin(), _data.begin() + 2 };
	if (_chunk_no == 1)
		return { _data.begin() + 2, _data.begin() + 13 };
	if (_chunk_no == 2)
		return { _data.begin() + 13, _data.begin() + 23 };
	if (_chunk_no == 3)
		return { _data.begin() + 23, _data.begin() + 35 };
	if (_chunk_no == 4)
		return { _data.begin() + 35, _data.end() };
	return { _data.end(), _data.end() };
}    

template <typename Stream, typename CompletionToken>
decltype(auto) async_assemble(
	Stream& stream, detail::duration wait_for,
	CompletionToken&& token
) {
	return stream.async_assemble(
		stream, wait_for, std::forward<CompletionToken>(token)
	);
}


void test_single(asio::io_context& ioc) {
	using namespace std::chrono;

	fake_stream s(asio::make_strand(ioc));

	auto on_message = [] (
		error_code ec, uint8_t control_code, byte_iter first, byte_iter last
	) {
		fmt::print(stderr, "Error code: {}\n", ec.message());
		if (ec) return;
		size_t remain_length = std::distance(first, last);
		auto rv = decoders::decode_connack(control_code, remain_length, first);
		const auto& [session_present, reason_code, cap] = *rv;
		fmt::print(stderr, "Got CONNACK message, reason_code {}, session {}\n", reason_code, session_present);
		fmt::print(stderr, "session_expiry_interval: {}\n", *cap[prop::session_expiry_interval]);
		fmt::print(stderr, "maximum_packet_size: {}\n", *cap[prop::maximum_packet_size]);
		fmt::print(stderr, "wildcard_subscription_available: {}\n", *cap[prop::wildcard_subscription_available]);
	};

	async_assemble(s, seconds(1), std::move(on_message));

	ioc.run();
	ioc.restart();
}

asio::awaitable<void> test_multiple_coro(asio::io_context& ioc) {
	using namespace std::chrono;

	fake_stream s(asio::make_strand(ioc));

	auto [ec1, cc1, first1, last1] = co_await async_assemble(
		s, seconds(1), asio::use_nothrow_awaitable
	);
	size_t remain_length1 = std::distance(first1, last1);
	auto rv1 = decoders::decode_connack(cc1, remain_length1, first1);
	if (rv1)
		fmt::print(stderr, "CONNACK correctly decoded\n");

	auto [ec2, cc2, first2, last2] = co_await async_assemble(
		s, seconds(1), asio::use_nothrow_awaitable
	);
	size_t remain_length2 = std::distance(first2, last2);
	auto rv2 = decoders::decode_puback(cc2, remain_length2, first2);

	if (rv2)
		fmt::print(stderr, "PUBACK correctly decoded\n");
}

void test_multiple(asio::io_context& ioc) {
	co_spawn(ioc, test_multiple_coro(ioc), asio::detached);
	ioc.run();
	ioc.restart();
}

} // end namespace async_mqtt

void test_assembling(asio::io_context& ioc) {
	using namespace std::chrono;

	async_mqtt::test_single(ioc);
	async_mqtt::test_multiple(ioc);
}
