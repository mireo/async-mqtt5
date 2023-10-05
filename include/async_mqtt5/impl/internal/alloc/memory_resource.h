#ifndef ASYNC_MQTT5_MEMORY_RESOURCE_H
#define ASYNC_MQTT5_MEMORY_RESOURCE_H

// -*- C++ -*-
//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include <tuple>
#include <cstddef>
#include <cstdlib>
#include <limits>
#include <memory>
#include <new>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace pma {

struct erased_type { };

template <size_t...> struct __tuple_indices {};

template <class _IdxType, _IdxType... _Values>
struct __integer_sequence {
	template <
		template <class _OIdxType, _OIdxType...> class _ToIndexSeq,
		class _ToIndexType
	>
	using __convert = _ToIndexSeq<_ToIndexType, _Values...>;

	template <size_t _Sp>
	using __to_tuple_indices = __tuple_indices<(_Values + _Sp)...>;
};

template <size_t _Ep, size_t _Sp>
using __make_indices_imp =
	typename __make_integer_seq<__integer_sequence, size_t, _Ep - _Sp>::template
	__to_tuple_indices<_Sp>;

template <size_t _Ep, size_t _Sp = 0>
struct __make_tuple_indices {
	static_assert(_Sp <= _Ep, "__make_tuple_indices input error");
	using type = __make_indices_imp<_Ep, _Sp>;
};



struct allocator_arg_t {
	explicit allocator_arg_t() = default;
};

template <class _Tp>
struct __has_allocator_type
{
private:
	struct __two {char __lx; char __lxx;};
	template <class _Up> static __two __test(...);
	template <class _Up> static char __test(typename _Up::allocator_type* = 0);
public:
	static const bool value = sizeof(__test<_Tp>(0)) == 1;
};

template <class _Tp, class _Alloc, bool = __has_allocator_type<_Tp>::value>
struct __uses_allocator :
	public std::integral_constant<
		bool,
		std::is_convertible<_Alloc, typename _Tp::allocator_type>::value
	>
{};

template <class _Tp, class _Alloc>
struct __uses_allocator<_Tp, _Alloc, false> : public std::false_type {};

template <class _Tp, class _Alloc>
struct uses_allocator : public __uses_allocator<_Tp, _Alloc> {};

template <class _Tp, class _Alloc>
inline constexpr size_t uses_allocator_v = uses_allocator<_Tp, _Alloc>::value;

template <
	class _Tp, class _Alloc,
	bool = std::uses_allocator<_Tp, _Alloc>::value,
	bool = __has_allocator_type<_Tp>::value
>
struct __lfts_uses_allocator : public std::false_type {};

template <class _Tp, class _Alloc>
struct __lfts_uses_allocator<_Tp, _Alloc, false, false> :
	public std::false_type {};

template <class _Tp, class _Alloc, bool HasAlloc>
struct __lfts_uses_allocator<_Tp, _Alloc, true, HasAlloc> :
	public std::true_type {};

template <class _Tp, class _Alloc>
struct __lfts_uses_allocator<_Tp, _Alloc, false, true> :
	public std::integral_constant<
		bool,
		std::is_convertible<_Alloc, typename _Tp::allocator_type>::value
	  	|| std::is_same<erased_type, typename _Tp::allocator_type>::value
	>
{};

template <bool _UsesAlloc, class _Tp, class _Alloc, class ..._Args>
struct __lfts_uses_alloc_ctor_imp {
	static const int value = 0;
};

template <class _Tp, class _Alloc, class ..._Args>
struct __lfts_uses_alloc_ctor_imp<true, _Tp, _Alloc, _Args...>
{
	static const bool __ic_first =
		std::is_constructible<_Tp, std::allocator_arg_t, _Alloc, _Args...>::value;

	static const bool __ic_second =
		std::conditional<
			__ic_first,
			std::false_type,
			std::is_constructible<_Tp, _Args..., _Alloc>
		>::type::value;

	static_assert(
		__ic_first || __ic_second,
		"Request for uses allocator construction is ill-formed"
	);

	static const int value = __ic_first ? 1 : 2;
};

template <class _Tp, class _Alloc, class ..._Args>
struct __lfts_uses_alloc_ctor :
	std::integral_constant<int,
		__lfts_uses_alloc_ctor_imp<
			__lfts_uses_allocator<_Tp, _Alloc>::value
		  , _Tp, _Alloc, _Args...
		>::value
	>
{};

template <class _Tp, class _Allocator, class... _Args>
inline void __user_alloc_construct_impl(
	std::integral_constant<int, 2>, _Tp *__storage,
	const _Allocator &__a, _Args &&... __args
) {
	new (__storage) _Tp (std::forward<_Args>(__args)..., __a);
}

template <class _Tp, class _Alloc, class ..._Args>
inline void __lfts_user_alloc_construct(
	_Tp * __store, const _Alloc & __a, _Args &&... __args
) {
	__user_alloc_construct_impl(
		typename __lfts_uses_alloc_ctor<_Tp, _Alloc, _Args...>::type(),
	   __store, __a, std::forward<_Args>(__args)...
	);
}

inline size_t __aligned_allocation_size(size_t __s, size_t __a) noexcept {
	return (__s + __a - 1) & ~(__a - 1);
}

class memory_resource {
	static const size_t __max_align = alignof(max_align_t);

public:
	virtual ~memory_resource() = default;

	void* allocate(size_t __bytes, size_t __align = __max_align) {
		return do_allocate(__bytes, __align);
	}

	void deallocate(void * __p, size_t __bytes, size_t __align = __max_align) {
		do_deallocate(__p, __bytes, __align);
	}

	bool is_equal(memory_resource const & __other) const noexcept {
		return do_is_equal(__other);
	}

private:
	virtual void* do_allocate(size_t, size_t) = 0;
	virtual void do_deallocate(void*, size_t, size_t) = 0;
	virtual bool do_is_equal(memory_resource const &) const noexcept = 0;
};

inline bool operator==(
	memory_resource const & __lhs,
	memory_resource const & __rhs
) noexcept {
	return &__lhs == &__rhs || __lhs.is_equal(__rhs);
}

inline
bool operator!=(
	memory_resource const & __lhs,
	memory_resource const & __rhs
) noexcept {
	return !(__lhs == __rhs);
}

memory_resource* new_delete_resource() noexcept;

memory_resource* null_memory_resource() noexcept;

memory_resource* get_default_resource() noexcept;

// memory_resource* set_default_resource(memory_resource * __new_res) noexcept;

template <class _ValueType>
class polymorphic_allocator {
public:
	using value_type = _ValueType;

	polymorphic_allocator() noexcept :
		__res_(get_default_resource())
	{}

	polymorphic_allocator(memory_resource* __r) noexcept :
		__res_(__r)
	{}

	polymorphic_allocator(polymorphic_allocator const &) = default;

	template <class _Tp>
	polymorphic_allocator(polymorphic_allocator<_Tp> const & __other) noexcept :
		__res_(__other.resource())
	{}

	polymorphic_allocator &
	operator=(polymorphic_allocator const &) = delete;

	_ValueType* allocate(size_t __n) {
		if (__n > __max_size())
			throw std::bad_array_new_length();
		return static_cast<_ValueType*>(
			__res_->allocate(__n * sizeof(_ValueType), alignof(_ValueType))
		);
	}

	void deallocate(_ValueType * __p, size_t __n) noexcept {
		__res_->deallocate(__p, __n * sizeof(_ValueType), alignof(_ValueType));
	}

	template <class _Tp, class ..._Ts>
	void construct(_Tp* __p, _Ts &&... __args) {
		__lfts_user_alloc_construct(
			__p, *this, std::forward<_Ts>(__args)...
		);
	}

	template <class _T1, class _T2, class ..._Args1, class ..._Args2>
	void construct(
		std::pair<_T1, _T2>* __p, std::piecewise_construct_t,
		std::tuple<_Args1...> __x, std::tuple<_Args2...> __y
	) {
		::new ((void*)__p) std::pair<_T1, _T2>(
			std::piecewise_construct,
		  	__transform_tuple(
				typename __lfts_uses_alloc_ctor<
					_T1, polymorphic_allocator&, _Args1...
				>::type(),
				std::move(__x),
				typename __make_tuple_indices<sizeof...(_Args1)>::type{}
			),
		  __transform_tuple(
				typename __lfts_uses_alloc_ctor<
					_T2, polymorphic_allocator&, _Args2...
			  	>::type(),
				std::move(__y),
				typename __make_tuple_indices<sizeof...(_Args2)>::type{}
		  	)
		);
	}

	template <class _T1, class _T2>
	void construct(std::pair<_T1, _T2>* __p) {
		construct(__p, std::piecewise_construct, std::tuple<>(), std::tuple<>());
	}

	template <class _T1, class _T2, class _Up, class _Vp>
	void construct(std::pair<_T1, _T2> * __p, _Up && __u, _Vp && __v) {
		construct(
			__p, std::piecewise_construct,
			std::forward_as_tuple(std::forward<_Up>(__u)),
			std::forward_as_tuple(std::forward<_Vp>(__v))
		);
	}

	template <class _T1, class _T2, class _U1, class _U2>
	void construct(std::pair<_T1, _T2> * __p, std::pair<_U1, _U2> const & __pr) {
		construct(
			__p, std::piecewise_construct,
			std::forward_as_tuple(__pr.first),
			std::forward_as_tuple(__pr.second)
		);
	}

	template <class _T1, class _T2, class _U1, class _U2>
	void construct(std::pair<_T1, _T2> * __p, std::pair<_U1, _U2> && __pr) {
		construct(
			__p, std::piecewise_construct,
			std::forward_as_tuple(std::forward<_U1>(__pr.first)),
			std::forward_as_tuple(std::forward<_U2>(__pr.second))
		);
	}

	template <class _Tp>
	void destroy(_Tp * __p) noexcept {
		__p->~_Tp();
	}

	polymorphic_allocator
	select_on_container_copy_construction() const noexcept {
		return polymorphic_allocator();
	}

	memory_resource* resource() const noexcept {
		return __res_;
	}

private:
	template <class ..._Args, size_t ..._Idx>
	std::tuple<_Args&&...>
	__transform_tuple(
		std::integral_constant<int, 0>, std::tuple<_Args...>&& __t,
		__tuple_indices<_Idx...>
	) const {
		return std::forward_as_tuple(std::get<_Idx>(std::move(__t))...);
	}

	template <class ..._Args, size_t ..._Idx>
	std::tuple<allocator_arg_t const&, polymorphic_allocator&, _Args&&...>
	__transform_tuple(
		std::integral_constant<int, 1>, std::tuple<_Args...> && __t,
		__tuple_indices<_Idx...>
	) {
		using _Tup = std::tuple<
			allocator_arg_t const&,
			polymorphic_allocator&, _Args&&...
		>;
		return _Tup(allocator_arg_t{}, *this, std::get<_Idx>(std::move(__t))...);
	}

	template <class ..._Args, size_t ..._Idx>
	std::tuple<_Args&&..., polymorphic_allocator&>
	__transform_tuple(
		std::integral_constant<int, 2>, std::tuple<_Args...> && __t,
		__tuple_indices<_Idx...>
	) {
		using _Tup = std::tuple<_Args&&..., polymorphic_allocator&>;
		return _Tup(std::get<_Idx>(std::move(__t))..., *this);
	}

	size_t __max_size() const noexcept {
		return std::numeric_limits<size_t>::max() / sizeof(value_type);
	}

	memory_resource * __res_;
};

template <class _Tp, class _Up>
inline bool operator==(
	polymorphic_allocator<_Tp> const & __lhs,
	polymorphic_allocator<_Up> const & __rhs
) noexcept {
	return *__lhs.resource() == *__rhs.resource();
}

template <class _Tp, class _Up>
inline bool operator!=(
	polymorphic_allocator<_Tp> const & __lhs,
	polymorphic_allocator<_Up> const & __rhs
) noexcept {
	return !(__lhs == __rhs);
}

template <class _CharAlloc>
class __resource_adaptor_imp : public memory_resource {
	using _CTraits = std::allocator_traits<_CharAlloc>;
	static_assert(
		std::is_same<typename _CTraits::value_type, char>::value &&
		std::is_same<typename _CTraits::pointer, char*>::value &&
		std::is_same<typename _CTraits::void_pointer, void*>::value
	);

	static const size_t _MaxAlign = alignof(max_align_t);

	using _Alloc = typename _CTraits::template rebind_alloc<
		typename std::aligned_storage<_MaxAlign, _MaxAlign>::type
	>;

	using _ValueType = typename _Alloc::value_type;

	_Alloc __alloc_;

public:
	using allocator_type = _CharAlloc;

	__resource_adaptor_imp() = default;
	__resource_adaptor_imp(__resource_adaptor_imp const &) = default;
	__resource_adaptor_imp(__resource_adaptor_imp &&) noexcept = default;

	explicit __resource_adaptor_imp(allocator_type const & __a) :
		__alloc_(__a)
	{}

	explicit __resource_adaptor_imp(allocator_type && __a) :
		__alloc_(std::move(__a))
	{}

	__resource_adaptor_imp &
	operator=(__resource_adaptor_imp const &) = default;

	allocator_type get_allocator() const {
		return __alloc_;
	}

private:
	void * do_allocate(size_t __bytes, size_t) override {
		if (__bytes > __max_size())
			throw std::bad_array_new_length();
		size_t __s = __aligned_allocation_size(__bytes, _MaxAlign) / _MaxAlign;
		return __alloc_.allocate(__s);
	}

	void do_deallocate(void * __p, size_t __bytes, size_t) override	{
		size_t __s = __aligned_allocation_size(__bytes, _MaxAlign) / _MaxAlign;
		__alloc_.deallocate((_ValueType*)__p, __s);
	}

	bool do_is_equal(memory_resource const & __other) const noexcept override {
		auto __p = dynamic_cast<__resource_adaptor_imp const *>(&__other);
		return __p  ? __alloc_ == __p->__alloc_ : false;
	}

	size_t __max_size() const noexcept {
		return std::numeric_limits<size_t>::max() - _MaxAlign;
	}
};

template <class _Alloc>
using resource_adaptor = __resource_adaptor_imp<
	typename std::allocator_traits<_Alloc>::template rebind_alloc<char>
>;

class __new_delete_memory_resource_imp : public memory_resource {
	void* do_allocate(size_t size, size_t /*align*/) override {
		return new std::byte[size];
	}

	void do_deallocate(void *p, size_t n, size_t align) override {
		delete [](std::byte*)(p);
	}

	bool do_is_equal(memory_resource const & other) const noexcept override { 
		return &other == this; 
	}

public:
	~__new_delete_memory_resource_imp() override = default;
};

class __null_memory_resource_imp : public memory_resource {
public:
	~__null_memory_resource_imp() override = default;

protected:
	void* do_allocate(size_t, size_t) override {
		throw std::bad_alloc();
	}
	void do_deallocate(void*, size_t, size_t) override {}
	bool do_is_equal(memory_resource const & __other) const noexcept override {
		return &__other == this;
	}
};

inline memory_resource* new_delete_resource() noexcept {
	static __new_delete_memory_resource_imp inst { };
	return &inst;
}

/*

// Commented out to prevent creation of polymorphic_allocator without
// explicitly provided memory_resource

inline std::atomic<memory_resource*>& __default_memory_resource(
	bool set = false, memory_resource* = nullptr
) {
	static std::atomic<memory_resource*> def { 
		new_delete_resource() 
	};
	return def;
}


inline memory_resource * get_default_resource() noexcept {
	return __default_memory_resource();
}

inline memory_resource * set_default_resource(memory_resource * __new_res) noexcept {
	return __default_memory_resource(true, __new_res);
}

*/

} // end namespace pma

#endif // !ASYNC_MQTT5_MEMORY_RESOURCE_H
