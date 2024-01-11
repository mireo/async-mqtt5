#ifndef ASYNC_MQTT5_TEST_TEST_BROKER_HPP
#define ASYNC_MQTT5_TEST_TEST_BROKER_HPP

#include <memory>
#include <numeric>
#include <string>
#include <vector>

#include <boost/asio/any_completion_handler.hpp>
#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/cancellation_signal.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/prepend.hpp>

#include <boost/asio/ip/tcp.hpp>

#include <async_mqtt5/impl/codecs/message_decoders.hpp>

#include "test_common/message_exchange.hpp"
#include "test_common/packet_util.hpp"
#include "test_common/protocol_logging.hpp"


namespace async_mqtt5::test {

namespace asio = boost::asio;
using error_code = boost::system::error_code;

class pending_read {
	void* _buffer_data { nullptr };
	size_t _buffer_size { 0 };
	asio::any_completion_handler<void (error_code, size_t)> _handler {};

public:
	template <typename MutableBuffer, typename Handler>
	pending_read(const MutableBuffer& buffer, Handler&& handler) :
		_buffer_data(asio::buffer_cast<void*>(buffer)),
		_buffer_size(buffer.size()),
		_handler(std::move(handler))
	{}

	pending_read() = default;
	pending_read(pending_read&&) = default;
	pending_read& operator=(pending_read&&) = default;

	size_t consume(const std::vector<uint8_t>& data) {
		size_t num_bytes = std::min(_buffer_size, data.size());
		std::memcpy(_buffer_data, data.data(), num_bytes);
		return num_bytes;
	}

	template <typename Executor>
	void complete(const Executor& ex, error_code ec, size_t bytes_read) {
		if (empty())
			return;
		if (ec || bytes_read || _buffer_size == 0)
			asio::post(ex, asio::prepend(std::move(_handler), ec, bytes_read));
	}

	constexpr bool empty() const {
		return !_handler;
	}
};

class test_broker : public asio::execution_context::service {
public:
	using executor_type = asio::any_io_executor;
	using protocol_type = asio::ip::tcp;
	using endpoint_type = asio::ip::tcp::endpoint;

	static inline asio::execution_context::id id {};

private:
	using base = asio::execution_context::service;

	struct on_receive {};
	struct on_delayed_complete {};

	struct broker_data {
		error_code ec;
		std::vector<uint8_t> bytes;
	};

	executor_type _ex;
	std::deque<broker_data> _broker_data;
	pending_read _pending_read;

	msg_exchange _broker_side;
	std::vector<std::unique_ptr<asio::cancellation_signal>> _cancel_signals;

public:
	test_broker(
		asio::execution_context& context,
		asio::any_io_executor ex = {}, msg_exchange broker_side = {}
	) :
		base(context), _ex(std::move(ex)), _broker_side(std::move(broker_side))
	{
		launch_broker_ops();
	}

	test_broker(const test_broker&) = delete;
	void operator=(const test_broker&) = delete;

	executor_type get_executor() const noexcept {
		return _ex;
	}

	void close_connection() {
		_pending_read.complete(
			get_executor(), asio::error::operation_aborted, 0
		);

		for (auto& cs : _cancel_signals)
			cs->emit(asio::cancellation_type::terminal);

		_broker_data.clear();
	}

	bool received_all_expected() {
		return !_broker_side.pop_reply_action().has_value();
	}

	template <typename ConstBufferSequence, typename WriteToken>
	decltype(auto) write_to_network(
		const ConstBufferSequence& buffers,
		WriteToken&& token
	) {
		auto initiation = [this](
			auto handler, const ConstBufferSequence& buffers
		) {
			auto reply_action = _broker_side.pop_reply_action();

			size_t bytes_written = std::accumulate(
				std::begin(buffers), std::end(buffers), size_t(0),
				[](size_t a, const auto& b) { return a + b.size(); }
			);

			executor_type ex = get_executor();

			if (reply_action) {
				const auto& expected = reply_action->expected_packets();

				size_t buffers_size = std::distance(
					buffers.begin(), buffers.end()
				);
				BOOST_CHECK_EQUAL(buffers_size, expected.size());

				size_t num_packets = std::min(buffers_size, expected.size());
				auto it = buffers.begin();
				for (size_t i = 0; i < num_packets; ++i, ++it) {
					BOOST_CHECK_EQUAL(it->size(), expected[i].size());
					size_t len = std::min(it->size(), expected[i].size());
					if (memcmp(it->data(), expected[i].data(), len)) {
						std::ostringstream stream;
						stream << "Packet mismatch! Expected: "
							<< to_readable_packet(expected[i])
							<< " Received: "
							<< to_readable_packet(std::string((const char*)it->data(), len));
						log(stream.str());
					}
				}
			}

			auto complete_op = reply_action ?
				reply_action->write_completion(ex) :
				delayed_op<error_code>(ex, 0ms, error_code {});

			async_delay(
				make_cancel_slot(),	std::move(complete_op),
				asio::prepend(
					std::ref(*this), on_delayed_complete {},
					std::move(handler), bytes_written
				)
			);

			if (!reply_action.has_value())
				return;

			for (auto& op : reply_action->pop_reply_ops(ex))
				async_delay(
					make_cancel_slot(),	std::move(op),
					asio::prepend(std::ref(*this), on_receive {})
				);
		};

		return asio::async_initiate<WriteToken, void (error_code, size_t)>(
			std::move(initiation), token, buffers
		);
	}

	template <typename MutableBuffer, typename ReadToken>
	decltype(auto) read_from_network(
		const MutableBuffer& buffer, ReadToken&& token
	) {
		auto initiation = [this](
			auto handler, const MutableBuffer& buffer
		) {
			_pending_read = pending_read(buffer, std::move(handler));
			complete_read();
		};

		return asio::async_initiate<ReadToken, void (error_code, size_t)>(
			std::move(initiation), token, buffer
		);
	}


	void operator()(
		on_receive, error_code delay_ec,
		error_code ec, std::vector<uint8_t> bytes
	) {
		remove_cancel_signal();

		if (delay_ec) // asio::operation_aborted
			return;

		_broker_data.push_back({ ec, std::move(bytes) });
		complete_read();
	}

	template <typename Handler>
	void operator()(
		on_delayed_complete, Handler handler, size_t bytes,
		error_code delay_ec, error_code ec
	) {
		remove_cancel_signal();

		if (delay_ec) { // asio::operation_aborted
			ec = delay_ec;
			bytes = 0;
		}

		asio::dispatch(asio::prepend(std::move(handler), ec, bytes));
	}

private:

	void shutdown() override { }

	void launch_broker_ops() {
		for (auto& op: _broker_side.pop_broker_ops(get_executor())) {
			async_delay(
				asio::cancellation_slot {},
				std::move(op),
				asio::prepend(std::ref(*this), on_receive {})
			);
		}
	}

	void complete_read() {
		if (_pending_read.empty())
			return;

		error_code ec = {};
		size_t bytes_read = 0;

		if (!_broker_data.empty()) {
			auto& [read_ec, bytes] = _broker_data.front();
			ec = read_ec;
			bytes_read = _pending_read.consume(bytes);

			if (bytes_read == bytes.size())
				_broker_data.pop_front();
			else
				bytes.erase(bytes.begin(), bytes.begin() + bytes_read);
		}

		_pending_read.complete(get_executor(), ec, bytes_read);
	}

	asio::cancellation_slot make_cancel_slot() {
		_cancel_signals.push_back(
			std::make_unique<asio::cancellation_signal>()
		);
		return _cancel_signals.back()->slot();
	}

	void remove_cancel_signal() {
		_cancel_signals.erase(
			std::remove_if(
				_cancel_signals.begin(), _cancel_signals.end(),
				[](auto& sig_ptr) {	return !sig_ptr->slot().has_handler(); }
			),
			_cancel_signals.end()
		);
	}
};

} // end namespace async_mqtt5::test



// Funs temporarily moved out of network service
//
//void process_packet(const std::string& packet) {
//	using enum control_code_e;
//
//	auto code = extract_code(uint8_t(*packet.data()));
//	if (code == connack)
//		determine_network_properties(packet);
//	else if (code == puback || code == pubcomp)
//		_num_outgoing_publishes--;
//}
//
//void determine_network_properties(const std::string& connack) {
//	auto begin = connack.cbegin() + 1 /* fixed header */;
//	auto _ = decoders::type_parse(begin, connack.cend(), decoders::basic::varint_);
//	auto rv = decoders::decode_connack(connack.size(), begin);
//	const auto& [session_present, reason_code, ca_props] = *rv;
//
//	if (ca_props[prop::receive_maximum])
//		_max_receive = *ca_props[prop::receive_maximum];
//	else
//		_max_receive = MAX_LIMIT;
//}
//
//void count_outgoing_publishes(
//	const std::vector<std::string>& packets
//) {
//	for (const auto& packet : packets) {
//		auto code = extract_code(uint8_t(*packet.data()));
//		if (code == control_code_e::publish) {
//			auto flags = *packet.data() & 0b00001111;
//			auto qos = extract_qos(flags);
//
//			if (qos != qos_e::at_most_once)
//				_num_outgoing_publishes++;
//
//			BOOST_ASIO_CHECK_MESSAGE(
//				_num_outgoing_publishes <= _max_receive,
//				"There are more outgoing PUBLISH packets than\
//					it is allowed by the Maxmimum Receive Limit!"
//			);
//		}
//	}
//}


// write_to_network stuff

//auto packets = to_packets(buffers);
//count_outgoing_publishes(packets);
//// TODO: this is just for debug right now
//if (!write_ec)
//	for (const auto& packet : packets)
//	test::log(to_readable_packet(packet, write_ec, false));

#endif // ASYNC_MQTT5_TEST_TEST_BROKER_HPP
