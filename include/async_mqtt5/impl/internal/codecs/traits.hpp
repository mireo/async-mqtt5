#ifndef ASYNC_MQTT5_TRAITS_HPP
#define ASYNC_MQTT5_TRAITS_HPP

#include <optional>
#include <vector>

#include <boost/range/iterator_range_core.hpp>

namespace async_mqtt5 {

template <typename> constexpr bool is_optional_impl = false;
template <typename T> constexpr bool is_optional_impl<std::optional<T>> = true;

template <typename T>
constexpr bool is_optional = is_optional_impl<std::remove_cvref_t<T>>;

template <class, template<class...> class>
constexpr bool is_specialization = false;

template <template<class...> class T, class... Args>
constexpr bool is_specialization<T<Args...>, T> = true;

template <typename T>
concept is_vector = is_specialization<std::remove_cvref_t<T>, std::vector>;

template <typename T>
concept is_boost_iterator = is_specialization<
	std::remove_cvref_t<T>, boost::iterator_range
>;

} // end namespace async_mqtt5

#endif // !ASYNC_MQTT5_TRAITS_HPP
