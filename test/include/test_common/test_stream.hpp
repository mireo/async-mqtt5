//
// Copyright (c) 2023-2024 Ivica Siladic, Bruno Iljazovic, Korina Simicevic
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef ASYNC_MQTT5_TEST_TEST_STREAM_HPP
#define ASYNC_MQTT5_TEST_TEST_STREAM_HPP

#include <boost/asio/dispatch.hpp>
#include <boost/asio/execution_context.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/prepend.hpp>
#include <boost/asio/recycling_allocator.hpp>

#include <boost/asio/ip/tcp.hpp>

#include <async_mqtt5/detail/cancellable_handler.hpp>

#include "test_common/test_broker.hpp"

namespace async_mqtt5::test {

namespace asio = boost::asio;

using error_code = boost::system::error_code;
using time_stamp = std::chrono::time_point<std::chrono::steady_clock>;
using duration = time_stamp::duration;

namespace detail {

class test_stream_impl {
public:
	using executor_type = test_broker::executor_type;
	using protocol_type = test_broker::protocol_type;
	using endpoint_type = test_broker::endpoint_type;

private:
	executor_type _ex;
	test_broker* _test_broker { nullptr };
	endpoint_type _remote_ep;

	template <typename Handler>
	friend class read_op;

	template <typename Handler>
	friend class write_op;

public:
	test_stream_impl(executor_type ex) : _ex(std::move(ex))	{}

	executor_type get_executor() const noexcept {
		return _ex;
	}

	void open(const protocol_type&, error_code& ec) {
		ec = {};
		_test_broker = &asio::use_service<test_broker>(_ex.context());
	}

	void cancel(error_code&) {}

	void close(error_code& ec) {
		disconnect();
		ec = {};
		_test_broker = nullptr;
	}

	void shutdown(asio::ip::tcp::socket::shutdown_type, error_code& ec) {
		ec = {};
	}

	void connect(const endpoint_type& ep, error_code& ec) {
		ec = {};
		_remote_ep = ep;
	}

	void disconnect() {
		_remote_ep = {};
		if (_test_broker)
			_test_broker->close_connection();
	}

	endpoint_type remote_endpoint(error_code& ec) {
		if (_remote_ep == endpoint_type {})
			ec = asio::error::not_connected;
		else
			ec = {};
		return _remote_ep;
	}

	bool is_open() const {
		return _test_broker != nullptr;
	}

	bool is_connected() const {
		return _remote_ep != endpoint_type {};
	}
};



template <typename Handler>
class read_op {
	struct on_read {};
	std::shared_ptr<test_stream_impl> _stream_impl;

	using handler_type = async_mqtt5::detail::cancellable_handler<
		Handler,
		typename test_stream_impl::executor_type
	>;
	handler_type _handler;
public:
	read_op(
		std::shared_ptr<test_stream_impl> stream_impl, Handler handler
	) :
		_stream_impl(std::move(stream_impl)),
		_handler(std::move(handler), _stream_impl->get_executor())
	{
		auto slot = asio::get_associated_cancellation_slot(_handler);
		if (slot.is_connected())
			slot.assign([stream_impl = _stream_impl](asio::cancellation_type_t) {
				stream_impl->_test_broker->cancel_pending_read();
			});
	}

	read_op(read_op&&) = default;
	read_op(const read_op&) = delete;

	using executor_type = test_stream_impl::executor_type;
	executor_type get_executor() const noexcept {
		return _stream_impl->get_executor();
	}

	using allocator_type = asio::recycling_allocator<void>;
	allocator_type get_allocator() const noexcept {
		return allocator_type {};
	}

	template <typename BufferType>
	void perform(const BufferType& buffer) {
		if (!_stream_impl->is_open() || !_stream_impl->is_connected())
			return complete_post(asio::error::not_connected, 0);

		_stream_impl->_test_broker->read_from_network(
			buffer,
			asio::prepend(std::move(*this), on_read {})
		);
	}

	void operator()(on_read, error_code ec, size_t bytes_read) {
		if (ec)
			_stream_impl->disconnect();
		complete(ec, bytes_read);
	}

private:
	void complete_post(error_code ec, size_t bytes_read) {
		_handler.complete_post(ec, bytes_read);
	}

	void complete(error_code ec, size_t bytes_read) {
		_handler.complete(ec, bytes_read);
	}
};

template <typename Handler>
class write_op {
	struct on_write {};

	std::shared_ptr<test_stream_impl> _stream_impl;
	Handler _handler;

public:
	write_op(
		std::shared_ptr<test_stream_impl> stream_impl, Handler handler
	) :
		_stream_impl(std::move(stream_impl)),
		_handler(std::move(handler))
	{}

	write_op(write_op&&) = default;
	write_op(const write_op&) = delete;

	using executor_type = test_stream_impl::executor_type;
	executor_type get_executor() const noexcept {
		return _stream_impl->get_executor();
	}

	using allocator_type = asio::recycling_allocator<void>;
	allocator_type get_allocator() const noexcept {
		return allocator_type {};
	}

	template <typename BufferType>
	void perform(const BufferType& buffers) {
		if (!_stream_impl->is_open() || !_stream_impl->is_connected())
			return complete_post(asio::error::not_connected, 0);

		_stream_impl->_test_broker->write_to_network(
			buffers,
			asio::prepend(std::move(*this), on_write {})
		);
	}

	void operator()(on_write, error_code ec, size_t bytes_written) {
		if (ec)
			_stream_impl->disconnect();
		complete(ec, bytes_written);
	}

private:
	void complete_post(error_code ec, size_t bytes_written) {
		asio::post(
			get_executor(),
			asio::prepend(std::move(_handler), ec, bytes_written)
		);
	}

	void complete(error_code ec, size_t bytes_written) {
		asio::dispatch(
			get_executor(),
			asio::prepend(std::move(_handler), ec, bytes_written)
		);
	}
};

} // end namespace detail

class test_stream {
public:
	using executor_type = test_broker::executor_type;
	using protocol_type = test_broker::protocol_type;
	using endpoint_type = test_broker::endpoint_type;

private:
	std::shared_ptr<detail::test_stream_impl> _impl;

public:
	test_stream(executor_type ex) :
		_impl(std::make_shared<detail::test_stream_impl>(std::move(ex)))
	{}

	test_stream(const test_stream&) = delete;

	~test_stream() {
		error_code ec;
		close(ec); // cancel() would be more appropriate
	}

	executor_type get_executor() const noexcept {
		return _impl->get_executor();
	}

	void open(const protocol_type& p, error_code& ec) {
		_impl->open(p, ec);
	}

	void cancel(error_code& ec) {
		_impl->cancel(ec);
	}

	void close(error_code& ec) {
		_impl->close(ec);
	}

	void connect(const endpoint_type& ep, error_code& ec) {
		_impl->connect(ep, ec);
	}

	void disconnect() {
		_impl->disconnect();
	}

	bool is_open() const {
		return _impl->is_open();
	}

	bool is_connected() const {
		return _impl->is_connected();
	}

	void shutdown(asio::ip::tcp::socket::shutdown_type st, error_code& ec) {
		return _impl->shutdown(st, ec);
	}

	endpoint_type remote_endpoint(error_code& ec) {
		return _impl->remote_endpoint(ec);
	}

	template<typename SettableSocketOption>
	void set_option(const SettableSocketOption&, error_code&) {}

	template <typename ConnectToken>
	decltype(auto) async_connect(
		const endpoint_type& ep, ConnectToken&& token
	) {

		auto initiation = [this](auto handler, const endpoint_type& ep) {
			error_code ec;
			open(asio::ip::tcp::v4(), ec);

			if (!ec)
				connect(ep, ec);

			asio::post(get_executor(), asio::prepend(std::move(handler), ec));
		};

		return asio::async_initiate<ConnectToken, void (error_code)>(
			std::move(initiation), token, ep
		);
	}

	template<typename ConstBufferSequence, typename WriteToken>
	decltype(auto) async_write_some(
		const ConstBufferSequence& buffers, WriteToken&& token
	) {
		using Signature = void (error_code, size_t);

		auto initiation = [this](
			auto handler, const ConstBufferSequence& buffers
		) {
			detail::write_op { _impl, std::move(handler) }.perform(buffers);
		};

		return asio::async_initiate<WriteToken, Signature>(
			std::move(initiation), token, buffers
		);
	}

	template<typename MutableBufferSequence, typename ReadToken>
	decltype(auto) async_read_some(
		const MutableBufferSequence& buffers,
		ReadToken&& token
	) {
		using Signature = void (error_code, size_t);

		auto initiation = [this](
			auto handler, const MutableBufferSequence& buffers
		) {
			detail::read_op { _impl, std::move(handler) }.perform(buffers);
		};

		return asio::async_initiate<ReadToken, Signature>(
			std::move(initiation), token, buffers
		);
	}

};


} // end namespace async_mqtt5::test

#endif // ASYNC_MQTT5_TEST_TEST_STREAM_HPP
