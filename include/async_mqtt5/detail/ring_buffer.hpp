#ifndef ASYNC_MQTT5_RING_BUFFER_HPP
#define ASYNC_MQTT5_RING_BUFFER_HPP

#include <memory>

namespace async_mqtt5::detail {

/*

Best used LIFO queues.
It supports random access iterators, constant time insert
and erase operations at the beginning or the end of the
buffer and interoperability with std algorithms. Buffer
capacity is ensured to be power of 2.

*/

template <typename T, typename Allocator = std::allocator<T>>
class ring_buffer {
public:
/*
	EMPTY             | 1               | 2               | FULL
	|-----------------|-----------------|-----------------|----------------
	| ............... | ****........... | ****.......**** | ***************
	| >               | [   >           |     >      [    |     ^
	| B, F == npos    | F   B           |     B      F    |     B == F
	|-----------------|-----------------|-----------------|----------------
*/
	using value_type = T;
	using allocator_type = Allocator;
	using allocator_traits = std::allocator_traits<allocator_type>;
	using size_type = std::size_t;
	using reference = value_type&;
	using const_reference = const value_type&;
	using pointer = T*;
	using const_pointer = const T*;

	template <typename P, typename R, typename ring_pointer> class _iter;

	using iterator = _iter<pointer, reference, ring_buffer*>;
	using const_iterator = _iter<const_pointer, const_reference, const ring_buffer*>;

	iterator begin() noexcept;
	iterator end() noexcept;
	const_iterator begin() const noexcept;
	const_iterator end() const noexcept;
	const_iterator cbegin() const noexcept;
	const_iterator cend() const noexcept;

	ring_buffer() = default;

	template <
		typename Alloc,
		std::enable_if_t<
			std::is_constructible_v<allocator_type, const Alloc&>, bool
		> = true
	>
	explicit ring_buffer(size_type capacity, Alloc&& allocator) :
		_alloc((Alloc&&)allocator)
	{
		reserve(capacity);
	}

	template <
		typename Alloc,
		std::enable_if_t<
			std::is_constructible_v<allocator_type, const Alloc&>,
			bool
		> = true
	>
	explicit ring_buffer(Alloc&& allocator) :
		_alloc((Alloc&&)allocator)
	{}

	explicit ring_buffer(size_type capacity) :
		ring_buffer(capacity, allocator_type { })
	{}

	ring_buffer(ring_buffer&& other) noexcept :
		_buff { std::exchange(other._buff, nullptr) },
		_front { std::exchange(other._front, -1) },
		_back { std::exchange(other._back, 0) },
		_capacity { std::exchange(other._capacity, 0) },
		_alloc { other._alloc }
	{ }

	~ring_buffer() {
		if (_buff) {
			clear();
			_alloc.deallocate(_buff, _capacity);
		}
	}

	ring_buffer& operator=(ring_buffer&& other) noexcept {
		if constexpr (
			allocator_traits::propagate_on_container_move_assignment::value
		) {
			clear_and_exchange_with(other);
			_alloc = other._alloc;
		}
		else {
			if (_alloc == other._alloc) {
				clear_and_exchange_with(other);
			}
			else {
				clear();
				auto s = other.size();
				reserve(s);
				for (size_type i = 0; i < s; ++i) {
					push_back((value_type&&)other.front());
					other.pop_front();
				}
			}
		}
		return *this;
	}

	size_type capacity() const noexcept {
		return _capacity;
	}

	size_type size() const noexcept {
		if (empty()) {
			return 0;
		}
		return _back > _front ? _back - _front : (_capacity - _front) + _back;
	}

	bool empty() const noexcept {
		return _front == npos;
	}

	bool full() const noexcept {
		return _front == _back;
	}

	reference front() noexcept {
		return _buff[_front];
	}

	const_reference front() const noexcept {
		return _buff[_front];
	}

	reference back() noexcept {
		return _buff[index(_back - 1)];
	}

	const_reference back() const noexcept {
		return _buff[index(_back - 1)];
	}

	reference operator[](size_type i) noexcept {
		return _buff[index(_front + i)];
	}

	const_reference operator[](size_type i) const noexcept {
		return _buff[index(_front + i)];
	}

	void pop_front() noexcept {
		allocator_traits::destroy(_alloc, &_buff[_front]);
		_front = index(_front + 1);
		if (_front == _back) {
			_front = npos;
			_back = 0;
		}
	}

	void push_front(const value_type& v) noexcept {
		grow_if_needed();
		_front = _front == npos ? index(_back - 1) : index(_front - 1);
		allocator_traits::construct(_alloc, &_buff[_front], v);
	}

	void push_front(value_type&& v) noexcept {
		grow_if_needed();
		_front = _front == npos ? index(_back - 1) : index(_front - 1);
		allocator_traits::construct(_alloc, &_buff[_front], (value_type&&)v);
	}

	template <typename... Args>
	void emplace_front(Args&&... args) noexcept {
		grow_if_needed();
		_front = _front == npos ? index(_back - 1) : index(_front - 1);
		allocator_traits::construct(_alloc, &_buff[_front], (Args&&)args...);
	}

	void push_back(const value_type& v) noexcept {
		grow_if_needed();
		allocator_traits::construct(_alloc, &_buff[_back], v);
		if (_front == npos) {
			_front = _back;
		}
		_back = index(_back + 1);
	}

	void push_back(value_type&& v) noexcept {
		grow_if_needed();
		allocator_traits::construct(_alloc, &_buff[_back], (value_type&&)v);
		if (_front == npos) {
			_front = _back;
		}
		_back = index(_back + 1);
	}

	template <typename... Args>
	void emplace_back(Args&&... args) {
		grow_if_needed();
		allocator_traits::construct(_alloc, &_buff[_back], (Args&&)args...);
		if (_front == npos) {
			_front = _back;
		}
		_back = index(_back + 1);
	}

	void pop_back() noexcept {
		_back = index(_back - 1);
		allocator_traits::destroy(_alloc, &_buff[_back]);
		if (_front == _back) {
			_front = npos;
			_back = 0;
		}
	}

	void clear() noexcept {
		for (size_type i = 0; i < size(); ++i) {
			allocator_traits::destroy(_alloc, &_buff[index(i)]);
		}
		_front = npos;
		_back = 0;
	}

	void swap(ring_buffer& b) noexcept {
		_buff = std::exchange(b._buff, _buff);
		_front = std::exchange(b._front, _front);
		_back = std::exchange(b._back, _back);
		_capacity = std::exchange(b._capacity, _capacity);
		_alloc = std::exchange(b._alloc, _alloc);
	}

	void reserve(size_type new_capacity) noexcept {
		if (new_capacity <= _capacity) {
			return;
		}

		if ((new_capacity & (new_capacity - 1)) != 0) {
		#if defined(_MSC_VER)
			unsigned long msb = 0;
			_BitScanReverse(&msb, static_cast<unsigned long>(new_capacity));
			uint32_t lz = 32u - msb - 1;
		#else
			uint32_t lz = __builtin_clz(new_capacity);
		#endif
			new_capacity = 1ull << (32u - lz);
		}

		auto new_buff = _alloc.allocate(new_capacity);
		auto s = size();
		if (_buff) {
			for (size_type i = 0; i < s; ++i) {
				auto& v = operator[](i);
				allocator_traits::construct(
					_alloc, &new_buff[i], (value_type&&)v
				);
				allocator_traits::destroy(_alloc, &v);
			}
			_alloc.deallocate(_buff, _capacity);
		}
		_buff = new_buff;
		_back = s;
		_front = _back == 0 ? npos : 0;
		_capacity = new_capacity;
	}

private:
	constexpr size_type index(size_type i) const noexcept {
		return i & (_capacity - 1);
	}

	void grow_if_needed() {
		if (!_buff || full())
			reserve(_capacity == 0 ? min_capacity : _capacity * 2);
	}

	void clear_and_exchange_with(ring_buffer& other) noexcept {
		clear();
		_alloc.deallocate(_buff, _capacity);
		_buff = std::exchange(other._buff, nullptr);
		_front = std::exchange(other._front, npos);
		_back = std::exchange(other._back, 0);
		_capacity = std::exchange(other._capacity, 0);
	}

	pointer _buff = nullptr;
	size_type _front = npos;
	size_type _back = 0;
	size_type _capacity = 0;
	allocator_type _alloc;

	static constexpr size_type npos = static_cast<size_type>(-1);
	static constexpr size_type min_capacity = 4;
};

template <typename T, typename Alloc>
template <typename P, typename R, typename ring_pointer>
class ring_buffer<T, Alloc>::_iter {
public:
	using value_type = T;
	using pointer = P;
	using reference = R;
	using difference_type = std::ptrdiff_t;

	_iter() noexcept = default;
	_iter(const _iter&) noexcept = default;
	_iter(_iter&&) noexcept = default;
	_iter& operator=(const _iter&) noexcept = default;
	_iter& operator=(_iter&&) noexcept = default;

	reference operator*() const noexcept {
		return *_p;
	}

	pointer operator->() const noexcept {
		return _p;
	}

	_iter& operator++() noexcept {
		_p = &_b->operator[](++_i);
		return *this;
	}

	_iter operator++(int) noexcept {
		auto tmp = *this;
		_p = &_b->operator[](++_i);
		return tmp;
	}

	_iter& operator--() noexcept {
		_p = &_b->operator[](--_i);
		return *this;
	}

	_iter operator--(int) noexcept {
		auto tmp = *this;
		_p = &_b->operator[](++_i);
		return tmp;
	}

	_iter& operator+=(difference_type d) noexcept {
		_p = &_b->operator[]((_i += d));
		return *this;
	}

	_iter& operator-=(difference_type d) noexcept {
		_p = &_b->operator[]((_i -= d));
		return *this;
	}

private:
	friend class ring_buffer;

	_iter(ring_pointer b, size_type i) :
		_b(b), _i(i), _p(&b->operator[](i))
	{}

	ring_pointer _b = nullptr;
	size_type _i = npos;
	pointer _p = nullptr;

	friend bool operator==(const _iter& a, const _iter& b) noexcept {
		return a._i == b._i;
	}

	friend auto operator<(const _iter& a, const _iter& b) noexcept {
		return difference_type(a._i - b._i);
	}

	friend difference_type operator-(const _iter& a, const _iter& b) noexcept {
		return a._i - b._i;
	}

	friend _iter operator+(const _iter& a, difference_type d) noexcept {
		return { a._b, a._i + d };
	}

	friend _iter operator-(const _iter& a, difference_type d) noexcept {
		return { a._b, a._i - d };
	}
};

template <typename T, typename Alloc>
typename ring_buffer<T, Alloc>::iterator ring_buffer<T, Alloc>::begin()
noexcept {
	return { this, 0 };
}

template <typename T, typename Alloc>
typename ring_buffer<T, Alloc>::iterator ring_buffer<T, Alloc>::end()
noexcept {
	return { this, size() };
}

template <typename T, typename Alloc>
typename ring_buffer<T, Alloc>::const_iterator ring_buffer<T, Alloc>::begin()
const noexcept {
	return { this, 0 };
}

template <typename T, typename Alloc>
typename ring_buffer<T, Alloc>::const_iterator ring_buffer<T, Alloc>::end()
const noexcept {
	return { this, size() };
}

template <typename T, typename Alloc>
typename ring_buffer<T, Alloc>::const_iterator ring_buffer<T, Alloc>::cbegin()
const noexcept {
	return { this, 0 };
}

template <typename T, typename Alloc>
typename ring_buffer<T, Alloc>::const_iterator ring_buffer<T, Alloc>::cend()
const noexcept {
	return { this, size() };
}

} // namespace async_mqtt5::detail

namespace std {

template <typename T, typename Alloc>
void swap(
	async_mqtt5::detail::ring_buffer<T, Alloc>& a,
	async_mqtt5::detail::ring_buffer<T, Alloc>& b)
{
	a.swap(b);
}

}

#endif // !ASYNC_MQTT5_RING_BUFFER_HPP
