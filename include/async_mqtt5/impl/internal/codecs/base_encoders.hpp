#ifndef ASYNC_MQTT5_BASE_ENCODERS_HPP
#define ASYNC_MQTT5_BASE_ENCODERS_HPP

#include <cstddef>
#include <cstdint>

#include <boost/endian/conversion.hpp>

#include <async_mqtt5/property_types.hpp>

#include <async_mqtt5/impl/internal/codecs/traits.hpp>

namespace async_mqtt5::encoders {

namespace basic {

inline void to_variable_bytes(std::string& s, int32_t val) {
	if (val > 0xfffffff) return;
	while (val > 127) {
		s.push_back(char((val & 0b01111111) | 0b10000000));
		val >>= 7;
	}
	s.push_back(val & 0b01111111);
}

inline size_t variable_length(int32_t val) {
	if (val > 0xfffffff) return 0;
	size_t rv = 1;
	for (; val > 127; ++rv) val >>= 7;
	return rv;
}

struct encoder {};

template <size_t bits, typename repr = uint8_t>
class flag_def : public encoder {
	template <size_t num_bits>
	using least_type = std::conditional_t<
		num_bits <= 8, uint8_t, 
		std::conditional_t<
			num_bits <= 16, uint16_t,
			std::conditional_t<
				num_bits <= 32, uint32_t, 
				std::conditional_t<num_bits <= 64, uint64_t, void>
			>
		>
	>;

	template <size_t oth_bits, typename oth_repr>
	friend class flag_def;

	repr _val { 0 };

public:
	flag_def(repr val) : _val(val) {}
	flag_def() = default;

	template <class T, typename projection = std::identity>
	requires (is_optional<T>)
	auto operator()(T&& value, projection proj = {}) const {
		if constexpr (std::is_same_v<projection, std::identity>) {
			repr val = value.has_value();
			return flag_def<bits, repr> { val };
		}
		else {
			repr val = value.has_value() ? static_cast<repr>(std::invoke(proj, *value)) : 0;
			return flag_def<bits, repr> { val };
		}
	}

	template <class T, typename projection = std::identity>
	requires (!is_optional<T>)
	auto operator()(T&& value, projection proj = {}) const {
		auto val = static_cast<repr>(std::invoke(proj, value));
		return flag_def<bits, repr> { val };
	}

	uint16_t byte_size() const { return sizeof(repr); }

	template <size_t rhs_bits, typename rhs_repr>
	auto operator|(const flag_def<rhs_bits, rhs_repr>& rhs) const {
		using res_repr = least_type<bits + rhs_bits>;
		auto val = static_cast<res_repr>((_val << rhs_bits) | rhs._val);
		return flag_def<bits + rhs_bits, res_repr> { val };
	}

	std::string& encode(std::string& s) const {
		using namespace boost::endian;
		size_t sz = s.size(); s.resize(sz + sizeof(repr));
		auto p = reinterpret_cast<uint8_t*>(s.data() + sz);
		endian_store<repr, sizeof(repr), order::big>(p, _val);
		return s;
	}
};

template <size_t bits, typename repr = uint8_t>
constexpr auto flag = flag_def<bits, repr>{};


template <typename T, typename Repr>
class int_val : public encoder {
	T _val;
public:
	int_val(T val) : _val(val) {}

	uint16_t byte_size() const { 
		if constexpr (is_optional<T>) {
			if (_val) return uint16_t(val_length(*_val));
			return uint16_t(0);
		}
		else
			return uint16_t(val_length(_val));
	}

	std::string& encode(std::string& s) const {
		if constexpr (is_optional<T>) {
			if (_val) return encode_val(s, *_val);
			return s;
		}
		else
			return encode_val(s, _val);
	}
private:
	template <typename U>
	static size_t val_length(U&& val) {
		if constexpr (std::is_same_v<Repr, intptr_t>)
			return variable_length(int32_t(val));
		else
			return sizeof(Repr); 
	}

	template <typename U>
	static std::string& encode_val(std::string& s, U&& val) {
		using namespace boost::endian;
		if constexpr (std::is_same_v<Repr, intptr_t>) {
			to_variable_bytes(s, int32_t(val));
			return s;			
		}
		else {
			size_t sz = s.size(); s.resize(sz + sizeof(Repr));
			auto p = reinterpret_cast<uint8_t*>(s.data() + sz);
			endian_store<Repr, sizeof(Repr), order::big>(p, val);
			return s;
		}
	}
};

template <typename Repr>
class int_def {
public:
	template <typename T>
	auto operator()(T&& val) const {
		return int_val<T, Repr> { std::forward<T>(val) };
	}

	template <typename T, typename projection>
	auto operator()(T&& val, projection proj) const {
		if constexpr (is_optional<T>) {
			using rv_type = std::invoke_result_t<
				projection, typename std::remove_cvref_t<T>::value_type
			>;
			if (val.has_value())
				return (*this)(std::invoke(proj, *val));
			return int_val<rv_type, Repr> { rv_type {} };
		}
		else {
			using rv_type = std::invoke_result_t<projection, T>;
			return int_val<rv_type, Repr> { std::invoke(proj, val) };
		}
	}
};

constexpr auto byte_ = int_def<uint8_t>{};
constexpr auto int16_ = int_def<uint16_t>{};
constexpr auto int32_ = int_def<uint32_t>{};
constexpr auto varlen_ = int_def<intptr_t>{};


template <typename T>
class array_val : public encoder {
	T _val;
	bool _with_length;
public:
	array_val(T val, bool with_length) : _val(val), _with_length(with_length) {
		static_assert(std::is_reference_v<T> || std::is_same_v<T, std::string_view>);
	}

	uint16_t byte_size() const { 
		if constexpr (is_optional<T>)
			return uint16_t(_val ? _with_length * 2 + val_length(*_val) : 0); 
		else
		 	return uint16_t(_with_length * 2 + val_length(_val));
	}

	std::string& encode(std::string& s) const {
		if constexpr (is_optional<T>) {
			if (_val) return encode_val(s, *_val);
			return s;
		}
		else
			return encode_val(s, _val);
	}

private:
	template <typename U>
	static size_t val_length(U&& val) {
		if constexpr (std::same_as<std::remove_cvref_t<U>, const char*>)
			return std::strlen(val);
		if constexpr (requires { val.size(); })
			return val.size();
		else // fallback to type const char (&)[N] (substract 1 for trailing 0)
			return sizeof(val) - 1; 
	}

	template <typename U>
	std::string& encode_val(std::string& s, U&& u) const {
		using namespace boost::endian;
		int16_t byte_len = int16_t(val_length(std::forward<U>(u)));
		if (byte_len == 0 && !_with_length) return s;
		if (_with_length) {
			size_t sz = s.size(); s.resize(sz + 2);
			auto p = reinterpret_cast<uint8_t*>(s.data() + sz);
			endian_store<int16_t, sizeof(int16_t), order::big>(p, byte_len);
		}
		s.append(std::begin(u), std::begin(u) + byte_len);
		return s;
	}
};

template <bool with_length = true>
class array_def {
public:
	template <typename T>
	auto operator()(T&& val) const {
		return array_val<T> { std::forward<T>(val), with_length };
	}

	template <typename T, typename projection>
	auto operator()(T&& val, projection proj) const {
		if constexpr (is_optional<T>) {
			using rv_type = std::invoke_result_t<
				projection, typename std::remove_cvref_t<T>::value_type
			>;
			if (val.has_value())
				return (*this)(std::invoke(proj, *val));
			return array_val<rv_type> { rv_type {}, false };
		}
		else {
			const auto& av = std::invoke(proj, val);
			return array_val<T> { av, true };
		}
	}
};

using utf8_def = array_def<true>;

constexpr auto utf8_ = utf8_def{};
constexpr auto binary_ = array_def<true>{}; // for now
constexpr auto verbatim_ = array_def<false>{};


template <class T, class U>
class composed_val : public encoder {
	T _lhs; U _rhs;
public:
	composed_val(T lhs, U rhs) : 
		_lhs(std::forward<T>(lhs)), _rhs(std::forward<U>(rhs)) {}

	uint16_t byte_size() const { 
		return uint16_t(_lhs.byte_size() + _rhs.byte_size()); 
	}

	std::string& encode(std::string& s) const {
		_lhs.encode(s);
		return _rhs.encode(s);
	}
};

template <class T, class U>
requires (std::derived_from<std::decay_t<T>, encoder> && std::derived_from<std::decay_t<U>, encoder>)
inline auto operator&(T&& t, U&& u) {
	return composed_val(std::forward<T>(t), std::forward<U>(u));
}

template <class T>
requires (std::derived_from<std::decay_t<T>, encoder>)
std::string& operator<<(std::string& s, T&& t) {
	return t.encode(s);
}

} // end namespace basic

namespace detail {
template <std::integral_constant p, std::size_t I, typename Tuple>
constexpr bool match_v = std::is_same_v<decltype(p), typename std::tuple_element_t<I, Tuple>::key>;

template <std::integral_constant p, typename Tuple, typename Idxs = std::make_index_sequence<std::tuple_size_v<Tuple>>>
struct type_index;

template <std::integral_constant p, template <typename...> typename Tuple, typename... Args, std::size_t... Is>
struct type_index<p, Tuple<Args...>, std::index_sequence<Is...>>
	: std::integral_constant<std::size_t, ((Is * match_v<p, Is, Tuple<Args...>>)+... + 0)> {
	static_assert(1 == (match_v<p, Is, Tuple<Args...>> + ... + 0), "T doesn't appear once in tuple");
};
} // end namespace detail

namespace prop {


namespace pp = async_mqtt5::prop;

template <std::integral_constant p, typename T>
struct prop_encoder_type { using key = decltype(p); using value = T; };

using encoder_types = std::tuple<
	prop_encoder_type<pp::shared_subscription_available, basic::int_def<uint8_t>>,
	prop_encoder_type<pp::payload_format_indicator, basic::int_def<uint8_t>>,
	prop_encoder_type<pp::message_expiry_interval, basic::int_def<int32_t>>,
	prop_encoder_type<pp::content_type, basic::utf8_def>,
	prop_encoder_type<pp::response_topic, basic::utf8_def>,
	prop_encoder_type<pp::correlation_data, basic::utf8_def>,
	prop_encoder_type<pp::subscription_identifier, basic::int_def<intptr_t>>,
	prop_encoder_type<pp::session_expiry_interval, basic::int_def<int32_t>>,
	prop_encoder_type<pp::assigned_client_identifier, basic::utf8_def>,
	prop_encoder_type<pp::server_keep_alive, basic::int_def<int16_t>>,
	prop_encoder_type<pp::authentication_method, basic::utf8_def>,
	prop_encoder_type<pp::authentication_data, basic::utf8_def>,
	prop_encoder_type<pp::request_problem_information, basic::int_def<uint8_t>>,
	prop_encoder_type<pp::will_delay_interval, basic::int_def<int32_t>>,
	prop_encoder_type<pp::request_response_information, basic::int_def<uint8_t>>,
	prop_encoder_type<pp::response_information, basic::utf8_def>,
	prop_encoder_type<pp::server_reference, basic::utf8_def>,
	prop_encoder_type<pp::reason_string, basic::utf8_def>,
	prop_encoder_type<pp::receive_maximum, basic::int_def<int16_t>>,
	prop_encoder_type<pp::topic_alias_maximum, basic::int_def<int16_t>>,
	prop_encoder_type<pp::topic_alias, basic::int_def<int16_t>>,
	prop_encoder_type<pp::maximum_qos, basic::int_def<uint8_t>>,
	prop_encoder_type<pp::retain_available, basic::int_def<uint8_t>>,
	prop_encoder_type<pp::user_property, basic::utf8_def>,
	prop_encoder_type<pp::maximum_packet_size, basic::int_def<int32_t>>,
	prop_encoder_type<pp::wildcard_subscription_available, basic::int_def<uint8_t>>,
	prop_encoder_type<pp::subscription_identifier_available, basic::int_def<uint8_t>>
>;

template <std::integral_constant p>
constexpr auto encoder_for_prop = typename std::tuple_element_t<
	detail::type_index<p, encoder_types>::value, encoder_types
>::value {};


template <typename T, std::integral_constant p>
class prop_val;

template <typename T, std::integral_constant p>
requires (!is_vector<T> && is_optional<T>)
class prop_val<T, p> : public basic::encoder {
	 // T is always std::optional
	using opt_type = typename std::remove_cvref_t<T>::value_type;
	// allows T to be reference type to std::optional
	static inline std::optional<opt_type> nulltype; 
	T _val;
public:
	prop_val(T val) : _val(val) {
		static_assert(std::is_reference_v<T>);
	}
	prop_val() : _val(nulltype) {}

	size_t byte_size() const {
		if (!_val) return 0;
		auto sval = encoder_for_prop<p>(_val);
		return 1 + sval.byte_size();
	}

	std::string& encode(std::string& s) const {
		if (!_val)
			return s;
		s.push_back(p());
		auto sval = encoder_for_prop<p>(_val);
		return sval.encode(s); 
	}
};

template <typename T, std::integral_constant p>
requires (is_vector<T>)
class prop_val<T, p> : public basic::encoder {
	// allows T to be reference type to std::vector
	static inline std::remove_cvref_t<T> nulltype;
	T _val;
public:
	prop_val(T val) : _val(val) {
		static_assert(std::is_reference_v<T>);
	}

	prop_val() : _val(nulltype) { }

	size_t byte_size() const {
		if (_val.empty()) return 0;
		size_t total_size = 0;
		for (const auto& pr: _val) {
			auto sval = encoder_for_prop<p>(pr);
			size_t prop_size = sval.byte_size();
			if (prop_size) total_size += 1 + prop_size;
		}
		return total_size;
	}

	std::string& encode(std::string& s) const {
		if (_val.empty())
			return s;

		for (const auto& pr: _val) {
			auto sval = encoder_for_prop<p>(pr);
			s.push_back(p());
			sval.encode(s); 
		}
		return s;
	}
};


template <typename Props>
class props_val : public basic::encoder {
	static inline std::decay_t<Props> nulltype; 

	template <std::integral_constant P, typename T>
	static auto to_prop_val(const T& val) {
		return prop_val<const T&, P>(val);
	}
		
	template <std::integral_constant... Ps>
	static auto to_prop_vals(const pp::properties<Ps...>& props) {
		return std::make_tuple(to_prop_val<Ps>(props[Ps])...);
	}

	template <class Func>
	auto apply_each(Func&& func) const {
		return std::apply([&func](const auto&... props) {
			return (std::invoke(func, props), ...);
		}, _prop_vals);
	}

	decltype(to_prop_vals(std::declval<Props>())) _prop_vals;
	bool _may_omit;
public:
	props_val(Props val, bool may_omit) : _prop_vals(to_prop_vals(val)), _may_omit(may_omit) {
		static_assert(std::is_reference_v<Props>);
	}
	props_val(bool may_omit) : _prop_vals(to_prop_vals(nulltype)), _may_omit(may_omit) { }

	size_t byte_size() const {
		size_t psize = props_size();
		if (_may_omit && psize == 0) return 0;
		return psize + basic::varlen_(psize).byte_size();
	}

	std::string& encode(std::string& s) const {
		size_t psize = props_size();
		if (_may_omit && psize == 0) return s;
		basic::varlen_(psize).encode(s);
		apply_each([&s](const auto& pv) { return pv.encode(s); });
		return s;
	}
private:
	size_t props_size() const {
		size_t retval = 0;
		apply_each([&retval](const auto& pv) { return retval += pv.byte_size(); });
		return retval;
	}
};

template <bool may_omit>
class props_def {
public:
	template <typename T>
	auto operator()(T&& prop_container) const {
		if constexpr (is_optional<T>) {
			if (prop_container.has_value())
				return (*this)(*prop_container);
			return props_val<const typename std::remove_cvref_t<T>::value_type&>(true);
		}
		else {
			return props_val<T> { prop_container, may_omit };
		}
	}
};

constexpr auto props_ = props_def<false>{};
constexpr auto props_may_omit_ = props_def<true>{};

} // end namespace prop

} // end namespace async_mqtt5::encoders

#endif // !ASYNC_MQTT5_BASE_ENCODERS_HPP
