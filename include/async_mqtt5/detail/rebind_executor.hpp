//
// Copyright (c) 2023-2024 Ivica Siladic, Bruno Iljazovic, Korina Simicevic
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef ASYNC_MQTT5_REBIND_EXECUTOR_HPP
#define ASYNC_MQTT5_REBIND_EXECUTOR_HPP

#include <boost/beast/websocket/stream.hpp>

namespace boost::asio::ssl {

// forward declare to preserve optional OpenSSL dependency
template <typename Stream>
class stream;

} // end namespace boost::asio::ssl

namespace async_mqtt5::detail {

namespace asio = boost::asio;

template <typename Stream, typename Executor>
struct rebind_executor {
	using other = typename Stream::template rebind_executor<Executor>::other;
};

// asio::ssl::stream does not define a rebind_executor member type
template <typename Stream, typename Executor>
struct rebind_executor<asio::ssl::stream<Stream>, Executor> {
	using other = typename asio::ssl::stream<typename rebind_executor<Stream, Executor>::other>;
};

template <typename Stream, typename Executor>
struct rebind_executor<boost::beast::websocket::stream<asio::ssl::stream<Stream>>, Executor> {
	using other = typename boost::beast::websocket::stream<
		asio::ssl::stream<typename rebind_executor<Stream, Executor>::other>,
		boost::beast::websocket::stream<asio::ssl::stream<Stream>>::is_deflate_supported::value
	>;
};

} // end namespace async_mqtt5::detail

#endif // !ASYNC_MQTT5_REBIND_EXECUTOR_HPP
