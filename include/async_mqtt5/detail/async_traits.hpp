#ifndef ASYNC_MQTT5_ASYNC_TRAITS_HPP
#define ASYNC_MQTT5_ASYNC_TRAITS_HPP

#include <type_traits>

#include <boost/asio/associated_executor.hpp>
#include <boost/asio/prefer.hpp>
#include <boost/asio/write.hpp>

#include <boost/beast/core/stream_traits.hpp>

#include <boost/type_traits/detected_or.hpp>
#include <boost/type_traits/is_detected.hpp>
#include <boost/type_traits/remove_cv_ref.hpp>

#include <async_mqtt5/types.hpp>

namespace async_mqtt5 {

namespace asio = boost::asio;

template <typename StreamType>
struct tls_handshake_type {};

template <typename TlsContext, typename TlsStream>
void assign_tls_sni(const authority_path& ap, TlsContext& ctx, TlsStream& s);

namespace detail {

template <typename Handler, typename DfltExecutor>
using tracking_type = std::decay_t<
	typename asio::prefer_result<
		asio::associated_executor_t<Handler, DfltExecutor>,
		asio::execution::outstanding_work_t::tracked_t
	>::type
>;

template <typename Handler, typename DfltExecutor>
tracking_type<Handler, DfltExecutor>
tracking_executor(const Handler& handler, const DfltExecutor& ex) {
	return asio::prefer(
		asio::get_associated_executor(handler, ex),
		asio::execution::outstanding_work.tracked
	);
}

template <typename T, typename ...Ts>
using async_write_sig = decltype(
	std::declval<T&>().async_write(std::declval<Ts>()...)
);

constexpr auto write_handler_t = [](error_code, size_t) {};

template <typename T, typename B>
constexpr bool has_async_write = boost::is_detected<
	async_write_sig, T, B, decltype(write_handler_t)
>::value;


constexpr auto handshake_handler_t = [](error_code) {};

template <typename T>
using tls_handshake_t = typename T::handshake_type;

template <typename T>
using tls_handshake_type_of = boost::detected_or_t<void, tls_handshake_t, T>;

template <typename T, typename ...Ts>
using async_tls_handshake_sig = decltype(
	std::declval<T&>().async_handshake(std::declval<Ts>()...)
);

template <typename T>
constexpr bool has_tls_handshake = boost::is_detected<
	async_tls_handshake_sig, T, tls_handshake_type_of<T>,
	decltype(handshake_handler_t)
>::value;


template <typename T, typename ...Ts>
using async_ws_handshake_sig = decltype(
	std::declval<T&>().async_handshake(std::declval<Ts>()...)
);

template <typename T>
constexpr bool has_ws_handshake = boost::is_detected<
	async_ws_handshake_sig, T,
	std::string_view, std::string_view,
	decltype(handshake_handler_t)
>::value;


template <typename T>
using tls_context_sig = decltype(
	std::declval<T&>().tls_context()
);

template <typename T>
constexpr bool has_tls_context = boost::is_detected<
	tls_context_sig, T
>::value;


template <typename T>
using next_layer_sig = decltype(
	std::declval<T&>().next_layer()
);

template <typename T>
constexpr bool has_next_layer = boost::is_detected<
	next_layer_sig, T
>::value;

template <typename T, typename Enable = void>
struct next_layer_type {
	using type = T;
};

template <typename T>
struct next_layer_type<
	T, std::enable_if_t<has_next_layer<T>>
> {
	using type = typename std::remove_reference_t<T>::next_layer_type;
};

template <typename T>
typename next_layer_type<T, std::enable_if_t<!has_next_layer<T>>>::type&
next_layer(T&& a) {
	return a;
}

template <typename T>
typename next_layer_type<T, std::enable_if_t<has_next_layer<T>>>::type&
next_layer(T&& a) {
	return a.next_layer();
}

template <typename S>
using lowest_layer_type = typename boost::beast::lowest_layer_type<S>;

template <typename S>
lowest_layer_type<S>& lowest_layer(S&& a) {
	return boost::beast::get_lowest_layer(std::forward<S>(a));
}

template <typename T, typename Enable = void>
struct has_tls_layer_impl : std::false_type {};

template <typename T>
struct has_tls_layer_impl<
	T, std::enable_if_t<has_tls_handshake<T>>
> : std::true_type {};

template <typename T>
struct has_tls_layer_impl<
	T, std::enable_if_t<!has_tls_handshake<T> && has_next_layer<T>>
> : has_tls_layer_impl<
	boost::remove_cv_ref_t<decltype(std::declval<T&>().next_layer())>
> {};

template <typename T>
constexpr bool has_tls_layer = has_tls_layer_impl<
	boost::remove_cv_ref_t<T>
>::value;


template <
	typename Stream,
	typename ConstBufferSequence,
	typename CompletionToken
>
decltype(auto) async_write(
	Stream& stream, const ConstBufferSequence& buff, CompletionToken&& token
) {
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
