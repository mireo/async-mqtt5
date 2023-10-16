#ifndef ASYNC_MQTT5_PROPERTY_TYPES_HPP
#define ASYNC_MQTT5_PROPERTY_TYPES_HPP

#include <cstdint>
#include <optional>
#include <string>
#include <type_traits>
#include <vector>

namespace async_mqtt5::prop {

constexpr std::integral_constant<uint8_t, 0x01> payload_format_indicator {};
constexpr std::integral_constant<uint8_t, 0x02> message_expiry_interval {};
constexpr std::integral_constant<uint8_t, 0x03> content_type {};
constexpr std::integral_constant<uint8_t, 0x08> response_topic {};
constexpr std::integral_constant<uint8_t, 0x09> correlation_data {};
constexpr std::integral_constant<uint8_t, 0x0b> subscription_identifier {};
constexpr std::integral_constant<uint8_t, 0x11> session_expiry_interval {};
constexpr std::integral_constant<uint8_t, 0x12> assigned_client_identifier {};
constexpr std::integral_constant<uint8_t, 0x13> server_keep_alive {};
constexpr std::integral_constant<uint8_t, 0x15> authentication_method {};
constexpr std::integral_constant<uint8_t, 0x16> authentication_data {};
constexpr std::integral_constant<uint8_t, 0x17> request_problem_information {};
constexpr std::integral_constant<uint8_t, 0x18> will_delay_interval {};
constexpr std::integral_constant<uint8_t, 0x19> request_response_information {};
constexpr std::integral_constant<uint8_t, 0x1a> response_information {};
constexpr std::integral_constant<uint8_t, 0x1c> server_reference {};
constexpr std::integral_constant<uint8_t, 0x1f> reason_string {};
constexpr std::integral_constant<uint8_t, 0x21> receive_maximum {};
constexpr std::integral_constant<uint8_t, 0x22> topic_alias_maximum {};
constexpr std::integral_constant<uint8_t, 0x23> topic_alias {};
constexpr std::integral_constant<uint8_t, 0x24> maximum_qos {};
constexpr std::integral_constant<uint8_t, 0x25> retain_available {};
constexpr std::integral_constant<uint8_t, 0x26> user_property {};
constexpr std::integral_constant<uint8_t, 0x27> maximum_packet_size {};
constexpr std::integral_constant<uint8_t, 0x28> wildcard_subscription_available {};
constexpr std::integral_constant<uint8_t, 0x29> subscription_identifier_available {};
constexpr std::integral_constant<uint8_t, 0x2a> shared_subscription_available {};

template <std::integral_constant p>
struct property_traits;

#define DEF_PROPERTY_TRAIT(Pname, Ptype) \
template <> struct property_traits<Pname> {\
	static constexpr std::string_view name = #Pname; \
	using type = Ptype;\
}\


DEF_PROPERTY_TRAIT(payload_format_indicator, std::optional<uint8_t>);
DEF_PROPERTY_TRAIT(message_expiry_interval, std::optional<int32_t>);
DEF_PROPERTY_TRAIT(content_type, std::optional<std::string>);
DEF_PROPERTY_TRAIT(response_topic, std::optional<std::string>);
DEF_PROPERTY_TRAIT(correlation_data, std::optional<std::string>);
DEF_PROPERTY_TRAIT(subscription_identifier, std::optional<uint32_t>);
DEF_PROPERTY_TRAIT(session_expiry_interval, std::optional<int32_t>);
DEF_PROPERTY_TRAIT(assigned_client_identifier, std::optional<std::string>);
DEF_PROPERTY_TRAIT(server_keep_alive, std::optional<int16_t>);
DEF_PROPERTY_TRAIT(authentication_method, std::optional<std::string>);
DEF_PROPERTY_TRAIT(authentication_data, std::optional<std::string>);
DEF_PROPERTY_TRAIT(request_problem_information, std::optional<uint8_t>);
DEF_PROPERTY_TRAIT(will_delay_interval, std::optional<int32_t>);
DEF_PROPERTY_TRAIT(request_response_information, std::optional<uint8_t>);
DEF_PROPERTY_TRAIT(response_information, std::optional<std::string>);
DEF_PROPERTY_TRAIT(server_reference, std::optional<std::string>);
DEF_PROPERTY_TRAIT(reason_string, std::optional<std::string>);
DEF_PROPERTY_TRAIT(receive_maximum, std::optional<int16_t>);
DEF_PROPERTY_TRAIT(topic_alias_maximum, std::optional<uint16_t>);
DEF_PROPERTY_TRAIT(topic_alias, std::optional<int16_t>);
DEF_PROPERTY_TRAIT(maximum_qos, std::optional<uint8_t>);
DEF_PROPERTY_TRAIT(retain_available, std::optional<uint8_t>);
DEF_PROPERTY_TRAIT(user_property, std::vector<std::string>);
DEF_PROPERTY_TRAIT(maximum_packet_size, std::optional<int32_t>);
DEF_PROPERTY_TRAIT(wildcard_subscription_available, std::optional<uint8_t>);
DEF_PROPERTY_TRAIT(subscription_identifier_available, std::optional<uint8_t>);
DEF_PROPERTY_TRAIT(shared_subscription_available, std::optional<uint8_t>);

#undef DEF_PROPERTY_TRAIT

template <std::integral_constant p>
using value_type_t = typename property_traits<p>::type;

template <std::integral_constant p>
constexpr std::string_view name_v = property_traits<p>::name;

template <std::integral_constant... Ps>
class properties {
	template <std::integral_constant p>
	struct prop_type {
		using key = decltype(p);
		constexpr static std::string_view name = name_v<p>;
		value_type_t<p> value;
	};
	std::tuple<prop_type<Ps>...> _props;

public:

	template <typename T, T v>
	constexpr auto& operator[](std::integral_constant<T, v> p) noexcept {
		using Ptype = decltype(p);
		return std::get<prop_type<Ptype{}>>(_props).value;
	}

	template <typename T, T v>
	constexpr const auto& operator[](std::integral_constant<T, v> p) const noexcept {
		using Ptype = decltype(p);
		return std::get<prop_type<Ptype{}>>(_props).value;
	}

	template <class Func>
	constexpr static bool is_apply_on_v =
		std::conjunction_v<std::is_invocable<Func, value_type_t<Ps>&>...>;

	template <class Func>
	constexpr static bool is_nothrow_apply_on_v =
		std::conjunction_v<std::is_nothrow_invocable<Func, value_type_t<Ps>&>...>;

	template <class Func> requires is_apply_on_v<Func>
	constexpr bool apply_on(uint8_t property_id, Func&& func) noexcept(is_nothrow_apply_on_v<Func>) {
		return std::apply([&func, property_id](auto&... props) {
			auto pc = [&func, property_id]<std::integral_constant prop>(prop_type<prop>& px) {
				if (prop.value == property_id)
					std::invoke(func, px.value);
				return prop.value != property_id;
			};
			return (pc(props) && ...);
		}, _props);
	}

	template <class Func>
	constexpr static bool is_visitor_v =
		std::conjunction_v<std::is_invocable_r<bool, Func, decltype(Ps), value_type_t<Ps>&>...>;

	template <class Func>
	constexpr static bool is_nothrow_visitor_v =
		std::conjunction_v<std::is_nothrow_invocable<Func, decltype(Ps), value_type_t<Ps>&>...>;

	template <typename Func> requires is_visitor_v<Func>
	constexpr bool visit(Func && func) const noexcept(is_nothrow_visitor_v<Func>) {
		return std::apply(
			[&func](const auto&... props) {
				auto pc = [&func]<std::integral_constant prop>(const prop_type<prop>& px) {
					return std::invoke(func, prop, px.value);
				};
				return (pc(props) &&...);
			},
			_props);
	}

	template <typename Func> requires is_visitor_v<Func>
	constexpr bool visit(Func&& func) noexcept(is_nothrow_visitor_v<Func>) {
		return std::apply(
			[&func](auto&... props) {
				auto pc = [&func]<std::integral_constant prop>(prop_type<prop>& px) {
					return std::invoke(func, prop, px.value);
				};
				return (pc(props) && ...);
			},
			_props);
	}
};



} // end namespace async_mqtt5::prop

#endif // !ASYNC_MQTT5_PROPERTY_TYPES_HPP
