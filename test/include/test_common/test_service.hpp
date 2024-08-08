//
// Copyright (c) 2023-2024 Ivica Siladic, Bruno Iljazovic, Korina Simicevic
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef ASYNC_MQTT5_TEST_TEST_SERVICE_HPP
#define ASYNC_MQTT5_TEST_TEST_SERVICE_HPP

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/associated_executor.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/prepend.hpp>

#include <async_mqtt5/types.hpp>

#include <async_mqtt5/impl/client_service.hpp>

namespace async_mqtt5::test {

namespace asio = boost::asio;

template <
	typename StreamType,
	typename TlsContext = std::monostate
>
class test_service : public async_mqtt5::detail::client_service<StreamType, TlsContext> {
	using error_code = boost::system::error_code;
	using base = async_mqtt5::detail::client_service<StreamType, TlsContext>;

	asio::any_io_executor _ex;
	connack_props _test_props;
public:
	explicit test_service(const asio::any_io_executor ex)
		: base(ex), _ex(ex)
	{}

	test_service(const asio::any_io_executor ex, connack_props props)
		: base(ex), _ex(ex), _test_props(std::move(props))
	{}

	template <typename BufferType, typename CompletionToken>
	decltype(auto) async_send(
		const BufferType&, uint32_t, unsigned,
		CompletionToken&& token
	) {
		auto initiation = [this](auto handler) {
			asio::post(_ex,
				asio::prepend(std::move(handler), error_code {})
			);
		};

		return asio::async_initiate<
			CompletionToken, void (error_code)
		> (std::move(initiation), token);
	}

	template <typename Prop>
	const auto& connack_property(Prop p) const {
		return _test_props[p];
	}

	const auto& connack_properties() {
		return _test_props;
	}

};


template <
	typename StreamType,
	typename TlsContext = std::monostate
>
class overrun_client : public async_mqtt5::detail::client_service<StreamType, TlsContext> {
public:
	explicit overrun_client(const asio::any_io_executor& ex) :
		async_mqtt5::detail::client_service<StreamType, TlsContext>(ex)
	{}

	uint16_t allocate_pid() {
		return 0;
	}
};


} // end namespace async_mqtt5::test

#endif // ASYNC_MQTT5_TEST_TEST_SERVICE_HPP
