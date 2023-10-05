#ifndef ASYNC_MQTT5_STRING_H
#define ASYNC_MQTT5_STRING_H

#include <string>

#include "memory.h"

namespace pma {

template <class _CharT, class _Traits = std::char_traits<_CharT>>
using basic_string =
    std::basic_string<_CharT, _Traits, alloc<_CharT>>;


using string = basic_string<char>;
using u16string = basic_string<char16_t>;
using u32string = basic_string<char32_t>;
using wstring = basic_string<wchar_t>;

} // namespace pma

#endif // !ASYNC_MQTT5_STRING_H
