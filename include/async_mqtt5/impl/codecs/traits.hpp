#ifndef ASYNC_MQTT5_TRAITS_HPP
#define ASYNC_MQTT5_TRAITS_HPP

#include <optional>
#include <vector>

#include <boost/range/iterator_range_core.hpp>
#include <boost/type_traits/remove_cv_ref.hpp>


namespace async_mqtt5 {

template <typename>
constexpr bool is_optional_impl = false;

template <typename T>
constexpr bool is_optional_impl<std::optional<T>> = true;

template <typename T>
constexpr bool is_optional = is_optional_impl<boost::remove_cv_ref_t<T>>;

template <typename, template <typename...> typename>
constexpr bool is_specialization = false;

template <template <typename...> typename T, typename... Args>
constexpr bool is_specialization<T<Args...>, T> = true;

template <typename T>
constexpr bool is_vector = is_specialization<
	boost::remove_cv_ref_t<T>, std::vector
>;

template <typename T>
constexpr bool is_boost_iterator = is_specialization<
	boost::remove_cv_ref_t<T>, boost::iterator_range
>;



} // end namespace async_mqtt5

#endif // !ASYNC_MQTT5_TRAITS_HPP
