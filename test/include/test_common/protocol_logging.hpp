//
// Copyright (c) 2023-2024 Ivica Siladic, Bruno Iljazovic, Korina Simicevic
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef ASYNC_MQTT5_TEST_PROTOCOL_LOGGING_HPP
#define ASYNC_MQTT5_TEST_PROTOCOL_LOGGING_HPP

#include <string>

namespace async_mqtt5::test {

inline bool& logging_enabled() {
	static bool enabled = false;
	return enabled;
}

inline void log(const std::string& message) {
	if (logging_enabled())
		fprintf(stderr, "%s\n", message.c_str());
}


} // end namespace async_mqtt5::test

#endif // ASYNC_MQTT5_TEST_PROTOCOL_LOGGING_HPP
