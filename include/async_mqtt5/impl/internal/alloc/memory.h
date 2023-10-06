#ifndef ASYNC_MQTT5_MEMORY_H
#define ASYNC_MQTT5_MEMORY_H

#include "memory_resource.h"

namespace pma {

template <typename T>
class alloc : public polymorphic_allocator<T> {
	using base = polymorphic_allocator<T>;

public:
	alloc(pma::memory_resource* r) noexcept : base(r) {}

	template <class T2>
	alloc(const alloc<T2>& other) noexcept : base(other.resource())	{}

	using value_type = typename base::value_type;
	using pointer = typename base::value_type*;
	using const_pointer = const typename base::value_type*;
	using reference = typename base::value_type&;
	using const_reference = const typename base::value_type&;
	using size_type = std::size_t;
	using difference_type = std::ptrdiff_t;

	// https://stackoverflow.com/questions/27471053/example-usage-of-propagate-on-container-move-assignment

	using propagate_on_container_copy_assignment = std::false_type;
	using propagate_on_container_move_assignment = std::false_type;
	using propagate_on_container_swap = std::false_type;

	alloc select_on_container_copy_construction() const noexcept {
		return alloc(base::resource());
	}
};

} // end namespace pma

#endif // !ASYNC_MQTT5_MEMORY_H
