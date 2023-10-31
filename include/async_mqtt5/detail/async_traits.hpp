#ifndef ASYNC_MQTT5_ASYNC_TRAITS_HPP
#define ASYNC_MQTT5_ASYNC_TRAITS_HPP

#include <concepts>
#include <type_traits>

#include <boost/asio/associated_executor.hpp>
#include <boost/asio/prefer.hpp>
#include <boost/asio/write.hpp>

#include <boost/beast/core/stream_traits.hpp>

#include <async_mqtt5/types.hpp>

namespace async_mqtt5 {

namespace asio = boost::asio;

// TODO: move tls_handshake_type and assign_tls_sni to
// separate header

template <typename StreamType>
struct tls_handshake_type {};

template <typename TlsContext, typename TlsStream>
void assign_tls_sni(const authority_path& ap, TlsContext& ctx, TlsStream& s);

namespace detail {

template <typename Handler>
auto tracking_executor(const Handler& handler) {
	return asio::prefer(
		asio::get_associated_executor(handler),
		asio::execution::outstanding_work.tracked
	);
}

template <typename Handler>
using tracking_type = std::decay_t<
	decltype(tracking_executor(std::declval<Handler>()))
>;

template <typename T, typename B>
concept has_async_write = requires(T a) {
	a.async_write(
		std::declval<B>(),
		[](error_code, size_t) {}
	);
};

template<typename T>
concept has_tls_handshake = requires(T a) {
	a.async_handshake(
		typename T::handshake_type{},
		[](error_code) {}
	);
};

template<typename T>
concept has_ws_handshake = requires(T a) {
	a.async_handshake(
		std::declval<std::string_view>(),
		std::declval<std::string_view>(),
		[](error_code) {}
	);
};

template <typename T>
concept has_tls_context = requires(T a) {
	a.tls_context();
};

template <typename T>
concept has_next_layer = requires(T a) {
	a.next_layer();
};

template <typename T>
struct next_layer_type {
	using type = T;
};

template <typename T>
requires has_next_layer<T>
struct next_layer_type<T> {
	using type = typename std::remove_reference_t<T>::next_layer_type;
};

template <typename T>
requires (!has_next_layer<T>)
typename next_layer_type<T>::type& next_layer(T&& a) {
	return a;
}

template <typename T>
requires has_next_layer<T>
typename next_layer_type<T>::type& next_layer(T&& a) {
	return a.next_layer();
}

template <typename S>
using lowest_layer_type = typename boost::beast::lowest_layer_type<S>;

template <typename S>
lowest_layer_type<S>& lowest_layer(S&& a) {
	return boost::beast::get_lowest_layer(std::forward<S>(a));
}

template <typename T>
struct has_tls_layer_impl : std::false_type {};

template <typename T>
requires has_tls_handshake<T>
struct has_tls_layer_impl<T> : std::true_type {};

template <typename T>
requires (!has_tls_handshake<T> && has_next_layer<T>)
struct has_tls_layer_impl<T> : has_tls_layer_impl<
	std::remove_cvref_t<decltype(std::declval<T&>().next_layer())>
> {};

template <typename T>
concept has_tls_layer = has_tls_layer_impl<std::remove_cvref_t<T>>::value;

//TODO: move to appropriate place
template <
	typename Stream,
	typename ConstBufferSequence,
	typename CompletionToken
>
decltype(auto) async_write(
	Stream& stream, const ConstBufferSequence& buff, CompletionToken&& token
) {
	// TODO: find layer that has async write method
	if constexpr (has_async_write<Stream, ConstBufferSequence>)
		return stream.async_write(
			buff, std::forward<CompletionToken>(token)
		);
	else
		return asio::async_write(
			stream, buff, std::forward<CompletionToken>(token)
		);
}

template <typename TlsContext, typename Stream>
void setup_tls_sni(const authority_path& ap, TlsContext& ctx, Stream& s) {
	if constexpr (has_tls_handshake<Stream>) 
		assign_tls_sni(ap, ctx, s);
	else if constexpr (has_next_layer<Stream>)
		setup_tls_sni(ap, ctx, next_layer(s));
}

} // end namespace detail

} // end namespace async_mqtt5

#endif // !ASYNC_MQTT5_ASYNC_TRAITS_HPP
