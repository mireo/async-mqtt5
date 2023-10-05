#ifndef ASYNC_MQTT5_VECTOR_H
#define ASYNC_MQTT5_VECTOR_H

#include <vector>

#include "memory.h"

namespace pma {

template <class _ValueT>
using vector = std::vector<_ValueT, alloc<_ValueT>>;

} // namespace pma

#endif // !ASYNC_MQTT5_VECTOR_H
